#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

#include "audio_capture.h"
#include "app_profile.h"
#include "incremental_refinement.h"
#include "local_refinement.h"
#include "output_mode.h"
#include "speech_language.h"
#include "speech_models.h"
#include "transcriber.h"

#include <filesystem>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace {
constexpr OSType kHotKeySignature = 'SAID';
constexpr UInt32 kHotKeyIdentifier = 1;

NSString * ns_string(const std::string & value) {
    NSString * result = [[NSString alloc]
        initWithBytes:value.data()
        length:value.size()
        encoding:NSUTF8StringEncoding];
    return result != nil ? result : @"";
}

bool accessibility_trusted(bool prompt) {
    if (!prompt) {
        return AXIsProcessTrusted();
    }
    NSDictionary * options = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

void paste_from_clipboard() {
    CGEventRef key_down = CGEventCreateKeyboardEvent(nullptr, kVK_ANSI_V, true);
    CGEventRef key_up = CGEventCreateKeyboardEvent(nullptr, kVK_ANSI_V, false);
    if (key_down == nullptr || key_up == nullptr) {
        if (key_down != nullptr) CFRelease(key_down);
        if (key_up != nullptr) CFRelease(key_up);
        return;
    }
    CGEventSetFlags(key_down, kCGEventFlagMaskCommand);
    CGEventSetFlags(key_up, kCGEventFlagMaskCommand);
    CGEventPost(kCGHIDEventTap, key_down);
    CGEventPost(kCGHIDEventTap, key_up);
    CFRelease(key_down);
    CFRelease(key_up);
}

std::filesystem::path model_path() {
    NSDictionary<NSString *, NSString *> * environment = NSProcessInfo.processInfo.environment;
    NSString * override_path = environment[@"SAID_MODEL"];
    if (override_path.length > 0) {
        return std::filesystem::path(override_path.fileSystemRepresentation);
    }

    NSURL * resources = NSBundle.mainBundle.resourceURL;
    NSString * recognizer_name = [NSString
        stringWithUTF8String:speech_models::kRecognizer];
    NSURL * bundled = [[resources URLByAppendingPathComponent:@"models"]
        URLByAppendingPathComponent:recognizer_name];
    const std::filesystem::path bundled_path(bundled.path.fileSystemRepresentation);
    if (speech_models::complete_bundle(bundled_path)) {
        return bundled_path;
    }

    NSArray<NSURL *> * support_urls = [NSFileManager.defaultManager
        URLsForDirectory:NSApplicationSupportDirectory
        inDomains:NSUserDomainMask];
    NSURL * support = support_urls.firstObject;
    NSURL * installed = [[support URLByAppendingPathComponent:@"SAID/models"]
        URLByAppendingPathComponent:recognizer_name];
    return std::filesystem::path(installed.path.fileSystemRepresentation);
}

std::string clean_transcript(std::string transcript) {
    if (transcript.empty() ||
        contains_japanese_script(transcript) ||
        contains_korean_script(transcript)) {
        return transcript;
    }
    IncrementalRefinementSession refinement(
        1, OutputMode::Clean, AppProfile::Unknown);
    refinement.append_recognized(transcript);
    auto repair = refinement.make_request(RefinementStage::RecognitionRepair);
    refinement.apply(run_local_recognition_repair(repair));
    auto cleanup = refinement.make_request(RefinementStage::SpokenCleanup);
    refinement.apply(run_local_spoken_cleanup(cleanup));
    return refinement.clean_text();
}

int run_self_test() {
    try {
        const std::filesystem::path recognizer = model_path();
        if (!speech_models::complete_bundle(recognizer)) {
            std::cerr << "self-test: bundled speech model is incomplete\n";
            return 2;
        }
        Transcriber transcriber(recognizer, 1);
        const std::vector<float> silence(16000, 0.0F);
        if (!transcriber.transcribe(silence).empty()) {
            std::cerr << "self-test: silence produced a transcript\n";
            return 3;
        }
        if (clean_transcript("um hello from SAID").empty()) {
            std::cerr << "self-test: Clean produced no output\n";
            return 4;
        }
        std::cout << "SAID " << SAID_VERSION
                  << " macOS self-test passed\n";
        return 0;
    } catch (const std::exception & exception) {
        std::cerr << "self-test: " << exception.what() << '\n';
        return 5;
    } catch (...) {
        std::cerr << "self-test: unknown failure\n";
        return 6;
    }
}
}

@class SAIDAppDelegate;
static SAIDAppDelegate * g_app_delegate = nil;

@interface SAIDAppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate> {
    NSStatusItem * status_item_;
    NSMenu * menu_;
    NSMenuItem * status_menu_item_;
    NSMenuItem * toggle_menu_item_;
    NSMenuItem * exact_menu_item_;
    NSMenuItem * clean_menu_item_;
    NSMenuItem * japanese_menu_item_;
    NSMenuItem * korean_menu_item_;
    NSMenuItem * accessibility_menu_item_;
    EventHotKeyRef hot_key_;
    EventHandlerRef hot_key_handler_;
    std::unique_ptr<AudioCapture> audio_;
    std::unique_ptr<Transcriber> transcriber_;
    bool ready_;
    bool recording_;
    bool busy_;
    pid_t target_process_;
}
- (void)toggleDictation:(id)sender;
@end

static OSStatus hot_key_handler(
    EventHandlerCallRef,
    EventRef event,
    void *) {
    EventHotKeyID hot_key_id{};
    const OSStatus result = GetEventParameter(
        event,
        kEventParamDirectObject,
        typeEventHotKeyID,
        nullptr,
        sizeof(hot_key_id),
        nullptr,
        &hot_key_id);
    if (result == noErr &&
        hot_key_id.signature == kHotKeySignature &&
        hot_key_id.id == kHotKeyIdentifier) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [g_app_delegate toggleDictation:nil];
        });
        return noErr;
    }
    return eventNotHandledErr;
}

@implementation SAIDAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    g_app_delegate = self;
    audio_ = std::make_unique<AudioCapture>();
    ready_ = false;
    recording_ = false;
    busy_ = true;
    target_process_ = 0;

    [self buildStatusMenu];
    [self installGlobalHotKey];
    [self refreshMenu];

    const std::filesystem::path recognizer = model_path();
    if (!speech_models::complete_bundle(recognizer)) {
        [self showFatalError:
            @"The local speech model bundle is missing. Reinstall SAID from the DMG, "
             "or set SAID_MODEL to a complete SenseVoice model bundle."];
        return;
    }

    status_menu_item_.title = @"Loading local speech model…";
    auto recognizer_copy = std::make_shared<std::filesystem::path>(recognizer);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        __block std::string error;
        try {
            transcriber_ = std::make_unique<Transcriber>(*recognizer_copy);
        } catch (const std::exception & exception) {
            error = exception.what();
        } catch (...) {
            error = "The local speech model could not be loaded.";
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            if (!error.empty()) {
                [self showFatalError:ns_string(error)];
                return;
            }
            ready_ = true;
            busy_ = false;
            [self setStatus:@"Ready — press ⌃⌥Space"];
            [self refreshMenu];
            accessibility_trusted(true);
        });
    });
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    if (recording_) {
        audio_->stop();
    }
    if (hot_key_ != nullptr) {
        UnregisterEventHotKey(hot_key_);
        hot_key_ = nullptr;
    }
    if (hot_key_handler_ != nullptr) {
        RemoveEventHandler(hot_key_handler_);
        hot_key_handler_ = nullptr;
    }
    g_app_delegate = nil;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return NO;
}

- (void)buildStatusMenu {
    status_item_ = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
    NSStatusBarButton * button = status_item_.button;
    NSImage * icon = NSApplication.sharedApplication.applicationIconImage;
    if (icon != nil) {
        icon.size = NSMakeSize(18.0, 18.0);
        [icon setTemplate:YES];
        button.image = icon;
    } else {
        button.title = @"S";
    }
    button.toolTip = @"SAID local dictation";

    menu_ = [[NSMenu alloc] initWithTitle:@"SAID"];
    menu_.delegate = self;
    status_menu_item_ = [[NSMenuItem alloc] initWithTitle:@"Starting…" action:nil keyEquivalent:@""];
    status_menu_item_.enabled = NO;
    [menu_ addItem:status_menu_item_];

    toggle_menu_item_ = [[NSMenuItem alloc]
        initWithTitle:@"Start dictation"
        action:@selector(toggleDictation:)
        keyEquivalent:@""];
    toggle_menu_item_.target = self;
    [menu_ addItem:toggle_menu_item_];
    [menu_ addItem:NSMenuItem.separatorItem];

    exact_menu_item_ = [[NSMenuItem alloc]
        initWithTitle:@"Exact output"
        action:@selector(selectExact:)
        keyEquivalent:@""];
    exact_menu_item_.target = self;
    [menu_ addItem:exact_menu_item_];
    clean_menu_item_ = [[NSMenuItem alloc]
        initWithTitle:@"Clean output"
        action:@selector(selectClean:)
        keyEquivalent:@""];
    clean_menu_item_.target = self;
    [menu_ addItem:clean_menu_item_];
    [menu_ addItem:NSMenuItem.separatorItem];

    japanese_menu_item_ = [[NSMenuItem alloc]
        initWithTitle:@"Allow Japanese"
        action:@selector(toggleJapanese:)
        keyEquivalent:@""];
    japanese_menu_item_.target = self;
    [menu_ addItem:japanese_menu_item_];
    korean_menu_item_ = [[NSMenuItem alloc]
        initWithTitle:@"Allow Korean"
        action:@selector(toggleKorean:)
        keyEquivalent:@""];
    korean_menu_item_.target = self;
    [menu_ addItem:korean_menu_item_];
    [menu_ addItem:NSMenuItem.separatorItem];

    accessibility_menu_item_ = [[NSMenuItem alloc]
        initWithTitle:@"Accessibility permission…"
        action:@selector(requestAccessibility:)
        keyEquivalent:@""];
    accessibility_menu_item_.target = self;
    [menu_ addItem:accessibility_menu_item_];

    NSMenuItem * quit_item = [[NSMenuItem alloc]
        initWithTitle:@"Quit SAID"
        action:@selector(terminate:)
        keyEquivalent:@"q"];
    [menu_ addItem:quit_item];
    status_item_.menu = menu_;
}

- (void)installGlobalHotKey {
    EventTypeSpec event_type{kEventClassKeyboard, kEventHotKeyPressed};
    OSStatus result = InstallApplicationEventHandler(
        &hot_key_handler,
        1,
        &event_type,
        nullptr,
        &hot_key_handler_);
    if (result != noErr) {
        [self showError:@"SAID could not install its global shortcut. Use the menu bar icon instead."];
        return;
    }

    EventHotKeyID hot_key_id{kHotKeySignature, kHotKeyIdentifier};
    result = RegisterEventHotKey(
        kVK_Space,
        controlKey | optionKey,
        hot_key_id,
        GetApplicationEventTarget(),
        0,
        &hot_key_);
    if (result != noErr) {
        [self showError:@"⌃⌥Space is already in use. Use the SAID menu bar icon to dictate."];
    }
}

- (void)menuWillOpen:(NSMenu *)menu {
    (void)menu;
    NSRunningApplication * frontmost = NSWorkspace.sharedWorkspace.frontmostApplication;
    const pid_t own_process = NSProcessInfo.processInfo.processIdentifier;
    if (frontmost.processIdentifier != 0 &&
        frontmost.processIdentifier != own_process) {
        target_process_ = frontmost.processIdentifier;
    }
    [self refreshMenu];
}

- (void)refreshMenu {
    toggle_menu_item_.enabled = ready_ && !busy_;
    toggle_menu_item_.title = recording_ ? @"Stop dictation" : @"Start dictation";

    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    const bool clean = ![defaults objectForKey:@"SAIDCleanOutput"] ||
        [defaults boolForKey:@"SAIDCleanOutput"];
    exact_menu_item_.state = clean ? NSControlStateValueOff : NSControlStateValueOn;
    clean_menu_item_.state = clean ? NSControlStateValueOn : NSControlStateValueOff;
    japanese_menu_item_.state = [defaults boolForKey:@"SAIDAllowJapanese"]
        ? NSControlStateValueOn : NSControlStateValueOff;
    korean_menu_item_.state = [defaults boolForKey:@"SAIDAllowKorean"]
        ? NSControlStateValueOn : NSControlStateValueOff;
    accessibility_menu_item_.title = accessibility_trusted(false)
        ? @"Accessibility permission granted"
        : @"Grant Accessibility permission…";
}

- (void)setStatus:(NSString *)status {
    status_menu_item_.title = status;
    status_item_.button.toolTip = [@"SAID — " stringByAppendingString:status];
}

- (void)toggleDictation:(id)sender {
    (void)sender;
    if (!ready_ || busy_) {
        return;
    }
    if (!recording_) {
        [self startDictation];
    } else {
        [self stopDictation];
    }
}

- (void)startDictation {
    NSRunningApplication * target = NSWorkspace.sharedWorkspace.frontmostApplication;
    const pid_t own_process = NSProcessInfo.processInfo.processIdentifier;
    if (target.processIdentifier != 0 && target.processIdentifier != own_process) {
        target_process_ = target.processIdentifier;
    }
    std::string error;
    if (!audio_->start(error)) {
        [self showError:ns_string(error)];
        NSURL * privacy = [NSURL URLWithString:
            @"x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone"];
        [NSWorkspace.sharedWorkspace openURL:privacy];
        return;
    }
    recording_ = true;
    [self setStatus:@"Listening — press ⌃⌥Space to stop"];
    [self refreshMenu];
}

- (SpeechLanguageMask)enabledLanguages {
    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    SpeechLanguageMask languages = kDefaultSpeechLanguages;
    languages = set_speech_language_enabled(
        languages,
        SpeechLanguage::Japanese,
        [defaults boolForKey:@"SAIDAllowJapanese"]);
    languages = set_speech_language_enabled(
        languages,
        SpeechLanguage::Korean,
        [defaults boolForKey:@"SAIDAllowKorean"]);
    return languages;
}

- (void)stopDictation {
    std::vector<float> samples = audio_->stop();
    recording_ = false;
    busy_ = true;
    [self setStatus:@"Transcribing locally…"];
    [self refreshMenu];

    const SpeechLanguageMask languages = [self enabledLanguages];
    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    const bool clean = ![defaults objectForKey:@"SAIDCleanOutput"] ||
        [defaults boolForKey:@"SAIDCleanOutput"];
    auto sample_copy = std::make_shared<std::vector<float>>(std::move(samples));
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        __block std::string transcript;
        __block std::string error;
        try {
            transcript = transcriber_->transcribe(*sample_copy, languages);
            if (clean && !transcript.empty()) {
                transcript = clean_transcript(std::move(transcript));
            }
        } catch (const std::exception & exception) {
            error = exception.what();
        } catch (...) {
            error = "Dictation failed.";
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            busy_ = false;
            if (!error.empty()) {
                [self showError:ns_string(error)];
                [self setStatus:@"Ready — press ⌃⌥Space"];
                [self refreshMenu];
                return;
            }
            [self deliverTranscript:ns_string(transcript)];
            [self refreshMenu];
        });
    });
}

- (void)deliverTranscript:(NSString *)transcript {
    if (transcript.length == 0) {
        [self setStatus:@"No speech heard — ready"];
        [self restoreReadyStatusSoon];
        return;
    }

    NSPasteboard * pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
    [pasteboard setString:transcript forType:NSPasteboardTypeString];

    NSRunningApplication * frontmost = NSWorkspace.sharedWorkspace.frontmostApplication;
    if (target_process_ == 0 || frontmost.processIdentifier != target_process_) {
        [self setStatus:@"Copied — focus changed"];
        [self restoreReadyStatusSoon];
        return;
    }

    if (!accessibility_trusted(false)) {
        [self setStatus:@"Copied — grant Accessibility to paste"];
        accessibility_trusted(true);
        [self restoreReadyStatusSoon];
        return;
    }

    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(0.12 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
            paste_from_clipboard();
        });
    [self setStatus:@"Typed — ready"];
    [self restoreReadyStatusSoon];
}

- (void)restoreReadyStatusSoon {
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC),
        dispatch_get_main_queue(), ^{
            if (!recording_ && !busy_) {
                [self setStatus:@"Ready — press ⌃⌥Space"];
            }
        });
}

- (void)selectExact:(id)sender {
    (void)sender;
    [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"SAIDCleanOutput"];
    [self refreshMenu];
}

- (void)selectClean:(id)sender {
    (void)sender;
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"SAIDCleanOutput"];
    [self refreshMenu];
}

- (void)toggleJapanese:(id)sender {
    (void)sender;
    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    [defaults setBool:![defaults boolForKey:@"SAIDAllowJapanese"]
               forKey:@"SAIDAllowJapanese"];
    [self refreshMenu];
}

- (void)toggleKorean:(id)sender {
    (void)sender;
    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    [defaults setBool:![defaults boolForKey:@"SAIDAllowKorean"]
               forKey:@"SAIDAllowKorean"];
    [self refreshMenu];
}

- (void)requestAccessibility:(id)sender {
    (void)sender;
    if (accessibility_trusted(true)) {
        [self refreshMenu];
        return;
    }
    NSURL * privacy = [NSURL URLWithString:
        @"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"];
    [NSWorkspace.sharedWorkspace openURL:privacy];
}

- (void)showError:(NSString *)message {
    [NSApplication.sharedApplication activateIgnoringOtherApps:YES];
    NSAlert * alert = [[NSAlert alloc] init];
    alert.messageText = @"SAID";
    alert.informativeText = message;
    alert.alertStyle = NSAlertStyleWarning;
    [alert runModal];
}

- (void)showFatalError:(NSString *)message {
    busy_ = true;
    [self setStatus:@"Could not start"];
    [self refreshMenu];
    [self showError:message];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0) {
            return run_self_test();
        }
        if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
            std::cout << "SAID " << SAID_VERSION << '\n';
            return 0;
        }
        NSApplication * application = NSApplication.sharedApplication;
        SAIDAppDelegate * delegate = [[SAIDAppDelegate alloc] init];
        application.delegate = delegate;
        [application setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [application run];
    }
    return 0;
}

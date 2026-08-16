#include "advanced_model_download.h"
#include "adapt_job_queue.h"
#include "app_profile.h"
#include "audio_capture.h"
#include "app_settings.h"
#include "grammar_correction.h"
#include "grammar_corrector.h"
#include "incremental_refinement.h"
#include "local_refinement.h"
#include "overlay.h"
#include "output_mode.h"
#include "resource.h"
#include "setup_window.h"
#include "speech_language.h"
#include "streaming_mode.h"
#include "text_injector.h"
#include "transcriber.h"
#include "transcript.h"
#include "win_util.h"

#include <windows.h>
#include <propidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <commctrl.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
constexpr wchar_t kControllerClassName[] = L"SAIDControllerWindow";
constexpr UINT kMessageToggle = WM_APP + 1;
constexpr UINT kMessageWorker = WM_APP + 2;
constexpr UINT kMessageTray = WM_APP + 3;
constexpr UINT kMessageExternalInput = WM_APP + 4;
constexpr UINT kMessageAdvancedModel = WM_APP + 5;
constexpr UINT_PTR kStreamingAudioTimer = 1;
constexpr UINT kStreamingAudioPollMilliseconds = 100;
constexpr UINT kTrayId = 1;
constexpr UINT kMenuStatus = 100;
constexpr UINT kMenuSetup = 101;
constexpr UINT kMenuOpenModels = 102;
constexpr UINT kMenuQuit = 103;
constexpr UINT kMenuStreaming = 105;
constexpr UINT kMenuOutputExact = 106;
constexpr UINT kMenuOutputClean = 107;
constexpr UINT kMenuOutputAdapt = 108;
constexpr UINT kMenuCancelAdvancedDownload = 109;
constexpr UINT kMenuRemoveAdvancedModel = 110;
constexpr UINT kMenuProfileAuto = 111;
constexpr UINT kMenuProfileChat = 112;
constexpr UINT kMenuProfileMail = 113;
constexpr UINT kMenuProfileDocument = 114;
constexpr UINT kMenuProfileDeveloper = 115;
constexpr UINT kMenuProfileCode = 116;
constexpr UINT kMenuProfileShell = 117;

enum class WorkerMessageKind {
    Ready,
    StreamRevision,
    AdaptRevision,
    FinalDraft,
    Correcting,
    Transcript,
    Error,
};

bool grammar_correction_applied(GrammarCorrectionStatus status) {
    return status == GrammarCorrectionStatus::StandardApplied ||
           status == GrammarCorrectionStatus::AdvancedApplied ||
           status == GrammarCorrectionStatus::AdvancedFallback;
}

struct WorkerMessage {
    WorkerMessageKind kind;
    std::string text;
    GrammarCorrectionStatus grammar_status = GrammarCorrectionStatus::Disabled;
    uint64_t session_id = 0;
    bool streaming = false;
    uint64_t revision = 0;
};

void post_worker_message(
    HWND window,
    WorkerMessageKind kind,
    std::string text = {},
    GrammarCorrectionStatus grammar_status = GrammarCorrectionStatus::Disabled,
    uint64_t session_id = 0,
    bool streaming = false,
    uint64_t revision = 0) {
    auto * message = new WorkerMessage{
        kind, std::move(text), grammar_status, session_id, streaming, revision};
    if (!PostMessageW(window, kMessageWorker, 0, reinterpret_cast<LPARAM>(message))) {
        delete message;
    }
}

class AdaptWorker {
public:
    AdaptWorker(
        HWND notify_window,
        std::optional<std::filesystem::path> model_path)
        : notify_window_(notify_window),
          model_path_(std::move(model_path)),
          thread_(&AdaptWorker::run, this) {}

    ~AdaptWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            cancel_requested_.store(true, std::memory_order_release);
        }
        condition_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void submit_incremental(
        uint64_t session_id,
        uint64_t revision,
        std::string committed_prefix,
        std::string clean_tail,
        AppProfile profile) {
        if (clean_tail.empty() || !app_profile_allows_adapt(profile)) {
            return;
        }
        AdaptJob job;
        job.kind = AdaptJobKind::Incremental;
        job.session_id = session_id;
        job.revision = revision;
        job.committed_prefix = std::move(committed_prefix);
        job.clean_text = std::move(clean_tail);
        job.profile = profile;
        job.streaming = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_incremental(std::move(job));
        }
        condition_.notify_one();
    }

    void submit_final(
        uint64_t session_id,
        std::string clean_text,
        AppProfile profile,
        bool streaming) {
        AdaptJob job;
        job.kind = AdaptJobKind::Final;
        job.session_id = session_id;
        job.clean_text = std::move(clean_text);
        job.profile = profile;
        job.streaming = streaming;
        // Final composition supersedes any in-flight live rewrite. The worker
        // resets cancellation before it starts the final job.
        cancel_requested_.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_final(std::move(job));
        }
        condition_.notify_one();
    }

    void cancel(uint64_t session_id) {
        cancel_session_.store(session_id, std::memory_order_release);
        cancel_requested_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.cancel_session(session_id);
    }

    void set_model_path(std::optional<std::filesystem::path> path) {
        AdaptJob job;
        job.kind = AdaptJobKind::ModelPath;
        job.model_path = std::move(path);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_model_path(std::move(job));
        }
        condition_.notify_one();
    }

private:
    void run() {
        std::unique_ptr<LocalGrammarCorrector> corrector;
        std::optional<std::filesystem::path> loaded_path;
        while (true) {
            AdaptJob job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                if (stopping_) {
                    return;
                }
                job = std::move(*jobs_.pop_front());
            }

            if (job.kind == AdaptJobKind::ModelPath) {
                model_path_ = std::move(job.model_path);
                corrector.reset();
                loaded_path.reset();
                continue;
            }
            if (cancel_session_.load(std::memory_order_acquire) == job.session_id) {
                continue;
            }

            cancel_requested_.store(false, std::memory_order_release);
            if (model_path_ != loaded_path) {
                corrector.reset();
                loaded_path = model_path_;
            }
            if (model_path_ && !corrector) {
                try {
                    corrector = std::make_unique<LocalGrammarCorrector>(
                        *model_path_, 0, &cancel_requested_);
                } catch (...) {
                    corrector.reset();
                }
            }

            try {
                if (!corrector || !app_profile_allows_adapt(job.profile)) {
                    throw std::runtime_error("Adapt model unavailable.");
                }
                std::string rewritten = corrector->rewrite_for_app(
                    job.clean_text, job.profile, job.kind == AdaptJobKind::Final);
                if (job.kind == AdaptJobKind::Incremental) {
                    std::string visible = std::move(job.committed_prefix);
                    append_recognizer_segment(visible, rewritten);
                    post_worker_message(
                        notify_window_, WorkerMessageKind::AdaptRevision,
                        std::move(visible), GrammarCorrectionStatus::AdvancedApplied,
                        job.session_id, true, job.revision);
                } else {
                    post_worker_message(
                        notify_window_, WorkerMessageKind::Transcript,
                        std::move(rewritten), GrammarCorrectionStatus::AdvancedApplied,
                        job.session_id, job.streaming);
                }
            } catch (const GrammarCorrectionCancelled &) {
                if (job.kind == AdaptJobKind::Final &&
                    cancel_session_.load(std::memory_order_acquire) != job.session_id) {
                    post_worker_message(
                        notify_window_, WorkerMessageKind::Transcript,
                        std::move(job.clean_text), GrammarCorrectionStatus::Skipped,
                        job.session_id, job.streaming);
                }
            } catch (...) {
                if (job.kind == AdaptJobKind::Final) {
                    post_worker_message(
                        notify_window_, WorkerMessageKind::Transcript,
                        std::move(job.clean_text), GrammarCorrectionStatus::AdvancedFallback,
                        job.session_id, job.streaming);
                }
            }
        }
    }

    HWND notify_window_ = nullptr;
    std::optional<std::filesystem::path> model_path_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    AdaptJobQueue jobs_;
    std::atomic<bool> cancel_requested_{false};
    std::atomic<uint64_t> cancel_session_{0};
    bool stopping_ = false;
};

class RecognitionWorker {
public:
    RecognitionWorker(
        HWND notify_window,
        std::filesystem::path model_path,
        AdaptWorker * adapt_worker)
        : notify_window_(notify_window),
          model_path_(std::move(model_path)),
          adapt_worker_(adapt_worker),
          thread_(&RecognitionWorker::run, this) {}

    ~RecognitionWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        condition_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void submit(
        std::vector<float> samples,
        OutputMode output_mode,
        AppProfile app_profile,
        uint64_t session_id,
        SpeechLanguageMask speech_languages) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(RecognitionJob{
                JobKind::Transcribe,
                session_id,
                std::move(samples),
                output_mode,
                app_profile,
                false,
                speech_languages,
            });
        }
        condition_.notify_one();
    }

    void start_streaming(
        uint64_t session_id,
        OutputMode output_mode,
        AppProfile app_profile,
        bool live_delivery,
        SpeechLanguageMask speech_languages) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(RecognitionJob{
                JobKind::StreamStart,
                session_id,
                {},
                output_mode,
                app_profile,
                live_delivery,
                speech_languages,
            });
        }
        condition_.notify_one();
    }

    void push_streaming_audio(uint64_t session_id, std::vector<float> samples) {
        if (samples.empty()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!jobs_.empty() && jobs_.back().kind == JobKind::StreamAudio &&
                jobs_.back().session_id == session_id) {
                auto & pending = jobs_.back().audio;
                pending.insert(pending.end(),
                               std::make_move_iterator(samples.begin()),
                               std::make_move_iterator(samples.end()));
            } else {
                jobs_.push_back(RecognitionJob{
                    JobKind::StreamAudio,
                    session_id,
                    std::move(samples),
                    OutputMode::Exact,
                    AppProfile::Unknown,
                    false,
                });
            }
        }
        condition_.notify_one();
    }

    void finish_streaming(uint64_t session_id) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(RecognitionJob{
                JobKind::StreamFinish,
                session_id,
                {},
                OutputMode::Exact,
                AppProfile::Unknown,
                false,
            });
        }
        condition_.notify_one();
    }

private:
    enum class JobKind {
        Transcribe,
        StreamStart,
        StreamAudio,
        StreamFinish,
    };

    struct RecognitionJob {
        JobKind kind = JobKind::Transcribe;
        uint64_t session_id = 0;
        std::vector<float> audio;
        OutputMode output_mode = OutputMode::Exact;
        AppProfile app_profile = AppProfile::Unknown;
        bool live_delivery = false;
        SpeechLanguageMask speech_languages = kDefaultSpeechLanguages;
    };

    struct StreamingContext {
        uint64_t session_id = 0;
        OutputMode output_mode = OutputMode::Exact;
        AppProfile app_profile = AppProfile::Unknown;
        bool live_delivery = false;
        std::string transcript;
        std::unique_ptr<IncrementalRefinementSession> refinement;
    };

    void post_streaming_phrases(
        StreamingContext & context,
        std::vector<std::string> phrases) {
        for (const auto & phrase : phrases) {
            std::string delta = append_recognizer_segment(context.transcript, phrase);
            if (delta.empty()) {
                continue;
            }
            context.refinement->append_recognized(phrase);
            const bool preserve_recognizer_text =
                contains_japanese_script(phrase) || contains_korean_script(phrase);
            if (context.output_mode != OutputMode::Exact &&
                !preserve_recognizer_text) {
                auto repair = context.refinement->make_request(
                    RefinementStage::RecognitionRepair);
                context.refinement->apply(run_local_recognition_repair(repair));
                auto cleanup = context.refinement->make_request(
                    RefinementStage::SpokenCleanup);
                context.refinement->apply(run_local_spoken_cleanup(cleanup));
            }
            post_worker_message(
                notify_window_, WorkerMessageKind::StreamRevision,
                context.refinement->live_text(),
                context.output_mode == OutputMode::Exact
                    ? GrammarCorrectionStatus::Disabled
                    : (preserve_recognizer_text
                        ? GrammarCorrectionStatus::Skipped
                        : GrammarCorrectionStatus::StandardApplied),
                context.session_id, context.live_delivery,
                context.refinement->revision());
            const std::string adapt_tail =
                context.refinement->revisable_clean_text();
            if (context.output_mode == OutputMode::Adapt &&
                adapt_worker_ != nullptr &&
                !contains_japanese_script(adapt_tail) &&
                !contains_korean_script(adapt_tail)) {
                adapt_worker_->submit_incremental(
                    context.session_id,
                    context.refinement->revision(),
                    context.refinement->committed_clean_text(),
                    adapt_tail,
                    context.app_profile);
            }
        }
    }

    void finish_transcript(
        std::string transcript,
        OutputMode output_mode,
        AppProfile app_profile,
        uint64_t session_id,
        bool streaming) {
        const bool preserve_recognizer_text =
            contains_japanese_script(transcript) ||
            contains_korean_script(transcript);
        if (output_mode != OutputMode::Exact && !transcript.empty() &&
            !preserve_recognizer_text) {
            IncrementalRefinementSession refinement(
                session_id, output_mode, app_profile);
            refinement.append_recognized(transcript);
            auto repair = refinement.make_request(RefinementStage::RecognitionRepair);
            refinement.apply(run_local_recognition_repair(repair));
            auto cleanup = refinement.make_request(RefinementStage::SpokenCleanup);
            refinement.apply(run_local_spoken_cleanup(cleanup));
            transcript = refinement.clean_text();
        }
        // The transcriber already normalizes Chinese/English one phrase at a
        // time and deliberately leaves Japanese/Korean untouched.
        if (output_mode == OutputMode::Adapt && !preserve_recognizer_text) {
            post_worker_message(
                notify_window_, WorkerMessageKind::FinalDraft,
                std::move(transcript), GrammarCorrectionStatus::StandardApplied,
                session_id, streaming);
            return;
        }
        const GrammarCorrectionStatus status = preserve_recognizer_text
            ? GrammarCorrectionStatus::Skipped
            : (output_mode == OutputMode::Clean
                ? GrammarCorrectionStatus::StandardApplied
                : GrammarCorrectionStatus::Disabled);
        post_worker_message(
            notify_window_, WorkerMessageKind::Transcript,
            std::move(transcript), status, session_id, streaming);
    }

    void run() {
        try {
            Transcriber transcriber(model_path_);
            std::optional<StreamingContext> streaming_context;
            post_worker_message(notify_window_, WorkerMessageKind::Ready);

            while (true) {
                RecognitionJob job;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                    if (stopping_) {
                        return;
                    }
                    job = std::move(jobs_.front());
                    jobs_.pop_front();
                }

                try {
                    if (job.kind == JobKind::Transcribe) {
                        finish_transcript(
                            transcriber.transcribe(job.audio, job.speech_languages),
                            job.output_mode,
                            job.app_profile,
                            job.session_id,
                            false);
                    } else if (job.kind == JobKind::StreamStart) {
                        transcriber.start_streaming(job.speech_languages);
                        streaming_context = StreamingContext{
                            job.session_id,
                            job.output_mode,
                            job.app_profile,
                            job.live_delivery,
                            {},
                            std::make_unique<IncrementalRefinementSession>(
                                job.session_id, job.output_mode, job.app_profile),
                        };
                    } else if (job.kind == JobKind::StreamAudio) {
                        if (streaming_context &&
                            streaming_context->session_id == job.session_id) {
                            post_streaming_phrases(
                                *streaming_context,
                                transcriber.accept_streaming_audio(job.audio));
                        }
                    } else if (streaming_context &&
                               streaming_context->session_id == job.session_id) {
                        post_streaming_phrases(
                            *streaming_context, transcriber.finish_streaming());
                        const auto context = std::move(*streaming_context);
                        streaming_context.reset();
                        std::string settled = context.output_mode == OutputMode::Exact
                            ? context.refinement->exact_text()
                            : context.refinement->clean_text();
                        finish_transcript(
                            std::move(settled),
                            context.output_mode,
                            context.app_profile,
                            context.session_id,
                            context.live_delivery);
                    }
                } catch (const std::exception & error) {
                    if (job.kind != JobKind::Transcribe) {
                        streaming_context.reset();
                    }
                    post_worker_message(
                        notify_window_, WorkerMessageKind::Error, error.what(),
                        GrammarCorrectionStatus::Disabled, job.session_id,
                        job.kind != JobKind::Transcribe);
                }
            }
        } catch (const std::exception & error) {
            post_worker_message(notify_window_, WorkerMessageKind::Error, error.what());
        }
    }

    HWND notify_window_ = nullptr;
    std::filesystem::path model_path_;
    AdaptWorker * adapt_worker_ = nullptr;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<RecognitionJob> jobs_;
    bool stopping_ = false;
};

class SaidApp {
public:
    SaidApp(HINSTANCE instance, bool force_onboarding, bool background, bool preview_ui,
            bool preview_first_run, int preview_overlay_state, int preview_page)
        : instance_(instance),
          force_onboarding_(force_onboarding),
          background_(background),
          preview_ui_(preview_ui),
          preview_first_run_(preview_first_run),
          preview_overlay_state_(preview_overlay_state),
          preview_page_(preview_page) {}

    int run() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.hInstance = instance_;
        window_class.lpfnWndProc = SaidApp::window_proc;
        window_class.lpszClassName = kControllerClassName;
        icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_SAID), IMAGE_ICON,
                                              0, 0, LR_DEFAULTSIZE | LR_SHARED));
        if (icon_ == nullptr) {
            icon_ = LoadIconW(nullptr, IDI_APPLICATION);
        }
        window_class.hIcon = icon_;
        window_class.hIconSm = icon_;
        tray_icon_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_SAID_TRAY), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        if (tray_icon_ == nullptr) {
            tray_icon_ = icon_;
        }
        if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 1;
        }

        controller_ = CreateWindowExW(0, kControllerClassName, L"SAID", WS_OVERLAPPED,
                                      0, 0, 0, 0, nullptr, nullptr, instance_, this);
        if (preview_ui_ || preview_overlay_state_ != 0) {
            settings_.use_preview_defaults();
        } else {
            settings_.load();
        }
        shortcut_matcher_.set_binding(
            static_cast<DWORD>(settings_.shortcut()),
            settings_.shortcut_modifiers());
        shortcut_matcher_.seed_modifiers(current_shortcut_modifiers());
        if (controller_ == nullptr || !overlay_.create(instance_) ||
            !setup_.create(instance_, controller_, icon_, &settings_)) {
            return 1;
        }

        if (preview_ui_ || preview_overlay_state_ != 0) {
            if (preview_ui_) {
                setup_.show(preview_first_run_, preview_page_);
            }
            if (preview_overlay_state_ == 1) {
                overlay_.show_transcribing(GetDesktopWindow());
            } else if (preview_overlay_state_ == 2) {
                overlay_.show_listening(GetDesktopWindow(), nullptr, L"Right Alt");
            } else if (preview_overlay_state_ == 3) {
                overlay_.show_notice(GetDesktopWindow(), L"Inserted", 0);
            } else if (preview_overlay_state_ == 4) {
                overlay_.show_error(GetDesktopWindow(),
                                    L"Speech model not found — reinstall SAID or open the model folder", 0);
            } else if (preview_overlay_state_ == 5) {
                overlay_.show_correcting(GetDesktopWindow(), L"Right Alt");
            } else if (preview_overlay_state_ == 6) {
                overlay_.show_listening(
                    GetDesktopWindow(), nullptr, L"Right Alt", true);
            } else if (preview_overlay_state_ == 7) {
                overlay_.show_streaming_paused(
                    GetDesktopWindow(), nullptr, L"Right Alt");
            } else if (preview_overlay_state_ == 8) {
                overlay_.show_finalizing(GetDesktopWindow());
            } else if (preview_overlay_state_ == 9) {
                overlay_.show_listening(
                    GetDesktopWindow(), nullptr, L"Right Alt", true, L"cleaning live");
            } else if (preview_overlay_state_ == 10) {
                overlay_.show_listening(
                    GetDesktopWindow(), nullptr, L"Right Alt", true,
                    L"adapting for Developer prompt");
            } else if (preview_overlay_state_ == 11) {
                overlay_.show_listening(
                    GetDesktopWindow(), nullptr, L"Right Alt", true, L"cleaning · Shell");
            } else if (preview_overlay_state_ == 12) {
                overlay_.show_success(
                    GetDesktopWindow(), L"Inserted", L"Kept the clean version", 0);
            } else if (preview_overlay_state_ == 13) {
                overlay_.show_notice(GetDesktopWindow(), L"No speech detected", 0);
            } else if (preview_overlay_state_ == 14) {
                overlay_.show_success(
                    GetDesktopWindow(), L"Transcript copied",
                    L"Focus changed while processing", 0);
            }
            MSG preview_message{};
            while (GetMessageW(&preview_message, nullptr, 0, 0) > 0) {
                if (setup_.visible() && IsDialogMessageW(setup_.window(), &preview_message)) {
                    continue;
                }
                TranslateMessage(&preview_message);
                DispatchMessageW(&preview_message);
            }
            return static_cast<int>(preview_message.wParam);
        }

        add_tray_icon();
        const std::vector<std::wstring> arguments = command_line_arguments();
        model_path_ = resolve_model_path(arguments);
        grammar_model_path_ = resolve_grammar_model_path(arguments, model_path_);
        if (grammar_model_path_ == expected_grammar_model_path() &&
            !advanced_model::has_expected_size(*grammar_model_path_)) {
            grammar_model_path_.reset();
        }
        advanced_model_state_ = grammar_model_path_
            ? AdvancedModelState::Installed
            : AdvancedModelState::NotInstalled;
        setup_.set_advanced_model_state(advanced_model_state_);
        if (model_path_) {
            set_tray_tip(L"SAID — loading speech model");
            adapt_worker_ = std::make_unique<AdaptWorker>(
                controller_, grammar_model_path_);
            worker_ = std::make_unique<RecognitionWorker>(
                controller_, *model_path_, adapt_worker_.get());
        } else {
            set_state(State::ModelError);
            set_tray_tip(L"SAID — speech model missing");
        }

        if (!grammar_model_path_ && settings_.advanced_model_download_pending()) {
            settings_.set_output_mode(OutputMode::Adapt);
            start_advanced_model_download();
        } else if (!grammar_model_path_ &&
                   settings_.output_mode() == OutputMode::Adapt) {
            settings_.set_output_mode(OutputMode::Clean);
            advanced_model_fallback_notice_pending_ = true;
            setup_.refresh_grammar_controls();
        }

        hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, instance_, 0);
        if (hook_ == nullptr) {
            MessageBoxW(nullptr, L"SAID could not register its keyboard shortcut.", L"SAID", MB_OK | MB_ICONERROR);
            return 1;
        }
        hook_target_ = controller_;

        if (force_onboarding_ || (!background_ && !settings_.onboarding_complete())) {
            setup_.show(!settings_.onboarding_complete());
        }

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (setup_.visible() && IsDialogMessageW(setup_.window(), &message)) {
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        hook_target_ = nullptr;
        end_streaming_target_guard();
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
        audio_.stop();
        worker_.reset();
        remove_tray_icon();
        return static_cast<int>(message.wParam);
    }

private:
    enum class State {
        Loading,
        Ready,
        Recording,
        Transcribing,
        Correcting,
        ModelError,
    };

    static inline HWND hook_target_ = nullptr;
    static inline ShortcutPressMatcher shortcut_matcher_{};
    static inline std::atomic<bool> track_external_input_{false};
    static inline std::atomic<bool> external_input_detected_{false};

    static DWORD modifier_for_key(DWORD virtual_key) {
        switch (virtual_key) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            return kShortcutModifierControl;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
            return kShortcutModifierAlt;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
            return kShortcutModifierShift;
        case VK_LWIN:
        case VK_RWIN:
            return kShortcutModifierWindows;
        default:
            return 0;
        }
    }

    static DWORD current_shortcut_modifiers() {
        DWORD modifiers = 0;
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= kShortcutModifierControl;
        if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) modifiers |= kShortcutModifierAlt;
        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= kShortcutModifierShift;
        if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) {
            modifiers |= kShortcutModifierWindows;
        }
        return modifiers;
    }

    static LRESULT CALLBACK keyboard_proc(int code, WPARAM wparam, LPARAM lparam) {
        if (code == HC_ACTION) {
            const auto * key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
            const bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
            const bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
            if (!down && !up) {
                return CallNextHookEx(nullptr, code, wparam, lparam);
            }
            const bool right_alt = key->vkCode == VK_RMENU ||
                (key->vkCode == VK_MENU && (key->flags & LLKHF_EXTENDED) != 0);
            const bool injected = (key->flags & LLKHF_INJECTED) != 0;
            const ShortcutHookAction action = shortcut_matcher_.handle_event(
                key->vkCode,
                modifier_for_key(key->vkCode),
                right_alt,
                down ? ShortcutKeyEvent::Down : ShortcutKeyEvent::Up,
                injected,
                track_external_input_.load(std::memory_order_relaxed));
            if (action.external_input) {
                external_input_detected_.store(true, std::memory_order_release);
                if (hook_target_ != nullptr) {
                    PostMessageW(hook_target_, kMessageExternalInput, 0, 0);
                }
            }
            if (action.toggle && hook_target_ != nullptr) {
                PostMessageW(hook_target_, kMessageToggle,
                             static_cast<WPARAM>(key->vkCode), 0);
            }
            if (action.consume) {
                return 1;
            }
        }
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }

    static LRESULT CALLBACK mouse_proc(int code, WPARAM wparam, LPARAM lparam) {
        if (code == HC_ACTION && track_external_input_.load(std::memory_order_relaxed)) {
            const bool interaction = wparam == WM_LBUTTONDOWN || wparam == WM_RBUTTONDOWN ||
                wparam == WM_MBUTTONDOWN || wparam == WM_XBUTTONDOWN ||
                wparam == WM_MOUSEWHEEL || wparam == WM_MOUSEHWHEEL;
            if (interaction) {
                external_input_detected_.store(true, std::memory_order_release);
                if (hook_target_ != nullptr) {
                    PostMessageW(hook_target_, kMessageExternalInput, 0, 0);
                }
            }
        }
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        SaidApp * self = reinterpret_cast<SaidApp *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto * create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
            self = static_cast<SaidApp *>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->controller_ = window;
        }
        if (self != nullptr) {
            return self->handle_message(message, wparam, lparam);
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case kMessageToggle:
            if (setup_.handle_custom_shortcut_key(static_cast<DWORD>(wparam))) {
                return 0;
            }
            if (!setup_.handle_shortcut_pressed(settings_.shortcut())) {
                toggle_recording();
            }
            return 0;
        case kMessageShortcutChanged:
            shortcut_matcher_.set_binding(
                static_cast<DWORD>(wparam),
                static_cast<DWORD>(lparam));
            return 0;
        case kMessageOutputModeRequested:
            request_output_mode(
                static_cast<OutputMode>(wparam),
                reinterpret_cast<HWND>(lparam));
            return 0;
        case kMessageAdvancedModel: {
            std::unique_ptr<AdvancedModelDownloadEvent> event(
                reinterpret_cast<AdvancedModelDownloadEvent *>(lparam));
            handle_advanced_model_event(*event);
            return 0;
        }
        case kMessageWorker: {
            std::unique_ptr<WorkerMessage> worker_message(reinterpret_cast<WorkerMessage *>(lparam));
            handle_worker_message(*worker_message);
            return 0;
        }
        case kMessageExternalInput:
            revoke_streaming_ownership();
            return 0;
        case kMessageTray:
            if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
                show_tray_menu();
            } else if (LOWORD(lparam) == WM_LBUTTONDBLCLK) {
                setup_.show(false);
            }
            return 0;
        case WM_COMMAND:
            handle_command(LOWORD(wparam));
            return 0;
        case WM_TIMER:
            if (wparam == kStreamingAudioTimer) {
                pump_streaming_audio();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(controller_, message, wparam, lparam);
        }
    }

    void toggle_recording() {
        switch (state_) {
        case State::Ready:
            start_recording();
            break;
        case State::Recording:
            finish_recording();
            break;
        case State::Loading:
            overlay_.show_notice(GetForegroundWindow(), L"Loading the speech model…", 1800);
            break;
        case State::Transcribing:
            if (streaming_dictation_) {
                overlay_.show_finalizing(target_window_);
            } else {
                overlay_.show_transcribing(target_window_);
            }
            break;
        case State::Correcting:
            insert_verbatim_now();
            break;
        case State::ModelError:
            overlay_.show_error(GetForegroundWindow(), L"Speech model not found — reinstall SAID or open the model folder", 4000);
            break;
        }
    }

    void start_recording() {
        target_window_ = GetForegroundWindow();
        target_control_ = focused_control(target_window_);
        active_app_identity_ = app_identity_for_window(target_window_);
        active_app_profile_ = resolve_app_profile(
            active_app_identity_, settings_.app_profile_override(active_app_identity_));
        std::string error;
        if (!audio_.start(error)) {
            overlay_.show_error(target_window_, utf8_to_wide(error));
            return;
        }
        active_session_id_ = ++next_session_id_;
        active_output_mode_ = settings_.output_mode();
        active_speech_languages_ = settings_.speech_languages();
        streaming_dictation_ = settings_.streaming_mode_enabled();
        incremental_recognition_ =
            streaming_dictation_ || active_output_mode_ != OutputMode::Exact;
        stream_audio_offset_ = 0;
        inserted_live_text_.clear();
        stream_owns_target_ = streaming_dictation_;
        if (incremental_recognition_) {
            worker_->start_streaming(
                active_session_id_, active_output_mode_,
                active_app_profile_, streaming_dictation_,
                active_speech_languages_);
            if (streaming_dictation_) {
                begin_streaming_target_guard();
            }
            SetTimer(
                controller_, kStreamingAudioTimer,
                kStreamingAudioPollMilliseconds, nullptr);
        }
        set_state(State::Recording);
        set_tray_tip(std::wstring(L"SAID — listening · ") +
                     shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) + L" to finish");
        if (streaming_dictation_ && !stream_owns_target_) {
            overlay_.show_streaming_paused(
                target_window_, &audio_,
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()));
        } else {
            std::wstring refinement;
            if (active_output_mode_ == OutputMode::Clean) {
                refinement = streaming_dictation_ ? L"cleaning live" : L"cleaning";
            } else if (active_output_mode_ == OutputMode::Adapt &&
                       app_profile_allows_adapt(active_app_profile_)) {
                refinement = std::wstring(L"adapting for ") +
                    app_profile_name(active_app_profile_);
            } else if (active_output_mode_ == OutputMode::Adapt) {
                refinement = std::wstring(L"cleaning · ") +
                    app_profile_name(active_app_profile_);
            }
            overlay_.show_listening(
                target_window_, &audio_,
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()),
                streaming_dictation_, std::move(refinement));
        }
    }

    void finish_recording() {
        const bool reached_limit = audio_.reached_limit();
        if (incremental_recognition_) {
            KillTimer(controller_, kStreamingAudioTimer);
        }
        std::vector<float> samples = audio_.stop();
        set_state(State::Transcribing);
        if (incremental_recognition_) {
            if (stream_audio_offset_ < samples.size()) {
                std::vector<float> tail(
                    samples.begin() + static_cast<std::ptrdiff_t>(stream_audio_offset_),
                    samples.end());
                stream_audio_offset_ = samples.size();
                worker_->push_streaming_audio(active_session_id_, std::move(tail));
            }
            worker_->finish_streaming(active_session_id_);
            set_tray_tip(streaming_dictation_
                ? L"SAID — finishing live transcript"
                : L"SAID — finishing transcript");
            if (streaming_dictation_) {
                overlay_.show_finalizing(target_window_);
            } else {
                overlay_.show_transcribing(target_window_);
            }
        } else {
            set_tray_tip(L"SAID — transcribing");
            overlay_.show_transcribing(target_window_);
            worker_->submit(
                std::move(samples), active_output_mode_,
                active_app_profile_,
                active_session_id_,
                active_speech_languages_);
        }
        if (reached_limit) {
            overlay_.show_error(target_window_, L"Ten-minute limit reached; transcribing captured audio", 2600);
        }
    }

    void pump_streaming_audio() {
        if (!incremental_recognition_ || state_ != State::Recording || !worker_) {
            return;
        }
        std::vector<float> samples = audio_.samples_since(stream_audio_offset_);
        if (samples.empty()) {
            return;
        }
        stream_audio_offset_ += samples.size();
        worker_->push_streaming_audio(active_session_id_, std::move(samples));
    }

    void begin_streaming_target_guard() {
        external_input_detected_.store(false, std::memory_order_release);
        track_external_input_.store(true, std::memory_order_release);
        mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, instance_, 0);
        if (mouse_hook_ == nullptr || target_control_ == nullptr) {
            stream_owns_target_ = false;
        }
    }

    void end_streaming_target_guard() {
        track_external_input_.store(false, std::memory_order_release);
        external_input_detected_.store(false, std::memory_order_release);
        if (mouse_hook_ != nullptr) {
            UnhookWindowsHookEx(mouse_hook_);
            mouse_hook_ = nullptr;
        }
    }

    bool can_modify_streaming_target() const {
        return streaming_dictation_ && stream_owns_target_ &&
            !external_input_detected_.load(std::memory_order_acquire) &&
            foreground_focus_matches(target_window_, target_control_);
    }

    void revoke_streaming_ownership() {
        if (!streaming_dictation_ || !stream_owns_target_) {
            return;
        }
        stream_owns_target_ = false;
        if (state_ == State::Recording) {
            overlay_.show_streaming_paused(
                target_window_, &audio_,
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()));
        }
    }

    void handle_worker_message(const WorkerMessage & message) {
        if (message.kind == WorkerMessageKind::Ready) {
            set_state(State::Ready);
            set_tray_tip(std::wstring(L"SAID — ready · ") +
                         shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) +
                         L" to dictate");
            if (advanced_model_fallback_notice_pending_) {
                advanced_model_fallback_notice_pending_ = false;
                overlay_.show_notice(
                    GetForegroundWindow(),
                    L"Adapt model unavailable — Clean is active",
                    3200);
            }
            return;
        }

        if (ignored_transcript_session_ &&
            message.kind == WorkerMessageKind::Transcript &&
            message.session_id == *ignored_transcript_session_) {
            ignored_transcript_session_.reset();
            return;
        }
        if (message.session_id != 0 && message.session_id != active_session_id_) {
            return;
        }

        if (message.kind == WorkerMessageKind::StreamRevision) {
            latest_stream_revision_ = message.revision;
            handle_streaming_revision(message.text);
            return;
        }

        if (message.kind == WorkerMessageKind::AdaptRevision) {
            if (active_output_mode_ == OutputMode::Adapt &&
                message.revision == latest_stream_revision_) {
                handle_streaming_revision(message.text);
            }
            return;
        }

        if (message.kind == WorkerMessageKind::FinalDraft) {
            if (active_output_mode_ == OutputMode::Adapt &&
                grammar_model_path_ && adapt_worker_ &&
                app_profile_allows_adapt(active_app_profile_)) {
                pending_verbatim_transcript_ = message.text;
                set_state(State::Correcting);
                set_tray_tip(
                    std::wstring(L"SAID — finishing structure · ") +
                    shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) +
                    L" keeps the clean draft");
                overlay_.show_correcting(
                    target_window_,
                    shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()),
                    message.streaming);
                adapt_worker_->submit_final(
                    message.session_id, message.text, active_app_profile_, message.streaming);
            } else if (message.streaming) {
                complete_streaming_delivery(
                    message.text, GrammarCorrectionStatus::AdvancedFallback);
            } else {
                deliver_transcript(
                    message.text, GrammarCorrectionStatus::AdvancedFallback);
            }
            return;
        }

        if (message.kind == WorkerMessageKind::Correcting) {
            pending_verbatim_transcript_ = message.text;
            set_state(State::Correcting);
            set_tray_tip(
                std::wstring(L"SAID — finishing structure · ") +
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) +
                L" keeps the clean draft");
            overlay_.show_correcting(
                target_window_,
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()),
                message.streaming);
            return;
        }

        if (message.kind == WorkerMessageKind::Error) {
            if (state_ == State::Loading) {
                set_state(State::ModelError);
            } else {
                if (message.streaming || incremental_recognition_) {
                    KillTimer(controller_, kStreamingAudioTimer);
                    audio_.stop();
                    clear_streaming_session();
                }
                set_state(State::Ready);
            }
            set_tray_tip(state_ == State::ModelError ? L"SAID — model error" : L"SAID — ready");
            overlay_.show_error(target_window_ != nullptr ? target_window_ : GetForegroundWindow(),
                                utf8_to_wide(message.text));
            return;
        }

        pending_verbatim_transcript_.clear();
        if (message.streaming) {
            complete_streaming_delivery(message.text, message.grammar_status);
        } else {
            deliver_transcript(message.text, message.grammar_status);
        }
    }

    void handle_streaming_revision(const std::string & text) {
        if (!streaming_dictation_ || (text.empty() && inserted_live_text_.empty())) {
            return;
        }
        const StreamingRevisionDisposition disposition =
            plan_streaming_revision_delivery(
                inserted_live_text_, text, can_modify_streaming_target());
        if (disposition == StreamingRevisionDisposition::Pause) {
            revoke_streaming_ownership();
            return;
        }

        std::wstring error;
        bool delivered = true;
        if (disposition == StreamingRevisionDisposition::Insert) {
            delivered = inject_utf8_text(text, error);
        } else if (disposition == StreamingRevisionDisposition::ReplaceOwnedText) {
            delivered = replace_recent_utf8_text(inserted_live_text_, text, error);
        }
        if (delivered) {
            inserted_live_text_ = text;
        } else {
            revoke_streaming_ownership();
        }
    }

    void insert_verbatim_now() {
        if (pending_verbatim_transcript_.empty()) {
            overlay_.show_correcting(
                target_window_,
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()),
                streaming_dictation_);
            return;
        }

        if (adapt_worker_) {
            adapt_worker_->cancel(active_session_id_);
        }
        ignored_transcript_session_ = active_session_id_;
        std::string transcript = std::move(pending_verbatim_transcript_);
        pending_verbatim_transcript_.clear();
        if (streaming_dictation_) {
            complete_streaming_delivery(transcript, GrammarCorrectionStatus::Skipped);
        } else {
            deliver_transcript(transcript, GrammarCorrectionStatus::Skipped);
        }
    }

    void set_ready_after_dictation() {
        set_state(State::Ready);
        set_tray_tip(std::wstring(L"SAID — ready · ") +
                     shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) +
                     L" to dictate");
    }

    void clear_streaming_session() {
        end_streaming_target_guard();
        streaming_dictation_ = false;
        incremental_recognition_ = false;
        stream_owns_target_ = false;
        stream_audio_offset_ = 0;
        latest_stream_revision_ = 0;
        inserted_live_text_.clear();
        pending_verbatim_transcript_.clear();
        active_session_id_ = 0;
    }

    void complete_streaming_delivery(
        const std::string & text,
        GrammarCorrectionStatus grammar_status) {
        const bool had_live_text = !inserted_live_text_.empty();
        const bool final_text_matches_live = text == inserted_live_text_;
        StreamingFinalDisposition disposition = plan_streaming_final_delivery(
            inserted_live_text_, text, can_modify_streaming_target());

        std::wstring error;
        bool polished_in_place = false;
        if (disposition == StreamingFinalDisposition::ReplaceLiveText) {
            polished_in_place = replace_recent_utf8_text(
                inserted_live_text_, text, error);
            if (!polished_in_place) {
                stream_owns_target_ = false;
                disposition = StreamingFinalDisposition::CopyFinalText;
            }
        } else if (disposition == StreamingFinalDisposition::KeepLiveText &&
                   grammar_correction_applied(grammar_status)) {
            polished_in_place = true;
        }

        if (disposition == StreamingFinalDisposition::NoSpeech) {
            clear_streaming_session();
            set_ready_after_dictation();
            overlay_.show_notice(target_window_, L"No speech detected", 1600);
            return;
        }

        if (disposition == StreamingFinalDisposition::KeepLiveText ||
            disposition == StreamingFinalDisposition::ReplaceLiveText) {
            const HWND result_target = target_window_;
            clear_streaming_session();
            set_ready_after_dictation();
            if (polished_in_place) {
                const wchar_t * detail = grammar_status == GrammarCorrectionStatus::AdvancedApplied
                    ? L"Adapted locally"
                    : (grammar_status == GrammarCorrectionStatus::AdvancedFallback
                        ? L"Kept the clean version"
                        : L"Cleaned speech mistakes");
                overlay_.show_success(
                    result_target,
                    final_text_matches_live ? L"Checked in place" : L"Refined in place",
                    detail, 1500);
            } else if (grammar_status == GrammarCorrectionStatus::Skipped) {
                overlay_.show_success(
                    result_target, L"Inserted live verbatim",
                    L"Adapt skipped", 1400);
            } else {
                overlay_.show_success(
                    result_target, L"Inserted live",
                    L"Ready for the next thought", 900);
            }
            return;
        }

        const HWND result_target = GetForegroundWindow();
        const bool copied = copy_utf8_text(text, error);
        clear_streaming_session();
        set_ready_after_dictation();
        if (copied) {
            overlay_.show_success(
                result_target,
                had_live_text ? L"Final transcript copied" : L"Transcript copied",
                had_live_text ? L"Live text was left in place" : L"Focus changed while processing",
                2800);
        } else {
            overlay_.show_error(result_target, error);
        }
    }

    void deliver_transcript(
        const std::string & text,
        GrammarCorrectionStatus grammar_status) {
        active_session_id_ = 0;
        incremental_recognition_ = false;
        streaming_dictation_ = false;
        set_ready_after_dictation();
        if (text.empty()) {
            overlay_.show_notice(target_window_, L"No speech detected", 1600);
            return;
        }

        std::wstring error;
        const HWND foreground = GetForegroundWindow();
        if (foreground != target_window_) {
            if (copy_utf8_text(text, error)) {
                show_delivery_result(foreground, true, grammar_status);
            } else {
                overlay_.show_error(foreground, error);
            }
            return;
        }

        if (inject_utf8_text(text, error)) {
            show_delivery_result(target_window_, false, grammar_status);
        } else {
            std::wstring clipboard_error;
            if (copy_utf8_text(text, clipboard_error)) {
                overlay_.show_error(target_window_, error + L" Transcript copied.", 3500);
            } else {
                overlay_.show_error(target_window_, error, 3500);
            }
        }
    }

    void show_delivery_result(
        HWND target,
        bool copied,
        GrammarCorrectionStatus grammar_status) {
        const std::wstring action = copied ? L"Transcript copied" : L"Inserted";
        if (grammar_status == GrammarCorrectionStatus::AdvancedApplied) {
            overlay_.show_success(
                target, action, L"Adapted for this app on your PC",
                copied ? 2600 : 1400);
        } else if (grammar_status == GrammarCorrectionStatus::StandardApplied) {
            overlay_.show_success(
                target, action, L"Cleaned speech mistakes",
                copied ? 2400 : 1200);
        } else if (grammar_status == GrammarCorrectionStatus::AdvancedFallback) {
            overlay_.show_success(
                target, action, L"Kept the clean version",
                copied ? 2600 : 1600);
        } else if (grammar_status == GrammarCorrectionStatus::Skipped) {
            overlay_.show_success(target, action + L" verbatim",
                                  L"Adapt skipped", copied ? 2600 : 1400);
        } else {
            overlay_.show_success(target, action,
                                  copied ? L"Focus changed while processing" : L"Ready for the next thought",
                                  copied ? 2400 : 850);
        }
    }

    void add_tray_icon() {
        tray_ = {};
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = controller_;
        tray_.uID = kTrayId;
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        tray_.uCallbackMessage = kMessageTray;
        tray_.hIcon = tray_icon_;
        lstrcpynW(tray_.szTip, L"SAID — starting", static_cast<int>(std::size(tray_.szTip)));
        Shell_NotifyIconW(NIM_ADD, &tray_);
        tray_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray_);
    }

    void remove_tray_icon() {
        if (tray_.hWnd != nullptr) {
            Shell_NotifyIconW(NIM_DELETE, &tray_);
            tray_.hWnd = nullptr;
        }
    }

    void set_tray_tip(const std::wstring & text) {
        tray_.uFlags = NIF_TIP | NIF_SHOWTIP;
        lstrcpynW(tray_.szTip, text.c_str(), static_cast<int>(std::size(tray_.szTip)));
        Shell_NotifyIconW(NIM_MODIFY, &tray_);
    }

    void set_state(State state) {
        state_ = state;
        if (controller_ == nullptr) {
            return;
        }
        switch (state_) {
        case State::Loading: SetWindowTextW(controller_, L"SAID:loading"); break;
        case State::Ready: SetWindowTextW(controller_, L"SAID:ready"); break;
        case State::Recording: SetWindowTextW(controller_, L"SAID:recording"); break;
        case State::Transcribing: SetWindowTextW(controller_, L"SAID:transcribing"); break;
        case State::Correcting: SetWindowTextW(controller_, L"SAID:correcting"); break;
        case State::ModelError: SetWindowTextW(controller_, L"SAID:model-error"); break;
        }
    }

    bool confirm_advanced_model_download(HWND owner) const {
        const HWND dialog_owner = owner != nullptr && IsWindowVisible(owner) ? owner : nullptr;
        constexpr int kDownloadButton = 1001;
        const TASKDIALOG_BUTTON buttons[]{
            {kDownloadButton, L"Download 639 MB and enable Adapt"},
            {IDCANCEL, L"Not now"},
        };
        TASKDIALOGCONFIG dialog{};
        dialog.cbSize = sizeof(dialog);
        dialog.hwndParent = dialog_owner;
        dialog.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_USE_COMMAND_LINKS;
        dialog.pszWindowTitle = L"SAID — Adapt";
        dialog.pszMainInstruction = L"Adapt uses an optional local writing model.";
        dialog.pszContent =
            L"Model: Qwen3 0.6B Q8\n"
            L"Download: 639 MB\n"
            L"Disk space required: about 850 MB\n"
            L"Memory while warm: about 1.7 GB\n"
            L"System RAM: 16 GB recommended\n"
            L"Runs locally after installation\n\n"
            L"Adapt may organize and rephrase dictated text for the current app. "
            L"You can remove it later. Clean remains active if Adapt is unavailable "
            L"or a rewrite fails SAID's safety check.";
        dialog.cButtons = static_cast<UINT>(std::size(buttons));
        dialog.pButtons = buttons;
        dialog.nDefaultButton = IDCANCEL;
        int selected = IDCANCEL;
        using TaskDialogIndirectFunction = HRESULT (WINAPI *)(
            const TASKDIALOGCONFIG *, int *, int *, BOOL *);
        HMODULE common_controls = LoadLibraryW(L"comctl32.dll");
        if (common_controls != nullptr) {
            const auto task_dialog = reinterpret_cast<TaskDialogIndirectFunction>(
                GetProcAddress(common_controls, "TaskDialogIndirect"));
            if (task_dialog != nullptr) {
                const HRESULT result = task_dialog(&dialog, &selected, nullptr, nullptr);
                FreeLibrary(common_controls);
                if (SUCCEEDED(result)) {
                    return selected == kDownloadButton;
                }
            } else {
                FreeLibrary(common_controls);
            }
        }
        return MessageBoxW(
                   dialog_owner,
                   L"Adapt downloads a 639 MB local writing model and requires about "
                   L"850 MB of free disk space. Once warm, SAID uses about 1.7 GB of "
                   L"memory; 16 GB of system RAM is recommended. It works offline "
                   L"afterward and can be removed later.\n\nDownload and enable Adapt?",
                   L"SAID — Adapt",
                   MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2) == IDYES;
    }

    bool has_space_for_advanced_model(HWND owner) {
        const std::filesystem::path destination = expected_grammar_model_path();
        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
        ULARGE_INTEGER available{};
        if (!GetDiskFreeSpaceExW(destination.parent_path().c_str(), &available, nullptr, nullptr) ||
            available.QuadPart < advanced_model::kRequiredFreeBytes) {
            overlay_.show_error(
                owner != nullptr ? owner : GetForegroundWindow(),
                L"Adapt needs at least 850 MB of free disk space.", 4200);
            return false;
        }
        return true;
    }

    bool start_advanced_model_download() {
        advanced_model_state_ = AdvancedModelState::Downloading;
        setup_.set_advanced_model_state(advanced_model_state_, 0);
        if (!advanced_model_downloader_.start(
                controller_, kMessageAdvancedModel, expected_grammar_model_path())) {
            advanced_model_state_ = AdvancedModelState::Failed;
            setup_.set_advanced_model_state(advanced_model_state_);
            settings_.set_advanced_model_download_pending(false);
            settings_.set_output_mode(OutputMode::Clean);
            return false;
        }
        return true;
    }

    void request_output_mode(OutputMode mode, HWND owner) {
        if (mode != OutputMode::Exact &&
            mode != OutputMode::Clean &&
            mode != OutputMode::Adapt) {
            return;
        }
        if (mode != OutputMode::Adapt) {
            if (advanced_model_downloader_.active()) {
                advanced_model_downloader_.cancel();
            }
            settings_.set_advanced_model_download_pending(false);
            settings_.set_output_mode(mode);
            setup_.refresh_grammar_controls();
            overlay_.show_notice(
                owner != nullptr ? owner : GetForegroundWindow(),
                std::wstring(L"Output · ") + output_mode_name(mode),
                1800);
            return;
        }

        const std::filesystem::path expected = expected_grammar_model_path();
        if (!grammar_model_path_ && advanced_model::has_expected_size(expected)) {
            grammar_model_path_ = expected;
            if (adapt_worker_) adapt_worker_->set_model_path(grammar_model_path_);
            advanced_model_state_ = AdvancedModelState::Installed;
        }
        if (grammar_model_path_) {
            settings_.set_output_mode(OutputMode::Adapt);
            settings_.set_advanced_model_download_pending(false);
            setup_.set_advanced_model_state(AdvancedModelState::Installed);
            overlay_.show_notice(
                owner != nullptr ? owner : GetForegroundWindow(),
                L"Adapt on · runs locally", 2200);
            return;
        }
        if (advanced_model_downloader_.active()) {
            if (owner == setup_.window()) {
                advanced_model_downloader_.cancel();
                overlay_.show_notice(owner, L"Cancelling Adapt download…", 1800);
                return;
            }
            settings_.set_output_mode(OutputMode::Adapt);
            setup_.refresh_grammar_controls();
            overlay_.show_notice(
                owner != nullptr ? owner : GetForegroundWindow(),
                L"Adapt model is still downloading · Clean is active meanwhile",
                2800);
            return;
        }
        if (!confirm_advanced_model_download(owner) || !has_space_for_advanced_model(owner)) {
            setup_.refresh_grammar_controls();
            return;
        }
        settings_.set_output_mode(OutputMode::Adapt);
        settings_.set_advanced_model_download_pending(true);
        if (start_advanced_model_download()) {
            overlay_.show_notice(
                owner != nullptr ? owner : GetForegroundWindow(),
                L"Downloading Adapt · Clean stays active until it is ready",
                3000);
        }
    }

    void handle_advanced_model_event(const AdvancedModelDownloadEvent & event) {
        advanced_model_state_ = event.state;
        const int percent = event.total == 0
            ? 0
            : static_cast<int>(std::min<uint64_t>(100, event.transferred * 100 / event.total));
        setup_.set_advanced_model_state(event.state, percent);
        if (event.state == AdvancedModelState::Installed) {
            grammar_model_path_ = expected_grammar_model_path();
            if (adapt_worker_) adapt_worker_->set_model_path(grammar_model_path_);
            settings_.set_advanced_model_download_pending(false);
            settings_.set_output_mode(OutputMode::Adapt);
            setup_.refresh_grammar_controls();
            overlay_.show_success(
                GetForegroundWindow(), L"Adapt is ready",
                L"The local language model is installed", 3200);
        } else if (event.state == AdvancedModelState::Failed ||
                   event.state == AdvancedModelState::Cancelled) {
            settings_.set_advanced_model_download_pending(false);
            settings_.set_output_mode(OutputMode::Clean);
            setup_.refresh_grammar_controls();
            if (event.state == AdvancedModelState::Failed) {
                overlay_.show_error(
                    GetForegroundWindow(),
                    L"Adapt download failed — Clean remains active",
                    4200);
            } else {
                overlay_.show_notice(
                    GetForegroundWindow(),
                    L"Adapt download cancelled · Clean is active",
                    2600);
            }
        }
    }

    void remove_advanced_model() {
        if (advanced_model_downloader_.active()) {
            advanced_model_downloader_.cancel();
        }
        settings_.set_advanced_model_download_pending(false);
        settings_.set_output_mode(OutputMode::Clean);
        grammar_model_path_.reset();
        if (adapt_worker_) adapt_worker_->set_model_path(std::nullopt);
        const std::filesystem::path path = expected_grammar_model_path();
        std::error_code error;
        const bool removed = std::filesystem::remove(path, error) ||
                             !std::filesystem::exists(path, error);
        if (!removed) {
            MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
        std::filesystem::remove(path.wstring() + L".download", error);
        advanced_model_state_ = AdvancedModelState::NotInstalled;
        setup_.set_advanced_model_state(advanced_model_state_);
        overlay_.show_notice(
            GetForegroundWindow(),
            removed ? L"Adapt model removed · Clean is active"
                    : L"Adapt model will be removed after Windows restarts",
            3400);
    }

    void show_tray_menu() {
        tray_context_identity_ = app_identity_for_window(GetForegroundWindow());
        POINT cursor{};
        GetCursorPos(&cursor);
        HMENU menu = CreatePopupMenu();
        const std::wstring status = status_text();
        AppendMenuW(menu, MF_STRING | MF_GRAYED, kMenuStatus, status.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuSetup, L"Setup && settings…");
        AppendMenuW(menu, MF_STRING | (settings_.streaming_mode_enabled() ? MF_CHECKED : 0),
                    kMenuStreaming, L"Streaming mode");
        const OutputMode output_mode = settings_.output_mode();
        HMENU output_menu = CreatePopupMenu();
        AppendMenuW(output_menu,
                    MF_STRING | (output_mode == OutputMode::Exact ? MF_CHECKED : 0),
                    kMenuOutputExact, L"Exact — recognizer text");
        AppendMenuW(output_menu,
                    MF_STRING | (output_mode == OutputMode::Clean ? MF_CHECKED : 0),
                    kMenuOutputClean, L"Clean — fix speech mistakes");
        std::wstring advanced_label = L"Adapt — download local writing model…";
        if (advanced_model_state_ == AdvancedModelState::Installed) {
            advanced_label = L"Adapt — local model installed";
        } else if (advanced_model_state_ == AdvancedModelState::Downloading ||
                   advanced_model_state_ == AdvancedModelState::Verifying) {
            advanced_label = L"Adapt — downloading…";
        }
        AppendMenuW(output_menu,
                    MF_STRING | (output_mode == OutputMode::Adapt ? MF_CHECKED : 0),
                    kMenuOutputAdapt, advanced_label.c_str());
        if (advanced_model_downloader_.active()) {
            AppendMenuW(output_menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(output_menu, MF_STRING, kMenuCancelAdvancedDownload, L"Cancel download");
        } else if (advanced_model_state_ == AdvancedModelState::Installed &&
                   grammar_model_path_ == expected_grammar_model_path()) {
            AppendMenuW(output_menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(output_menu, MF_STRING, kMenuRemoveAdvancedModel,
                        L"Remove Adapt model (639 MB)");
        }
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(output_menu),
                    L"Output");

        const AppProfile automatic_profile = classify_app_identity(tray_context_identity_);
        const auto profile_override = settings_.app_profile_override(tray_context_identity_);
        const AppProfile effective_profile = profile_override.value_or(automatic_profile);
        HMENU profile_menu = CreatePopupMenu();
        std::wstring automatic_label = std::wstring(L"Automatic — ") +
            app_profile_name(automatic_profile);
        AppendMenuW(profile_menu,
                    MF_STRING | (!profile_override ? MF_CHECKED : 0),
                    kMenuProfileAuto, automatic_label.c_str());
        const auto append_profile = [&](UINT command, AppProfile profile) {
            AppendMenuW(profile_menu,
                        MF_STRING | (profile_override && effective_profile == profile
                            ? MF_CHECKED : 0),
                        command, app_profile_name(profile));
        };
        append_profile(kMenuProfileChat, AppProfile::Chat);
        append_profile(kMenuProfileMail, AppProfile::Mail);
        append_profile(kMenuProfileDocument, AppProfile::Document);
        append_profile(kMenuProfileDeveloper, AppProfile::DeveloperPrompt);
        append_profile(kMenuProfileCode, AppProfile::CodeEditor);
        append_profile(kMenuProfileShell, AppProfile::Shell);
        const std::wstring profile_heading = tray_context_identity_.executable.empty()
            ? L"Application behavior"
            : std::wstring(L"Application · ") + tray_context_identity_.executable;
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(profile_menu),
                    profile_heading.c_str());
        AppendMenuW(menu, MF_STRING, kMenuOpenModels, L"Open model folder");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuQuit, L"Quit SAID");
        SetForegroundWindow(controller_);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                            cursor.x, cursor.y, 0, controller_, nullptr);
        DestroyMenu(menu);
        if (command != 0) {
            handle_command(command);
        }
    }

    std::wstring status_text() const {
        switch (state_) {
        case State::Loading: return L"Loading model…";
        case State::Ready:
            return std::wstring(L"Ready — ") +
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) + L" to dictate";
        case State::Recording:
            return streaming_dictation_
                ? std::wstring(L"Listening live — ") +
                    shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) + L" to finish"
                : std::wstring(L"Listening — ") +
                    shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) + L" to finish";
        case State::Transcribing:
            return streaming_dictation_ ? L"Finishing live transcript…" : L"Transcribing…";
        case State::Correcting:
            return std::wstring(L"Finishing structure — ") +
                shortcut_name(settings_.shortcut(), settings_.shortcut_modifiers()) +
                L" keeps the clean draft";
        case State::ModelError: return L"Speech model missing or invalid";
        }
        return L"SAID";
    }

    void show_current_status() {
        const HWND target = GetForegroundWindow();
        if (state_ == State::ModelError) {
            overlay_.show_error(target, L"Speech model not found — reinstall SAID or open the model folder", 4000);
        } else {
            overlay_.show_notice(target, status_text(), 2200);
        }
    }

    void handle_command(UINT command) {
        if (command == kMenuQuit) {
            DestroyWindow(controller_);
        } else if (command == kMenuSetup) {
            setup_.show(false);
        } else if (command == kMenuOutputExact ||
                   command == kMenuOutputClean ||
                   command == kMenuOutputAdapt) {
            const OutputMode mode = command == kMenuOutputExact
                ? OutputMode::Exact
                : (command == kMenuOutputClean
                    ? OutputMode::Clean
                    : OutputMode::Adapt);
            request_output_mode(mode, controller_);
        } else if (command == kMenuCancelAdvancedDownload) {
            advanced_model_downloader_.cancel();
        } else if (command == kMenuRemoveAdvancedModel) {
            remove_advanced_model();
        } else if (command == kMenuStreaming) {
            settings_.set_streaming_mode_enabled(!settings_.streaming_mode_enabled());
            overlay_.show_notice(
                GetForegroundWindow(),
                settings_.streaming_mode_enabled()
                    ? L"Streaming mode on"
                    : L"Streaming mode off",
                2000);
        } else if (command >= kMenuProfileAuto && command <= kMenuProfileShell) {
            std::optional<AppProfile> profile;
            if (command != kMenuProfileAuto) {
                switch (command) {
                case kMenuProfileChat: profile = AppProfile::Chat; break;
                case kMenuProfileMail: profile = AppProfile::Mail; break;
                case kMenuProfileDocument: profile = AppProfile::Document; break;
                case kMenuProfileDeveloper: profile = AppProfile::DeveloperPrompt; break;
                case kMenuProfileCode: profile = AppProfile::CodeEditor; break;
                case kMenuProfileShell: profile = AppProfile::Shell; break;
                default: break;
                }
            }
            settings_.set_app_profile_override(tray_context_identity_, profile);
            const AppProfile effective = resolve_app_profile(tray_context_identity_, profile);
            overlay_.show_notice(
                GetForegroundWindow(),
                std::wstring(L"Application behavior · ") + app_profile_name(effective),
                2400);
        } else if (command == kMenuOpenModels) {
            std::filesystem::path folder = model_path_ ? model_path_->parent_path() : expected_model_path().parent_path();
            std::error_code error;
            std::filesystem::create_directories(folder, error);
            ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (command == kMenuStatus) {
            show_current_status();
        }
    }

    HINSTANCE instance_ = nullptr;
    HWND controller_ = nullptr;
    HWND target_window_ = nullptr;
    HWND target_control_ = nullptr;
    HHOOK hook_ = nullptr;
    HHOOK mouse_hook_ = nullptr;
    HICON icon_ = nullptr;
    HICON tray_icon_ = nullptr;
    NOTIFYICONDATAW tray_{};
    State state_ = State::Loading;
    std::optional<std::filesystem::path> model_path_;
    std::optional<std::filesystem::path> grammar_model_path_;
    AdvancedModelDownloader advanced_model_downloader_;
    AdvancedModelState advanced_model_state_ = AdvancedModelState::NotInstalled;
    std::string pending_verbatim_transcript_;
    std::string inserted_live_text_;
    std::optional<uint64_t> ignored_transcript_session_;
    uint64_t next_session_id_ = 0;
    uint64_t active_session_id_ = 0;
    AppIdentity active_app_identity_;
    AppIdentity tray_context_identity_;
    AppProfile active_app_profile_ = AppProfile::Unknown;
    OutputMode active_output_mode_ = OutputMode::Exact;
    SpeechLanguageMask active_speech_languages_ = kDefaultSpeechLanguages;
    size_t stream_audio_offset_ = 0;
    uint64_t latest_stream_revision_ = 0;
    bool streaming_dictation_ = false;
    bool incremental_recognition_ = false;
    bool stream_owns_target_ = false;
    bool advanced_model_fallback_notice_pending_ = false;
    AppSettings settings_;
    AudioCapture audio_;
    Overlay overlay_;
    SetupWindow setup_;
    std::unique_ptr<AdaptWorker> adapt_worker_;
    std::unique_ptr<RecognitionWorker> worker_;
    bool force_onboarding_ = false;
    bool background_ = false;
    bool preview_ui_ = false;
    bool preview_first_run_ = true;
    int preview_overlay_state_ = 0;
    int preview_page_ = 0;
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Gdiplus::GdiplusStartupInput gdiplus_input;
    ULONG_PTR gdiplus_token = 0;
    if (Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr) != Gdiplus::Ok) {
        return 1;
    }

    const std::vector<std::wstring> arguments = command_line_arguments();
    const auto has_argument = [&arguments](const wchar_t * value) {
        return std::find(arguments.begin() + std::min<size_t>(1, arguments.size()), arguments.end(), value) != arguments.end();
    };
    int preview_page = 0;
    if (has_argument(L"--preview-page-1")) preview_page = 1;
    if (has_argument(L"--preview-page-2")) preview_page = 2;
    if (has_argument(L"--preview-page-3")) preview_page = 3;
    const bool preview_settings = has_argument(L"--preview-settings");
    const bool preview_ui = has_argument(L"--preview-ui") || preview_settings || preview_page != 0;
    int preview_overlay_state = 0;
    if (has_argument(L"--preview-overlay") || has_argument(L"--preview-overlay-transcribing")) {
        preview_overlay_state = 1;
    } else if (has_argument(L"--preview-overlay-listening")) {
        preview_overlay_state = 2;
    } else if (has_argument(L"--preview-overlay-success")) {
        preview_overlay_state = 3;
    } else if (has_argument(L"--preview-overlay-error")) {
        preview_overlay_state = 4;
    } else if (has_argument(L"--preview-overlay-correcting")) {
        preview_overlay_state = 5;
    } else if (has_argument(L"--preview-overlay-streaming")) {
        preview_overlay_state = 6;
    } else if (has_argument(L"--preview-overlay-streaming-paused")) {
        preview_overlay_state = 7;
    } else if (has_argument(L"--preview-overlay-finalizing")) {
        preview_overlay_state = 8;
    } else if (has_argument(L"--preview-overlay-streaming-clean")) {
        preview_overlay_state = 9;
    } else if (has_argument(L"--preview-overlay-streaming-adapt")) {
        preview_overlay_state = 10;
    } else if (has_argument(L"--preview-overlay-streaming-shell")) {
        preview_overlay_state = 11;
    } else if (has_argument(L"--preview-overlay-fallback")) {
        preview_overlay_state = 12;
    } else if (has_argument(L"--preview-overlay-silence")) {
        preview_overlay_state = 13;
    } else if (has_argument(L"--preview-overlay-copied")) {
        preview_overlay_state = 14;
    }
    const bool preview = preview_ui || preview_overlay_state != 0;

    HANDLE legacy_singleton = nullptr;
    bool legacy_instance_exists = false;
    if (!preview) {
        legacy_singleton = CreateMutexW(nullptr, FALSE, L"Local\\VoiceKey.SingleInstance");
        if (legacy_singleton == nullptr) {
            Gdiplus::GdiplusShutdown(gdiplus_token);
            return 1;
        }
        legacy_instance_exists = GetLastError() == ERROR_ALREADY_EXISTS;
    }
    HANDLE singleton = CreateMutexW(
        nullptr, FALSE, preview ? L"Local\\SAID.PreviewInstance" : L"Local\\SAID.SingleInstance");
    if (singleton == nullptr) {
        if (legacy_singleton != nullptr) {
            CloseHandle(legacy_singleton);
        }
        Gdiplus::GdiplusShutdown(gdiplus_token);
        return 1;
    }
    if (legacy_instance_exists || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"SAID is already running in the system tray.", L"SAID", MB_OK | MB_ICONINFORMATION);
        CloseHandle(singleton);
        if (legacy_singleton != nullptr) {
            CloseHandle(legacy_singleton);
        }
        Gdiplus::GdiplusShutdown(gdiplus_token);
        return 0;
    }

    int result = 0;
    {
        SaidApp app(instance, has_argument(L"--onboarding"), has_argument(L"--background"),
                    preview_ui, !preview_settings, preview_overlay_state, preview_page);
        result = app.run();
    }
    CloseHandle(singleton);
    if (legacy_singleton != nullptr) {
        CloseHandle(legacy_singleton);
    }
    Gdiplus::GdiplusShutdown(gdiplus_token);
    return result;
}

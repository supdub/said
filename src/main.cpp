#include "audio_capture.h"
#include "app_settings.h"
#include "overlay.h"
#include "resource.h"
#include "setup_window.h"
#include "text_injector.h"
#include "transcriber.h"
#include "win_util.h"

#include <windows.h>
#include <propidl.h>
#include <gdiplus.h>
#include <shellapi.h>

#include <algorithm>
#include <condition_variable>
#include <filesystem>
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
constexpr UINT kTrayId = 1;
constexpr UINT kMenuStatus = 100;
constexpr UINT kMenuSetup = 101;
constexpr UINT kMenuOpenModels = 102;
constexpr UINT kMenuQuit = 103;

enum class WorkerMessageKind {
    Ready,
    Transcript,
    Error,
};

struct WorkerMessage {
    WorkerMessageKind kind;
    std::string text;
};

void post_worker_message(HWND window, WorkerMessageKind kind, std::string text = {}) {
    auto * message = new WorkerMessage{kind, std::move(text)};
    if (!PostMessageW(window, kMessageWorker, 0, reinterpret_cast<LPARAM>(message))) {
        delete message;
    }
}

class RecognitionWorker {
public:
    RecognitionWorker(HWND notify_window, std::filesystem::path model_path)
        : notify_window_(notify_window), model_path_(std::move(model_path)), thread_(&RecognitionWorker::run, this) {}

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

    void submit(std::vector<float> samples) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_audio_ = std::move(samples);
        }
        condition_.notify_one();
    }

private:
    void run() {
        try {
            Transcriber transcriber(model_path_);
            post_worker_message(notify_window_, WorkerMessageKind::Ready);

            while (true) {
                std::vector<float> audio;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait(lock, [this] { return stopping_ || pending_audio_.has_value(); });
                    if (stopping_) {
                        return;
                    }
                    audio = std::move(*pending_audio_);
                    pending_audio_.reset();
                }

                try {
                    post_worker_message(notify_window_, WorkerMessageKind::Transcript,
                                        transcriber.transcribe(audio));
                } catch (const std::exception & error) {
                    post_worker_message(notify_window_, WorkerMessageKind::Error, error.what());
                }
            }
        } catch (const std::exception & error) {
            post_worker_message(notify_window_, WorkerMessageKind::Error, error.what());
        }
    }

    HWND notify_window_ = nullptr;
    std::filesystem::path model_path_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<std::vector<float>> pending_audio_;
    bool stopping_ = false;
};

class SaidApp {
public:
    SaidApp(HINSTANCE instance, bool force_onboarding, bool background, bool preview_ui,
            int preview_overlay_state, int preview_page)
        : instance_(instance),
          force_onboarding_(force_onboarding),
          background_(background),
          preview_ui_(preview_ui),
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
        settings_.load();
        hotkey_vk_ = static_cast<DWORD>(settings_.shortcut());
        if (controller_ == nullptr || !overlay_.create(instance_) ||
            !setup_.create(instance_, controller_, icon_, &settings_)) {
            return 1;
        }

        if (preview_ui_ || preview_overlay_state_ != 0) {
            if (preview_ui_) {
                setup_.show(false, preview_page_);
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
        model_path_ = resolve_model_path(command_line_arguments());
        if (model_path_) {
            set_tray_tip(L"SAID — loading speech model");
            worker_ = std::make_unique<RecognitionWorker>(controller_, *model_path_);
        } else {
            set_state(State::ModelError);
            set_tray_tip(L"SAID — speech model missing");
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
        ModelError,
    };

    static inline HWND hook_target_ = nullptr;
    static inline bool hotkey_down_ = false;
    static inline DWORD hotkey_vk_ = VK_RMENU;

    static LRESULT CALLBACK keyboard_proc(int code, WPARAM wparam, LPARAM lparam) {
        if (code == HC_ACTION) {
            const auto * key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
            const bool right_alt = key->vkCode == VK_RMENU ||
                (key->vkCode == VK_MENU && (key->flags & LLKHF_EXTENDED) != 0);
            const bool matches = hotkey_vk_ == VK_RMENU ? right_alt : key->vkCode == hotkey_vk_;
            const bool injected = (key->flags & LLKHF_INJECTED) != 0;
            if (matches && !injected) {
                const bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
                const bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
                if (down && !hotkey_down_) {
                    hotkey_down_ = true;
                    if (hook_target_ != nullptr) {
                        PostMessageW(hook_target_, kMessageToggle, 0, 0);
                    }
                } else if (up) {
                    hotkey_down_ = false;
                }
                return 1;
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
            if (!setup_.handle_shortcut_pressed(settings_.shortcut())) {
                toggle_recording();
            }
            return 0;
        case kMessageShortcutChanged:
            hotkey_vk_ = static_cast<DWORD>(wparam);
            return 0;
        case kMessageWorker: {
            std::unique_ptr<WorkerMessage> worker_message(reinterpret_cast<WorkerMessage *>(lparam));
            handle_worker_message(*worker_message);
            return 0;
        }
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
            overlay_.show_transcribing(target_window_);
            break;
        case State::ModelError:
            overlay_.show_error(GetForegroundWindow(), L"Speech model not found — reinstall SAID or open the model folder", 4000);
            break;
        }
    }

    void start_recording() {
        target_window_ = GetForegroundWindow();
        std::string error;
        if (!audio_.start(error)) {
            overlay_.show_error(target_window_, utf8_to_wide(error));
            return;
        }
        set_state(State::Recording);
        set_tray_tip(std::wstring(L"SAID — listening · ") + shortcut_name(settings_.shortcut()) + L" to finish");
        overlay_.show_listening(target_window_, &audio_, shortcut_name(settings_.shortcut()));
    }

    void finish_recording() {
        const bool reached_limit = audio_.reached_limit();
        std::vector<float> samples = audio_.stop();
        set_state(State::Transcribing);
        set_tray_tip(L"SAID — transcribing");
        overlay_.show_transcribing(target_window_);
        if (reached_limit) {
            overlay_.show_error(target_window_, L"Ten-minute limit reached; transcribing captured audio", 2600);
        }
        worker_->submit(std::move(samples));
    }

    void handle_worker_message(const WorkerMessage & message) {
        if (message.kind == WorkerMessageKind::Ready) {
            set_state(State::Ready);
            set_tray_tip(std::wstring(L"SAID — ready · ") + shortcut_name(settings_.shortcut()) + L" to dictate");
            return;
        }

        if (message.kind == WorkerMessageKind::Error) {
            if (state_ == State::Loading) {
                set_state(State::ModelError);
            } else {
                set_state(State::Ready);
            }
            set_tray_tip(state_ == State::ModelError ? L"SAID — model error" : L"SAID — ready");
            overlay_.show_error(target_window_ != nullptr ? target_window_ : GetForegroundWindow(),
                                utf8_to_wide(message.text));
            return;
        }

        set_state(State::Ready);
        set_tray_tip(std::wstring(L"SAID — ready · ") + shortcut_name(settings_.shortcut()) + L" to dictate");
        if (message.text.empty()) {
            overlay_.show_notice(target_window_, L"No speech detected", 1600);
            return;
        }

        std::wstring error;
        const HWND foreground = GetForegroundWindow();
        if (foreground != target_window_) {
            if (copy_utf8_text(message.text, error)) {
                overlay_.show_notice(foreground, L"Transcript copied", 2400);
            } else {
                overlay_.show_error(foreground, error);
            }
            return;
        }

        if (inject_utf8_text(message.text, error)) {
            overlay_.show_notice(target_window_, L"Inserted", 850);
        } else {
            std::wstring clipboard_error;
            if (copy_utf8_text(message.text, clipboard_error)) {
                overlay_.show_error(target_window_, error + L" Transcript copied.", 3500);
            } else {
                overlay_.show_error(target_window_, error, 3500);
            }
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
        case State::ModelError: SetWindowTextW(controller_, L"SAID:model-error"); break;
        }
    }

    void show_tray_menu() {
        POINT cursor{};
        GetCursorPos(&cursor);
        HMENU menu = CreatePopupMenu();
        const std::wstring status = status_text();
        AppendMenuW(menu, MF_STRING | MF_GRAYED, kMenuStatus, status.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuSetup, L"Setup && settings…");
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
        case State::Ready: return std::wstring(L"Ready — ") + shortcut_name(settings_.shortcut()) + L" to dictate";
        case State::Recording: return std::wstring(L"Listening — ") + shortcut_name(settings_.shortcut()) + L" to finish";
        case State::Transcribing: return L"Transcribing…";
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
    HHOOK hook_ = nullptr;
    HICON icon_ = nullptr;
    HICON tray_icon_ = nullptr;
    NOTIFYICONDATAW tray_{};
    State state_ = State::Loading;
    std::optional<std::filesystem::path> model_path_;
    AppSettings settings_;
    AudioCapture audio_;
    Overlay overlay_;
    SetupWindow setup_;
    std::unique_ptr<RecognitionWorker> worker_;
    bool force_onboarding_ = false;
    bool background_ = false;
    bool preview_ui_ = false;
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
    const bool preview_ui = has_argument(L"--preview-ui") || preview_page != 0;
    int preview_overlay_state = 0;
    if (has_argument(L"--preview-overlay") || has_argument(L"--preview-overlay-transcribing")) {
        preview_overlay_state = 1;
    } else if (has_argument(L"--preview-overlay-listening")) {
        preview_overlay_state = 2;
    } else if (has_argument(L"--preview-overlay-success")) {
        preview_overlay_state = 3;
    } else if (has_argument(L"--preview-overlay-error")) {
        preview_overlay_state = 4;
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
                    preview_ui, preview_overlay_state, preview_page);
        result = app.run();
    }
    CloseHandle(singleton);
    if (legacy_singleton != nullptr) {
        CloseHandle(legacy_singleton);
    }
    Gdiplus::GdiplusShutdown(gdiplus_token);
    return result;
}

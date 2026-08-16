#include "text_injector.h"
#include "win_util.h"

#include <windows.h>

#include <chrono>
#include <atomic>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace {
constexpr wchar_t kProbeClass[] = L"SAIDTextInjectorProbe";

void pump_messages_for(std::chrono::milliseconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    MSG message{};
    while (std::chrono::steady_clock::now() < deadline) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

bool expect_edit_text(HWND edit, const wchar_t * expected, const char * stage) {
    wchar_t actual[256]{};
    GetWindowTextW(edit, actual, static_cast<int>(std::size(actual)));
    const std::wstring actual_text(actual);
    const std::wstring expected_text(expected);
    if (actual_text == expected_text) {
        return true;
    }
    std::cerr << stage << " text mismatch: expected " << expected_text.size()
              << " UTF-16 units, received " << actual_text.size()
              << "; actual UTF-8: " << wide_to_utf8(actual_text) << "\n";
    return false;
}

bool focus_edit_end(HWND window, HWND edit) {
    SetForegroundWindow(window);
    SetFocus(edit);
    const LRESULT length = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL,
                 static_cast<WPARAM>(length), static_cast<LPARAM>(length));
    pump_messages_for(std::chrono::milliseconds(50));
    return GetForegroundWindow() == window && GetFocus() == edit;
}

bool run_while_pumping(const std::function<bool()> & operation) {
    std::atomic<bool> complete{false};
    bool result = false;
    std::thread worker([&] {
        result = operation();
        complete.store(true, std::memory_order_release);
    });
    while (!complete.load(std::memory_order_acquire)) {
        pump_messages_for(std::chrono::milliseconds(10));
    }
    worker.join();
    pump_messages_for(std::chrono::milliseconds(100));
    return result;
}

class ForegroundLock {
public:
    ForegroundLock() : locked_(LockSetForegroundWindow(LSFW_LOCK) != FALSE) {}
    ~ForegroundLock() {
        if (locked_) LockSetForegroundWindow(LSFW_UNLOCK);
    }
    bool locked() const { return locked_; }

private:
    bool locked_ = false;
};
}

int main() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.lpszClassName = kProbeClass;
    if (!RegisterClassExW(&window_class)) {
        std::cerr << "could not register probe window\n";
        return 1;
    }

    HWND window = CreateWindowExW(WS_EX_TOPMOST, kProbeClass, L"SAID insertion probe",
                                  WS_OVERLAPPEDWINDOW, 100, 100, 520, 120,
                                  nullptr, nullptr, instance, nullptr);
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                12, 16, 480, 32, window, nullptr, instance, nullptr);
    if (window == nullptr || edit == nullptr) {
        std::cerr << "could not create probe edit control\n";
        return 2;
    }

    ShowWindow(window, SW_SHOW);
    SetWindowTextW(edit, L"User draft: ");
    if (!focus_edit_end(window, edit)) {
        std::cerr << "could not focus the probe edit control\n";
        DestroyWindow(window);
        return 3;
    }
    ForegroundLock foreground_lock;
    if (!foreground_lock.locked()) {
        std::cerr << "could not lock the probe foreground window\n";
        DestroyWindow(window);
        return 4;
    }

    constexpr char kRecognizedUtf8[] =
        "SAID uh English \xF0\x9F\x8E\x99 \xE4\xB8\xAD\xE6\x96\x87";
    std::wstring error;
    if (!run_while_pumping([&] { return inject_utf8_text(kRecognizedUtf8, error); })) {
        std::wcerr << error << L"\n";
        DestroyWindow(window);
        return 4;
    }
    if (!expect_edit_text(
            edit,
            L"User draft: SAID uh English \xD83C\xDF99 \x4E2D\x6587",
            "initial insertion")) {
        DestroyWindow(window);
        return 5;
    }

    constexpr char kUntrackedUtf8[] =
        "Different English \xF0\x9F\x8E\x99 \xE4\xB8\xAD\xE6\x96\x87";
    if (run_while_pumping([&] {
            return replace_recent_utf8_text(kUntrackedUtf8, "unsafe replacement", error);
        })) {
        std::cerr << "replacement accepted an untracked suffix\n";
        DestroyWindow(window);
        return 6;
    }
    error.clear();
    if (!expect_edit_text(
            edit,
            L"User draft: SAID uh English \xD83C\xDF99 \x4E2D\x6587",
            "rejected untracked replacement")) {
        DestroyWindow(window);
        return 7;
    }

    constexpr char kCleanUtf8[] =
        "SAID English \xF0\x9F\x8E\x99 \xE4\xB8\xAD\xE6\x96\x87";
    if (!focus_edit_end(window, edit)) {
        std::cerr << "lost focus before Clean replacement\n";
        DestroyWindow(window);
        return 6;
    }
    if (!run_while_pumping([&] {
            return replace_recent_utf8_text(kRecognizedUtf8, kCleanUtf8, error);
        })) {
        std::wcerr << error << L"\n";
        DestroyWindow(window);
        return 5;
    }
    if (!expect_edit_text(
            edit,
            L"User draft: SAID English \xD83C\xDF99 \x4E2D\x6587",
            "Clean replacement")) {
        DestroyWindow(window);
        return 6;
    }

    constexpr char kLaterRevisionUtf8[] =
        "SAID polished \xF0\x9F\x8E\x99 \xE4\xB8\xAD\xE6\x96\x87. Next phrase.";
    if (!focus_edit_end(window, edit)) {
        std::cerr << "lost focus before later replacement\n";
        DestroyWindow(window);
        return 7;
    }
    if (!run_while_pumping([&] {
            return replace_recent_utf8_text(kCleanUtf8, kLaterRevisionUtf8, error);
        })) {
        std::wcerr << error << L"\n";
        DestroyWindow(window);
        return 7;
    }
    if (!expect_edit_text(
            edit,
            L"User draft: SAID polished \xD83C\xDF99 \x4E2D\x6587. Next phrase.",
            "later replacement")) {
        DestroyWindow(window);
        return 8;
    }

    constexpr char kFinalUtf8[] =
        "SAID polished \xF0\x9F\x8E\x99 \xE4\xB8\xAD\xE6\x96\x87\xE3\x80\x82Next phrase.";
    if (!focus_edit_end(window, edit)) {
        std::cerr << "lost focus before final replacement\n";
        DestroyWindow(window);
        return 9;
    }
    if (!run_while_pumping([&] {
            return replace_recent_utf8_text(kLaterRevisionUtf8, kFinalUtf8, error);
        })) {
        std::wcerr << error << L"\n";
        DestroyWindow(window);
        return 9;
    }
    if (!expect_edit_text(
            edit,
            L"User draft: SAID polished \xD83C\xDF99 \x4E2D\x6587\x3002Next phrase.",
            "final replacement")) {
        DestroyWindow(window);
        return 10;
    }
    DestroyWindow(window);

    std::cout << "preserved pre-existing text while replacing three tracked live revisions without duplication\n";
    return 0;
}

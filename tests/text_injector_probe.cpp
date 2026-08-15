#include "text_injector.h"

#include <windows.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {
constexpr wchar_t kProbeClass[] = L"VoiceKeyTextInjectorProbe";

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

    HWND window = CreateWindowExW(WS_EX_TOPMOST, kProbeClass, L"VoiceKey insertion probe",
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
    SetForegroundWindow(window);
    SetFocus(edit);
    pump_messages_for(std::chrono::milliseconds(100));

    constexpr char kExpectedUtf8[] = "VoiceKey English \xE4\xB8\xAD\xE6\x96\x87";
    std::wstring error;
    if (!inject_utf8_text(kExpectedUtf8, error)) {
        std::wcerr << error << L"\n";
        DestroyWindow(window);
        return 3;
    }
    pump_messages_for(std::chrono::milliseconds(250));

    wchar_t actual[128]{};
    GetWindowTextW(edit, actual, static_cast<int>(std::size(actual)));
    DestroyWindow(window);
    if (std::wstring(actual) != L"VoiceKey English \x4E2D\x6587") {
        std::wcerr << L"unexpected injected text: " << actual << L"\n";
        return 4;
    }

    std::cout << "inserted English and Chinese Unicode text through SendInput\n";
    return 0;
}

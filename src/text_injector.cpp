#include "text_injector.h"

#include "win_util.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <vector>

bool inject_utf8_text(const std::string & text, std::wstring & error) {
    const std::wstring wide = utf8_to_wide(text);
    if (wide.empty() && !text.empty()) {
        error = L"The recognized text was not valid UTF-8.";
        return false;
    }

    constexpr size_t kCodeUnitsPerBatch = 64;
    for (size_t offset = 0; offset < wide.size(); offset += kCodeUnitsPerBatch) {
        const size_t count = std::min(kCodeUnitsPerBatch, wide.size() - offset);
        std::vector<INPUT> inputs(count * 2U);
        for (size_t index = 0; index < count; ++index) {
            INPUT down{};
            down.type = INPUT_KEYBOARD;
            down.ki.wScan = wide[offset + index];
            down.ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[index * 2U] = down;

            INPUT up = down;
            up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs[index * 2U + 1U] = up;
        }

        const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        if (sent != inputs.size()) {
            error = L"Windows blocked text insertion. The target may be running as administrator.";
            return false;
        }
    }
    return true;
}

bool copy_utf8_text(const std::string & text, std::wstring & error) {
    const std::wstring wide = utf8_to_wide(text);
    if (wide.empty() && !text.empty()) {
        error = L"The recognized text was not valid UTF-8.";
        return false;
    }
    if (!OpenClipboard(nullptr)) {
        error = L"Could not open the clipboard.";
        return false;
    }

    const size_t bytes = (wide.size() + 1U) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        error = L"Could not allocate clipboard memory.";
        return false;
    }

    void * destination = GlobalLock(memory);
    if (destination == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        error = L"Could not lock clipboard memory.";
        return false;
    }
    std::memcpy(destination, wide.c_str(), bytes);
    GlobalUnlock(memory);

    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        error = L"Could not put the transcript on the clipboard.";
        return false;
    }
    CloseClipboard();
    return true;
}

#include "text_injector.h"

#include "win_util.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <cstring>
#include <vector>

namespace {
constexpr size_t kMaximumReplaceCodePoints = 16384;

INPUT virtual_key_input(WORD key, DWORD flags) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = flags;
    return input;
}

size_t unicode_code_point_count(const std::wstring & text) {
    size_t count = 0;
    for (size_t index = 0; index < text.size(); ++index) {
        const wchar_t value = text[index];
        if (value >= 0xD800 && value <= 0xDBFF && index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            ++index;
        }
        ++count;
    }
    return count;
}

bool erase_recent_code_points(size_t code_points, std::wstring & error) {
    constexpr size_t kKeypressesPerBatch = 64;
    while (code_points > 0) {
        const size_t count = std::min(code_points, kKeypressesPerBatch);
        std::vector<INPUT> erase(count * 2U);
        for (size_t index = 0; index < count; ++index) {
            erase[index * 2U] = virtual_key_input(VK_BACK, 0);
            erase[index * 2U + 1U] = virtual_key_input(VK_BACK, KEYEVENTF_KEYUP);
        }
        if (SendInput(static_cast<UINT>(erase.size()), erase.data(), sizeof(INPUT)) !=
            erase.size()) {
            error = L"Windows blocked in-place refinement.";
            return false;
        }
        code_points -= count;
    }
    return true;
}

enum class EditReplacementResult {
    NotApplicable,
    Replaced,
    Failed,
};

bool is_native_edit_control(HWND control) {
    std::array<wchar_t, 64> class_name{};
    if (GetClassNameW(control, class_name.data(), static_cast<int>(class_name.size())) <= 0) {
        return false;
    }
    std::wstring lowered(class_name.data());
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    });
    return lowered == L"edit" || lowered.find(L"richedit") != std::wstring::npos;
}

EditReplacementResult replace_in_focused_edit(
    const std::wstring & existing,
    const std::wstring & replacement,
    std::wstring & error) {
    HWND foreground = GetForegroundWindow();
    HWND control = focused_control(foreground);
    if (control == nullptr || !is_native_edit_control(control) ||
        (GetWindowLongPtrW(control, GWL_STYLE) & ES_PASSWORD) != 0) {
        return EditReplacementResult::NotApplicable;
    }

    DWORD selection_start = 0;
    DWORD selection_end = 0;
    SendMessageW(control, EM_GETSEL,
                 reinterpret_cast<WPARAM>(&selection_start),
                 reinterpret_cast<LPARAM>(&selection_end));
    if (selection_start != selection_end || selection_end < existing.size()) {
        error = L"The caret moved before SAID could refine the live text.";
        return EditReplacementResult::Failed;
    }

    constexpr LRESULT kMaximumControlCodeUnits = 1024 * 1024;
    const LRESULT length = SendMessageW(control, WM_GETTEXTLENGTH, 0, 0);
    if (length < 0 || length > kMaximumControlCodeUnits ||
        selection_end > static_cast<DWORD>(length)) {
        error = L"The target text could not be verified before refinement.";
        return EditReplacementResult::Failed;
    }
    std::vector<wchar_t> text(static_cast<size_t>(length) + 1U, L'\0');
    const LRESULT copied = SendMessageW(
        control, WM_GETTEXT, static_cast<WPARAM>(text.size()),
        reinterpret_cast<LPARAM>(text.data()));
    if (copied < 0 || selection_end > static_cast<DWORD>(copied)) {
        error = L"The target text could not be verified before refinement.";
        return EditReplacementResult::Failed;
    }
    const size_t suffix_start = static_cast<size_t>(selection_end) - existing.size();
    if (!std::equal(existing.begin(), existing.end(), text.begin() + suffix_start)) {
        error = L"The target text changed before SAID could refine it.";
        return EditReplacementResult::Failed;
    }

    SendMessageW(control, EM_SETSEL,
                 static_cast<WPARAM>(suffix_start),
                 static_cast<LPARAM>(selection_end));
    SendMessageW(control, EM_REPLACESEL, TRUE,
                 reinterpret_cast<LPARAM>(replacement.c_str()));
    return EditReplacementResult::Replaced;
}
}

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

bool replace_recent_utf8_text(
    const std::string & existing_text,
    const std::string & replacement_text,
    std::wstring & error) {
    if (existing_text.empty()) {
        return inject_utf8_text(replacement_text, error);
    }

    const std::wstring existing = utf8_to_wide(existing_text);
    const std::wstring replacement = utf8_to_wide(replacement_text);
    if ((existing.empty() && !existing_text.empty()) ||
        (replacement.empty() && !replacement_text.empty())) {
        error = L"The transcript was not valid UTF-8.";
        return false;
    }

    const size_t code_points = unicode_code_point_count(existing);
    if (code_points > kMaximumReplaceCodePoints) {
        error = L"The live transcript is too long to replace safely.";
        return false;
    }

    const EditReplacementResult edit_result =
        replace_in_focused_edit(existing, replacement, error);
    if (edit_result == EditReplacementResult::Replaced) {
        return true;
    }
    if (edit_result == EditReplacementResult::Failed) {
        return false;
    }

    // Terminal line editors (including Codex under tmux) do not expose a
    // Windows text selection. Shift+Left becomes an escape sequence or moves
    // the terminal cursor, so inserting the polished text would append a
    // duplicate. Backspace is understood by both terminals and GUI editors:
    // erase only the exact live suffix SAID tracked, then insert its final form.
    if (!erase_recent_code_points(code_points, error)) {
        return false;
    }
    if (replacement_text.empty()) {
        return true;
    }
    return inject_utf8_text(replacement_text, error);
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

#include "grammar_wait_ui.h"

GrammarWaitCopy grammar_wait_copy(
    uint64_t elapsed_milliseconds,
    const std::wstring & shortcut) {
    if (elapsed_milliseconds < 3000U) {
        return {
            L"Finishing structure",
            L"Local only · " + shortcut + L" keeps the clean draft",
        };
    }
    if (elapsed_milliseconds < 8000U) {
        return {
            L"Still adapting",
            L"Clean draft is safe · " + shortcut + L" keeps it",
        };
    }
    return {
        L"Taking longer than usual",
        shortcut + L" keeps the clean draft now",
    };
}

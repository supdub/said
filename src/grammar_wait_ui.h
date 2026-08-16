#pragma once

#include <cstdint>
#include <string>

struct GrammarWaitCopy {
    std::wstring title;
    std::wstring subtitle;
};

GrammarWaitCopy grammar_wait_copy(
    uint64_t elapsed_milliseconds,
    const std::wstring & shortcut);

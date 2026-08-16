#pragma once

#include <cstdint>
#include <optional>

enum class OutputMode : uint32_t {
    Exact = 0,
    Clean = 1,
    Adapt = 2,
};

constexpr OutputMode default_output_mode() {
    return OutputMode::Clean;
}

OutputMode sanitize_output_mode(uint32_t value);

// Legacy grammar values were Off=0, Standard=1, and Advanced=2. Advanced was
// a conservative copyedit mode, so upgrades must not silently opt in to the
// broader Adapt behavior.
OutputMode migrate_output_mode(
    std::optional<uint32_t> stored_output_mode,
    std::optional<uint32_t> legacy_grammar_mode,
    std::optional<uint32_t> legacy_grammar_enabled,
    bool onboarding_complete);

const wchar_t * output_mode_name(OutputMode mode);

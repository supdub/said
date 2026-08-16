#include "output_mode.h"

OutputMode sanitize_output_mode(uint32_t value) {
    switch (static_cast<OutputMode>(value)) {
    case OutputMode::Exact:
        return OutputMode::Exact;
    case OutputMode::Clean:
        return OutputMode::Clean;
    case OutputMode::Adapt:
        return OutputMode::Adapt;
    }
    return OutputMode::Clean;
}

OutputMode migrate_output_mode(
    std::optional<uint32_t> stored_output_mode,
    std::optional<uint32_t> legacy_grammar_mode,
    std::optional<uint32_t> legacy_grammar_enabled,
    bool onboarding_complete) {
    if (stored_output_mode) {
        return sanitize_output_mode(*stored_output_mode);
    }
    if (legacy_grammar_mode) {
        return *legacy_grammar_mode == 0U ? OutputMode::Exact : OutputMode::Clean;
    }
    if (legacy_grammar_enabled) {
        return *legacy_grammar_enabled == 0U ? OutputMode::Exact : OutputMode::Clean;
    }
    return onboarding_complete ? OutputMode::Exact : default_output_mode();
}

const wchar_t * output_mode_name(OutputMode mode) {
    switch (mode) {
    case OutputMode::Exact:
        return L"Exact";
    case OutputMode::Clean:
        return L"Clean";
    case OutputMode::Adapt:
        return L"Adapt";
    }
    return L"Clean";
}

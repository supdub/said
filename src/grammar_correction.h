#pragma once

#include <stdexcept>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class GrammarCorrectionMode : uint32_t {
    Off = 0,
    Standard = 1,
    Advanced = 2,
};

enum class GrammarCorrectionStatus {
    Disabled,
    StandardApplied,
    AdvancedApplied,
    AdvancedFallback,
    Skipped,
};

class GrammarCorrectionCancelled final : public std::runtime_error {
public:
    GrammarCorrectionCancelled() : std::runtime_error("Adapt skipped.") {}
};

struct GrammarCorrectionResult {
    std::string text;
    GrammarCorrectionStatus status = GrammarCorrectionStatus::Disabled;
};

class GrammarCorrectionBackend {
public:
    virtual ~GrammarCorrectionBackend() = default;
    virtual std::string correct(const std::string & text) = 0;
};

GrammarCorrectionMode default_grammar_correction_mode(bool onboarding_complete);

GrammarCorrectionMode migrate_grammar_correction_mode(
    std::optional<uint32_t> stored_mode,
    std::optional<uint32_t> legacy_enabled,
    bool onboarding_complete);

const wchar_t * grammar_correction_mode_name(GrammarCorrectionMode mode);

std::string standard_grammar_correction(const std::string & transcript);

GrammarCorrectionResult process_grammar_correction(
    const std::string & transcript,
    GrammarCorrectionMode mode,
    GrammarCorrectionBackend * advanced_backend);

bool grammar_correction_output_is_safe(
    const std::string & original,
    const std::string & corrected);

std::vector<std::string> split_for_grammar_correction(
    const std::string & text,
    size_t target_bytes = 1200,
    size_t maximum_bytes = 1800);

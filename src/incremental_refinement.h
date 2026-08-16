#pragma once

#include "app_profile.h"
#include "output_mode.h"

#include <cstdint>
#include <string>
#include <vector>

enum class RefinementStage {
    RecognitionRepair,
    SpokenCleanup,
    ApplicationAdapt,
};

enum class SpeechUnitPhase {
    Recognized,
    Repaired,
    Cleaned,
    Adapted,
};

struct SpeechUnit {
    uint64_t id = 0;
    std::string exact;
    std::string repaired;
    std::string cleaned;
    std::string adapted;
    SpeechUnitPhase phase = SpeechUnitPhase::Recognized;
    bool committed = false;
};

struct RefinementUnitView {
    uint64_t id = 0;
    std::string text;
};

struct RefinementRequest {
    uint64_t session_id = 0;
    uint64_t base_revision = 0;
    RefinementStage stage = RefinementStage::RecognitionRepair;
    OutputMode mode = OutputMode::Exact;
    AppProfile profile = AppProfile::Unknown;
    std::string read_only_context;
    std::vector<RefinementUnitView> units;
};

struct RefinementResult {
    uint64_t session_id = 0;
    uint64_t base_revision = 0;
    RefinementStage stage = RefinementStage::RecognitionRepair;
    std::vector<RefinementUnitView> units;
    // Only in-process rule/dictionary transforms may set this. Model-backed
    // results must always pass the full rewrite safety gate.
    bool trusted_deterministic = false;
};

enum class RefinementApplyDisposition {
    Applied,
    Stale,
    Invalid,
    Unsafe,
};

class IncrementalRefinementSession {
public:
    IncrementalRefinementSession(
        uint64_t session_id,
        OutputMode mode,
        AppProfile profile,
        size_t revisable_phrases = 2U,
        size_t revisable_code_points = 320U);

    uint64_t session_id() const;
    uint64_t revision() const;
    OutputMode mode() const;
    AppProfile profile() const;

    uint64_t append_recognized(std::string phrase);
    RefinementRequest make_request(RefinementStage stage) const;
    RefinementApplyDisposition apply(const RefinementResult & result);
    void settle();

    std::string exact_text() const;
    std::string clean_text() const;
    std::string live_text() const;
    std::string committed_clean_text() const;
    std::string revisable_clean_text() const;
    const std::vector<SpeechUnit> & units() const;

private:
    std::string render(RefinementStage stage) const;
    void commit_eligible_units();

    uint64_t session_id_ = 0;
    uint64_t revision_ = 0;
    uint64_t next_unit_id_ = 1;
    OutputMode mode_ = OutputMode::Exact;
    AppProfile profile_ = AppProfile::Unknown;
    size_t revisable_phrases_ = 2U;
    size_t revisable_code_points_ = 320U;
    std::vector<SpeechUnit> units_;
};

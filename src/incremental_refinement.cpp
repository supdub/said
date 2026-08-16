#include "incremental_refinement.h"

#include "rewrite_safety.h"
#include "transcript.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace {
size_t utf8_code_points(const std::string & text) {
    return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](unsigned char byte) {
        return (byte & 0xC0U) != 0x80U;
    }));
}

bool phase_at_least(SpeechUnitPhase phase, SpeechUnitPhase expected) {
    return static_cast<int>(phase) >= static_cast<int>(expected);
}

const std::string & text_for_stage(const SpeechUnit & unit, RefinementStage stage) {
    switch (stage) {
    case RefinementStage::RecognitionRepair:
        return unit.exact;
    case RefinementStage::SpokenCleanup:
        return phase_at_least(unit.phase, SpeechUnitPhase::Repaired)
            ? unit.repaired : unit.exact;
    case RefinementStage::ApplicationAdapt:
        return phase_at_least(unit.phase, SpeechUnitPhase::Cleaned)
            ? unit.cleaned
            : (phase_at_least(unit.phase, SpeechUnitPhase::Repaired) ? unit.repaired : unit.exact);
    }
    return unit.exact;
}

std::string visible_unit_text(const SpeechUnit & unit, OutputMode mode, AppProfile profile) {
    if (mode == OutputMode::Exact) {
        return unit.exact;
    }
    if (mode == OutputMode::Adapt && app_profile_allows_adapt(profile) &&
        phase_at_least(unit.phase, SpeechUnitPhase::Adapted)) {
        return unit.adapted;
    }
    if (phase_at_least(unit.phase, SpeechUnitPhase::Cleaned)) {
        return unit.cleaned;
    }
    if (phase_at_least(unit.phase, SpeechUnitPhase::Repaired)) {
        return unit.repaired;
    }
    return unit.exact;
}
}

IncrementalRefinementSession::IncrementalRefinementSession(
    uint64_t session_id,
    OutputMode mode,
    AppProfile profile,
    size_t revisable_phrases,
    size_t revisable_code_points)
    : session_id_(session_id),
      mode_(mode),
      profile_(profile),
      revisable_phrases_(std::max<size_t>(1U, revisable_phrases)),
      revisable_code_points_(std::max<size_t>(32U, revisable_code_points)) {}

uint64_t IncrementalRefinementSession::session_id() const { return session_id_; }
uint64_t IncrementalRefinementSession::revision() const { return revision_; }
OutputMode IncrementalRefinementSession::mode() const { return mode_; }
AppProfile IncrementalRefinementSession::profile() const { return profile_; }

uint64_t IncrementalRefinementSession::append_recognized(std::string phrase) {
    if (phrase.empty()) {
        return 0;
    }
    SpeechUnit unit;
    unit.id = next_unit_id_++;
    unit.exact = std::move(phrase);
    unit.repaired = unit.exact;
    unit.cleaned = unit.exact;
    unit.adapted = unit.exact;
    units_.push_back(std::move(unit));
    ++revision_;
    if (mode_ == OutputMode::Exact) {
        commit_eligible_units();
    }
    return units_.back().id;
}

RefinementRequest IncrementalRefinementSession::make_request(RefinementStage stage) const {
    RefinementRequest request;
    request.session_id = session_id_;
    request.base_revision = revision_;
    request.stage = stage;
    request.mode = mode_;
    request.profile = profile_;

    auto first_revisable = std::find_if(units_.begin(), units_.end(), [](const SpeechUnit & unit) {
        return !unit.committed;
    });
    if (first_revisable != units_.begin() && first_revisable != units_.end()) {
        const SpeechUnit & context = *std::prev(first_revisable);
        request.read_only_context = visible_unit_text(context, mode_, profile_);
    }
    for (auto it = first_revisable; it != units_.end(); ++it) {
        request.units.push_back({it->id, text_for_stage(*it, stage)});
    }
    return request;
}

RefinementApplyDisposition IncrementalRefinementSession::apply(const RefinementResult & result) {
    if (result.session_id != session_id_ || result.base_revision != revision_) {
        return RefinementApplyDisposition::Stale;
    }
    if (result.stage == RefinementStage::ApplicationAdapt &&
        (mode_ != OutputMode::Adapt || !app_profile_allows_adapt(profile_))) {
        return RefinementApplyDisposition::Invalid;
    }
    const RefinementRequest expected = make_request(result.stage);
    if (result.units.size() != expected.units.size() || result.units.empty()) {
        return RefinementApplyDisposition::Invalid;
    }
    for (size_t index = 0; index < result.units.size(); ++index) {
        if (result.units[index].id != expected.units[index].id ||
            (result.units[index].text.empty() && !result.trusted_deterministic)) {
            return RefinementApplyDisposition::Invalid;
        }
        if (result.trusted_deterministic &&
            result.stage != RefinementStage::ApplicationAdapt) {
            continue;
        }
        OutputMode safety_mode = result.stage == RefinementStage::ApplicationAdapt
            ? OutputMode::Adapt : OutputMode::Clean;
        const RewriteSafetyResult safety = validate_rewrite(
            expected.units[index].text, result.units[index].text,
            safety_mode, profile_);
        if (!safety.safe) {
            return RefinementApplyDisposition::Unsafe;
        }
    }

    for (const auto & revision : result.units) {
        auto unit = std::find_if(units_.begin(), units_.end(), [&](const SpeechUnit & candidate) {
            return candidate.id == revision.id && !candidate.committed;
        });
        if (unit == units_.end()) {
            return RefinementApplyDisposition::Stale;
        }
        switch (result.stage) {
        case RefinementStage::RecognitionRepair:
            unit->repaired = revision.text;
            unit->cleaned = revision.text;
            unit->adapted = revision.text;
            unit->phase = SpeechUnitPhase::Repaired;
            break;
        case RefinementStage::SpokenCleanup:
            unit->cleaned = revision.text;
            unit->adapted = revision.text;
            unit->phase = SpeechUnitPhase::Cleaned;
            break;
        case RefinementStage::ApplicationAdapt:
            unit->adapted = revision.text;
            unit->phase = SpeechUnitPhase::Adapted;
            break;
        }
    }
    ++revision_;
    if (result.stage == RefinementStage::SpokenCleanup ||
        result.stage == RefinementStage::ApplicationAdapt) {
        commit_eligible_units();
    }
    return RefinementApplyDisposition::Applied;
}

void IncrementalRefinementSession::settle() {
    for (auto & unit : units_) {
        unit.committed = true;
    }
    ++revision_;
}

std::string IncrementalRefinementSession::render(RefinementStage stage) const {
    std::string result;
    for (const SpeechUnit & unit : units_) {
        const std::string * text = &unit.exact;
        if (stage == RefinementStage::RecognitionRepair) {
            text = &unit.repaired;
        } else if (stage == RefinementStage::SpokenCleanup) {
            text = &unit.cleaned;
        } else {
            text = &unit.adapted;
        }
        append_recognizer_segment(result, *text);
    }
    return result;
}

std::string IncrementalRefinementSession::exact_text() const {
    std::string result;
    for (const SpeechUnit & unit : units_) {
        append_recognizer_segment(result, unit.exact);
    }
    return result;
}

std::string IncrementalRefinementSession::clean_text() const {
    std::string result;
    for (const SpeechUnit & unit : units_) {
        append_recognizer_segment(result,
            phase_at_least(unit.phase, SpeechUnitPhase::Cleaned)
                ? unit.cleaned
                : (phase_at_least(unit.phase, SpeechUnitPhase::Repaired)
                    ? unit.repaired : unit.exact));
    }
    return result;
}

std::string IncrementalRefinementSession::live_text() const {
    std::string result;
    for (const SpeechUnit & unit : units_) {
        append_recognizer_segment(result, visible_unit_text(unit, mode_, profile_));
    }
    return result;
}

std::string IncrementalRefinementSession::committed_clean_text() const {
    std::string result;
    for (const SpeechUnit & unit : units_) {
        if (!unit.committed) {
            break;
        }
        append_recognizer_segment(result,
            phase_at_least(unit.phase, SpeechUnitPhase::Cleaned)
                ? unit.cleaned
                : (phase_at_least(unit.phase, SpeechUnitPhase::Repaired)
                    ? unit.repaired : unit.exact));
    }
    return result;
}

std::string IncrementalRefinementSession::revisable_clean_text() const {
    std::string result;
    for (const SpeechUnit & unit : units_) {
        if (unit.committed) {
            continue;
        }
        append_recognizer_segment(result,
            phase_at_least(unit.phase, SpeechUnitPhase::Cleaned)
                ? unit.cleaned
                : (phase_at_least(unit.phase, SpeechUnitPhase::Repaired)
                    ? unit.repaired : unit.exact));
    }
    return result;
}

const std::vector<SpeechUnit> & IncrementalRefinementSession::units() const {
    return units_;
}

void IncrementalRefinementSession::commit_eligible_units() {
    size_t revisable_count = 0;
    size_t revisable_points = 0;
    for (auto it = units_.rbegin(); it != units_.rend(); ++it) {
        if (it->committed) {
            break;
        }
        ++revisable_count;
        revisable_points += utf8_code_points(visible_unit_text(*it, mode_, profile_));
    }

    for (SpeechUnit & unit : units_) {
        if (unit.committed) {
            continue;
        }
        const bool too_many = revisable_count > revisable_phrases_;
        const bool too_long = revisable_points > revisable_code_points_ && revisable_count > 1U;
        if (!too_many && !too_long) {
            break;
        }
        const bool ready = mode_ == OutputMode::Exact ||
            phase_at_least(unit.phase, SpeechUnitPhase::Cleaned);
        if (!ready) {
            break;
        }
        unit.committed = true;
        --revisable_count;
        revisable_points -= std::min(
            revisable_points,
            utf8_code_points(visible_unit_text(unit, mode_, profile_)));
    }
}

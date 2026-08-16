#include "local_refinement.h"

#include "grammar_correction.h"
#include "speech_language.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace {
bool ascii_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), ascii_space);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(), ascii_space).base();
    return std::string(first, last);
}

bool starts_with_ascii_case_insensitive(const std::string & value, const std::string & prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (size_t index = 0; index < prefix.size(); ++index) {
        const unsigned char left = static_cast<unsigned char>(value[index]);
        const unsigned char right = static_cast<unsigned char>(prefix[index]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }
    return true;
}

void remove_prefix_punctuation(std::string & value) {
    value = trim(std::move(value));
    bool changed = true;
    while (changed && !value.empty()) {
        changed = false;
        for (const char * punctuation : {",", ".", ":", ";", "，", "。", "：", "；"}) {
            const std::string token(punctuation);
            if (value.rfind(token, 0) == 0) {
                value.erase(0, token.size());
                value = trim(std::move(value));
                changed = true;
                break;
            }
        }
    }
}

std::string remove_high_confidence_fillers(std::string value) {
    static const std::regex kEnglishStart(
        R"(^\s*(?:(?:uh+|um+|erm+)[,.:;]?\s*)+)",
        std::regex::ECMAScript | std::regex::icase);
    value = std::regex_replace(value, kEnglishStart, "");

    for (const char * filler : {"呃", "嗯"}) {
        const std::string token(filler);
        while (value.rfind(token, 0) == 0) {
            value.erase(0, token.size());
            remove_prefix_punctuation(value);
        }
    }
    return trim(std::move(value));
}

void capitalize_initial_ascii(std::string & value) {
    if (!value.empty()) {
        const unsigned char first = static_cast<unsigned char>(value.front());
        if (first >= 'a' && first <= 'z') {
            value.front() = static_cast<char>(std::toupper(first));
        }
    }
}

bool replace_last_match(
    std::string & value,
    const std::regex & pattern,
    const std::string & replacement) {
    std::sregex_iterator begin(value.begin(), value.end(), pattern);
    const std::sregex_iterator end;
    if (begin == end) {
        return false;
    }
    std::smatch last;
    for (auto it = begin; it != end; ++it) {
        last = *it;
    }
    value.replace(
        static_cast<size_t>(last.position()),
        static_cast<size_t>(last.length()),
        replacement);
    return true;
}

bool apply_chinese_cross_phrase_correction(std::string & previous, std::string & current) {
    std::string remainder = trim(current);
    const std::string marker = "不对";
    if (remainder.rfind(marker, 0) != 0) {
        return false;
    }
    remainder.erase(0, marker.size());
    remove_prefix_punctuation(remainder);

    static const std::regex kChineseTime(
        "((零|〇|一|二|两|三|四|五|六|七|八|九|十|百|[0-9]){1,4}点"
        "(半|((零|〇|一|二|两|三|四|五|六|七|八|九|十|[0-9]){1,3}分))?到?)",
        std::regex::ECMAScript);
    std::smatch replacement;
    if (!std::regex_search(remainder, replacement, kChineseTime)) {
        current = remainder;
        return true;
    }
    if (!replace_last_match(previous, kChineseTime, replacement.str())) {
        current = remainder;
        return true;
    }

    remainder.erase(
        static_cast<size_t>(replacement.position()),
        static_cast<size_t>(replacement.length()));
    remove_prefix_punctuation(remainder);
    current = remainder;
    return true;
}

bool apply_english_cross_phrase_correction(std::string & previous, std::string & current) {
    std::string remainder = trim(current);
    size_t marker_bytes = 0;
    for (const std::string marker : {"actually", "no,", "no ", "rather"}) {
        if (starts_with_ascii_case_insensitive(remainder, marker)) {
            marker_bytes = marker.size();
            break;
        }
    }
    if (marker_bytes == 0) {
        return false;
    }
    remainder.erase(0, marker_bytes);
    remove_prefix_punctuation(remainder);

    static const std::regex kWeekday(
        R"(\b(Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday)\b)",
        std::regex::ECMAScript | std::regex::icase);
    std::smatch replacement;
    if (!std::regex_search(remainder, replacement, kWeekday)) {
        current = remainder;
        return true;
    }
    if (!replace_last_match(previous, kWeekday, replacement.str())) {
        current = remainder;
        return true;
    }
    remainder.erase(
        static_cast<size_t>(replacement.position()),
        static_cast<size_t>(replacement.length()));
    remove_prefix_punctuation(remainder);
    if (starts_with_ascii_case_insensitive(remainder, "and ") &&
        !previous.empty() && previous.back() == '.') {
        previous.back() = ',';
    }
    current = remainder;
    return true;
}

void apply_inline_corrections(std::string & value) {
    static const std::regex kEnglishWeekday(
        R"(\b(Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday)\b\s*[,—-]?\s*(?:actually|no,?\s*)\s*(Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday)\b)",
        std::regex::ECMAScript | std::regex::icase);
    value = std::regex_replace(value, kEnglishWeekday, "$2");

    static const std::regex kChineseNotBut(
        "不是([^，。！？!?]{1,36})[，, ]*(是|改成)([^，。！？!?]{1,36})",
        std::regex::ECMAScript);
    value = std::regex_replace(value, kChineseNotBut, "$3");
}

std::vector<std::string> cleanup_window(const RefinementRequest & request) {
    std::vector<std::string> result;
    result.reserve(request.units.size());
    for (const auto & unit : request.units) {
        if (contains_japanese_script(unit.text) ||
            contains_korean_script(unit.text)) {
            result.push_back(trim(unit.text));
            continue;
        }
        std::string cleaned = remove_high_confidence_fillers(unit.text);
        apply_inline_corrections(cleaned);
        if (request.profile != AppProfile::Shell) {
            capitalize_initial_ascii(cleaned);
        }
        result.push_back(trim(std::move(cleaned)));
    }

    for (size_t index = 1; index < result.size(); ++index) {
        if (result[index].empty()) {
            continue;
        }
        if (contains_japanese_script(result[index - 1U]) ||
            contains_korean_script(result[index - 1U]) ||
            contains_japanese_script(result[index]) ||
            contains_korean_script(result[index])) {
            continue;
        }
        if (!apply_chinese_cross_phrase_correction(result[index - 1U], result[index])) {
            apply_english_cross_phrase_correction(result[index - 1U], result[index]);
        }
    }

    for (size_t index = 1; index < result.size(); ++index) {
        if (!result[index].empty() && result[index] == result[index - 1U]) {
            result[index].clear();
        }
    }
    return result;
}

RefinementResult make_result(
    const RefinementRequest & request,
    RefinementStage stage,
    std::vector<std::string> texts) {
    RefinementResult result;
    result.session_id = request.session_id;
    result.base_revision = request.base_revision;
    result.stage = stage;
    result.trusted_deterministic = true;
    for (size_t index = 0; index < request.units.size(); ++index) {
        result.units.push_back({request.units[index].id, std::move(texts[index])});
    }
    return result;
}
}

RefinementResult run_local_recognition_repair(const RefinementRequest & request) {
    std::vector<std::string> repaired;
    repaired.reserve(request.units.size());
    for (const auto & unit : request.units) {
        repaired.push_back(
            request.profile == AppProfile::Shell ||
                contains_japanese_script(unit.text) ||
                contains_korean_script(unit.text)
            ? unit.text
            : standard_grammar_correction(unit.text));
    }
    return make_result(
        request, RefinementStage::RecognitionRepair, std::move(repaired));
}

RefinementResult run_local_spoken_cleanup(const RefinementRequest & request) {
    return make_result(
        request, RefinementStage::SpokenCleanup, cleanup_window(request));
}

#include "rewrite_safety.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <regex>
#include <string_view>
#include <vector>

namespace {
std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character < 0x80U
            ? static_cast<char>(std::tolower(character))
            : static_cast<char>(character);
    });
    return value;
}

std::map<std::string, size_t> protected_tokens(const std::string & text) {
    static const std::regex kProtected(
        R"((https?://[^\s]+)|([A-Za-z]:\\[^\s]+)|((?:\.{0,2}/)[A-Za-z0-9_./-]+)|(--?[A-Za-z][A-Za-z0-9_-]*)|([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)|(\b\d+(?:[.:/-]\d+)*\b))",
        std::regex::ECMAScript);
    std::map<std::string, size_t> result;
    for (std::sregex_iterator it(text.begin(), text.end(), kProtected), end; it != end; ++it) {
        ++result[it->str()];
    }
    return result;
}

size_t count_ascii_word(const std::string & lowered, std::string_view word) {
    size_t count = 0;
    size_t offset = 0;
    while ((offset = lowered.find(word, offset)) != std::string::npos) {
        const bool left = offset == 0 ||
            !std::isalnum(static_cast<unsigned char>(lowered[offset - 1]));
        const size_t after = offset + word.size();
        const bool right = after == lowered.size() ||
            !std::isalnum(static_cast<unsigned char>(lowered[after]));
        if (left && right) {
            ++count;
        }
        offset = after;
    }
    return count;
}

size_t count_substring(const std::string & value, std::string_view needle) {
    size_t count = 0;
    size_t offset = 0;
    while ((offset = value.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

bool preserves_negation(const std::string & source, const std::string & candidate) {
    const std::string source_lower = lowercase_ascii(source);
    const std::string candidate_lower = lowercase_ascii(candidate);
    static constexpr std::string_view kEnglish[] = {
        "not", "never", "no", "only", "without", "must", "cannot", "can't", "don't",
    };
    for (std::string_view word : kEnglish) {
        if (count_ascii_word(source_lower, word) != count_ascii_word(candidate_lower, word)) {
            return false;
        }
    }
    static constexpr std::string_view kChinese[] = {
        "不要", "不能", "不可以", "没有", "从不", "只能", "必须",
    };
    for (std::string_view word : kChinese) {
        if (count_substring(source, word) != count_substring(candidate, word)) {
            return false;
        }
    }
    return true;
}

size_t count_case_sensitive_ascii_token(
    const std::string & text,
    const std::string & token) {
    size_t count = 0;
    size_t offset = 0;
    while ((offset = text.find(token, offset)) != std::string::npos) {
        const bool left = offset == 0 ||
            !std::isalnum(static_cast<unsigned char>(text[offset - 1]));
        const size_t after = offset + token.size();
        const bool right = after == text.size() ||
            !std::isalnum(static_cast<unsigned char>(text[after]));
        if (left && right) {
            ++count;
        }
        offset = after;
    }
    return count;
}

bool preserves_named_and_technical_tokens(
    const std::string & source,
    const std::string & candidate) {
    static const std::regex kCaseToken(
        R"(\b(?:[A-Z][A-Za-z0-9_-]*|[A-Za-z0-9_-]*[A-Z][A-Za-z0-9_-]*)\b)",
        std::regex::ECMAScript);
    std::map<std::string, size_t> source_tokens;
    for (std::sregex_iterator it(source.begin(), source.end(), kCaseToken), end;
         it != end; ++it) {
        ++source_tokens[it->str()];
    }
    for (const auto & [token, expected] : source_tokens) {
        if (count_case_sensitive_ascii_token(candidate, token) != expected) {
            return false;
        }
    }
    return true;
}

std::vector<uint32_t> utf8_code_points(const std::string & text) {
    std::vector<uint32_t> result;
    for (size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        uint32_t point = first;
        size_t width = 1;
        if ((first & 0xE0U) == 0xC0U && index + 1U < text.size()) {
            point = static_cast<uint32_t>(first & 0x1FU) << 6U;
            point |= static_cast<unsigned char>(text[index + 1U]) & 0x3FU;
            width = 2;
        } else if ((first & 0xF0U) == 0xE0U && index + 2U < text.size()) {
            point = static_cast<uint32_t>(first & 0x0FU) << 12U;
            point |= static_cast<uint32_t>(
                static_cast<unsigned char>(text[index + 1U]) & 0x3FU) << 6U;
            point |= static_cast<unsigned char>(text[index + 2U]) & 0x3FU;
            width = 3;
        } else if ((first & 0xF8U) == 0xF0U && index + 3U < text.size()) {
            point = static_cast<uint32_t>(first & 0x07U) << 18U;
            point |= static_cast<uint32_t>(
                static_cast<unsigned char>(text[index + 1U]) & 0x3FU) << 12U;
            point |= static_cast<uint32_t>(
                static_cast<unsigned char>(text[index + 2U]) & 0x3FU) << 6U;
            point |= static_cast<unsigned char>(text[index + 3U]) & 0x3FU;
            width = 4;
        }
        result.push_back(point);
        index += width;
    }
    return result;
}

bool is_cjk_ideograph(uint32_t point) {
    return (point >= 0x3400U && point <= 0x4DBFU) ||
           (point >= 0x4E00U && point <= 0x9FFFU) ||
           (point >= 0xF900U && point <= 0xFAFFU);
}

bool preserves_cjk_content(const std::string & source, const std::string & candidate) {
    std::map<uint32_t, size_t> source_counts;
    std::map<uint32_t, size_t> candidate_counts;
    size_t source_total = 0;
    for (uint32_t point : utf8_code_points(source)) {
        if (is_cjk_ideograph(point)) {
            ++source_counts[point];
            ++source_total;
        }
    }
    if (source_total < 6U) {
        return true;
    }
    for (uint32_t point : utf8_code_points(candidate)) {
        if (is_cjk_ideograph(point)) {
            ++candidate_counts[point];
        }
    }
    size_t retained = 0;
    for (const auto & [point, count] : source_counts) {
        retained += std::min(count, candidate_counts[point]);
    }
    // Clean has already removed fillers and explicit self-corrections. Adapt
    // may reorder Chinese prose, but losing more than one fifth of its source
    // ideographs is too risky for an automatic rewrite.
    return retained * 5U >= source_total * 4U;
}

std::vector<std::string> ascii_words(const std::string & text) {
    std::vector<std::string> words;
    std::string current;
    for (unsigned char character : text) {
        if (character < 0x80U && std::isalnum(character)) {
            current.push_back(static_cast<char>(std::tolower(character)));
        } else if (!current.empty()) {
            words.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        words.push_back(std::move(current));
    }
    return words;
}

size_t edit_distance(const std::vector<std::string> & left, const std::vector<std::string> & right) {
    std::vector<size_t> previous(right.size() + 1U);
    std::vector<size_t> current(right.size() + 1U);
    for (size_t index = 0; index <= right.size(); ++index) {
        previous[index] = index;
    }
    for (size_t row = 1; row <= left.size(); ++row) {
        current[0] = row;
        for (size_t column = 1; column <= right.size(); ++column) {
            current[column] = std::min({
                previous[column] + 1U,
                current[column - 1U] + 1U,
                previous[column - 1U] + (left[row - 1U] == right[column - 1U] ? 0U : 1U),
            });
        }
        previous.swap(current);
    }
    return previous.back();
}

bool shell_syntax_added(const std::string & source, const std::string & candidate) {
    if (candidate.find('\n') != std::string::npos || candidate.find('\r') != std::string::npos) {
        return true;
    }
    static constexpr std::string_view kOperators[] = {
        "&&", "||", "$(", ";", "|", ">", "<",
    };
    for (std::string_view token : kOperators) {
        if (count_substring(candidate, token) > count_substring(source, token)) {
            return true;
        }
    }
    return false;
}
}

RewriteSafetyResult validate_rewrite(
    const std::string & source,
    const std::string & candidate,
    OutputMode mode,
    AppProfile profile) {
    if (candidate.empty() && !source.empty()) {
        return {false, RewriteSafetyFailure::EmptyOutput};
    }
    const std::string lowered = lowercase_ascii(candidate);
    if (lowered.find("<|assistant") != std::string::npos ||
        lowered.find("</think>") != std::string::npos ||
        lowered.rfind("rewrite:", 0) == 0 ||
        lowered.rfind("rewritten text:", 0) == 0 ||
        lowered.rfind("corrected text:", 0) == 0 ||
        lowered.rfind("output:", 0) == 0) {
        return {false, RewriteSafetyFailure::ModelScaffolding};
    }
    if (!source.empty()) {
        const size_t minimum = mode == OutputMode::Adapt
            ? std::max<size_t>(1U, source.size() / 4U)
            : std::max<size_t>(1U, source.size() / 2U);
        const size_t maximum = source.size() * (mode == OutputMode::Adapt ? 3U : 2U) + 96U;
        if (candidate.size() < minimum || candidate.size() > maximum) {
            return {false, RewriteSafetyFailure::Length};
        }
    }
    if (profile == AppProfile::Shell && shell_syntax_added(source, candidate)) {
        return {false, RewriteSafetyFailure::ShellSyntax};
    }
    if (protected_tokens(source) != protected_tokens(candidate)) {
        return {false, RewriteSafetyFailure::ProtectedToken};
    }
    if (!preserves_named_and_technical_tokens(source, candidate)) {
        return {false, RewriteSafetyFailure::ProtectedToken};
    }
    if (!preserves_negation(source, candidate)) {
        return {false, RewriteSafetyFailure::Negation};
    }
    if (mode == OutputMode::Adapt && !preserves_cjk_content(source, candidate)) {
        return {false, RewriteSafetyFailure::ContentLoss};
    }
    if (mode == OutputMode::Clean) {
        const auto source_words = ascii_words(source);
        const auto candidate_words = ascii_words(candidate);
        if (!source_words.empty()) {
            const size_t budget = std::max<size_t>(2U,
                static_cast<size_t>(std::ceil(source_words.size() * 0.40)));
            if (edit_distance(source_words, candidate_words) > budget) {
                return {false, RewriteSafetyFailure::CleanEditBudget};
            }
        }
    }
    return {true, RewriteSafetyFailure::None};
}

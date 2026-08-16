#include "grammar_correction.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {
bool ascii_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

std::string trim_ascii_space(const std::string & text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char value) {
        return ascii_space(value);
    });
    if (first == text.end()) {
        return {};
    }
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char value) {
        return ascii_space(value);
    }).base();
    return std::string(first, last);
}

std::string lowercase_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return value < 0x80U ? static_cast<char>(std::tolower(value)) : static_cast<char>(value);
    });
    return text;
}

bool suspicious_prefix(const std::string & text) {
    const std::string lower = lowercase_ascii(text);
    constexpr std::string_view prefixes[]{
        "assistant:",
        "corrected:",
        "corrected text:",
        "here is",
        "input:",
        "output:",
    };
    for (const auto prefix : prefixes) {
        if (lower.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

bool token_contains_protected_character(const std::string & token) {
    return std::any_of(token.begin(), token.end(), [](unsigned char value) {
        return std::isdigit(value) != 0 || value == '@' || value == '_' ||
               value == '/' || value == '\\';
    });
}

std::vector<std::string> ascii_words(const std::string & text) {
    std::vector<std::string> words;
    size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() &&
               !(std::isalnum(static_cast<unsigned char>(text[start])) ||
                 text[start] == '\'')) {
            ++start;
        }
        size_t end = start;
        while (end < text.size() &&
               (std::isalnum(static_cast<unsigned char>(text[end])) ||
                text[end] == '\'')) {
            ++end;
        }
        if (end > start) {
            words.push_back(lowercase_ascii(text.substr(start, end - start)));
        }
        start = end;
    }
    return words;
}

size_t word_edit_distance(
    const std::vector<std::string> & before,
    const std::vector<std::string> & after) {
    std::vector<size_t> previous(after.size() + 1);
    std::vector<size_t> current(after.size() + 1);
    for (size_t index = 0; index <= after.size(); ++index) {
        previous[index] = index;
    }
    for (size_t row = 1; row <= before.size(); ++row) {
        current[0] = row;
        for (size_t column = 1; column <= after.size(); ++column) {
            const size_t substitution = before[row - 1] == after[column - 1] ? 0 : 1;
            current[column] = std::min({
                previous[column] + 1,
                current[column - 1] + 1,
                previous[column - 1] + substitution,
            });
        }
        previous.swap(current);
    }
    return previous.back();
}

size_t count_words_from_set(
    const std::vector<std::string> & words,
    const std::unordered_set<std::string> & values) {
    return static_cast<size_t>(std::count_if(words.begin(), words.end(), [&](const auto & word) {
        return values.find(word) != values.end();
    }));
}

bool preserves_closed_class_words(
    const std::vector<std::string> & original,
    const std::vector<std::string> & corrected) {
    static const std::unordered_set<std::string> pronouns{
        "i", "me", "my", "mine", "myself", "we", "us", "our", "ours", "ourselves",
        "you", "your", "yours", "yourself", "yourselves", "he", "him", "his", "himself",
        "she", "her", "hers", "herself", "it", "its", "itself", "they", "them", "their",
        "theirs", "themselves",
    };
    static const std::unordered_set<std::string> modals{
        "can", "could", "may", "might", "must", "shall", "should", "will", "would",
    };
    static const std::unordered_set<std::string> negations{
        "no", "not", "never", "don't", "doesn't", "didn't", "isn't", "aren't", "wasn't",
        "weren't", "can't", "couldn't", "won't", "wouldn't", "shouldn't", "haven't", "hasn't",
        "hadn't",
    };

    std::unordered_map<std::string, size_t> original_pronouns;
    std::unordered_map<std::string, size_t> corrected_pronouns;
    std::unordered_map<std::string, size_t> original_modals;
    std::unordered_map<std::string, size_t> corrected_modals;
    for (const auto & word : original) {
        if (pronouns.count(word) != 0) ++original_pronouns[word];
        if (modals.count(word) != 0) ++original_modals[word];
    }
    for (const auto & word : corrected) {
        if (pronouns.count(word) != 0) ++corrected_pronouns[word];
        if (modals.count(word) != 0) ++corrected_modals[word];
    }
    return original_pronouns == corrected_pronouns &&
           original_modals == corrected_modals &&
           count_words_from_set(original, negations) ==
               count_words_from_set(corrected, negations);
}

std::string regex_replace_all(
    const std::string & text,
    const char * pattern,
    const char * replacement) {
    return std::regex_replace(
        text, std::regex(pattern, std::regex_constants::icase), replacement);
}

bool has_terminal_punctuation(const std::string & text) {
    if (text.empty()) return false;
    const char last = text.back();
    return last == '.' || last == '!' || last == '?' ||
           (text.size() >= 3 &&
            (text.compare(text.size() - 3, 3, "。") == 0 ||
             text.compare(text.size() - 3, 3, "！") == 0 ||
             text.compare(text.size() - 3, 3, "？") == 0));
}

bool starts_like_question(const std::string & text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char value) {
        return ascii_space(value);
    });
    if (first == text.end() || static_cast<unsigned char>(*first) >= 0x80U) return false;
    const auto words = ascii_words(text);
    if (words.empty()) return false;
    static const std::unordered_set<std::string> question_starters{
        "who", "what", "when", "where", "why", "how", "which", "whose",
    };
    return question_starters.count(words.front()) != 0;
}

bool contains_cjk(const std::string & text) {
    for (size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        uint32_t codepoint = first;
        size_t width = 1;
        if ((first & 0xE0U) == 0xC0U && index + 1 < text.size()) {
            codepoint = ((first & 0x1FU) << 6) |
                        (static_cast<unsigned char>(text[index + 1]) & 0x3FU);
            width = 2;
        } else if ((first & 0xF0U) == 0xE0U && index + 2 < text.size()) {
            codepoint = ((first & 0x0FU) << 12) |
                        ((static_cast<unsigned char>(text[index + 1]) & 0x3FU) << 6) |
                        (static_cast<unsigned char>(text[index + 2]) & 0x3FU);
            width = 3;
        } else if ((first & 0xF8U) == 0xF0U && index + 3 < text.size()) {
            codepoint = ((first & 0x07U) << 18) |
                        ((static_cast<unsigned char>(text[index + 1]) & 0x3FU) << 12) |
                        ((static_cast<unsigned char>(text[index + 2]) & 0x3FU) << 6) |
                        (static_cast<unsigned char>(text[index + 3]) & 0x3FU);
            width = 4;
        }
        if ((codepoint >= 0x3400U && codepoint <= 0x9FFFU) ||
            (codepoint >= 0xF900U && codepoint <= 0xFAFFU) ||
            (codepoint >= 0x20000U && codepoint <= 0x2FA1FU)) {
            return true;
        }
        index += width;
    }
    return false;
}

void replace_all(std::string & text, const std::string & from, const std::string & to) {
    size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
}

std::vector<std::string> protected_tokens(const std::string & text) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() && ascii_space(static_cast<unsigned char>(text[start]))) {
            ++start;
        }
        size_t end = start;
        while (end < text.size() && !ascii_space(static_cast<unsigned char>(text[end]))) {
            ++end;
        }
        if (end == start) {
            break;
        }

        size_t clean_start = start;
        size_t clean_end = end;
        constexpr std::string_view removable = "\"'()[]{}<>,;:!?";
        while (clean_start < clean_end &&
               removable.find(text[clean_start]) != std::string_view::npos) {
            ++clean_start;
        }
        while (clean_end > clean_start &&
               removable.find(text[clean_end - 1]) != std::string_view::npos) {
            --clean_end;
        }
        const std::string token = text.substr(clean_start, clean_end - clean_start);
        if (!token.empty() && token_contains_protected_character(token)) {
            result.push_back(token);
        }
        start = end;
    }
    return result;
}

bool ends_utf8_character(const std::string & text, size_t index) {
    return index == 0 || index >= text.size() ||
           (static_cast<unsigned char>(text[index]) & 0xC0U) != 0x80U;
}

bool sentence_boundary_at(const std::string & text, size_t index) {
    if (index == 0 || index > text.size()) {
        return false;
    }
    const char last = text[index - 1];
    if (last == '.' || last == '?' || last == '!' || last == '\n') {
        return true;
    }
    constexpr std::string_view chinese_boundaries[]{"。", "？", "！"};
    for (const auto boundary : chinese_boundaries) {
        if (index >= boundary.size() &&
            text.compare(index - boundary.size(), boundary.size(), boundary) == 0) {
            return true;
        }
    }
    return false;
}
}

GrammarCorrectionMode default_grammar_correction_mode(bool onboarding_complete) {
    return onboarding_complete
        ? GrammarCorrectionMode::Off
        : GrammarCorrectionMode::Standard;
}

GrammarCorrectionMode migrate_grammar_correction_mode(
    std::optional<uint32_t> stored_mode,
    std::optional<uint32_t> legacy_enabled,
    bool onboarding_complete) {
    if (stored_mode && *stored_mode <= static_cast<uint32_t>(GrammarCorrectionMode::Advanced)) {
        return static_cast<GrammarCorrectionMode>(*stored_mode);
    }
    if (legacy_enabled) {
        return *legacy_enabled == 0
            ? GrammarCorrectionMode::Off
            : GrammarCorrectionMode::Advanced;
    }
    return default_grammar_correction_mode(onboarding_complete);
}

const wchar_t * grammar_correction_mode_name(GrammarCorrectionMode mode) {
    switch (mode) {
    case GrammarCorrectionMode::Off: return L"Off";
    case GrammarCorrectionMode::Standard: return L"Standard";
    case GrammarCorrectionMode::Advanced: return L"Advanced";
    }
    return L"Standard";
}

std::string standard_grammar_correction(const std::string & transcript) {
    std::string result = trim_ascii_space(transcript);
    if (result.empty()) return result;

    // Deliberately narrow, high-confidence rules. Standard mode is meant to
    // be predictable and tiny, not a general-purpose rewriting engine.
    result = regex_replace_all(result, R"(\b(he|she|it)\s+don't\s+likes\b)", "$1 doesn't like");
    result = regex_replace_all(result, R"(\b(he|she|it)\s+don't\s+like\b)", "$1 doesn't like");
    result = regex_replace_all(result, R"(\bI\s+has\s+finish\b)", "I finished");
    result = regex_replace_all(result, R"(\b(you|we|they)\s+has\b)", "$1 have");
    result = regex_replace_all(result, R"(\b(he|she|it)\s+have\b)", "$1 has");
    result = regex_replace_all(result, R"(\b(you|we|they)\s+was\b)", "$1 were");
    result = regex_replace_all(result, R"(\b(could|would|should)\s+of\b)", "$1 have");
    result = regex_replace_all(result, R"(\bdidn't\s+went\b)", "didn't go");
    result = regex_replace_all(result, R"(\bthis\s+are\b)", "this is");
    result = regex_replace_all(result, R"(\bthese\s+is\b)", "these are");
    result = regex_replace_all(result, R"(\bthere\s+is\s+(many|several)\b)", "there are $1");

    replace_all(result, "了但是", "了，但是");
    replace_all(result, "跑的很", "跑得很");
    replace_all(result, "渐渐的", "渐渐地");
    replace_all(result, "必需吃", "必须吃");
    replace_all(result, "必需做", "必须做");

    const auto first_non_space = std::find_if_not(result.begin(), result.end(), [](unsigned char value) {
        return ascii_space(value);
    });
    if (first_non_space != result.end() &&
        static_cast<unsigned char>(*first_non_space) < 0x80U &&
        std::islower(static_cast<unsigned char>(*first_non_space))) {
        *first_non_space = static_cast<char>(std::toupper(static_cast<unsigned char>(*first_non_space)));
    }
    if (!has_terminal_punctuation(result)) {
        if (starts_like_question(result)) {
            result.push_back('?');
        } else if (contains_cjk(result)) {
            result += "。";
        }
    }
    return result;
}

GrammarCorrectionResult process_grammar_correction(
    const std::string & transcript,
    GrammarCorrectionMode mode,
    GrammarCorrectionBackend * advanced_backend) {
    if (mode == GrammarCorrectionMode::Off || transcript.empty()) {
        return {transcript, GrammarCorrectionStatus::Disabled};
    }
    const std::string standard = standard_grammar_correction(transcript);
    if (mode == GrammarCorrectionMode::Standard) {
        return {standard, GrammarCorrectionStatus::StandardApplied};
    }
    if (advanced_backend == nullptr) {
        return {standard, GrammarCorrectionStatus::AdvancedFallback};
    }

    try {
        const std::string corrected = trim_ascii_space(advanced_backend->correct(transcript));
        if (!grammar_correction_output_is_safe(transcript, corrected)) {
            return {standard, GrammarCorrectionStatus::AdvancedFallback};
        }
        return {corrected, GrammarCorrectionStatus::AdvancedApplied};
    } catch (const GrammarCorrectionCancelled &) {
        return {transcript, GrammarCorrectionStatus::Skipped};
    } catch (const std::exception &) {
        return {standard, GrammarCorrectionStatus::AdvancedFallback};
    } catch (...) {
        return {standard, GrammarCorrectionStatus::AdvancedFallback};
    }
}

bool grammar_correction_output_is_safe(
    const std::string & original,
    const std::string & corrected) {
    if (original.empty()) {
        return corrected.empty();
    }
    if (corrected.empty() || corrected.find("<|") != std::string::npos ||
        corrected.find("</think>") != std::string::npos || suspicious_prefix(corrected)) {
        return false;
    }

    const size_t minimum_size = std::max<size_t>(1, original.size() / 3);
    const size_t maximum_size = original.size() * 3 + 96;
    if (corrected.size() < minimum_size || corrected.size() > maximum_size) {
        return false;
    }

    for (const auto & token : protected_tokens(original)) {
        if (corrected.find(token) == std::string::npos) {
            return false;
        }
    }
    const auto original_words = ascii_words(original);
    const auto corrected_words = ascii_words(corrected);
    if (!original_words.empty()) {
        const size_t maximum_edits = std::max<size_t>(
            2, static_cast<size_t>(std::ceil(original_words.size() * 0.50)));
        if (word_edit_distance(original_words, corrected_words) > maximum_edits ||
            !preserves_closed_class_words(original_words, corrected_words)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> split_for_grammar_correction(
    const std::string & text,
    size_t target_bytes,
    size_t maximum_bytes) {
    if (text.empty()) {
        return {};
    }
    target_bytes = std::max<size_t>(32, target_bytes);
    maximum_bytes = std::max(target_bytes, maximum_bytes);

    std::vector<std::string> chunks;
    size_t start = 0;
    while (text.size() - start > maximum_bytes) {
        const size_t hard_end = std::min(text.size(), start + maximum_bytes);
        const size_t preferred_end = std::min(hard_end, start + target_bytes);
        size_t end = 0;

        for (size_t candidate = preferred_end; candidate <= hard_end; ++candidate) {
            if (ends_utf8_character(text, candidate) && sentence_boundary_at(text, candidate)) {
                end = candidate;
                break;
            }
        }
        if (end == 0) {
            for (size_t candidate = preferred_end; candidate > start + target_bytes / 2; --candidate) {
                if (ends_utf8_character(text, candidate) && sentence_boundary_at(text, candidate)) {
                    end = candidate;
                    break;
                }
            }
        }
        if (end == 0) {
            for (size_t candidate = hard_end; candidate > start; --candidate) {
                if (ends_utf8_character(text, candidate) &&
                    ascii_space(static_cast<unsigned char>(text[candidate - 1]))) {
                    end = candidate;
                    break;
                }
            }
        }
        if (end == 0) {
            end = hard_end;
            while (end > start && !ends_utf8_character(text, end)) {
                --end;
            }
        }
        if (end <= start) {
            end = hard_end;
        }
        chunks.push_back(text.substr(start, end - start));
        start = end;
    }
    chunks.push_back(text.substr(start));
    return chunks;
}

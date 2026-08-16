#include "transcript.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
bool is_ascii_space(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool ascii_alphanumeric(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) != 0;
}

bool ascii_phrase_ending(char value) {
    return value == '.' || value == ',' || value == '!' || value == '?' ||
           value == ':' || value == ';';
}

bool utf8_phrase_ending(const std::string & text) {
    static constexpr const char * kEndings[] = {
        "\xE3\x80\x82", // IDEOGRAPHIC FULL STOP
        "\xEF\xBC\x81", // FULLWIDTH EXCLAMATION MARK
        "\xEF\xBC\x9F", // FULLWIDTH QUESTION MARK
        "\xEF\xBC\x8C", // FULLWIDTH COMMA
        "\xEF\xBC\x9A", // FULLWIDTH COLON
        "\xEF\xBC\x9B", // FULLWIDTH SEMICOLON
    };
    for (const char * ending : kEndings) {
        const std::string suffix(ending);
        if (text.size() >= suffix.size() &&
            text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return true;
        }
    }
    return false;
}

bool contains_ascii_alphanumeric(const std::string & text) {
    return std::any_of(text.begin(), text.end(), [](char value) {
        return ascii_alphanumeric(value);
    });
}

struct Utf8Character {
    uint32_t value = 0;
    std::string bytes;
};

std::vector<Utf8Character> decode_utf8(const std::string & text) {
    std::vector<Utf8Character> result;
    result.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        size_t width = 1;
        uint32_t value = first;
        if ((first & 0xE0U) == 0xC0U) {
            width = 2;
            value = first & 0x1FU;
        } else if ((first & 0xF0U) == 0xE0U) {
            width = 3;
            value = first & 0x0FU;
        } else if ((first & 0xF8U) == 0xF0U) {
            width = 4;
            value = first & 0x07U;
        }

        bool valid = index + width <= text.size();
        for (size_t offset = 1; valid && offset < width; ++offset) {
            const unsigned char continuation =
                static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
            value = (value << 6) | (continuation & 0x3FU);
        }
        if (!valid) {
            width = 1;
            value = first;
        }
        result.push_back({value, text.substr(index, width)});
        index += width;
    }
    return result;
}

std::string encode_utf8(uint32_t value) {
    std::string result;
    if (value <= 0x7FU) {
        result.push_back(static_cast<char>(value));
    } else if (value <= 0x7FFU) {
        result.push_back(static_cast<char>(0xC0U | (value >> 6)));
        result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else if (value <= 0xFFFFU) {
        result.push_back(static_cast<char>(0xE0U | (value >> 12)));
        result.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else {
        result.push_back(static_cast<char>(0xF0U | (value >> 18)));
        result.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    }
    return result;
}

bool is_east_asian_character(uint32_t value) {
    return (value >= 0x3400U && value <= 0x9FFFU) ||
           (value >= 0xF900U && value <= 0xFAFFU) ||
           (value >= 0x20000U && value <= 0x2FA1FU) ||
           (value >= 0x3040U && value <= 0x30FFU) ||
           (value >= 0xAC00U && value <= 0xD7AFU);
}

bool is_horizontal_space(uint32_t value) {
    return value == ' ' || value == '\t';
}

bool is_line_break(uint32_t value) {
    return value == '\r' || value == '\n';
}

bool is_digit(uint32_t value) {
    return value >= '0' && value <= '9';
}

bool is_terminal_mark(uint32_t value) {
    return value == '.' || value == '?' || value == '!' ||
           value == 0x3002U || value == 0xFF0EU ||
           value == 0xFF1FU || value == 0xFF01U;
}

bool is_sentence_boundary_at(
    const std::vector<Utf8Character> & characters,
    size_t index) {
    const uint32_t value = characters[index].value;
    if (is_line_break(value) || value == 0x3002U || value == 0xFF0EU ||
        value == 0xFF1FU || value == 0xFF01U) {
        return true;
    }
    if (value != '.' && value != '?' && value != '!') {
        return false;
    }
    return index + 1 >= characters.size() ||
           is_horizontal_space(characters[index + 1].value) ||
           is_line_break(characters[index + 1].value);
}

bool is_supported_punctuation(uint32_t value) {
    return value == ',' || value == '.' || value == '?' || value == '!' ||
           value == ':' || value == ';' || value == 0xFF0CU ||
           value == 0x3002U || value == 0xFF0EU || value == 0xFF1FU ||
           value == 0xFF01U || value == 0xFF1AU || value == 0xFF1BU;
}

bool is_chinese_style_punctuation(uint32_t value) {
    return value == 0xFF0CU || value == 0x3002U || value == 0xFF1FU ||
           value == 0xFF01U || value == 0xFF1AU || value == 0xFF1BU;
}

uint32_t chinese_punctuation(uint32_t value) {
    switch (value) {
    case ',': case 0xFF0CU: return 0xFF0CU;
    case '.': case 0x3002U: case 0xFF0EU: return 0x3002U;
    case '?': case 0xFF1FU: return 0xFF1FU;
    case '!': case 0xFF01U: return 0xFF01U;
    case ':': case 0xFF1AU: return 0xFF1AU;
    case ';': case 0xFF1BU: return 0xFF1BU;
    default: return value;
    }
}

uint32_t ascii_punctuation(uint32_t value) {
    switch (value) {
    case 0xFF0CU: return ',';
    case 0x3002U: case 0xFF0EU: return '.';
    case 0xFF1FU: return '?';
    case 0xFF01U: return '!';
    case 0xFF1AU: return ':';
    case 0xFF1BU: return ';';
    default: return value;
    }
}

size_t previous_nonspace(
    const std::vector<Utf8Character> & characters,
    size_t index) {
    while (index > 0) {
        --index;
        if (!is_horizontal_space(characters[index].value)) {
            return index;
        }
    }
    return characters.size();
}

size_t next_nonspace(
    const std::vector<Utf8Character> & characters,
    size_t index) {
    for (++index; index < characters.size(); ++index) {
        if (!is_horizontal_space(characters[index].value)) {
            return index;
        }
    }
    return characters.size();
}

bool clause_contains_east_asian(
    const std::vector<Utf8Character> & characters,
    size_t punctuation_index) {
    for (size_t index = punctuation_index; index > 0;) {
        --index;
        const uint32_t value = characters[index].value;
        if (is_sentence_boundary_at(characters, index)) {
            break;
        }
        if (is_east_asian_character(value)) {
            return true;
        }
    }
    for (size_t index = punctuation_index + 1; index < characters.size(); ++index) {
        const uint32_t value = characters[index].value;
        if (is_sentence_boundary_at(characters, index)) {
            break;
        }
        if (is_east_asian_character(value)) {
            return true;
        }
    }
    return false;
}

bool is_numeric_punctuation(
    uint32_t punctuation,
    uint32_t left,
    uint32_t right) {
    if (!is_digit(left) || !is_digit(right)) {
        return false;
    }
    return punctuation == '.' || punctuation == ',' ||
           punctuation == ':' || punctuation == 0xFF0EU ||
           punctuation == 0xFF0CU || punctuation == 0xFF1AU;
}

bool is_period_run(
    const std::vector<Utf8Character> & characters,
    size_t index) {
    if (characters[index].value != '.' &&
        characters[index].value != 0xFF0EU &&
        characters[index].value != 0x3002U) {
        return false;
    }
    return (index > 0 && (characters[index - 1].value == '.' ||
                          characters[index - 1].value == 0xFF0EU)) ||
           (index + 1 < characters.size() &&
            (characters[index + 1].value == '.' ||
             characters[index + 1].value == 0xFF0EU));
}

#ifdef _WIN32
bool is_cjk_character(wchar_t value) {
    return (value >= 0x3400 && value <= 0x4DBF) ||
           (value >= 0x4E00 && value <= 0x9FFF) ||
           (value >= 0xF900 && value <= 0xFAFF);
}

wchar_t chinese_punctuation_for(wchar_t value) {
    switch (value) {
    case L',': return L'\uFF0C';
    case L'.': return L'\u3002';
    case L'?': return L'\uFF1F';
    case L'!': return L'\uFF01';
    case L':': return L'\uFF1A';
    case L';': return L'\uFF1B';
    default: return value;
    }
}
#endif
}

std::string join_recognizer_segments(const std::vector<std::string> & segments) {
    std::string result;
    for (const auto & segment : segments) {
        result += segment;
    }

    const auto first = std::find_if_not(result.begin(), result.end(), [](unsigned char value) {
        return is_ascii_space(value);
    });
    if (first == result.end()) {
        return {};
    }
    const auto last = std::find_if_not(result.rbegin(), result.rend(), [](unsigned char value) {
        return is_ascii_space(value);
    }).base();
    return std::string(first, last);
}

std::string append_recognizer_segment(
    std::string & transcript,
    const std::string & segment) {
    const std::string part = join_recognizer_segments({segment});
    if (part.empty()) {
        return {};
    }

    std::string delta;
    if (!transcript.empty() && ascii_alphanumeric(part.front()) &&
        (ascii_alphanumeric(transcript.back()) ||
         ascii_phrase_ending(transcript.back()) ||
         (utf8_phrase_ending(transcript) &&
          contains_ascii_alphanumeric(transcript)))) {
        delta.push_back(' ');
    }
    delta += part;
    transcript += delta;
    return delta;
}

std::string capitalize_spelled_initialisms(const std::string & text) {
    std::string result;
    result.reserve(text.size());

    size_t index = 0;
    while (index < text.size()) {
        const auto is_letter = [](char value) {
            const unsigned char byte = static_cast<unsigned char>(value);
            return byte < 0x80U && std::isalpha(byte) != 0;
        };
        const bool starts_token = is_letter(text[index]) &&
            (index == 0 || !is_letter(text[index - 1]));
        if (!starts_token) {
            result.push_back(text[index++]);
            continue;
        }

        std::string letters;
        size_t token = index;
        size_t run_end = index;
        while (token < text.size() && is_letter(text[token]) &&
               (token + 1 == text.size() || is_ascii_space(
                   static_cast<unsigned char>(text[token + 1])))) {
            letters.push_back(static_cast<char>(
                std::toupper(static_cast<unsigned char>(text[token]))));
            run_end = token + 1;

            size_t next = run_end;
            while (next < text.size() &&
                   is_ascii_space(static_cast<unsigned char>(text[next]))) {
                ++next;
            }
            if (next >= text.size() || !is_letter(text[next])) {
                break;
            }
            token = next;
        }

        if (letters.size() >= 2) {
            result += letters;
            index = run_end;
        } else {
            result.push_back(text[index++]);
        }
    }
    return result;
}

std::string normalize_bilingual_punctuation(const std::string & text) {
    const std::vector<Utf8Character> characters = decode_utf8(text);
    std::vector<Utf8Character> output;
    output.reserve(characters.size());

    for (size_t index = 0; index < characters.size(); ++index) {
        Utf8Character current = characters[index];
        if (is_supported_punctuation(current.value)) {
            const size_t left_index = previous_nonspace(characters, index);
            const size_t right_index = next_nonspace(characters, index);
            const uint32_t left = left_index < characters.size()
                ? characters[left_index].value : 0;
            const uint32_t right = right_index < characters.size()
                ? characters[right_index].value : 0;
            const bool numeric = is_numeric_punctuation(current.value, left, right);
            const bool local_east_asian =
                is_east_asian_character(left) || is_east_asian_character(right);
            const bool ends_clause = right_index == characters.size() ||
                                     is_line_break(right);
            const bool clause_east_asian =
                clause_contains_east_asian(characters, index);
            const bool sentence_east_asian =
                is_terminal_mark(current.value) && ends_clause && clause_east_asian;
            const bool use_chinese = !numeric && !is_period_run(characters, index) &&
                (local_east_asian || sentence_east_asian ||
                 (is_chinese_style_punctuation(current.value) && clause_east_asian));
            current.value = use_chinese
                ? chinese_punctuation(current.value)
                : ascii_punctuation(current.value);
            current.bytes = encode_utf8(current.value);

            // A model occasionally emits a space before punctuation or the
            // same Chinese mark twice. Both are deterministic presentation
            // errors and can be repaired without touching any word.
            while (!output.empty() && is_horizontal_space(output.back().value)) {
                output.pop_back();
            }
            if (!output.empty() && current.value >= 0x80U &&
                (current.value == 0x3002U || current.value == 0xFF1FU ||
                 current.value == 0xFF01U) &&
                (output.back().value == ',' || output.back().value == ':' ||
                 output.back().value == ';' || output.back().value == 0xFF0CU ||
                 output.back().value == 0xFF1AU ||
                 output.back().value == 0xFF1BU)) {
                output.pop_back();
            }
            if (!output.empty() && current.value >= 0x80U &&
                output.back().value == current.value) {
                continue;
            }
        } else if (is_horizontal_space(current.value) && !output.empty() &&
                   output.back().value >= 0x80U &&
                   is_supported_punctuation(output.back().value)) {
            // Chinese typography does not put ASCII spaces after full-width
            // punctuation. Keep line breaks, which carry structure.
            continue;
        }
        output.push_back(std::move(current));
    }

    std::string result;
    result.reserve(text.size());
    for (const auto & character : output) {
        result += character.bytes;
    }
    return result;
}

std::string normalize_to_simplified_chinese(const std::string & text) {
#ifdef _WIN32
    if (text.empty()) {
        return {};
    }

    const int wide_count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_count <= 0) {
        return text;
    }

    std::wstring wide(static_cast<size_t>(wide_count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), wide.data(), wide_count) <= 0) {
        return text;
    }

    const int normalized_count = LCMapStringEx(
        L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, wide.data(), wide_count,
        nullptr, 0, nullptr, nullptr, 0);
    if (normalized_count <= 0) {
        return text;
    }

    std::wstring normalized(static_cast<size_t>(normalized_count), L'\0');
    if (LCMapStringEx(L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, wide.data(), wide_count,
                      normalized.data(), normalized_count, nullptr, nullptr, 0) <= 0) {
        return text;
    }

    for (size_t index = 0; index < normalized.size(); ++index) {
        const wchar_t mapped = chinese_punctuation_for(normalized[index]);
        if (mapped == normalized[index]) {
            continue;
        }
        const bool follows_chinese = index > 0 && is_cjk_character(normalized[index - 1]);
        const bool precedes_chinese = index + 1 < normalized.size() &&
                                      is_cjk_character(normalized[index + 1]);
        if (follows_chinese || precedes_chinese) {
            normalized[index] = mapped;
        }
    }

    const int utf8_count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, normalized.data(), normalized_count,
        nullptr, 0, nullptr, nullptr);
    if (utf8_count <= 0) {
        return text;
    }

    std::string result(static_cast<size_t>(utf8_count), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            normalized.data(), normalized_count,
                            result.data(), utf8_count, nullptr, nullptr) <= 0) {
        return text;
    }
    return normalize_bilingual_punctuation(result);
#else
    return normalize_bilingual_punctuation(text);
#endif
}

bool audio_has_signal(const std::vector<float> & samples) {
    constexpr size_t kMinimumSamples = 16000U / 4U;
    if (samples.size() < kMinimumSamples) {
        return false;
    }

    double square_sum = 0.0;
    float peak = 0.0F;
    for (const float sample : samples) {
        if (!std::isfinite(sample)) {
            continue;
        }
        const float absolute = std::abs(sample);
        peak = std::max(peak, absolute);
        square_sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const double rms = std::sqrt(square_sum / static_cast<double>(samples.size()));
    return peak >= 0.003F || rms >= 0.0005;
}

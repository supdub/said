#include "speech_language.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace {
struct Utf8Character {
    std::uint32_t value = 0;
    std::string_view bytes;
};

std::vector<Utf8Character> decode_utf8(std::string_view text) {
    std::vector<Utf8Character> result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        std::size_t width = 1;
        std::uint32_t value = first;
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
        for (std::size_t offset = 1; valid && offset < width; ++offset) {
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

bool is_japanese_script(std::uint32_t value) {
    return (value >= 0x3040U && value <= 0x30FFU) ||
           (value >= 0x31F0U && value <= 0x31FFU) ||
           (value >= 0xFF65U && value <= 0xFF9FU) ||
           (value >= 0x1AFF0U && value <= 0x1AFFFU) ||
           (value >= 0x1B000U && value <= 0x1B16FU);
}

bool is_korean_script(std::uint32_t value) {
    return (value >= 0x1100U && value <= 0x11FFU) ||
           (value >= 0x3130U && value <= 0x318FU) ||
           (value >= 0xA960U && value <= 0xA97FU) ||
           (value >= 0xAC00U && value <= 0xD7FFU) ||
           (value >= 0xFFA0U && value <= 0xFFDCU);
}

std::string canonical_tag(std::string_view tag) {
    std::string result;
    result.reserve(tag.size());
    for (const char value : tag) {
        const unsigned char byte = static_cast<unsigned char>(value);
        if (byte < 0x80U && std::isalpha(byte) != 0) {
            result.push_back(static_cast<char>(std::tolower(byte)));
        }
    }
    return result;
}

bool is_ascii_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

std::string trim_ascii_space(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), is_ascii_space);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(), is_ascii_space).base();
    return std::string(first, last);
}
}

SpeechLanguageMask sanitize_speech_languages(SpeechLanguageMask languages) {
    return (languages & kSupportedSpeechLanguages) | kRequiredSpeechLanguages;
}

bool speech_language_enabled(
    SpeechLanguageMask languages,
    SpeechLanguage language) {
    return (sanitize_speech_languages(languages) & speech_language_bit(language)) != 0;
}

SpeechLanguageMask set_speech_language_enabled(
    SpeechLanguageMask languages,
    SpeechLanguage language,
    bool enabled) {
    const SpeechLanguageMask bit = speech_language_bit(language);
    languages = enabled ? (languages | bit) : (languages & ~bit);
    return sanitize_speech_languages(languages);
}

std::optional<SpeechLanguage> speech_language_from_sense_voice_tag(
    std::string_view tag) {
    const std::string canonical = canonical_tag(tag);
    if (canonical == "zh" || canonical == "yue") {
        return SpeechLanguage::Chinese;
    }
    if (canonical == "en") {
        return SpeechLanguage::English;
    }
    if (canonical == "ja") {
        return SpeechLanguage::Japanese;
    }
    if (canonical == "ko") {
        return SpeechLanguage::Korean;
    }
    return std::nullopt;
}

bool sense_voice_language_allowed(
    std::string_view tag,
    SpeechLanguageMask languages) {
    const auto language = speech_language_from_sense_voice_tag(tag);
    return !language || speech_language_enabled(languages, *language);
}

bool contains_japanese_script(std::string_view text) {
    const auto characters = decode_utf8(text);
    return std::any_of(characters.begin(), characters.end(), [](const auto & character) {
        return is_japanese_script(character.value);
    });
}

bool contains_korean_script(std::string_view text) {
    const auto characters = decode_utf8(text);
    return std::any_of(characters.begin(), characters.end(), [](const auto & character) {
        return is_korean_script(character.value);
    });
}

std::string apply_speech_language_whitelist(
    std::string_view text,
    std::string_view sense_voice_tag,
    SpeechLanguageMask languages) {
    languages = sanitize_speech_languages(languages);
    if (!sense_voice_language_allowed(sense_voice_tag, languages)) {
        return {};
    }

    const bool allow_japanese =
        speech_language_enabled(languages, SpeechLanguage::Japanese);
    const bool allow_korean =
        speech_language_enabled(languages, SpeechLanguage::Korean);
    if (allow_japanese && allow_korean) {
        return trim_ascii_space(std::string(text));
    }

    std::string filtered;
    filtered.reserve(text.size());
    for (const auto & character : decode_utf8(text)) {
        if ((!allow_japanese && is_japanese_script(character.value)) ||
            (!allow_korean && is_korean_script(character.value))) {
            continue;
        }
        filtered.append(character.bytes.data(), character.bytes.size());
    }
    return trim_ascii_space(std::move(filtered));
}

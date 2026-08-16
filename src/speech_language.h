#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

enum class SpeechLanguage : std::uint32_t {
    Chinese = 1U << 0,
    English = 1U << 1,
    Japanese = 1U << 2,
    Korean = 1U << 3,
};

using SpeechLanguageMask = std::uint32_t;

constexpr SpeechLanguageMask speech_language_bit(SpeechLanguage language) {
    return static_cast<SpeechLanguageMask>(language);
}

constexpr SpeechLanguageMask kRequiredSpeechLanguages =
    speech_language_bit(SpeechLanguage::Chinese) |
    speech_language_bit(SpeechLanguage::English);
constexpr SpeechLanguageMask kSupportedSpeechLanguages =
    kRequiredSpeechLanguages |
    speech_language_bit(SpeechLanguage::Japanese) |
    speech_language_bit(SpeechLanguage::Korean);
constexpr SpeechLanguageMask kDefaultSpeechLanguages = kRequiredSpeechLanguages;

SpeechLanguageMask sanitize_speech_languages(SpeechLanguageMask languages);
bool speech_language_enabled(SpeechLanguageMask languages, SpeechLanguage language);
SpeechLanguageMask set_speech_language_enabled(
    SpeechLanguageMask languages,
    SpeechLanguage language,
    bool enabled);

// SenseVoice reports tags such as <|zh|>. Cantonese shares the Chinese
// writing-system allowance because SAID does not expose a separate Cantonese
// option in v1.
std::optional<SpeechLanguage> speech_language_from_sense_voice_tag(
    std::string_view tag);
bool sense_voice_language_allowed(
    std::string_view tag,
    SpeechLanguageMask languages);

bool contains_japanese_script(std::string_view text);
bool contains_korean_script(std::string_view text);

// Drops a phrase when SenseVoice positively identifies a disabled language,
// then removes any remaining disabled Japanese/Korean script from mixed text.
// Unknown or absent language tags fail open so recognition is never lost only
// because a future runtime changes its tag format.
std::string apply_speech_language_whitelist(
    std::string_view text,
    std::string_view sense_voice_tag,
    SpeechLanguageMask languages);

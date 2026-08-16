#include "speech_language.h"

#include <cassert>

int main() {
    assert(sanitize_speech_languages(0) == kDefaultSpeechLanguages);
    assert(speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::Chinese));
    assert(speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::English));
    assert(!speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::Japanese));
    assert(!speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::Korean));

    SpeechLanguageMask languages = set_speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::Japanese, true);
    assert(speech_language_enabled(languages, SpeechLanguage::Japanese));
    languages = set_speech_language_enabled(
        languages, SpeechLanguage::Korean, true);
    assert(speech_language_enabled(languages, SpeechLanguage::Korean));
    languages = set_speech_language_enabled(
        languages, SpeechLanguage::Chinese, false);
    assert(speech_language_enabled(languages, SpeechLanguage::Chinese));

    assert(speech_language_from_sense_voice_tag("<|zh|>") ==
           SpeechLanguage::Chinese);
    assert(speech_language_from_sense_voice_tag("<|yue|>") ==
           SpeechLanguage::Chinese);
    assert(speech_language_from_sense_voice_tag("en") ==
           SpeechLanguage::English);
    assert(speech_language_from_sense_voice_tag("<|ja|>") ==
           SpeechLanguage::Japanese);
    assert(speech_language_from_sense_voice_tag("<|ko|>") ==
           SpeechLanguage::Korean);
    assert(!speech_language_from_sense_voice_tag("<|future|>"));

    assert(apply_speech_language_whitelist(
               u8"你好 hello", "<|zh|>", kDefaultSpeechLanguages) ==
           u8"你好 hello");
    assert(apply_speech_language_whitelist(
               u8"こんにちは", "<|ja|>", kDefaultSpeechLanguages).empty());
    assert(apply_speech_language_whitelist(
               u8"안녕하세요", "<|ko|>", kDefaultSpeechLanguages).empty());
    assert(apply_speech_language_whitelist(
               u8"你好こんにちは hello", "<|zh|>", kDefaultSpeechLanguages) ==
           u8"你好 hello");
    assert(apply_speech_language_whitelist(
               u8"你好안녕하세요 hello", "<|zh|>", kDefaultSpeechLanguages) ==
           u8"你好 hello");

    const SpeechLanguageMask japanese = set_speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::Japanese, true);
    assert(apply_speech_language_whitelist(
               u8"明日の meeting は十時です", "<|ja|>", japanese) ==
           u8"明日の meeting は十時です");
    assert(contains_japanese_script(u8"明日の meeting は十時です"));
    assert(!contains_korean_script(u8"明日の meeting は十時です"));

    const SpeechLanguageMask korean = set_speech_language_enabled(
        kDefaultSpeechLanguages, SpeechLanguage::Korean, true);
    assert(apply_speech_language_whitelist(
               u8"내일 meeting은 열 시예요", "<|ko|>", korean) ==
           u8"내일 meeting은 열 시예요");
    assert(contains_korean_script(u8"내일 meeting은 열 시예요"));

    // Unknown tags fail open, but disabled scripts are still removed.
    assert(apply_speech_language_whitelist(
               u8"hello こんにちは", "", kDefaultSpeechLanguages) ==
           "hello");
    return 0;
}

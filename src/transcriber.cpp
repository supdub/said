#include "transcriber.h"

#include "speech_language.h"
#include "speech_models.h"
#include "transcript.h"

#include <sherpa-onnx/c-api/cxx-api.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {
constexpr int32_t kSampleRate = 16000;
constexpr int32_t kVadWindowSize = 512;
constexpr size_t kDirectRecognitionSamples = static_cast<size_t>(kSampleRate) * 25U;
constexpr size_t kFallbackChunkSamples = static_cast<size_t>(kSampleRate) * 20U;
constexpr float kStreamingMaximumPhraseSeconds = 6.0F;
constexpr size_t kStreamingDecodeTailSamples =
    static_cast<size_t>(kSampleRate) * 3U / 4U;
constexpr size_t kStreamingResidualTailSamples =
    static_cast<size_t>(kSampleRate) / 2U;

std::string path_utf8(const std::filesystem::path & path) {
    return path.u8string();
}

std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> create_vad(
    const std::string & model_path,
    float maximum_speech_seconds,
    float buffer_seconds) {
    sherpa_onnx::cxx::VadModelConfig config;
    config.silero_vad.model = model_path;
    config.silero_vad.threshold = 0.5F;
    config.silero_vad.min_silence_duration = 0.5F;
    config.silero_vad.min_speech_duration = 0.25F;
    config.silero_vad.max_speech_duration = maximum_speech_seconds;
    config.silero_vad.window_size = kVadWindowSize;
    config.sample_rate = kSampleRate;
    config.num_threads = 1;
    config.provider = "cpu";
    return std::make_unique<sherpa_onnx::cxx::VoiceActivityDetector>(
        sherpa_onnx::cxx::VoiceActivityDetector::Create(config, buffer_seconds));
}
}

struct Transcriber::Impl {
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recognizer;
    std::unique_ptr<sherpa_onnx::cxx::OfflinePunctuation> punctuation;
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad;
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> streaming_vad;
    std::string vad_model_path;
    std::vector<float> streaming_remainder;
    std::vector<float> streaming_uncommitted_audio;
    SpeechLanguageMask streaming_languages = kDefaultSpeechLanguages;
    bool streaming_disabled_language_tail_pending = false;
    bool streaming_active = false;

    std::string decode(
        const float * samples,
        size_t count,
        SpeechLanguageMask languages,
        bool * disabled_language_rejected = nullptr) const {
        if (disabled_language_rejected) {
            *disabled_language_rejected = false;
        }
        if (count == 0) {
            return {};
        }
        auto stream = recognizer->CreateStream();
        stream.AcceptWaveform(kSampleRate, samples, static_cast<int32_t>(count));
        recognizer->Decode(&stream);
        const auto result = recognizer->GetResult(&stream);
        const auto detected_language =
            speech_language_from_sense_voice_tag(result.lang);
        if (disabled_language_rejected && detected_language &&
            !speech_language_enabled(languages, *detected_language)) {
            *disabled_language_rejected = true;
        }
        std::string text = apply_speech_language_whitelist(
            result.text, result.lang, languages);
        if (text.empty()) {
            return {};
        }

        // SenseVoice deliberately runs without its built-in ITN: on mixed
        // speech it can alter English words. Restore acronyms separately. The
        // bundled punctuation model is Chinese/English-only, so Japanese and
        // Korean phrases bypass it and Chinese simplification.
        text = capitalize_spelled_initialisms(text);
        const bool japanese_or_korean =
            detected_language == SpeechLanguage::Japanese ||
            detected_language == SpeechLanguage::Korean ||
            contains_japanese_script(text) || contains_korean_script(text);
        if (!japanese_or_korean) {
            text = punctuation->AddPunctuation(text);
            text = normalize_to_simplified_chinese(text);
        }
        return join_recognizer_segments({text});
    }

    std::vector<std::string> drain_streaming_segments() {
        std::vector<std::string> phrases;
        bool consumed_segment = false;
        while (!streaming_vad->IsEmpty()) {
            const auto segment = streaming_vad->Front();
            // Decode the full audio accumulated around the VAD segment when
            // available. SenseVoice's language tag becomes unreliable on a
            // tightly trimmed short segment (for example, disabled Korean can
            // be misclassified as a one-letter English phrase), while the
            // surrounding silence preserves enough context for the same
            // language whitelist used by non-streaming transcription.
            const float * samples = segment.samples.data();
            size_t sample_count = segment.samples.size();
            std::vector<float> contextual_samples;
            if (!streaming_uncommitted_audio.empty()) {
                contextual_samples = streaming_uncommitted_audio;
                contextual_samples.resize(
                    contextual_samples.size() + kStreamingDecodeTailSamples, 0.0F);
                samples = contextual_samples.data();
                sample_count = contextual_samples.size();
            }
            bool disabled_language_rejected = false;
            std::string phrase = decode(
                samples, sample_count, streaming_languages,
                &disabled_language_rejected);
            const bool residual_disabled_language_tail =
                streaming_disabled_language_tail_pending &&
                !disabled_language_rejected &&
                segment.samples.size() <= kStreamingResidualTailSamples;
            if (disabled_language_rejected) {
                streaming_disabled_language_tail_pending = true;
            } else {
                streaming_disabled_language_tail_pending = false;
            }
            if (!phrase.empty() && !residual_disabled_language_tail) {
                phrases.push_back(std::move(phrase));
            }
            consumed_segment = true;
            streaming_vad->Pop();
        }
        if (consumed_segment) {
            streaming_uncommitted_audio.clear();
        }
        return phrases;
    }
};

Transcriber::Transcriber(const std::filesystem::path & model_path, int requested_threads)
    : impl_(std::make_unique<Impl>()) {
    if (!speech_models::complete_bundle(model_path)) {
        throw std::runtime_error(
            "The speech model bundle is incomplete. All four model files must be in: " +
            path_utf8(model_path.parent_path()));
    }

    const unsigned int available = std::max(1U, std::thread::hardware_concurrency());
    const int thread_count = requested_threads > 0
        ? std::clamp(requested_threads, 1, 16)
        : static_cast<int>(std::clamp(available / 2U, 2U, 8U));

    const auto files = speech_models::bundle_files(model_path);
    sherpa_onnx::cxx::OfflineRecognizerConfig recognizer_config;
    recognizer_config.model_config.sense_voice.model = path_utf8(files[0]);
    recognizer_config.model_config.sense_voice.language = "auto";
    recognizer_config.model_config.sense_voice.use_itn = false;
    recognizer_config.model_config.tokens = path_utf8(files[1]);
    recognizer_config.model_config.num_threads = thread_count;
    recognizer_config.model_config.provider = "cpu";
    impl_->recognizer = std::make_unique<sherpa_onnx::cxx::OfflineRecognizer>(
        sherpa_onnx::cxx::OfflineRecognizer::Create(recognizer_config));
    if (!impl_->recognizer->Get()) {
        throw std::runtime_error("Could not load the SenseVoice speech model: " + path_utf8(files[0]));
    }

    sherpa_onnx::cxx::OfflinePunctuationConfig punctuation_config;
    punctuation_config.model.ct_transformer = path_utf8(files[2]);
    punctuation_config.model.num_threads = std::min(thread_count, 4);
    punctuation_config.model.provider = "cpu";
    impl_->punctuation = std::make_unique<sherpa_onnx::cxx::OfflinePunctuation>(
        sherpa_onnx::cxx::OfflinePunctuation::Create(punctuation_config));
    if (!impl_->punctuation->Get()) {
        throw std::runtime_error("Could not load the punctuation model: " + path_utf8(files[2]));
    }

    impl_->vad_model_path = path_utf8(files[3]);
    impl_->vad = create_vad(impl_->vad_model_path, 20.0F, 30.0F);
    if (!impl_->vad->Get()) {
        throw std::runtime_error("Could not load the voice activity model: " + path_utf8(files[3]));
    }
}

Transcriber::~Transcriber() = default;

std::string Transcriber::transcribe(
    const std::vector<float> & samples,
    SpeechLanguageMask languages) {
    languages = sanitize_speech_languages(languages);
    if (!audio_has_signal(samples)) {
        return {};
    }

    if (samples.size() <= kDirectRecognitionSamples) {
        return impl_->decode(samples.data(), samples.size(), languages);
    }

    impl_->vad->Reset();
    std::string transcript;
    size_t segment_count = 0;
    size_t offset = 0;
    while (offset + static_cast<size_t>(kVadWindowSize) <= samples.size()) {
        impl_->vad->AcceptWaveform(samples.data() + offset, kVadWindowSize);
        offset += static_cast<size_t>(kVadWindowSize);
        while (!impl_->vad->IsEmpty()) {
            const auto segment = impl_->vad->Front();
            append_recognizer_segment(
                transcript,
                impl_->decode(segment.samples.data(), segment.samples.size(), languages));
            ++segment_count;
            impl_->vad->Pop();
        }
    }
    if (offset < samples.size()) {
        std::array<float, kVadWindowSize> final_window{};
        std::copy(samples.begin() + static_cast<std::ptrdiff_t>(offset), samples.end(),
                  final_window.begin());
        impl_->vad->AcceptWaveform(final_window.data(), kVadWindowSize);
    }
    impl_->vad->Flush();
    while (!impl_->vad->IsEmpty()) {
        const auto segment = impl_->vad->Front();
        append_recognizer_segment(
            transcript,
            impl_->decode(segment.samples.data(), segment.samples.size(), languages));
        ++segment_count;
        impl_->vad->Pop();
    }

    // A conservative amplitude gate can still admit audio that the VAD does
    // not classify. Keep a bounded fallback rather than dropping it silently.
    if (segment_count == 0) {
        for (size_t start = 0; start < samples.size(); start += kFallbackChunkSamples) {
            const size_t count = std::min(kFallbackChunkSamples, samples.size() - start);
            append_recognizer_segment(
                transcript, impl_->decode(samples.data() + start, count, languages));
        }
    }
    return transcript;
}

void Transcriber::start_streaming(SpeechLanguageMask languages) {
    if (!impl_->streaming_vad) {
        impl_->streaming_vad = create_vad(
            impl_->vad_model_path, kStreamingMaximumPhraseSeconds, 12.0F);
        if (!impl_->streaming_vad->Get()) {
            impl_->streaming_vad.reset();
            throw std::runtime_error("Could not start streaming voice activity detection.");
        }
    }
    impl_->streaming_vad->Reset();
    impl_->streaming_remainder.clear();
    impl_->streaming_uncommitted_audio.clear();
    impl_->streaming_languages = sanitize_speech_languages(languages);
    impl_->streaming_disabled_language_tail_pending = false;
    impl_->streaming_active = true;
}

std::vector<std::string> Transcriber::accept_streaming_audio(
    const std::vector<float> & samples) {
    if (!impl_->streaming_active) {
        throw std::logic_error("Streaming transcription has not been started.");
    }
    if (samples.empty()) {
        return {};
    }

    std::vector<float> input;
    input.reserve(impl_->streaming_remainder.size() + samples.size());
    input.insert(input.end(), impl_->streaming_remainder.begin(), impl_->streaming_remainder.end());
    input.insert(input.end(), samples.begin(), samples.end());
    impl_->streaming_remainder.clear();

    std::vector<std::string> phrases;
    size_t offset = 0;
    while (offset + static_cast<size_t>(kVadWindowSize) <= input.size()) {
        const float * window = input.data() + offset;
        impl_->streaming_uncommitted_audio.insert(
            impl_->streaming_uncommitted_audio.end(), window, window + kVadWindowSize);
        impl_->streaming_vad->AcceptWaveform(window, kVadWindowSize);
        offset += static_cast<size_t>(kVadWindowSize);

        auto completed = impl_->drain_streaming_segments();
        phrases.insert(phrases.end(),
                       std::make_move_iterator(completed.begin()),
                       std::make_move_iterator(completed.end()));
    }
    impl_->streaming_remainder.assign(
        input.begin() + static_cast<std::ptrdiff_t>(offset), input.end());
    return phrases;
}

std::vector<std::string> Transcriber::finish_streaming() {
    if (!impl_->streaming_active) {
        throw std::logic_error("Streaming transcription has not been started.");
    }

    if (!impl_->streaming_remainder.empty()) {
        impl_->streaming_uncommitted_audio.insert(
            impl_->streaming_uncommitted_audio.end(),
            impl_->streaming_remainder.begin(),
            impl_->streaming_remainder.end());
        std::array<float, kVadWindowSize> final_window{};
        std::copy(impl_->streaming_remainder.begin(),
                  impl_->streaming_remainder.end(), final_window.begin());
        impl_->streaming_vad->AcceptWaveform(final_window.data(), kVadWindowSize);
    }
    impl_->streaming_vad->Flush();
    std::vector<std::string> phrases = impl_->drain_streaming_segments();

    // Preserve the existing conservative amplitude fallback for a short final
    // phrase that the VAD does not classify.
    if (phrases.empty() && audio_has_signal(impl_->streaming_uncommitted_audio)) {
        bool disabled_language_rejected = false;
        std::string phrase = impl_->decode(
            impl_->streaming_uncommitted_audio.data(),
            impl_->streaming_uncommitted_audio.size(),
            impl_->streaming_languages,
            &disabled_language_rejected);
        const bool residual_disabled_language_tail =
            impl_->streaming_disabled_language_tail_pending &&
            !disabled_language_rejected &&
            impl_->streaming_uncommitted_audio.size() <=
                kStreamingResidualTailSamples;
        if (!phrase.empty() && !residual_disabled_language_tail) {
            phrases.push_back(std::move(phrase));
        }
    }

    impl_->streaming_remainder.clear();
    impl_->streaming_uncommitted_audio.clear();
    impl_->streaming_disabled_language_tail_pending = false;
    impl_->streaming_active = false;
    return phrases;
}

#include "transcriber.h"

#include "speech_models.h"
#include "transcript.h"

#include <sherpa-onnx/c-api/cxx-api.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>

namespace {
constexpr int32_t kSampleRate = 16000;
constexpr int32_t kVadWindowSize = 512;
constexpr size_t kDirectRecognitionSamples = static_cast<size_t>(kSampleRate) * 25U;
constexpr size_t kFallbackChunkSamples = static_cast<size_t>(kSampleRate) * 20U;

std::string path_utf8(const std::filesystem::path & path) {
    return path.u8string();
}

bool ascii_alphanumeric(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) != 0;
}

void append_part(std::string & transcript, const std::string & part) {
    if (part.empty()) {
        return;
    }
    if (!transcript.empty() && ascii_alphanumeric(transcript.back()) &&
        ascii_alphanumeric(part.front())) {
        transcript.push_back(' ');
    }
    transcript += part;
}
}

struct Transcriber::Impl {
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recognizer;
    std::unique_ptr<sherpa_onnx::cxx::OfflinePunctuation> punctuation;
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad;

    std::string decode(const float * samples, size_t count) const {
        if (count == 0) {
            return {};
        }
        auto stream = recognizer->CreateStream();
        stream.AcceptWaveform(kSampleRate, samples, static_cast<int32_t>(count));
        recognizer->Decode(&stream);
        std::string text = recognizer->GetResult(&stream).text;
        if (text.empty()) {
            return {};
        }

        // SenseVoice deliberately runs without its built-in ITN: on mixed
        // speech it can alter English words. Restore acronyms and punctuation
        // as separate, token-preserving passes instead.
        text = capitalize_spelled_initialisms(text);
        return join_recognizer_segments({punctuation->AddPunctuation(text)});
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

    sherpa_onnx::cxx::VadModelConfig vad_config;
    vad_config.silero_vad.model = path_utf8(files[3]);
    vad_config.silero_vad.threshold = 0.5F;
    vad_config.silero_vad.min_silence_duration = 0.5F;
    vad_config.silero_vad.min_speech_duration = 0.25F;
    vad_config.silero_vad.max_speech_duration = 20.0F;
    vad_config.silero_vad.window_size = kVadWindowSize;
    vad_config.sample_rate = kSampleRate;
    vad_config.num_threads = 1;
    vad_config.provider = "cpu";
    impl_->vad = std::make_unique<sherpa_onnx::cxx::VoiceActivityDetector>(
        sherpa_onnx::cxx::VoiceActivityDetector::Create(vad_config, 30.0F));
    if (!impl_->vad->Get()) {
        throw std::runtime_error("Could not load the voice activity model: " + path_utf8(files[3]));
    }
}

Transcriber::~Transcriber() = default;

std::string Transcriber::transcribe(const std::vector<float> & samples) {
    if (!audio_has_signal(samples)) {
        return {};
    }

    if (samples.size() <= kDirectRecognitionSamples) {
        return normalize_to_simplified_chinese(impl_->decode(samples.data(), samples.size()));
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
            append_part(transcript, impl_->decode(segment.samples.data(), segment.samples.size()));
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
        append_part(transcript, impl_->decode(segment.samples.data(), segment.samples.size()));
        ++segment_count;
        impl_->vad->Pop();
    }

    // A conservative amplitude gate can still admit audio that the VAD does
    // not classify. Keep a bounded fallback rather than dropping it silently.
    if (segment_count == 0) {
        for (size_t start = 0; start < samples.size(); start += kFallbackChunkSamples) {
            const size_t count = std::min(kFallbackChunkSamples, samples.size() - start);
            append_part(transcript, impl_->decode(samples.data() + start, count));
        }
    }
    return normalize_to_simplified_chinese(transcript);
}

#include "transcriber.h"

#include "transcript.h"

#include <whisper.h>

#include <algorithm>
#include <stdexcept>
#include <thread>

namespace {
constexpr char kDictationPrompt[] =
    "以下是简体中文和 English 的语音输入，例如 VoiceKey；使用自然、规范的标点。";
}

Transcriber::Transcriber(const std::filesystem::path & model_path, int requested_threads) {
    const unsigned int available = std::max(1U, std::thread::hardware_concurrency());
    thread_count_ = requested_threads > 0
        ? std::clamp(requested_threads, 1, 16)
        : static_cast<int>(std::clamp(available / 2U, 2U, 10U));

    whisper_context_params context_params = whisper_context_default_params();
    context_params.use_gpu = false;

    const std::string path_utf8 = model_path.u8string();
    context_ = whisper_init_from_file_with_params(path_utf8.c_str(), context_params);
    if (context_ == nullptr) {
        throw std::runtime_error("Could not load the speech model: " + path_utf8);
    }
    if (!whisper_is_multilingual(context_)) {
        whisper_free(context_);
        context_ = nullptr;
        throw std::runtime_error("The selected model is English-only. Use a multilingual Whisper model.");
    }
}

Transcriber::~Transcriber() {
    if (context_ != nullptr) {
        whisper_free(context_);
    }
}

std::string Transcriber::transcribe(const std::vector<float> & samples) {
    if (!audio_has_signal(samples)) {
        return {};
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    params.n_threads = thread_count_;
    params.translate = false;
    params.no_context = true;
    params.no_timestamps = false;
    params.single_segment = false;
    params.print_special = false;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.token_timestamps = false;
    params.language = "auto";
    params.detect_language = false;
    params.suppress_blank = true;
    params.suppress_nst = true;
    params.initial_prompt = kDictationPrompt;
    params.prompt_tokens = nullptr;
    params.prompt_n_tokens = 0;
    params.greedy.best_of = 5;
    params.beam_search.beam_size = 5;

    const int result = whisper_full(
        context_,
        params,
        samples.data(),
        static_cast<int>(samples.size()));
    if (result != 0) {
        throw std::runtime_error("Speech recognition failed with error " + std::to_string(result) + ".");
    }

    std::vector<std::string> segments;
    const int count = whisper_full_n_segments(context_);
    segments.reserve(static_cast<size_t>(std::max(0, count)));
    for (int index = 0; index < count; ++index) {
        const char * text = whisper_full_get_segment_text(context_, index);
        if (text != nullptr) {
            segments.emplace_back(text);
        }
    }
    return normalize_to_simplified_chinese(join_recognizer_segments(segments));
}

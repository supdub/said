#include "transcriber.h"

#include <miniaudio.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char ** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: said_transcribe_file SENSEVOICE_MODEL AUDIO [THREADS]\n"
                     "       Place the token, punctuation, and VAD files beside the model.\n";
        return 2;
    }

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 16000);
    ma_decoder decoder{};
    const ma_result init_result = ma_decoder_init_file(argv[2], &config, &decoder);
    if (init_result != MA_SUCCESS) {
        std::cerr << "could not decode audio: " << ma_result_description(init_result) << "\n";
        return 3;
    }

    ma_uint64 frame_count = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count) != MA_SUCCESS || frame_count == 0) {
        ma_decoder_uninit(&decoder);
        std::cerr << "audio file has no decodable frames\n";
        return 4;
    }

    constexpr ma_uint64 kMaximumTestFrames = 16000U * 60U * 5U;
    frame_count = std::min(frame_count, kMaximumTestFrames);
    std::vector<float> samples(static_cast<size_t>(frame_count));
    ma_uint64 frames_read = 0;
    const ma_result read_result = ma_decoder_read_pcm_frames(
        &decoder, samples.data(), frame_count, &frames_read);
    ma_decoder_uninit(&decoder);
    if (read_result != MA_SUCCESS && read_result != MA_AT_END) {
        std::cerr << "could not read audio: " << ma_result_description(read_result) << "\n";
        return 5;
    }
    samples.resize(static_cast<size_t>(frames_read));

    try {
        const auto load_started = std::chrono::steady_clock::now();
        const int requested_threads = argc == 4 ? std::stoi(argv[3]) : 0;
        Transcriber transcriber{std::filesystem::path(argv[1]), requested_threads};
        const auto load_finished = std::chrono::steady_clock::now();
        const std::string transcript = transcriber.transcribe(samples);
        const auto transcribe_finished = std::chrono::steady_clock::now();
        const auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            load_finished - load_started).count();
        const auto transcribe_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            transcribe_finished - load_finished).count();
        std::cerr << "model_load_ms=" << load_ms << " transcribe_ms=" << transcribe_ms << "\n";
        std::cout << transcript << "\n";
        return 0;
    } catch (const std::exception & error) {
        std::cerr << error.what() << "\n";
        return 6;
    }
}

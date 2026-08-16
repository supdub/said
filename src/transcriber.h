#pragma once

#include "speech_language.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Transcriber {
public:
    explicit Transcriber(const std::filesystem::path & model_path, int requested_threads = 0);
    ~Transcriber();

    Transcriber(const Transcriber &) = delete;
    Transcriber & operator=(const Transcriber &) = delete;

    std::string transcribe(
        const std::vector<float> & samples,
        SpeechLanguageMask languages = kDefaultSpeechLanguages);
    void start_streaming(
        SpeechLanguageMask languages = kDefaultSpeechLanguages);
    std::vector<std::string> accept_streaming_audio(const std::vector<float> & samples);
    std::vector<std::string> finish_streaming();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

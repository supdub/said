#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct whisper_context;

class Transcriber {
public:
    explicit Transcriber(const std::filesystem::path & model_path, int requested_threads = 0);
    ~Transcriber();

    Transcriber(const Transcriber &) = delete;
    Transcriber & operator=(const Transcriber &) = delete;

    std::string transcribe(const std::vector<float> & samples);

private:
    whisper_context * context_ = nullptr;
    int thread_count_ = 2;
};

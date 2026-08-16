#pragma once

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

    std::string transcribe(const std::vector<float> & samples);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

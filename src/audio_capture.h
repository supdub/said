#pragma once

#include <memory>
#include <cstddef>
#include <string>
#include <vector>

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    AudioCapture(const AudioCapture &) = delete;
    AudioCapture & operator=(const AudioCapture &) = delete;

    bool start(std::string & error);
    std::vector<float> stop();
    std::vector<float> samples_since(size_t offset) const;
    float level() const;
    bool running() const;
    bool reached_limit() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

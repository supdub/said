#pragma once

#include <memory>
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
    float level() const;
    bool running() const;
    bool reached_limit() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

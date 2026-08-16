#include "audio_capture.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
    AudioCapture capture;
    std::string error;
    if (!capture.start(error)) {
        std::cerr << error << "\n";
        return 1;
    }

    // Leave enough room for a slower Windows capture-period boundary. The
    // assertion below still rejects a stalled device, without making a
    // healthy microphone fail when the first packet arrives late.
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    const auto samples = capture.stop();
    if (samples.size() < 2000) {
        std::cerr << "microphone returned too few samples: " << samples.size() << "\n";
        return 2;
    }

    std::cout << "captured " << samples.size() << " mono samples at 16000 Hz\n";
    return 0;
}

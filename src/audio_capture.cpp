#define MINIAUDIO_IMPLEMENTATION
#include "audio_capture.h"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <utility>

namespace {
constexpr ma_uint32 kSampleRate = 16000;
constexpr size_t kMaxSamples = static_cast<size_t>(kSampleRate) * 60U * 10U;
}

struct AudioCapture::Impl {
    ma_device device{};
    bool initialized = false;
    std::atomic<bool> active{false};
    std::atomic<bool> limit_reached{false};
    std::atomic<float> current_level{0.0F};
    std::mutex samples_mutex;
    std::vector<float> samples;

    static void callback(ma_device * device, void *, const void * input, ma_uint32 frame_count) {
        auto * self = static_cast<Impl *>(device->pUserData);
        if (self == nullptr || input == nullptr || !self->active.load(std::memory_order_relaxed)) {
            return;
        }

        const auto * frames = static_cast<const float *>(input);
        double square_sum = 0.0;
        for (ma_uint32 i = 0; i < frame_count; ++i) {
            const float value = std::clamp(frames[i], -1.0F, 1.0F);
            square_sum += static_cast<double>(value) * static_cast<double>(value);
        }
        const float rms = frame_count == 0
            ? 0.0F
            : static_cast<float>(std::sqrt(square_sum / static_cast<double>(frame_count)));
        self->current_level.store(std::min(1.0F, rms * 12.0F), std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(self->samples_mutex);
        const size_t room = self->samples.size() < kMaxSamples
            ? kMaxSamples - self->samples.size()
            : 0;
        const size_t count = std::min<size_t>(room, frame_count);
        self->samples.insert(self->samples.end(), frames, frames + count);
        if (count < frame_count) {
            self->limit_reached.store(true, std::memory_order_relaxed);
        }
    }
};

AudioCapture::AudioCapture() : impl_(std::make_unique<Impl>()) {}

AudioCapture::~AudioCapture() {
    stop();
}

bool AudioCapture::start(std::string & error) {
    if (impl_->active.load(std::memory_order_relaxed)) {
        error = "The microphone is already recording.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->samples_mutex);
        impl_->samples.clear();
        impl_->samples.reserve(kSampleRate * 30U);
    }
    impl_->limit_reached.store(false, std::memory_order_relaxed);
    impl_->current_level.store(0.0F, std::memory_order_relaxed);

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = kSampleRate;
    config.periodSizeInMilliseconds = 20;
    config.dataCallback = Impl::callback;
    config.pUserData = impl_.get();

    const ma_result init_result = ma_device_init(nullptr, &config, &impl_->device);
    if (init_result != MA_SUCCESS) {
        error = std::string("Could not open the default microphone: ") + ma_result_description(init_result);
        return false;
    }
    impl_->initialized = true;
    impl_->active.store(true, std::memory_order_release);

    const ma_result start_result = ma_device_start(&impl_->device);
    if (start_result != MA_SUCCESS) {
        impl_->active.store(false, std::memory_order_release);
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
        error = std::string("Could not start the default microphone: ") + ma_result_description(start_result);
        return false;
    }

    return true;
}

std::vector<float> AudioCapture::stop() {
    if (impl_->initialized) {
        impl_->active.store(false, std::memory_order_release);
        ma_device_stop(&impl_->device);
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
    }
    impl_->current_level.store(0.0F, std::memory_order_relaxed);

    std::vector<float> result;
    {
        std::lock_guard<std::mutex> lock(impl_->samples_mutex);
        result.swap(impl_->samples);
    }
    return result;
}

std::vector<float> AudioCapture::samples_since(size_t offset) const {
    std::lock_guard<std::mutex> lock(impl_->samples_mutex);
    if (offset >= impl_->samples.size()) {
        return {};
    }
    return std::vector<float>(
        impl_->samples.begin() + static_cast<std::ptrdiff_t>(offset),
        impl_->samples.end());
}

float AudioCapture::level() const {
    return impl_->current_level.load(std::memory_order_relaxed);
}

bool AudioCapture::running() const {
    return impl_->active.load(std::memory_order_relaxed);
}

bool AudioCapture::reached_limit() const {
    return impl_->limit_reached.load(std::memory_order_relaxed);
}

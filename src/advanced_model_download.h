#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace advanced_model {
inline constexpr wchar_t kDisplayName[] = L"SAID Adapt writing model";
inline constexpr wchar_t kUrl[] =
    L"https://huggingface.co/Qwen/Qwen3-0.6B-GGUF/resolve/main/"
    L"Qwen3-0.6B-Q8_0.gguf";
inline constexpr char kSha256[] =
    "9465e63a22add5354d9bb4b99e90117043c7124007664907259bd16d043bb031";
inline constexpr uint64_t kDownloadBytes = 639446688ULL;
inline constexpr uint64_t kRequiredFreeBytes = 850ULL * 1024ULL * 1024ULL;

inline bool has_expected_size(const std::filesystem::path & path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error &&
           std::filesystem::file_size(path, error) == kDownloadBytes && !error;
}
}

enum class AdvancedModelState {
    NotInstalled,
    Downloading,
    Verifying,
    Installed,
    Failed,
    Cancelled,
};

struct AdvancedModelDownloadEvent {
    AdvancedModelState state = AdvancedModelState::NotInstalled;
    uint64_t transferred = 0;
    uint64_t total = advanced_model::kDownloadBytes;
    HRESULT error = S_OK;
};

class AdvancedModelDownloader {
public:
    AdvancedModelDownloader();
    ~AdvancedModelDownloader();

    AdvancedModelDownloader(const AdvancedModelDownloader &) = delete;
    AdvancedModelDownloader & operator=(const AdvancedModelDownloader &) = delete;

    bool start(HWND notify_window, UINT notify_message, std::filesystem::path destination);
    void cancel();
    bool active() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

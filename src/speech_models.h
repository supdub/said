#pragma once

#include <array>
#include <filesystem>
#include <system_error>

namespace speech_models {
inline constexpr wchar_t kRecognizer[] = L"sense-voice-small.int8.onnx";
inline constexpr wchar_t kTokens[] = L"sense-voice-small.tokens.txt";
inline constexpr wchar_t kPunctuation[] = L"ct-transformer-punctuation.int8.onnx";
inline constexpr wchar_t kVad[] = L"silero-vad.onnx";

inline std::array<std::filesystem::path, 4> bundle_files(
    const std::filesystem::path & recognizer_path) {
    const auto directory = recognizer_path.parent_path();
    return {
        recognizer_path,
        directory / kTokens,
        directory / kPunctuation,
        directory / kVad,
    };
}

inline bool complete_bundle(const std::filesystem::path & recognizer_path) {
    for (const auto & path : bundle_files(recognizer_path)) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            return false;
        }
    }
    return true;
}
}

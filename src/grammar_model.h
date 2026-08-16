#pragma once

#include <filesystem>
#include <system_error>

namespace grammar_model {
inline constexpr wchar_t kFilename[] = L"Qwen3-0.6B-Q8_0.gguf";

inline bool is_valid(const std::filesystem::path & path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}
}

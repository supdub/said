#include "win_util.h"

#include <shellapi.h>

#include <array>
#include <cstdlib>
#include <system_error>

std::wstring utf8_to_wide(const std::string & value) {
    if (value.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), count);
    return result;
}

std::string wide_to_utf8(const std::wstring & value) {
    if (value.empty()) {
        return {};
    }
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}

std::filesystem::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (count == 0 || count >= buffer.size()) {
        return std::filesystem::current_path();
    }
    buffer.resize(count);
    return std::filesystem::path(buffer).parent_path();
}

std::vector<std::wstring> command_line_arguments() {
    int count = 0;
    LPWSTR * raw = CommandLineToArgvW(GetCommandLineW(), &count);
    if (raw == nullptr) {
        return {};
    }
    std::vector<std::wstring> result;
    result.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        result.emplace_back(raw[index]);
    }
    LocalFree(raw);
    return result;
}

namespace {
bool is_file(const std::filesystem::path & path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

std::optional<std::filesystem::path> local_app_data_models_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
    if (count == 0 || count >= buffer.size()) {
        return std::nullopt;
    }
    buffer.resize(count);
    return std::filesystem::path(buffer) / L"VoiceKey" / L"models";
}
}

std::optional<std::filesystem::path> resolve_model_path(const std::vector<std::wstring> & arguments) {
    for (size_t index = 1; index + 1 < arguments.size(); ++index) {
        if (arguments[index] == L"--model") {
            const std::filesystem::path candidate(arguments[index + 1]);
            if (is_file(candidate)) {
                return candidate;
            }
            return std::nullopt;
        }
    }

    std::wstring environment(32768, L'\0');
    const DWORD environment_count = GetEnvironmentVariableW(
        L"VOICEKEY_MODEL", environment.data(), static_cast<DWORD>(environment.size()));
    if (environment_count > 0 && environment_count < environment.size()) {
        environment.resize(environment_count);
        const std::filesystem::path candidate(environment);
        if (is_file(candidate)) {
            return candidate;
        }
    }

    const std::filesystem::path base = executable_directory();
    const std::array candidates{
        base / L"models" / L"ggml-base-q8_0.bin",
        base / L"models" / L"ggml-base-q5_1.bin",
        base / L"models" / L"ggml-base.bin",
        base / L"ggml-base-q8_0.bin",
        base / L"ggml-base-q5_1.bin",
        base / L"ggml-base.bin",
    };
    for (const auto & candidate : candidates) {
        if (is_file(candidate)) {
            return candidate;
        }
    }

    const auto local = local_app_data_models_directory();
    if (local) {
        const std::array local_candidates{
            *local / L"ggml-base-q8_0.bin",
            *local / L"ggml-base-q5_1.bin",
            *local / L"ggml-base.bin",
        };
        for (const auto & candidate : local_candidates) {
            if (is_file(candidate)) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

std::filesystem::path expected_model_path() {
    const auto local = local_app_data_models_directory();
    if (local) {
        return *local / L"ggml-base-q8_0.bin";
    }
    return executable_directory() / L"models" / L"ggml-base-q8_0.bin";
}

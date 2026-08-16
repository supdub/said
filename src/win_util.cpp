#include "win_util.h"

#include "grammar_model.h"
#include "speech_models.h"

#include <shellapi.h>

#include <array>
#include <cstdlib>

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

HWND focused_control(HWND foreground_window) {
    if (foreground_window == nullptr) {
        return nullptr;
    }
    GUITHREADINFO information{};
    information.cbSize = sizeof(information);
    const DWORD thread = GetWindowThreadProcessId(foreground_window, nullptr);
    if (thread == 0 || !GetGUIThreadInfo(thread, &information)) {
        return nullptr;
    }
    return information.hwndFocus;
}

bool foreground_focus_matches(HWND foreground_window, HWND control) {
    return foreground_window != nullptr && control != nullptr &&
           GetForegroundWindow() == foreground_window &&
           focused_control(foreground_window) == control;
}

AppIdentity app_identity_for_window(HWND window) {
    AppIdentity identity;
    if (window == nullptr) {
        return identity;
    }

    const int title_length = GetWindowTextLengthW(window);
    if (title_length > 0) {
        identity.title.resize(static_cast<size_t>(title_length) + 1U);
        const int copied = GetWindowTextW(
            window, identity.title.data(), static_cast<int>(identity.title.size()));
        identity.title.resize(copied > 0 ? static_cast<size_t>(copied) : 0U);
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == 0) {
        return identity;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return identity;
    }
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (QueryFullProcessImageNameW(process, 0, path.data(), &size) && size > 0) {
        path.resize(size);
        identity.executable = std::filesystem::path(path).filename().wstring();
    }
    CloseHandle(process);
    return identity;
}

namespace {
std::optional<std::filesystem::path> local_app_data_models_directory(const wchar_t * product_name) {
    std::wstring buffer(32768, L'\0');
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
    if (count == 0 || count >= buffer.size()) {
        return std::nullopt;
    }
    buffer.resize(count);
    return std::filesystem::path(buffer) / product_name / L"models";
}

std::optional<std::filesystem::path> legacy_install_models_directory() {
    std::wstring buffer(32768, L'\0');
    DWORD size = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\VoiceKey", L"InstallDirectory",
                     RRF_RT_REG_SZ, nullptr, buffer.data(), &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    const size_t length = buffer.find(L'\0');
    if (length == 0 || length == std::wstring::npos) {
        return std::nullopt;
    }
    buffer.resize(length);
    return std::filesystem::path(buffer) / L"models";
}

std::optional<std::filesystem::path> environment_model(const wchar_t * name) {
    std::wstring environment(32768, L'\0');
    const DWORD environment_count = GetEnvironmentVariableW(
        name, environment.data(), static_cast<DWORD>(environment.size()));
    if (environment_count == 0 || environment_count >= environment.size()) {
        return std::nullopt;
    }
    environment.resize(environment_count);
    const std::filesystem::path candidate(environment);
    return speech_models::complete_bundle(candidate)
        ? std::optional<std::filesystem::path>(candidate)
        : std::nullopt;
}

std::optional<std::filesystem::path> environment_grammar_model(const wchar_t * name) {
    std::wstring environment(32768, L'\0');
    const DWORD environment_count = GetEnvironmentVariableW(
        name, environment.data(), static_cast<DWORD>(environment.size()));
    if (environment_count == 0 || environment_count >= environment.size()) {
        return std::nullopt;
    }
    environment.resize(environment_count);
    const std::filesystem::path candidate(environment);
    return grammar_model::is_valid(candidate)
        ? std::optional<std::filesystem::path>(candidate)
        : std::nullopt;
}

std::optional<std::filesystem::path> first_model_in(const std::filesystem::path & directory) {
    const auto candidate = directory / speech_models::kRecognizer;
    return speech_models::complete_bundle(candidate)
        ? std::optional<std::filesystem::path>(candidate)
        : std::nullopt;
}
}

std::optional<std::filesystem::path> resolve_model_path(const std::vector<std::wstring> & arguments) {
    for (size_t index = 1; index + 1 < arguments.size(); ++index) {
        if (arguments[index] == L"--model") {
            const std::filesystem::path candidate(arguments[index + 1]);
            if (speech_models::complete_bundle(candidate)) {
                return candidate;
            }
            return std::nullopt;
        }
    }

    if (const auto current_environment = environment_model(L"SAID_MODEL")) {
        return current_environment;
    }
    if (const auto legacy_environment = environment_model(L"VOICEKEY_MODEL")) {
        return legacy_environment;
    }

    const std::filesystem::path base = executable_directory();
    const std::array candidates{
        base / L"models" / speech_models::kRecognizer,
        base / speech_models::kRecognizer,
    };
    for (const auto & candidate : candidates) {
        if (speech_models::complete_bundle(candidate)) {
            return candidate;
        }
    }

    const auto local = local_app_data_models_directory(L"SAID");
    if (local) {
        if (const auto candidate = first_model_in(*local)) {
            return candidate;
        }
    }

    const auto legacy_local = local_app_data_models_directory(L"VoiceKey");
    if (legacy_local) {
        if (const auto candidate = first_model_in(*legacy_local)) {
            return candidate;
        }
    }
    const auto legacy_install = legacy_install_models_directory();
    if (legacy_install) {
        if (const auto candidate = first_model_in(*legacy_install)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> resolve_grammar_model_path(
    const std::vector<std::wstring> & arguments,
    const std::optional<std::filesystem::path> & speech_model_path) {
    for (size_t index = 1; index + 1 < arguments.size(); ++index) {
        if (arguments[index] == L"--grammar-model") {
            const std::filesystem::path candidate(arguments[index + 1]);
            return grammar_model::is_valid(candidate)
                ? std::optional<std::filesystem::path>(candidate)
                : std::nullopt;
        }
    }

    if (const auto environment = environment_grammar_model(L"SAID_GRAMMAR_MODEL")) {
        return environment;
    }
    if (speech_model_path) {
        const auto sibling = speech_model_path->parent_path() / grammar_model::kFilename;
        if (grammar_model::is_valid(sibling)) {
            return sibling;
        }
    }

    const std::filesystem::path base = executable_directory();
    const std::array candidates{
        base / L"models" / grammar_model::kFilename,
        base / grammar_model::kFilename,
    };
    for (const auto & candidate : candidates) {
        if (grammar_model::is_valid(candidate)) {
            return candidate;
        }
    }

    const auto local = local_app_data_models_directory(L"SAID");
    if (local) {
        const auto candidate = *local / grammar_model::kFilename;
        if (grammar_model::is_valid(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::filesystem::path expected_model_path() {
    const auto local = local_app_data_models_directory(L"SAID");
    if (local) {
        return *local / speech_models::kRecognizer;
    }
    return executable_directory() / L"models" / speech_models::kRecognizer;
}

std::filesystem::path expected_grammar_model_path() {
    const auto local = local_app_data_models_directory(L"SAID");
    if (local) {
        return *local / grammar_model::kFilename;
    }
    return executable_directory() / L"models" / grammar_model::kFilename;
}

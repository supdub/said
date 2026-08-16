#include "app_settings.h"

#include <array>
#include <optional>
#include <string>

namespace {
constexpr wchar_t kSettingsKey[] = L"Software\\SAID";
constexpr wchar_t kLegacySettingsKey[] = L"Software\\VoiceKey";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"SAID";
constexpr wchar_t kLegacyRunValue[] = L"VoiceKey";

std::optional<DWORD> read_dword(const wchar_t * key, const wchar_t * name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_DWORD,
                     &type, &value, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return value;
}

void write_dword(const wchar_t * name, DWORD value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(key, name, 0, REG_DWORD,
                       reinterpret_cast<const BYTE *>(&value), sizeof(value));
        RegCloseKey(key);
    }
}

std::wstring quoted_executable_path() {
    std::array<wchar_t, 32768> path{};
    const DWORD count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (count == 0 || count >= path.size()) {
        return {};
    }
    return L"\"" + std::wstring(path.data(), count) + L"\" --background";
}

bool run_entry_exists(const wchar_t * name) {
    wchar_t value[32768]{};
    DWORD size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, name, RRF_RT_REG_SZ,
                        nullptr, value, &size) == ERROR_SUCCESS;
}

bool write_run_entry(const wchar_t * name, const std::wstring & command) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegSetValueExW(
        key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
}

void AppSettings::load() {
    auto onboarding = read_dword(kSettingsKey, L"OnboardingComplete");
    if (!onboarding) {
        onboarding = read_dword(kLegacySettingsKey, L"OnboardingComplete");
        if (onboarding) {
            write_dword(L"OnboardingComplete", *onboarding);
        }
    }
    onboarding_complete_ = onboarding.value_or(0) != 0;

    auto shortcut_value = read_dword(kSettingsKey, L"Shortcut");
    if (!shortcut_value) {
        shortcut_value = read_dword(kLegacySettingsKey, L"Shortcut");
        if (shortcut_value) {
            write_dword(L"Shortcut", *shortcut_value);
        }
    }
    const DWORD shortcut = shortcut_value.value_or(static_cast<DWORD>(ShortcutKey::RightAlt));
    shortcut_ = shortcut == static_cast<DWORD>(ShortcutKey::F8)
        ? ShortcutKey::F8
        : ShortcutKey::RightAlt;

    if (run_entry_exists(kLegacyRunValue)) {
        if (!run_entry_exists(kRunValue)) {
            const std::wstring command = quoted_executable_path();
            if (!command.empty() && !write_run_entry(kRunValue, command)) {
                return;
            }
        }
        RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kLegacyRunValue);
    }
}

bool AppSettings::onboarding_complete() const {
    return onboarding_complete_;
}

void AppSettings::set_onboarding_complete(bool complete) {
    onboarding_complete_ = complete;
    write_dword(L"OnboardingComplete", complete ? 1U : 0U);
}

ShortcutKey AppSettings::shortcut() const {
    return shortcut_;
}

void AppSettings::set_shortcut(ShortcutKey shortcut) {
    shortcut_ = shortcut;
    write_dword(L"Shortcut", static_cast<DWORD>(shortcut));
}

bool AppSettings::launch_at_login() const {
    return run_entry_exists(kRunValue) || run_entry_exists(kLegacyRunValue);
}

bool AppSettings::set_launch_at_login(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring command = quoted_executable_path();
        if (command.empty()) {
            RegCloseKey(key);
            return false;
        }
        result = RegSetValueExW(key, kRunValue, 0, REG_SZ,
                               reinterpret_cast<const BYTE *>(command.c_str()),
                               static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            RegDeleteValueW(key, kLegacyRunValue);
        }
    } else {
        result = RegDeleteValueW(key, kRunValue);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
        const LONG legacy_result = RegDeleteValueW(key, kLegacyRunValue);
        if (result == ERROR_SUCCESS && legacy_result != ERROR_SUCCESS &&
            legacy_result != ERROR_FILE_NOT_FOUND) {
            result = legacy_result;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

const wchar_t * shortcut_name(ShortcutKey shortcut) {
    return shortcut == ShortcutKey::F8 ? L"F8" : L"Right Alt";
}

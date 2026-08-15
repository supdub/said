#include "app_settings.h"

#include <array>
#include <string>

namespace {
constexpr wchar_t kSettingsKey[] = L"Software\\VoiceKey";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

DWORD read_dword(const wchar_t * name, DWORD fallback) {
    DWORD value = fallback;
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_DWORD,
                     &type, &value, &size) != ERROR_SUCCESS) {
        return fallback;
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
}

void AppSettings::load() {
    onboarding_complete_ = read_dword(L"OnboardingComplete", 0) != 0;
    const DWORD shortcut = read_dword(L"Shortcut", static_cast<DWORD>(ShortcutKey::RightAlt));
    shortcut_ = shortcut == static_cast<DWORD>(ShortcutKey::F8)
        ? ShortcutKey::F8
        : ShortcutKey::RightAlt;
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
    wchar_t value[32768]{};
    DWORD size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, L"VoiceKey", RRF_RT_REG_SZ,
                        nullptr, value, &size) == ERROR_SUCCESS;
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
        result = RegSetValueExW(key, L"VoiceKey", 0, REG_SZ,
                               reinterpret_cast<const BYTE *>(command.c_str()),
                               static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, L"VoiceKey");
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

const wchar_t * shortcut_name(ShortcutKey shortcut) {
    return shortcut == ShortcutKey::F8 ? L"F8" : L"Right Alt";
}

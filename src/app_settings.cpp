#include "app_settings.h"

#include "output_mode.h"
#include "streaming_mode.h"

#include <array>
#include <optional>
#include <string>

namespace {
constexpr wchar_t kSettingsKey[] = L"Software\\SAID";
constexpr wchar_t kAppProfilesKey[] = L"Software\\SAID\\AppProfiles";
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

void write_dword_at(const wchar_t * key_path, const wchar_t * name, DWORD value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key_path, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(key, name, 0, REG_DWORD,
                       reinterpret_cast<const BYTE *>(&value), sizeof(value));
        RegCloseKey(key);
    }
}

void write_dword(const wchar_t * name, DWORD value) {
    write_dword_at(kSettingsKey, name, value);
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
    preview_ = false;
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
    DWORD shortcut = shortcut_value.value_or(static_cast<DWORD>(ShortcutKey::RightAlt));
    DWORD shortcut_modifiers = read_dword(kSettingsKey, L"ShortcutModifiers")
        .value_or(0) & kShortcutModifierMask;
    if (!is_valid_shortcut_binding(shortcut, shortcut_modifiers)) {
        shortcut = static_cast<DWORD>(ShortcutKey::RightAlt);
        shortcut_modifiers = 0;
    }
    shortcut_ = static_cast<ShortcutKey>(shortcut);
    shortcut_modifiers_ = shortcut_modifiers;

    output_mode_ = migrate_output_mode(
        read_dword(kSettingsKey, L"OutputMode"),
        read_dword(kSettingsKey, L"GrammarCorrectionMode"),
        read_dword(kSettingsKey, L"GrammarCorrection"),
        onboarding_complete_);
    write_dword(
        L"OutputMode",
        static_cast<DWORD>(output_mode_));
    advanced_model_download_pending_ =
        read_dword(kSettingsKey, L"AdvancedModelDownloadPending").value_or(0) != 0;

    // Streaming changes when text is delivered, so it is always opt-in for
    // both new installations and upgrades.
    streaming_mode_enabled_ = read_dword(kSettingsKey, L"StreamingMode")
        .value_or(default_streaming_mode_enabled() ? 1U : 0U) != 0;

    speech_languages_ = sanitize_speech_languages(
        read_dword(kSettingsKey, L"SpeechLanguages")
            .value_or(kDefaultSpeechLanguages));
    write_dword(L"SpeechLanguages", static_cast<DWORD>(speech_languages_));

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

void AppSettings::use_preview_defaults() {
    preview_ = true;
    preview_launch_at_login_ = false;
    onboarding_complete_ = false;
    shortcut_ = ShortcutKey::RightAlt;
    shortcut_modifiers_ = 0;
    output_mode_ = default_output_mode();
    advanced_model_download_pending_ = false;
    streaming_mode_enabled_ = default_streaming_mode_enabled();
    speech_languages_ = kDefaultSpeechLanguages;
}

bool AppSettings::onboarding_complete() const {
    return onboarding_complete_;
}

void AppSettings::set_onboarding_complete(bool complete) {
    onboarding_complete_ = complete;
    if (!preview_) {
        write_dword(L"OnboardingComplete", complete ? 1U : 0U);
    }
}

ShortcutKey AppSettings::shortcut() const {
    return shortcut_;
}

DWORD AppSettings::shortcut_modifiers() const {
    return shortcut_modifiers_;
}

void AppSettings::set_shortcut(ShortcutKey shortcut, DWORD modifiers) {
    modifiers &= kShortcutModifierMask;
    if (!is_valid_shortcut_binding(static_cast<DWORD>(shortcut), modifiers)) {
        shortcut = ShortcutKey::RightAlt;
        modifiers = 0;
    }
    shortcut_ = shortcut;
    shortcut_modifiers_ = modifiers;
    if (!preview_) {
        write_dword(L"Shortcut", static_cast<DWORD>(shortcut));
        write_dword(L"ShortcutModifiers", shortcut_modifiers_);
    }
}

bool AppSettings::launch_at_login() const {
    if (preview_) {
        return preview_launch_at_login_;
    }
    return run_entry_exists(kRunValue) || run_entry_exists(kLegacyRunValue);
}

bool AppSettings::set_launch_at_login(bool enabled) {
    if (preview_) {
        preview_launch_at_login_ = enabled;
        return true;
    }
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

OutputMode AppSettings::output_mode() const {
    return output_mode_;
}

void AppSettings::set_output_mode(OutputMode mode) {
    if (mode != OutputMode::Exact &&
        mode != OutputMode::Clean &&
        mode != OutputMode::Adapt) {
        mode = OutputMode::Clean;
    }
    output_mode_ = mode;
    if (!preview_) {
        write_dword(L"OutputMode", static_cast<DWORD>(mode));
    }
}

bool AppSettings::advanced_model_download_pending() const {
    return advanced_model_download_pending_;
}

void AppSettings::set_advanced_model_download_pending(bool pending) {
    advanced_model_download_pending_ = pending;
    if (!preview_) {
        write_dword(L"AdvancedModelDownloadPending", pending ? 1U : 0U);
    }
}

bool AppSettings::streaming_mode_enabled() const {
    return streaming_mode_enabled_;
}

void AppSettings::set_streaming_mode_enabled(bool enabled) {
    streaming_mode_enabled_ = enabled;
    if (!preview_) {
        write_dword(L"StreamingMode", enabled ? 1U : 0U);
    }
}

SpeechLanguageMask AppSettings::speech_languages() const {
    return speech_languages_;
}

void AppSettings::set_speech_language_enabled(
    SpeechLanguage language,
    bool enabled) {
    speech_languages_ = ::set_speech_language_enabled(
        speech_languages_, language, enabled);
    if (!preview_) {
        write_dword(L"SpeechLanguages", static_cast<DWORD>(speech_languages_));
    }
}

std::optional<AppProfile> AppSettings::app_profile_override(
    const AppIdentity & identity) const {
    if (preview_) {
        return std::nullopt;
    }
    const std::wstring key = app_identity_override_key(identity);
    const auto stored = read_dword(kAppProfilesKey, key.c_str());
    if (!stored || *stored > static_cast<DWORD>(AppProfile::Shell)) {
        return std::nullopt;
    }
    const AppProfile profile = static_cast<AppProfile>(*stored);
    return profile == AppProfile::Unknown
        ? std::nullopt
        : std::optional<AppProfile>(profile);
}

void AppSettings::set_app_profile_override(
    const AppIdentity & identity,
    std::optional<AppProfile> profile) {
    if (preview_) {
        return;
    }
    const std::wstring key = app_identity_override_key(identity);
    if (!profile || *profile == AppProfile::Unknown) {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, kAppProfilesKey, key.c_str());
        return;
    }
    write_dword_at(kAppProfilesKey, key.c_str(), static_cast<DWORD>(*profile));
}

namespace {
std::wstring virtual_key_name(DWORD virtual_key) {
    switch (virtual_key) {
    case VK_BACK: return L"Backspace";
    case VK_RETURN: return L"Enter";
    case VK_SPACE: return L"Space";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_SNAPSHOT: return L"Print Screen";
    case VK_RMENU: return L"Right Alt";
    default:
        break;
    }

    UINT scan_code = MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC);
    switch (virtual_key) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_DIVIDE:
    case VK_NUMLOCK:
        scan_code |= 0x100;
        break;
    default:
        break;
    }
    wchar_t name[64]{};
    if (GetKeyNameTextW(static_cast<LONG>(scan_code << 16), name,
                        static_cast<int>(std::size(name))) > 0) {
        return name;
    }
    return L"Key " + std::to_wstring(virtual_key);
}
}

std::wstring shortcut_name(ShortcutKey shortcut, DWORD modifiers) {
    const DWORD key = static_cast<DWORD>(shortcut);
    if (shortcut == ShortcutKey::RightAlt && modifiers == 0) {
        return L"Right Alt";
    }
    if (shortcut == ShortcutKey::F8 && modifiers == 0) {
        return L"F8";
    }

    std::wstring name;
    const auto append_modifier = [&name](const wchar_t * modifier) {
        if (!name.empty()) name += L" + ";
        name += modifier;
    };
    if ((modifiers & kShortcutModifierControl) != 0) append_modifier(L"Ctrl");
    if ((modifiers & kShortcutModifierAlt) != 0) append_modifier(L"Alt");
    if ((modifiers & kShortcutModifierShift) != 0) append_modifier(L"Shift");
    if ((modifiers & kShortcutModifierWindows) != 0) append_modifier(L"Win");
    append_modifier(virtual_key_name(key).c_str());
    return name;
}

#pragma once

#include "app_profile.h"
#include "output_mode.h"
#include "shortcut_binding.h"
#include "speech_language.h"

#include <windows.h>

#include <optional>
#include <string>

enum class ShortcutKey : DWORD {
    RightAlt = VK_RMENU,
    F8 = VK_F8,
};

class AppSettings {
public:
    void load();
    void use_preview_defaults();

    bool onboarding_complete() const;
    void set_onboarding_complete(bool complete);

    ShortcutKey shortcut() const;
    DWORD shortcut_modifiers() const;
    void set_shortcut(ShortcutKey shortcut, DWORD modifiers = 0);

    bool launch_at_login() const;
    bool set_launch_at_login(bool enabled);

    OutputMode output_mode() const;
    void set_output_mode(OutputMode mode);

    bool advanced_model_download_pending() const;
    void set_advanced_model_download_pending(bool pending);

    bool streaming_mode_enabled() const;
    void set_streaming_mode_enabled(bool enabled);

    SpeechLanguageMask speech_languages() const;
    void set_speech_language_enabled(SpeechLanguage language, bool enabled);

    std::optional<AppProfile> app_profile_override(const AppIdentity & identity) const;
    void set_app_profile_override(
        const AppIdentity & identity,
        std::optional<AppProfile> profile);

private:
    bool preview_ = false;
    bool preview_launch_at_login_ = false;
    bool onboarding_complete_ = false;
    ShortcutKey shortcut_ = ShortcutKey::RightAlt;
    DWORD shortcut_modifiers_ = 0;
    OutputMode output_mode_ = OutputMode::Clean;
    bool advanced_model_download_pending_ = false;
    bool streaming_mode_enabled_ = false;
    SpeechLanguageMask speech_languages_ = kDefaultSpeechLanguages;
};

std::wstring shortcut_name(ShortcutKey shortcut, DWORD modifiers = 0);

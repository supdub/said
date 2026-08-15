#pragma once

#include <windows.h>

enum class ShortcutKey : DWORD {
    RightAlt = VK_RMENU,
    F8 = VK_F8,
};

class AppSettings {
public:
    void load();

    bool onboarding_complete() const;
    void set_onboarding_complete(bool complete);

    ShortcutKey shortcut() const;
    void set_shortcut(ShortcutKey shortcut);

    bool launch_at_login() const;
    bool set_launch_at_login(bool enabled);

private:
    bool onboarding_complete_ = false;
    ShortcutKey shortcut_ = ShortcutKey::RightAlt;
};

const wchar_t * shortcut_name(ShortcutKey shortcut);

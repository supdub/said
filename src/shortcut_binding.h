#pragma once

#include <array>
#include <cstdint>

constexpr std::uint32_t kShortcutModifierControl = 1U << 0;
constexpr std::uint32_t kShortcutModifierAlt = 1U << 1;
constexpr std::uint32_t kShortcutModifierShift = 1U << 2;
constexpr std::uint32_t kShortcutModifierWindows = 1U << 3;
constexpr std::uint32_t kShortcutModifierMask = kShortcutModifierControl |
    kShortcutModifierAlt | kShortcutModifierShift | kShortcutModifierWindows;

bool is_shortcut_modifier_key(std::uint32_t virtual_key);
bool is_valid_shortcut_key(std::uint32_t virtual_key);
bool is_valid_shortcut_binding(
    std::uint32_t virtual_key,
    std::uint32_t modifiers);

enum class ShortcutKeyEvent {
    Down,
    Up,
};

struct ShortcutHookAction {
    bool consume = false;
    bool toggle = false;
    bool external_input = false;
};

// Tracks a configured shortcut from the keyboard events themselves. Low-level
// keyboard hooks run before Windows updates its asynchronous key state, so the
// event stream is the reliable source of modifier and press/release state.
class ShortcutPressMatcher {
public:
    void set_binding(
        std::uint32_t virtual_key,
        std::uint32_t modifiers);
    void seed_modifiers(std::uint32_t modifiers);

    ShortcutHookAction handle_event(
        std::uint32_t virtual_key,
        std::uint32_t event_modifier,
        bool is_right_alt,
        ShortcutKeyEvent event,
        bool injected,
        bool track_external_input);

    std::uint32_t active_modifiers() const;

private:
    void update_modifier(
        std::uint32_t virtual_key,
        std::uint32_t modifier,
        bool down);
    std::uint32_t modifiers_from_keys() const;

    std::uint32_t virtual_key_ = 0;
    std::uint32_t modifiers_ = 0;
    std::uint32_t seeded_modifiers_ = 0;
    std::array<std::uint32_t, 256> modifier_keys_{};
    bool shortcut_down_ = false;
};

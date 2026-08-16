#include "shortcut_binding.h"

namespace {
// Windows virtual-key values are part of the persisted shortcut format. Keep
// this module independent of windows.h so the matching contract can run in the
// platform-independent regression suite.
constexpr std::uint32_t kVirtualKeyBack = 0x08;
constexpr std::uint32_t kVirtualKeyTab = 0x09;
constexpr std::uint32_t kVirtualKeyReturn = 0x0D;
constexpr std::uint32_t kVirtualKeyShift = 0x10;
constexpr std::uint32_t kVirtualKeyControl = 0x11;
constexpr std::uint32_t kVirtualKeyMenu = 0x12;
constexpr std::uint32_t kVirtualKeyEscape = 0x1B;
constexpr std::uint32_t kVirtualKeySpace = 0x20;
constexpr std::uint32_t kVirtualKeyLWin = 0x5B;
constexpr std::uint32_t kVirtualKeyRWin = 0x5C;
constexpr std::uint32_t kVirtualKeyNumpad0 = 0x60;
constexpr std::uint32_t kVirtualKeyDivide = 0x6F;
constexpr std::uint32_t kVirtualKeyLShift = 0xA0;
constexpr std::uint32_t kVirtualKeyRShift = 0xA1;
constexpr std::uint32_t kVirtualKeyLControl = 0xA2;
constexpr std::uint32_t kVirtualKeyRControl = 0xA3;
constexpr std::uint32_t kVirtualKeyLMenu = 0xA4;
constexpr std::uint32_t kVirtualKeyRMenu = 0xA5;
constexpr std::uint32_t kVirtualKeyOem1 = 0xBA;
constexpr std::uint32_t kVirtualKeyOem102 = 0xE2;
constexpr std::uint32_t kVirtualKeyDelete = 0x2E;

bool is_typing_key(std::uint32_t virtual_key) {
    return (virtual_key >= static_cast<std::uint32_t>('0') &&
            virtual_key <= static_cast<std::uint32_t>('9')) ||
        (virtual_key >= static_cast<std::uint32_t>('A') &&
         virtual_key <= static_cast<std::uint32_t>('Z')) ||
        (virtual_key >= kVirtualKeyNumpad0 &&
         virtual_key <= kVirtualKeyDivide) ||
        (virtual_key >= kVirtualKeyOem1 &&
         virtual_key <= kVirtualKeyOem102) ||
        virtual_key == kVirtualKeySpace ||
        virtual_key == kVirtualKeyReturn ||
        virtual_key == kVirtualKeyBack;
}
}

bool is_shortcut_modifier_key(std::uint32_t virtual_key) {
    switch (virtual_key) {
    case kVirtualKeyShift:
    case kVirtualKeyLShift:
    case kVirtualKeyRShift:
    case kVirtualKeyControl:
    case kVirtualKeyLControl:
    case kVirtualKeyRControl:
    case kVirtualKeyMenu:
    case kVirtualKeyLMenu:
    case kVirtualKeyRMenu:
    case kVirtualKeyLWin:
    case kVirtualKeyRWin:
        return true;
    default:
        return false;
    }
}

bool is_valid_shortcut_key(std::uint32_t virtual_key) {
    if (virtual_key < kVirtualKeyBack || virtual_key > 0xFE) {
        return false;
    }
    if (virtual_key == kVirtualKeyRMenu) {
        return true;
    }
    return !is_shortcut_modifier_key(virtual_key) &&
        virtual_key != kVirtualKeyEscape &&
        virtual_key != kVirtualKeyTab;
}

bool is_valid_shortcut_binding(
    std::uint32_t virtual_key,
    std::uint32_t modifiers) {
    modifiers &= kShortcutModifierMask;
    if (!is_valid_shortcut_key(virtual_key) ||
        (modifiers & kShortcutModifierWindows) != 0) {
        return false;
    }
    if (virtual_key == kVirtualKeyRMenu) {
        return modifiers == 0;
    }
    if (virtual_key == kVirtualKeyDelete &&
        (modifiers & (kShortcutModifierControl | kShortcutModifierAlt)) ==
            (kShortcutModifierControl | kShortcutModifierAlt)) {
        return false;
    }
    return !is_typing_key(virtual_key) ||
        (modifiers & (kShortcutModifierControl | kShortcutModifierAlt)) != 0;
}

void ShortcutPressMatcher::set_binding(
    std::uint32_t virtual_key,
    std::uint32_t modifiers) {
    virtual_key_ = virtual_key;
    modifiers_ = modifiers & kShortcutModifierMask;
    // A binding can change while the key used to record it is still down. Do
    // not suppress that key's release: this matcher did not consume its press.
    shortcut_down_ = false;
}

void ShortcutPressMatcher::seed_modifiers(std::uint32_t modifiers) {
    seeded_modifiers_ = modifiers & kShortcutModifierMask;
}

ShortcutHookAction ShortcutPressMatcher::handle_event(
    std::uint32_t virtual_key,
    std::uint32_t event_modifier,
    bool is_right_alt,
    ShortcutKeyEvent event,
    bool injected,
    bool track_external_input) {
    ShortcutHookAction action;
    if (injected) {
        return action;
    }

    const bool down = event == ShortcutKeyEvent::Down;
    if (down) {
        update_modifier(virtual_key, event_modifier, true);
    }

    const bool same_key = virtual_key_ == kVirtualKeyRMenu
        ? is_right_alt
        : virtual_key == virtual_key_;
    const bool matches = same_key &&
        (virtual_key_ == kVirtualKeyRMenu ||
         active_modifiers() == modifiers_);
    const bool configured_modifier =
        (event_modifier & modifiers_) != 0;

    if (same_key && shortcut_down_) {
        action.consume = true;
        if (!down) {
            shortcut_down_ = false;
        }
    } else if (matches && down) {
        shortcut_down_ = true;
        action.consume = true;
        action.toggle = true;
    } else if (down && !configured_modifier && track_external_input) {
        action.external_input = true;
    }

    if (!down) {
        update_modifier(virtual_key, event_modifier, false);
    }
    return action;
}

std::uint32_t ShortcutPressMatcher::active_modifiers() const {
    return (seeded_modifiers_ | modifiers_from_keys()) &
        kShortcutModifierMask;
}

void ShortcutPressMatcher::update_modifier(
    std::uint32_t virtual_key,
    std::uint32_t modifier,
    bool down) {
    modifier &= kShortcutModifierMask;
    if (modifier == 0) {
        return;
    }
    // The first event for a seeded modifier supersedes the startup snapshot.
    seeded_modifiers_ &= ~modifier;
    if (virtual_key < modifier_keys_.size()) {
        modifier_keys_[virtual_key] = down ? modifier : 0;
    }
}

std::uint32_t ShortcutPressMatcher::modifiers_from_keys() const {
    std::uint32_t modifiers = 0;
    for (const std::uint32_t modifier : modifier_keys_) {
        modifiers |= modifier;
    }
    return modifiers;
}

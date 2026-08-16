#include "shortcut_binding.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {
constexpr std::uint32_t kKeyControl = 0xA2;
constexpr std::uint32_t kKeyRightAlt = 0xA5;
constexpr std::uint32_t kKeyShift = 0xA0;
constexpr std::uint32_t kKeyF8 = 0x77;
constexpr std::uint32_t kKeyF9 = 0x78;
constexpr std::uint32_t kKeyF10 = 0x79;

ShortcutHookAction down(
    ShortcutPressMatcher & matcher,
    std::uint32_t key,
    std::uint32_t modifier = 0,
    bool right_alt = false,
    bool injected = false,
    bool track_external = false) {
    return matcher.handle_event(
        key, modifier, right_alt, ShortcutKeyEvent::Down,
        injected, track_external);
}

ShortcutHookAction up(
    ShortcutPressMatcher & matcher,
    std::uint32_t key,
    std::uint32_t modifier = 0,
    bool right_alt = false,
    bool injected = false,
    bool track_external = false) {
    return matcher.handle_event(
        key, modifier, right_alt, ShortcutKeyEvent::Up,
        injected, track_external);
}

void test_custom_chord_toggles_once_and_consumes_its_pair() {
    ShortcutPressMatcher matcher;
    matcher.set_binding(kKeyF9, kShortcutModifierControl);

    const auto control_down = down(
        matcher, kKeyControl, kShortcutModifierControl);
    assert(!control_down.consume && !control_down.toggle);

    const auto first = down(matcher, kKeyF9);
    assert(first.consume && first.toggle);

    const auto repeat = down(matcher, kKeyF9);
    assert(repeat.consume && !repeat.toggle);

    const auto release = up(matcher, kKeyF9);
    assert(release.consume && !release.toggle);
    up(matcher, kKeyControl, kShortcutModifierControl);

    down(matcher, kKeyControl, kShortcutModifierControl);
    assert(down(matcher, kKeyF9).toggle);
    assert(up(matcher, kKeyF9).consume);
    up(matcher, kKeyControl, kShortcutModifierControl);
}

void test_custom_key_requires_the_recorded_modifiers() {
    ShortcutPressMatcher matcher;
    matcher.set_binding(
        kKeyF10,
        kShortcutModifierControl | kShortcutModifierShift);

    down(matcher, kKeyControl, kShortcutModifierControl);
    assert(!down(matcher, kKeyF10).toggle);
    up(matcher, kKeyF10);

    down(matcher, kKeyShift, kShortcutModifierShift);
    assert(down(matcher, kKeyF10).toggle);
    assert(up(matcher, kKeyF10).consume);
    up(matcher, kKeyShift, kShortcutModifierShift);
    up(matcher, kKeyControl, kShortcutModifierControl);
}

void test_capture_handoff_does_not_swallow_an_unmatched_release() {
    ShortcutPressMatcher matcher;
    matcher.set_binding(kKeyRightAlt, 0);

    // The F9 press reached the settings window and was recorded there. The
    // controller applies the new binding before that physical press is released.
    assert(!down(matcher, kKeyF9).consume);
    matcher.set_binding(kKeyF9, 0);
    const auto capture_release = up(matcher, kKeyF9);
    assert(!capture_release.consume);

    const auto rehearsal = down(matcher, kKeyF9);
    assert(rehearsal.consume && rehearsal.toggle);
    assert(up(matcher, kKeyF9).consume);

    // The same handoff must retain a held modifier while a custom typing-key
    // chord is saved, without turning the capture press into a partial hotkey.
    matcher.set_binding(kKeyRightAlt, 0);
    down(matcher, kKeyControl, kShortcutModifierControl);
    assert(!down(matcher, static_cast<std::uint32_t>('K')).consume);
    matcher.set_binding(
        static_cast<std::uint32_t>('K'), kShortcutModifierControl);
    assert(!up(matcher, static_cast<std::uint32_t>('K')).consume);
    up(matcher, kKeyControl, kShortcutModifierControl);

    down(matcher, kKeyControl, kShortcutModifierControl);
    assert(down(matcher, static_cast<std::uint32_t>('K')).toggle);
    assert(up(matcher, static_cast<std::uint32_t>('K')).consume);
    up(matcher, kKeyControl, kShortcutModifierControl);
}

void test_injected_shortcuts_are_ignored() {
    ShortcutPressMatcher matcher;
    matcher.set_binding(kKeyF8, 0);

    const auto injected_down = down(
        matcher, kKeyF8, 0, false, true);
    const auto injected_up = up(
        matcher, kKeyF8, 0, false, true);
    assert(!injected_down.consume && !injected_down.toggle);
    assert(!injected_up.consume && !injected_up.toggle);
}

void test_right_alt_keeps_altgr_compatibility() {
    ShortcutPressMatcher matcher;
    matcher.set_binding(kKeyRightAlt, 0);

    down(matcher, kKeyControl, kShortcutModifierControl);
    const auto press = down(
        matcher, kKeyRightAlt, kShortcutModifierAlt, true);
    assert(press.consume && press.toggle);
    assert(up(
        matcher, kKeyRightAlt, kShortcutModifierAlt, true).consume);
    up(matcher, kKeyControl, kShortcutModifierControl);
}

void test_external_input_distinguishes_chord_modifiers() {
    ShortcutPressMatcher matcher;
    matcher.set_binding(kKeyF9, kShortcutModifierControl);

    const auto modifier = down(
        matcher, kKeyControl, kShortcutModifierControl,
        false, false, true);
    assert(!modifier.external_input);

    const auto shortcut = down(
        matcher, kKeyF9, 0, false, false, true);
    assert(shortcut.toggle && !shortcut.external_input);
    up(matcher, kKeyF9);

    const auto typing = down(
        matcher, static_cast<std::uint32_t>('A'), 0,
        false, false, true);
    assert(typing.external_input);
}

void test_binding_validation_contract() {
    assert(is_valid_shortcut_binding(kKeyF9, 0));
    assert(is_valid_shortcut_binding(
        static_cast<std::uint32_t>('K'), kShortcutModifierControl));
    assert(!is_valid_shortcut_binding(static_cast<std::uint32_t>('K'), 0));
    assert(!is_valid_shortcut_binding(
        static_cast<std::uint32_t>('K'), kShortcutModifierWindows));
    assert(!is_valid_shortcut_binding(
        0x2E, kShortcutModifierControl | kShortcutModifierAlt));
}
}

int main() {
    test_custom_chord_toggles_once_and_consumes_its_pair();
    test_custom_key_requires_the_recorded_modifiers();
    test_capture_handoff_does_not_swallow_an_unmatched_release();
    test_injected_shortcuts_are_ignored();
    test_right_alt_keeps_altgr_compatibility();
    test_external_input_distinguishes_chord_modifiers();
    test_binding_validation_contract();
    std::cout << "shortcut capture-to-activation contract passed\n";
    return 0;
}

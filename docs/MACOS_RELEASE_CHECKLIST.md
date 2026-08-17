# SAID macOS DMG release checklist

The first macOS release is a focused one-shot dictation build. It does not
claim parity with the Windows setup, streaming, Adapt, or application-profile
features.

1. On an Apple Silicon Mac, run `./scripts/build-macos.sh` with no signing
   variables and verify that all five portable test executables pass and the
   universal DMG is produced.
2. Mount the DMG and verify that it contains `SAID.app` and the Applications
   shortcut. Drag the app to Applications before testing privacy permissions.
3. Verify the app and DMG with `codesign --verify --deep --strict --verbose=2`.
   For a public release, use a Developer ID Application certificate, notarize,
   staple, and validate the DMG. Do not publish the ad-hoc-signed build as a
   Gatekeeper-ready download.
4. Inspect `SAID.app/Contents/Resources/models`. It must contain exactly the
   four required speech files and must not contain Qwen3 or a partial download.
   Verify the hashes against `scripts/build-macos.sh`.
5. Confirm the bundle contains the project license, third-party notices, and
   model licenses under `Contents/Resources/licenses`.
6. Launch from Applications on a clean macOS 12-or-later user account. Confirm
   that SAID appears in the menu bar, not the Dock, and that its local speech
   model reaches the Ready state.
7. Grant Microphone permission, dictate Chinese, English, and mixed speech,
   and confirm Control–Option–Space starts and stops recording without taking
   focus from the target app.
8. Before Accessibility permission is granted, confirm the final transcript is
   copied but no synthetic paste is attempted. After granting permission,
   relaunch SAID and confirm text lands at the original caret in TextEdit,
   Safari, Mail, Terminal, and VS Code. Change to another app while SAID is
   transcribing and confirm it copies instead of pasting into the new app.
9. Exercise Exact and Clean output. Confirm Clean removes only deterministic,
   high-confidence speech artifacts and Exact preserves the recognizer text.
10. Confirm Chinese and English are always accepted. Test Japanese and Korean
    disabled, then enable each menu toggle separately and together; optional
    scripts must remain unmodified by Clean.
11. Deny Microphone permission and confirm SAID explains the failure and opens
    the correct Privacy & Security pane. Remove Accessibility permission and
    confirm clipboard fallback still preserves the transcript.
12. Test silence, a ten-minute recording limit, rapid repeated hotkey presses,
    quitting while recording, a conflicting global shortcut, and relaunching
    after preferences have been saved.
13. Test the universal binary on both Apple Silicon and Intel hardware, or on
    Apple Silicon plus a real Intel CI runner. Do not infer Intel runtime
    success only from `lipo -info`.
14. Generate `SHA256SUMS-macos.txt`, download the uploaded release artifacts,
    re-check the hash, and confirm the notarization ticket remains stapled.

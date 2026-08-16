# SAID Windows release checklist

1. Regenerate brand assets and inspect tray sizes on light/dark backgrounds.
2. Build Release x64 with `scripts/build-windows.ps1 -TestAdaptModel`.
   The normal build must not download or package an Adapt model.
3. Confirm speech-bundle hashes and Qwen3 0.6B Q8 hash
   `9465e63a22add5354d9bb4b99e90117043c7124007664907259bd16d043bb031`
   in both build and runtime download definitions.
4. Run `said_core_tests`, `said_speech_language_tests`, `said_shortcut_tests`,
   `said_refinement_tests`, and `said_refinement_performance_tests`. Run
   `scripts/test-preview-ui.ps1 -Executable <path-to-said.exe>` to verify
   accessible preview control states and registry isolation. Run
   microphone, file-transcription, Unicode injection/replacement, and
   real-model probes on native Windows.
5. The real-model suite must cover Clean fixtures; Chat, Mail, and Developer
   Adapt; Chinese content-loss fallback; dictated prompt injection; protected
   names/numbers/negation; long chunks; Exact bypass; and warm inference under
   three seconds. Record the benchmark corpus and pass rate before release.
6. Do not describe the generic 0.6B model as equivalent to a large writing
   model. Adapt leaves preview status until the documented human-preference and
   semantic-preservation quality gates pass; Clean remains the default.
7. Inspect Exact, Clean, known-profile Adapt, unknown/Shell fallback, live
   paused, final structure, safe fallback, success, silence, and error overlays
   at 100%, 150%, and 200%, in light, dark, High Contrast, and reduced motion.
8. Verify setup labels **Exact / Clean / Adapt**, the 639 MB and ~850 MB free
   disclosure, local-only copy, cancellation, failure, retry, and removal. The
   language row must show checked, fixed Chinese/English tags and unchecked,
   keyboard-focusable Japanese/Korean tags in a clean profile.
9. In tray Application behavior, verify automatic classification and overrides
   for Chat, Mail, Document, Developer prompt, Code editor, and Shell. Confirm
   Windows Terminal + tmux defaults to Shell and a Codex-specific override is
   scoped to that context key.
10. Upgrade legacy settings: Off → Exact, Standard/Advanced → Clean, and no
    migration silently enables or downloads Adapt. Preserve shortcut, startup,
    app overrides, and existing model access. Existing profiles must migrate to
    the Chinese/English language default; Japanese/Korean choices must persist.
11. With the published SenseVoice sample audio, confirm the default whitelist
    accepts Chinese/English and blocks Japanese/Korean. Enable each optional tag
    separately and together; confirm whole and streaming recognition, and verify
    Japanese/Korean bypass the Chinese/English punctuation and simplification
    passes.
12. Confirm **Type while I speak** is off in clean and upgraded profiles. With
    it on, dictate through pauses and continuous speech; stable phrases must
    appear once and recognition must continue while Adapt is slow.
13. Start in a field containing user text. Exercise raw, Clean, live Adapt, and
    final revisions. The existing prefix must remain unchanged and no revision
    may append a duplicate. Repeat with emoji, Chinese, and surrogate pairs.
14. While live, type, click, move the caret, change focus/control, and block
    SendInput. SAID must revoke ownership, leave all visible text untouched,
    finish to the clipboard, and never retry by appending.
15. Stop while live Adapt is running. Confirm pending live work is coalesced or
    cancelled, the final job wins, and an old session can never update the next
    dictation. Press the shortcut during final Adapt and confirm immediate Clean.
16. Cancel an Adapt download, exit during download, resume after restart, fail
    its SHA, and remove an installed model. A partial file must never activate;
    failure/removal returns effective behavior to Clean.
17. Inspect the portable ZIP and installer. They must contain the four-file
    speech bundle but not `Qwen3-0.6B-Q8_0.gguf`, research caches, a legacy
    executable, or a product-named wrapper directory.
18. Verify install/upgrade/uninstall entries and that uninstall leaves
    user-supplied models and unrelated files intact.
19. Verify Notepad, browser fields, Outlook/mail, VS Code, Windows Terminal,
    Codex under tmux, and at least one sandboxed app on native Windows 10/11.
20. Authenticode-sign `said.exe` and the installer with an RFC 3161 timestamp;
    verify signatures, submit to Defender, and publish SHA-256 sums.
21. Complete trademark and distribution-name clearance before public release.

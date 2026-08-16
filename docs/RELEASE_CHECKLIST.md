# SAID Windows release checklist

1. Regenerate brand assets with `python scripts/generate-brand-assets.py` and inspect `artifacts/said-tray-size-check.png` at 16, 20, 24, and 32 px on both backgrounds.
2. Build Release x64 with Visual Studio 2022 by running `scripts/build-windows.ps1`.
3. Confirm all four pinned model SHA-256 values in `scripts/build-windows.ps1`: SenseVoice `c71f0ce00bec95b07744e116345e33d8cbbe08cef896382cf907bf4b51a2cd51`, tokens `f449eb28dc567533d7fa59be34e2abca8784f771850c78a47fb731a31429a1dc`, punctuation `65a3fb9f5ad7bfb96bf69e0dc4481df97f6ee60513c1d94ce981ba6effd524b1`, and VAD `9e2449e1087496d8d4caba907f23e0bd3f78d91fa552479bb9c23ac09cbb1fd6`.
4. Run core tests plus the microphone, Unicode insertion, and file-transcription probes.
5. Verify insertion into Notepad and a browser field without the overlay taking focus. Change focus during transcription and confirm the transcript is copied instead.
6. Inspect all four setup pages and every overlay state at 100%, 150%, and 200% scaling in light, dark, Windows High Contrast, and reduced-motion modes. `scripts/capture-window.ps1 -Theme high-contrast` and `scripts/capture-overlay.ps1 -ReducedMotion` provide deterministic capture paths; still repeat the check with the real Windows accessibility settings before release.
7. Verify the tray icon at 16, 20, 24, and 32 px on light and dark taskbars. Every state must remain identifiable in grayscale.
8. Install with `SAID-Setup-0.2.0.exe`; verify the SAID Start Menu folder, optional startup and desktop entries, Apps & Features metadata, first-run setup, and known-file uninstall cleanup.
9. Upgrade from the currently released VoiceKey installer with custom shortcut/startup choices. Confirm the shortcut, startup preference, and existing model access survive; confirm the legacy startup entry and shortcuts are removed.
10. Confirm uninstall leaves user-supplied models and unrelated files intact.
11. Inspect `SAID-windows-x64-0.2.0.zip`; its root must contain `said.exe`, docs, and the four-file SenseVoice model bundle, with no legacy executable or product-named directory.
12. Authenticode-sign `said.exe` before packaging, then sign the completed installer with a trusted code-signing certificate and RFC 3161 timestamp.
13. Verify both signatures with `signtool verify /pa /all /v`, submit the installer to Microsoft Defender, and publish SHA-256 values beside both downloads.
14. Complete trademark, store-name, package-name, social-handle, and domain clearance before public release.

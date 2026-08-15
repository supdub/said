# Windows release checklist

1. Build Release x64 with Visual Studio 2022 and run `scripts/build-windows.ps1`.
2. Confirm the pinned model hash is `c577b9a86e7e048a0b7eada054f4dd79a56bbfa911fbdacf900ac5b567cbb7d9`.
3. Run the core tests plus the microphone, Unicode insertion, and transcription probes.
4. Inspect setup at 100%, 150%, and 200% scaling in both Windows app themes.
5. Install with `VoiceKey-Setup-0.2.0.exe`; verify Start Menu, optional startup/desktop entries, Apps & Features metadata, first-run setup, and uninstall cleanup.
6. Authenticode-sign `voicekey.exe` before packaging, then sign the completed installer with a trusted code-signing certificate and RFC 3161 timestamp.
7. Verify both signatures with `signtool verify /pa /all /v` and submit the installer to Microsoft Defender for a final malware scan.
8. Publish the installer SHA-256 beside the download. Keep the portable ZIP as the secondary artifact.

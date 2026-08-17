# SAID 0.4.0 for macOS

This is the first macOS build of SAID. It packages the existing local
SenseVoice, punctuation, VAD, and deterministic Clean pipeline in a native
menu-bar app for macOS 12 or later.

## Before opening

This preview is ad-hoc signed because the project does not yet have an Apple
Developer ID certificate. Verify the DMG against `SHA256SUMS-macos.txt`, drag
SAID to Applications, then Control-click SAID and choose **Open**. If macOS
still blocks it, choose **Open Anyway** under System Settings → Privacy &
Security. The release is deliberately marked as a GitHub prerelease until a
notarized build is available.

## Included

- Universal Apple Silicon and Intel application in a drag-to-Applications DMG.
- Control–Option–Space global start/stop shortcut.
- Local, one-shot Chinese/English dictation with optional Japanese/Korean.
- Exact and Clean output modes.
- Paste-at-caret through macOS Accessibility, with clipboard fallback.
- Verified model bundle, bundled licenses, ad-hoc local signing, and optional
  Developer ID signing and notarization.

## Not yet included on macOS

- Live phrase typing.
- Adapt and its optional Qwen model.
- Per-application profiles.
- The visual setup window, login startup setting, and shortcut recorder.

Microphone audio and recognized text stay on the Mac. The application makes no
cloud transcription request.

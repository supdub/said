# SAID 0.3.0

SAID 0.3.0 turns the local voice keyboard into a controlled dictation workflow:
choose Exact, Clean, or app-aware Adapt output, optionally type stable phrases
while speaking, and keep every rewrite local to the PC.

## Highlights

- **Exact** preserves recognizer text, **Clean** applies bundled deterministic
  speech repair, and **Adapt** uses an optional local Qwen3 0.6B model to shape
  the result for chat, mail, documents, developer prompts, editors, or shells.
- **Type while I speak** can insert settled phrases after natural pauses. Clean
  and Adapt revisions replace only SAID-owned text; focus or caret uncertainty
  safely moves the final result to the clipboard.
- Adapt is lazy-loaded, cancellable, removable, protected by content-preservation
  checks, and falls back to the complete Clean draft when a rewrite is unsafe.
- Setup & settings has a permanent four-section navigation rail with immediate
  saving, clearer state labels, accessible control names, and polished light,
  dark, High Contrast, reduced-motion, and 100–200% DPI layouts.
- Shortcut settings offer **Right Alt**, **F8**, and a safe custom recorder with
  rehearsal and validation for reserved or unsafe combinations.
- Recognition languages now appear as a whitelist in Dictation settings.
  Chinese and English stay enabled by default; Japanese and Korean can be
  switched on independently and persist across restarts.
- Disabled Japanese/Korean SenseVoice output is blocked before punctuation.
  Enabled Japanese/Korean text bypasses Chinese/English cleanup and Adapt so
  its original writing is preserved.
- The tray can override automatic application classification, including a
  conservative Shell fallback for terminal contexts.

## Quality and safety

- Native contract, shortcut, language, refinement, performance, microphone,
  transcription, Unicode replacement, preview-isolation, and real-model suites
  passed on Windows.
- The real-model gate covers Clean and application-aware Adapt output, bilingual
  content preservation, prompt injection, long input, fallback, and a warm
  inference budget under three seconds.
- Portable and installer contents are verified to contain the four-file speech
  bundle and exclude the optional Adapt model and research caches.

## Distribution

- `SAID-Setup-0.3.0.exe` — per-user Windows installer.
- `SAID-windows-x64-0.3.0.zip` — portable Windows x64 package.
- `SHA256SUMS.txt` — SHA-256 checksums for both artifacts.

The installer and executable are not Authenticode-signed. Windows may show a
SmartScreen warning; verify the SHA-256 checksum before running the package.

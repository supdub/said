# VoiceKey product brief

## The promise

**Talk. It types.** VoiceKey turns speech into text at the caret, locally, with one keyboard shortcut.

## Product structure

- **Installer:** Familiar Windows setup with a real icon, per-user install path, Start Menu entry, optional desktop icon, optional launch-at-login, uninstaller, and immediate first-run launch.
- **First run:** A four-step native window—welcome, microphone check, shortcut rehearsal, first dictation. It can be reopened from the tray as **Setup & settings**.
- **Everyday surface:** A non-activating 320 × 56 status strip near the bottom of the active display. It uses a voice-cursor mark, waveform, direct state label, and secondary shortcut/progress cue.
- **Tray:** Branded icon, exact current status, Setup & settings, model folder, and Quit.

## Visual system

| Role | Light | Dark |
|---|---|---|
| Canvas | `#F4F2ED` chalk | `#161716` carbon |
| Surface | `#FCFAF5` paper | `#222321` raised carbon |
| Text | `#20211E` ink | `#F2F0E9` chalk |
| Muted | `#686A63` graphite | `#A7A89F` ash |
| Border | `#D8D6CF` hairline | `#3A3B37` hairline |
| Accent | `#E24B32` vermilion | `#F06449` signal vermilion |
| Success | `#267A55` evergreen | `#54B98A` mint |
| Error | `#B93632` oxblood | `#F06B67` coral |

The accent is rare: primary actions, the active voice cursor, waveform, and focus rings. Segoe UI Variable is preferred where present, with Segoe UI fallback for full Windows 10 support and bilingual text coverage.

## Interaction rules

- Recording feedback begins on the shortcut press, before any heavy work.
- The waveform reflects actual microphone level; no decorative fake motion while listening.
- Transcribing uses a directional four-dot sequence that travels toward the caret mark.
- Buttons have default, hover, pressed, focus, disabled, loading, error, and success treatments where applicable.
- Escape closes setup only after first-run completion. The overlay never captures keyboard or pointer input.
- Windows High Contrast and reduced client animation settings take precedence over custom styling/motion.

## Copy inventory

- Ready: “Ready” / “Tap Right Alt to dictate”
- Listening: “Listening” / “Tap Right Alt to finish”
- Processing: “Turning speech into text” / “Usually about a second”
- Success: “Inserted” / “Ready for the next thought”
- Focus changed: “Transcript copied” / “Focus changed while transcribing”
- Silence: “No speech detected” / “Try again a little closer to the microphone”
- Missing model: “Speech model not found” / “Reinstall VoiceKey or choose the model folder”

## Acceptance criteria

- A user can install, confirm microphone activity, learn the shortcut, complete a first dictation, find settings again, and uninstall without documentation.
- Warm transcription latency on the reference Windows machine remains near one second for a short phrase and never regresses to the original scalar build.
- The executable, tray, shortcuts, Apps & Features entry, setup window, and installer all use the VoiceKey identity.
- Both light and dark themes remain legible at 100%, 150%, and 200% display scaling.

# SAID product brief

## The promise

**Say it once.** Your words land at the caret. On this PC.

SAID is a local Windows voice keyboard, not a writing assistant. It inserts the recognizer’s text without summarizing, rewriting, changing tone, or inventing cleaner prose.

## Product structure

- **Installer:** Per-user Windows setup with the SAID icon, Start Menu entry, optional desktop icon, optional launch-at-login, uninstaller, and immediate first-run launch.
- **First run:** A four-step native window—welcome, microphone check, shortcut rehearsal, and ready state. It reopens from the tray as **Setup & settings**.
- **Everyday surface:** A non-activating 380 × 72 status instrument near the bottom of the active display. It uses live microphone energy, a dots-to-caret transition, direct state labels, and a secondary shortcut or recovery cue.
- **Tray:** Monochrome icon, exact current status, Setup & settings, model folder, and Quit SAID.

## Visual system

The brand is strictly monochrome. Windows High Contrast and user-selected native chrome are accessibility exemptions.

| Role | Value | Use |
| --- | --- | --- |
| Ink | `#151613` | Dark canvas, light-theme text, light-theme primary actions |
| Raised Ink | `#242522` | Everyday overlay and dark elevated controls |
| Bone | `#F5F2E9` | Light canvas, dark-theme text, dark-theme primary actions |
| Muted Bone | `#C5C3BA` | Secondary text on Ink |
| Dark hairline | `#454641` | Borders on Ink |
| Light hairline | `#D1CEC5` | Borders on Bone |

Additional surfaces are mixtures of Ink and Bone. There is no success, warning, or error hue; explicit copy and distinct glyphs carry state.

The `SAID` wordmark is custom vector geometry. Native product UI uses Segoe UI Variable where available and Segoe UI as fallback for bilingual coverage.

## Interaction rules

- Recording feedback begins on the shortcut press, before any heavy work.
- Listening renders actual microphone-level history in Bone; it never uses a decorative fake waveform.
- Transcribing shows three dots resolving into the caret with short ease-out motion.
- Success holds a check and explicit label; error holds an exclamation and explicit recovery copy.
- The overlay never captures keyboard or pointer input.
- Interactive setup targets are at least 44 px high, keyboard order is logical, and focus has a separated monochrome outline.
- Windows High Contrast colors take precedence over the brand palette. Reduced motion switches states without spatial travel.

## Copy inventory

- Ready: “Ready” / “Tap Right Alt to dictate”
- Listening: “Listening” / “Tap Right Alt to finish”
- Processing: “Turning speech into text” / “Usually about a second”
- Success: “Inserted” / “Ready for the next thought”
- Focus changed: “Transcript copied” / “Focus changed while transcribing”
- Silence: “No speech detected” / “Try again a little closer to the microphone”
- Missing model: “Speech model not found” / “Reinstall SAID or open the model folder”

## Acceptance criteria

- A user can install, confirm microphone activity, learn the shortcut, dictate, find settings again, and uninstall without separate documentation.
- Warm transcription latency remains near one second for a short phrase on the reference machine and does not regress to a scalar build.
- The executable, tray, shortcuts, Apps & Features entry, setup window, portable archive, and installer all use the SAID identity.
- Listening, transcribing, success, and error are distinguishable in grayscale and without reading color.
- Light, dark, High Contrast, reduced-motion, and 100%, 150%, and 200% display scaling remain usable.
- An upgrade retains the existing shortcut, startup preference, and access to current model locations.

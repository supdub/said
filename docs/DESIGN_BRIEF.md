# SAID product brief

## The promise

**Say it once.** Your words land at the caret. On this PC.

SAID is a local Windows voice keyboard. It turns speech into usable text while
keeping the user in control of how much rewriting occurs. Stable phrases may
be processed while the user continues speaking; full-thought organization runs
only when the dictation has enough context.

## Product structure

- **Installer:** Per-user setup with the speech bundle and Clean output. Adapt
  is not bundled and requires a separate, explicit 639 MB local-model download.
- **Setup & settings:** A permanent four-item section rail jumps directly to
  Welcome, Microphone, Shortcut, or Dictation. First run still offers a guided
  Continue/Back path; later visits save changes immediately and close with Done.
  Shortcut offers Right Alt, F8, or a recorded custom key combination. The
  Dictation section presents Chinese and English as fixed selected language
  tags, with Japanese and Korean as independent opt-in tags.
- **Everyday surface:** A non-activating 380 × 72 status instrument near the
  bottom of the active display.
- **Tray:** Current status, live-typing toggle, Output submenu, Application
  behavior override, model lifecycle actions, Setup & settings, model folder,
  and Quit SAID.

## Output contract

| Mode | Behavior |
| --- | --- |
| **Exact** | Post-ASR text is unchanged. No cleanup or rewrite model runs. |
| **Clean** | Bundled deterministic repair removes high-confidence fillers, repeated starts, punctuation errors, and explicit spoken self-corrections without reorganizing ideas. |
| **Adapt** | Clean runs first. A separate local worker may organize the revisable tail for Chat, Mail, Document, Developer prompt, or Code editor; Shell and unknown destinations remain Clean. A final bounded pass may organize the complete dictation. |

Clean is the new-install default. Legacy Off migrates to Exact; legacy Standard
and Advanced migrate to Clean. Adapt is never enabled by migration.

## Interaction rules

- Settings sections never require sequential traversal. Each section is one
  click away from every other section.
- Recognition languages form an explicit whitelist. Chinese and English are
  always enabled for the primary mixed-language use case; Japanese and Korean
  save immediately and apply from the next dictation so an active session never
  changes interpretation midway through speech.
- Custom alphanumeric shortcuts require Ctrl or Alt so SAID cannot accidentally
  consume ordinary typing; Shift can be added. Windows-key shortcuts remain
  reserved for the operating system. The chosen virtual key and modifier mask
  persist together and are shown consistently in tray and overlay copy.
- Recording feedback starts on the shortcut press before heavy work.
- Live typing is off by default. VAD-finalized phrases enter a versioned speech
  session while the microphone remains open.
- Recognition repair and spoken cleanup run immediately on stable phrases.
  The newest two phrases or 320 Unicode code points remain revisable.
- Recognition never waits for Adapt. There is at most one running model job and
  one coalesced newest pending revision.
- Pre-existing field text is not model input and never belongs to SAID. A live
  revision replaces only the exact suffix SAID inserted.
- Keyboard or pointer input, focus/control change, injection failure, or
  uncertain caret state revokes ownership permanently for that dictation. The
  visible field is then left untouched and the final result is copied.
- During final Adapt, the configured shortcut cancels model work and keeps the
  complete Clean draft immediately.
- Unsafe output silently falls back during speech. Final fallback is named as
  **Kept the clean version**.

## Visual system

The brand is strictly monochrome. Ink `#151613`, Raised Ink `#242522`, Bone
`#F5F2E9`, Muted Bone `#C5C3BA`, and their mixtures carry the interface.
Windows High Contrast takes precedence. Segoe UI Variable falls back to Segoe
UI for Chinese/English coverage. Motion explains listening, recognition, or a
final composition wait; reduced motion changes state without spatial travel.

## Copy inventory

- Exact listening: **Listening** / **Tap Right Alt to finish**
- Clean live: **Listening · cleaning live** / **Tap Right Alt to finish**
- Known Adapt destination: **Listening · adapting** /
  **Developer prompt · Tap Right Alt to finish**
- Ownership lost: **Live typing paused** / **Tap Right Alt to finish and copy**
- Final Adapt: **Finishing structure** / **Right Alt keeps the clean draft**
- Continued wait: **Still adapting** / **Clean draft is safe · F8 keeps it**
- Safe fallback: **Kept the clean version**
- Focus changed: **Transcript copied** / **Focus changed while processing**
- Silence: **No speech detected** / **Try again a little closer to the microphone**

## Acceptance criteria

- Exact, Clean, and Adapt are understandable without model terminology.
- Stable phrases start Clean/Adapt work before recording ends; final document
  structure waits for the complete thought.
- Stale model results never overwrite newer recognition revisions.
- Existing textbox content survives three or more live revisions byte-for-byte,
  including Unicode and a pre-existing prefix.
- App detection is overridable per executable/context; Windows Terminal under
  tmux is Shell unless the user explicitly maps that context to Developer
  prompt.
- Adapt download is cancellable/resumable, hash-verified, atomically activated,
  removable, and never included in the installer.
- Missing, cancelled, slow, unsafe, or low-quality Adapt output preserves the
  complete Clean draft.
- Every settings section is directly reachable, and custom shortcuts can be
  recorded, rehearsed, persisted, and used by the global keyboard listener.
- Language tags communicate selected state with both a checkmark and contrast;
  default Chinese/English and optional Japanese/Korean behavior persists across
  restart and applies equally to whole-utterance and streaming recognition.
- Warm Adapt phrase work targets P95 under 1.5 seconds; rule cleanup targets P95
  under 20 ms per phrase. Recognition remains independently responsive.

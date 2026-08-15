# SAID Rebrand Specification

Status: approved design direction for future implementation
Last updated: 2026-08-15
Replaces: the user-facing **VoiceKey** identity and the vermilion brand palette

This document is the source of truth for the rebrand. If it conflicts with
`.impeccable.md`, `docs/DESIGN_BRIEF.md`, existing screenshots, or current brand
assets, this document wins for naming and visual identity.

## Decision

- Product name: **SAID**
- Name presentation: uppercase `SAID` in all branded and user-facing surfaces
- Core palette: strictly monochrome — near-black and bone white
- Signature visual: three speech dots resolving into a text caret
- Core promise: **Say it once.**
- Product descriptor: **Local voice. Exact text.**
- Product behavior does not change as part of the rebrand

The visual direction board is available at:

- `artifacts/rebrand-direction-board.png`
- `artifacts/rebrand-direction-board.svg`

Use the board for composition, proportions, and the dots-to-caret language. The
signal yellow shown in that early exploration is deprecated. Replace it with
bone white or near-black according to the surface.

## Product Positioning

SAID is a lightweight Windows 10/11 voice keyboard for bilingual writers,
developers, and keyboard-first users. One shortcut starts listening; the same
shortcut finishes. The recognized words are inserted at the caret.

SAID is deliberately:

- Local and private
- Fast to invoke
- Faithful to what the user said
- Available in any ordinary text field
- Quiet when it is not in use

SAID is not positioned as an AI writing assistant. It does not summarize,
rewrite, change tone, or invent cleaner prose. Do not market model intelligence;
market immediacy, privacy, and exactness.

## Brand Personality

Three words: **terse, tactile, exact**.

The product should feel like a physical black key or an ink mark:

- Immediate when pressed
- Calm while listening
- Definite when finished
- Precise without feeling clinical
- Premium without decorative luxury cues

Copy should be brief and literal. Avoid jokes, hype, AI vocabulary, and vague
claims such as “revolutionary,” “magical,” or “intelligent.”

## Naming Rules

Use:

- `SAID` for the wordmark, window titles, installer, Start Menu, tray, and docs
- `said.exe` for the shipping executable
- `SAID-Setup-<version>.exe` for the installer
- `SAID-windows-x64-<version>.zip` for the portable archive
- `SAID — Setup & settings` for the setup window title

Do not use:

- Said AI
- Said Voice
- SAID Voice Keyboard as the primary name
- VoiceKey by SAID
- Any `-less` construction intended to imitate Typeless

Pronunciation is the ordinary English word “said” (`/sɛd/`).

`SAID` is the approved working product name, but trademark, store-name, package-
name, social-handle, and domain clearance must be completed before a public
release. A web search is not legal clearance.

## Verbal Identity

Primary lockup:

> SAID
> Local voice. Exact text.

Primary headline:

> Say it once.

Supporting copy:

> Your words land at the caret. On this PC.

Installer welcome:

> SAID, ready when you are
> Say it once. Your words land wherever the caret already is. Recognition stays
> on this computer. Setup takes about a minute.

Preferred state labels remain functional rather than branded:

- Ready
- Listening
- Turning speech into text
- Inserted
- Transcript copied
- No speech detected
- Speech model not found

Replace product references in recovery copy, for example:

- “Open SAID from the tray for details”
- “Reinstall SAID or choose the model folder”
- “Launch SAID when I sign in”
- “Quit SAID”

## Color System

“Strictly black and white” means no chromatic brand accent. Do not use yellow,
vermilion, red, green, blue, purple, gradients, or colored glow in branded UI.
State must never depend on color.

The palette uses tinted near-black and bone rather than pure `#000000` and
`#ffffff`.

| Token | OKLCH source | sRGB fallback | Use |
|---|---|---|---|
| Ink | `oklch(0.145 0.008 100)` | `#151613` | Primary dark surface and dark text |
| Raised ink | `oklch(0.205 0.009 100)` | `#242522` | Elevated dark surface |
| Bone | `oklch(0.955 0.017 100)` | `#F5F2E9` | Primary light surface and light text |
| Muted bone | `oklch(0.815 0.014 100)` | `#C5C3BA` | Secondary text on dark surfaces |
| Dark hairline | `oklch(0.320 0.009 100)` | `#454641` | Borders on dark surfaces |
| Light hairline | `oklch(0.820 0.014 100)` | `#D1CEC5` | Borders on light surfaces |

Additional neutral states should be derived by mixing Ink and Bone. Do not add a
third hue.

Native Windows chrome, user-selected system accent colors, and Windows High
Contrast colors are accessibility exemptions. Do not override them merely to
keep a screenshot monochrome.

## Logo and App Icon

The signature mark is **speech becoming a caret**:

```text
•  •  •  |
```

Construction:

- Three equal circular dots move left-to-right toward one vertical caret
- The caret is taller and slightly heavier than a dot diameter
- Spacing tightens subtly as the dots approach the caret
- The mark must remain recognizable at 16 × 16 pixels
- Static icon: bone mark on an Ink tile
- Inverse mark: Ink mark on Bone
- Tray icon: use a simplified monochrome silhouette suitable for Windows
- Wordmark: `SAID` followed by a caret-like terminal stroke when space permits

The mark may animate in the overlay: dots appear or travel toward the caret, and
the caret may blink. Motion and opacity replace the deprecated signal color.

Do not use:

- A microphone glyph
- A generic waveform inside a colored rounded square
- Speech bubbles
- Keyboard-key clip art
- Gradients, glows, glass effects, or dimensional chrome
- A colored tip on the caret

The app tile may retain the platform-appropriate rounded-square silhouette, but
the dots-to-caret mark must do the identifying work.

## Typography

- The `SAID` wordmark should be custom-drawn or converted to vector outlines; it
  must not depend on a font being installed on the user’s machine.
- The wordmark should feel narrow, firm, and slightly mechanical without using a
  monospace typeface.
- Native product UI should retain Segoe UI Variable where available and Segoe UI
  as fallback for fast rendering and reliable Chinese/English coverage.
- Use typography, spacing, weight, fill, and outline—not color—to establish
  hierarchy and state.
- Do not use gradient text, novelty display fonts, or an all-monospace interface.

## Interface Direction

### Setup and settings

- Default light surface: Bone canvas, Ink text, Ink primary action
- Dark surface: Ink canvas, Bone text, Bone primary action
- Keep the composition asymmetric and editorial, with generous empty space
- Reduce soft, friendly rounding; prefer firm 8–12 px control radii
- Progress uses one filled dot and outlined or lower-contrast remaining dots
- Do not introduce decorative cards around every content group
- Interactive targets remain at least 44 px high
- Focus is shown with a clearly separated monochrome outline

### Everyday overlay

The overlay is SAID’s signature product surface.

- Use an Ink or Raised Ink body in both Windows light and dark themes
- Use Bone primary text and Muted Bone secondary text
- Keep it compact, non-activating, and near the bottom of the active display
- Listening uses a real microphone-level waveform rendered in Bone
- Transcribing uses the three-dots-to-caret motion in Bone
- Success uses a check shape plus explicit text
- Error uses an exclamation shape plus explicit text
- Never communicate success, warning, or error with color alone
- Preserve reduced-motion behavior and Windows High Contrast support

### Installer and tray

- Installer artwork should be predominantly Ink with a Bone mark
- Avoid a large repeated logo plus decorative echo of the same logo
- Use the dots-to-caret mark once, confidently
- The tray icon must be tested at 16, 20, 24, and 32 px on light and dark taskbars
- Installer, executable metadata, uninstall entry, shortcuts, and tray copy must
  all say `SAID`

## Motion

Motion communicates transformation, never decoration.

- Listening: live waveform follows actual microphone energy
- Transcribing: three dots resolve into the caret
- Finished: movement stops immediately and the final glyph holds briefly
- Use short ease-out state changes; no bounce or elastic easing
- Animate transform and opacity rather than layout dimensions
- With reduced motion enabled, switch glyphs without spatial travel

## Accessibility

Monochrome design increases the importance of form and copy:

- Every state has a distinct glyph and explicit text label
- Focus indicators must meet contrast requirements on both Ink and Bone
- Muted text must remain readable; do not lower opacity until it becomes gray haze
- Preserve keyboard navigation and logical tab order
- Preserve Windows High Contrast colors even when they break the brand palette
- Verify at 100%, 150%, and 200% display scaling

## Engineering Scope

The rebrand must cover user-visible identity, packaged assets, build artifacts,
and durable identifiers. Search case-insensitively for `VoiceKey` and `voicekey`
before considering the work complete.

Primary touchpoints currently include:

- `CMakeLists.txt`
- `README.md`
- `.impeccable.md`
- `docs/DESIGN_BRIEF.md`
- `docs/RELEASE_CHECKLIST.md`
- `installer/VoiceKey.nsi`
- `assets/voicekey-mark.svg`
- `scripts/generate-brand-assets.py`
- `resources/voicekey.rc`
- `resources/app.manifest`
- `src/app_settings.cpp`
- `src/main.cpp`
- `src/overlay.cpp`
- `src/setup_window.cpp`
- `src/ui_theme.h`
- `src/win_util.cpp`
- Windows capture and smoke-test scripts
- Integration probe and transcript tests

Expected new brand assets:

- `assets/said-mark.svg`
- `assets/said.ico`
- `assets/said-64.png`
- `assets/said-256.png`
- Updated monochrome installer bitmaps

Target packaging names:

- `said.exe`
- `SAID-Setup-<version>.exe`
- `SAID-windows-x64-<version>.zip`
- `%LOCALAPPDATA%\Programs\SAID`
- `%LOCALAPPDATA%\SAID\models\...`
- `HKCU\Software\SAID`
- Start Menu folder `SAID`
- Startup value `SAID`

Internal C++ class names do not need to be renamed solely for aesthetics, but
window class names, single-instance mutexes, registry keys, resource metadata,
environment variables, capture scripts, and test expectations must be reviewed
because they can affect behavior and automation.

## Compatibility and Migration

Do not strand existing VoiceKey installations or settings.

For at least one transition release:

- Read new SAID settings first, then fall back to the legacy
  `HKCU\Software\VoiceKey` key
- Read `SAID_MODEL` first, then fall back to `VOICEKEY_MODEL`
- Search the new SAID model directory first, then the legacy VoiceKey model path
- Detect and remove or replace legacy VoiceKey startup entries during upgrade
- Avoid running VoiceKey and SAID simultaneously through compatible
  single-instance detection
- Ensure uninstall does not delete a user-supplied model or unrelated files

The migration should be tested from the currently released VoiceKey installer,
not only from a clean machine.

## Implementation Order

1. Update `.impeccable.md` and `docs/DESIGN_BRIEF.md` to match this specification.
2. Create the final vector wordmark, icon, tray silhouette, and installer assets.
3. Replace the palette and state treatments while preserving High Contrast.
4. Update setup, overlay, tray, installer, resources, and all user-facing copy.
5. Rename packaging, executable metadata, filesystem paths, and durable IDs.
6. Add migration fallbacks for settings, model location, startup, and instances.
7. Update scripts, tests, README, release checklist, and screenshots.
8. Build the Windows installer and portable archive.
9. Run unit, integration, smoke, scaling, light/dark taskbar, and upgrade tests.
10. Perform trademark/domain clearance before public release.

## Acceptance Criteria

The rebrand is complete only when:

- No user-facing surface says VoiceKey
- No vermilion or signal yellow remains in branded assets or UI
- The app icon and tray icon are legible at their smallest Windows sizes
- Setup, overlay, installer, and tray visibly belong to one monochrome system
- Listening, transcribing, success, and error remain distinguishable without color
- Existing users retain their shortcut, startup preference, and model access
- The app still inserts text without stealing focus
- Light, dark, High Contrast, reduced-motion, and 100–200% scaling checks pass
- Installer and portable artifacts use the SAID name consistently
- Documentation accurately describes local, faithful transcription without AI-
  writing claims

## Explicit Non-Goals

Do not use the rebrand as permission to add:

- Cloud transcription
- LLM rewriting or tone controls
- Accounts, subscriptions, or telemetry
- A transcription history window
- A permanent dashboard
- New shortcut behavior
- Decorative animation unrelated to current state

The rebrand should make the existing focused product memorable without turning it
into a different product.

<p align="center">
  <img src="assets/said-256.png" width="104" alt="SAID app icon">
</p>

<h1 align="center">SAID</h1>

<p align="center">
  <strong>Say it once.</strong><br>
  Local voice. Your text. Your words land at the caret—on this PC.
</p>

<p align="center">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-151613?style=flat-square&logo=windows11&logoColor=F5F2E9">
  <img alt="Local processing" src="https://img.shields.io/badge/processing-local-151613?style=flat-square">
  <img alt="English and Chinese" src="https://img.shields.io/badge/speech-English%20%2B%20%E4%B8%AD%E6%96%87-151613?style=flat-square">
  <a href="LICENSE"><img alt="MIT license" src="https://img.shields.io/badge/license-MIT-151613?style=flat-square"></a>
</p>

<p align="center">
  <a href="https://github.com/supdub/said/releases"><img src="assets/readme-download-windows.svg" width="344" alt="Get SAID for Windows 10 or 11"></a>
</p>

> [!WARNING]
> The `v0.3.0` installer and portable build are not Authenticode-signed, so
> Microsoft Defender SmartScreen may show an **unrecognized app** warning.
> Download SAID only from this repository's release page and verify the files
> against the attached `SHA256SUMS.txt` before running them.

SAID is a lightweight Windows 10/11 voice keyboard. Tap your shortcut once,
speak in Chinese, English, or both, and tap it again. Your words land at the
caret without leaving this PC. Japanese and Korean recognition can be enabled
from settings without downloading another model.

Right Alt is the default shortcut; F8 is available for AltGr keyboard layouts, and the settings window can record a custom key or modifier combination. Recognition runs locally with the multilingual SenseVoice Small model through `sherpa-onnx`. Its recognition-language whitelist always includes Chinese and English; Japanese and Korean can be added as independent tags in **Setup & settings**. A separate Chinese/English punctuation pass improves Chinese/English readability, while enabled Japanese/Korean output bypasses that pass and Chinese simplification so its writing is preserved.

Output has three local modes. **Exact** keeps the allowed-language recognizer transcript unchanged. **Clean** is the recommended default and uses bundled English/Chinese rules to fix high-confidence speech mistakes, fillers, punctuation, repetition, and explicit self-corrections—no language model or extra download. **Adapt** applies Clean, then uses an optional Qwen3 0.6B model through `llama.cpp` to organize the thought for the current application. SAID discloses the exact 639 MB download and about 850 MB required free space before downloading it. Adapt works offline after installation and can be removed from the tray menu. If the model is absent or its result fails preservation checks, SAID keeps the complete Clean text. Neither audio nor text is sent to a cloud service.

**Type while I speak** is available from **Setup & settings** and the tray menu and is off by default for every installation. When enabled, SAID uses its existing local VAD and SenseVoice model to settle short phrases after a natural pause. Exact types each stable phrase as recognized. Clean repairs and replaces only SAID's current revisable tail while speech continues. Adapt runs on a separate coalescing worker and may revise that same tail; full-document organization waits until dictation ends. Streaming adds no second speech recognizer or cloud service.

## User experience

1. Put the caret in any editable field.
2. Tap your SAID shortcut. A compact status instrument appears without taking focus.
3. Speak naturally. The waveform follows the live microphone level. With streaming enabled, finalized phrases appear at the caret as you pause.
4. Tap the shortcut again. SAID settles the last phrase. Adapt may run one bounded whole-thought pass; while it runs, tap the shortcut once more to keep the complete Clean draft immediately.

On first launch, SAID runs a short microphone check, lets you choose **Right Alt**, **F8**, or a recorded custom shortcut, lets you rehearse it, and presents **Type while I speak**, the Chinese/English language whitelist with optional Japanese/Korean tags, **Exact / Clean / Adapt**, and startup choices before the first dictation. Double-click the SAID tray icon at any time to reopen **Setup & settings**. Its permanent section navigation jumps directly to Welcome, Microphone, Shortcut, or Dictation without stepping through the other pages; changes save automatically. Language changes take effect on the next dictation. The tray menu exposes the same output choices, Adapt download cancellation/removal, and per-application behavior overrides.

If focus changes before processing finishes, SAID copies the result instead of typing into the wrong app. During live typing, any manual typing, pointer click, focus change, injection failure, or uncertain caret state permanently revokes ownership for that dictation; already visible text is left untouched and the final result is copied. Text that was already in the field is never sent to the rewrite model and never becomes part of SAID's replaceable range. Clean is immediate and model-free. The optional Adapt model loads lazily and stays warm. Preservation checks reject prompt scaffolding, answers to dictated instructions, information loss, changed names/numbers/negation, and missing technical tokens; rejected output falls back to Clean.

## Install on Windows

Run `SAID-Setup-0.3.0.exe`. The per-user installer does not need administrator access. It installs SAID, the local speech bundle, and Clean, adds Start Menu and uninstall entries, optionally creates a desktop shortcut, and can keep SAID ready after sign-in. The optional 639 MB Adapt model is not part of the installer and is downloaded only after explicit confirmation.

The portable `SAID-windows-x64-0.3.0.zip` is available for users who do not want an installed app.

## Build on Windows

Prerequisites: Visual Studio 2022 Build Tools with Desktop C++, CMake 3.21+, Git, PowerShell 5+, and NSIS for the installer.

```powershell
Set-ExecutionPolicy -Scope Process Bypass
./scripts/build-windows.ps1
```

The script builds `said.exe`, runs the contract, incremental-refinement, and performance tests, downloads and verifies the four-file speech bundle, creates the portable ZIP, and builds the monochrome NSIS installer. Pass `-TestAdaptModel` (the old `-TestAdvancedGrammar` name remains an alias) to additionally download the pinned Qwen3 model and run Clean, application-aware Adapt, bilingual content-loss, prompt-preservation, and warm-latency probes; that model remains in the build cache and is never copied into release packages. Pass `-SkipInstaller` only when NSIS is unavailable.

Run the unpacked app with:

```powershell
./dist/SAID/said.exe
```

SAID is a tray app. Right-click its tray icon for status, Setup & settings, the model folder, or Quit SAID. It does not require administrator privileges; Windows prevents normal apps from injecting text into elevated windows.

## Manual CMake build

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

For an offline source build with an existing `sherpa-onnx` checkout:

```powershell
cmake -S . -B build -A x64 `
  -DSAID_SHERPA_ONNX_SOURCE_DIR=C:/src/sherpa-onnx
```

The model is searched in this order:

1. `--model C:\path\to\sense-voice-small.int8.onnx`
2. `%SAID_MODEL%`
3. The legacy `%VOICEKEY_MODEL%` override for transition installs
4. `models\sense-voice-small.int8.onnx` beside `said.exe`
5. `%LOCALAPPDATA%\SAID\models\sense-voice-small.int8.onnx`
6. Legacy local and installed model folders

The selected model directory must also contain `sense-voice-small.tokens.txt`, `ct-transformer-punctuation.int8.onnx`, and `silero-vad.onnx`. SAID rejects partial bundles at startup instead of failing during dictation.

The optional Adapt model is searched through the legacy-compatible `--grammar-model` and `%SAID_GRAMMAR_MODEL%` overrides, beside the selected speech model, beside `said.exe`, and in `%LOCALAPPDATA%\SAID\models`. Its filename is `Qwen3-0.6B-Q8_0.gguf`. SAID downloads it with Windows Background Intelligent Transfer Service, verifies SHA-256 `9465e63a22add5354d9bb4b99e90117043c7124007664907259bd16d043bb031`, then activates it atomically. Downloads can be cancelled and resume after an app or Windows restart. A missing or rejected Adapt result never blocks dictation: SAID keeps Clean.

The Visual Studio build remains the supported release build. Public artifacts should be Authenticode-signed; unsigned local builds can trigger Microsoft Defender SmartScreen reputation warnings.

## Existing installations

The transition release reads current SAID settings first and then falls back to the pre-rebrand registry key, model environment override, and model folders. It replaces the old startup entry with `SAID`, and the compatible single-instance guard prevents the old and new executables from running together. Old Whisper `.bin` files are not compatible with the new recognizer and are left untouched. The uninstaller deliberately leaves user model files in `%LOCALAPPDATA%\SAID\models`.

## Hardware requirements and benchmark

SAID runs recognition entirely on the CPU; it does not require a dedicated GPU
or NPU. These recommendations include enough headroom to keep an editor, browser,
and normal development tools open while dictating.

| | Practical minimum | Recommended for coding |
| --- | --- | --- |
| Operating system | Windows 10 or 11, 64-bit | Windows 11, 64-bit |
| Processor | 4-core x64 CPU | 6 or more modern CPU cores / 12 or more logical processors |
| Memory | 8 GB RAM | 16 GB RAM; 32 GB for containers, virtual machines, or large local builds |
| Storage | SSD with 2 GB free during setup | SSD with 3 GB free for SAID, updates, and the development toolchain |
| Graphics | Integrated graphics are sufficient | No dedicated GPU is needed |

The base installation occupies about 360 MB. Adapt adds a 639 MB download and
requires about 850 MB free for download verification and atomic activation.
Before Adapt is first used, the reference run used about 564 MB of memory while
idle and reached a 605 MB high-water mark during startup. Qwen3 is lazy-loaded
only when Adapt runs; Exact and Clean never load it.

Once Adapt is warm, its local rewrite engine uses about 1.10 GiB of resident
memory: approximately 612 MiB of model-backed pages and 515 MiB of private
working memory. Combined with the speech stack, expect SAID to use about 1.65
GiB while Adapt remains warm. Active Adapt inference averaged 5.6–6.7 logical
CPU cores on the 32-thread reference machine, then returned to an inactive
worker between requests. **16 GB RAM is recommended** for normal Adapt use; use
32 GB when running containers, virtual machines, or large local builds beside
SAID. See the [Adapt resource profile](docs/ADAPT_RESOURCE_PROFILE.md) for the
measurement method, environment, and update procedure.

### Reference performance

The current Release x64 build was measured on an Intel Core i9-14900HX with 32
logical processors available to the benchmark, 16 GB RAM, and SSD storage. Each
result is the median of three new-process runs against the same 30.42-second
bilingual recording; the model and file data were already in the operating-system
cache.

| Recognition threads | Model load | Recognition | Speed vs. recording | Peak memory |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 1.34 s | 2.18 s | 14x real time | 601 MB |
| 2 | 1.24 s | 1.29 s | 24x real time | 602 MB |
| 4 | 1.26 s | 0.86 s | 35x real time | 609 MB |
| 8 (automatic on this CPU) | 1.29 s | 0.95 s | 32x real time | 610 MB |

For shorter samples, 5.59 seconds of Mandarin took 0.17 seconds and 7.15 seconds
of English took 0.19 seconds after model loading. SAID keeps the model loaded, so
the model-load cost normally occurs once when the app starts rather than after
every dictation.

The automated development environment cannot provide native Windows timing, so
these results are directional measurements of the Windows x64 executable under
a compatibility layer, not a hardware certification. The practical minimum is
deliberately conservative; native Windows results may vary with CPU generation,
power mode, microphone driver, and other work running at the same time.

### Runtime profile

- Native Win32 UI and global keyboard hook; no Electron or browser runtime.
- The CPU-only SenseVoice, punctuation, and VAD models remain loaded after startup. Clean has no model. Qwen3 loads only for Adapt and remains warm afterward.
- Live typing uses VAD to decode each completed phrase once. Recognition never waits for Adapt; pending model work is coalesced to the newest revision, and only the most recent two phrases or 320 Unicode code points remain mutable.
- Clean uses deterministic high-confidence rules. Adapt uses deterministic decoding plus fail-closed checks for technical tokens, names, numbers, negation, CJK content coverage, shell syntax, and model scaffolding. Neither mode guarantees perfect prose; rejected or ambiguous changes keep Clean.
- SenseVoice uses non-autoregressive decoding with automatic language selection. SAID keeps Chinese/English enabled for code switching, rejects a phrase positively tagged as disabled Japanese/Korean, and removes residual disabled Japanese/Korean script before punctuation.
- Dictations up to 25 seconds are recognized intact. Longer audio is split at speech boundaries into segments no longer than 20 seconds.
- Inference uses up to half the logical processors, capped at eight threads.
- Audio capture is capped at ten minutes per dictation.

## Current limitations

- Some elevated or unusually sandboxed applications can reject Windows Unicode input; SAID copies the transcript in that case.
- Users with AltGr keyboard layouts should select F8 during setup so Right Alt does not intercept AltGr characters.
- Very short English insertions inside Mandarin—especially uncommon names and one-syllable words—remain the hardest recognition case.
- SenseVoice accepts one language prompt rather than a native multi-language set, so the whitelist is a fail-safe output boundary around automatic recognition, not decoder retraining. Chinese and Japanese share Han characters; the detected SenseVoice language tag provides the phrase-level distinction.
- The generic Qwen3 0.6B candidate meets the size and warm-latency target but is conservative and is not equivalent to a larger or task-distilled writing model; Chinese whole-thought rewrites commonly fall back to Clean. Use Clean for predictable correction or Exact for quotations, legal text, code, shell input, and any task that requires post-ASR wording unchanged.
- SenseVoice is an offline recognizer, so streaming updates arrive phrase by phrase after a natural pause or six-second speech boundary rather than as unstable token-by-token hypotheses.

See [Mixed Chinese/English ASR investigation](docs/MIXED_LANGUAGE_ASR.md) for the benchmark and design rationale.

<p align="center">
  <img src="assets/said-256.png" width="104" alt="SAID app icon">
</p>

<h1 align="center">SAID</h1>

<p align="center">
  <strong>Say it once.</strong><br>
  Local voice. Exact text. Your words land at the caret—on this PC.
</p>

<p align="center">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-151613?style=flat-square&logo=windows11&logoColor=F5F2E9">
  <img alt="Local processing" src="https://img.shields.io/badge/processing-local-151613?style=flat-square">
  <img alt="English and Chinese" src="https://img.shields.io/badge/speech-English%20%2B%20%E4%B8%AD%E6%96%87-151613?style=flat-square">
  <a href="LICENSE"><img alt="MIT license" src="https://img.shields.io/badge/license-MIT-151613?style=flat-square"></a>
</p>

<p align="center">
  <a href="https://github.com/supdub/said/releases"><img src="assets/readme-download-windows.svg" width="344" alt="Get SAID for Windows 10 or 11"></a>
  <a href="#linux-support"><img src="assets/readme-linux-status.svg" width="344" alt="Linux native installer not available; read the support status"></a>
</p>

> [!NOTE]
> This repository does not have a packaged release yet. The Windows button opens
> the release page, where the signed `.exe` installer will appear when the first
> release is published. SAID is currently a native Windows app; see
> [Linux support](#linux-support) before trying to build it on Linux.

SAID is a lightweight Windows 10/11 voice keyboard. Tap your shortcut once,
speak in Chinese, English, or both, and tap it again. Your words land at the
caret without leaving this PC.

Right Alt is the default shortcut; F8 is available for AltGr keyboard layouts. Recognition runs locally with the multilingual SenseVoice Small model through `sherpa-onnx`. A separate Chinese/English punctuation pass improves readability without rewriting embedded English, and Chinese output is normalized to simplified characters. SAID does not summarize, translate, remove filler words, change tone, or send audio to a cloud service.

## User experience

1. Put the caret in any editable field.
2. Tap your SAID shortcut. A compact status instrument appears without taking focus.
3. Speak naturally. The waveform follows the live microphone level.
4. Tap the shortcut again. The dots resolve into a caret while SAID turns speech into text, then the text is inserted at the original caret.

On first launch, SAID runs a short microphone check, lets you choose **Right Alt** or **F8**, and lets you rehearse the shortcut. Double-click the SAID tray icon at any time to reopen **Setup & settings**.

If focus changes before recognition finishes, SAID copies the transcript instead of typing into the wrong app. The microphone is open only between the two shortcut presses. The model stays loaded so later dictations begin quickly.

## Install on Windows

Run `SAID-Setup-0.2.0.exe`. The per-user installer does not need administrator access. It installs SAID and its local speech model, adds Start Menu and uninstall entries, optionally creates a desktop shortcut, and can keep SAID ready after sign-in.

The portable `SAID-windows-x64-0.2.0.zip` is available for users who do not want an installed app.

### Linux support

There is no Linux installer or native desktop application today. SAID relies on
Win32 UI, a Windows global keyboard hook, Windows audio capture, and Windows text
injection. Linux can run the platform-independent transcript tests and
cross-compile the Windows application with Zig, but that output is still a
Windows executable.

## Build on Windows

Prerequisites: Visual Studio 2022 Build Tools with Desktop C++, CMake 3.21+, Git, PowerShell 5+, and NSIS for the installer.

```powershell
Set-ExecutionPolicy -Scope Process Bypass
./scripts/build-windows.ps1
```

The script builds `said.exe`, runs tests, downloads and verifies the pinned speech, token, punctuation, and voice-activity models, creates the portable ZIP, and builds the monochrome NSIS installer. The first build downloads about 315 MB of model data and caches it under the build directory. Pass `-SkipInstaller` only when NSIS is unavailable.

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

### Cross-build from Linux with Zig

The checked-in Zig toolchain file is used for CI and build verification. Install `x86_64-w64-mingw32-windres` as well so the icon, manifest, and version resources are embedded:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-x64.cmake \
  -DZIG_EXECUTABLE=/path/to/zig \
  -DSAID_SHERPA_ONNX_SOURCE_DIR=/path/to/sherpa-onnx \
  -DSAID_BUILD_TESTS=OFF
cmake --build build-windows --parallel
```

The Visual Studio build remains the supported release build. Public artifacts should be Authenticode-signed; unsigned local builds can trigger Microsoft Defender SmartScreen reputation warnings.

## Existing installations

The transition release reads current SAID settings first and then falls back to the pre-rebrand registry key, model environment override, and model folders. It replaces the old startup entry with `SAID`, and the compatible single-instance guard prevents the old and new executables from running together. Old Whisper `.bin` files are not compatible with the new recognizer and are left untouched. The uninstaller deliberately leaves user model files in `%LOCALAPPDATA%\SAID\models`.

## Resource profile

- Native Win32 UI and global keyboard hook; no Electron or browser runtime.
- Microphone capture is inactive while idle.
- The CPU-only SenseVoice, punctuation, and VAD models use about 315 MB on disk and remain loaded after startup.
- SenseVoice uses non-autoregressive decoding with automatic language selection for Mandarin/English code switching.
- Dictations up to 25 seconds are recognized intact. Longer audio is split at speech boundaries into segments no longer than 20 seconds.
- Inference uses up to half the logical processors, capped at eight threads.
- Audio capture is capped at ten minutes per dictation.

## Current limitations

- Some elevated or unusually sandboxed applications can reject Windows Unicode input; SAID copies the transcript in that case.
- Users with AltGr keyboard layouts should select F8 during setup so Right Alt does not intercept AltGr characters.
- Very short English insertions inside Mandarin—especially uncommon names and one-syllable words—remain the hardest recognition case.

See [Mixed Chinese/English ASR investigation](docs/MIXED_LANGUAGE_ASR.md) for the benchmark and design rationale.

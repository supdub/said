# VoiceKey

VoiceKey is a lightweight Windows 10/11 voice keyboard. Tap your shortcut once,
speak in Chinese, English, or both, and tap it again. The recognized text is
inserted into the field that already had focus. Right Alt is the default; F8 is
available for AltGr keyboard layouts.

The app intentionally does one job: speech recognition. It does not summarize,
rewrite, translate, remove filler words, change tone, or send audio to an LLM.
Recognition runs locally through an AVX2-optimized, high-quality Q8 multilingual
`whisper.cpp` base model. VoiceKey uses context-aware beam search and native
speech boundaries, then normalizes Chinese output to simplified characters.

## User experience

1. Put the caret in any editable field.
2. Tap your VoiceKey shortcut. A small voice bar appears without taking focus.
3. Speak naturally.
4. Tap the shortcut again. The bar shows `Turning speech into text`, then VoiceKey types the
   verbatim recognizer output at the caret.

On first launch, VoiceKey walks through a short microphone check, lets you choose
**Right Alt** or **F8**, and gives you a chance to rehearse the shortcut. Double-click
the VoiceKey tray icon at any time to reopen Setup & settings.

## Install on Windows

Run `VoiceKey-Setup-0.2.0.exe`. The per-user installer does not need administrator
access. It installs VoiceKey and its private speech model, adds Start Menu and
uninstall entries, optionally creates a desktop shortcut, and can keep VoiceKey
ready after sign-in.

The portable `VoiceKey-windows-x64-0.2.0.zip` remains available for users who do
not want an installed app.

The microphone is opened only between the two shortcut presses. The model stays
loaded so subsequent dictations start quickly. If focus changes before recognition
finishes, VoiceKey copies the transcript instead of typing into the wrong app.

## Build on Windows

Prerequisites: Visual Studio 2022 Build Tools with Desktop C++, CMake 3.21+, Git,
and PowerShell 5+.

```powershell
Set-ExecutionPolicy -Scope Process Bypass
./scripts/build-windows.ps1
```

The script builds `voicekey.exe`, downloads the pinned multilingual base model,
verifies its SHA-256 hash, creates the portable ZIP, and builds the branded NSIS
installer. Pass `-SkipInstaller` only when NSIS is unavailable.

Run:

```powershell
./dist/VoiceKey/voicekey.exe
```

VoiceKey is a tray app. Right-click its tray icon for status, the model folder,
or Quit. It does not require administrator privileges; Windows prevents normal
apps from injecting text into elevated/admin windows.

## Manual CMake build

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

For an offline build with an existing whisper.cpp checkout:

```powershell
cmake -S . -B build -A x64 `
  -DVOICEKEY_WHISPER_SOURCE_DIR=C:/src/whisper.cpp
```

The model is searched in this order:

1. `--model C:\path\to\multilingual-model.bin`
2. `%VOICEKEY_MODEL%`
3. `models\ggml-base-q8_0.bin` beside `voicekey.exe`
4. `models\ggml-base-q5_1.bin` beside `voicekey.exe` (legacy fallback)
5. `models\ggml-base.bin` beside `voicekey.exe` (legacy fallback)
6. `%LOCALAPPDATA%\VoiceKey\models\ggml-base-q8_0.bin`
7. Older base models in that same local model folder (legacy fallback)

Use a multilingual model (`ggml-base-q8_0.bin`, not an `.en` model).

### Cross-build from Linux with Zig

The checked-in Zig toolchain file is used for CI and build verification. Install
`x86_64-w64-mingw32-windres` as well so the app icon, manifest, and version
resources are embedded:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-x64.cmake \
  -DZIG_EXECUTABLE=/path/to/zig \
  -DVOICEKEY_WHISPER_SOURCE_DIR=/path/to/whisper.cpp \
  -DVOICEKEY_BUILD_TESTS=OFF
cmake --build build-windows --parallel
```

The Visual Studio build remains the supported release build because it is the
best match for ordinary Windows machines.

Public release artifacts should be Authenticode-signed before distribution;
unsigned local builds can trigger Microsoft Defender SmartScreen reputation
warnings even though the installer does not request administrator access.

## Resource profile

- Native Win32 UI and global keyboard hook; no Electron or browser runtime.
- Microphone capture is inactive while idle.
- One CPU-only multilingual Q8 base model (about 82 MB on disk) remains loaded after startup.
- Five-candidate beam search favors transcription quality and punctuation over minimum latency.
- AVX2/FMA/F16C kernels replace the scalar fallback used by the first build.
- Inference uses up to half the logical processors, capped at ten threads.
- Audio capture is capped at ten minutes per dictation.

## Current limitations

- Text insertion uses Windows Unicode keyboard input. Some elevated or unusually
  sandboxed applications can reject it; the transcript is then copied.
- Right Alt is the default shortcut. Users with AltGr keyboard layouts should
  select F8 during setup so VoiceKey does not intercept AltGr characters.

param(
    [string]$BuildDirectory = "build",
    [string]$Configuration = "Release",
    [string]$WhisperSource = "",
    [string]$Makensis = "",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildPath = Join-Path $ProjectRoot $BuildDirectory
$DistPath = Join-Path $ProjectRoot "dist/VoiceKey"
$ModelDirectory = Join-Path $DistPath "models"
$ModelPath = Join-Path $ModelDirectory "ggml-base-q8_0.bin"
$LegacyModelPath = Join-Path $ModelDirectory "ggml-base-q5_1.bin"
$ExpectedModelSha256 = "c577b9a86e7e048a0b7eada054f4dd79a56bbfa911fbdacf900ac5b567cbb7d9"

$ConfigureArguments = @(
    "-S", $ProjectRoot,
    "-B", $BuildPath,
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DVOICEKEY_BUILD_TESTS=ON"
)
if ($WhisperSource) {
    $ConfigureArguments += "-DVOICEKEY_WHISPER_SOURCE_DIR=$WhisperSource"
}

cmake @ConfigureArguments
cmake --build $BuildPath --config $Configuration --parallel
ctest --test-dir $BuildPath -C $Configuration --output-on-failure

New-Item -ItemType Directory -Force -Path $ModelDirectory | Out-Null
$ExecutableCandidates = @(
    (Join-Path $BuildPath "$Configuration/voicekey.exe"),
    (Join-Path $BuildPath "voicekey.exe")
)
$Executable = $ExecutableCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Executable) {
    throw "The build completed but voicekey.exe was not found."
}
Copy-Item -Force $Executable (Join-Path $DistPath "voicekey.exe")

if (-not (Test-Path $ModelPath) -or
    (Get-FileHash -Algorithm SHA256 $ModelPath).Hash.ToLowerInvariant() -ne $ExpectedModelSha256) {
    Write-Host "Downloading the high-quality multilingual Whisper base model (82 MB)..."
    Invoke-WebRequest `
        -Uri "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q8_0.bin" `
        -OutFile $ModelPath
}

$ActualHash = (Get-FileHash -Algorithm SHA256 $ModelPath).Hash.ToLowerInvariant()
if ($ActualHash -ne $ExpectedModelSha256) {
    Remove-Item -Force $ModelPath
    throw "The downloaded model failed SHA-256 verification."
}
if (Test-Path $LegacyModelPath) {
    Remove-Item -Force $LegacyModelPath
}

Copy-Item -Force (Join-Path $ProjectRoot "README.md") (Join-Path $DistPath "README.md")
Copy-Item -Force (Join-Path $ProjectRoot "LICENSE") (Join-Path $DistPath "LICENSE")
Copy-Item -Force (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") (Join-Path $DistPath "THIRD_PARTY_NOTICES.md")

$PortableZip = Join-Path $ProjectRoot "dist/VoiceKey-windows-x64-0.2.0.zip"
if (Test-Path $PortableZip) {
    Remove-Item -Force $PortableZip
}
Compress-Archive -Path $DistPath -DestinationPath $PortableZip -CompressionLevel Optimal

if (-not $SkipInstaller) {
    if (-not $Makensis) {
        $MakensisCommand = Get-Command makensis.exe -ErrorAction SilentlyContinue
        if (-not $MakensisCommand) {
            $MakensisCommand = Get-Command makensis -ErrorAction SilentlyContinue
        }
        if ($MakensisCommand) {
            $Makensis = $MakensisCommand.Source
        }
    }
    if (-not $Makensis) {
        throw "NSIS was not found. Install NSIS, pass -Makensis, or use -SkipInstaller."
    }
    $InstallerPath = Join-Path $ProjectRoot "dist/VoiceKey-Setup-0.2.0.exe"
    & $Makensis `
        "/DPROJECT_ROOT=$ProjectRoot" `
        "/DDIST_DIR=$DistPath" `
        "/DOUTPUT_FILE=$InstallerPath" `
        (Join-Path $ProjectRoot "installer/VoiceKey.nsi")
}

Write-Host "VoiceKey portable package: $PortableZip"
if (-not $SkipInstaller) {
    Write-Host "VoiceKey installer: $(Join-Path $ProjectRoot 'dist/VoiceKey-Setup-0.2.0.exe')"
}

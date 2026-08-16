param(
    [string]$BuildDirectory = "build",
    [string]$Configuration = "Release",
    [string]$SherpaOnnxSource = "",
    [string]$Makensis = "",
    [string]$Version = "0.2.0",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildPath = Join-Path $ProjectRoot $BuildDirectory
$DistPath = Join-Path $ProjectRoot "dist/SAID"
$ModelDirectory = Join-Path $DistPath "models"
$ModelCache = Join-Path $BuildPath "model-cache"
$RecognizerName = "sense-voice-small.int8.onnx"
$TokensName = "sense-voice-small.tokens.txt"
$PunctuationName = "ct-transformer-punctuation.int8.onnx"
$VadName = "silero-vad.onnx"

$RecognizerSha256 = "c71f0ce00bec95b07744e116345e33d8cbbe08cef896382cf907bf4b51a2cd51"
$TokensSha256 = "f449eb28dc567533d7fa59be34e2abca8784f771850c78a47fb731a31429a1dc"
$PunctuationSha256 = "65a3fb9f5ad7bfb96bf69e0dc4481df97f6ee60513c1d94ce981ba6effd524b1"
$VadSha256 = "9e2449e1087496d8d4caba907f23e0bd3f78d91fa552479bb9c23ac09cbb1fd6"
$PunctuationArchiveSha256 = "c0d5aa5f8eeb686032345e180bedf39319dc2e0556781c6264bcadba8328a6e1"

function Test-VerifiedFile {
    param([string]$Path, [string]$ExpectedSha256)
    return (Test-Path $Path) -and
        ((Get-FileHash -Algorithm SHA256 $Path).Hash.ToLowerInvariant() -eq $ExpectedSha256)
}

function Get-VerifiedFile {
    param(
        [string]$Uri,
        [string]$Path,
        [string]$ExpectedSha256,
        [string]$Description
    )
    if (Test-VerifiedFile $Path $ExpectedSha256) {
        return
    }
    if (Test-Path $Path) {
        Remove-Item -Force $Path
    }
    Write-Host "Downloading $Description..."
    Invoke-WebRequest -Uri $Uri -OutFile $Path
    if (-not (Test-VerifiedFile $Path $ExpectedSha256)) {
        Remove-Item -Force $Path
        throw "$Description failed SHA-256 verification."
    }
}

$ConfigureArguments = @(
    "-S", $ProjectRoot,
    "-B", $BuildPath,
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DSAID_BUILD_TESTS=ON"
)
if ($SherpaOnnxSource) {
    $ConfigureArguments += "-DSAID_SHERPA_ONNX_SOURCE_DIR=$SherpaOnnxSource"
}

cmake @ConfigureArguments
cmake --build $BuildPath --config $Configuration --parallel
ctest --test-dir $BuildPath -C $Configuration --output-on-failure

if (Test-Path $DistPath) {
    Remove-Item -Recurse -Force $DistPath
}
New-Item -ItemType Directory -Force -Path $ModelDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $ModelCache | Out-Null
$ExecutableCandidates = @(
    (Join-Path $BuildPath "$Configuration/said.exe"),
    (Join-Path $BuildPath "said.exe")
)
$Executable = $ExecutableCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Executable) {
    throw "The build completed but said.exe was not found."
}
Copy-Item -Force $Executable (Join-Path $DistPath "said.exe")
$ExecutableDirectory = Split-Path -Parent $Executable
foreach ($RuntimeName in @("onnxruntime.dll", "onnxruntime_providers_shared.dll")) {
    $RuntimePath = Join-Path $ExecutableDirectory $RuntimeName
    if (Test-Path $RuntimePath) {
        Copy-Item -Force $RuntimePath (Join-Path $DistPath $RuntimeName)
    }
}

$RecognizerCachePath = Join-Path $ModelCache $RecognizerName
$TokensCachePath = Join-Path $ModelCache $TokensName
$PunctuationCachePath = Join-Path $ModelCache $PunctuationName
$VadCachePath = Join-Path $ModelCache $VadName

Get-VerifiedFile `
    "https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.int8.onnx" `
    $RecognizerCachePath $RecognizerSha256 "SenseVoice Small int8 model (239 MB)"
Get-VerifiedFile `
    "https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt" `
    $TokensCachePath $TokensSha256 "SenseVoice token table"
Get-VerifiedFile `
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx" `
    $VadCachePath $VadSha256 "Silero voice activity model"

if (-not (Test-VerifiedFile $PunctuationCachePath $PunctuationSha256)) {
    $PunctuationArchive = Join-Path $ModelCache "punctuation-model.tar.bz2"
    Get-VerifiedFile `
        "https://github.com/k2-fsa/sherpa-onnx/releases/download/punctuation-models/sherpa-onnx-punct-ct-transformer-zh-en-vocab272727-2024-04-12-int8.tar.bz2" `
        $PunctuationArchive $PunctuationArchiveSha256 "Chinese/English punctuation model (62 MB)"

    $TarCommand = Get-Command tar.exe -ErrorAction SilentlyContinue
    if (-not $TarCommand) {
        throw "tar.exe is required to unpack the punctuation model and is included with current Windows versions."
    }
    $PunctuationExtractPath = Join-Path $BuildPath "punctuation-model-extract"
    if (Test-Path $PunctuationExtractPath) {
        Remove-Item -Recurse -Force $PunctuationExtractPath
    }
    New-Item -ItemType Directory -Force -Path $PunctuationExtractPath | Out-Null
    & $TarCommand.Source -xjf $PunctuationArchive -C $PunctuationExtractPath
    if ($LASTEXITCODE -ne 0) {
        throw "Could not unpack the punctuation model."
    }
    $ExtractedPunctuation = Get-ChildItem $PunctuationExtractPath -Recurse -File -Filter "model.int8.onnx" |
        Select-Object -First 1
    if (-not $ExtractedPunctuation) {
        throw "The punctuation archive did not contain model.int8.onnx."
    }
    Copy-Item -Force $ExtractedPunctuation.FullName $PunctuationCachePath
    if (-not (Test-VerifiedFile $PunctuationCachePath $PunctuationSha256)) {
        Remove-Item -Force $PunctuationCachePath
        throw "The unpacked punctuation model failed SHA-256 verification."
    }
}

Copy-Item -Force $RecognizerCachePath (Join-Path $ModelDirectory $RecognizerName)
Copy-Item -Force $TokensCachePath (Join-Path $ModelDirectory $TokensName)
Copy-Item -Force $PunctuationCachePath (Join-Path $ModelDirectory $PunctuationName)
Copy-Item -Force $VadCachePath (Join-Path $ModelDirectory $VadName)

Copy-Item -Force (Join-Path $ProjectRoot "README.md") (Join-Path $DistPath "README.md")
Copy-Item -Force (Join-Path $ProjectRoot "LICENSE") (Join-Path $DistPath "LICENSE")
Copy-Item -Force (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") (Join-Path $DistPath "THIRD_PARTY_NOTICES.md")
Copy-Item -Recurse -Force (Join-Path $ProjectRoot "third_party") (Join-Path $DistPath "third_party")

$PortableZip = Join-Path $ProjectRoot "dist/SAID-windows-x64-$Version.zip"
if (Test-Path $PortableZip) {
    Remove-Item -Force $PortableZip
}
Compress-Archive -Path (Join-Path $DistPath "*") -DestinationPath $PortableZip -CompressionLevel Optimal

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
    $InstallerPath = Join-Path $ProjectRoot "dist/SAID-Setup-$Version.exe"
    & $Makensis `
        "/DPROJECT_ROOT=$ProjectRoot" `
        "/DDIST_DIR=$DistPath" `
        "/DOUTPUT_FILE=$InstallerPath" `
        "/DPRODUCT_VERSION=$Version" `
        (Join-Path $ProjectRoot "installer/SAID.nsi")
}

Write-Host "SAID portable package: $PortableZip"
if (-not $SkipInstaller) {
    Write-Host "SAID installer: $InstallerPath"
}

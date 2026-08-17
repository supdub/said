#!/bin/bash
set -euo pipefail

project_root=$(cd "$(dirname "$0")/.." && pwd)
build_dir=${BUILD_DIRECTORY:-"$project_root/build-macos"}
dist_dir=${DIST_DIRECTORY:-"$project_root/dist"}
model_cache=${MODEL_CACHE_DIRECTORY:-"$build_dir/model-cache"}
model_dir="$build_dir/release-models"
configuration=${CONFIGURATION:-Release}
architectures=${SAID_MACOS_ARCHITECTURES:-"arm64;x86_64"}
punctuation_extract=""
dmg_mount=""

cleanup() {
    if [[ -n "$dmg_mount" && -d "$dmg_mount" ]]; then
        hdiutil detach "$dmg_mount" -quiet || true
        rmdir "$dmg_mount" 2>/dev/null || true
    fi
    if [[ -n "$punctuation_extract" && -d "$punctuation_extract" ]]; then
        rm -rf "$punctuation_extract"
    fi
}
trap cleanup EXIT

recognizer_name=sense-voice-small.int8.onnx
tokens_name=sense-voice-small.tokens.txt
punctuation_name=ct-transformer-punctuation.int8.onnx
vad_name=silero-vad.onnx

recognizer_sha256=c71f0ce00bec95b07744e116345e33d8cbbe08cef896382cf907bf4b51a2cd51
tokens_sha256=f449eb28dc567533d7fa59be34e2abca8784f771850c78a47fb731a31429a1dc
punctuation_sha256=65a3fb9f5ad7bfb96bf69e0dc4481df97f6ee60513c1d94ce981ba6effd524b1
vad_sha256=9e2449e1087496d8d4caba907f23e0bd3f78d91fa552479bb9c23ac09cbb1fd6
punctuation_archive_sha256=c0d5aa5f8eeb686032345e180bedf39319dc2e0556781c6264bcadba8328a6e1

verified_file() {
    local path=$1
    local expected=$2
    [[ -f "$path" ]] && [[ "$(shasum -a 256 "$path" | awk '{print $1}')" == "$expected" ]]
}

download_verified() {
    local url=$1
    local path=$2
    local expected=$3
    local description=$4
    if verified_file "$path" "$expected"; then
        return
    fi
    if [[ -e "$path" ]]; then
        rm -f "$path"
    fi
    echo "Downloading $description..."
    curl --fail --location --retry 3 --output "$path" "$url"
    if ! verified_file "$path" "$expected"; then
        rm -f "$path"
        echo "$description failed SHA-256 verification." >&2
        exit 1
    fi
}

mkdir -p "$build_dir" "$dist_dir" "$model_cache" "$model_dir"
build_dir=$(cd "$build_dir" && pwd)
dist_dir=$(cd "$dist_dir" && pwd)
model_cache=$(cd "$model_cache" && pwd)
model_dir=$(cd "$model_dir" && pwd)
project_version=$(sed -nE \
    's/^project\(SAID VERSION ([^ ]+) LANGUAGES.*$/\1/p' \
    "$project_root/CMakeLists.txt")
if [[ -z "$project_version" ]]; then
    echo "Could not read the SAID version from CMakeLists.txt." >&2
    exit 1
fi
if [[ -n "${APPLE_NOTARY_PROFILE:-}" && -z "${APPLE_SIGNING_IDENTITY:-}" ]]; then
    echo "APPLE_SIGNING_IDENTITY is required when notarization is requested." >&2
    exit 1
fi
notary_value_count=0
for notary_value in "${APPLE_ID:-}" "${APPLE_TEAM_ID:-}" "${APPLE_APP_PASSWORD:-}"; do
    if [[ -n "$notary_value" ]]; then
        notary_value_count=$((notary_value_count + 1))
    fi
done
if [[ "$notary_value_count" -ne 0 && "$notary_value_count" -ne 3 ]]; then
    echo "APPLE_ID, APPLE_TEAM_ID, and APPLE_APP_PASSWORD must be set together." >&2
    exit 1
fi
if [[ "$notary_value_count" -eq 3 && -z "${APPLE_SIGNING_IDENTITY:-}" ]]; then
    echo "APPLE_SIGNING_IDENTITY is required when notarization is requested." >&2
    exit 1
fi

download_verified \
    "https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.int8.onnx" \
    "$model_cache/$recognizer_name" \
    "$recognizer_sha256" \
    "SenseVoice Small int8 model (239 MB)"
download_verified \
    "https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt" \
    "$model_cache/$tokens_name" \
    "$tokens_sha256" \
    "SenseVoice token table"
download_verified \
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx" \
    "$model_cache/$vad_name" \
    "$vad_sha256" \
    "Silero voice activity model"

if ! verified_file "$model_cache/$punctuation_name" "$punctuation_sha256"; then
    punctuation_archive="$model_cache/punctuation-model.tar.bz2"
    download_verified \
        "https://github.com/k2-fsa/sherpa-onnx/releases/download/punctuation-models/sherpa-onnx-punct-ct-transformer-zh-en-vocab272727-2024-04-12-int8.tar.bz2" \
        "$punctuation_archive" \
        "$punctuation_archive_sha256" \
        "Chinese/English punctuation model (62 MB)"
    punctuation_extract=$(mktemp -d "${TMPDIR:-/tmp}/said-punctuation.XXXXXX")
    tar -xjf "$punctuation_archive" -C "$punctuation_extract"
    extracted_punctuation=$(find "$punctuation_extract" -type f -name model.int8.onnx -print -quit)
    if [[ -z "$extracted_punctuation" ]]; then
        echo "The punctuation archive did not contain model.int8.onnx." >&2
        exit 1
    fi
    cp "$extracted_punctuation" "$model_cache/$punctuation_name"
    if ! verified_file "$model_cache/$punctuation_name" "$punctuation_sha256"; then
        rm -f "$model_cache/$punctuation_name"
        echo "The unpacked punctuation model failed SHA-256 verification." >&2
        exit 1
    fi
fi

cp "$model_cache/$recognizer_name" "$model_dir/$recognizer_name"
cp "$model_cache/$tokens_name" "$model_dir/$tokens_name"
cp "$model_cache/$punctuation_name" "$model_dir/$punctuation_name"
cp "$model_cache/$vad_name" "$model_dir/$vad_name"

if [[ "$architectures" == *arm64* && "$architectures" == *x86_64* ]]; then
    artifact_arch=universal
else
    artifact_arch=${architectures//;/_}
fi

cmake \
    -S "$project_root" \
    -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$configuration" \
    -DCMAKE_OSX_ARCHITECTURES="$architectures" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DSAID_BUILD_TESTS=ON \
    -DSAID_MACOS_MODEL_DIR="$model_dir" \
    -DSAID_MACOS_ARTIFACT_ARCH="$artifact_arch"
cmake --build "$build_dir" --config "$configuration" --parallel
ctest --test-dir "$build_dir" -C "$configuration" --output-on-failure

app_path="$build_dir/SAID.app"
if [[ ! -d "$app_path" ]]; then
    echo "The build completed but $app_path was not found." >&2
    exit 1
fi

if [[ -n "${APPLE_SIGNING_IDENTITY:-}" ]]; then
    codesign \
        --force \
        --deep \
        --options runtime \
        --timestamp \
        --entitlements "$project_root/resources/macos/SAID.entitlements" \
        --sign "$APPLE_SIGNING_IDENTITY" \
        "$app_path"
else
    codesign --force --deep --sign - "$app_path"
fi
codesign --verify --deep --strict --verbose=2 "$app_path"
"$app_path/Contents/MacOS/SAID" --version | grep -F "SAID $project_version"
"$app_path/Contents/MacOS/SAID" --self-test
if [[ "$artifact_arch" == universal ]]; then
    lipo -verify_arch arm64 x86_64 "$app_path/Contents/MacOS/SAID"
else
    lipo -verify_arch "$architectures" "$app_path/Contents/MacOS/SAID"
fi

cpack --config "$build_dir/CPackConfig.cmake" -G DragNDrop -B "$dist_dir"
dmg_path="$dist_dir/SAID-macos-$artifact_arch-$project_version.dmg"
if [[ ! -f "$dmg_path" ]]; then
    echo "CPack completed but the SAID DMG was not found." >&2
    exit 1
fi

dmg_mount=$(mktemp -d "${TMPDIR:-/tmp}/said-dmg.XXXXXX")
hdiutil attach "$dmg_path" -nobrowse -readonly -mountpoint "$dmg_mount" -quiet
mounted_app="$dmg_mount/SAID.app"
if [[ ! -d "$mounted_app" || ! -L "$dmg_mount/Applications" ]]; then
    echo "The DMG does not contain SAID.app and the Applications shortcut." >&2
    exit 1
fi
codesign --verify --deep --strict --verbose=2 "$mounted_app"
if [[ "$artifact_arch" == universal ]]; then
    lipo -verify_arch arm64 x86_64 "$mounted_app/Contents/MacOS/SAID"
else
    lipo -verify_arch "$architectures" "$mounted_app/Contents/MacOS/SAID"
fi
"$mounted_app/Contents/MacOS/SAID" --version | grep -F "SAID $project_version"
"$mounted_app/Contents/MacOS/SAID" --self-test
for model_file in "$recognizer_name" "$tokens_name" "$punctuation_name" "$vad_name"; do
    if ! cmp -s "$model_dir/$model_file" \
        "$mounted_app/Contents/Resources/models/$model_file"; then
        echo "The DMG contains an incorrect $model_file." >&2
        exit 1
    fi
done
for license_file in LICENSE THIRD_PARTY_NOTICES.md; do
    if [[ ! -f "$mounted_app/Contents/Resources/licenses/$license_file" ]]; then
        echo "The DMG is missing $license_file." >&2
        exit 1
    fi
done
if [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' \
    "$mounted_app/Contents/Info.plist")" != "$project_version" ]]; then
    echo "The app bundle version does not match $project_version." >&2
    exit 1
fi
hdiutil detach "$dmg_mount" -quiet
rmdir "$dmg_mount" 2>/dev/null || true
dmg_mount=""

if [[ -n "${APPLE_SIGNING_IDENTITY:-}" ]]; then
    codesign --force --timestamp --sign "$APPLE_SIGNING_IDENTITY" "$dmg_path"
    codesign --verify --verbose=2 "$dmg_path"
fi

if [[ -n "${APPLE_NOTARY_PROFILE:-}" ]]; then
    xcrun notarytool submit "$dmg_path" \
        --keychain-profile "$APPLE_NOTARY_PROFILE" \
        --wait
    xcrun stapler staple "$dmg_path"
    xcrun stapler validate "$dmg_path"
    spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg_path"
elif [[ -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" ]]; then
    xcrun notarytool submit "$dmg_path" \
        --apple-id "$APPLE_ID" \
        --team-id "$APPLE_TEAM_ID" \
        --password "$APPLE_APP_PASSWORD" \
        --wait
    xcrun stapler staple "$dmg_path"
    xcrun stapler validate "$dmg_path"
    spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg_path"
fi

checksum_path="$dist_dir/SHA256SUMS-macos.txt"
(
    cd "$dist_dir"
    shasum -a 256 "$(basename "$dmg_path")" >"SHA256SUMS-macos.txt"
)

echo "SAID macOS release: $dmg_path"
echo "SAID macOS checksum: $checksum_path"

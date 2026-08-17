#!/bin/bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 source.png output.icns" >&2
    exit 2
fi

source_png=$1
output_icns=$2
icon_work_dir=$(mktemp -d "${TMPDIR:-/tmp}/said-icon.XXXXXX")
iconset="$icon_work_dir/SAID.iconset"
trap 'rm -rf "$icon_work_dir"' EXIT

mkdir -p "$iconset"
for size in 16 32 128 256 512; do
    double_size=$((size * 2))
    sips -z "$size" "$size" "$source_png" --out "$iconset/icon_${size}x${size}.png" >/dev/null
    sips -z "$double_size" "$double_size" "$source_png" \
        --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done

mkdir -p "$(dirname "$output_icns")"
iconutil -c icns "$iconset" -o "$output_icns"

#!/bin/sh
set -eu

if [ "$#" -lt 4 ]; then
    echo "usage: package.sh <linux|macos> <arch> <void-yt-binary> <output-dir> [version]" >&2
    exit 2
fi

platform=$1
arch=$2
binary=$3
output_dir=$4
version=${5:-0.1.0}
yt_dlp_version=2026.07.04
qjs_version=0.15.0

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/void-yt-package.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

stage="$work_dir/stage"
mkdir -p "$stage/tools" "$stage/licenses" "$output_dir"
cp "$binary" "$stage/void-yt"
cp "$project_dir/LICENSE" "$stage/LICENSE"
cp "$project_dir/THIRD_PARTY_NOTICES.md" "$stage/THIRD_PARTY_NOTICES.md"
cp "$project_dir/licenses/quickjs-ng.txt" "$stage/licenses/quickjs-ng.txt"
cp "$project_dir/licenses/yt-dlp.txt" "$stage/licenses/yt-dlp.txt"
cp "$project_dir/scripts/update.sh" "$stage/tools/update.sh"
cp "$project_dir/scripts/install-ffmpeg.sh" "$stage/tools/install-ffmpeg.sh"
chmod 755 "$stage/void-yt" "$stage/tools/update.sh" "$stage/tools/install-ffmpeg.sh"

verify_sha256() {
    expected=$1
    file=$2
    if command -v sha256sum >/dev/null 2>&1; then
        actual=$(sha256sum "$file" | awk '{print $1}')
    else
        actual=$(shasum -a 256 "$file" | awk '{print $1}')
    fi
    if [ "$actual" != "$expected" ]; then
        echo "checksum mismatch for $file" >&2
        exit 1
    fi
}

case "$platform-$arch" in
    linux-x86_64)
        yt_asset=yt-dlp_linux
        yt_hash=6bbb3d314cde4febe36e5fa1d55462e29c974f63444e707871834f6d8cc210ae
        qjs_asset=qjs-linux-x86_64
        qjs_hash=6f87100b30b2212d529b2024213d27c3ea8082d8b4ec5aec0f01e8a0824c4e1d
        ;;
    linux-aarch64)
        yt_asset=yt-dlp_linux_aarch64
        yt_hash=b6ce97646773070d7a7ffd6bbbdcaecb47c48483909c54c915bf08a7a9b5e0b1
        qjs_asset=qjs-linux-aarch64
        qjs_hash=72782b85afb49d4c1228495138fd38771fff949a6ecee3d8151077cd127d7072
        ;;
    macos-universal)
        yt_asset=yt-dlp_macos
        yt_hash=498bd0dae17855c599d371d68ec5bafc439a9d8640e838be25c765a9792f261b
        qjs_asset=qjs-darwin
        qjs_hash=b17a9e1c23636a831cd94b97ffd79ec78b73941a51e40a8f2c2a0a5d896fa3e4
        ;;
    *)
        echo "unsupported package target: $platform-$arch" >&2
        exit 2
        ;;
esac

curl -fL --retry 3 \
    "https://github.com/yt-dlp/yt-dlp/releases/download/$yt_dlp_version/$yt_asset" \
    -o "$stage/tools/yt-dlp"
curl -fL --retry 3 \
    "https://github.com/quickjs-ng/quickjs/releases/download/v$qjs_version/$qjs_asset" \
    -o "$stage/tools/qjs"

verify_sha256 "$yt_hash" "$stage/tools/yt-dlp"
verify_sha256 "$qjs_hash" "$stage/tools/qjs"
chmod 755 "$stage/tools/yt-dlp" "$stage/tools/qjs"

cat > "$stage/BUILD_INFO.txt" <<EOF
Void-YT: $version
yt-dlp: $yt_dlp_version
QuickJS-NG: $qjs_version
Target: $platform-$arch
EOF

asset_name="void-yt-$platform-$arch.tar.gz"
tar -C "$stage" -czf "$output_dir/$asset_name" .
echo "$output_dir/$asset_name"

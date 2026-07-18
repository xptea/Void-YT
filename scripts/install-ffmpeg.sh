#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: install-ffmpeg.sh <application-directory> <curl>" >&2
    exit 2
fi

app_dir=$1
curl_path=$2
ffmpeg_version=8.1.2
tools_dir="$app_dir/tools"
target="$tools_dir/ffmpeg"
pending="$tools_dir/ffmpeg.pending"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/void-yt-ffmpeg.XXXXXX")
trap 'rm -f "$pending"; rm -rf "$work_dir"' EXIT HUP INT TERM

case "$(uname -s)-$(uname -m)" in
    Linux-x86_64|Linux-amd64)
        archive_name=ffmpeg-n8.1.2-22-g94138f6973-linux64-lgpl-8.1.tar.xz
        default_url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-07-17-13-22/$archive_name"
        default_sha256=d0707d8050d1e6e15cf7b6208c31052142ea0380dcdd1b552563cd5f37f672d0
        archive_kind=tar.xz
        download_size="about 107 MB"
        ;;
    Linux-aarch64|Linux-arm64)
        archive_name=ffmpeg-n8.1.2-22-g94138f6973-linuxarm64-lgpl-8.1.tar.xz
        default_url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-07-17-13-22/$archive_name"
        default_sha256=d4fb83ef3d4731333f17c2def4102dace67c59950c7b61c18fef1c4266255c4c
        archive_kind=tar.xz
        download_size="about 92 MB"
        ;;
    Darwin-x86_64|Darwin-arm64)
        archive_name=ffmpeg-8.1.2.zip
        default_url=https://evermeet.cx/ffmpeg/ffmpeg-8.1.2.zip
        default_sha256=e91df72a1ee7c26606f90dd2dd4dcccc6a75140ff9ea6fdd50faae828b82ba69
        archive_kind=zip
        download_size="about 25 MB"
        ;;
    *)
        echo "void-yt: automatic FFmpeg installation is unsupported on $(uname -s) $(uname -m)" >&2
        exit 1
        ;;
esac

archive_url=${VOID_YT_FFMPEG_URL:-$default_url}
expected_sha256=${VOID_YT_FFMPEG_SHA256:-$default_sha256}
archive="$work_dir/$archive_name"
unpacked="$work_dir/unpacked"
mkdir -p "$tools_dir" "$unpacked"

echo "Downloading FFmpeg $ffmpeg_version ($download_size)..."
"$curl_path" -fL --retry 3 --connect-timeout 10 -o "$archive" -- "$archive_url"

if command -v sha256sum >/dev/null 2>&1; then
    actual_sha256=$(sha256sum "$archive" | awk '{print $1}')
else
    actual_sha256=$(shasum -a 256 "$archive" | awk '{print $1}')
fi
if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "void-yt: FFmpeg checksum verification failed" >&2
    exit 1
fi

case "$archive_kind" in
    tar.xz)
        tar -xJf "$archive" -C "$unpacked"
        ;;
    zip)
        if command -v ditto >/dev/null 2>&1; then
            ditto -x -k "$archive" "$unpacked"
        elif command -v unzip >/dev/null 2>&1; then
            unzip -q "$archive" -d "$unpacked"
        else
            echo "void-yt: ditto or unzip is required to install FFmpeg" >&2
            exit 1
        fi
        ;;
esac

candidate=$(find "$unpacked" -type f -name ffmpeg -print | head -n 1)
if [ -z "$candidate" ]; then
    echo "void-yt: the FFmpeg archive did not contain an ffmpeg executable" >&2
    exit 1
fi

cp "$candidate" "$pending"
chmod 755 "$pending"
if ! "$pending" -version >/dev/null 2>&1; then
    echo "void-yt: the downloaded FFmpeg executable failed its version check" >&2
    exit 1
fi
mv -f "$pending" "$target"
cat > "$tools_dir/ffmpeg-source.txt" <<EOF
FFmpeg: $ffmpeg_version
Provider URL: $default_url
Archive SHA-256: $default_sha256
License information: https://ffmpeg.org/legal.html
EOF
echo "FFmpeg $ffmpeg_version installed and verified."

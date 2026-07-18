#!/bin/sh
set -eu

repo=${VOID_YT_REPO:-@GITHUB_REPOSITORY@}
install_dir=${VOID_YT_INSTALL_DIR:-"$HOME/.local/share/void-yt"}
bin_dir=${VOID_YT_BIN_DIR:-"$HOME/.local/bin"}

case "$repo" in
    @*)
        echo "installer has not been stamped with its GitHub repository" >&2
        echo "set VOID_YT_REPO=owner/repository and run again" >&2
        exit 2
        ;;
esac

os=$(uname -s)
machine=$(uname -m)
case "$os" in
    Linux)
        case "$machine" in
            x86_64|amd64) asset=void-yt-linux-x86_64.tar.gz ;;
            aarch64|arm64) asset=void-yt-linux-aarch64.tar.gz ;;
            *) echo "unsupported Linux architecture: $machine" >&2; exit 2 ;;
        esac
        ;;
    Darwin) asset=void-yt-macos-universal.tar.gz ;;
    *) echo "unsupported operating system: $os" >&2; exit 2 ;;
esac

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/void-yt-install.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM
base_url="https://github.com/$repo/releases/latest/download"

echo "Downloading $asset..."
curl -fL --retry 3 "$base_url/$asset" -o "$work_dir/$asset"
curl -fL --retry 3 "$base_url/checksums.sha256" -o "$work_dir/checksums.sha256"

expected=$(awk -v name="$asset" '$2 == name || $2 == "*" name { print $1; exit }' "$work_dir/checksums.sha256")
if [ -z "$expected" ]; then
    echo "release checksum is missing for $asset" >&2
    exit 1
fi
if command -v sha256sum >/dev/null 2>&1; then
    actual=$(sha256sum "$work_dir/$asset" | awk '{print $1}')
else
    actual=$(shasum -a 256 "$work_dir/$asset" | awk '{print $1}')
fi
if [ "$actual" != "$expected" ]; then
    echo "checksum verification failed for $asset" >&2
    exit 1
fi

mkdir -p "$work_dir/unpacked" "$install_dir" "$bin_dir"
tar -C "$work_dir/unpacked" -xzf "$work_dir/$asset"
cp -R "$work_dir/unpacked/." "$install_dir/"
chmod 755 "$install_dir/void-yt" "$install_dir/tools/yt-dlp" "$install_dir/tools/qjs"
ln -sfn "$install_dir/void-yt" "$bin_dir/void-yt"

echo "Void-YT installed to $install_dir"
case ":$PATH:" in
    *":$bin_dir:"*) ;;
    *) echo "Add $bin_dir to PATH, then run: void-yt doctor" ;;
esac

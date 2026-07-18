#!/bin/sh
set -u

if [ "$#" -lt 4 ]; then
    exit 2
fi

repository=$1
parent_pid=$2
install_dir=$3
executable=$4
shift 4

restart_app() {
    rm -f "$0" 2>/dev/null || true
    exec "$executable" "$@"
}

while kill -0 "$parent_pid" 2>/dev/null; do
    sleep 0.2
done

os=$(uname -s)
machine=$(uname -m)
case "$os" in
    Linux)
        case "$machine" in
            x86_64|amd64) asset=void-yt-linux-x86_64.tar.gz ;;
            aarch64|arm64) asset=void-yt-linux-aarch64.tar.gz ;;
            *) echo "void-yt: automatic update does not support $machine" >&2; restart_app "$@" ;;
        esac
        ;;
    Darwin) asset=void-yt-macos-universal.tar.gz ;;
    *) echo "void-yt: automatic update does not support $os" >&2; restart_app "$@" ;;
esac

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/void-yt-update.XXXXXX") || restart_app "$@"
base_url=${VOID_YT_UPDATE_BASE_URL:-"https://github.com/$repository/releases/latest/download"}

if ! curl -fsL --retry 3 "$base_url/$asset" -o "$work_dir/$asset" ||
   ! curl -fsL --retry 3 "$base_url/checksums.sha256" -o "$work_dir/checksums.sha256"; then
    echo "void-yt: update download failed; continuing with the installed version" >&2
    rm -rf "$work_dir"
    restart_app "$@"
fi

expected=$(awk -v name="$asset" '$2 == name || $2 == "*" name { print $1; exit }' "$work_dir/checksums.sha256")
if command -v sha256sum >/dev/null 2>&1; then
    actual=$(sha256sum "$work_dir/$asset" | awk '{print $1}')
else
    actual=$(shasum -a 256 "$work_dir/$asset" | awk '{print $1}')
fi
if [ -z "$expected" ] || [ "$actual" != "$expected" ]; then
    echo "void-yt: update checksum verification failed; continuing with the installed version" >&2
    rm -rf "$work_dir"
    restart_app "$@"
fi

mkdir -p "$work_dir/unpacked"
if ! tar -C "$work_dir/unpacked" -xzf "$work_dir/$asset" ||
   ! cp -R "$work_dir/unpacked/." "$install_dir/"; then
    echo "void-yt: update installation failed; continuing with the installed version" >&2
    rm -rf "$work_dir"
    restart_app "$@"
fi

chmod 755 "$install_dir/void-yt" "$install_dir/tools/yt-dlp" \
    "$install_dir/tools/qjs" "$install_dir/tools/update.sh"
rm -rf "$work_dir"
echo "Void-YT updated successfully."
restart_app "$@"

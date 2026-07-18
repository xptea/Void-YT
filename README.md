# Void-YT

Void-YT is a small, open-source C terminal application for downloading media.
Official release archives bundle a pinned yt-dlp and QuickJS, so users do not
need Python, Node, Deno, or a separate yt-dlp installation.

FFmpeg is downloaded on demand instead of being bundled, keeping the initial
Void-YT install small. On the first `doctor`, `download`, or URL command,
Void-YT uses an existing FFmpeg from `PATH` when available. Otherwise it
downloads a pinned build from a provider linked by ffmpeg.org, verifies its
SHA-256 checksum and `ffmpeg -version`, and stores it in Void-YT's private
`tools` directory. It never changes the global system `PATH`.

> Only download media you are authorized to access. Void-YT does not bypass DRM.

## Install

The release workflow stamps each installer with this repository automatically.

Linux and macOS:

```sh
curl -fsSL https://github.com/xptea/Void-YT/releases/latest/download/install.sh | sh
```

Windows PowerShell:

```powershell
curl.exe -fsSL https://github.com/xptea/Void-YT/releases/latest/download/install.ps1 | Out-String | Invoke-Expression
```

The Unix installer uses `~/.local/share/void-yt` and creates
`~/.local/bin/void-yt`. The Windows installer uses
`%LOCALAPPDATA%\Void-YT` and adds that directory to the user's `PATH`. Both
installers verify the release archive against `checksums.sha256`.

## Usage

```text
void-yt doctor
void-yt "https://www.youtube.com/watch?v=..."
void-yt formats "https://www.youtube.com/watch?v=..."
void-yt download URL [additional yt-dlp options]
```

Additional arguments following the URL are passed directly to yt-dlp without
invoking a shell. When a URL is the only argument in an interactive terminal,
Void-YT loads the available resolutions and estimated sizes, shows an
arrow-key menu, and then prompts for a download folder. Press Enter to accept
the default `Downloads` folder, or paste another path. Choosing `audio only`
extracts an MP3. Supplying explicit yt-dlp options bypasses the menu so scripts
remain noninteractive.

## Automatic updates

On every normal launch, Void-YT requests the small `version.txt` file from the
latest GitHub Release. If a newer semantic version is available, it downloads
the correct platform archive, verifies its SHA-256 checksum, waits for the
current executable to exit, installs the update, and restarts the original
command. An offline or failed check never blocks normal use.

Set `VOID_YT_NO_UPDATE=1` to disable automatic update checks. Source builds that
do not include `tools/update.sh` or `tools/update.ps1` report the available
version but do not modify themselves.

Environment overrides:

- `VOID_YT_YTDLP`: path to a different yt-dlp executable.
- `VOID_YT_QJS`: path to a different `qjs` executable.
- `VOID_YT_FFMPEG`: path to an FFmpeg executable.
- `VOID_YT_NO_FFMPEG_INSTALL=1`: keep FFmpeg optional and use combined formats only.
- `VOID_YT_CURL`: path to the curl executable used for update checks.
- `VOID_YT_REPO`: installer repository override, such as `owner/Void-YT`.
- `VOID_YT_INSTALL_DIR`: installer destination override.

## Build from source

Requirements: a C17 compiler and CMake 3.20 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

A source build discovers yt-dlp and QuickJS from `PATH` or the environment
overrides above. The release packaging scripts download and checksum the pinned
standalone tools into a `tools` directory beside the Void-YT executable.

## Release process

Push a version tag such as `v0.3.0`. GitHub Actions will:

1. Build and test the C executable on Linux, macOS, and Windows.
2. Produce Linux x86-64/ARM64, universal macOS, and Windows x86-64 archives.
3. Add checksum-verified yt-dlp and QuickJS binaries.
4. Generate SHA-256 checksums, `version.txt`, and build-provenance attestations.
5. Publish the archives, updater metadata, and stamped installers to GitHub Releases.

Dependency versions and hashes are intentionally pinned in
`scripts/package.sh` and `scripts/package.ps1`. Updating either tool requires
updating its version and verified SHA-256 values together.

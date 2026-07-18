# Third-party notices

Release archives bundle the following independent command-line tools. They are
not linked into Void-YT.

## yt-dlp

- Project: https://github.com/yt-dlp/yt-dlp
- License: Unlicense, with additional bundled components covered by their own
  licenses.
- The official standalone build includes its complete third-party notices.

## QuickJS-NG

- Project: https://github.com/quickjs-ng/quickjs
- License: MIT
- Copyright belongs to the QuickJS and QuickJS-NG contributors.

## FFmpeg

FFmpeg is not included in the Void-YT release archive. If it is missing,
Void-YT downloads a pinned executable directly from a binary provider linked by
the official FFmpeg download page. The downloaded executable remains an
independent program used by yt-dlp for merging and post-processing.

- Project and source: https://ffmpeg.org/
- Download providers: https://ffmpeg.org/download.html
- License and legal information: https://ffmpeg.org/legal.html
- Windows and Linux provider: https://github.com/BtbN/FFmpeg-Builds
- macOS provider: https://evermeet.cx/ffmpeg/

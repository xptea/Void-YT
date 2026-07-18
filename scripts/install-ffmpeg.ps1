param(
    [Parameter(Mandatory = $true)][string]$ApplicationDirectory,
    [Parameter(Mandatory = $true)][string]$Curl
)

$ErrorActionPreference = "Stop"
$FfmpegVersion = "8.1.2"
$ArchiveName = "ffmpeg-n8.1.2-22-g94138f6973-win64-lgpl-8.1.zip"
$DefaultUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-07-17-13-22/$ArchiveName"
$DefaultSha256 = "66fdaf7e314968332c4c3fffbe730fedce47f9ac456ae3a04f73cd531080f4b3"
$ArchiveUrl = if ($env:VOID_YT_FFMPEG_URL) { $env:VOID_YT_FFMPEG_URL } else { $DefaultUrl }
$ExpectedSha256 = if ($env:VOID_YT_FFMPEG_SHA256) { $env:VOID_YT_FFMPEG_SHA256.ToLowerInvariant() } else { $DefaultSha256 }
$ToolsDirectory = Join-Path $ApplicationDirectory "tools"
$Target = Join-Path $ToolsDirectory "ffmpeg.exe"
$Pending = Join-Path $ToolsDirectory "ffmpeg.pending.exe"
$WorkDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("void-yt-ffmpeg-" + [guid]::NewGuid())
$Archive = Join-Path $WorkDirectory $ArchiveName
$Unpacked = Join-Path $WorkDirectory "unpacked"

try {
    New-Item -ItemType Directory -Path $Unpacked -Force | Out-Null
    New-Item -ItemType Directory -Path $ToolsDirectory -Force | Out-Null
    Write-Host "Downloading FFmpeg $FfmpegVersion for Windows x86-64 (about 139 MB)..."
    & $Curl -fL --retry 3 --connect-timeout 10 --output $Archive -- $ArchiveUrl
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg download failed with curl exit code $LASTEXITCODE"
    }

    $ActualSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Archive).Hash.ToLowerInvariant()
    if ($ActualSha256 -ne $ExpectedSha256) {
        throw "FFmpeg checksum verification failed"
    }

    Expand-Archive -LiteralPath $Archive -DestinationPath $Unpacked -Force
    $Candidate = Get-ChildItem -LiteralPath $Unpacked -Recurse -File -Filter "ffmpeg.exe" |
        Where-Object { $_.FullName -match "[\\/]bin[\\/]ffmpeg\.exe$" } |
        Select-Object -First 1
    if (-not $Candidate) {
        throw "The FFmpeg archive did not contain bin/ffmpeg.exe"
    }

    Copy-Item -LiteralPath $Candidate.FullName -Destination $Pending -Force
    & $Pending -version *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "The downloaded FFmpeg executable failed its version check"
    }
    Move-Item -LiteralPath $Pending -Destination $Target -Force
    @"
FFmpeg: $FfmpegVersion
Provider: BtbN/FFmpeg-Builds (linked by ffmpeg.org)
Source: $DefaultUrl
Archive SHA-256: $DefaultSha256
License information: https://ffmpeg.org/legal.html
"@ | Set-Content -Encoding utf8 (Join-Path $ToolsDirectory "ffmpeg-source.txt")
    Write-Host "FFmpeg $FfmpegVersion installed and verified."
}
finally {
    if (Test-Path -LiteralPath $Pending) {
        Remove-Item -LiteralPath $Pending -Force
    }
    if (Test-Path -LiteralPath $WorkDirectory) {
        Remove-Item -LiteralPath $WorkDirectory -Recurse -Force
    }
}

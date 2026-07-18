param(
    [Parameter(Mandatory = $true)][string]$Binary,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$YtDlpVersion = "2026.07.04"
$QuickJsVersion = "0.15.0"
$YtDlpHash = "52fe3c26dcf71fbdc85b528589020bb0b8e383155cfa81b64dd447bbe35e24b8"
$QuickJsHash = "f157d58a9e14e958991e4b0f01b3a6d1d7dc25f3ae78f85c6c8da01c19bf77bf"
$ProjectDirectory = Split-Path -Parent $PSScriptRoot
$WorkDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("void-yt-package-" + [guid]::NewGuid())
$Stage = Join-Path $WorkDirectory "stage"

try {
    New-Item -ItemType Directory -Path (Join-Path $Stage "tools") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $Stage "licenses") -Force | Out-Null
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    Copy-Item -LiteralPath $Binary -Destination (Join-Path $Stage "void-yt.exe")
    Copy-Item -LiteralPath (Join-Path $ProjectDirectory "LICENSE") -Destination $Stage
    Copy-Item -LiteralPath (Join-Path $ProjectDirectory "THIRD_PARTY_NOTICES.md") -Destination $Stage
    Copy-Item -LiteralPath (Join-Path $ProjectDirectory "licenses\quickjs-ng.txt") -Destination (Join-Path $Stage "licenses")
    Copy-Item -LiteralPath (Join-Path $ProjectDirectory "licenses\yt-dlp.txt") -Destination (Join-Path $Stage "licenses")

    $YtDlpPath = Join-Path $Stage "tools\yt-dlp.exe"
    $QuickJsPath = Join-Path $Stage "tools\qjs.exe"
    Invoke-WebRequest -Uri "https://github.com/yt-dlp/yt-dlp/releases/download/$YtDlpVersion/yt-dlp.exe" -OutFile $YtDlpPath
    Invoke-WebRequest -Uri "https://github.com/quickjs-ng/quickjs/releases/download/v$QuickJsVersion/qjs-windows-x86_64.exe" -OutFile $QuickJsPath

    if ((Get-FileHash -Algorithm SHA256 $YtDlpPath).Hash.ToLowerInvariant() -ne $YtDlpHash) {
        throw "yt-dlp checksum mismatch"
    }
    if ((Get-FileHash -Algorithm SHA256 $QuickJsPath).Hash.ToLowerInvariant() -ne $QuickJsHash) {
        throw "QuickJS checksum mismatch"
    }

    @"
Void-YT: $Version
yt-dlp: $YtDlpVersion
QuickJS-NG: $QuickJsVersion
Target: windows-x86_64
"@ | Set-Content -Encoding utf8 (Join-Path $Stage "BUILD_INFO.txt")

    $Archive = Join-Path $OutputDirectory "void-yt-windows-x86_64.zip"
    Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $Archive -Force
    Write-Output $Archive
}
finally {
    if (Test-Path -LiteralPath $WorkDirectory) {
        Remove-Item -LiteralPath $WorkDirectory -Recurse -Force
    }
}

param(
    [Parameter(Mandatory = $true)][string]$Repository,
    [Parameter(Mandatory = $true)][int]$ParentPid,
    [Parameter(Mandatory = $true)][string]$InstallDirectory,
    [Parameter(Mandatory = $true)][string]$Executable
)

$ErrorActionPreference = "Stop"
$Asset = "void-yt-windows-x86_64.zip"
$WorkDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("void-yt-update-" + [guid]::NewGuid())
$RestartCommandLine = $env:VOID_YT_RESTART_COMMAND_LINE

try {
    $Parent = Get-Process -Id $ParentPid -ErrorAction SilentlyContinue
    if ($Parent) { $Parent | Wait-Process }

    $Archive = Join-Path $WorkDirectory $Asset
    $Checksums = Join-Path $WorkDirectory "checksums.sha256"
    $Unpacked = Join-Path $WorkDirectory "unpacked"
    $BaseUrl = if ($env:VOID_YT_UPDATE_BASE_URL) {
        $env:VOID_YT_UPDATE_BASE_URL.TrimEnd("/")
    } else {
        "https://github.com/$Repository/releases/latest/download"
    }
    New-Item -ItemType Directory -Path $Unpacked -Force | Out-Null

    Invoke-WebRequest -Uri "$BaseUrl/$Asset" -OutFile $Archive
    Invoke-WebRequest -Uri "$BaseUrl/checksums.sha256" -OutFile $Checksums

    $EscapedAsset = [regex]::Escape($Asset)
    $Match = Select-String -Path $Checksums -Pattern "^([a-fA-F0-9]{64})\s+\*?$EscapedAsset$" | Select-Object -First 1
    if (-not $Match) { throw "Release checksum is missing for $Asset" }
    $Expected = $Match.Matches[0].Groups[1].Value.ToLowerInvariant()
    $Actual = (Get-FileHash -Algorithm SHA256 $Archive).Hash.ToLowerInvariant()
    if ($Actual -ne $Expected) { throw "Update checksum verification failed" }

    Expand-Archive -LiteralPath $Archive -DestinationPath $Unpacked -Force
    Copy-Item -Path (Join-Path $Unpacked "*") -Destination $InstallDirectory -Recurse -Force
    Write-Host "Void-YT updated successfully."
}
catch {
    Write-Warning "Automatic update failed: $($_.Exception.Message). Continuing with the installed version."
}
finally {
    if (Test-Path -LiteralPath $WorkDirectory) {
        Remove-Item -LiteralPath $WorkDirectory -Recurse -Force
    }
    Remove-Item Env:VOID_YT_RESTART_COMMAND_LINE -ErrorAction SilentlyContinue
}

if ($RestartCommandLine) {
    $Process = Start-Process -FilePath $Executable -ArgumentList $RestartCommandLine -NoNewWindow -Wait -PassThru
} else {
    $Process = Start-Process -FilePath $Executable -NoNewWindow -Wait -PassThru
}
Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue
exit $Process.ExitCode

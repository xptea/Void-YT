$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$Repository = if ($env:VOID_YT_REPO) { $env:VOID_YT_REPO } else { "@GITHUB_REPOSITORY@" }
$InstallDirectory = if ($env:VOID_YT_INSTALL_DIR) { $env:VOID_YT_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA "Void-YT" }
$Asset = "void-yt-windows-x86_64.zip"

if ($Repository.StartsWith("@")) {
    throw "Installer has not been stamped. Set VOID_YT_REPO=owner/repository and run again."
}
$WorkDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("void-yt-install-" + [guid]::NewGuid())
$Archive = Join-Path $WorkDirectory $Asset
$Checksums = Join-Path $WorkDirectory "checksums.sha256"
$Unpacked = Join-Path $WorkDirectory "unpacked"
$BaseUrl = "https://github.com/$Repository/releases/latest/download"

try {
    Write-Host "Installing Void-YT..."
    New-Item -ItemType Directory -Path $Unpacked -Force | Out-Null
    Invoke-WebRequest -Uri "$BaseUrl/$Asset" -OutFile $Archive
    Invoke-WebRequest -Uri "$BaseUrl/checksums.sha256" -OutFile $Checksums

    $EscapedAsset = [regex]::Escape($Asset)
    $Match = Select-String -Path $Checksums -Pattern "^([a-fA-F0-9]{64})\s+\*?$EscapedAsset$" | Select-Object -First 1
    if (-not $Match) { throw "Release checksum is missing for $Asset" }
    $Expected = $Match.Matches[0].Groups[1].Value.ToLowerInvariant()
    $Actual = (Get-FileHash -Algorithm SHA256 $Archive).Hash.ToLowerInvariant()
    if ($Actual -ne $Expected) { throw "Checksum verification failed for $Asset" }

    Expand-Archive -LiteralPath $Archive -DestinationPath $Unpacked -Force
    New-Item -ItemType Directory -Path $InstallDirectory -Force | Out-Null
    Copy-Item -Path (Join-Path $Unpacked "*") -Destination $InstallDirectory -Recurse -Force

    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $PathEntries = @($UserPath -split ";" | Where-Object { $_ })
    if ($InstallDirectory -notin $PathEntries) {
        $NewPath = (($PathEntries + $InstallDirectory) -join ";")
        [Environment]::SetEnvironmentVariable("Path", $NewPath, "User")
        $env:Path += ";$InstallDirectory"
    }

    Write-Host "Setting up media tools (one-time download)..."
    $PreviousNoUpdate = $env:VOID_YT_NO_UPDATE
    $PreviousErrorActionPreference = $ErrorActionPreference
    try {
        $env:VOID_YT_NO_UPDATE = "1"
        # Windows PowerShell 5.1 wraps native stderr as ErrorRecord objects.
        # Capture those records and trust the executable's exit code instead.
        $ErrorActionPreference = "Continue"
        $DoctorLines = & (Join-Path $InstallDirectory "void-yt.exe") doctor 2>&1
        $DoctorExitCode = $LASTEXITCODE
        $DoctorOutput = $DoctorLines | Out-String
    }
    finally {
        if ($null -eq $PreviousNoUpdate) {
            Remove-Item Env:\VOID_YT_NO_UPDATE -ErrorAction SilentlyContinue
        }
        else {
            $env:VOID_YT_NO_UPDATE = $PreviousNoUpdate
        }
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
    if ($DoctorExitCode -ne 0 -or $DoctorOutput -notmatch "(?m)^\[ok\]\s+FFmpeg\s+") {
        throw "Media tool setup failed.`n$DoctorOutput"
    }

    Write-Host "Void-YT is ready."
    Write-Host 'Run: void-yt "youtube URL"'
}
finally {
    if (Test-Path -LiteralPath $WorkDirectory) {
        Remove-Item -LiteralPath $WorkDirectory -Recurse -Force
    }
}

# Release build for STFU: the zip players install.
#
#   1. Refuses a working tree with uncommitted changes (a release is a commit), unless -allowDirty.
#   2. Builds everything with .\Build_Local.ps1 -noDeploy (plugin, web UI, Papyrus), unless -skipBuild.
#   3. Stages build\package\stage in the mod's layout: SKSE\Plugins\STFU.dll, STFU.esp, Scripts, Source\Scripts,
#      Sound\STFU, PrismaUI\views\STFU (from web-ui\dist).
#   4. Checks what it staged: every .psc has a .pex no older than it, index.html's bundles are exactly the ones
#      staged, and vcpkg.json's version matches CMakeLists.txt's.
#   5. Zips it: build\package\STFU v<version>.zip. (Not build\release: paths are case-insensitive, and that is
#      MSBuild's build\Release output folder.)
#
# Usage:
#   .\Build_Release.ps1                # build, check, zip
#   .\Build_Release.ps1 -skipBuild     # pack the last build as it is
#   .\Build_Release.ps1 -allowDirty    # a test release from uncommitted work
#
# The outcome is printed and written to %TEMP%\stfu-release-result.json. See docs/DEVELOPMENT.md#releases.

#Requires -Version 7

param(
    [string]$config = "Release",
    [switch]$skipBuild,
    [switch]$allowDirty
)
$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot
Add-Type -AssemblyName System.IO.Compression.FileSystem

$resultFile = Join-Path $env:TEMP "stfu-release-result.json"
function Complete-Release {
    param([string]$Status, [string]$Message, [string]$Archive = "")
    $color = if ($Status -eq 'SUCCESS') { 'Green' } else { 'Red' }
    Write-Host ""
    Write-Host "==================== RELEASE $Status ====================" -ForegroundColor $color
    if ($Archive) { Write-Host "  Archive: $Archive" -ForegroundColor $color }
    if ($Message) { Write-Host "  $Message" -ForegroundColor $color }
    Write-Host "=========================================================" -ForegroundColor $color
    @{ status = $Status; message = $Message; archive = $Archive; finished = (Get-Date -Format o) } |
        ConvertTo-Json | Set-Content -LiteralPath $resultFile -Encoding UTF8
    exit $(if ($Status -eq 'SUCCESS') { 0 } else { 1 })
}
trap { Complete-Release -Status 'FAILURE' -Message $_.Exception.Message }

# --- 1. A commit ---------------------------------------------------------------
$commit = (git rev-parse --short HEAD).Trim()
$dirty = git status --porcelain
if ($dirty) {
    if (-not $allowDirty) { Complete-Release -Status 'FAILURE' -Message "Uncommitted changes: commit first, or -allowDirty for a test release" }
    Write-Host "Uncommitted changes: a test release, not commit $commit's." -ForegroundColor Yellow
    $commit += "-dirty"
}
$version = (Select-String -Path "CMakeLists.txt" -Pattern '^\s*VERSION\s+([\d.]+)').Matches[0].Groups[1].Value
Write-Host "STFU $version ($commit)" -ForegroundColor Cyan

# --- 2. Build ------------------------------------------------------------------
if (-not $skipBuild) {
    & (Join-Path $PSScriptRoot "Build_Local.ps1") -noDeploy -config $config
    if ($LASTEXITCODE -ne 0) { Complete-Release -Status 'FAILURE' -Message "Build_Local.ps1 failed (see above)" }
}

# --- 3. Stage --------------------------------------------------------------------
$release = Join-Path $PSScriptRoot "build\package"
$stage = Join-Path $release "stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$files = [ordered]@{
    "SKSE\Plugins\STFU.dll" = "build\$config\STFU.dll"
    "STFU.esp"              = "STFU.esp"
}
$folders = [ordered]@{
    "Scripts"             = "Scripts"
    "Source\Scripts"      = "Source\Scripts"
    "Sound\STFU"          = "Sound\STFU"
    "PrismaUI\views\STFU" = "web-ui\dist"
}
foreach ($to in $files.Keys) {
    if (-not (Test-Path -LiteralPath $files[$to])) { throw "Missing $($files[$to]): build first" }
    New-Item -ItemType Directory -Force -Path (Split-Path (Join-Path $stage $to)) | Out-Null
    Copy-Item -LiteralPath $files[$to] -Destination (Join-Path $stage $to)
}
foreach ($to in $folders.Keys) {
    if (-not (Test-Path -LiteralPath $folders[$to])) { throw "Missing $($folders[$to])" }
    New-Item -ItemType Directory -Force -Path (Join-Path $stage $to) | Out-Null
    Copy-Item -Path (Join-Path $folders[$to] "*") -Destination (Join-Path $stage $to) -Recurse
}

# --- 4. Check --------------------------------------------------------------------
$problems = @()

# Compiled .pex files are gitignored, so they're whatever was last compiled locally: each must exist and be current.
foreach ($psc in Get-ChildItem (Join-Path $stage "Source\Scripts\*.psc")) {
    $pex = Join-Path $stage "Scripts\$($psc.BaseName).pex"
    if (-not (Test-Path -LiteralPath $pex)) { $problems += "No .pex for $($psc.Name)"; continue }
    if ((Get-Item -LiteralPath $pex).LastWriteTime -lt $psc.LastWriteTime) { $problems += "$($psc.BaseName).pex is older than its .psc (recompile it)" }
}

# The menu: index.html must load bundles that are staged, and nothing else should be in assets\.
$view = Join-Path $stage "PrismaUI\views\STFU"
$referenced = [regex]::Matches((Get-Content -LiteralPath (Join-Path $view "index.html") -Raw), 'assets/[^"'']+') | ForEach-Object { $_.Value -replace '/', '\' }
$staged = Get-ChildItem (Join-Path $view "assets") -File | ForEach-Object { "assets\$($_.Name)" }
foreach ($ref in $referenced) { if ($staged -notcontains $ref) { $problems += "index.html loads $ref, which is not in the package" } }
foreach ($file in $staged) { if ($referenced -notcontains $file) { $problems += "$file is not used by index.html (stale bundle)" } }

$vcpkgVersion = (Get-Content -LiteralPath "vcpkg.json" -Raw | ConvertFrom-Json).'version-string'
if ($vcpkgVersion -ne $version) { $problems += "vcpkg.json says $vcpkgVersion, CMakeLists.txt says $version" }

if ($problems) { Complete-Release -Status 'FAILURE' -Message ("Checks failed:`n  - " + ($problems -join "`n  - ")) }

# --- 5. Zip ----------------------------------------------------------------------
$archive = Join-Path $release "STFU v$version.zip"
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $archive, [IO.Compression.CompressionLevel]::Optimal, $false)
$count = (Get-ChildItem $stage -Recurse -File).Count
$size = "{0:N0} KB" -f ((Get-Item -LiteralPath $archive).Length / 1KB)
Complete-Release -Status 'SUCCESS' -Archive $archive -Message "$count files, $size, from $commit"

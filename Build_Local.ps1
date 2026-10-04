# Incremental plugin build (never /t:Rebuild: it rebuilds all of CommonLib), web UI, Papyrus, deploy to every instance.
# Switches and config: docs/DEVELOPMENT.md#build-script.  PASS/FAIL also goes to %TEMP%\stfu-build-result.json.

#Requires -Version 7
# (npm and Pyro log to stderr, which Windows PowerShell 5.1 turns into a terminating error under "Stop".)

param(
    [string]$preset = "vs2022-windows",
    [string]$config = "Release",
    [int]$threads,
    [switch]$noDeploy,
    [switch]$skipWeb,
    [switch]$skipScripts,
    [switch]$fresh
)
$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$target   = "STFU"
$dllName  = "STFU.dll"
$builtDll = Join-Path $PSScriptRoot "build\$config\$dllName"
$espName  = "STFU.esp"
$webDir   = Join-Path $PSScriptRoot "web-ui"
$webDist  = Join-Path $webDir "dist"

# Folders STFU owns inside a deployed mod (source in the repo -> path in the mod). Mirrored (/MIR), so a file removed
# from the source leaves the deploy too: old hashed menu bundles and stale .pex files don't pile up.
$mirroredFolders = [ordered]@{
    "Scripts"        = "Scripts"
    "Source\Scripts" = "Source\Scripts"
    "Sound\STFU"     = "Sound\STFU"
    "web-ui\dist"    = "PrismaUI\views\STFU"
}

# --- Build-result reporting -------------------------------------------------
$scriptStartTime = Get-Date
$buildResultFile = Join-Path $env:TEMP "stfu-build-result.json"
$script:deployed     = @()
$script:deployFailed = @()

function Write-BuildSummary {
    param([string]$Status, [string]$Stage, [int]$Code = 0, [string]$Message = "")
    $finish  = Get-Date
    $elapsed = [math]::Round(($finish - $scriptStartTime).TotalSeconds, 1)
    $color   = if ($Status -eq 'SUCCESS') { 'Green' } else { 'Red' }

    Write-Host ""
    Write-Host "==================== BUILD $Status ====================" -ForegroundColor $color
    Write-Host ("  Finished: {0}" -f $finish.ToString("yyyy-MM-dd HH:mm:ss")) -ForegroundColor $color
    Write-Host ("  Elapsed:  {0}s" -f $elapsed) -ForegroundColor $color
    if ($Stage)      { Write-Host ("  Stage:    {0}" -f $Stage) -ForegroundColor $color }
    if ($Code -ne 0) { Write-Host ("  ExitCode: {0}" -f $Code) -ForegroundColor $color }
    foreach ($d in $script:deployed)     { Write-Host ("  Deployed: {0}" -f $d) -ForegroundColor $color }
    foreach ($d in $script:deployFailed) { Write-Host ("  FAILED:   {0}" -f $d) -ForegroundColor Red }
    if ($Message)    { Write-Host ("  Detail:   {0}" -f $Message) -ForegroundColor $color }
    Write-Host "=======================================================" -ForegroundColor $color

    $payload = [ordered]@{
        status         = $Status
        stage          = $Stage
        exitCode       = $Code
        message        = $Message
        deployedTo     = $script:deployed
        deployFailed   = $script:deployFailed
        finishedAt     = $finish.ToString("o")
        elapsedSeconds = $elapsed
    }
    try { $payload | ConvertTo-Json | Set-Content -LiteralPath $buildResultFile -Encoding UTF8 } catch { }
}

function Complete-Build {
    param([string]$Status, [string]$Stage, [int]$Code = 0, [string]$Message = "")
    Write-BuildSummary -Status $Status -Stage $Stage -Code $Code -Message $Message
    if ($Status -eq 'SUCCESS') { exit 0 }
    exit ($(if ($Code -ne 0) { $Code } else { 1 }))
}

trap {
    Write-BuildSummary -Status 'FAILURE' -Stage 'exception' -Code 1 -Message $_.Exception.Message
    break
}

# --- Configuration ------------------------------------------------------------
# Machine-specific settings live in Build_Config_Local.ps1 (committed with empty values; fill it in locally).
$defaultOutputPath     = ""
$additionalOutputPaths = @()
$ckPath                = ""
$pyroPath              = ""
$defaultThreads        = 16
if (Test-Path .\Build_Config_Local.ps1) { . .\Build_Config_Local.ps1 }
if ($env:STFU_OUTPUT_PATH) { $defaultOutputPath = $env:STFU_OUTPUT_PATH }
if ($env:STFU_CK_PATH)     { $ckPath = $env:STFU_CK_PATH }
if (-not $threads)         { $threads = $defaultThreads }

# --- Configure (only when needed) ---------------------------------------------
# The VS generator's ZERO_CHECK reconfigures itself when CMakeLists changes; configure only on a first build or -fresh.
if ($fresh -or -not (Test-Path "build\CMakeCache.txt")) {
    $cmakeArgs = @("--preset", $preset, "-Wno-dev")
    if ($fresh) { $cmakeArgs += "--fresh" }
    Write-Host "Configuring ($preset)..." -ForegroundColor Cyan
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { Complete-Build -Status 'FAILURE' -Stage 'configure' -Code $LASTEXITCODE }
}

# --- Build (plugin target only, incremental) -----------------------------------
Write-Host "Building $target ($config, $threads threads)..." -ForegroundColor Cyan
& cmake --build build --config $config --target $target --parallel $threads
if ($LASTEXITCODE -ne 0) { Complete-Build -Status 'FAILURE' -Stage 'build' -Code $LASTEXITCODE }
if (-not (Test-Path -LiteralPath $builtDll)) { Complete-Build -Status 'FAILURE' -Stage 'build' -Message "Build reported success but $builtDll is missing." }

# --- Web UI (npm, only when its sources are newer than dist) --------------------
if (-not $skipWeb) {
    $distIndex = Join-Path $webDist "index.html"
    $webSources = @(Get-ChildItem (Join-Path $webDir "src") -Recurse -File) +
                  @(Get-ChildItem $webDir -File | Where-Object { $_.Name -match '^(index\.html|package(-lock)?\.json|vite\.config\.\w+|tsconfig.*\.json|tailwind\.config\.\w+|postcss\.config\.\w+)$' })
    $newestSource = $webSources | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not (Test-Path -LiteralPath $distIndex) -or (Get-Item -LiteralPath $distIndex).LastWriteTime -lt $newestSource.LastWriteTime) {
        Push-Location -LiteralPath $webDir
        try {
            if (-not (Test-Path "node_modules")) {
                Write-Host "Installing web UI dependencies (npm ci)..." -ForegroundColor Cyan
                $npmOut = & npm ci 2>&1
                if ($LASTEXITCODE -ne 0) { $npmOut | ForEach-Object { Write-Host "  $_" }; Complete-Build -Status 'FAILURE' -Stage 'web' -Code $LASTEXITCODE -Message "npm ci failed" }
            }
            Write-Host "Building web UI (npm run build)..." -ForegroundColor Cyan
            $npmOut = & npm run build 2>&1
            if ($LASTEXITCODE -ne 0) { $npmOut | ForEach-Object { Write-Host "  $_" }; Complete-Build -Status 'FAILURE' -Stage 'web' -Code $LASTEXITCODE -Message "npm run build failed" }
        } finally {
            Pop-Location
        }
    } else {
        Write-Host "Web UI is up to date." -ForegroundColor DarkGray
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $webDist "index.html"))) {
    Complete-Build -Status 'FAILURE' -Stage 'web' -Message "web-ui\dist\index.html is missing (build the web UI or drop -skipWeb)."
}

# --- Papyrus (Pyro, only if a project file exists) ------------------------------
if (-not $skipScripts) {
    if (Test-Path skyrimse.ppj) {
        if (-not $pyroPath) {
            # Newest papyrus-lang extension install wins (the folder name carries the version).
            $pyroPath = Get-ChildItem "$env:USERPROFILE\.vscode\extensions\joelday.papyrus-lang-vscode-*\pyro\pyro.exe" -ErrorAction SilentlyContinue |
                        Sort-Object { $_.Directory.Parent.LastWriteTime } -Descending | Select-Object -First 1 -ExpandProperty FullName
        }
        if (-not $pyroPath -or -not (Test-Path -LiteralPath $pyroPath)) {
            Complete-Build -Status 'FAILURE' -Stage 'scripts' -Message "pyro.exe not found. Install the papyrus-lang VS Code extension, set `$pyroPath in Build_Config_Local.ps1, or pass -skipScripts."
        }
        if (-not $ckPath -or -not (Test-Path -LiteralPath $ckPath)) {
            Complete-Build -Status 'FAILURE' -Stage 'scripts' -Message "Creation Kit path not set or missing ('$ckPath'). Set `$ckPath in Build_Config_Local.ps1 or STFU_CK_PATH."
        }

        Write-Host "Compiling Papyrus (Pyro, skyrimse.ppj)..." -ForegroundColor Cyan
        $pyroOut = & $pyroPath "skyrimse.ppj" "--game-path" $ckPath 2>&1
        $pyroExit = $LASTEXITCODE
        $pyroErrors = $pyroOut | Where-Object { "$_" -match '(?i)\berror\b|failed' -and "$_" -notmatch '\b0 (error|failed)' }
        if ($pyroExit -ne 0 -or $pyroErrors) {
            $pyroOut | ForEach-Object { Write-Host "  $_" }
            Complete-Build -Status 'FAILURE' -Stage 'scripts' -Code $pyroExit -Message (($pyroErrors | Select-Object -First 3) -join ' | ')
        }
        # Pyro is incremental: "No scripts were compiled." just means nothing changed.
        $pyroSummary = $pyroOut | Where-Object { "$_" -match 'scripts were compiled|Compil\w+ \d+|succeeded' } | Select-Object -Last 1
        if ($pyroSummary) { Write-Host ("  " + ("$pyroSummary" -replace '^.*\[INFO\]\s*', '')) -ForegroundColor DarkGray }
    } else {
        Write-Host "No skyrimse.ppj - Papyrus not compiled (the existing Scripts\*.pex are deployed as they are)." -ForegroundColor DarkGray
    }

    # Every .psc must have a .pex (compiled .pex files are gitignored, so a fresh clone has none).
    $missing = Get-ChildItem "Source\Scripts\*.psc" | Where-Object { -not (Test-Path -LiteralPath (Join-Path "Scripts" ($_.BaseName + ".pex"))) }
    if ($missing) { Complete-Build -Status 'FAILURE' -Stage 'scripts' -Message ("No .pex for: " + (($missing | ForEach-Object BaseName) -join ', ')) }
}

# --- Deploy ------------------------------------------------------------------
function Deploy-To {
    param([string]$dest)
    Write-Host "Deploying to: $dest" -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path (Join-Path $dest "SKSE\Plugins") | Out-Null

    # The DLL is locked while that instance's game is running - report it, but still
    # copy everything else: a menu or script change can be tested without restarting.
    $dllLocked = $false
    try {
        Copy-Item -LiteralPath $builtDll -Destination (Join-Path $dest "SKSE\Plugins\$dllName") -Force
    } catch {
        $dllLocked = $true
    }

    # A deployed .esp newer than the repo's was edited there (CK, xEdit): overwriting it would lose
    # the edits. Copy it back into the repo first, or delete it to take the repo's.
    $deployedEsp = Join-Path $dest $espName
    if ((Test-Path -LiteralPath $deployedEsp) -and
        (Get-Item -LiteralPath $deployedEsp).LastWriteTime -gt (Get-Item -LiteralPath $espName).LastWriteTime) {
        $script:deployFailed += "$dest ($espName was edited there; copy it back into the repo, or delete it to take the repo's)"
    } else {
        try { Copy-Item -LiteralPath $espName -Destination $deployedEsp -Force } catch { $dllLocked = $true }
    }

    foreach ($source in $mirroredFolders.Keys) {
        if (-not (Test-Path -LiteralPath $source)) { continue }
        robocopy $source (Join-Path $dest $mirroredFolders[$source]) /MIR /XF meta.ini /NFL /NDL /NJH /NJS /NP /R:2 /W:2 | Out-Null
        # robocopy exit codes 0-7 are success; 8+ is a real failure.
        if ($LASTEXITCODE -ge 8) {
            $script:deployFailed += "$dest (robocopy $source exit $LASTEXITCODE)"
            return
        }
    }
    if ($dllLocked) {
        $script:deployFailed += "$dest (DLL/ESP locked - is that instance's game running? Other files were updated)"
        return
    }
    $script:deployed += $dest
}

if ($noDeploy) {
    Write-Host "-noDeploy set - skipping deploy." -ForegroundColor DarkGray
} else {
    $targets = @($defaultOutputPath) + $additionalOutputPaths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique
    if (-not $targets) {
        Write-Host "No output paths configured (Build_Config_Local.ps1) - nothing to deploy." -ForegroundColor Yellow
    }
    foreach ($dest in $targets) { Deploy-To $dest }
    if ($script:deployFailed) {
        Complete-Build -Status 'FAILURE' -Stage 'deploy' -Message "$($script:deployFailed.Count) instance(s) not updated."
    }
}

Complete-Build -Status 'SUCCESS' -Stage 'complete'

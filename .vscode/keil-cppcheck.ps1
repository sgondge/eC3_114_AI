<#
    Runs cppcheck on the C sources of a Keil uVision project, using the project's own defines and include paths.

    Usage:
        keil-cppcheck.ps1 -Project MDK_ARMv5\Project_54253.uvprojx [-Full] [-Keil]

    -Full discards the incremental cache (cppcheck\build\<name>) and re-analyses every file.
    -Keil is used by the uVision "After Build" user command: messages use uVision's "<file>(<line>):" format
          and the exit code is always 0 (KEIL_CPPCHECK_STRICT=1 restores it). Skipped when the build is driven
          by keil-build.ps1, which runs cppcheck itself.
    Settings: keil-cppcheck.json. Project suppressions: cppcheck\suppressions.txt.
    Reports:  cppcheck\reports\<name>_cppcheck.xml and .txt.

    Findings are printed as "<file>(<line>,<col>): error|warning|info: [cppcheck] <id>: <message>"
    for the task problem matcher.

    Exit code: 0 = clean / style only, 1 = warnings, 2 = errors, 0 also when cppcheck is not installed.
#>
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [switch]$Full,
    [switch]$Keil
)

$ErrorActionPreference = 'Stop'

if ($Keil -and ($env:KEIL_CPPCHECK_SKIP_UV -eq '1' -or $env:KEIL_CPPCHECK -eq '0')) { exit 0 }

$root = Split-Path $PSScriptRoot -Parent
$cfg = Get-Content (Join-Path $PSScriptRoot 'keil-cppcheck.json') -Raw | ConvertFrom-Json

$exe = @($env:CPPCHECK_EXE, $cfg.exe) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $exe) {
    $cmd = Get-Command cppcheck -ErrorAction SilentlyContinue
    if ($cmd) { $exe = $cmd.Source }
}
if (-not $exe) {
    Write-Host 'cppcheck not found (set CPPCHECK_EXE or "exe" in keil-cppcheck.json); static analysis skipped.'
    exit 0
}

$projectPath = (Resolve-Path $Project).Path
$projectDir = Split-Path $projectPath -Parent
$projectName = Split-Path $projectPath -Leaf
$name = $cfg.projects.$projectName
if (-not $name) { $name = [System.IO.Path]::GetFileNameWithoutExtension($projectPath) }

function SplitList([string]$s, [string]$sep) {
    if (-not $s) { return @() }
    @($s -split $sep | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}
function FullPath([string]$p) { [System.IO.Path]::GetFullPath((Join-Path $projectDir $p)).TrimEnd('\') }

# ---- Collect sources, defines and include paths from the uvprojx -------------------------------
[xml]$uv = Get-Content $projectPath
$tgt = $uv.Project.Targets.Target | Select-Object -First 1
$ads = $tgt.TargetOption.TargetArmAds

$defines = New-Object System.Collections.Generic.List[string]
$includes = New-Object System.Collections.Generic.List[string]
function AddDefines([string]$s) { foreach ($d in (SplitList $s '[,\s]+')) { if (-not $defines.Contains($d)) { $defines.Add($d) } } }
function AddIncludes([string]$s) { foreach ($i in (SplitList $s ';')) { $p = FullPath $i; if (-not $includes.Contains($p)) { $includes.Add($p) } } }

AddDefines ([string]$ads.Cads.VariousControls.Define)
AddIncludes ([string]$ads.Cads.VariousControls.IncludePath)
foreach ($d in $cfg.extraDefines) { AddDefines $d }
if (([string]$ads.ArmAdsMisc.useUlib) -eq '1') { AddDefines '__MICROLIB' }

$exclude = [string]$cfg.excludeSourcePattern
$sources = New-Object System.Collections.Generic.List[string]
foreach ($g in $tgt.Groups.Group) {
    if (([string]$g.GroupOption.CommonProperty.IncludeInBuild) -eq '0') { continue }
    AddDefines ([string]$g.GroupOption.GroupArmAds.Cads.VariousControls.Define)
    AddIncludes ([string]$g.GroupOption.GroupArmAds.Cads.VariousControls.IncludePath)
    foreach ($f in $g.Files.File) {
        if ([string]$f.FileType -ne '1') { continue }
        if (([string]$f.FileOption.CommonProperty.IncludeInBuild) -eq '0') { continue }
        AddDefines ([string]$f.FileOption.FileArmAds.Cads.VariousControls.Define)
        AddIncludes ([string]$f.FileOption.FileArmAds.Cads.VariousControls.IncludePath)
        $src = FullPath $f.FilePath
        if ($exclude -and $src -match $exclude) { continue }
        if ((Test-Path $src) -and -not $sources.Contains($src)) { $sources.Add($src) }
    }
}

if (-not $sources.Count) {
    Write-Host "cppcheck: no C sources to analyse in $projectName."
    exit 0
}

# ---- Run cppcheck ------------------------------------------------------------------------------
$buildDir = Join-Path $root "cppcheck\build\$name"
$reportDir = Join-Path $root 'cppcheck\reports'
if ($Full -and (Test-Path $buildDir)) { Remove-Item $buildDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $buildDir, $reportDir | Out-Null

$fileList = Join-Path $buildDir 'files.txt'
[System.IO.File]::WriteAllLines($fileList, $sources)
$xmlReport = Join-Path $reportDir "${name}_cppcheck.xml"
$txtReport = Join-Path $reportDir "${name}_cppcheck.txt"

$cpArgs = @(
    '--quiet', '--language=c', "--std=$($cfg.std)", "--platform=$($cfg.platform)",
    "--enable=$($cfg.enable)", '--inline-suppr', '--xml', "--output-file=$xmlReport",
    "--cppcheck-build-dir=$buildDir", "-j$([Environment]::ProcessorCount)", "--file-list=$fileList"
)
$cpArgs += $defines | ForEach-Object { "-D$_" }
$cpArgs += $includes | ForEach-Object { "-I$_" }
$cpArgs += $cfg.suppress | ForEach-Object { "--suppress=$_" }
$suppressionsFile = Join-Path $root 'cppcheck\suppressions.txt'
if (Test-Path $suppressionsFile) { $cpArgs += "--suppressions-list=$suppressionsFile" }

Write-Host ''
Write-Host ('=' * 100)
Write-Host (" cppcheck: {0}  ({1} files, {2}{3})" -f $projectName, $sources.Count, $(if ($Full) { 'full' } else { 'incremental' }), ", $($cfg.enable)")
Write-Host ('=' * 100)

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$quoted = $cpArgs | ForEach-Object { if ($_ -match '\s') { '"{0}"' -f $_ } else { $_ } }
$proc = Start-Process -FilePath $exe -ArgumentList $quoted -Wait -PassThru -NoNewWindow
$sw.Stop()

if ($proc.ExitCode -ne 0 -or -not (Test-Path $xmlReport)) {
    Write-Host ("{0}(1): warning: [cppcheck] toolError: cppcheck exited with code {1}; see output above." -f $projectPath, $proc.ExitCode)
    exit 1
}

# ---- Report ------------------------------------------------------------------------------------
[xml]$xml = Get-Content $xmlReport
$skipIds = @('checkersReport', 'missingIncludeSystem', 'unmatchedSuppression', 'toomanyconfigs')
$findings = @()
foreach ($e in @($xml.results.errors.error)) {
    if (-not $e -or $skipIds -contains $e.id) { continue }
    $loc = @($e.location) | Select-Object -First 1
    $file = $projectPath; $line = 1; $col = 1
    if ($loc) {
        $file = [System.IO.Path]::GetFullPath([string]$loc.file)
        $line = [Math]::Max(1, [int]$loc.line)
        $col = [Math]::Max(1, [int]$loc.column)
    }
    $sev = switch ([string]$e.severity) { 'error' { 'error' } 'warning' { 'warning' } default { 'info' } }
    $findings += [pscustomobject]@{ File = $file; Line = $line; Col = $col; Sev = $sev; Kind = [string]$e.severity; Id = [string]$e.id; Msg = [string]$e.msg }
}

$rank = @{ error = 0; warning = 1; info = 2 }
$findings = $findings | Sort-Object { $rank[$_.Sev] }, File, Line
if ($Keil) {
    $lines = @($findings | ForEach-Object { "{0}({1}): {2}: [cppcheck] {3}: {4}" -f $_.File, $_.Line, $(if ($_.Sev -eq 'info') { 'note' } else { $_.Sev }), $_.Id, $_.Msg })
}
else {
    $lines = @($findings | ForEach-Object { "{0}({1},{2}): {3}: [cppcheck] {4}: {5}" -f $_.File, $_.Line, $_.Col, $_.Sev, $_.Id, $_.Msg })
}
$lines | ForEach-Object { Write-Host $_ }

$byKind = $findings | Group-Object Kind | Sort-Object Count -Descending | ForEach-Object { '{0} {1}' -f $_.Count, $_.Name }
$byId = $findings | Group-Object Id | Sort-Object Count -Descending | Select-Object -First 6 | ForEach-Object { '{0} x{1}' -f $_.Name, $_.Count }
$summary = @(
    '',
    (" Findings: {0}{1}" -f @($findings).Count, $(if ($byKind) { '  (' + ($byKind -join ', ') + ')' } else { '' })),
    $(if ($byId) { " Most frequent: " + ($byId -join ', ') } else { $null }),
    (" Time: {0:N1}s   Reports: {1}, {2}" -f $sw.Elapsed.TotalSeconds, $xmlReport, $txtReport),
    ('=' * 100)
) | Where-Object { $_ -ne $null }
$summary | ForEach-Object { Write-Host $_ }

$header = @(
    "cppcheck report for $projectName",
    "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')   cppcheck: $exe",
    "Files analysed: $($sources.Count)",
    ''
)
[System.IO.File]::WriteAllLines($txtReport, [string[]]($header + $lines + $summary))

if ($Keil -and $env:KEIL_CPPCHECK_STRICT -ne '1') { exit 0 }
if (@($findings | Where-Object Sev -eq 'error').Count) { exit 2 }
if (@($findings | Where-Object Sev -eq 'warning').Count) { exit 1 }
exit 0

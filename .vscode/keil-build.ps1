<#
    Headless Keil uVision (MDK-ARM v5) build wrapper for Cursor / VS Code tasks.

    Usage:
        keil-build.ps1 -Project MDK_ARMv5\Project_54253.uvprojx [-Action build|rebuild|clean|flash] [-Target HT32]

    UV4 exit codes: 0 = OK, 1 = warnings, 2 = errors, 3 = fatal errors,
                    11 = cannot open project, 12 = device not found, 13 = cannot write output, 15 = cannot access file.
    Override the UV4 location with the KEIL_UV4 environment variable.
    build/rebuild also run keil-cppcheck.ps1 (KEIL_CPPCHECK=0 skips it, KEIL_CPPCHECK_STRICT=1 fails the build
    on cppcheck errors) and keil-memcheck.ps1 (budgets in keil-memory.json).
#>
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [ValidateSet('build', 'rebuild', 'clean', 'flash')][string]$Action = 'build',
    [string]$Target = 'HT32'
)

$ErrorActionPreference = 'Stop'

$candidates = @(
    $env:KEIL_UV4,
    "$env:LOCALAPPDATA\Keil_v5\UV4\UV4.exe",
    'C:\Keil_v5\UV4\UV4.exe',
    'C:\Keil\UV4\UV4.exe'
) | Where-Object { $_ -and (Test-Path $_) }

if (-not $candidates) {
    Write-Host 'error: UV4.exe not found. Set the KEIL_UV4 environment variable to its full path.'
    exit 100
}
$uv4 = @($candidates)[0]

$projectPath = (Resolve-Path $Project).Path
$logFile = Join-Path ([System.IO.Path]::GetTempPath()) ("keil_{0}_{1}.log" -f [System.IO.Path]::GetFileNameWithoutExtension($projectPath), $Action)
if (Test-Path $logFile) { Remove-Item $logFile -Force }

$switch = @{ build = '-b'; rebuild = '-r'; clean = '-c'; flash = '-f' }[$Action]
$uvArgs = @($switch, "`"$projectPath`"", '-t', "`"$Target`"", '-j0', '-o', "`"$logFile`"")

Write-Host "> $uv4 $($uvArgs -join ' ')"
# The uVision "After Build" cppcheck command is skipped here; cppcheck runs below with Cursor's message format.
$env:KEIL_CPPCHECK_SKIP_UV = '1'
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $uv4 -ArgumentList $uvArgs -Wait -PassThru -NoNewWindow
$sw.Stop()

if (Test-Path $logFile) {
    Get-Content $logFile | ForEach-Object { Write-Host $_ }
}

$code = $proc.ExitCode
Write-Host ("`n{0} finished in {1:N1}s (UV4 exit code {2})" -f $Action, $sw.Elapsed.TotalSeconds, $code)

$isBuild = $Action -in 'build', 'rebuild'

$cppcheckCode = 0
if ($isBuild -and $env:KEIL_CPPCHECK -ne '0') {
    & (Join-Path $PSScriptRoot 'keil-cppcheck.ps1') -Project $projectPath -Full:($Action -eq 'rebuild')
    $cppcheckCode = $LASTEXITCODE
}

# Warnings (1) are not a failure.
if ($code -gt 1) { exit $code }

if ($isBuild) {
    & (Join-Path $PSScriptRoot 'keil-memcheck.ps1') -Project $projectPath -LogFile $logFile
    if ($LASTEXITCODE -ge 2 -and $env:KEIL_MEMCHECK_STRICT -ne '0') {
        Write-Host 'Build marked as failed because of memory errors (set KEIL_MEMCHECK_STRICT=0 to only report them).'
        exit 2
    }
}
if ($cppcheckCode -ge 2 -and $env:KEIL_CPPCHECK_STRICT -eq '1') {
    Write-Host 'Build marked as failed because cppcheck reported errors (KEIL_CPPCHECK_STRICT=1).'
    exit 2
}
exit 0

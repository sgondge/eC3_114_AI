<#
    Post-build memory analysis for Keil MDK (ARMCC 5) projects.

    Inputs (all produced by a normal uVision build):
      - UV4 build log      : "Program Size: Code= RO-data= RW-data= ZI-data="
      - <output>.map       : load/execution regions, RAM layout, HEAP/STACK, per-object and per-symbol sizes,
                             malloc users
      - <output>.htm       : static call graph -> worst-case stack depth of main and every ISR, recursion
      - <project>.uvprojx  : optimisation level, map/htm locations
      - keil-memory.json   : real flash/RAM limits (including data areas the linker does not know about)

    Diagnostics are printed as "<file>(<line>): error|warning|note: ..." so the task problem matcher shows them
    in the Problems panel. A "Recommendations" section turns the numbers into concrete changes.

    Exit code: 0 = OK / notes only, 1 = warnings, 2 = errors.
#>
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$LogFile
)

$ErrorActionPreference = 'Stop'

function HexToInt([string]$s) { [Convert]::ToInt64(($s -replace '^0x', ''), 16) }
function Hex([long]$v) { '0x{0:X8}' -f $v }
function HexShort([long]$v) { '0x{0:X}' -f $v }
function Bytes([long]$v) { '{0:N0} B' -f $v }
function Free([long]$v) { if ($v -lt 0) { 'OVER by {0:N0} B' -f (-$v) } else { 'free {0:N0} B' -f $v } }
function RoundUp([long]$v, [long]$a) { [long]([Math]::Ceiling($v / [double]$a) * $a) }
function RoundDown([long]$v, [long]$a) { [long]([Math]::Floor($v / [double]$a) * $a) }
function StripHtml([string]$s) { (($s -replace '<[^>]+>', '') -replace '&rArr;', '->' -replace '&nbsp;', ' ' -replace '\s+', ' ').Trim() }
function Bar([double]$pct) {
    $width = 20
    $filled = [Math]::Max(0, [Math]::Min($width, [int][Math]::Round($pct / 100 * $width)))
    '[' + ('#' * $filled) + ('.' * ($width - $filled)) + ']'
}
function FindLine([string]$file, [string]$pattern) {
    if ($file -and (Test-Path $file)) {
        $m = Select-String -Path $file -Pattern $pattern | Select-Object -First 1
        if ($m) { return $m.LineNumber }
    }
    return 1
}

$script:worst = 0
function Report([string]$severity, [string]$file, [int]$line, [string]$message) {
    if ($line -lt 1) { $line = 1 }
    $shown = $severity
    if ($severity -eq 'note') { $shown = 'info' }
    Write-Host ("{0}({1}): {2}: [mem] {3}" -f $file, $line, $shown, $message)
    $level = @{ note = 0; warning = 1; error = 2 }[$severity]
    if ($level -gt $script:worst) { $script:worst = $level }
}

# Priority: 0 = must fix (memory corruption), 1-2 = robustness, 3 = sizing, 4-5 = hardening / housekeeping
$recs = New-Object System.Collections.Generic.List[object]
function Rec([string]$area, [int]$prio, [string]$text) { $recs.Add([pscustomobject]@{ Prio = $prio; Seq = $recs.Count; Text = ("[{0}] {1}" -f $area, $text) }) }

$projectPath = (Resolve-Path $Project).Path
$projectDir = Split-Path $projectPath -Parent
$projectName = Split-Path $projectPath -Leaf

# ---- Project settings ------------------------------------------------------------------------
[xml]$uv = Get-Content $projectPath
$tgt = $uv.Project.Targets.Target | Select-Object -First 1
$common = $tgt.TargetOption.TargetCommonOption
$mapPath = [System.IO.Path]::GetFullPath((Join-Path $projectDir (Join-Path $common.ListingPath ($common.OutputName + '.map'))))
$htmPath = [System.IO.Path]::GetFullPath((Join-Path $projectDir (Join-Path $common.OutputDirectory ($common.OutputName + '.htm'))))
$optimLevels = @{ 0 = 'default'; 1 = '-O0'; 2 = '-O1'; 3 = '-O2'; 4 = '-O3' }
$optim = $optimLevels[[int]$tgt.TargetOption.TargetArmAds.Cads.Optim]
$microlib = ([string]$tgt.TargetOption.TargetArmAds.ArmAdsMisc.useUlib) -eq '1'

# ---- Budgets -------------------------------------------------------------------------------
$cfg = Get-Content (Join-Path $PSScriptRoot 'keil-memory.json') -Raw | ConvertFrom-Json
$th = $cfg.thresholds
$budget = $cfg.projects.$projectName
if (-not $budget) {
    $cpu = [string]$common.Cpu
    $ram = [regex]::Match($cpu, 'IRAM\((0x[0-9A-Fa-f]+)-(0x[0-9A-Fa-f]+)\)')
    $rom = [regex]::Match($cpu, 'IROM\((0x[0-9A-Fa-f]+)-(0x[0-9A-Fa-f]+)\)')
    $budget = [pscustomobject]@{
        startupFile = $null; scatterFile = $null
        ram         = [pscustomobject]@{ name = 'IRAM'; start = $ram.Groups[1].Value; end = Hex ((HexToInt $ram.Groups[2].Value) + 1) }
        flash       = @([pscustomobject]@{ name = 'IROM'; loadRegion = $null; start = $rom.Groups[1].Value; end = Hex ((HexToInt $rom.Groups[2].Value) + 1); limitReason = 'device IROM' })
    }
}
$startupPath = $null
if ($budget.startupFile) {
    $startupPath = Join-Path $projectDir $budget.startupFile
    if (-not (Test-Path $startupPath)) { $startupPath = $null }
}
$scatterPath = $null
if ($budget.scatterFile) {
    $scatterPath = Join-Path $projectDir $budget.scatterFile
    if (-not (Test-Path $scatterPath)) { $scatterPath = $null }
}

Write-Host ''
Write-Host ('=' * 100)
Write-Host (" Memory analysis: {0}  (target {1}, {2}{3})" -f $projectName, $tgt.TargetName, $optim, $(if ($microlib) { ', MicroLIB' } else { '' }))
Write-Host ('=' * 100)

# ---- Program Size from the build log -----------------------------------------------------------
$ps = $null
if ($LogFile -and (Test-Path $LogFile)) {
    $m = Select-String -Path $LogFile -Pattern 'Program Size:\s*Code=(\d+)\s+RO-data=(\d+)\s+RW-data=(\d+)\s+ZI-data=(\d+)' | Select-Object -Last 1
    if ($m) {
        $g = $m.Matches[0].Groups
        $ps = [pscustomobject]@{ Code = [long]$g[1].Value; RO = [long]$g[2].Value; RW = [long]$g[3].Value; ZI = [long]$g[4].Value; Source = 'build log' }
    }
}

if (-not (Test-Path $mapPath)) {
    Report 'warning' $projectPath 1 "Map file not found ($mapPath). Enable 'Linker Listing' in Options for Target > Listing."
    exit $script:worst
}

# ---- Parse the map -----------------------------------------------------------------------------
$map = Get-Content $mapPath
$loadRegions = @{}
$execRegions = @()
$sections = @{}
$ramSymbols = @()
$objects = @()
$mallocUsers = @{}
$ramRows = @()
$inComponents = $false
$curExec = $null
$totalRom = $null
$grand = $null
$ramStart = HexToInt $budget.ram.start
$ramEnd = HexToInt $budget.ram.end
$ramCap = $ramEnd - $ramStart

for ($i = 0; $i -lt $map.Count; $i++) {
    $l = $map[$i]

    if ($l -match '^\s*(\S+)\((\S+)\) refers to (\S+)\(\S+\) for (malloc|realloc|calloc)\s*$') {
        $caller = $Matches[1]; $callerSection = $Matches[2]; $callee = $Matches[3]
        if ($callee -match '^malloc' -and $caller -notmatch '^malloc') {
            $mallocUsers[('{0}:{1}' -f $caller, ($callerSection -replace '^i\.', ''))] = $true
        }
        continue
    }
    if ($l -match '^\s*Load Region (\S+) \(Base: 0x([0-9A-Fa-f]+), Size: 0x([0-9A-Fa-f]+), Max: 0x([0-9A-Fa-f]+)(.*)\)') {
        $size = HexToInt $Matches[3]
        $cm = [regex]::Match([string]$Matches[5], 'COMPRESSED\[0x([0-9A-Fa-f]+)\]')
        if ($cm.Success) { $size = HexToInt $cm.Groups[1].Value }
        $loadRegions[$Matches[1]] = [pscustomobject]@{ Name = $Matches[1]; Base = (HexToInt $Matches[2]); Size = $size; Max = (HexToInt $Matches[4]); Line = $i + 1 }
        $curExec = $null
        continue
    }
    if ($l -match '^\s*Execution Region (\S+) \(Exec base: 0x([0-9A-Fa-f]+), Load base: 0x([0-9A-Fa-f]+), Size: 0x([0-9A-Fa-f]+), Max: 0x([0-9A-Fa-f]+)') {
        $curExec = [pscustomobject]@{ Name = $Matches[1]; Base = (HexToInt $Matches[2]); Size = (HexToInt $Matches[4]); Max = (HexToInt $Matches[5]); Line = $i + 1 }
        $execRegions += $curExec
        continue
    }
    if ($curExec -and $curExec.Base -ge $ramStart -and $l -match '^\s+0x([0-9A-Fa-f]{8})\s+(?:0x[0-9A-Fa-f]{8}|-|COMPRESSED)\s+0x([0-9A-Fa-f]{8})\s+(Data|Zero|PAD)\b(.*)$') {
        $addr = HexToInt $Matches[1]; $size = HexToInt $Matches[2]; $kind = $Matches[3]; $rest = $Matches[4]
        if ($rest -match '\bHEAP\b') { $kind = 'HEAP' } elseif ($rest -match '\bSTACK\b') { $kind = 'STACK' }
        $ramRows += [pscustomobject]@{ Addr = $addr; Size = $size; Kind = $kind }
        continue
    }
    if ($l -match '^\s*(HEAP|STACK)\s+0x([0-9A-Fa-f]+)\s+Section\s+(\d+)\s') {
        $sections[$Matches[1]] = [pscustomobject]@{ Base = (HexToInt $Matches[2]); Size = [long]$Matches[3]; Line = $i + 1 }
        continue
    }
    if ($l -match '^\s+(\S+)\s+0x(2[0-9A-Fa-f]{7})\s+Data\s+(\d+)\s+(\S+)') {
        if ([long]$Matches[3] -gt 0) {
            $ramSymbols += [pscustomobject]@{ Name = $Matches[1]; Addr = (HexToInt $Matches[2]); Size = [long]$Matches[3]; Owner = $Matches[4] }
        }
        continue
    }
    if ($l -match 'Image component sizes') { $inComponents = $true; $curExec = $null; continue }
    if ($inComponents -and $l -match '^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\S+\.o)\s*$') {
        $objects += [pscustomobject]@{ Name = $Matches[7]; Code = [long]$Matches[1]; RO = [long]$Matches[3]; RW = [long]$Matches[4]; ZI = [long]$Matches[5] }
        continue
    }
    if ($l -match '^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+Grand Totals') {
        $grand = [pscustomobject]@{ Code = [long]$Matches[1]; RO = [long]$Matches[3]; RW = [long]$Matches[4]; ZI = [long]$Matches[5]; Source = 'linker map, build was up to date' }
        continue
    }
    if ($l -match 'Total ROM Size \(Code \+ RO Data \+ RW Data\)\s+(\d+)') { $totalRom = [long]$Matches[1] }
}

if (-not $ps) { $ps = $grand }
if (-not $ps) {
    Report 'warning' $mapPath 1 'Could not find Program Size in the build log or Grand Totals in the map file.'
    exit $script:worst
}

# ---- Parse the static call graph (.htm) ------------------------------------------------------
$cg = $null
if (Test-Path $htmPath) {
    $h = Get-Content $htmPath
    $cg = [pscustomobject]@{ Max = $null; Unknown = $false; Chain = ''; Cycles = @(); Funcs = @{} }
    $inRec = $false
    for ($i = 0; $i -lt $h.Count; $i++) {
        $l = $h[$i]
        if ($l -match 'Maximum Stack Usage =\s*(\d+) bytes(.*)') {
            $cg.Max = [long]$Matches[1]
            $cg.Unknown = $Matches[2] -match 'Unknown'
            if ($i + 2 -lt $h.Count) { $cg.Chain = StripHtml $h[$i + 2] }
            continue
        }
        if ($l -match 'Mutually Recursive functions') { $inRec = $true; continue }
        if ($l -match 'Function Pointers') { $inRec = $false; continue }
        if ($inRec -and $l -match '<LI><a [^>]*>([^<]+)</a>.*?&rArr;.*?<a [^>]*>([^<]+)</a>') {
            # Default exception/IRQ stubs are "B ." loops and show up as self-recursion
            if (-not ($Matches[1] -eq $Matches[2] -and $Matches[1] -match 'Handler$')) {
                $cg.Cycles += [pscustomobject]@{ From = $Matches[1]; To = $Matches[2] }
            }
            continue
        }
        if ($l -match '<STRONG><a name="[^"]*"></a>([^<]+)</STRONG> \([^,]*, (\d+) bytes, Stack size (\d+) bytes') {
            $name = $Matches[1]; $codeBytes = [long]$Matches[2]; $own = [long]$Matches[3]
            $depth = $own; $chain = $name
            if ($i + 1 -lt $h.Count -and $h[$i + 1] -match 'Max Depth = (\d+)<LI>Call Chain = (.*)$') {
                $depth = [long]$Matches[1]; $chain = StripHtml $Matches[2]
            }
            $cg.Funcs[$name] = [pscustomobject]@{ Name = $name; Code = $codeBytes; Own = $own; Depth = $depth; Chain = $chain }
        }
    }
}

# ---- Summary -----------------------------------------------------------------------------------
$heap = $sections['HEAP']; $stack = $sections['STACK']
$heapSize = 0; $stackSize = 0
if ($heap) { $heapSize = $heap.Size }
if ($stack) { $stackSize = $stack.Size }
$staticRam = $ps.RW + $ps.ZI - $heapSize - $stackSize

Write-Host (" Program Size ({0}): Code={1} RO-data={2} RW-data={3} ZI-data={4}" -f $ps.Source, $ps.Code, $ps.RO, $ps.RW, $ps.ZI)
Write-Host ("   Flash  Code + RO-data + RW-data = {0,10}{1}" -f (Bytes ($ps.Code + $ps.RO + $ps.RW)), $(if ($totalRom) { "   (in flash after RW compression: $(Bytes $totalRom))" } else { '' }))
Write-Host ("   RAM    RW-data + ZI-data        = {0,10}   = static data {1} + heap {2} + stack {3}" -f (Bytes ($ps.RW + $ps.ZI)), (Bytes $staticRam), (Bytes $heapSize), (Bytes $stackSize))
Write-Host ''

# ---- Flash regions -----------------------------------------------------------------------------
Write-Host ' Region usage'
$appFlashPct = 0
foreach ($f in $budget.flash) {
    $start = HexToInt $f.start
    $end = HexToInt $f.end
    $cap = $end - $start
    $lr = $null
    if ($f.loadRegion) { $lr = $loadRegions[$f.loadRegion] }
    if (-not $lr) { $lr = $loadRegions.Values | Where-Object { $_.Base -ge $start -and $_.Base -lt $end } | Select-Object -First 1 }
    if (-not $lr) { Write-Host ("   Flash {0,-24} no load region found in map" -f $f.name); continue }

    $used = ($lr.Base + $lr.Size) - $start
    $pct = 100.0 * $used / $cap
    if ($pct -gt $appFlashPct) { $appFlashPct = $pct }
    Write-Host ("   Flash {0,-24} {1}-{2} {3,10} / {4,10} {5,6:N1}% {6} {7}" -f $f.name, (Hex $lr.Base), (Hex ($lr.Base + $lr.Size)), (Bytes $used), (Bytes $cap), $pct, (Bar $pct), (Free ($cap - $used)))

    if ($used -gt $cap) {
        Report 'error' $mapPath $lr.Line ("FLASH OVERFLOW in {0}: image ends at {1}, {2} past {3} ({4}). The linker does not check this limit." -f $f.name, (Hex ($lr.Base + $lr.Size)), (Bytes ($used - $cap)), (Hex $end), $f.limitReason)
    }
    elseif ($pct -ge $th.warnPercent) {
        Report 'warning' $mapPath $lr.Line ("{0} flash is {1:N1}% full; only {2} left before {3} ({4})." -f $f.name, $pct, (Bytes ($cap - $used)), (Hex $end), $f.limitReason)
    }
    elseif ($pct -ge $th.notePercent) {
        Report 'note' $mapPath $lr.Line ("{0} flash is {1:N1}% full ({2} free)." -f $f.name, $pct, (Bytes ($cap - $used)))
    }

    if ($lr.Max -ge 0xFFFFFFFF -or ($lr.Base + $lr.Max) -gt $end) {
        $limit = $end - $lr.Base
        if ($scatterPath) {
            Rec 'Linker' 4 ("Make armlink enforce the {0} limit: in {1} change the load region header to '{2} {3} {4}' (currently no max size), so an overflow into {5} fails the link instead of corrupting data." -f $f.name, $budget.scatterFile, $lr.Name, (Hex $lr.Base), (Hex $limit), (Hex $end))
        }
        else {
            Rec 'Linker' 4 ("Make armlink enforce the {0} limit: set IROM1 to start {1}, size {2} in Options for Target > Target." -f $f.name, (Hex $lr.Base), (HexShort $limit))
        }
    }
}

# ---- RAM regions and layout -------------------------------------------------------------------
$ramRegions = $execRegions | Where-Object { $_.Base -ge $ramStart -and $_.Base -lt ($ramStart + 0x10000000) }
$reserved = 0
$ramTop = $ramStart
if ($ramRegions) {
    $first = $ramRegions | Sort-Object Base | Select-Object -First 1
    $last = $ramRegions | Sort-Object { $_.Base + $_.Size } | Select-Object -Last 1
    $reserved = $first.Base - $ramStart
    $ramTop = $last.Base + $last.Size
    $ramUsed = $ramTop - $ramStart
    $pct = 100.0 * $ramUsed / $ramCap
    Write-Host ("   RAM   {0,-24} {1}-{2} {3,10} / {4,10} {5,6:N1}% {6} {7}" -f $budget.ram.name, (Hex $ramStart), (Hex $ramTop), (Bytes $ramUsed), (Bytes $ramCap), $pct, (Bar $pct), (Free ($ramCap - $ramUsed)))

    if ($ramUsed -gt $ramCap) {
        Report 'error' $mapPath $last.Line ("RAM OVERFLOW: execution region '{0}' ends at {1}, {2} past the end of {3} ({4}). The linker does not report it because the region has no max size." -f $last.Name, (Hex $ramTop), (Bytes ($ramUsed - $ramCap)), $budget.ram.name, (Hex $ramEnd))
    }
    elseif ($pct -ge $th.warnPercent) {
        Report 'warning' $mapPath $last.Line ("RAM is {0:N1}% full; only {1} left." -f $pct, (Bytes ($ramCap - $ramUsed)))
    }
    elseif ($pct -ge $th.notePercent) {
        Report 'note' $mapPath $last.Line ("RAM is {0:N1}% full ({1} free)." -f $pct, (Bytes ($ramCap - $ramUsed)))
    }

    foreach ($r in $ramRegions) {
        if ($r.Max -ge 0xFFFFFFFF -and $scatterPath) {
            Rec 'Linker' 4 ("Make armlink enforce the RAM limit: in {0} change '{1} {2}' to '{1} {2} {3}' so RAM overflow fails the link." -f $budget.scatterFile, $r.Name, (Hex $r.Base), (Hex ($ramEnd - $r.Base)))
        }
    }

    # Layout: merge consecutive rows of the same kind (padding joins the preceding block)
    $blocks = @()
    foreach ($row in ($ramRows | Sort-Object Addr)) {
        $kind = $row.Kind
        if ($kind -eq 'PAD' -and $blocks.Count) { $kind = $blocks[-1].Kind }
        if ($blocks.Count -and $blocks[-1].Kind -eq $kind) { $blocks[-1].End = $row.Addr + $row.Size }
        else { $blocks += [pscustomobject]@{ Kind = $kind; Start = $row.Addr; End = $row.Addr + $row.Size } }
    }
    $labels = @{ Data = 'RW-data (.data, copied from flash at startup)'; Zero = 'ZI-data (.bss, zeroed at startup)'; HEAP = 'HEAP (malloc pool)'; STACK = 'STACK (grows down from the top)' }
    Write-Host ''
    Write-Host ' RAM layout (from map)'
    if ($reserved -gt 0) { Write-Host ("   {0}-{1} {2,10}  reserved / not used by the linker" -f (Hex $ramStart), (Hex $first.Base), (Bytes $reserved)) }
    foreach ($b in $blocks) {
        $mark = ''
        if ($b.Start -ge $ramEnd) { $mark = '   <-- entirely OUTSIDE physical RAM' }
        elseif ($b.End -gt $ramEnd) { $mark = ("   <-- crosses end of RAM at {0}" -f (Hex $ramEnd)) }
        Write-Host ("   {0}-{1} {2,10}  {3}{4}" -f (Hex $b.Start), (Hex $b.End), (Bytes ($b.End - $b.Start)), $labels[$b.Kind], $mark)
    }
    if ($ramTop -lt $ramEnd) { Write-Host ("   {0}-{1} {2,10}  unused" -f (Hex $ramTop), (Hex $ramEnd), (Bytes ($ramEnd - $ramTop))) }
}
else {
    Write-Host '   RAM   no RAM execution region found in map'
}

$stackLine = FindLine $startupPath '^\s*Stack_Size\s+EQU'
$heapLine = FindLine $startupPath '^\s*Heap_Size\s+EQU'
$startupRef = $mapPath
if ($startupPath) { $startupRef = $startupPath }

if ($stack -and ($stack.Base + $stack.Size) -gt $ramEnd) {
    $outside = [Math]::Min($stack.Size, $stack.Base + $stack.Size - $ramEnd)
    Report 'error' $startupRef $(if ($startupPath) { $stackLine } else { $stack.Line }) ("STACK OUTSIDE RAM: stack {0}-{1}, initial SP {1}; {2} of {3} lies beyond {4}. The first push or exception after reset faults." -f (Hex $stack.Base), (Hex ($stack.Base + $stack.Size)), (Bytes $outside), (Bytes $stack.Size), (Hex $ramEnd))
}
if ($heap -and $heap.Size -gt 0 -and ($heap.Base + $heap.Size) -gt $ramEnd) {
    Report 'error' $startupRef $(if ($startupPath) { $heapLine } else { $heap.Line }) ("HEAP OUTSIDE RAM: heap {0}-{1} extends {2} past {3}." -f (Hex $heap.Base), (Hex ($heap.Base + $heap.Size)), (Bytes ($heap.Base + $heap.Size - $ramEnd)), (Hex $ramEnd))
}

# ---- Stack analysis (call graph) ----------------------------------------------------------------
$stackNeed = $null
if ($cg -and $cg.Max) {
    $isrs = @($cg.Funcs.Values | Where-Object { $_.Name -match '(IRQHandler|_Handler)$' -and $_.Name -ne 'Reset_Handler' -and $_.Depth -gt 0 } | Sort-Object Depth -Descending)
    $isrDepth = 0; $isrName = 'none'
    if ($isrs.Count) { $isrDepth = $isrs[0].Depth; $isrName = $isrs[0].Name }
    $frame = 32
    $worstCase = $cg.Max + $isrDepth + $frame
    $margin = [double]$th.stackMarginPercent
    $stackNeed = RoundUp ([long]($worstCase * (1 + $margin / 100))) 64

    Write-Host ''
    Write-Host ' Stack analysis (static call graph)'
    Write-Host ("   main worst case        {0,10}{1}" -f (Bytes $cg.Max), $(if ($cg.Unknown) { '  + unknown (recursion / function pointers)' } else { '' }))
    Write-Host ("   via                    {0}" -f $cg.Chain)
    Write-Host ("   deepest ISR            {0,10}  {1}" -f (Bytes $isrDepth), $isrName)
    if ($isrs.Count) { Write-Host ("   via                    {0}" -f $isrs[0].Chain) }
    if ($isrs.Count -gt 1) {
        Write-Host ("   other ISRs             {0}" -f (($isrs | Select-Object -Skip 1 -First 4 | ForEach-Object { '{0} {1}' -f $_.Name, (Bytes $_.Depth) }) -join ', '))
    }
    Write-Host ("   exception frame        {0,10}" -f (Bytes $frame))
    Write-Host ("   worst case total       {0,10}   -> with {1}% margin: {2}   (configured: {3})" -f (Bytes $worstCase), $margin, (Bytes $stackNeed), (Bytes $stackSize))

    if ($stack -and $stackSize -lt $worstCase) {
        Report 'warning' $startupRef $stackLine ("Stack {0} is smaller than the call-graph worst case {1} (main {2} + {3} {4} + frame {5}); deep paths will overwrite the heap / data below the stack." -f (Bytes $stackSize), (Bytes $worstCase), (Bytes $cg.Max), $isrName, (Bytes $isrDepth), (Bytes $frame))
    }
    if ($cg.Cycles.Count) {
        $hubs = $cg.Cycles | Group-Object To | Sort-Object Count -Descending | Select-Object -First 3
        $hubText = ($hubs | ForEach-Object { '{0} (re-entered from {1} functions)' -f $_.Name, $_.Count }) -join ', '
        Report 'warning' $htmPath (FindLine $htmPath 'Mutually Recursive') ("{0} recursive call cycles make the real stack depth unbounded. Re-entry hubs: {1}." -f $cg.Cycles.Count, $hubText)
        Rec 'Stack' 2 ("Remove the re-entrancy that makes stack depth unbounded: {0}. Blocking waits (delay / GSM wait loops) that call the event dispatcher let state handlers run inside themselves. Either stop dispatching events inside wait loops, or guard the dispatcher with a 'busy' flag so it cannot nest." -f $hubText)
    }

    $blockingPattern = [string]$th.blockingFunctionPattern
    if ($blockingPattern) {
        foreach ($isr in $isrs) {
            $hops = $isr.Chain -split '\s*->\s*' | ForEach-Object { ($_ -replace '\(Cycle\)', '').Trim() }
            $hit = $hops | Select-Object -Skip 1 | Where-Object { $_ -match $blockingPattern } | Select-Object -First 1
            if ($hit) {
                $entry = @($hops)[1]
                Report 'warning' $htmPath (FindLine $htmPath ([regex]::Escape(">$($isr.Name)</STRONG>"))) ("Interrupt {0} can reach {1} ({2}). Blocking or state-machine code inside an ISR stalls other interrupts and adds {3} to the stack on top of main." -f $isr.Name, $hit, $isr.Chain, (Bytes $isr.Depth))
                Rec 'ISR' 1 ("Keep {0} short: it calls {1}, which leads to {2}. Set a flag (or post an event) in the ISR and do the state change in the main loop. This also cuts the ISR stack from {3} to the ISR's own few bytes." -f $isr.Name, $entry, $hit, (Bytes $isr.Depth))
            }
        }
    }
}

# ---- Heap analysis -------------------------------------------------------------------------------
$mallocList = @($mallocUsers.Keys | Sort-Object)
if ($heap -and $heap.Size -gt 0) {
    $heapPct = 100.0 * $heap.Size / $ramCap
    if (-not $mallocList.Count) {
        Report 'warning' $startupRef $heapLine ("Heap reserves {0} but nothing in the image calls malloc/realloc/calloc." -f (Bytes $heap.Size))
        Rec 'Heap' 3 ("Nothing uses the heap: set Heap_Size EQU 0 in {0} to free {1} of RAM." -f $budget.startupFile, (Bytes $heap.Size))
    }
    elseif ($heapPct -ge $th.maxHeapPercentOfRam) {
        Report 'warning' $startupRef $heapLine ("Heap reserves {0} ({1:N0}% of RAM). Heap users: {2}." -f (Bytes $heap.Size), $heapPct, ($mallocList -join ', '))
    }
}

# ---- Stack + heap sizing recommendation -------------------------------------------------------
if ($stackNeed -and $ramRegions) {
    $avail = $ramCap - $reserved - $staticRam
    $ramOver = ($ramTop -gt $ramEnd)
    $stackLow = $stackSize -lt $stackNeed
    $heapNeeded = $mallocList.Count -gt 0
    $minHeap = 0
    if ($heapNeeded) { $minHeap = [long]$th.minHeapBytesIfMallocUsed }

    if ($ramOver -or $stackLow) {
        $heapBudget = RoundDown ($avail - $stackNeed) 256
        if ($heapBudget -ge $minHeap) {
            $newHeap = 0
            if ($heapNeeded) { $newHeap = [Math]::Min($heapBudget, [Math]::Max($minHeap, $heapSize)) }
            $total = $reserved + $staticRam + $stackNeed + $newHeap
            Rec 'RAM' 0 ("Re-balance stack and heap in {0}:  Stack_Size EQU {1}  ({2}, was {3})   Heap_Size EQU {4}  ({5}, was {6}). RAM = {7} reserved + {8} static + stack + heap = {9} of {10} ({11:N1}%), {12} spare." -f $budget.startupFile, (HexShort $stackNeed), (Bytes $stackNeed), (Bytes $stackSize), (HexShort $newHeap), (Bytes $newHeap), (Bytes $heapSize), (Bytes $reserved), (Bytes $staticRam), (Bytes $total), (Bytes $ramCap), (100.0 * $total / $ramCap), (Bytes ($ramCap - $total)))
            if ($heapNeeded) {
                Rec 'Heap' 3 ("Verify the heap fits the real peak: heap users are {0}. Size cJSON payloads and malloc(n) requests (e.g. arrays sized from a counter) against {1}, and always handle a NULL return." -f ($mallocList -join ', '), (Bytes $newHeap))
            }
        }
        else {
            $cut = $minHeap - $heapBudget
            Rec 'RAM' 0 ("Static data ({0}) + required stack ({1}) leave only {2} for the heap; cut at least {3} of static RAM. Largest variables are listed above." -f (Bytes $staticRam), (Bytes $stackNeed), (Bytes ([Math]::Max(0, $avail - $stackNeed))), (Bytes $cut))
        }
    }
}

# ---- Build settings ----------------------------------------------------------------------------
if ($optim -eq '-O0') {
    Report 'note' $projectPath (FindLine $projectPath '<Optim>') ("Built with -O0 (no optimisation): larger code and deeper stack frames than needed.")
    Rec 'Build' 3 ("Compile with -O1 or -O2 (Options for Target > C/C++ > Optimization). -O0 typically costs 30-50% more code and much more stack per call; re-run this analysis after switching. Check that variables shared with ISRs are declared volatile before enabling optimisation.")
}

# ---- Top consumers -----------------------------------------------------------------------------
$n = [int]$th.topN
if ($objects.Count) {
    Write-Host ''
    Write-Host " Top $n RAM users by object (RW + ZI)"
    $objects | Sort-Object { $_.RW + $_.ZI } -Descending | Select-Object -First $n | Where-Object { ($_.RW + $_.ZI) -gt 0 } | ForEach-Object {
        Write-Host ("   {0,10}  {1,-34} RW {2,7}  ZI {3,7}" -f (Bytes ($_.RW + $_.ZI)), $_.Name, $_.RW, $_.ZI)
    }
    Write-Host ''
    Write-Host " Top $n flash users by object (Code + RO)"
    $objects | Sort-Object { $_.Code + $_.RO } -Descending | Select-Object -First $n | ForEach-Object {
        Write-Host ("   {0,10}  {1,-34} Code {2,7}  RO {3,7}" -f (Bytes ($_.Code + $_.RO)), $_.Name, $_.Code, $_.RO)
    }
}
if ($ramSymbols.Count) {
    Write-Host ''
    Write-Host " Top $n RAM variables"
    $ramSymbols | Sort-Object Size -Descending | Select-Object -First $n | ForEach-Object {
        Write-Host ("   {0,10}  {1}  {2,-34} {3}" -f (Bytes $_.Size), (Hex $_.Addr), $_.Name, $_.Owner)
    }
}
if ($mallocList.Count) {
    Write-Host ''
    Write-Host (" Heap users (call malloc/realloc): {0}" -f ($mallocList -join ', '))
}

if ($appFlashPct -ge $th.warnPercent -and $objects.Count) {
    $big = ($objects | Where-Object { $_.Name -notmatch '^(iap|startup_)' } | Sort-Object { $_.Code + $_.RO } -Descending | Select-Object -First 4 | ForEach-Object { '{0} {1}' -f $_.Name, (Bytes ($_.Code + $_.RO)) }) -join ', '
    Rec 'Flash' 5 ("Flash is above {0}%. Biggest modules: {1}. Look for duplicated string tables, debug/CLI text and printf/scanf use in these files." -f $th.warnPercent, $big)
}

# ---- Recommendations -----------------------------------------------------------------------------
Write-Host ''
if ($recs.Count) {
    Write-Host ' Recommendations'
    $k = 1
    foreach ($r in ($recs | Sort-Object Prio, Seq)) { Write-Host ("   {0}. {1}" -f $k, $r.Text); $k++ }
}
else {
    Write-Host ' Recommendations: none, memory layout is healthy.'
}

Write-Host ''
Write-Host (" Memory check: {0}" -f @('OK', 'WARNINGS', 'ERRORS')[$script:worst])
Write-Host ('=' * 100)
exit $script:worst

param(
    [string]$UProfBin = "C:\Program Files\AMD\AMDuProf\bin",
    [string]$ExePath  = "D:\03_Courses\3_2_05_Parallel_programming\Lab1_CPU_programming\matrix_dot.exe",
    [string]$OutRoot  = "C:\uProfBatchMatrix",
    [int[]]$Ns        = @(64, 128, 256, 512, 1024, 2048, 4096, 8192),
    [string[]]$Modes  = @("naive", "cache"),
    [string]$Metrics  = "ipc,l1,l2,pipeline_util"
)

$ErrorActionPreference = "Continue"

function Ensure-Dir {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
}

function Parse-ProgramOutput {
    param([string[]]$Lines)

    $result = @{
        ProgramTimeLabel = $null
        ProgramTimeMs    = $null
        GeneratedCsvPath = $null
    }

    foreach ($line in $Lines) {
        if ($line -match '^\s*(naive_time_ms|cache_opt_time_ms)\s*=\s*([0-9eE\.\+\-]+)\s*$') {
            $result.ProgramTimeLabel = $matches[1]
            $result.ProgramTimeMs    = [double]$matches[2]
        }
        elseif ($line -match 'Generated Timeseries data file path:\s*(.+)$') {
            $result.GeneratedCsvPath = $matches[1].Trim()
        }
    }

    return $result
}

function Find-UProfCsv {
    param([string]$RunDir)

    $csv = Get-ChildItem -Path $RunDir -Recurse -Filter "report-timeseries.csv" -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if ($null -eq $csv) { return $null }
    return $csv.FullName
}

function Parse-UProfSingleRow {
    param([string]$CsvPath)

    $parsed = @{
        CsvFound            = $false
        CsvHasData          = $false
        UtilizationPct      = $null
        EffFreqMHz          = $null
        IPC                 = $null
        CPI                 = $null
        DCAccessPTI         = $null
        L2AccessPTI         = $null
        L2MissPTI           = $null
        L2HitPTI            = $null
        FrontendBound       = $null
        BadSpeculation      = $null
        BackendBound        = $null
        BackendBoundMemory  = $null
        BackendBoundCPU     = $null
        Retiring            = $null
    }

    if (-not $CsvPath -or -not (Test-Path $CsvPath)) {
        return $parsed
    }

    $parsed.CsvFound = $true

    $rows = Import-Csv -Path $CsvPath -Delimiter ',' -Header (1..500)
    if (-not $rows) {
        return $parsed
    }

    $table = @()
    foreach ($r in $rows) {
        $vals = @()
        foreach ($p in $r.PSObject.Properties) {
            $vals += [string]$p.Value
        }
        $table += ,$vals
    }

    $coreIdx = -1
    for ($i = 0; $i -lt $table.Count; $i++) {
        if ($table[$i][0].Trim() -eq "CORE METRICS") {
            $coreIdx = $i
            break
        }
    }

    if ($coreIdx -lt 0) {
        return $parsed
    }

    $groupRow  = $table[$coreIdx + 1]
    $headerRow = $table[$coreIdx + 2]
    $dataRow   = $table[$coreIdx + 3]

    $sysStart = -1
    for ($i = 0; $i -lt $groupRow.Count; $i++) {
        if ($groupRow[$i].Trim() -eq "System (Aggregated)") {
            $sysStart = $i
            break
        }
    }

    if ($sysStart -lt 0) {
        return $parsed
    }

    $map = @{}
    for ($i = $sysStart; $i -lt $headerRow.Count; $i++) {
        if ($i -gt $sysStart -and $groupRow[$i].Trim() -ne "") {
            break
        }

        $name = $headerRow[$i].Trim()
        if ($name -eq "") { continue }

        $value = ""
        if ($i -lt $dataRow.Count) {
            $value = $dataRow[$i].Trim()
        }

        $map[$name] = $value
    }

    function Get-Num([string]$Name) {
        if ($map.ContainsKey($Name)) {
            $v = $map[$Name]
            if ($v -match '^[\+\-]?[0-9]*\.?[0-9]+([eE][\+\-]?[0-9]+)?$') {
                return [double]$v
            }
        }
        return $null
    }

    $parsed.CsvHasData         = $true
    $parsed.UtilizationPct     = Get-Num "Utilization (%)"
    $parsed.EffFreqMHz         = Get-Num "Eff Freq (MHz)"
    $parsed.IPC                = Get-Num "IPC (Sys + User)"
    $parsed.CPI                = Get-Num "CPI (Sys + User)"
    $parsed.DCAccessPTI        = Get-Num "DC Access (pti)"
    $parsed.L2AccessPTI        = Get-Num "L2 Access (pti)"
    $parsed.L2MissPTI          = Get-Num "L2 Miss (pti)"
    $parsed.L2HitPTI           = Get-Num "L2 Hit (pti)"
    $parsed.FrontendBound      = Get-Num "Frontend_Bound"
    $parsed.BadSpeculation     = Get-Num "Bad_Speculation"
    $parsed.BackendBound       = Get-Num "Backend_Bound"
    $parsed.BackendBoundMemory = Get-Num "Backend_Bound.Memory"
    $parsed.BackendBoundCPU    = Get-Num "Backend_Bound.CPU"
    $parsed.Retiring           = Get-Num "Retiring"

    return $parsed
}

function Run-OneCase {
    param(
        [int]$N,
        [string]$Mode
    )

    $tag = "matrix_${N}_${Mode}"
    $runDir = Join-Path $OutRoot $tag

    if (Test-Path $runDir) {
        Remove-Item -Path $runDir -Recurse -Force
    }
    Ensure-Dir $runDir

    $stdoutLog  = Join-Path $runDir "stdout.log"
    $stderrLog  = Join-Path $runDir "stderr.log"
    $consoleLog = Join-Path $runDir "console.log"

    Write-Host "=================================================="
    Write-Host "Running case: n=$N mode=$Mode"
    Write-Host "Run dir     : $runDir"
    Write-Host "=================================================="

    $exe = Join-Path $UProfBin "AMDuProfPcm.exe"

    $argList = @(
        "-m", $Metrics,
        "-a",
        "--html",
        "-O", $runDir,
        "--",
        $ExePath,
        "$N",
        "$Mode"
    )

    $p = Start-Process `
        -FilePath $exe `
        -ArgumentList $argList `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -NoNewWindow `
        -PassThru `
        -Wait

    $exitCode = $p.ExitCode

    $cmdOutput = @()
    if (Test-Path $stdoutLog) {
        $cmdOutput += Get-Content $stdoutLog
    }
    if (Test-Path $stderrLog) {
        $cmdOutput += Get-Content $stderrLog
    }

    $cmdOutput | Out-File -FilePath $consoleLog -Encoding utf8

    if ($cmdOutput.Count -gt 0) {
        $cmdOutput | ForEach-Object { Write-Host $_ }
    }

    $prog = Parse-ProgramOutput -Lines $cmdOutput

    $csvPath = $prog.GeneratedCsvPath
    if (-not $csvPath -or -not (Test-Path $csvPath)) {
        $csvPath = Find-UProfCsv -RunDir $runDir
    }

    $u = Parse-UProfSingleRow -CsvPath $csvPath

    $status = "OK"
    if ($exitCode -ne 0) {
        $status = "UPROF_EXIT_$exitCode"
    } elseif (-not $u.CsvFound) {
        $status = "NO_CSV"
    } elseif (-not $u.CsvHasData) {
        $status = "CSV_PARSE_FAIL"
    }

    return [PSCustomObject]@{
        n                    = $N
        mode                 = $Mode
        status               = $status
        run_dir              = $runDir
        csv_path             = $csvPath
        stdout_log           = $stdoutLog
        stderr_log           = $stderrLog
        console_log          = $consoleLog
        program_time_label   = $prog.ProgramTimeLabel
        program_time_ms      = $prog.ProgramTimeMs
        utilization_pct      = $u.UtilizationPct
        eff_freq_mhz         = $u.EffFreqMHz
        ipc                  = $u.IPC
        cpi                  = $u.CPI
        dc_access_pti        = $u.DCAccessPTI
        l2_access_pti        = $u.L2AccessPTI
        l2_miss_pti          = $u.L2MissPTI
        l2_hit_pti           = $u.L2HitPTI
        frontend_bound       = $u.FrontendBound
        bad_speculation      = $u.BadSpeculation
        backend_bound        = $u.BackendBound
        backend_bound_memory = $u.BackendBoundMemory
        backend_bound_cpu    = $u.BackendBoundCPU
        retiring             = $u.Retiring
    }
}

Ensure-Dir $OutRoot

$summary = @()

foreach ($n in $Ns) {
    foreach ($mode in $Modes) {
        $summary += Run-OneCase -N $n -Mode $mode
    }
}

$summaryCsv = Join-Path $OutRoot "matrix_summary.csv"
$summary | Export-Csv -Path $summaryCsv -NoTypeInformation -Encoding UTF8

Write-Host ""
Write-Host "All done."
Write-Host "Summary CSV: $summaryCsv"

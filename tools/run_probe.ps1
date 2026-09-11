param(
    [string]$GimpRoot = "$env:LOCALAPPDATA\Programs\GIMP 3"
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$probeRoot = Join-Path $repoRoot '.stage0'
$profilePath = Join-Path $probeRoot 'profile'
$outputPath = Join-Path $probeRoot 'output'
$gimpExe = Join-Path $GimpRoot 'bin\gimp-console-3.0.exe'
if (-not (Test-Path -LiteralPath $gimpExe)) { throw "GIMP executable missing: $gimpExe" }
New-Item -ItemType Directory -Force -Path $profilePath, $outputPath | Out-Null
$runStarted = [DateTime]::UtcNow
$previousProfile = $env:GIMP3_DIRECTORY
$previousOutput = $env:PERSPECTIVE_PROBE_OUTPUT
$previousScript = $env:PERSPECTIVE_PROBE_SCRIPT
try {
    $env:GIMP3_DIRECTORY = $profilePath
    $env:PERSPECTIVE_PROBE_OUTPUT = $outputPath
    $env:PERSPECTIVE_PROBE_SCRIPT = Join-Path $PSScriptRoot 'probe_gimp.py'
    $batch = "import os; exec(compile(open(os.environ['PERSPECTIVE_PROBE_SCRIPT'], encoding='utf-8').read(), os.environ['PERSPECTIVE_PROBE_SCRIPT'], 'exec'))"
    & $gimpExe --new-instance --no-interface --no-data --no-fonts --console-messages --batch-interpreter=python-fu-eval --batch $batch --quit 2>&1 | Tee-Object -FilePath (Join-Path $probeRoot 'console.log')
    if ($LASTEXITCODE -ne 0) { throw "GIMP probe failed: $LASTEXITCODE" }
    $reportFile = Get-Item -LiteralPath (Join-Path $outputPath 'report.json')
    if ($reportFile.LastWriteTimeUtc -lt $runStarted) { throw 'Probe report was not refreshed' }
    $report = Get-Content -LiteralPath $reportFile.FullName -Raw | ConvertFrom-Json
    if (-not $report.parasite_roundtrip) { throw 'XCF metadata roundtrip failed' }
} finally {
    $env:GIMP3_DIRECTORY = $previousProfile
    $env:PERSPECTIVE_PROBE_OUTPUT = $previousOutput
    $env:PERSPECTIVE_PROBE_SCRIPT = $previousScript
}

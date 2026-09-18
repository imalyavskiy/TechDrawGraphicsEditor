<#
.SYNOPSIS
Runs one built-in self-test mode against the portable Release package.
.PARAMETER Platform
Qt platform plugin: windows for native-dialog coverage or offscreen for headless rendering.
.OUTPUTS
Prints result.txt and writes the complete test artifacts under build\<platform>-results.
.NOTES
Returns nonzero on a missing package, timeout, application failure or stale report.
#>
param([ValidateSet('windows','offscreen')][string]$Platform='windows')
$Architecture = if ($env:TECHDRAW_TEST_ARCHITECTURE) { $env:TECHDRAW_TEST_ARCHITECTURE } else { 'x64' }
if ($Architecture -notin @('x64', 'x86')) { throw "Unsupported test architecture: $Architecture" }
$ErrorActionPreference='Stop'
# Resolve every project path from this script so tests do not depend on the caller's working directory.
$projectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$packageSuffix = if ($Architecture -eq 'x86') { '-x86' } else { '' }
$exe=Join-Path $projectRoot "dist\TechDraw$packageSuffix\TechDraw.exe"
$outputPath=Join-Path $projectRoot "build\$Platform-results$packageSuffix"
if (-not (Test-Path -LiteralPath $exe)) { throw 'Release package was not found. Run build-release.bat first.' }
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
# Capture the start instant so an old success report cannot mask a failed launch.
$started=[DateTime]::UtcNow
$arguments=@('-platform',$Platform,'--self-test',('"'+$outputPath+'"'))
$process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
# Bound every run so a modal dialog or deadlock cannot block the command file indefinitely.
if (-not $process.WaitForExit(20000)) { Stop-Process -Id $process.Id; throw 'Self-test timed out' }
if ($process.ExitCode -ne 0) { throw "Self-test failed ($($process.ExitCode)); see $outputPath\trace.txt" }
$report=Get-Item -LiteralPath (Join-Path $outputPath 'result.txt')
if ($report.LastWriteTimeUtc -lt $started) { throw 'Self-test report was not refreshed' }
Get-Content -LiteralPath $report.FullName

param([ValidateSet('windows','offscreen')][string]$Platform='windows')
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'dist\Drawing\Drawing.exe'
$outputPath=Join-Path $PSScriptRoot "build\$Platform-results"
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$started=[DateTime]::UtcNow
$arguments=@('-platform',$Platform,'--self-test',('"'+$outputPath+'"'))
$process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(20000)) { Stop-Process -Id $process.Id; throw 'Self-test timed out' }
if ($process.ExitCode -ne 0) { throw "Self-test failed ($($process.ExitCode)); see $outputPath\trace.txt" }
$report=Get-Item -LiteralPath (Join-Path $outputPath 'result.txt')
if ($report.LastWriteTimeUtc -lt $started) { throw 'Self-test report was not refreshed' }
Get-Content -LiteralPath $report.FullName

<#
.SYNOPSIS
Verifies install, repair and uninstall against an isolated package directory.
.PARAMETER ProjectRoot
Validated repository root supplied by test-installer.bat.
.PARAMETER Architecture
Installer architecture to inspect: x64 or x86.
.NOTES
Shell integration is disabled so this check does not change shortcuts, associations or uninstall records.
#>
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][ValidateSet('x64','x86')][string]$Architecture
)
$ErrorActionPreference='Stop'
$packageSuffix=if($Architecture -eq 'x86'){'-x86'}else{''}
$installerBuild=Join-Path $ProjectRoot "build\installer-$Architecture"
$installScript=Join-Path $installerBuild 'install-techdraw.ps1'
$testRoot=Join-Path $ProjectRoot "build\installer-lifecycle$packageSuffix"
$copiedUninstaller=Join-Path $ProjectRoot "build\installer-uninstall$packageSuffix.ps1"

if(-not(Test-Path -LiteralPath $installScript)){throw "Packaged install script is missing: $installScript"}
Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $copiedUninstaller -Force -ErrorAction SilentlyContinue

try{
    # A clean install must create the executable and ownership manifest.
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $installScript -Quiet -SkipShellIntegration -InstallDirectory $testRoot
    if($LASTEXITCODE -ne 0){throw "Clean install failed with exit code $LASTEXITCODE"}
    $executable=Join-Path $testRoot 'TechDraw.exe';$manifest=Join-Path $testRoot '.techdraw-install.json'
    if(-not(Test-Path -LiteralPath $executable) -or -not(Test-Path -LiteralPath $manifest)){throw 'Clean install is incomplete.'}
    & (Join-Path $PSScriptRoot 'verify-package.ps1') -PackageDirectory $testRoot -Architecture $Architecture

    # Repair must replace damaged owned files while keeping unrelated content.
    Set-Content -LiteralPath (Join-Path $testRoot 'foreign.txt') -Value 'preserve'
    Set-Content -LiteralPath $executable -Value 'damaged'
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $installScript -Quiet -SkipShellIntegration -InstallDirectory $testRoot -Repair
    if($LASTEXITCODE -ne 0 -or (Get-Item -LiteralPath $executable).Length -lt 10000){throw 'Repair did not restore the executable.'}

    # Run uninstall from a copy because the installed original is one of the files being removed.
    Copy-Item -LiteralPath (Join-Path $testRoot 'Uninstall-TechDraw.ps1') -Destination $copiedUninstaller
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $copiedUninstaller -Uninstall -Quiet -Scope CurrentUser -InstallDirectory $testRoot
    if($LASTEXITCODE -ne 0){throw "Uninstall failed with exit code $LASTEXITCODE"}
    if(Test-Path -LiteralPath $executable){throw 'An owned application file survived uninstall.'}
    if(-not(Test-Path -LiteralPath (Join-Path $testRoot 'foreign.txt'))){throw 'Uninstall removed a foreign file.'}
    Write-Output "Installer lifecycle verified for $Architecture."
}finally{
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $copiedUninstaller -Force -ErrorAction SilentlyContinue
}

<#
.SYNOPSIS
Reserved signing step for application EXE and DLL files before installer packaging.
.PARAMETER PackageDirectory
Validated portable package whose binaries would be signed.
.PARAMETER Architecture
Package architecture used for diagnostics and later certificate policy selection.
.OUTPUTS
An explicit skip message while signing credentials are not configured.
.NOTES
This intentional no-op keeps signing in the release pipeline without storing certificates,
private keys, passwords or service tokens in the repository.
#>
param(
    [Parameter(Mandatory = $true)][string]$PackageDirectory,
    [Parameter(Mandatory = $true)][ValidateSet('x64', 'x86')][string]$Architecture
)

$ErrorActionPreference = 'Stop'
$packagePath = [IO.Path]::GetFullPath($PackageDirectory)
if (-not (Test-Path -LiteralPath $packagePath -PathType Container)) {
    throw "Cannot sign a missing portable package: $packagePath"
}
if (-not (Test-Path -LiteralPath (Join-Path $packagePath 'TechDraw.exe') -PathType Leaf)) {
    throw "Cannot sign an incomplete portable package: $packagePath"
}

Write-Output "SIGNING SKIPPED: application binaries for $Architecture in $packagePath"
Write-Output 'No signing certificate is configured; this stage is a deliberate successful placeholder.'
exit 0

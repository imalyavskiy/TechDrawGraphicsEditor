<#
.SYNOPSIS
Reserved signing step for the finished installer executable.
.PARAMETER InstallerPath
Installer produced after IExpress packaging.
.PARAMETER Architecture
Installer payload architecture used for diagnostics and later certificate policy selection.
.OUTPUTS
An explicit skip message while signing credentials are not configured.
.NOTES
The finished container must be signed after packaging. This intentional no-op preserves that order
without committing credentials or pretending that the artifact already carries a valid signature.
#>
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [Parameter(Mandatory = $true)][ValidateSet('x64', 'x86')][string]$Architecture
)

$ErrorActionPreference = 'Stop'
$resolvedPath = [IO.Path]::GetFullPath($InstallerPath)
if (-not (Test-Path -LiteralPath $resolvedPath -PathType Leaf)) {
    throw "Cannot sign a missing installer: $resolvedPath"
}

Write-Output "SIGNING SKIPPED: $Architecture installer $resolvedPath"
Write-Output 'No signing certificate is configured; this stage is a deliberate successful placeholder.'
exit 0

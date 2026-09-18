<#
.SYNOPSIS
Verifies that every Windows executable in a portable package has the expected architecture.
.PARAMETER PackageDirectory
Portable directory produced by build-release.bat.
.PARAMETER Architecture
Expected PE architecture: x64 or x86.
.OUTPUTS
One summary line on success; throws with the offending file on mismatch or invalid input.
.NOTES
The check reads the PE COFF machine field directly and therefore needs no Visual Studio tools.
#>
param(
    [Parameter(Mandatory = $true)][string]$PackageDirectory,
    [Parameter(Mandatory = $true)][ValidateSet('x64', 'x86')][string]$Architecture
)

$ErrorActionPreference = 'Stop'

# Reads the COFF machine value after validating the DOS and PE signatures.
function Get-PeMachine {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $reader = New-Object IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a PE file: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadUInt32()
        if ($peOffset -gt ($stream.Length - 6)) { throw "Invalid PE header offset: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Invalid PE signature: $Path" }
        return $reader.ReadUInt16()
    } finally {
        $stream.Dispose()
    }
}

$packagePath = [IO.Path]::GetFullPath($PackageDirectory)
if (-not (Test-Path -LiteralPath $packagePath -PathType Container)) {
    throw "Portable package directory does not exist: $packagePath"
}

$expectedMachine = if ($Architecture -eq 'x64') { 0x8664 } else { 0x014C }
$binaries = @(Get-ChildItem -LiteralPath $packagePath -Recurse -File | Where-Object {
    $_.Extension -in @('.exe', '.dll')
})
if ($binaries.Count -eq 0) { throw "Portable package contains no EXE or DLL files: $packagePath" }

foreach ($binary in $binaries) {
    $actualMachine = Get-PeMachine -Path $binary.FullName
    if ($actualMachine -ne $expectedMachine) {
        throw ('Architecture mismatch in {0}: expected {1} (0x{2:X4}), found 0x{3:X4}' -f
            $binary.FullName, $Architecture, $expectedMachine, $actualMachine)
    }
}

Write-Output "Verified $($binaries.Count) PE files as $Architecture in $packagePath"

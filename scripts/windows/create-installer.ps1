<#
.SYNOPSIS
Builds the per-user IExpress installer from the portable Release directory.
.PARAMETER ProjectRoot
Validated repository root supplied by build-installer.bat.
.OUTPUTS
dist\installer\TechnicalDrawing-Setup.exe; intermediate files remain in build\installer for diagnostics.
.NOTES
Requires PowerShell 5, Compress-Archive and the Windows IExpress executable. Any error terminates with a nonzero code.
#>
param([Parameter(Mandatory = $true)][string]$ProjectRoot)

$ErrorActionPreference = 'Stop'

# Prepare two self-contained installer payload files in an ignored build directory.
$portableDirectory = Join-Path $ProjectRoot 'dist\TechDraw'
$installerBuildDirectory = Join-Path $ProjectRoot 'build\installer'
$installerOutputDirectory = Join-Path $ProjectRoot 'dist\installer'
$payloadArchive = Join-Path $installerBuildDirectory 'payload.zip'
$installerScript = Join-Path $installerBuildDirectory 'install-techdraw.ps1'
$sedPath = Join-Path $installerBuildDirectory 'TechnicalDrawing.sed'
$installerPath = Join-Path $installerOutputDirectory 'TechnicalDrawing-Setup.exe'
$iexpressDirectory = Join-Path $env:TEMP ("TechDraw-iexpress-" + [Guid]::NewGuid().ToString('N'))

# Do not package a partial or stale directory when the expected entry point is absent.
if (-not (Test-Path -LiteralPath (Join-Path $portableDirectory 'TechDraw.exe'))) {
    throw 'The portable Release package is incomplete.'
}

# Recreate only fixed installer artifacts below the validated project root.
New-Item -ItemType Directory -Force -Path $installerBuildDirectory, $installerOutputDirectory | Out-Null
Remove-Item -LiteralPath $payloadArchive, $installerScript, $sedPath, $installerPath -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $portableDirectory '*') -DestinationPath $payloadArchive -CompressionLevel Optimal
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'install-techdraw.ps1') -Destination $installerScript

# IExpress cannot reliably consume directive and payload paths containing spaces. Build its cabinet in a
# GUID-named temporary directory, then copy the resulting EXE back to the documented project output path.
New-Item -ItemType Directory -Path $iexpressDirectory | Out-Null
try {
    Copy-Item -LiteralPath $payloadArchive, $installerScript -Destination $iexpressDirectory
    $iexpressSedPath = Join-Path $iexpressDirectory 'TechnicalDrawing.sed'
    $iexpressOutputPath = Join-Path $iexpressDirectory 'TechnicalDrawing-Setup.exe'
    $sourceDirectory = $iexpressDirectory.TrimEnd('\') + '\'
    $sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=0
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=
TargetName=$iexpressOutputPath
FriendlyName=Technical Drawing Setup
AppLaunched=powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File install-techdraw.ps1
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[SourceFiles]
SourceFiles0=$sourceDirectory
[SourceFiles0]
%FILE0%=
%FILE1%=
[Strings]
FILE0="payload.zip"
FILE1="install-techdraw.ps1"
"@
    # Keep a copy of the generated directive in build\installer for repeatable diagnosis.
    Set-Content -LiteralPath $iexpressSedPath -Value $sed -Encoding ascii
    Copy-Item -LiteralPath $iexpressSedPath -Destination $sedPath

    $iexpress = Join-Path $env:SystemRoot 'System32\iexpress.exe'
    # Wait for IExpress and verify both its exit code and its expected output file.
    $process = Start-Process -FilePath $iexpress -ArgumentList @('/N', '/Q', $iexpressSedPath) -Wait -PassThru
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $iexpressOutputPath)) {
        throw "IExpress failed with exit code $($process.ExitCode)."
    }
    Copy-Item -LiteralPath $iexpressOutputPath -Destination $installerPath
} finally {
    # A unique temporary directory makes cleanup safe even when the project path contains spaces.
    Remove-Item -LiteralPath $iexpressDirectory -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Output "Installer created: $installerPath"

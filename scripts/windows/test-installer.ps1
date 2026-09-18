<#
.SYNOPSIS
Verifies the Qt Installer Framework lifecycle in an isolated project directory.
.DESCRIPTION
Build output is installed headlessly with shell integration disabled, checked,
damaged and repaired by the setup EXE, then uninstalled while a foreign file is
kept. The script never writes Start menu entries, associations or uninstall keys.
#>
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][ValidateSet('x64', 'x86')][string]$Architecture
)

$ErrorActionPreference = 'Stop'
$projectRootFull = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$buildRoot = [IO.Path]::GetFullPath((Join-Path $projectRootFull 'build')).TrimEnd('\')
$testSuffix = if ($Architecture -eq 'x86') { '-x86' } else { '' }
$testRoot = Join-Path $buildRoot "ifw-lifecycle$testSuffix"
$cacheRoot = Join-Path $buildRoot "ifw-cache$testSuffix"
$metadataBackup = Join-Path $buildRoot "ifw-repair-metadata$testSuffix"
$controllerPath = Join-Path $buildRoot "ifw-test-controller$testSuffix.qs"
$installerPath = Join-Path $projectRootFull "dist\installer\TechnicalDrawing-Setup-$Architecture.exe"
$savedQtPluginPath = $env:QT_PLUGIN_PATH

foreach ($path in @($testRoot, $cacheRoot, $metadataBackup, $controllerPath)) {
    $resolved = [IO.Path]::GetFullPath($path)
    if (-not $resolved.StartsWith($buildRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe installer test path: $resolved"
    }
}
if (-not (Test-Path -LiteralPath $installerPath)) { throw "Installer is missing: $installerPath" }

# Recreate only verified children of the repository build directory.
Remove-Item -LiteralPath $testRoot, $cacheRoot, $metadataBackup -Recurse -Force -ErrorAction SilentlyContinue
$controllerRoot = $testRoot.Replace('\', '/')
@"
function Controller()
{
    installer.setValue("TechDrawSkipShellIntegration", "true");
    installer.setValue("TechDrawLaunch", "false");
    installer.setValue("TechDrawLegacyProductId", "TechnicalDrawingLifecycleTest-$Architecture");
    installer.setValue("TechDrawLegacyAssociationExtension", ".td-lifecycle-$Architecture");
    installer.setValue("TechDrawLegacyAssociationProgId", "TechnicalDrawing.LifecycleTest.$Architecture");
    installer.setValue("TechDrawLegacyProgramsDirectory", "$controllerRoot/legacy-programs");
    installer.setValue("TechDrawLegacyDesktopDirectory", "$controllerRoot/legacy-desktop");
}
"@ | Set-Content -LiteralPath $controllerPath -Encoding utf8

# Runs one QtIFW CLI command and fails immediately on a nonzero exit status.
function Invoke-InstallerCommand {
    param([string]$Executable, [string[]]$Arguments, [string]$Description)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description failed with exit code $LASTEXITCODE." }
}

# Moves only QtIFW bookkeeping away so setup can exercise repair into the same directory.
function Move-QtIfwMetadata {
    New-Item -ItemType Directory -Force -Path $metadataBackup | Out-Null
    $names = @(
        'components.xml', 'InstallationLog.txt', 'installer.dat', 'network.xml',
        'TechDrawMaintenance.dat', 'TechDrawMaintenance.exe', 'TechDrawMaintenance.ini', 'installerResources'
    )
    foreach ($name in $names) {
        $source = Join-Path $testRoot $name
        if (Test-Path -LiteralPath $source) { Move-Item -LiteralPath $source -Destination $metadataBackup -Force }
    }
}

try {
    # QtIFW is statically linked and must not scan the application's Qt 5 plugin tree.
    $env:QT_PLUGIN_PATH = ''
    $installArguments = @(
        '--cache-path', $cacheRoot,
        '--script', $controllerPath,
        '--root', $testRoot,
        '--accept-licenses', '--accept-messages', '--confirm-command',
        'install', 'org.techdraw.editor'
    )
    Invoke-InstallerCommand $installerPath $installArguments 'Clean QtIFW installation'

    $executable = Join-Path $testRoot 'TechDraw.exe'
    $maintenance = Join-Path $testRoot 'TechDrawMaintenance.exe'
    if (-not (Test-Path -LiteralPath $executable) -or -not (Test-Path -LiteralPath $maintenance)) {
        throw 'QtIFW installation did not create the application and maintenance tool.'
    }
    # The maintenance tool follows the architecture of the installed QtIFW toolset;
    # only the application payload follows the optional x86 application kit.
    & (Join-Path $PSScriptRoot 'verify-package.ps1') -PackageDirectory $testRoot -Architecture $Architecture `
        -ExcludeFileName 'TechDrawMaintenance.exe'

    # Repair must overwrite damaged owned content and preserve an unrelated file.
    $foreignFile = Join-Path $testRoot 'foreign.txt'
    Set-Content -LiteralPath $foreignFile -Value 'preserve'
    Set-Content -LiteralPath $executable -Value 'damaged'
    Move-QtIfwMetadata
    Remove-Item -LiteralPath $cacheRoot -Recurse -Force -ErrorAction SilentlyContinue
    Invoke-InstallerCommand $installerPath $installArguments 'QtIFW repair installation'
    if ((Get-Item -LiteralPath $executable).Length -lt 10000) { throw 'Repair did not restore TechDraw.exe.' }

    # The replacement installation owns new metadata; the detached old metadata is no longer needed.
    Remove-Item -LiteralPath $metadataBackup -Recurse -Force -ErrorAction SilentlyContinue
    Invoke-InstallerCommand $maintenance @('--accept-messages', '--confirm-command', 'purge') 'QtIFW uninstall'
    if (Test-Path -LiteralPath $executable) { throw 'An application file survived uninstall.' }
    if (-not (Test-Path -LiteralPath $foreignFile)) { throw 'Uninstall removed a foreign file.' }

    Write-Output "Qt Installer Framework lifecycle verified for $Architecture."
} finally {
    $env:QT_PLUGIN_PATH = $savedQtPluginPath
    Remove-Item -LiteralPath $testRoot, $cacheRoot, $metadataBackup -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $controllerPath -Force -ErrorAction SilentlyContinue
}

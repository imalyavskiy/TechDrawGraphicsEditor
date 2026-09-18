<#
.SYNOPSIS
Creates a Qt Installer Framework package tree from repository templates.
.DESCRIPTION
The script treats CMakeLists.txt and installer/product.json as the canonical
sources for product version and product identity. Generated config, metadata,
payload files and archives remain below the ignored build directory.
#>
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][ValidateSet('x64', 'x86')][string]$Architecture,
    [Parameter(Mandatory = $true)][string]$IfwRoot
)

$ErrorActionPreference = 'Stop'

# Reject an accidental invocation against another directory before cleanup.
$projectRootFull = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
foreach ($required in @('CMakeLists.txt', 'installer\product.json', 'resources\techdraw.ico')) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectRootFull $required))) {
        throw "Invalid project root. Missing $required"
    }
}

# Windows PowerShell 5.1 otherwise treats UTF-8 files without a BOM as the
# current ANSI code page. Every repository text input is explicitly decoded so
# localized product names cannot be damaged while expanding the QtIFW files.
$product = Get-Content -LiteralPath (Join-Path $projectRootFull 'installer\product.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$cmake = Get-Content -LiteralPath (Join-Path $projectRootFull 'CMakeLists.txt') -Raw -Encoding UTF8
$versionMatch = [regex]::Match($cmake, 'project\s*\(\s*TechDraw\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)', 'IgnoreCase')
if (-not $versionMatch.Success) { throw 'Cannot read the TechDraw version from CMakeLists.txt.' }
$productVersion = $versionMatch.Groups[1].Value

$packageSuffix = if ($Architecture -eq 'x86') { '-x86' } else { '' }
$portableDirectory = Join-Path $projectRootFull "dist\TechDraw$packageSuffix"
if (-not (Test-Path -LiteralPath (Join-Path $portableDirectory $product.executable))) {
    throw "The portable Release package is incomplete: $portableDirectory"
}

$binaryCreator = Join-Path $IfwRoot 'bin\binarycreator.exe'
$installerBase = Join-Path $IfwRoot 'bin\installerbase.exe'
if (-not (Test-Path -LiteralPath $binaryCreator) -or -not (Test-Path -LiteralPath $installerBase)) {
    throw "Qt Installer Framework tools were not found under $IfwRoot"
}
$ifwVersion = [Version](Get-Item -LiteralPath $binaryCreator).VersionInfo.ProductVersion
$minimumIfwVersion = [Version][string]$product.minimumIfwVersion
if ($ifwVersion -lt $minimumIfwVersion) {
    throw "Qt Installer Framework $minimumIfwVersion or newer is required; found $ifwVersion."
}

$installerBuildDirectory = Join-Path $projectRootFull "build\installer-$Architecture"
$ifwBuildDirectory = Join-Path $installerBuildDirectory 'ifw'
$configDirectory = Join-Path $ifwBuildDirectory 'config'
$packagesDirectory = Join-Path $ifwBuildDirectory 'packages'
$componentDirectory = Join-Path $packagesDirectory ([string]$product.componentId)
$dataDirectory = Join-Path $componentDirectory 'data'
$metaDirectory = Join-Path $componentDirectory 'meta'

# The deletion target is a fixed child of the validated repository build tree.
$expectedPrefix = ([IO.Path]::GetFullPath((Join-Path $projectRootFull 'build'))).TrimEnd('\') + '\'
$resolvedBuild = [IO.Path]::GetFullPath($ifwBuildDirectory)
if (-not $resolvedBuild.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to recreate a directory outside the project build tree: $resolvedBuild"
}
Remove-Item -LiteralPath $resolvedBuild -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $configDirectory, $dataDirectory, $metaDirectory | Out-Null

# Copy the already verified portable package without introducing another deployment path.
Get-ChildItem -LiteralPath $portableDirectory -Force | Copy-Item -Destination $dataDirectory -Recurse -Force
$portableFiles = @(Get-ChildItem -LiteralPath $portableDirectory -Recurse -File | ForEach-Object {
    $_.FullName.Substring($portableDirectory.Length).TrimStart('\').Replace('\', '/')
})
$migrationDirectory = Join-Path $dataDirectory '_installer'
New-Item -ItemType Directory -Force -Path $migrationDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRootFull 'installer\migrate-legacy.ps1') -Destination $migrationDirectory
$portableFiles | Set-Content -LiteralPath (Join-Path $migrationDirectory 'payload-files.txt') -Encoding utf8

# Expands a source template using the one shared metadata object.
function Expand-TechDrawTemplate {
    param([string]$Source, [string]$Destination)
    $text = Get-Content -LiteralPath $Source -Raw -Encoding UTF8
    $replacements = [ordered]@{
        '@PRODUCT_ID@' = [string]$product.productId
        '@PRODUCT_NAME@' = [string]$product.name
        '@PRODUCT_DISPLAY_NAME@' = [string]$product.displayName
        '@PRODUCT_PUBLISHER@' = [string]$product.publisher
        '@PRODUCT_VERSION@' = $productVersion
        '@PRODUCT_ARCHITECTURE@' = $Architecture
        '@MAINTENANCE_TOOL@' = [string]$product.maintenanceTool
        '@RELEASE_DATE@' = (Get-Date -Format 'yyyy-MM-dd')
    }
    foreach ($entry in $replacements.GetEnumerator()) { $text = $text.Replace($entry.Key, $entry.Value) }
    Set-Content -LiteralPath $Destination -Value $text -Encoding utf8
}

$installerSource = Join-Path $projectRootFull 'installer'
Expand-TechDrawTemplate (Join-Path $installerSource 'config\config.xml.in') (Join-Path $configDirectory 'config.xml')
Copy-Item -LiteralPath (Join-Path $installerSource 'config\controller.qs') -Destination $configDirectory
Copy-Item -LiteralPath (Join-Path $installerSource 'config\wizard-banner.png') -Destination $configDirectory
Copy-Item -LiteralPath (Join-Path $projectRootFull 'resources\techdraw.ico') -Destination $configDirectory
Copy-Item -LiteralPath (Join-Path $projectRootFull 'resources\techdraw.png') -Destination (Join-Path $configDirectory 'window-icon.png')

$metaSource = Join-Path $installerSource "packages\$($product.componentId)\meta"
Expand-TechDrawTemplate (Join-Path $metaSource 'package.xml.in') (Join-Path $metaDirectory 'package.xml')
Expand-TechDrawTemplate (Join-Path $metaSource 'component.qs') (Join-Path $metaDirectory 'component.qs')
Get-ChildItem -LiteralPath $metaSource -File |
    Where-Object { $_.Name -notin @('package.xml.in', 'component.qs') } |
    Copy-Item -Destination $metaDirectory

# Fail packaging before binarycreator when a future script change corrupts the
# localized title or the component source while crossing PowerShell encodings.
$generatedConfig = Get-Content -LiteralPath (Join-Path $configDirectory 'config.xml') -Raw -Encoding UTF8
$generatedComponent = Get-Content -LiteralPath (Join-Path $metaDirectory 'component.qs') -Raw -Encoding UTF8
if (-not $generatedConfig.Contains([string]$product.displayName) -or
    -not $generatedComponent.Contains([string]$product.displayName)) {
    throw 'Generated QtIFW files failed the UTF-8 localization check.'
}

[ordered]@{
    product = $product.name
    displayName = $product.displayName
    version = $productVersion
    architecture = $Architecture
    qtInstallerFramework = $ifwVersion.ToString()
    config = (Join-Path $configDirectory 'config.xml')
    packages = $packagesDirectory
    portableFiles = $portableFiles.Count
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $installerBuildDirectory 'ifw-package.json') -Encoding utf8

Write-Output "QtIFW package prepared: $ifwBuildDirectory"

<#
.SYNOPSIS
Removes registrations and owned files left by the stage-6 IExpress installer.
.DESCRIPTION
The Qt Installer Framework component invokes this helper after its payload has
been extracted. Files that belong to both the legacy and the new payload are
kept when both installations use the same directory. Documents, settings and
unlisted files are never removed.
#>
param(
    [Parameter(Mandatory = $true)][string]$TargetDirectory,
    [Parameter(Mandatory = $true)][ValidateSet('CurrentUser', 'AllUsers')][string]$SelectedScope,
    [Parameter(Mandatory = $true)][ValidateSet('move', 'keep', 'cancel')][string]$MigrationMode
)

$ErrorActionPreference = 'Stop'
$productId = 'TechnicalDrawing'
$manifestName = '.techdraw-install.json'
$legacyUninstallerName = 'Uninstall-TechDraw.ps1'

# Resolves the legacy uninstall record for one installation scope.
function Get-LegacyInstallation {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$Scope)
    $hive = if ($Scope -eq 'AllUsers') { 'HKLM:' } else { 'HKCU:' }
    $key = "$hive\Software\Microsoft\Windows\CurrentVersion\Uninstall\$productId"
    if (-not (Test-Path -LiteralPath $key)) { return $null }
    $record = Get-ItemProperty -LiteralPath $key
    if ([string]$record.InstallerTechnology -eq 'QtIFW') { return $null }
    return [pscustomobject]@{
        Scope = $Scope
        Key = $key
        Directory = [string]$record.InstallLocation
    }
}

# Returns the exact shortcut paths created by the legacy installer.
function Get-LegacyShortcuts {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$Scope)
    $programs = if ($Scope -eq 'AllUsers') { 'CommonPrograms' } else { 'Programs' }
    $desktop = if ($Scope -eq 'AllUsers') { 'CommonDesktopDirectory' } else { 'Desktop' }
    $menu = Join-Path ([Environment]::GetFolderPath($programs)) 'Technical Drawing'
    return [pscustomobject]@{
        Menu = $menu
        Start = Join-Path $menu 'Technical Drawing.lnk'
        Uninstall = Join-Path $menu 'Uninstall Technical Drawing.lnk'
        Desktop = Join-Path ([Environment]::GetFolderPath($desktop)) 'Technical Drawing.lnk'
    }
}

# Removes the old .drw association only while it still points to the old copy.
function Remove-LegacyAssociation {
    param([string]$Scope, [string]$OwnedDirectory)
    $classes = if ($Scope -eq 'AllUsers') { 'HKLM:\Software\Classes' } else { 'HKCU:\Software\Classes' }
    $extension = Join-Path $classes '.drw'
    $progId = Join-Path $classes 'TechnicalDrawing.Project'
    $commandKey = Join-Path $progId 'shell\open\command'
    $extensionValue = if (Test-Path -LiteralPath $extension) { [string](Get-Item -LiteralPath $extension).GetValue('') } else { '' }
    $commandValue = if (Test-Path -LiteralPath $commandKey) { [string](Get-Item -LiteralPath $commandKey).GetValue('') } else { '' }
    if ($extensionValue -eq 'TechnicalDrawing.Project' -and $commandValue -like "*$OwnedDirectory*") {
        Remove-Item -LiteralPath $extension, $progId -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Removes only files named in the legacy ownership manifest.
function Remove-LegacyPayload {
    param($Installation, [string[]]$NewPayloadFiles)
    if ([string]::IsNullOrWhiteSpace($Installation.Directory)) { return }
    $oldRoot = [IO.Path]::GetFullPath($Installation.Directory).TrimEnd('\')
    $newRoot = [IO.Path]::GetFullPath($TargetDirectory).TrimEnd('\')
    $sameDirectory = $oldRoot.Equals($newRoot, [StringComparison]::OrdinalIgnoreCase)
    $manifestPath = Join-Path $oldRoot $manifestName
    if (Test-Path -LiteralPath $manifestPath) {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        foreach ($relativeValue in @($manifest.files)) {
            $relative = ([string]$relativeValue).Replace('\', '/').TrimStart('/')
            if ($sameDirectory -and $relative -in $NewPayloadFiles) { continue }
            $owned = [IO.Path]::GetFullPath((Join-Path $oldRoot $relative.Replace('/', '\')))
            if ($owned.StartsWith($oldRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
                Remove-Item -LiteralPath $owned -Force -ErrorAction SilentlyContinue
            }
        }
    }
    Remove-Item -LiteralPath $manifestPath, (Join-Path $oldRoot $legacyUninstallerName) -Force -ErrorAction SilentlyContinue
    Get-ChildItem -LiteralPath $oldRoot -Directory -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

# Reads the generated list of files now owned by the QtIFW component.
$payloadList = Join-Path $TargetDirectory '_installer\payload-files.txt'
$newPayloadFiles = if (Test-Path -LiteralPath $payloadList) {
    @(Get-Content -LiteralPath $payloadList | ForEach-Object { $_.Replace('\', '/').TrimStart('/') })
} else { @() }

foreach ($scope in @('CurrentUser', 'AllUsers')) {
    $legacy = Get-LegacyInstallation $scope
    if ($null -eq $legacy) { continue }
    $isSelectedScope = $scope -eq $SelectedScope
    if (-not $isSelectedScope -and $MigrationMode -ne 'move') { continue }

    Remove-LegacyPayload $legacy $newPayloadFiles
    $shortcuts = Get-LegacyShortcuts $scope
    Remove-Item -LiteralPath $shortcuts.Start, $shortcuts.Uninstall, $shortcuts.Desktop -Force -ErrorAction SilentlyContinue
    if ((Test-Path -LiteralPath $shortcuts.Menu) -and -not (Get-ChildItem -LiteralPath $shortcuts.Menu -Force)) {
        Remove-Item -LiteralPath $shortcuts.Menu -Force -ErrorAction SilentlyContinue
    }
    Remove-LegacyAssociation $scope $legacy.Directory
    Remove-Item -LiteralPath $legacy.Key -Recurse -Force -ErrorAction SilentlyContinue
}


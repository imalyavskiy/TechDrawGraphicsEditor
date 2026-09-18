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
    [Parameter(Mandatory = $true)][ValidateSet('Apply', 'Rollback', 'Commit')][string]$Action,
    [Parameter(Mandatory = $true)][string]$BackupDirectory,
    [string]$TargetDirectory,
    [ValidateSet('CurrentUser', 'AllUsers')][string]$SelectedScope = 'CurrentUser',
    [ValidateSet('move', 'keep', 'cancel')][string]$MigrationMode = 'move',
    [string]$RegistryProductId = 'TechnicalDrawing',
    [string]$AssociationExtension = '.drw',
    [string]$AssociationProgId = 'TechnicalDrawing.Project',
    [string]$UserProgramsDirectory,
    [string]$UserDesktopDirectory
)

$ErrorActionPreference = 'Stop'
$manifestName = '.techdraw-install.json'
$legacyUninstallerName = 'Uninstall-TechDraw.ps1'
$stateFileName = 'migration-state.json'
$regExe = Join-Path $env:SystemRoot 'System32\reg.exe'

function Resolve-SafeBackupDirectory {
    $full = [IO.Path]::GetFullPath($BackupDirectory).TrimEnd('\')
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if (-not $full.StartsWith($temp + '\', [StringComparison]::OrdinalIgnoreCase) -or
        -not ([IO.Path]::GetFileName($full)).StartsWith('TechDraw-legacy-migration-', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe migration backup directory: $full"
    }
    return $full
}

$backupRoot = Resolve-SafeBackupDirectory
$statePath = Join-Path $backupRoot $stateFileName

function Write-MigrationState {
    $script:state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $statePath -Encoding UTF8
}

function ConvertTo-NativeRegistryPath {
    param([string]$Path)
    if ($Path.StartsWith('HKCU:\', [StringComparison]::OrdinalIgnoreCase)) {
        return 'HKEY_CURRENT_USER\' + $Path.Substring(6)
    }
    if ($Path.StartsWith('HKLM:\', [StringComparison]::OrdinalIgnoreCase)) {
        return 'HKEY_LOCAL_MACHINE\' + $Path.Substring(6)
    }
    throw "Unsupported registry path: $Path"
}

# Resolves the legacy uninstall record for one installation scope.
function Get-LegacyInstallation {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$Scope)
    $hive = if ($Scope -eq 'AllUsers') { 'HKLM:' } else { 'HKCU:' }
    $key = "$hive\Software\Microsoft\Windows\CurrentVersion\Uninstall\$RegistryProductId"
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
    if ($Scope -eq 'AllUsers') {
        $programsRoot = [Environment]::GetFolderPath('CommonPrograms')
        $desktopRoot = [Environment]::GetFolderPath('CommonDesktopDirectory')
    } else {
        $programsRoot = if ($UserProgramsDirectory) { $UserProgramsDirectory } else { [Environment]::GetFolderPath('Programs') }
        $desktopRoot = if ($UserDesktopDirectory) { $UserDesktopDirectory } else { [Environment]::GetFolderPath('Desktop') }
    }
    $menu = Join-Path $programsRoot 'Technical Drawing'
    return [pscustomobject]@{
        Menu = $menu
        Start = Join-Path $menu 'Technical Drawing.lnk'
        Uninstall = Join-Path $menu 'Uninstall Technical Drawing.lnk'
        Desktop = Join-Path $desktopRoot 'Technical Drawing.lnk'
    }
}

function Backup-OwnedFile {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    foreach ($entry in @($script:state.Files)) {
        if ([string]$entry.Original -eq $Path) { return }
    }
    $fileDirectory = Join-Path $backupRoot 'files'
    New-Item -ItemType Directory -Force -Path $fileDirectory | Out-Null
    $backup = Join-Path $fileDirectory ('{0:D5}.bak' -f @($script:state.Files).Count)
    Copy-Item -LiteralPath $Path -Destination $backup -Force
    $script:state.Files += [pscustomobject]@{ Original = $Path; Backup = $backup }
    Write-MigrationState
}

function Backup-RegistryKey {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    foreach ($entry in @($script:state.RegistryKeys)) {
        if ([string]$entry.Path -eq $Path) { return }
    }
    $registryDirectory = Join-Path $backupRoot 'registry'
    New-Item -ItemType Directory -Force -Path $registryDirectory | Out-Null
    $backup = Join-Path $registryDirectory ('{0:D3}.reg' -f @($script:state.RegistryKeys).Count)
    & $regExe export (ConvertTo-NativeRegistryPath $Path) $backup /y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Cannot back up registry key: $Path" }
    $script:state.RegistryKeys += [pscustomobject]@{ Path = $Path; Backup = $backup }
    Write-MigrationState
}

function Restore-Migration {
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { return }
    $savedState = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($entry in @($savedState.Files)) {
        if (-not (Test-Path -LiteralPath ([string]$entry.Backup) -PathType Leaf)) { continue }
        $parent = Split-Path -Parent ([string]$entry.Original)
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        Copy-Item -LiteralPath ([string]$entry.Backup) -Destination ([string]$entry.Original) -Force
    }
    foreach ($entry in @($savedState.RegistryKeys)) {
        if (-not (Test-Path -LiteralPath ([string]$entry.Backup) -PathType Leaf)) { continue }
        & $regExe import ([string]$entry.Backup) | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Cannot restore registry key: $($entry.Path)" }
    }
    Remove-Item -LiteralPath $backupRoot -Recurse -Force
}

# Removes the old association only while it still points to the old copy.
function Remove-LegacyAssociation {
    param([string]$Scope, [string]$OwnedDirectory)
    $classes = if ($Scope -eq 'AllUsers') { 'HKLM:\Software\Classes' } else { 'HKCU:\Software\Classes' }
    $extension = Join-Path $classes $AssociationExtension
    $progId = Join-Path $classes $AssociationProgId
    $commandKey = Join-Path $progId 'shell\open\command'
    $extensionValue = if (Test-Path -LiteralPath $extension) { [string](Get-Item -LiteralPath $extension).GetValue('') } else { '' }
    $commandValue = if (Test-Path -LiteralPath $commandKey) { [string](Get-Item -LiteralPath $commandKey).GetValue('') } else { '' }
    $ownsAssociation = $extensionValue -eq $AssociationProgId -and
        $commandValue.IndexOf($OwnedDirectory, [StringComparison]::OrdinalIgnoreCase) -ge 0
    if ($ownsAssociation) {
        Backup-RegistryKey $extension
        Backup-RegistryKey $progId
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
        $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
        foreach ($relativeValue in @($manifest.files)) {
            $relative = ([string]$relativeValue).Replace('\', '/').TrimStart('/')
            if ($sameDirectory -and $relative -in $NewPayloadFiles) { continue }
            $owned = [IO.Path]::GetFullPath((Join-Path $oldRoot $relative.Replace('/', '\')))
            if ($owned.StartsWith($oldRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
                Backup-OwnedFile $owned
                Remove-Item -LiteralPath $owned -Force -ErrorAction SilentlyContinue
            }
        }
    }
    foreach ($path in @($manifestPath, (Join-Path $oldRoot $legacyUninstallerName))) {
        Backup-OwnedFile $path
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }
    Get-ChildItem -LiteralPath $oldRoot -Directory -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

if ($Action -eq 'Rollback') {
    Restore-Migration
    exit 0
}
if ($Action -eq 'Commit') {
    if (Test-Path -LiteralPath $backupRoot) { Remove-Item -LiteralPath $backupRoot -Recurse -Force }
    exit 0
}

if ([string]::IsNullOrWhiteSpace($TargetDirectory)) { throw 'TargetDirectory is required for Apply.' }
if (Test-Path -LiteralPath $backupRoot) { throw "Migration backup already exists: $backupRoot" }
New-Item -ItemType Directory -Path $backupRoot | Out-Null
$script:state = [pscustomobject]@{ Files = @(); RegistryKeys = @() }
Write-MigrationState

try {
    # Reads the generated list of files now owned by the QtIFW component.
    $payloadList = Join-Path $TargetDirectory '_installer\payload-files.txt'
    $newPayloadFiles = if (Test-Path -LiteralPath $payloadList) {
        @(Get-Content -LiteralPath $payloadList -Encoding UTF8 | ForEach-Object { $_.Replace('\', '/').TrimStart('/') })
    } else { @() }

    foreach ($scope in @('CurrentUser', 'AllUsers')) {
        $legacy = Get-LegacyInstallation $scope
        if ($null -eq $legacy) { continue }
        $isSelectedScope = $scope -eq $SelectedScope
        if (-not $isSelectedScope -and $MigrationMode -ne 'move') { continue }

        Remove-LegacyPayload $legacy $newPayloadFiles
        $shortcuts = Get-LegacyShortcuts $scope
        foreach ($shortcut in @($shortcuts.Start, $shortcuts.Uninstall, $shortcuts.Desktop)) {
            Backup-OwnedFile $shortcut
            Remove-Item -LiteralPath $shortcut -Force -ErrorAction SilentlyContinue
        }
        if ((Test-Path -LiteralPath $shortcuts.Menu) -and -not (Get-ChildItem -LiteralPath $shortcuts.Menu -Force)) {
            Remove-Item -LiteralPath $shortcuts.Menu -Force -ErrorAction SilentlyContinue
        }
        Remove-LegacyAssociation $scope $legacy.Directory
        Backup-RegistryKey $legacy.Key
        Remove-Item -LiteralPath $legacy.Key -Recurse -Force -ErrorAction SilentlyContinue
    }
} catch {
    try { Restore-Migration } catch { Write-Warning "Automatic migration rollback failed: $_" }
    throw
}

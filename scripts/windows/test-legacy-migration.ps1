<#
.SYNOPSIS
Checks reversible migration from the stage-6 installer without touching a real installation.
.DESCRIPTION
Creates an isolated legacy payload, shortcuts, uninstall record and file association under
unique temporary names. The test verifies Apply plus Rollback and then Apply plus Commit.
#>
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
$projectRootFull = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
$migrationScript = Join-Path $projectRootFull 'installer\migrate-legacy.ps1'
if (-not (Test-Path -LiteralPath $migrationScript -PathType Leaf)) {
    throw "Invalid project root. Missing $migrationScript"
}

$identity = [Guid]::NewGuid().ToString('N')
$productId = "TechnicalDrawingMigrationTest-$identity"
$extension = ".td-migration-$identity"
$progId = "TechnicalDrawing.MigrationTest.$identity"
$testRoot = Join-Path $projectRootFull "build\legacy-migration-$identity"
$legacyRoot = Join-Path $testRoot 'legacy'
$targetRoot = Join-Path $testRoot 'target'
$programsRoot = Join-Path $testRoot 'programs'
$desktopRoot = Join-Path $testRoot 'desktop'
$backupRoot = Join-Path ([IO.Path]::GetTempPath()) "TechDraw-legacy-migration-test-$identity"
$uninstallKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$productId"
$extensionKey = "HKCU:\Software\Classes\$extension"
$progIdKey = "HKCU:\Software\Classes\$progId"

# Throws with a focused message instead of letting a later operation hide the failed assertion.
function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

# Recreates the exact subset of an IExpress installation consumed by the migration helper.
function New-FakeLegacyInstallation {
    New-Item -ItemType Directory -Force -Path $legacyRoot, (Join-Path $targetRoot '_installer'),
        (Join-Path $programsRoot 'Technical Drawing'), $desktopRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $legacyRoot 'owned.bin') -Value 'owned payload' -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $legacyRoot 'foreign.drw') -Value 'user document' -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $legacyRoot 'Uninstall-TechDraw.ps1') -Value '# legacy uninstaller' -Encoding UTF8
    @{ version = '0.0.0'; architecture = 'x64'; files = @('owned.bin') } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $legacyRoot '.techdraw-install.json') -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $targetRoot '_installer\payload-files.txt') -Value 'TechDraw.exe' -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $programsRoot 'Technical Drawing\Technical Drawing.lnk') -Value 'start' -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $programsRoot 'Technical Drawing\Uninstall Technical Drawing.lnk') -Value 'uninstall' -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $desktopRoot 'Technical Drawing.lnk') -Value 'desktop' -Encoding UTF8

    New-Item -Path $uninstallKey -Force | Out-Null
    New-ItemProperty -Path $uninstallKey -Name InstallLocation -Value $legacyRoot -Force | Out-Null
    New-ItemProperty -Path $uninstallKey -Name InstallerTechnology -Value 'IExpress' -Force | Out-Null
    New-Item -Path $extensionKey -Force | Out-Null
    Set-Item -LiteralPath $extensionKey -Value $progId
    New-Item -Path (Join-Path $progIdKey 'shell\open\command') -Force | Out-Null
    Set-Item -LiteralPath (Join-Path $progIdKey 'shell\open\command') -Value "`"$legacyRoot\TechDraw.exe`" `"%1`""
}

$commonArguments = @{
    Action = 'Apply'
    BackupDirectory = $backupRoot
    TargetDirectory = $targetRoot
    SelectedScope = 'CurrentUser'
    MigrationMode = 'move'
    RegistryProductId = $productId
    AssociationExtension = $extension
    AssociationProgId = $progId
    UserProgramsDirectory = $programsRoot
    UserDesktopDirectory = $desktopRoot
}

try {
    New-FakeLegacyInstallation
    & $migrationScript @commonArguments
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $legacyRoot 'owned.bin'))) 'Apply kept an owned payload file.'
    Assert-Condition (Test-Path -LiteralPath (Join-Path $legacyRoot 'foreign.drw')) 'Apply removed a foreign document.'
    Assert-Condition (-not (Test-Path -LiteralPath $uninstallKey)) 'Apply kept the legacy uninstall record.'
    Assert-Condition (-not (Test-Path -LiteralPath $extensionKey)) 'Apply kept the owned file association.'
    Assert-Condition (Test-Path -LiteralPath $backupRoot) 'Apply did not create its rollback archive.'

    & $migrationScript -Action Rollback -BackupDirectory $backupRoot
    Assert-Condition (Test-Path -LiteralPath (Join-Path $legacyRoot 'owned.bin')) 'Rollback did not restore the owned payload.'
    Assert-Condition (Test-Path -LiteralPath (Join-Path $legacyRoot '.techdraw-install.json')) 'Rollback did not restore the ownership manifest.'
    Assert-Condition (Test-Path -LiteralPath (Join-Path $programsRoot 'Technical Drawing\Technical Drawing.lnk')) 'Rollback did not restore the Start menu shortcut.'
    Assert-Condition (Test-Path -LiteralPath $uninstallKey) 'Rollback did not restore the uninstall record.'
    Assert-Condition (Test-Path -LiteralPath $extensionKey) 'Rollback did not restore the file association.'
    Assert-Condition (-not (Test-Path -LiteralPath $backupRoot)) 'Rollback kept its temporary archive.'

    & $migrationScript @commonArguments
    & $migrationScript -Action Commit -BackupDirectory $backupRoot
    Assert-Condition (-not (Test-Path -LiteralPath $backupRoot)) 'Commit kept its temporary archive.'
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $legacyRoot 'owned.bin'))) 'Commit restored a removed legacy file.'
    Assert-Condition (Test-Path -LiteralPath (Join-Path $legacyRoot 'foreign.drw')) 'Commit removed a foreign document.'
    Write-Output 'Legacy installer migration apply, rollback and commit verified.'
} finally {
    Remove-Item -LiteralPath $uninstallKey, $extensionKey, $progIdKey -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $testRoot, $backupRoot -Recurse -Force -ErrorAction SilentlyContinue
}

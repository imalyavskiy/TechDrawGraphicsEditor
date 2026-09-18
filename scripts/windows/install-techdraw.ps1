<#
.SYNOPSIS
Installs the portable payload for the current user and optionally creates Windows shell integration.
.PARAMETER InstallDirectory
Absolute or relative destination; drive roots are rejected before any cleanup.
.PARAMETER SkipShellIntegration
Skips shortcuts and the uninstall registry entry for isolated verification.
.PARAMETER Quiet
Suppresses the final success dialog.
.OUTPUTS
Application files in InstallDirectory and, unless skipped, user shortcuts and an uninstall entry.
.NOTES
The script is embedded in the generated installer. Any exception produces a nonzero exit code.
#>
param(
    [string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'Programs\Technical Drawing'),
    [switch]$SkipShellIntegration,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

# The generated EXE extracts this script and payload.zip into a temporary directory.
$payloadArchive = Join-Path $PSScriptRoot 'payload.zip'
$installDirectory = [IO.Path]::GetFullPath($InstallDirectory)
if ($installDirectory.TrimEnd('\') -eq [IO.Path]::GetPathRoot($installDirectory).TrimEnd('\')) {
    throw 'Refusing to use a drive root as the installation directory.'
}
$stagingDirectory = Join-Path $env:TEMP ("TechnicalDrawing-install-" + [Guid]::NewGuid().ToString('N'))
$desktopShortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Technical Drawing.lnk'
$programsDirectory = [Environment]::GetFolderPath('Programs')
$startMenuDirectory = Join-Path $programsDirectory 'Technical Drawing'
$startMenuShortcut = Join-Path $startMenuDirectory 'Technical Drawing.lnk'
$uninstallShortcut = Join-Path $startMenuDirectory 'Uninstall Technical Drawing.lnk'
$uninstallRegistryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\TechnicalDrawing'

if (-not (Test-Path -LiteralPath $payloadArchive)) {
    throw 'Installer payload.zip is missing.'
}

try {
    # Expand into a unique staging directory before replacing files in the validated destination.
    New-Item -ItemType Directory -Force -Path $stagingDirectory | Out-Null
    Expand-Archive -LiteralPath $payloadArchive -DestinationPath $stagingDirectory -Force

    New-Item -ItemType Directory -Force -Path $installDirectory | Out-Null
    Get-ChildItem -LiteralPath $installDirectory -Force | Remove-Item -Recurse -Force
    Copy-Item -Path (Join-Path $stagingDirectory '*') -Destination $installDirectory -Recurse -Force

    # Install a self-contained uninstaller that removes only this user's files and registrations.
    $uninstallScriptPath = Join-Path $installDirectory 'Uninstall-TechDraw.ps1'
    $uninstallScript = @'
$ErrorActionPreference = 'Stop'
# The script derives its installation directory from its own fixed location.
$installDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$desktopShortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Technical Drawing.lnk'
$startMenuDirectory = Join-Path ([Environment]::GetFolderPath('Programs')) 'Technical Drawing'
$registryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\TechnicalDrawing'
Add-Type -AssemblyName System.Windows.Forms
$answer = [System.Windows.Forms.MessageBox]::Show(
    'Удалить «Технический рисунок / Technical Draw»?',
    'Удаление Technical Drawing',
    [System.Windows.Forms.MessageBoxButtons]::YesNo,
    [System.Windows.Forms.MessageBoxIcon]::Question)
if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { exit 0 }
# Remove shell integration first, then defer deletion of the running script's directory.
Remove-Item -LiteralPath $desktopShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startMenuDirectory -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $registryPath -Recurse -Force -ErrorAction SilentlyContinue
$cleanup = Join-Path $env:TEMP ('TechnicalDrawing-uninstall-' + [Guid]::NewGuid().ToString('N') + '.cmd')
@("@echo off", "timeout /t 2 /nobreak >nul", "rmdir /s /q `"$installDirectory`"", 'del "%~f0"') |
    Set-Content -LiteralPath $cleanup -Encoding ascii
Start-Process -FilePath $env:ComSpec -ArgumentList @('/d', '/c', "`"$cleanup`"") -WindowStyle Hidden
'@
    Set-Content -LiteralPath $uninstallScriptPath -Value $uninstallScript -Encoding utf8

    if (-not $SkipShellIntegration) {
        # Shortcuts use the installation directory as their working directory for portable dependencies.
        New-Item -ItemType Directory -Force -Path $startMenuDirectory | Out-Null
        $shell = New-Object -ComObject WScript.Shell
        foreach ($shortcutPath in @($desktopShortcut, $startMenuShortcut)) {
            $shortcut = $shell.CreateShortcut($shortcutPath)
            $shortcut.TargetPath = Join-Path $installDirectory 'TechDraw.exe'
            $shortcut.WorkingDirectory = $installDirectory
            $shortcut.IconLocation = (Join-Path $installDirectory 'TechDraw.exe') + ',0'
            $shortcut.Save()
        }
        $shortcut = $shell.CreateShortcut($uninstallShortcut)
        $shortcut.TargetPath = 'powershell.exe'
        $shortcut.Arguments = "-NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$uninstallScriptPath`""
        $shortcut.WorkingDirectory = $installDirectory
        $shortcut.Save()

        # Register a per-user uninstall entry without requesting administrator rights.
        New-Item -Path $uninstallRegistryPath -Force | Out-Null
        New-ItemProperty -Path $uninstallRegistryPath -Name DisplayName -Value 'Технический рисунок / Technical Draw' -Force |
            Out-Null
        New-ItemProperty -Path $uninstallRegistryPath -Name DisplayVersion -Value '0.1.0' -Force | Out-Null
        New-ItemProperty -Path $uninstallRegistryPath -Name Publisher -Value 'Technical Drawing' -Force | Out-Null
        New-ItemProperty -Path $uninstallRegistryPath -Name DisplayIcon -Value (Join-Path $installDirectory 'TechDraw.exe') -Force |
            Out-Null
        $uninstallCommand = "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$uninstallScriptPath`""
        New-ItemProperty -Path $uninstallRegistryPath -Name UninstallString -Value $uninstallCommand -Force | Out-Null
        New-ItemProperty -Path $uninstallRegistryPath -Name NoModify -PropertyType DWord -Value 1 -Force | Out-Null
        New-ItemProperty -Path $uninstallRegistryPath -Name NoRepair -PropertyType DWord -Value 1 -Force | Out-Null
    }

    if (-not $Quiet) {
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.MessageBox]::Show(
            'Программа «Технический рисунок / Technical Draw» установлена.',
            'Technical Drawing',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null
    }
} finally {
    # Cleanup is best-effort because installation errors must remain the primary failure.
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue
}

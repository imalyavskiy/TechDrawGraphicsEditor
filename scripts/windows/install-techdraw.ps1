<#
.SYNOPSIS
Installs, repairs, updates or removes Technical Drawing from an IExpress payload.
.DESCRIPTION
Interactive mode uses a localized Windows Forms wizard. Quiet mode exists for automated checks.
Owned files are recorded in a private manifest so foreign files in a custom folder survive updates
and uninstall.
.PARAMETER InstallDirectory
Destination override. The default follows Scope.
.PARAMETER Scope
CurrentUser (default) or AllUsers. AllUsers is elevated before any system change.
.PARAMETER Language
English or Russian. The default follows the saved choice and then Windows UI culture.
.PARAMETER Quiet
Runs without custom UI, never launches the application and refuses an implicit downgrade.
.PARAMETER SkipShellIntegration
Skips shortcuts, registry and file association for isolated tests.
.PARAMETER Uninstall
Removes the owned files and registrations of the selected installation.
.PARAMETER RemoveSettings
During uninstall, also removes Technical Drawing settings for the current interactive user.
.NOTES
The script targets Windows PowerShell 5.1 and is copied beside the program for uninstall.
#>
param(
    [string]$InstallDirectory,
    [ValidateSet('CurrentUser', 'AllUsers')][string]$Scope = 'CurrentUser',
    [ValidateSet('English', 'Russian')][string]$Language,
    [switch]$DesktopShortcut,
    [switch]$TaskbarIntent,
    [switch]$AssociateDrw,
    [switch]$Launch,
    [switch]$AllowDowngrade,
    [switch]$Repair,
    [switch]$KeepOtherScope,
    [switch]$MigrateOtherScope,
    [switch]$SkipShellIntegration,
    [switch]$Quiet,
    [switch]$Uninstall,
    [switch]$RemoveSettings,
    [string]$StateFile,
    [switch]$ElevatedContinuation
)

$ErrorActionPreference = 'Stop'
$script:ProductId = 'TechnicalDrawing'
$script:DisplayName = 'Технический рисунок / Technical Draw'
$script:ManifestName = '.techdraw-install.json'
$script:UninstallScriptName = 'Uninstall-TechDraw.ps1'

# Returns true when this process has an elevated administrator token.
function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Returns one localized installer string by its stable identifier.
function Get-Text {
    param([Parameter(Mandatory = $true)][string]$Key, [object[]]$Arguments = @())
    $english = @{
        Title='Technical Drawing Setup'; Back='< Back'; Next='Next >'; Install='Install'; Remove='Remove'; Cancel='Cancel'; Browse='Browse...'
        LanguageTitle='Installer language'; LanguagePrompt='Choose the language used by this installer.'; English='English'; Russian='Русский'
        LicenseTitle='Prototype notice'; LicenseText="PROTOTYPE NOTICE`r`n`r`nThis build is not even an Alpha version. The real software license has not been selected yet.`r`n`r`nThis placeholder is not legal terms and grants no rights to use, modify, or distribute the program. It does not replace a LICENSE file."
        ScopeTitle='Installation scope and folder'; CurrentUser='Only for me'; AllUsers='For all users (administrator rights required)'; Folder='Installation folder:'
        OptionsTitle='Shortcuts and file association'; Start='Start menu (required)'; Desktop='Desktop'; Taskbar='Ask Windows to pin to taskbar on first launch'; Associate='Associate .drw files with Technical Drawing'
        FinalTitle='Ready to install'; Launch='Launch Technical Drawing after installation'; Ready='Click Install to begin. No more settings will be requested.'
        InvalidPath='Choose an absolute folder other than a drive root.'; NoWrite='The selected folder is not writable.'; NoSpace='The selected drive does not have enough free space.'
        Error='Installation error'; Installed='Technical Drawing was installed successfully.'; ExistingSame='The same version {0} is already installed. Repair it?'
        Downgrade='Version {0} is installed, but this installer contains older version {1}. Projects made by the newer version may be incompatible. Continue with downgrade?'
        OtherScope='Technical Drawing is installed for {0}. Yes: migrate it to {1}. No: keep both copies. Cancel: stop installation.'
        ScopeCurrent='the current user'; ScopeAll='all users'; UninstallTitle='Remove Technical Drawing'
        RemoveSettings='Delete application settings for the current user'; UninstallPrompt='Remove Technical Drawing? Projects, exported images and other documents will be preserved.'
        Removed='Technical Drawing was removed.'
    }
    $russian = @{
        Title='Установка «Технического рисунка»'; Back='< Назад'; Next='Далее >'; Install='Установить'; Remove='Удалить'; Cancel='Отмена'; Browse='Обзор...'
        LanguageTitle='Язык установщика'; LanguagePrompt='Выберите язык этого установщика.'; English='English'; Russian='Русский'
        LicenseTitle='Уведомление о прототипе'; LicenseText="УВЕДОМЛЕНИЕ О ПРОТОТИПЕ`r`n`r`nЭта сборка не является даже Alpha-версией. Настоящая лицензия программы ещё не выбрана.`r`n`r`nЭтот текст-заглушка не является юридическими условиями и не предоставляет прав на использование, изменение или распространение программы. Он не заменяет файл LICENSE."
        ScopeTitle='Область и каталог установки'; CurrentUser='Только для меня'; AllUsers='Для всех пользователей (нужны права администратора)'; Folder='Каталог установки:'
        OptionsTitle='Ярлыки и связь с файлами'; Start='Меню «Пуск» (обязательно)'; Desktop='Рабочий стол'; Taskbar='Запросить закрепление на панели задач при первом запуске'; Associate='Связать файлы .drw с «Техническим рисунком»'
        FinalTitle='Всё готово к установке'; Launch='Запустить «Технический рисунок» после установки'; Ready='Нажмите «Установить». Других вопросов о настройках не будет.'
        InvalidPath='Выберите абсолютный каталог, отличный от корня диска.'; NoWrite='В выбранный каталог нельзя записать данные.'; NoSpace='На выбранном диске недостаточно свободного места.'
        Error='Ошибка установки'; Installed='«Технический рисунок» успешно установлен.'; ExistingSame='Та же версия {0} уже установлена. Восстановить её?'
        Downgrade='Установлена версия {0}, а установщик содержит более старую версию {1}. Проекты новой версии могут оказаться несовместимыми. Продолжить downgrade?'
        OtherScope='«Технический рисунок» установлен в области «{0}». Да: перенести в «{1}». Нет: оставить обе копии. Отмена: прекратить установку.'
        ScopeCurrent='текущий пользователь'; ScopeAll='все пользователи'; UninstallTitle='Удаление «Технического рисунка»'
        RemoveSettings='Удалить настройки приложения текущего пользователя'; UninstallPrompt='Удалить «Технический рисунок»? Проекты, экспортированные изображения и другие документы будут сохранены.'
        Removed='«Технический рисунок» удалён.'
    }
    $dictionary = if ($script:SelectedLanguage -eq 'Russian') { $russian } else { $english }
    $value = [string]$dictionary[$Key]
    if ($Arguments.Count -gt 0) { return [string]::Format($value, $Arguments) }
    return $value
}

# Returns the uninstall registry path for one scope.
function Get-UninstallRegistryPath {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$RequestedScope)
    $hive = if ($RequestedScope -eq 'AllUsers') { 'HKLM:' } else { 'HKCU:' }
    return "$hive\Software\Microsoft\Windows\CurrentVersion\Uninstall\$script:ProductId"
}

# Reads one registered installation without assuming its files still exist.
function Get-Installation {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$RequestedScope)
    $path = Get-UninstallRegistryPath $RequestedScope
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    $item = Get-ItemProperty -LiteralPath $path
    return [pscustomobject]@{ Scope=$RequestedScope; RegistryPath=$path; Directory=[string]$item.InstallLocation; Version=[string]$item.DisplayVersion; Language=[string]$item.InstallerLanguage }
}

# Resolves the documented default folder for a scope.
function Get-DefaultInstallDirectory {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$RequestedScope)
    if ($RequestedScope -eq 'AllUsers') { return (Join-Path $env:ProgramFiles 'Technical Drawing') }
    return (Join-Path $env:LOCALAPPDATA 'Programs\Technical Drawing')
}

# Maps a scope to the Windows special folders used for shortcuts.
function Get-ShortcutPaths {
    param([ValidateSet('CurrentUser', 'AllUsers')][string]$RequestedScope)
    $programs = if ($RequestedScope -eq 'AllUsers') { 'CommonPrograms' } else { 'Programs' }
    $desktop = if ($RequestedScope -eq 'AllUsers') { 'CommonDesktopDirectory' } else { 'Desktop' }
    $menu = Join-Path ([Environment]::GetFolderPath($programs)) 'Technical Drawing'
    return [pscustomobject]@{ Menu=$menu; Start=(Join-Path $menu 'Technical Drawing.lnk'); Uninstall=(Join-Path $menu 'Uninstall Technical Drawing.lnk'); Desktop=(Join-Path ([Environment]::GetFolderPath($desktop)) 'Technical Drawing.lnk') }
}

# Validates path, write access and conservative free-space requirements.
function Test-InstallDestination {
    param([string]$Path, [long]$RequiredBytes)
    if (-not [IO.Path]::IsPathRooted($Path)) { throw (Get-Text InvalidPath) }
    $full = [IO.Path]::GetFullPath($Path)
    if ($full.TrimEnd('\') -eq [IO.Path]::GetPathRoot($full).TrimEnd('\')) { throw (Get-Text InvalidPath) }
    $drive = New-Object IO.DriveInfo([IO.Path]::GetPathRoot($full))
    if ($drive.AvailableFreeSpace -lt ($RequiredBytes * 3)) { throw (Get-Text NoSpace) }
    New-Item -ItemType Directory -Force -Path $full | Out-Null
    $probe = Join-Path $full ('.techdraw-write-' + [Guid]::NewGuid().ToString('N'))
    try { [IO.File]::WriteAllText($probe, 'probe') } catch { throw (Get-Text NoWrite) }
    finally { Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue }
    return $full
}

# Creates one Windows shortcut with a stable working directory and icon.
function New-Shortcut {
    param([string]$Path, [string]$Target, [string]$WorkingDirectory, [string]$Arguments='')
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    $shell = New-Object -ComObject WScript.Shell; $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath=$Target; $shortcut.WorkingDirectory=$WorkingDirectory; $shortcut.Arguments=$Arguments; $shortcut.IconLocation="$Target,0"; $shortcut.Save()
}

# Creates required shortcuts and the Add/Remove Programs registration.
function Set-ShellIntegration {
    param($State, $Metadata)
    $paths=Get-ShortcutPaths $State.Scope; $exe=Join-Path $State.InstallDirectory $Metadata.executable; $uninstaller=Join-Path $State.InstallDirectory $script:UninstallScriptName
    New-Shortcut $paths.Start $exe $State.InstallDirectory
    $uninstallArgs="-NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$uninstaller`" -Uninstall -Scope $($State.Scope) -InstallDirectory `"$($State.InstallDirectory)`""
    New-Shortcut $paths.Uninstall 'powershell.exe' $State.InstallDirectory $uninstallArgs
    if ($State.DesktopShortcut) { New-Shortcut $paths.Desktop $exe $State.InstallDirectory } else { Remove-Item -LiteralPath $paths.Desktop -Force -ErrorAction SilentlyContinue }
    $registry=Get-UninstallRegistryPath $State.Scope; New-Item -Path $registry -Force | Out-Null
    $values=@{ DisplayName=$script:DisplayName; DisplayVersion=[string]$Metadata.version; Publisher='Technical Drawing'; DisplayIcon=$exe; InstallLocation=$State.InstallDirectory; InstallerLanguage=$State.Language; InstallArchitecture=[string]$Metadata.architecture; DesktopShortcut=[int][bool]$State.DesktopShortcut; AssociateDrw=[int][bool]$State.AssociateDrw; UninstallString="powershell.exe $uninstallArgs" }
    foreach($entry in $values.GetEnumerator()){ New-ItemProperty -Path $registry -Name $entry.Key -Value $entry.Value -Force | Out-Null }
    New-ItemProperty -Path $registry -Name NoModify -PropertyType DWord -Value 1 -Force | Out-Null
    New-ItemProperty -Path $registry -Name NoRepair -PropertyType DWord -Value 1 -Force | Out-Null
}

# Removes the .drw association only when it still points to this installation.
function Remove-OwnedFileAssociation {
    param([string]$RequestedScope,[string]$OwnedDirectory)
    $classes=if($RequestedScope -eq 'AllUsers'){'HKLM:\Software\Classes'}else{'HKCU:\Software\Classes'}
    $extension=Join-Path $classes '.drw'; $progId=Join-Path $classes 'TechnicalDrawing.Project'; $commandPath=Join-Path $progId 'shell\open\command'
    $current=if(Test-Path -LiteralPath $extension){[string](Get-Item -LiteralPath $extension).GetValue('')}else{''}
    $command=if(Test-Path -LiteralPath $commandPath){[string](Get-Item -LiteralPath $commandPath).GetValue('')}else{''}
    if($current -eq 'TechnicalDrawing.Project' -and $command -like "*$OwnedDirectory*"){ Remove-Item -LiteralPath $extension,$progId -Recurse -Force -ErrorAction SilentlyContinue }
}

# Applies or withdraws the association in the selected scope.
function Set-FileAssociation {
    param($State,$Metadata)
    if(-not $State.AssociateDrw){ Remove-OwnedFileAssociation $State.Scope $State.InstallDirectory; return }
    $classes=if($State.Scope -eq 'AllUsers'){'HKLM:\Software\Classes'}else{'HKCU:\Software\Classes'}; $extension=Join-Path $classes '.drw'; $progId=Join-Path $classes 'TechnicalDrawing.Project'; $exe=Join-Path $State.InstallDirectory $Metadata.executable
    New-Item -Path $extension -Force | Out-Null; Set-Item -LiteralPath $extension -Value 'TechnicalDrawing.Project'
    New-Item -Path $progId -Force | Out-Null; Set-Item -LiteralPath $progId -Value 'Technical Drawing project'
    New-Item -Path (Join-Path $progId 'DefaultIcon') -Force | Out-Null; Set-Item -LiteralPath (Join-Path $progId 'DefaultIcon') -Value "$exe,0"
    New-Item -Path (Join-Path $progId 'shell\open\command') -Force | Out-Null; Set-Item -LiteralPath (Join-Path $progId 'shell\open\command') -Value "`"$exe`" `"%1`""
}

# Removes only files named by the private manifest and product-owned registrations.
function Remove-OwnedInstallation {
    param([string]$RequestedScope,[string]$Directory,[bool]$DeleteRegistration=$true)
    if([string]::IsNullOrWhiteSpace($Directory)){return}; $full=[IO.Path]::GetFullPath($Directory); $manifestPath=Join-Path $full $script:ManifestName
    if(Test-Path -LiteralPath $manifestPath){ $manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json; foreach($relative in @($manifest.files)){ $owned=Join-Path $full ([string]$relative).Replace('/','\'); if($owned.StartsWith($full,[StringComparison]::OrdinalIgnoreCase)){Remove-Item -LiteralPath $owned -Force -ErrorAction SilentlyContinue} }; Remove-Item -LiteralPath $manifestPath -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath (Join-Path $full $script:UninstallScriptName) -Force -ErrorAction SilentlyContinue
    Get-ChildItem -LiteralPath $full -Directory -Recurse -ErrorAction SilentlyContinue|Sort-Object FullName -Descending|Where-Object{-not(Get-ChildItem -LiteralPath $_.FullName -Force)}|Remove-Item -Force -ErrorAction SilentlyContinue
    if((Test-Path -LiteralPath $full) -and -not(Get-ChildItem -LiteralPath $full -Force)){Remove-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue}
    if($DeleteRegistration){ $paths=Get-ShortcutPaths $RequestedScope; Remove-Item -LiteralPath $paths.Start,$paths.Uninstall,$paths.Desktop -Force -ErrorAction SilentlyContinue; if((Test-Path -LiteralPath $paths.Menu) -and -not(Get-ChildItem -LiteralPath $paths.Menu -Force)){Remove-Item -LiteralPath $paths.Menu -Force -ErrorAction SilentlyContinue}; Remove-OwnedFileAssociation $RequestedScope $full; Remove-Item -LiteralPath (Get-UninstallRegistryPath $RequestedScope) -Recurse -Force -ErrorAction SilentlyContinue }
}

# Replaces the complete owned payload, restores previous owned files on failure, and preserves foreign files.
function Install-Payload {
    param($State,$Metadata,[string]$Archive)
    $stage=Join-Path $env:TEMP ('TechnicalDrawing-stage-'+[Guid]::NewGuid().ToString('N')); $backup=Join-Path $env:TEMP ('TechnicalDrawing-backup-'+[Guid]::NewGuid().ToString('N')); $directory=Test-InstallDestination $State.InstallDirectory ((Get-Item -LiteralPath $Archive).Length)
    try{
        New-Item -ItemType Directory -Path $stage,$backup|Out-Null; Expand-Archive -LiteralPath $Archive -DestinationPath $stage -Force
        $manifestPath=Join-Path $directory $script:ManifestName; $oldFiles=@(); if(Test-Path -LiteralPath $manifestPath){$oldFiles=@((Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json).files)}
        foreach($relative in $oldFiles){$old=Join-Path $directory ([string]$relative).Replace('/','\'); if(Test-Path -LiteralPath $old -PathType Leaf){$saved=Join-Path $backup ([string]$relative).Replace('/','\'); New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saved)|Out-Null; Copy-Item -LiteralPath $old -Destination $saved -Force}}
        foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){$relative=$file.FullName.Substring($stage.Length).TrimStart('\'); $destination=Join-Path $directory $relative; New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination)|Out-Null; Copy-Item -LiteralPath $file.FullName -Destination $destination -Force}
        foreach($relative in $oldFiles){if([string]$relative -notin @($Metadata.files)){Remove-Item -LiteralPath (Join-Path $directory ([string]$relative).Replace('/','\')) -Force -ErrorAction SilentlyContinue}}
        Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $directory $script:UninstallScriptName) -Force
        [ordered]@{version=$Metadata.version;architecture=$Metadata.architecture;files=@($Metadata.files)}|ConvertTo-Json -Depth 3|Set-Content -LiteralPath $manifestPath -Encoding utf8
    }catch{foreach($file in Get-ChildItem -LiteralPath $backup -Recurse -File -ErrorAction SilentlyContinue){$relative=$file.FullName.Substring($backup.Length).TrimStart('\');$destination=Join-Path $directory $relative;New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination)|Out-Null;Copy-Item -LiteralPath $file.FullName -Destination $destination -Force};throw}finally{Remove-Item -LiteralPath $stage,$backup -Recurse -Force -ErrorAction SilentlyContinue}
}

# Serializes the chosen options and waits for the elevated continuation to finish.
function Invoke-ElevatedInstallation {
    param($State)
    $file=Join-Path $env:TEMP ('TechnicalDrawing-state-'+[Guid]::NewGuid().ToString('N')+'.json'); $State|ConvertTo-Json|Set-Content -LiteralPath $file -Encoding utf8
    try{$arguments="-NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -StateFile `"$file`" -ElevatedContinuation -Quiet";$process=Start-Process powershell.exe -ArgumentList $arguments -Verb RunAs -Wait -PassThru;if($process.ExitCode -ne 0){throw "Elevated installer failed with exit code $($process.ExitCode)."}}finally{Remove-Item -LiteralPath $file -Force -ErrorAction SilentlyContinue}
}

# Shows the five-page localized wizard and returns its selected installation state.
function Show-InstallWizard {
    param($Metadata,$CurrentInstall,$AllInstall)
    Add-Type -AssemblyName System.Windows.Forms; Add-Type -AssemblyName System.Drawing
    $form=New-Object Windows.Forms.Form; $form.Size=New-Object Drawing.Size(620,430);$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false;$form.MinimizeBox=$false;$form.Font=New-Object Drawing.Font('Segoe UI',9)
    $content=New-Object Windows.Forms.Panel;$content.SetBounds(20,15,565,320);$form.Controls.Add($content)
    $back=New-Object Windows.Forms.Button;$back.SetBounds(315,345,85,28);$form.Controls.Add($back)
    $next=New-Object Windows.Forms.Button;$next.SetBounds(405,345,85,28);$form.Controls.Add($next)
    $cancel=New-Object Windows.Forms.Button;$cancel.SetBounds(500,345,85,28);$form.Controls.Add($cancel);$form.CancelButton=$cancel

    $languagePanel=New-Object Windows.Forms.Panel;$languagePanel.Dock='Fill'
    $languageTitle=New-Object Windows.Forms.Label;$languageTitle.SetBounds(0,10,540,25);$languageTitle.Font=New-Object Drawing.Font('Segoe UI',12,[Drawing.FontStyle]::Bold)
    $languagePrompt=New-Object Windows.Forms.Label;$languagePrompt.SetBounds(0,50,540,25)
    $languageChoice=New-Object Windows.Forms.ComboBox;$languageChoice.SetBounds(0,85,220,25);$languageChoice.DropDownStyle='DropDownList';[void]$languageChoice.Items.AddRange(@('English','Русский'));$languageChoice.SelectedIndex=if($script:SelectedLanguage -eq 'Russian'){1}else{0};$languagePanel.Controls.AddRange(@($languageTitle,$languagePrompt,$languageChoice))

    $licensePanel=New-Object Windows.Forms.Panel;$licensePanel.Dock='Fill'
    $licenseTitle=New-Object Windows.Forms.Label;$licenseTitle.SetBounds(0,10,540,25);$licenseTitle.Font=New-Object Drawing.Font('Segoe UI',12,[Drawing.FontStyle]::Bold)
    $licenseText=New-Object Windows.Forms.TextBox;$licenseText.SetBounds(0,48,540,235);$licenseText.Multiline=$true;$licenseText.ReadOnly=$true;$licenseText.ScrollBars='Vertical';$licensePanel.Controls.AddRange(@($licenseTitle,$licenseText))

    $scopePanel=New-Object Windows.Forms.Panel;$scopePanel.Dock='Fill'
    $scopeTitle=New-Object Windows.Forms.Label;$scopeTitle.SetBounds(0,10,540,25);$scopeTitle.Font=New-Object Drawing.Font('Segoe UI',12,[Drawing.FontStyle]::Bold)
    $currentRadio=New-Object Windows.Forms.RadioButton;$currentRadio.SetBounds(0,55,400,24);$currentRadio.Checked=$true
    $allRadio=New-Object Windows.Forms.RadioButton;$allRadio.SetBounds(0,85,500,24)
    $folderLabel=New-Object Windows.Forms.Label;$folderLabel.SetBounds(0,135,500,20)
    $folderBox=New-Object Windows.Forms.TextBox;$folderBox.SetBounds(0,160,440,25);$folderBox.Text=Get-DefaultInstallDirectory CurrentUser
    $browse=New-Object Windows.Forms.Button;$browse.SetBounds(450,158,90,27);$scopePanel.Controls.AddRange(@($scopeTitle,$currentRadio,$allRadio,$folderLabel,$folderBox,$browse))

    $optionsPanel=New-Object Windows.Forms.Panel;$optionsPanel.Dock='Fill'
    $optionsTitle=New-Object Windows.Forms.Label;$optionsTitle.SetBounds(0,10,540,25);$optionsTitle.Font=New-Object Drawing.Font('Segoe UI',12,[Drawing.FontStyle]::Bold)
    $startCheck=New-Object Windows.Forms.CheckBox;$startCheck.SetBounds(0,60,520,24);$startCheck.Checked=$true;$startCheck.Enabled=$false
    $desktopCheck=New-Object Windows.Forms.CheckBox;$desktopCheck.SetBounds(0,95,520,24)
    $taskbarCheck=New-Object Windows.Forms.CheckBox;$taskbarCheck.SetBounds(0,130,540,24)
    $associationCheck=New-Object Windows.Forms.CheckBox;$associationCheck.SetBounds(0,165,540,24);$associationCheck.Checked=$true;$optionsPanel.Controls.AddRange(@($optionsTitle,$startCheck,$desktopCheck,$taskbarCheck,$associationCheck))

    $finalPanel=New-Object Windows.Forms.Panel;$finalPanel.Dock='Fill'
    $finalTitle=New-Object Windows.Forms.Label;$finalTitle.SetBounds(0,10,540,25);$finalTitle.Font=New-Object Drawing.Font('Segoe UI',12,[Drawing.FontStyle]::Bold)
    $launchCheck=New-Object Windows.Forms.CheckBox;$launchCheck.SetBounds(0,70,540,24);$launchCheck.Checked=$true
    $readyLabel=New-Object Windows.Forms.Label;$readyLabel.SetBounds(0,115,540,50);$finalPanel.Controls.AddRange(@($finalTitle,$launchCheck,$readyLabel))

    $clean=($null -eq $CurrentInstall -and $null -eq $AllInstall);$pages=if($clean){@($languagePanel,$licensePanel,$scopePanel,$optionsPanel,$finalPanel)}else{@($languagePanel,$scopePanel,$optionsPanel,$finalPanel)}
    $script:WizardPage=0;$script:WizardResult=$null;$script:WizardPreflight=$null
    # Rewrites every custom caption after a language change.
    $localize={
        $form.Text=Get-Text Title;$back.Text=Get-Text Back;$cancel.Text=Get-Text Cancel;$languageTitle.Text=Get-Text LanguageTitle;$languagePrompt.Text=Get-Text LanguagePrompt
        $licenseTitle.Text=Get-Text LicenseTitle;$licenseText.Text=Get-Text LicenseText;$scopeTitle.Text=Get-Text ScopeTitle;$currentRadio.Text=Get-Text CurrentUser;$allRadio.Text=Get-Text AllUsers;$folderLabel.Text=Get-Text Folder;$browse.Text=Get-Text Browse
        $optionsTitle.Text=Get-Text OptionsTitle;$startCheck.Text=Get-Text Start;$desktopCheck.Text=Get-Text Desktop;$taskbarCheck.Text=Get-Text Taskbar;$associationCheck.Text=Get-Text Associate
        $finalTitle.Text=Get-Text FinalTitle;$launchCheck.Text=Get-Text Launch;$readyLabel.Text=Get-Text Ready;$next.Text=if($script:WizardPage -eq $pages.Count-1){Get-Text Install}else{Get-Text Next}
    }
    # Switches the content panel without creating a second state for any option.
    $showPage={$content.Controls.Clear();$content.Controls.Add($pages[$script:WizardPage]);$pages[$script:WizardPage].BringToFront();$back.Enabled=$script:WizardPage -gt 0;&$localize}
    $languageChoice.Add_SelectedIndexChanged({$script:SelectedLanguage=if($languageChoice.SelectedIndex -eq 1){'Russian'}else{'English'};&$localize})
    $currentRadio.Add_CheckedChanged({if($currentRadio.Checked){$folderBox.Text=Get-DefaultInstallDirectory CurrentUser}})
    $allRadio.Add_CheckedChanged({if($allRadio.Checked){$folderBox.Text=Get-DefaultInstallDirectory AllUsers}})
    $browse.Add_Click({$dialog=New-Object Windows.Forms.FolderBrowserDialog;$dialog.SelectedPath=$folderBox.Text;if($dialog.ShowDialog() -eq 'OK'){$folderBox.Text=$dialog.SelectedPath}})
    $back.Add_Click({if($script:WizardPage -gt 0){$script:WizardPage--;&$showPage}})
    $cancel.Add_Click({$form.DialogResult='Cancel';$form.Close()})
    $next.Add_Click({
        if($pages[$script:WizardPage] -eq $optionsPanel){
            try{$chosenDirectory=[IO.Path]::GetFullPath($folderBox.Text);if(-not[IO.Path]::IsPathRooted($folderBox.Text) -or $chosenDirectory.TrimEnd('\') -eq [IO.Path]::GetPathRoot($chosenDirectory).TrimEnd('\')){throw 'invalid'}}catch{[Windows.Forms.MessageBox]::Show((Get-Text InvalidPath),(Get-Text Error),'OK','Error')|Out-Null;return}
            $script:WizardPreflight=[pscustomobject]@{Scope=if($allRadio.Checked){'AllUsers'}else{'CurrentUser'};InstallDirectory=$chosenDirectory;Language=$script:SelectedLanguage;DesktopShortcut=$desktopCheck.Checked;TaskbarIntent=$taskbarCheck.Checked;AssociateDrw=$associationCheck.Checked;Launch=$launchCheck.Checked;AllowDowngrade=$false;Repair=$false;KeepOtherScope=$false;MigrateOtherScope=$false;SkipShellIntegration=$false}
            if(-not(Confirm-InstallMode $script:WizardPreflight $Metadata $CurrentInstall $AllInstall)){return}
        }
        if($script:WizardPage -lt $pages.Count-1){$script:WizardPage++;&$showPage;return}
        try{$chosenDirectory=[IO.Path]::GetFullPath($folderBox.Text)}catch{[Windows.Forms.MessageBox]::Show((Get-Text InvalidPath),(Get-Text Error),'OK','Error')|Out-Null;return}
        $script:WizardResult=$script:WizardPreflight;$script:WizardResult.InstallDirectory=$chosenDirectory;$script:WizardResult.Launch=$launchCheck.Checked
        $form.DialogResult='OK';$form.Close()
    })
    &$showPage;if($form.ShowDialog() -ne 'OK'){return $null};return $script:WizardResult
}

# Resolves repair, downgrade and cross-scope choices before files are changed.
function Confirm-InstallMode {
    param($State,$Metadata,$CurrentInstall,$AllInstall)
    $same=if($State.Scope -eq 'AllUsers'){$AllInstall}else{$CurrentInstall};$other=if($State.Scope -eq 'AllUsers'){$CurrentInstall}else{$AllInstall}
    if($same){
        $installedVersion=[version]$same.Version;$packageVersion=[version]$Metadata.version
        if($installedVersion -eq $packageVersion -and -not $State.Repair){if($Quiet){$State.Repair=$true}else{Add-Type -AssemblyName System.Windows.Forms;if([Windows.Forms.MessageBox]::Show((Get-Text ExistingSame @($same.Version)),(Get-Text Title),'YesNo','Question') -ne 'Yes'){return $false};$State.Repair=$true}}
        if($installedVersion -gt $packageVersion -and -not $State.AllowDowngrade){if($Quiet){throw 'Quiet downgrade requires -AllowDowngrade.'};Add-Type -AssemblyName System.Windows.Forms;if([Windows.Forms.MessageBox]::Show((Get-Text Downgrade @($same.Version,$Metadata.version)),(Get-Text Title),'YesNo','Warning','Button2') -ne 'Yes'){return $false};$State.AllowDowngrade=$true}
    }
    if($other -and -not $State.KeepOtherScope -and -not $State.MigrateOtherScope){
        if($Quiet){throw 'Another installation scope exists; choose -KeepOtherScope or -MigrateOtherScope.'};Add-Type -AssemblyName System.Windows.Forms
        $oldName=Get-Text $(if($other.Scope -eq 'AllUsers'){'ScopeAll'}else{'ScopeCurrent'});$newName=Get-Text $(if($State.Scope -eq 'AllUsers'){'ScopeAll'}else{'ScopeCurrent'})
        $answer=[Windows.Forms.MessageBox]::Show((Get-Text OtherScope @($oldName,$newName)),(Get-Text Title),'YesNoCancel','Warning');if($answer -eq 'Cancel'){return $false};if($answer -eq 'Yes'){$State.MigrateOtherScope=$true}else{$State.KeepOtherScope=$true}
    }
    return $true
}

# Runs the localized uninstaller and optionally clears only current-user application settings.
function Invoke-Uninstall {
    $existing=Get-Installation $Scope;$directory=if($InstallDirectory){[IO.Path]::GetFullPath($InstallDirectory)}elseif($existing){$existing.Directory}else{Split-Path -Parent $PSCommandPath}
    if($Scope -eq 'AllUsers' -and -not(Test-IsAdministrator)){$arguments="-NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Uninstall -Scope AllUsers -InstallDirectory `"$directory`"";if($RemoveSettings){$arguments+=' -RemoveSettings'};$process=Start-Process powershell.exe -ArgumentList $arguments -Verb RunAs -Wait -PassThru;exit $process.ExitCode}
    if(-not $Quiet){
        Add-Type -AssemblyName System.Windows.Forms;Add-Type -AssemblyName System.Drawing;$form=New-Object Windows.Forms.Form;$form.Text=Get-Text UninstallTitle;$form.Size=New-Object Drawing.Size(520,210);$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog'
        $label=New-Object Windows.Forms.Label;$label.SetBounds(20,20,460,55);$label.Text=Get-Text UninstallPrompt;$check=New-Object Windows.Forms.CheckBox;$check.SetBounds(20,80,450,25);$check.Text=Get-Text RemoveSettings
        $yes=New-Object Windows.Forms.Button;$yes.SetBounds(300,120,85,28);$yes.Text=Get-Text Remove;$yes.DialogResult='OK';$no=New-Object Windows.Forms.Button;$no.SetBounds(395,120,85,28);$no.Text=Get-Text Cancel;$no.DialogResult='Cancel';$form.Controls.AddRange(@($label,$check,$yes,$no));$form.AcceptButton=$yes;$form.CancelButton=$no
        if($form.ShowDialog() -ne 'OK'){return};$RemoveSettings=$check.Checked
    }
    Remove-OwnedInstallation $Scope $directory;if($RemoveSettings){Remove-Item -LiteralPath 'HKCU:\Software\TechDraw' -Recurse -Force -ErrorAction SilentlyContinue}
    if(-not $Quiet){[Windows.Forms.MessageBox]::Show((Get-Text Removed),(Get-Text UninstallTitle),'OK','Information')|Out-Null}
}

# Choose the initial language from the explicit option, saved install or Windows UI locale.
$savedInstall=Get-Installation CurrentUser;$savedLanguage=if($savedInstall){$savedInstall.Language}else{$null}
$script:SelectedLanguage=if($Language){$Language}elseif($savedLanguage -in @('English','Russian')){$savedLanguage}elseif([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ru'){'Russian'}else{'English'}
if($Uninstall){Invoke-Uninstall;exit 0}

$payloadArchive=Join-Path $PSScriptRoot 'payload.zip';$metadataPath=Join-Path $PSScriptRoot 'payload.json'
if(-not(Test-Path -LiteralPath $payloadArchive) -or -not(Test-Path -LiteralPath $metadataPath)){throw 'Installer payload is incomplete.'}
$metadata=Get-Content -LiteralPath $metadataPath -Raw|ConvertFrom-Json
if($metadata.architecture -eq 'x64' -and -not[Environment]::Is64BitOperatingSystem){throw 'The x64 installer requires 64-bit Windows.'}
$currentInstall=Get-Installation CurrentUser;$allInstall=Get-Installation AllUsers
if($StateFile){$state=Get-Content -LiteralPath $StateFile -Raw|ConvertFrom-Json;$script:SelectedLanguage=$state.Language}
elseif($Quiet){$state=[pscustomobject]@{Scope=$Scope;InstallDirectory=if($InstallDirectory){$InstallDirectory}else{Get-DefaultInstallDirectory $Scope};Language=$script:SelectedLanguage;DesktopShortcut=[bool]$DesktopShortcut;TaskbarIntent=[bool]$TaskbarIntent;AssociateDrw=[bool]$AssociateDrw;Launch=$false;AllowDowngrade=[bool]$AllowDowngrade;Repair=[bool]$Repair;KeepOtherScope=[bool]$KeepOtherScope;MigrateOtherScope=[bool]$MigrateOtherScope;SkipShellIntegration=[bool]$SkipShellIntegration}}
else{$state=Show-InstallWizard $metadata $currentInstall $allInstall;if($null -eq $state){exit 0}}

if(($Quiet -or $StateFile) -and -not(Confirm-InstallMode $state $metadata $currentInstall $allInstall)){exit 0}
$otherInstall=if($state.Scope -eq 'AllUsers'){$currentInstall}else{$allInstall};$needsElevation=$state.Scope -eq 'AllUsers' -or ($state.MigrateOtherScope -and $otherInstall -and $otherInstall.Scope -eq 'AllUsers')
if($needsElevation -and -not(Test-IsAdministrator)){Invoke-ElevatedInstallation $state;exit 0}
Install-Payload $state $metadata $payloadArchive
if(-not $state.SkipShellIntegration){Set-ShellIntegration $state $metadata;Set-FileAssociation $state $metadata;if($state.TaskbarIntent){New-Item -Path 'HKCU:\Software\TechDraw\TechDraw\installer' -Force|Out-Null;New-ItemProperty -Path 'HKCU:\Software\TechDraw\TechDraw\installer' -Name pendingTaskbarPin -Value 1 -PropertyType DWord -Force|Out-Null}}
if($state.MigrateOtherScope -and $otherInstall){Remove-OwnedInstallation $otherInstall.Scope $otherInstall.Directory}
if($state.Launch){Start-Process -FilePath (Join-Path $state.InstallDirectory $metadata.executable) -WorkingDirectory $state.InstallDirectory}
if(-not $Quiet -and -not $ElevatedContinuation){Add-Type -AssemblyName System.Windows.Forms;[Windows.Forms.MessageBox]::Show((Get-Text Installed),(Get-Text Title),'OK','Information')|Out-Null}

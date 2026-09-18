/*
 * Implements the Technical Drawing installation policy on top of Qt Installer
 * Framework. All state is stored under stable TechDraw* installer values so the
 * same package can be driven by the GUI or by reproducible command-line tests.
 */

var productVersion = "@PRODUCT_VERSION@";
var productArchitecture = "@PRODUCT_ARCHITECTURE@";
var productDisplayName = "@PRODUCT_DISPLAY_NAME@";
var maintenanceToolName = "@MAINTENANCE_TOOL@";
var productRegistryId = "@PRODUCT_ID@";
var registryBase = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + productRegistryId;
var existingInstallations = {};
var optionsWidget = null;
var languageWidget = null;
var noticeWidget = null;
var existingDecisionChecked = false;
var existingIfwBackup = "";
var existingIfwRoot = "";

function selectedRussian()
{
    return installer.value("TechDrawLanguage", "English") === "Russian";
}

function detectDefaultLanguage()
{
    var uiLanguage = installer.value("UILanguage", "en").toLowerCase();
    return uiLanguage.indexOf("ru") === 0 ? "Russian" : "English";
}

function applyLocalizedBanner(page)
{
    if (!page)
        return;
    var watermarkPixmap = 0;
    var bannerPixmap = 2;
    var backgroundPixmap = 3;
    page.setPixmap(bannerPixmap, page.pixmap(selectedRussian() ? backgroundPixmap : watermarkPixmap));
    page.title = " ";
}

function setDynamicPageIdentity(objectName, pageListTitle)
{
    var page = gui.pageByObjectName(objectName);
    if (!page)
        return;
    applyLocalizedBanner(page);
    page.setPageListTitle(pageListTitle);
}

function registryRoot(scope)
{
    return scope === "AllUsers" ? "HKEY_LOCAL_MACHINE\\" : "HKEY_CURRENT_USER\\";
}

function registryValue(scope, name)
{
    return installer.value(registryRoot(scope) + registryBase + "\\" + name, "");
}

function readInstallation(scope)
{
    var directory = registryValue(scope, "InstallLocation");
    if (directory === "")
        return null;
    return {
        scope: scope,
        directory: directory,
        version: registryValue(scope, "DisplayVersion"),
        technology: registryValue(scope, "InstallerTechnology")
    };
}

function compareVersions(left, right)
{
    var a = left.split(".");
    var b = right.split(".");
    var count = Math.max(a.length, b.length);
    for (var i = 0; i < count; ++i) {
        var av = i < a.length ? parseInt(a[i]) : 0;
        var bv = i < b.length ? parseInt(b[i]) : 0;
        if (av < bv)
            return -1;
        if (av > bv)
            return 1;
    }
    return 0;
}

function currentUserTarget()
{
    return installer.environmentVariable("LOCALAPPDATA") + "\\Programs\\Technical Drawing";
}

function allUsersTarget()
{
    return installer.environmentVariable("ProgramFiles") + "\\Technical Drawing";
}

function defaultTarget(scope)
{
    return scope === "AllUsers" ? allUsersTarget() : currentUserTarget();
}

function setInstallerDefaults()
{
    var scope = installer.value("TechDrawScope", "CurrentUser");
    if (scope !== "AllUsers")
        scope = "CurrentUser";
    installer.setValue("TechDrawScope", scope);
    var savedLanguage = registryValue("CurrentUser", "InstallerLanguage");
    if (savedLanguage === "")
        savedLanguage = registryValue("AllUsers", "InstallerLanguage");
    installer.setValue("TechDrawLanguage", installer.value("TechDrawLanguage",
                       savedLanguage === "Russian" || savedLanguage === "English" ? savedLanguage : detectDefaultLanguage()));
    installer.setValue("TechDrawDesktopShortcut", installer.value("TechDrawDesktopShortcut", "false"));
    installer.setValue("TechDrawTaskbarIntent", installer.value("TechDrawTaskbarIntent", "false"));
    installer.setValue("TechDrawAssociateDrw", installer.value("TechDrawAssociateDrw", "true"));
    installer.setValue("TechDrawLaunch", installer.value("TechDrawLaunch", "true"));
    installer.setValue("TechDrawMigration", installer.value("TechDrawMigration", "move"));
    if (installer.value("TargetDir", "") === "")
        installer.setValue("TargetDir", defaultTarget(scope));
}

function Component()
{
    setInstallerDefaults();
    existingInstallations.CurrentUser = readInstallation("CurrentUser");
    existingInstallations.AllUsers = readInstallation("AllUsers");

    component.loaded.connect(this, Component.prototype.loaded);
    installer.installationFinished.connect(this, Component.prototype.installationFinished);
    installer.finishButtonClicked.connect(this, Component.prototype.finishButtonClicked);
}

Component.prototype.loaded = function()
{
    if (installer.isInstaller()) {
        installer.addWizardPage(component, "LanguagePage", QInstaller.TargetDirectory);
        var scope = installer.value("TechDrawScope");
        var clean = existingInstallations.CurrentUser === null && existingInstallations.AllUsers === null;
        if (clean)
            installer.addWizardPage(component, "PrototypeNoticePage", QInstaller.TargetDirectory);
        installer.addWizardPage(component, "OptionsPage", QInstaller.ReadyForInstallation);

        languageWidget = gui.pageWidgetByObjectName("DynamicLanguagePage");
        noticeWidget = gui.pageWidgetByObjectName("DynamicPrototypeNoticePage");
        optionsWidget = gui.pageWidgetByObjectName("DynamicOptionsPage");
        this.configureLanguagePage();
        this.configureNoticePage();
        this.configureOptionsPage(scope);

        var readyPage = gui.pageById(QInstaller.ReadyForInstallation);
        if (readyPage)
            readyPage.entered.connect(this, Component.prototype.readyPageEntered);
        var performPage = gui.pageById(QInstaller.PerformInstallation);
        if (performPage)
            performPage.entered.connect(this, Component.prototype.nativePageEntered);
        var finishedPage = gui.pageById(QInstaller.InstallationFinished);
        if (finishedPage)
            finishedPage.entered.connect(this, Component.prototype.nativePageEntered);
        gui.interrupted.connect(this, Component.prototype.restoreExistingIfw);
    } else if (installer.isUninstaller()) {
        installer.addWizardPageItem(component, "UninstallOptions", QInstaller.ReadyForInstallation);
        this.retranslateUninstallPage();
    }
};

Component.prototype.configureLanguagePage = function()
{
    if (!languageWidget)
        return;
    languageWidget.languageCombo.currentIndex = selectedRussian() ? 1 : 0;
    languageWidget.languageCombo.currentIndexChanged.connect(this, Component.prototype.languageChanged);
};

Component.prototype.languageChanged = function(index)
{
    installer.setValue("TechDrawLanguage", index === 1 ? "Russian" : "English");
    this.retranslatePages();
};

Component.prototype.retranslateNativePages = function()
{
    var ru = selectedRussian();
    var ready = gui.pageById(QInstaller.ReadyForInstallation);
    var perform = gui.pageById(QInstaller.PerformInstallation);
    var finished = gui.pageById(QInstaller.InstallationFinished);
    if (ready) {
        applyLocalizedBanner(ready);
        ready.setPageListTitle(ru ? "Сводка установки" : "Installation summary");
    }
    if (perform) {
        applyLocalizedBanner(perform);
        perform.setPageListTitle(ru ? "Установка" : "Installing");
    }
    if (finished) {
        applyLocalizedBanner(finished);
        finished.setPageListTitle(ru ? "Завершение установки" : "Finished");
    }
    gui.setWizardPageButtonText(QInstaller.ReadyForInstallation, buttons.CommitButton,
                                ru ? "Установить" : "Install");
    gui.setWizardPageButtonText(QInstaller.ReadyForInstallation, buttons.BackButton,
                                ru ? "Назад" : "Back");
    gui.setWizardPageButtonText(QInstaller.ReadyForInstallation, buttons.CancelButton,
                                ru ? "Отмена" : "Cancel");
    gui.setWizardPageButtonText(QInstaller.PerformInstallation, buttons.CancelButton,
                                ru ? "Отмена" : "Cancel");
    gui.setWizardPageButtonText(QInstaller.InstallationFinished, buttons.FinishButton,
                                ru ? "Готово" : "Finish");
};

Component.prototype.configureNoticePage = function()
{
    this.retranslatePages();
};

Component.prototype.configureOptionsPage = function(scope)
{
    if (!optionsWidget)
        return;

    optionsWidget.currentUserRadio.checked = scope !== "AllUsers";
    optionsWidget.allUsersRadio.checked = scope === "AllUsers";
    var selectedInstallation = existingInstallations[scope];
    optionsWidget.targetEdit.text = selectedInstallation !== null
        ? selectedInstallation.directory : installer.value("TargetDir", defaultTarget(scope));
    optionsWidget.desktopCheck.checked = installer.value("TechDrawDesktopShortcut") === "true";
    optionsWidget.taskbarCheck.checked = installer.value("TechDrawTaskbarIntent") === "true";
    optionsWidget.associateCheck.checked = installer.value("TechDrawAssociateDrw") !== "false";
    optionsWidget.launchCheck.checked = installer.value("TechDrawLaunch") !== "false";

    optionsWidget.currentUserRadio.toggled.connect(this, Component.prototype.scopeChanged);
    optionsWidget.allUsersRadio.toggled.connect(this, Component.prototype.scopeChanged);
    optionsWidget.targetEdit.textChanged.connect(this, Component.prototype.optionsChanged);
    optionsWidget.desktopCheck.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.taskbarCheck.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.associateCheck.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.launchCheck.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.migrateRadio.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.keepBothRadio.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.cancelMigrationRadio.toggled.connect(this, Component.prototype.optionsChanged);
    optionsWidget.browseButton.clicked.connect(this, Component.prototype.chooseTarget);

    this.updateMigrationControls();
    this.optionsChanged();
    this.retranslatePages();
};

Component.prototype.scopeChanged = function(checked)
{
    if (!checked || !optionsWidget)
        return;
    var previousScope = installer.value("TechDrawScope", "CurrentUser");
    var nextScope = optionsWidget.allUsersRadio.checked ? "AllUsers" : "CurrentUser";
    var previousDefault = defaultTarget(previousScope);
    installer.setValue("TechDrawScope", nextScope);
    var nextInstallation = existingInstallations[nextScope];
    if (nextInstallation !== null)
        optionsWidget.targetEdit.text = nextInstallation.directory;
    else if (optionsWidget.targetEdit.text === previousDefault || optionsWidget.targetEdit.text === "")
        optionsWidget.targetEdit.text = defaultTarget(nextScope);
    existingDecisionChecked = false;
    this.updateMigrationControls();
    this.optionsChanged();
};

Component.prototype.chooseTarget = function()
{
    if (!optionsWidget)
        return;
    var caption = selectedRussian() ? "Выберите каталог установки" : "Choose the installation folder";
    var selected = QFileDialog.getExistingDirectory(caption, optionsWidget.targetEdit.text, "techdraw.target");
    if (selected !== "")
        optionsWidget.targetEdit.text = installer.toNativeSeparators(selected);
};

Component.prototype.updateMigrationControls = function()
{
    if (!optionsWidget)
        return;
    var selectedScope = optionsWidget.allUsersRadio.checked ? "AllUsers" : "CurrentUser";
    var otherScope = selectedScope === "AllUsers" ? "CurrentUser" : "AllUsers";
    var other = existingInstallations[otherScope];
    optionsWidget.migrationGroup.visible = other !== null;
    if (other !== null) {
        var prefix = selectedRussian() ? "Найдена установка в другой области: " : "An installation exists in the other scope: ";
        optionsWidget.migrationLabel.text = prefix + other.directory;
        if (optionsWidget.keepBothRadio.checked) {
            optionsWidget.migrationLabel.text += selectedRussian()
                ? "\nПри сохранении обеих копий пользовательская ассоциация .drw имеет приоритет над общесистемной."
                : "\nWhen both copies are kept, the current-user .drw association takes precedence over the all-users association.";
        }
    }
};

Component.prototype.optionsChanged = function()
{
    if (!optionsWidget)
        return;
    if (existingIfwBackup !== "")
        this.restoreExistingIfw();
    var scope = optionsWidget.allUsersRadio.checked ? "AllUsers" : "CurrentUser";
    var target = optionsWidget.targetEdit.text;
    installer.setValue("TechDrawScope", scope);
    installer.setValue("TargetDir", target);
    installer.setValue("TechDrawDesktopShortcut", optionsWidget.desktopCheck.checked ? "true" : "false");
    installer.setValue("TechDrawTaskbarIntent", optionsWidget.taskbarCheck.checked ? "true" : "false");
    installer.setValue("TechDrawAssociateDrw", optionsWidget.associateCheck.checked ? "true" : "false");
    installer.setValue("TechDrawLaunch", optionsWidget.launchCheck.checked ? "true" : "false");
    installer.setValue("TechDrawMigration", optionsWidget.cancelMigrationRadio.checked ? "cancel" :
                       (optionsWidget.keepBothRadio.checked ? "keep" : "move"));
    this.updateMigrationControls();

    var normalized = target.replace(/\\/g, "/");
    var valid = /^[A-Za-z]:\/.+/.test(normalized) && !/^[A-Za-z]:\/$/.test(normalized);
    optionsWidget.complete = valid && installer.value("TechDrawMigration") !== "cancel";
};

Component.prototype.retranslatePages = function()
{
    var ru = selectedRussian();
    this.retranslateNativePages();
    if (noticeWidget) {
        noticeWidget.windowTitle = ru ? "Уведомление о прототипе" : "Prototype notice";
        noticeWidget.titleLabel.text = noticeWidget.windowTitle;
        noticeWidget.noticeText.plainText = ru
            ? "УВЕДОМЛЕНИЕ О ПРОТОТИПЕ\n\nЭта сборка не является даже Alpha-версией. Настоящая лицензия программы ещё не выбрана.\n\nЭтот текст является заглушкой, не содержит юридических условий и не предоставляет прав на использование, изменение или распространение программы. Он не заменяет файл LICENSE."
            : "PROTOTYPE NOTICE\n\nThis build is not even an Alpha version. The real software license has not been selected yet.\n\nThis placeholder is not legal terms and grants no rights to use, modify, or distribute the program. It does not replace a LICENSE file.";
        noticeWidget.informationLabel.text = ru ? "Ознакомление с этим сообщением не требует подтверждения." : "This information does not require acceptance.";
    }
    if (optionsWidget) {
        optionsWidget.windowTitle = ru ? "Параметры установки" : "Installation settings";
        optionsWidget.titleLabel.text = optionsWidget.windowTitle;
        optionsWidget.scopeGroup.title = ru ? "Область установки" : "Installation scope";
        optionsWidget.currentUserRadio.text = ru ? "Только для меня" : "Only for me";
        optionsWidget.allUsersRadio.text = ru ? "Для всех пользователей (требуются права администратора)" : "For all users (administrator rights required)";
        optionsWidget.targetLabel.text = ru ? "Каталог установки:" : "Installation folder:";
        optionsWidget.browseButton.text = ru ? "Обзор..." : "Browse...";
        optionsWidget.shortcutsGroup.title = ru ? "Ярлыки и ассоциация файлов" : "Shortcuts and file association";
        optionsWidget.startMenuCheck.text = ru ? "Меню «Пуск» (обязательно)" : "Start menu (required)";
        optionsWidget.desktopCheck.text = ru ? "Рабочий стол" : "Desktop";
        optionsWidget.taskbarCheck.text = ru ? "Попросить Windows закрепить приложение на панели задач при первом запуске" : "Ask Windows to pin to taskbar on first launch";
        optionsWidget.associateCheck.text = ru ? "Связать файлы .drw с «Техническим рисунком»" : "Associate .drw files with Technical Drawing";
        optionsWidget.migrationGroup.title = ru ? "Существующая установка" : "Existing installation";
        optionsWidget.migrateRadio.text = ru ? "Перенести её в выбранную область" : "Move it to the selected scope";
        optionsWidget.keepBothRadio.text = ru ? "Оставить обе установки" : "Keep both installations";
        optionsWidget.cancelMigrationRadio.text = ru ? "Отменить установку" : "Cancel installation";
        optionsWidget.launchCheck.text = ru ? "Запустить «Технический рисунок» после установки" : "Launch Technical Drawing after installation";
        optionsWidget.readyLabel.text = ru ? "После этой страницы остаются только подтверждение и установка." : "After this page, only confirmation and installation remain.";
        this.updateMigrationControls();
    }
    setDynamicPageIdentity("DynamicLanguagePage", ru ? "Язык" : "Language");
    setDynamicPageIdentity("DynamicPrototypeNoticePage", ru ? "Уведомление о прототипе" : "Prototype notice");
    setDynamicPageIdentity("DynamicOptionsPage", ru ? "Параметры установки" : "Installation settings");
};

Component.prototype.retranslateUninstallPage = function()
{
    var widget = component.userInterface("UninstallOptions");
    if (!widget)
        return;
    var installedLanguage = registryValue("CurrentUser", "InstallerLanguage");
    if (installedLanguage === "")
        installedLanguage = registryValue("AllUsers", "InstallerLanguage");
    var ru = (installedLanguage === "" ? detectDefaultLanguage() : installedLanguage) === "Russian";
    widget.documentsLabel.text = ru
        ? "Проекты, экспортированные изображения и другие документы пользователя будут сохранены."
        : "Projects, exported images, and other user documents will be preserved.";
    widget.removeSettingsCheck.text = ru ? "Удалить настройки текущего пользователя" : "Remove settings of the current user";
};

Component.prototype.readyPageEntered = function()
{
    this.retranslateNativePages();
    this.updateReadySummary();
    if (existingDecisionChecked)
        return;
    existingDecisionChecked = true;
    var scope = installer.value("TechDrawScope", "CurrentUser");
    var existing = existingInstallations[scope];
    if (existing === null)
        return;

    var ru = selectedRussian();
    var comparison = compareVersions(existing.version, productVersion);
    if (comparison === 0) {
        var sameText = ru
            ? "Версия " + existing.version + " уже установлена. Восстановить программные файлы?"
            : "Version " + existing.version + " is already installed. Repair the program files?";
        var sameAnswer = QMessageBox.question("techdraw.repair", productDisplayName, sameText,
                                              QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes);
        if (sameAnswer !== QMessageBox.Yes) {
            gui.clickButton(QInstaller.BackButton);
            existingDecisionChecked = false;
            return;
        }
    } else if (comparison > 0 && installer.value("TechDrawAllowDowngrade", "false") !== "true") {
        var downgradeText = ru
            ? "Установлена версия " + existing.version + ", а этот установщик содержит более старую версию " + productVersion + ". Проекты новой версии могут быть несовместимы. Продолжить понижение версии?"
            : "Version " + existing.version + " is installed, but this installer contains older version " + productVersion + ". Projects made by the newer version may be incompatible. Continue with downgrade?";
        var downgradeAnswer = QMessageBox.warning("techdraw.downgrade", productDisplayName, downgradeText,
                                                  QMessageBox.Yes | QMessageBox.No, QMessageBox.No);
        if (downgradeAnswer !== QMessageBox.Yes) {
            gui.clickButton(QInstaller.BackButton);
            existingDecisionChecked = false;
            return;
        } else
            installer.setValue("TechDrawAllowDowngrade", "true");
    }
    if ((comparison <= 0 || installer.value("TechDrawAllowDowngrade", "false") === "true") &&
            existing.technology === "QtIFW")
        this.prepareExistingIfw(existing.directory);
};

Component.prototype.nativePageEntered = function()
{
    this.retranslateNativePages();
};

Component.prototype.updateReadySummary = function()
{
    var page = gui.pageById(QInstaller.ReadyForInstallation);
    if (!page || !page.InstallComponentsTreeview || !page.InstallMsgLabel)
        return;

    var ru = selectedRussian();
    var scope = installer.value("TechDrawScope", "CurrentUser");
    var existing = existingInstallations[scope];
    var action;
    if (existing === null) {
        action = ru ? "Чистая установка" : "Clean installation";
    } else {
        var comparison = compareVersions(existing.version, productVersion);
        if (comparison === 0)
            action = ru ? "Восстановление версии " + productVersion : "Repair version " + productVersion;
        else if (comparison < 0)
            action = ru ? "Обновление до версии " + productVersion : "Upgrade to version " + productVersion;
        else
            action = ru ? "Понижение до версии " + productVersion : "Downgrade to version " + productVersion;
    }

    var yes = ru ? "да" : "yes";
    var no = ru ? "нет" : "no";
    var items = [
        action,
        (ru ? "Компонент: " : "Component: ") + productDisplayName + " (" + productArchitecture + ")",
        (ru ? "Каталог: " : "Folder: ") + installer.value("TargetDir"),
        (ru ? "Область: " : "Scope: ") + (scope === "AllUsers"
            ? (ru ? "для всех пользователей" : "all users")
            : (ru ? "только для текущего пользователя" : "current user only")),
        (ru ? "Ярлык в меню «Пуск»: " : "Start menu shortcut: ") + yes,
        (ru ? "Ярлык на рабочем столе: " : "Desktop shortcut: ")
            + (installer.value("TechDrawDesktopShortcut") === "true" ? yes : no),
        (ru ? "Ассоциация файлов .drw: " : "Associate .drw files: ")
            + (installer.value("TechDrawAssociateDrw") !== "false" ? yes : no),
        (ru ? "Запуск после установки: " : "Launch after installation: ")
            + (installer.value("TechDrawLaunch") !== "false" ? yes : no)
    ];
    page.InstallMsgLabel.text = ru ? "Будут применены следующие параметры:" : "The following settings will be applied:";
    gui.setTextItems(page.InstallComponentsTreeview, items);
};

Component.prototype.prepareExistingIfw = function(root)
{
    if (existingIfwBackup !== "")
        return;
    var powershell = installer.environmentVariable("SystemRoot") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    var tempRoot = installer.environmentVariable("TEMP");
    existingIfwBackup = tempRoot + "\\TechDraw-ifw-backup-" + Date.now();
    existingIfwRoot = root;
    var script = "$ErrorActionPreference='Stop';$root=[IO.Path]::GetFullPath($args[0]);$backup=[IO.Path]::GetFullPath($args[1]);" +
        "New-Item -ItemType Directory -Force -Path $backup|Out-Null;" +
        "$names=@('components.xml','InstallationLog.txt','installer.dat','network.xml','" + maintenanceToolName + ".dat','" + maintenanceToolName + ".exe','" + maintenanceToolName + ".ini','installerResources');" +
        "foreach($name in $names){$path=Join-Path $root $name;if(Test-Path -LiteralPath $path){Move-Item -LiteralPath $path -Destination $backup -Force}}";
    var result = installer.execute(powershell, ["-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script,
                                                  existingIfwRoot, existingIfwBackup]);
    if (result.length < 2 || result[1] !== 0) {
        existingIfwBackup = "";
        existingIfwRoot = "";
        throw "Cannot prepare the existing Qt Installer Framework installation for replacement.";
    }
};

Component.prototype.restoreExistingIfw = function()
{
    if (existingIfwBackup === "")
        return;
    var powershell = installer.environmentVariable("SystemRoot") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    var script = "$ErrorActionPreference='Stop';$root=[IO.Path]::GetFullPath($args[0]);$backup=[IO.Path]::GetFullPath($args[1]);" +
        "if(Test-Path -LiteralPath $backup){Get-ChildItem -LiteralPath $backup -Force|ForEach-Object{$destination=Join-Path $root $_.Name;Remove-Item -LiteralPath $destination -Recurse -Force -ErrorAction SilentlyContinue;Move-Item -LiteralPath $_.FullName -Destination $root -Force};Remove-Item -LiteralPath $backup -Force -ErrorAction SilentlyContinue}";
    installer.execute(powershell, ["-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script,
                                   existingIfwRoot, existingIfwBackup]);
    existingIfwBackup = "";
    existingIfwRoot = "";
};

Component.prototype.discardExistingIfwBackup = function()
{
    if (existingIfwBackup === "")
        return;
    var powershell = installer.environmentVariable("SystemRoot") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    installer.execute(powershell, ["-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                                   "Remove-Item -LiteralPath $args[0] -Recurse -Force -ErrorAction SilentlyContinue",
                                   existingIfwBackup]);
    existingIfwBackup = "";
    existingIfwRoot = "";
};

function addOperation(elevated, name, arguments)
{
    if (elevated)
        component.addElevatedOperation(name, arguments);
    else
        component.addOperation(name, arguments);
}

function registryCommandArguments(action, key, name, value)
{
    if (action === "delete")
        return ["delete", key, "/f"];
    return ["add", key, "/v", name, "/t", "REG_SZ", "/d", value, "/f"];
}

function addRegistryValue(elevated, key, name, value, removeKeyOnUndo)
{
    var reg = installer.environmentVariable("SystemRoot") + "\\System32\\reg.exe";
    var args = registryCommandArguments("add", key, name, value);
    args.push("UNDOEXECUTE");
    args = args.concat(removeKeyOnUndo ? registryCommandArguments("delete", key, "", "")
                                      : ["delete", key, "/v", name, "/f"]);
    addOperation(elevated, "Execute", args);
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    // Automated package checks use an isolated root and must not alter the real
    // user's shortcuts, registry, legacy installation or file associations.
    if (installer.value("TechDrawSkipShellIntegration", "false") === "true")
        return;

    var scope = installer.value("TechDrawScope", "CurrentUser");
    var allUsers = scope === "AllUsers";
    var executable = "@TargetDir@/TechDraw.exe";
    var startRoot = allUsers ? "@AllUsersStartMenuProgramsPath@" : "@UserStartMenuProgramsPath@";
    var startDirectory = startRoot + "/Technical Drawing";
    var desktopRoot = allUsers ? installer.environmentVariable("PUBLIC") + "\\Desktop"
                               : installer.environmentVariable("USERPROFILE") + "\\Desktop";

    var migrationScript = "@TargetDir@/_installer/migrate-legacy.ps1";
    var powershell = installer.environmentVariable("SystemRoot") + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    var migrationArguments = [powershell, "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                              migrationScript, "-TargetDirectory", "@TargetDir@", "-SelectedScope", scope,
                              "-MigrationMode", installer.value("TechDrawMigration", "move")];
    addOperation(allUsers || existingInstallations.AllUsers !== null, "Execute", migrationArguments);

    addOperation(allUsers, "CreateShortcut", [executable, startDirectory + "/Technical Drawing.lnk"]);
    addOperation(allUsers, "CreateShortcut", ["@TargetDir@/" + maintenanceToolName + ".exe",
                                                startDirectory + "/Uninstall Technical Drawing.lnk", "--start-uninstaller"]);
    if (installer.value("TechDrawDesktopShortcut") === "true")
        addOperation(allUsers, "CreateShortcut", [executable, desktopRoot + "\\Technical Drawing.lnk"]);

    if (installer.value("TechDrawAssociateDrw") === "true") {
        addOperation(allUsers, "RegisterFileType", ["drw", "\"" + executable + "\" \"%1\"",
                                                     "Technical Drawing project", "application/x-techdraw",
                                                     executable + ",0", "ProgId=TechnicalDrawing.Project"]);
    }

    if (installer.value("TechDrawTaskbarIntent") === "true") {
        var settingsKey = "HKCU\\Software\\TechDraw\\TechDraw\\installer";
        addRegistryValue(false, settingsKey, "pendingTaskbarPin", "true", false);
    }

    var uninstallKey = (allUsers ? "HKLM\\" : "HKCU\\") + registryBase;
    var uninstallCommand = "\"@TargetDir@/" + maintenanceToolName + ".exe\" --start-uninstaller";
    addRegistryValue(allUsers, uninstallKey, "DisplayName", productDisplayName, true);
    addRegistryValue(allUsers, uninstallKey, "DisplayVersion", productVersion, false);
    addRegistryValue(allUsers, uninstallKey, "Publisher", "Technical Drawing", false);
    addRegistryValue(allUsers, uninstallKey, "DisplayIcon", executable + ",0", false);
    addRegistryValue(allUsers, uninstallKey, "InstallLocation", "@TargetDir@", false);
    addRegistryValue(allUsers, uninstallKey, "UninstallString", uninstallCommand, false);
    addRegistryValue(allUsers, uninstallKey, "InstallerLanguage", installer.value("TechDrawLanguage"), false);
    addRegistryValue(allUsers, uninstallKey, "InstallArchitecture", productArchitecture, false);
    addRegistryValue(allUsers, uninstallKey, "InstallerTechnology", "QtIFW", false);
};

Component.prototype.installationFinished = function()
{
    if (installer.isInstaller() && installer.status === QInstaller.Success) {
        this.discardExistingIfwBackup();
        installer.setValue("FinishedText", selectedRussian() ? "«Технический рисунок» успешно установлен." : "Technical Drawing was installed successfully.");
    } else if (installer.isInstaller()) {
        this.restoreExistingIfw();
    }
};

Component.prototype.finishButtonClicked = function()
{
    if (installer.status !== QInstaller.Success)
        return;
    if (installer.isInstaller() && installer.value("TechDrawLaunch", "true") === "true") {
        installer.executeDetached(installer.value("TargetDir") + "/TechDraw.exe", [], installer.value("TargetDir"));
    } else if (installer.isUninstaller()) {
        var widget = component.userInterface("UninstallOptions");
        if (widget && widget.removeSettingsCheck.checked) {
            var reg = installer.environmentVariable("SystemRoot") + "\\System32\\reg.exe";
            installer.execute(reg, ["delete", "HKCU\\Software\\TechDraw\\TechDraw", "/f"]);
        }
    }
};

/*
 * Controls the common Qt Installer Framework pages. Product-specific choices and
 * operations live in the component script so the package remains self-contained.
 */
function Controller()
{
    installer.guiElementsReady.connect(this, Controller.prototype.lockWindowSize);
    installer.setDefaultPageVisible(QInstaller.Introduction, false);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
    installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
}

Controller.prototype.lockWindowSize = function()
{
    gui.setFixedSize(gui.width, gui.height);
};

Controller.prototype.clearBannerCaption = function()
{
    var page = gui.currentPageWidget();
    if (page)
        page.title = " ";
};

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    this.clearBannerCaption();
};

Controller.prototype.PerformInstallationPageCallback = function()
{
    this.clearBannerCaption();
};

Controller.prototype.FinishedPageCallback = function()
{
    this.clearBannerCaption();
};

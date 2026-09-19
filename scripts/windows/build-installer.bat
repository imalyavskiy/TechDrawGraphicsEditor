@echo off
rem Purpose: build the portable Release package and wrap it in a Qt Installer Framework EXE.
rem Requirements: common.bat environment, PowerShell 5 and Qt Installer Framework 4.11+.
rem Parameter 1: x64 (default) or x86.
rem Result: dist\installer\TechnicalDrawing-Setup-x64.exe or TechnicalDrawing-Setup-x86.exe.
rem Exit codes: 0 on success, 1 when prerequisites, Release deployment or packaging fail.
setlocal
set "ARCHITECTURE=%~1"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"

rem Validate paths and always package a freshly rebuilt portable directory.
call "%~dp0common.bat" "%ARCHITECTURE%" || exit /b 1
call "%~dp0build-release.bat" "%ARCHITECTURE%" || exit /b 1

rem Resolve Qt Installer Framework from an explicit machine-local setting first,
rem then from the standard Qt Tools directory beside the configured Qt kit.
if not defined QT_IFW_ROOT (
    for %%I in ("%QT_ROOT%\..\..\Tools\QtInstallerFramework\4.11") do set "QT_IFW_ROOT=%%~fI"
)
set "BINARYCREATOR=%QT_IFW_ROOT%\bin\binarycreator.exe"
set "INSTALLERBASE=%QT_IFW_ROOT%\bin\installerbase.exe"
if not exist "%BINARYCREATOR%" (
    echo ERROR: Qt Installer Framework 4.11 or newer was not found.
    echo Install it with the Qt Maintenance Tool or set QT_IFW_ROOT in scripts\windows\environment.bat.
    exit /b 1
)
if not exist "%INSTALLERBASE%" (
    echo ERROR: installerbase.exe was not found under QT_IFW_ROOT: "%QT_IFW_ROOT%"
    exit /b 1
)

rem Generate the package tree from repository templates and the freshly deployed portable build.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare-ifw-package.ps1" ^
    -ProjectRoot "%PROJECT_ROOT%" -Architecture "%ARCHITECTURE%" -IfwRoot "%QT_IFW_ROOT%"
if errorlevel 1 (
    echo ERROR: Qt Installer Framework package preparation failed.
    exit /b 1
)

set "INSTALLER_PATH=%PROJECT_ROOT%\dist\installer\TechnicalDrawing-Setup-%ARCHITECTURE%.exe"
set "IFW_BUILD=%PROJECT_ROOT%\build\installer-%ARCHITECTURE%\ifw"
if not exist "%PROJECT_ROOT%\dist\installer" mkdir "%PROJECT_ROOT%\dist\installer"
if exist "%INSTALLER_PATH%" del /q "%INSTALLER_PATH%"

rem An offline-only package is one autonomous executable and never contacts a repository.
"%BINARYCREATOR%" --offline-only --template "%INSTALLERBASE%" ^
    --config "%IFW_BUILD%\config\config.xml" --packages "%IFW_BUILD%\packages" ^
    "%INSTALLER_PATH%"
if errorlevel 1 (
    echo ERROR: binarycreator failed.
    exit /b 1
)
if not exist "%INSTALLER_PATH%" (
    echo ERROR: binarycreator did not create "%INSTALLER_PATH%".
    exit /b 1
)

rem Signing the finished container is intentionally separate from signing the packaged binaries.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign-installer.ps1" ^
    -InstallerPath "%INSTALLER_PATH%" -Architecture "%ARCHITECTURE%"
if errorlevel 1 (
    echo ERROR: Installer signing stage failed.
    exit /b 1
)

echo Ready: "%INSTALLER_PATH%"
exit /b 0

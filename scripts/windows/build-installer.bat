@echo off
rem Purpose: build the portable Release package and wrap it in a Windows EXE installer.
rem Requirements: common.bat environment, PowerShell 5 and Windows IExpress.
rem Parameter 1: x64 (default) or x86.
rem Result: dist\installer\TechnicalDrawing-Setup-x64.exe or TechnicalDrawing-Setup-x86.exe.
rem Exit codes: 0 on success, 1 when prerequisites, Release deployment or packaging fail.
setlocal
set "ARCHITECTURE=%~1"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"

rem Validate paths and always package a freshly rebuilt portable directory.
call "%~dp0common.bat" "%ARCHITECTURE%" || exit /b 1
call "%~dp0build-release.bat" "%ARCHITECTURE%" || exit /b 1

if not exist "%SystemRoot%\System32\iexpress.exe" (
    echo ERROR: Windows IExpress was not found.
    exit /b 1
)

rem Delegate archive and IExpress generation to PowerShell for safe literal-path operations.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0create-installer.ps1" ^
    -ProjectRoot "%PROJECT_ROOT%" -Architecture "%ARCHITECTURE%"
if errorlevel 1 (
    echo ERROR: Installer creation failed.
    exit /b 1
)

set "INSTALLER_PATH=%PROJECT_ROOT%\dist\installer\TechnicalDrawing-Setup-%ARCHITECTURE%.exe"
rem Signing the finished container is intentionally separate from signing the packaged binaries.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign-installer.ps1" ^
    -InstallerPath "%INSTALLER_PATH%" -Architecture "%ARCHITECTURE%"
if errorlevel 1 (
    echo ERROR: Installer signing stage failed.
    exit /b 1
)

echo Ready: "%INSTALLER_PATH%"
exit /b 0

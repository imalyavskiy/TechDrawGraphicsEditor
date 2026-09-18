@echo off
rem Purpose: build the portable Release package and wrap it in a per-user EXE installer.
rem Requirements: common.bat environment, PowerShell 5 and Windows IExpress. Parameters: none.
rem Result: dist\installer\TechnicalDrawing-Setup.exe.
rem Exit codes: 0 on success, 1 when prerequisites, Release deployment or packaging fail.
setlocal

rem Validate paths and always package a freshly rebuilt portable directory.
call "%~dp0common.bat" || exit /b 1
call "%~dp0build-release.bat" || exit /b 1

if not exist "%SystemRoot%\System32\iexpress.exe" (
    echo ERROR: Windows IExpress was not found.
    exit /b 1
)

rem Delegate archive and IExpress generation to PowerShell for safe literal-path operations.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0create-installer.ps1" -ProjectRoot "%PROJECT_ROOT%"
if errorlevel 1 (
    echo ERROR: Installer creation failed.
    exit /b 1
)

echo Ready: "%PROJECT_ROOT%\dist\installer\TechnicalDrawing-Setup.exe"
exit /b 0

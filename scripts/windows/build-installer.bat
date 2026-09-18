@echo off
rem Build the portable Release package and wrap it in a per-user EXE installer.
rem Requirements: CMake, Ninja, Qt/MinGW paths from environment.bat, PowerShell 5 and Windows IExpress.
rem Result: dist\installer\TechnicalDrawing-Setup.exe. Any failure returns a nonzero exit code.
setlocal

call "%~dp0common.bat" || exit /b 1
call "%~dp0build-release.bat" || exit /b 1

if not exist "%SystemRoot%\System32\iexpress.exe" (
    echo ERROR: Windows IExpress was not found.
    exit /b 1
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0create-installer.ps1" -ProjectRoot "%PROJECT_ROOT%"
if errorlevel 1 (
    echo ERROR: Installer creation failed.
    exit /b 1
)

echo Ready: "%PROJECT_ROOT%\dist\installer\TechnicalDrawing-Setup.exe"
exit /b 0

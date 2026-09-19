@echo off
rem Purpose: build and verify one installer architecture in an isolated directory.
rem Requirements: the environment accepted by build-installer.bat, PowerShell 5 and QtIFW 4.11+.
rem Parameter 1: x64 (default) or x86. Result: lifecycle checks with no shell registrations.
rem Exit codes: 0 when packaging, install, repair and uninstall checks pass; 1 otherwise.
setlocal
set "ARCHITECTURE=%~1"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"

call "%~dp0build-installer.bat" "%ARCHITECTURE%" || exit /b 1
call "%~dp0common.bat" "%ARCHITECTURE%" || exit /b 1
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-legacy-migration.ps1" ^
    -ProjectRoot "%PROJECT_ROOT%"
if errorlevel 1 (
    echo ERROR: Legacy installer migration verification failed.
    exit /b 1
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-installer.ps1" ^
    -ProjectRoot "%PROJECT_ROOT%" -Architecture "%ARCHITECTURE%"
if errorlevel 1 (
    echo ERROR: Installer lifecycle verification failed.
    exit /b 1
)
exit /b 0

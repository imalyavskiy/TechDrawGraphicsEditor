@echo off
rem Purpose: build the additional 32-bit installer without changing x64 outputs.
rem Requirements: configured QT_ROOT_X86 and MINGW_ROOT_X86, PowerShell 5 and Qt Installer Framework 4.11+.
rem Parameters: none. Result: dist\installer\TechnicalDrawing-Setup-x86.exe.
rem Exit codes: forwards build-installer.bat.
call "%~dp0build-installer.bat" x86
exit /b %errorlevel%

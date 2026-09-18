@echo off
rem Purpose: run the installer lifecycle verification for the additional 32-bit package.
rem Requirements: configured x86 Qt/MinGW environment and the prerequisites of test-installer.bat.
rem Parameters: none. Exit codes: forwards test-installer.bat.
call "%~dp0test-installer.bat" x86
exit /b %errorlevel%

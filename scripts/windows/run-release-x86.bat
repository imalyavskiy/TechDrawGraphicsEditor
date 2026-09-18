@echo off
rem Purpose: launch the optional 32-bit portable Release application.
rem Requirements: a successful build-release-x86.bat run and configured x86 toolchain.
rem Parameters: none. Result: starts dist\TechDraw-x86\TechDraw.exe.
rem Exit code: forwarded from run-release.bat.
call "%~dp0run-release.bat" x86
exit /b %ERRORLEVEL%

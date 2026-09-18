@echo off
rem Purpose: launch the optional 32-bit Debug application with its matching runtime.
rem Requirements: a successful build-debug-x86.bat run and configured x86 toolchain.
rem Parameters: none. Result: starts build\cmake-debug-x86\TechDraw.exe.
rem Exit code: forwarded from run-debug.bat.
call "%~dp0run-debug.bat" x86
exit /b %ERRORLEVEL%

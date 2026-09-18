@echo off
rem Purpose: build the optional 32-bit Debug application in an isolated CMake directory.
rem Requirements: QT_ROOT_X86 and MINGW_ROOT_X86 configured in environment.bat.
rem Parameters: none. Result: build\cmake-debug-x86\TechDraw.exe.
rem Exit code: forwarded from configure-build.bat.
call "%~dp0configure-build.bat" Debug x86
exit /b %ERRORLEVEL%

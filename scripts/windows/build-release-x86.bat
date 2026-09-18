@echo off
rem Purpose: build and deploy the optional 32-bit Release package.
rem Requirements: QT_ROOT_X86 and MINGW_ROOT_X86 configured in environment.bat.
rem Parameters: none. Result: dist\TechDraw-x86 with the matching 32-bit runtime.
rem Exit code: forwarded from build-release.bat.
call "%~dp0build-release.bat" x86
exit /b %ERRORLEVEL%

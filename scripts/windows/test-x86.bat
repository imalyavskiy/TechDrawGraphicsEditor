@echo off
rem Purpose: run the complete self-test suite against the optional 32-bit Release package.
rem Requirements: build-release-x86.bat completed and the x86 toolchain is configured.
rem Parameters: none. Results: build\offscreen-results-x86 and build\windows-results-x86.
rem Exit code: forwarded from test.bat.
call "%~dp0test.bat" x86
exit /b %ERRORLEVEL%

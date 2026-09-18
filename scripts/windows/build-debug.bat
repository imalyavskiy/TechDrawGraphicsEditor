@echo off
rem Purpose: build the Debug application through the shared CMake/Ninja entry point.
rem Requirements: the environment accepted by common.bat. Parameters: none.
rem Result: build\cmake-debug\TechDraw.exe. Exit code: forwarded from configure-build.bat.
call "%~dp0configure-build.bat" Debug
exit /b %ERRORLEVEL%

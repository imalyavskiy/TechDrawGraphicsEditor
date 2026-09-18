@echo off
rem Build the Debug executable with CMake and Ninja.
call "%~dp0configure-build.bat" Debug
exit /b %ERRORLEVEL%

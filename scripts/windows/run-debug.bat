@echo off
rem Purpose: launch the existing Debug executable with matching Qt/MinGW runtime paths.
rem Requirements: a successful build-debug.bat run and the environment accepted by common.bat.
rem Parameters: none. Result: starts build\cmake-debug\TechDraw.exe asynchronously.
rem Exit codes: 0 when launch is accepted, 1 when setup, binary lookup or launch fails.
setlocal

rem Resolve and validate the repository and runtime before testing the binary path.
call "%~dp0common.bat" || exit /b 1
set "APP_DIR=%PROJECT_ROOT%\build\cmake-debug"
set "APP_EXE=%APP_DIR%\TechDraw.exe"

if not exist "%APP_EXE%" (
    echo Technical Draw Debug executable was not found:
    echo   "%APP_EXE%"
    echo Build the Debug configuration first.
    pause
    exit /b 1
)

rem Set the child working directory so relative Qt resources resolve exactly as in development.
pushd "%APP_DIR%" || exit /b 1
start "" /D "%APP_DIR%" "%APP_EXE%"
set "START_RESULT=%ERRORLEVEL%"
popd

exit /b %START_RESULT%

@echo off
rem Purpose: launch the portable Release package produced by build-release.bat.
rem Requirements: an existing matching portable package and the environment accepted by common.bat.
rem Parameter 1: x64 (default) or x86. Result: starts the matching TechDraw.exe asynchronously.
rem Exit codes: 0 when launch is accepted, 1 when setup, binary lookup or launch fails.
setlocal

set "ARCHITECTURE=%~1"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"

rem Resolve the repository consistently even though the portable EXE carries its own runtime.
call "%~dp0common.bat" "%ARCHITECTURE%" || exit /b 1
set "DEPLOY_SUFFIX="
if /i "%ARCHITECTURE%"=="x86" set "DEPLOY_SUFFIX=-x86"
set "APP_DIR=%PROJECT_ROOT%\dist\TechDraw%DEPLOY_SUFFIX%"
set "APP_EXE=%APP_DIR%\TechDraw.exe"

if not exist "%APP_EXE%" (
    echo Technical Draw Release executable was not found:
    echo   "%APP_EXE%"
    echo Build the Release configuration first.
    pause
    exit /b 1
)

rem Use the package directory as the working directory for its DLLs, plugins and translation catalog.
pushd "%APP_DIR%" || exit /b 1
start "" /D "%APP_DIR%" "%APP_EXE%"
set "START_RESULT=%ERRORLEVEL%"
popd

exit /b %START_RESULT%

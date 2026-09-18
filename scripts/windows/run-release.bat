@echo off
rem Launch the portable Release package produced by build-release.bat.
setlocal

call "%~dp0common.bat" || exit /b 1
set "APP_DIR=%PROJECT_ROOT%\dist\TechDraw"
set "APP_EXE=%APP_DIR%\TechDraw.exe"

if not exist "%APP_EXE%" (
    echo Technical Draw Release executable was not found:
    echo   "%APP_EXE%"
    echo Build the Release configuration first.
    pause
    exit /b 1
)

pushd "%APP_DIR%" || exit /b 1
start "" /D "%APP_DIR%" "%APP_EXE%"
set "START_RESULT=%ERRORLEVEL%"
popd

exit /b %START_RESULT%

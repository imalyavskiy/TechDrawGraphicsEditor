@echo off
rem Launch the CMake Debug executable with the matching Qt and MinGW runtime paths.
setlocal

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

pushd "%APP_DIR%" || exit /b 1
start "" /D "%APP_DIR%" "%APP_EXE%"
set "START_RESULT=%ERRORLEVEL%"
popd

exit /b %START_RESULT%

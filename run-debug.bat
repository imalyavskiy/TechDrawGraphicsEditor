@echo off
setlocal

set "APP_DIR=%~dp0build\debug"
set "APP_EXE=%APP_DIR%\TechDraw.exe"
set "QT_ROOT=F:\Qt\5.15.2\mingw81_64"
set "MINGW_ROOT=F:\Qt\Tools\mingw810_64"

if not exist "%APP_EXE%" (
    echo Technical Draw Debug executable was not found:
    echo   "%APP_EXE%"
    echo Build the Debug configuration first.
    pause
    exit /b 1
)

set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_ROOT%\plugins"

pushd "%APP_DIR%" || exit /b 1
start "" /D "%APP_DIR%" "%APP_EXE%"
set "START_RESULT=%ERRORLEVEL%"
popd

exit /b %START_RESULT%

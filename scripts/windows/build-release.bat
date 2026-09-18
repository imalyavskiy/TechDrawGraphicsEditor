@echo off
rem Build Release and create a portable directory with the required runtime files.
setlocal

call "%~dp0configure-build.bat" Release || exit /b 1
call "%~dp0common.bat" || exit /b 1

set "BUILD_EXE=%PROJECT_ROOT%\build\cmake-release\TechDraw.exe"
set "DEPLOY_DIR=%PROJECT_ROOT%\dist\TechDraw"
set "DEPLOY_EXE=%DEPLOY_DIR%\TechDraw.exe"

if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%" || (
    echo ERROR: Cannot create deployment directory: "%DEPLOY_DIR%"
    exit /b 1
)
copy /y "%BUILD_EXE%" "%DEPLOY_EXE%" >nul || (
    echo ERROR: Cannot copy the Release executable to the deployment directory.
    exit /b 1
)

for %%F in (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll) do (
    copy /y "%QT_ROOT%\bin\%%F" "%DEPLOY_DIR%\%%F" >nul || exit /b 1
)
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    copy /y "%MINGW_ROOT%\bin\%%F" "%DEPLOY_DIR%\%%F" >nul || exit /b 1
)

for %%D in (platforms styles) do mkdir "%DEPLOY_DIR%\%%D" || exit /b 1
copy /y "%QT_ROOT%\plugins\platforms\qwindows.dll" "%DEPLOY_DIR%\platforms\qwindows.dll" >nul || exit /b 1
copy /y "%QT_ROOT%\plugins\platforms\qoffscreen.dll" "%DEPLOY_DIR%\platforms\qoffscreen.dll" >nul || exit /b 1
copy /y "%QT_ROOT%\plugins\styles\qwindowsvistastyle.dll" "%DEPLOY_DIR%\styles\qwindowsvistastyle.dll" >nul || exit /b 1

>"%DEPLOY_DIR%\qt.conf" echo [Paths]
>>"%DEPLOY_DIR%\qt.conf" echo Prefix=.
>>"%DEPLOY_DIR%\qt.conf" echo Plugins=.

echo Ready: "%DEPLOY_EXE%"
exit /b 0

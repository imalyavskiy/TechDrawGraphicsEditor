@echo off
rem Purpose: build Release and recreate a portable application directory.
rem Requirements: the environment accepted by common.bat. Parameters: none.
rem Result: dist\TechDraw with EXE, QM catalog, Qt/MinGW DLLs, plugins and qt.conf.
rem Exit codes: 0 on success, 1 when build, validation, cleanup or deployment fails.
setlocal

rem Build first so a failed compilation cannot erase the previous portable package.
call "%~dp0configure-build.bat" Release || exit /b 1
call "%~dp0common.bat" || exit /b 1

set "BUILD_EXE=%PROJECT_ROOT%\build\cmake-release\TechDraw.exe"
set "DEPLOY_DIR=%PROJECT_ROOT%\dist\TechDraw"
set "DEPLOY_EXE=%DEPLOY_DIR%\TechDraw.exe"

rem PROJECT_ROOT was validated by common.bat; only its fixed dist\TechDraw child is recreated.
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%" || (
    echo ERROR: Cannot create deployment directory: "%DEPLOY_DIR%"
    exit /b 1
)
copy /y "%BUILD_EXE%" "%DEPLOY_EXE%" >nul || (
    echo ERROR: Cannot copy the Release executable to the deployment directory.
    exit /b 1
)

rem Keep the external catalog beside the EXE so QTranslator can replace text without rebuilding it.
if not exist "%PROJECT_ROOT%\build\cmake-release\translations\techdraw_ru.qm" (
    echo ERROR: Compiled translation catalog is missing.
    exit /b 1
)
mkdir "%DEPLOY_DIR%\translations" || exit /b 1
copy /y "%PROJECT_ROOT%\build\cmake-release\translations\techdraw_ru.qm" "%DEPLOY_DIR%\translations\techdraw_ru.qm" >nul || exit /b 1

rem Deploy the exact Qt and MinGW runtime that matches the configured compiler.
for %%F in (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll) do (
    copy /y "%QT_ROOT%\bin\%%F" "%DEPLOY_DIR%\%%F" >nul || exit /b 1
)
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    copy /y "%MINGW_ROOT%\bin\%%F" "%DEPLOY_DIR%\%%F" >nul || exit /b 1
)

rem Deploy the native and offscreen platforms used by the application and its self-tests.
for %%D in (platforms styles) do mkdir "%DEPLOY_DIR%\%%D" || exit /b 1
copy /y "%QT_ROOT%\plugins\platforms\qwindows.dll" "%DEPLOY_DIR%\platforms\qwindows.dll" >nul || exit /b 1
copy /y "%QT_ROOT%\plugins\platforms\qoffscreen.dll" "%DEPLOY_DIR%\platforms\qoffscreen.dll" >nul || exit /b 1
copy /y "%QT_ROOT%\plugins\styles\qwindowsvistastyle.dll" "%DEPLOY_DIR%\styles\qwindowsvistastyle.dll" >nul || exit /b 1

rem Force Qt to resolve plugins from the portable directory instead of the development machine.
>"%DEPLOY_DIR%\qt.conf" echo [Paths]
>>"%DEPLOY_DIR%\qt.conf" echo Prefix=.
>>"%DEPLOY_DIR%\qt.conf" echo Plugins=.

echo Ready: "%DEPLOY_EXE%"
exit /b 0

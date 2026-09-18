@echo off
rem Purpose: build Release and recreate a portable application directory.
rem Requirements: the environment accepted by common.bat.
rem Parameter 1: x64 (default) or x86. Result: dist\TechDraw for x64 or dist\TechDraw-x86.
rem Exit codes: 0 on success, 1 when build, validation, cleanup or deployment fails.
setlocal

set "ARCHITECTURE=%~1"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"

rem Build first so a failed compilation cannot erase the previous portable package.
call "%~dp0configure-build.bat" Release "%ARCHITECTURE%" || exit /b 1
call "%~dp0common.bat" "%ARCHITECTURE%" || exit /b 1

set "BUILD_SUFFIX="
set "DEPLOY_SUFFIX="
if /i "%ARCHITECTURE%"=="x86" (
    set "BUILD_SUFFIX=-x86"
    set "DEPLOY_SUFFIX=-x86"
)
set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-release%BUILD_SUFFIX%"
set "BUILD_EXE=%BUILD_DIR%\TechDraw.exe"
set "DEPLOY_DIR=%PROJECT_ROOT%\dist\TechDraw%DEPLOY_SUFFIX%"
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
if not exist "%BUILD_DIR%\translations\techdraw_ru.qm" (
    echo ERROR: Compiled translation catalog is missing.
    exit /b 1
)
mkdir "%DEPLOY_DIR%\translations" || exit /b 1
copy /y "%BUILD_DIR%\translations\techdraw_ru.qm" "%DEPLOY_DIR%\translations\techdraw_ru.qm" >nul || exit /b 1

rem Deploy the exact Qt and MinGW runtime that matches the configured compiler.
for %%F in (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll) do (
    copy /y "%QT_ROOT%\bin\%%F" "%DEPLOY_DIR%\%%F" >nul || exit /b 1
)
rem MinGW uses a different libgcc exception runtime on common x64 and x86 toolchains. Copy the
rem one supplied by the selected compiler together with the architecture-independent runtimes.
set "LIBGCC_FOUND="
for %%F in ("%MINGW_ROOT%\bin\libgcc_s_*-1.dll") do if exist "%%~fF" (
    copy /y "%%~fF" "%DEPLOY_DIR%\%%~nxF" >nul || exit /b 1
    set "LIBGCC_FOUND=1"
)
if not defined LIBGCC_FOUND (
    echo ERROR: Matching MinGW libgcc runtime was not found in "%MINGW_ROOT%\bin".
    exit /b 1
)
for %%F in (libstdc++-6.dll libwinpthread-1.dll) do (
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

rem Keep application signing between deployment and installer packaging. The current script is an
rem explicit successful placeholder until release credentials are provided outside the repository.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign-binaries.ps1" ^
    -PackageDirectory "%DEPLOY_DIR%" -Architecture "%ARCHITECTURE%"
if errorlevel 1 (
    echo ERROR: Application binary signing stage failed.
    exit /b 1
)

rem Refuse a mixed package before it can be tested or wrapped in an installer.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-package.ps1" ^
    -PackageDirectory "%DEPLOY_DIR%" -Architecture "%ARCHITECTURE%"
if errorlevel 1 (
    echo ERROR: Portable package architecture verification failed.
    exit /b 1
)

echo Ready: "%DEPLOY_EXE%"
exit /b 0

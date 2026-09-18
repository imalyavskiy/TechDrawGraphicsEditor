@echo off
rem Purpose: configure and build one CMake/Ninja configuration.
rem Requirements: the environment accepted by common.bat.
rem Parameter 1: Debug or Release. Result: build\cmake-debug or build\cmake-release\TechDraw.exe.
rem Exit codes: 0 on success, 1 on environment/configure/build failure, 2 on an invalid parameter.
setlocal

rem Validate the public parameter before creating or modifying a build directory.
set "CONFIGURATION=%~1"
if /i not "%CONFIGURATION%"=="Debug" if /i not "%CONFIGURATION%"=="Release" (
    echo ERROR: Expected configuration Debug or Release.
    exit /b 2
)

call "%~dp0common.bat" || exit /b 1

rem Keep independent CMake caches because Ninja is a single-configuration generator.
if /i "%CONFIGURATION%"=="Debug" (
    set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-debug"
) else (
    set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-release"
)

rem Configure with the Qt-supplied MinGW compiler selected explicitly.
"%CMAKE_EXE%" -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    "-DCMAKE_BUILD_TYPE=%CONFIGURATION%" ^
    "-DCMAKE_PREFIX_PATH=%QT_ROOT%" ^
    "-DCMAKE_CXX_COMPILER=%MINGW_ROOT%\bin\g++.exe"
if errorlevel 1 (
    echo ERROR: CMake configuration failed for %CONFIGURATION%.
    exit /b 1
)

rem Build every default target, including the external translation catalog.
"%CMAKE_EXE%" --build "%BUILD_DIR%" --parallel
if errorlevel 1 (
    echo ERROR: CMake build failed for %CONFIGURATION%.
    exit /b 1
)

echo Ready: "%BUILD_DIR%\TechDraw.exe"
exit /b 0

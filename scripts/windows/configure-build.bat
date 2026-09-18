@echo off
rem Purpose: configure and build one CMake/Ninja configuration.
rem Requirements: the environment accepted by common.bat.
rem Parameter 1: Debug or Release. Parameter 2: x64 (default) or x86.
rem Result: an architecture-specific build directory containing TechDraw.exe.
rem Exit codes: 0 on success, 1 on environment/configure/build failure, 2 on an invalid parameter.
setlocal

rem Validate the public parameter before creating or modifying a build directory.
set "CONFIGURATION=%~1"
if /i not "%CONFIGURATION%"=="Debug" if /i not "%CONFIGURATION%"=="Release" (
    echo ERROR: Expected configuration Debug or Release.
    exit /b 2
)

set "ARCHITECTURE=%~2"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"
call "%~dp0common.bat" "%ARCHITECTURE%" || exit /b 1

rem Keep independent CMake caches because Ninja is a single-configuration generator. Preserve
rem the historical unsuffixed x64 paths and suffix only the additional x86 configuration.
set "BUILD_SUFFIX="
if /i "%ARCHITECTURE%"=="x86" set "BUILD_SUFFIX=-x86"
if /i "%CONFIGURATION%"=="Debug" set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-debug%BUILD_SUFFIX%"
if /i "%CONFIGURATION%"=="Release" set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-release%BUILD_SUFFIX%"

rem Configure with the Qt-supplied MinGW compiler selected explicitly.
"%CMAKE_EXE%" -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    "-DCMAKE_BUILD_TYPE=%CONFIGURATION%" ^
    "-DTECHDRAW_ARCHITECTURE=%ARCHITECTURE%" ^
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

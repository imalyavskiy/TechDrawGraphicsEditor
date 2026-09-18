@echo off
rem Configure and build one CMake configuration. Call as: configure-build.bat Debug|Release.
setlocal

set "CONFIGURATION=%~1"
if /i not "%CONFIGURATION%"=="Debug" if /i not "%CONFIGURATION%"=="Release" (
    echo ERROR: Expected configuration Debug or Release.
    exit /b 2
)

call "%~dp0common.bat" || exit /b 1

if /i "%CONFIGURATION%"=="Debug" (
    set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-debug"
) else (
    set "BUILD_DIR=%PROJECT_ROOT%\build\cmake-release"
)

"%CMAKE_EXE%" -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    "-DCMAKE_BUILD_TYPE=%CONFIGURATION%" ^
    "-DCMAKE_PREFIX_PATH=%QT_ROOT%" ^
    "-DCMAKE_CXX_COMPILER=%MINGW_ROOT%\bin\g++.exe"
if errorlevel 1 (
    echo ERROR: CMake configuration failed for %CONFIGURATION%.
    exit /b 1
)

"%CMAKE_EXE%" --build "%BUILD_DIR%" --parallel
if errorlevel 1 (
    echo ERROR: CMake build failed for %CONFIGURATION%.
    exit /b 1
)

echo Ready: "%BUILD_DIR%\TechDraw.exe"
exit /b 0

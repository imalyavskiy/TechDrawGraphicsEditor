@echo off
rem Purpose: initialize and validate the shared build environment for other command files.
rem Requirements: repository layout, environment.bat or QT_ROOT/MINGW_ROOT, CMake and Ninja in PATH.
rem Parameter 1: x64 (default) or x86. Result: exports PROJECT_ROOT, ARCHITECTURE,
rem tool paths, PATH and QT_PLUGIN_PATH to the caller.
rem Exit codes: 0 on success, 1 when the repository or a required tool cannot be found.
rem This helper deliberately omits setlocal because callers need the variables it defines.

rem Resolve the repository root from this file, independently of the caller's working directory.
for %%I in ("%~dp0..\..") do set "PROJECT_ROOT=%%~fI"

rem Refuse an unrelated directory before any build or cleanup script uses PROJECT_ROOT.
for %%D in (src resources docs scripts\windows) do (
    if not exist "%PROJECT_ROOT%\%%D\" (
        echo ERROR: The project root is invalid. Missing directory: "%PROJECT_ROOT%\%%D"
        exit /b 1
    )
)

if not exist "%PROJECT_ROOT%\CMakeLists.txt" (
    echo ERROR: The project root is invalid. Missing file: "%PROJECT_ROOT%\CMakeLists.txt"
    exit /b 1
)

rem Load optional machine-local paths; the file is ignored by Git.
if exist "%~dp0environment.bat" call "%~dp0environment.bat"

rem Existing scripts remain x64 by default. Legacy QT_ROOT/MINGW_ROOT variables continue to
rem configure x64, while the explicit suffixed variables allow both toolchains to coexist.
set "ARCHITECTURE=%~1"
if not defined ARCHITECTURE set "ARCHITECTURE=x64"
if /i "%ARCHITECTURE%"=="x64" (
    if defined QT_ROOT_X64 set "QT_ROOT=%QT_ROOT_X64%"
    if defined MINGW_ROOT_X64 set "MINGW_ROOT=%MINGW_ROOT_X64%"
) else if /i "%ARCHITECTURE%"=="x86" (
    set "QT_ROOT=%QT_ROOT_X86%"
    set "MINGW_ROOT=%MINGW_ROOT_X86%"
) else (
    echo ERROR: Expected architecture x64 or x86, got "%ARCHITECTURE%".
    exit /b 2
)

if not defined QT_ROOT (
    echo ERROR: Qt root is not set for %ARCHITECTURE%.
    echo Copy "%~dp0environment.example.bat" to environment.bat and configure the matching Qt 5.15.2 path.
    exit /b 1
)
if not defined MINGW_ROOT (
    echo ERROR: MinGW root is not set for %ARCHITECTURE%.
    echo Copy "%~dp0environment.example.bat" to environment.bat and configure the matching MinGW path.
    exit /b 1
)

if not exist "%QT_ROOT%\lib\cmake\Qt5\Qt5Config.cmake" (
    echo ERROR: Qt 5 CMake package was not found under QT_ROOT: "%QT_ROOT%"
    exit /b 1
)
if not exist "%MINGW_ROOT%\bin\g++.exe" (
    echo ERROR: MinGW C++ compiler was not found under MINGW_ROOT: "%MINGW_ROOT%"
    exit /b 1
)

rem Resolve command-line tools once and expose their absolute paths to nested scripts.
for /f "delims=" %%I in ('where cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"
if not defined CMAKE_EXE (
    echo ERROR: cmake.exe was not found in PATH.
    exit /b 1
)
for /f "delims=" %%I in ('where ninja.exe 2^>nul') do if not defined NINJA_EXE set "NINJA_EXE=%%I"
if not defined NINJA_EXE (
    echo ERROR: ninja.exe was not found in PATH.
    exit /b 1
)

rem Put the matching Qt and MinGW runtime first for configure, build, test and Debug launch.
set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_ROOT%\plugins"
exit /b 0

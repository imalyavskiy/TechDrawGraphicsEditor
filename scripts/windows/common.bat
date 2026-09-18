@echo off
rem Shared environment initialization for the public Windows command files.
rem The caller keeps the variables because this helper deliberately does not use setlocal.

for %%I in ("%~dp0..\..") do set "PROJECT_ROOT=%%~fI"

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

if exist "%~dp0environment.bat" call "%~dp0environment.bat"

if not defined QT_ROOT (
    echo ERROR: QT_ROOT is not set.
    echo Copy "%~dp0environment.example.bat" to environment.bat and set the Qt 5.15.2 path.
    exit /b 1
)
if not defined MINGW_ROOT (
    echo ERROR: MINGW_ROOT is not set.
    echo Copy "%~dp0environment.example.bat" to environment.bat and set the MinGW 8.1.0 path.
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

set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;%PATH%"
set "QT_PLUGIN_PATH=%QT_ROOT%\plugins"
exit /b 0

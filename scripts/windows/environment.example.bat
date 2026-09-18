@echo off
rem Purpose: template for machine-local Qt and MinGW paths consumed by common.bat.
rem Usage: copy to environment.bat and edit both values; do not run this example directly.
rem Result: common.bat imports the variables. The local environment.bat is ignored by Git.
rem x64 is required for the supported build. x86 is optional and enables the additional scripts.
set "QT_ROOT_X64=C:\Qt\5.15.2\mingw81_64"
set "MINGW_ROOT_X64=C:\Qt\Tools\mingw810_64"
set "QT_ROOT_X86=C:\Qt\5.15.2\mingw81_32"
set "MINGW_ROOT_X86=C:\Qt\Tools\mingw810_32"

rem Qt Installer Framework is architecture-independent from the packaged application.
rem One 64-bit IFW toolset creates the supported x64 package and the optional x86 payload package.
set "QT_IFW_ROOT=C:\Qt\Tools\QtInstallerFramework\4.11"

rem Legacy names are accepted as x64 aliases; new configurations should use the explicit names above.
set "QT_ROOT=%QT_ROOT_X64%"
set "MINGW_ROOT=%MINGW_ROOT_X64%"

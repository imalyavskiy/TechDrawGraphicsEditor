@echo off
rem Purpose: template for machine-local Qt and MinGW paths consumed by common.bat.
rem Usage: copy to environment.bat and edit both values; do not run this example directly.
rem Result: common.bat imports the variables. The local environment.bat is ignored by Git.
set "QT_ROOT=C:\Qt\5.15.2\mingw81_64"
set "MINGW_ROOT=C:\Qt\Tools\mingw810_64"

@echo off
rem Run the built-in checks against the portable Release package on both Qt platforms.
setlocal

call "%~dp0common.bat" || exit /b 1
python "%PROJECT_ROOT%\tools\check_translations.py" || exit /b 1
for %%P in (offscreen windows) do (
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0test.ps1" -Platform %%P
    if errorlevel 1 (
        echo ERROR: Self-test failed on the %%P platform.
        exit /b 1
    )
)

exit /b 0

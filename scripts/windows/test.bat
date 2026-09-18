@echo off
rem Purpose: audit translations and run the portable Release self-test on two Qt platforms.
rem Requirements: Python 3, PowerShell 5 and a successful build-release.bat run.
rem Parameters: none. Results: build\offscreen-results and build\windows-results.
rem Exit codes: 0 when every audit/test passes, 1 on setup, audit or self-test failure.
setlocal

rem Validate the environment, then reject untranslated source literals before starting the GUI tests.
call "%~dp0common.bat" || exit /b 1
python "%PROJECT_ROOT%\tools\check_translations.py" || exit /b 1
rem Offscreen covers deterministic rendering; windows additionally exercises native dialogs.
for %%P in (offscreen windows) do (
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0test.ps1" -Platform %%P
    if errorlevel 1 (
        echo ERROR: Self-test failed on the %%P platform.
        exit /b 1
    )
)

exit /b 0

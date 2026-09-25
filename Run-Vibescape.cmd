@echo off
rem Launch our packaged build with its separate Vibescape preferences.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Vibescape.ps1" %*
if errorlevel 1 (
    echo.
    echo Vibescape could not start. See the error above.
    pause
    exit /b 1
)

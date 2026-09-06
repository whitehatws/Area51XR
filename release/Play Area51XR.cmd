@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Area51XR.ps1"
set "A51XR_EXIT=%ERRORLEVEL%"
if not "%A51XR_EXIT%"=="0" (
    echo.
    echo Area51XR stopped with exit code %A51XR_EXIT%.
    echo Review the message above or the logs folder for details.
    pause
)
exit /b %A51XR_EXIT%

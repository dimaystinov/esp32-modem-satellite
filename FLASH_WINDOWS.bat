@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\flash-windows.ps1"
set "flash_result=%ERRORLEVEL%"
echo.
if not "%flash_result%"=="0" echo Flash failed. Read the error above before retrying.
pause
exit /b %flash_result%

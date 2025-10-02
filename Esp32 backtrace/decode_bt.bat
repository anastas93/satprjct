@echo off
setlocal
set "LOG=%TEMP%\decode_bt.log"
echo [START] %DATE% %TIME% > "%LOG%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0decode_bt.ps1" 1>>"%LOG%" 2>>&1
set RC=%ERRORLEVEL%
echo [END ] %DATE% %TIME% RC=%RC%>> "%LOG%"
type "%LOG%"
echo.
echo --- Работа завершена. Код возврата: %RC% ---
pause
exit /b %RC%

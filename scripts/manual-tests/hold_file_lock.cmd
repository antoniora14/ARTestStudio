@echo off
setlocal
set "TARGET_PATH=%~1"
if not defined TARGET_PATH set /p "TARGET_PATH=Ruta completa del archivo .atd que se debe bloquear: "
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Hold-FileLock.ps1" -Path "%TARGET_PATH%"
set "EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%EXIT_CODE%"=="0" echo La prueba de bloqueo fallo con codigo %EXIT_CODE%.
pause
exit /b %EXIT_CODE%

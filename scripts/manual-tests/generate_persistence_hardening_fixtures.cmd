@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0New-PersistenceHardeningFixtures.ps1"
set "EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%EXIT_CODE%"=="0" echo La generacion de fixtures fallo con codigo %EXIT_CODE%.
pause
exit /b %EXIT_CODE%

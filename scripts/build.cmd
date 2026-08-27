@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*

if errorlevel 1 (
    echo.
    echo La compilacion fallo.
    exit /b 1
)

echo.
echo Compilacion y pruebas completadas correctamente.
exit /b 0
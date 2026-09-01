@echo off
setlocal

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*

if errorlevel 1 (
    echo.
    echo The build failed.
    exit /b 1
)

echo.
echo Build and tests completed successfully.
exit /b 0

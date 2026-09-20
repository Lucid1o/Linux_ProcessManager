@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."

if exist "%ROOT_DIR%\build\ui\pulse.exe" (
    set "BINARY=%ROOT_DIR%\build\ui\pulse.exe"
) else if exist "%ROOT_DIR%\build\bin\pulse.exe" (
    set "BINARY=%ROOT_DIR%\build\bin\pulse.exe"
) else (
    echo Error: pulse.exe not found. Please build the project first.
    exit /b 1
)

echo Launching Pulse from: %BINARY%
start "" "%BINARY%" %*

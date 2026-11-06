@echo off
setlocal
echo ============================================================
echo Croc 2 Archipelago — One-Click Release & GitHub Publisher
echo ============================================================
echo.

set PYTHON="C:\msys64\usr\bin\python.exe"
if not exist %PYTHON% (
    set PYTHON=python
)

%PYTHON% publish_release.py %*

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Release automation encountered an issue (Exit Code %ERRORLEVEL%).
) else (
    echo.
    echo [SUCCESS] Release build and git packaging completed!
)

echo.
pause


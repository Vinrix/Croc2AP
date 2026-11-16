@echo off
setlocal
:: Gathers everything needed for a bug report into one folder on the Desktop:
::   croc2_ap_debug.log      what the mod saw (written by test builds, or when debug_watch = 1)
::   croc2ap_activity.log    the launcher's list of everything sent and received
::   croc2ap_crystals_*.txt  which crystals each save has collected
::   croc2_mod.ini           your settings
:: Run it straight after a crash or a problem, before starting the game again.

set "GAME_DIR=C:\Program Files (x86)\Steam\steamapps\common\Croc 2"
if not exist "%GAME_DIR%\croc2.exe" (
    if exist "C:\Program Files\Steam\steamapps\common\Croc 2\croc2.exe" set "GAME_DIR=C:\Program Files\Steam\steamapps\common\Croc 2"
)
if not exist "%GAME_DIR%\croc2.exe" (
    if exist "%~dp0croc2.exe" set "GAME_DIR=%~dp0"
)
if not exist "%GAME_DIR%\croc2.exe" (
    set /p "GAME_DIR=Enter the full path to your Croc 2 folder: "
)
if not exist "%GAME_DIR%\croc2.exe" (
    echo [ERROR] croc2.exe was not found in "%GAME_DIR%".
    pause
    exit /b 1
)

for /f "tokens=1-6 delims=/:. " %%a in ("%date% %time%") do set "STAMP=%%a%%b%%c_%%d%%e%%f"
set "OUT=%USERPROFILE%\Desktop\Croc2AP_report_%STAMP%"
mkdir "%OUT%" 2>nul

set COUNT=0
for %%F in ("%GAME_DIR%\croc2_ap_debug.log" "%GAME_DIR%\croc2ap_activity.log" "%GAME_DIR%\croc2_mod.ini") do (
    if exist "%%~F" (
        copy /Y "%%~F" "%OUT%\" >nul
        set /a COUNT+=1
    )
)
for %%F in ("%GAME_DIR%\croc2ap_crystals_*.txt") do (
    copy /Y "%%~F" "%OUT%\" >nul
    set /a COUNT+=1
)

if "%COUNT%"=="0" (
    echo Nothing to collect in "%GAME_DIR%".
    echo The debug log is only written by test builds, or when croc2_mod.ini has debug_watch = 1.
    rmdir "%OUT%" 2>nul
    pause
    exit /b 1
)

echo Collected %COUNT% file(s) into:
echo   %OUT%
echo Drag those files into your GitHub issue or comment.
explorer "%OUT%"
pause

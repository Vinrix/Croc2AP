@echo off
setlocal
:: Gathers everything needed for a bug report into one folder on the Desktop:
::   croc2_ap_debug.log      what the mod saw (written by test builds, or when debug_watch = 1)
::   croc2ap_activity.log    the launcher's list of everything sent and received
::   croc2ap_crystals_*.txt  which crystals each save has collected
::   croc2_mod.ini           your settings (the server password line is left out)
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

:: A timestamp that reads the same whatever the PC's date format is.
set "STAMP=report"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%i"
:: The real Desktop folder: it is somewhere else when OneDrive backs the Desktop up.
set "DESKTOP=%USERPROFILE%\Desktop"
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('Desktop')"`) do set "DESKTOP=%%i"
set "OUT=%DESKTOP%\Croc2AP_report_%STAMP%"
mkdir "%OUT%" 2>nul

set COUNT=0
for %%F in ("%GAME_DIR%\croc2_ap_debug.log" "%GAME_DIR%\croc2ap_activity.log") do (
    if exist "%%~F" (
        copy /Y "%%~F" "%OUT%\" >nul
        set /a COUNT+=1
    )
)
:: The settings file, without the server password line: this folder is meant to be shared.
if exist "%GAME_DIR%\croc2_mod.ini" (
    findstr /V /I /B /C:"password" "%GAME_DIR%\croc2_mod.ini" > "%OUT%\croc2_mod.ini"
    set /a COUNT+=1
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

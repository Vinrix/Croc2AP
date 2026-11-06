@echo off
setlocal EnableDelayedExpansion
title Croc 2 Archipelago Randomizer - Installer

:: Ensure working directory is the script directory
cd /d "%~dp0"

:: Check for Administrator privileges (needed for Program Files (x86) and ProgramData)
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting Administrator privileges to install into Steam and Archipelago folders...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b
)

set "GAME_DIR=C:\Program Files (x86)\Steam\steamapps\common\Croc 2"
set "AP_DIR=C:\ProgramData\Archipelago"

echo ============================================================
echo   Croc 2 Archipelago Randomizer - Mod Installer
echo ============================================================
echo.

if not exist "%GAME_DIR%\croc2.exe" (
    echo [!] Default Croc 2 path not found at "%GAME_DIR%".
    set /p "GAME_DIR=Enter full path to your Croc 2 folder: "
)

if not exist "build\version.dll" (
    echo [ERROR] build\version.dll not found in "%~dp0build\".
    pause
    exit /b 1
)

:: Backup existing version.dll if it is not already backed up
if exist "%GAME_DIR%\version.dll" (
    if not exist "%GAME_DIR%\version.dll.vanilla_backup" (
        copy /Y "%GAME_DIR%\version.dll" "%GAME_DIR%\version.dll.vanilla_backup" >nul
        echo [+] Backed up original version.dll to version.dll.vanilla_backup
    )
)

:: Copy Hook DLL and Mod INI to Croc 2 folder
copy /Y "build\version.dll" "%GAME_DIR%\version.dll" >nul
if %errorLevel% neq 0 (
    echo [ERROR] Failed to copy version.dll to "%GAME_DIR%". Make sure Croc 2 is closed.
    pause
    exit /b 1
)
echo [+] Installed Hook DLL: "%GAME_DIR%\version.dll"

if not exist "%GAME_DIR%\croc2_mod.ini" (
    copy /Y "croc2_mod.ini" "%GAME_DIR%\croc2_mod.ini" >nul
    echo [+] Installed Config:   "%GAME_DIR%\croc2_mod.ini"
) else (
    echo [i] Preserved existing "%GAME_DIR%\croc2_mod.ini"
)

:: Install .apworld and .yaml template into Archipelago if present
if exist "%AP_DIR%" (
    if not exist "%AP_DIR%\custom_worlds" mkdir "%AP_DIR%\custom_worlds"
    copy /Y "dist\croc2.apworld" "%AP_DIR%\custom_worlds\croc2.apworld" >nul
    echo [+] Installed APWorld:  "%AP_DIR%\custom_worlds\croc2.apworld"

    if not exist "%AP_DIR%\Players\Templates" mkdir "%AP_DIR%\Players\Templates"
    copy /Y "Croc 2.yaml" "%AP_DIR%\Players\Templates\Croc 2.yaml" >nul
    echo [+] Installed YAML:     "%AP_DIR%\Players\Templates\Croc 2.yaml"
) else (
    echo [!] Archipelago directory "%AP_DIR%" not found. Skipped APWorld copy.
)

echo.
echo ============================================================
echo   Installation Complete!
echo   - Note: The mod runs in Dormant Vanilla Mode automatically
echo     whenever Croc2Client is not connected, or press F10 in-game.
echo ============================================================
pause


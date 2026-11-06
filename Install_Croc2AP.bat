@echo off
setlocal EnableDelayedExpansion
title Croc 2 Archipelago Randomizer - Installer

:: Ensure working directory is the script directory
cd /d "%~dp0"

:: Check for Administrator privileges
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

:: Detect game directory
if not exist "%GAME_DIR%\croc2.exe" (
    if exist "C:\Program Files\Steam\steamapps\common\Croc 2\croc2.exe" (
        set "GAME_DIR=C:\Program Files\Steam\steamapps\common\Croc 2"
    ) else (
        echo [!] Default Croc 2 path not found at "%GAME_DIR%".
        set /p "GAME_DIR=Enter full path to your Croc 2 folder: "
    )
)

:: Locate hook DLL (support root version.dll or build\version.dll)
set "HOOK_SRC="
if exist "%~dp0version.dll" set "HOOK_SRC=%~dp0version.dll"
if not defined HOOK_SRC (
    if exist "%~dp0build\version.dll" set "HOOK_SRC=%~dp0build\version.dll"
)
if not defined HOOK_SRC (
    echo [ERROR] version.dll not found in "%~dp0" or "%~dp0build\".
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

:: Copy Hook DLL
copy /Y "%HOOK_SRC%" "%GAME_DIR%\version.dll" >nul
if %errorLevel% neq 0 (
    echo [ERROR] Failed to copy version.dll to "%GAME_DIR%". Make sure Croc 2 is closed.
    pause
    exit /b 1
)
echo [+] Installed Hook DLL: "%GAME_DIR%\version.dll"

:: Copy Mod INI
if exist "%~dp0croc2_mod.ini" (
    if not exist "%GAME_DIR%\croc2_mod.ini" (
        copy /Y "%~dp0croc2_mod.ini" "%GAME_DIR%\croc2_mod.ini" >nul
        echo [+] Installed Config:   "%GAME_DIR%\croc2_mod.ini"
    ) else (
        echo [i] Preserved existing "%GAME_DIR%\croc2_mod.ini"
    )
)

:: Copy Launcher
if exist "%~dp0Croc2ArchipelagoLauncher.exe" (
    copy /Y "%~dp0Croc2ArchipelagoLauncher.exe" "%GAME_DIR%\Croc2ArchipelagoLauncher.exe" >nul
    echo [+] Installed Launcher: "%GAME_DIR%\Croc2ArchipelagoLauncher.exe"
)

:: Copy Instructions
if exist "%~dp0INSTALL_INSTRUCTIONS.txt" (
    copy /Y "%~dp0INSTALL_INSTRUCTIONS.txt" "%GAME_DIR%\INSTALL_INSTRUCTIONS.txt" >nul
)

:: Locate apworld file
set "APWORLD_SRC="
if exist "%~dp0croc2.apworld" set "APWORLD_SRC=%~dp0croc2.apworld"
if not defined APWORLD_SRC (
    if exist "%~dp0dist\croc2.apworld" set "APWORLD_SRC=%~dp0dist\croc2.apworld"
)

:: Locate YAML template
set "YAML_SRC="
if exist "%~dp0Croc 2.yaml" set "YAML_SRC=%~dp0Croc 2.yaml"
if not defined YAML_SRC (
    if exist "%~dp0templates\Croc 2.yaml" set "YAML_SRC=%~dp0templates\Croc 2.yaml"
)

:: Install .apworld and .yaml template into Archipelago
if exist "%AP_DIR%" (
    if defined APWORLD_SRC (
        if not exist "%AP_DIR%\custom_worlds" mkdir "%AP_DIR%\custom_worlds"
        copy /Y "%APWORLD_SRC%" "%AP_DIR%\custom_worlds\croc2.apworld" >nul
        echo [+] Installed APWorld:  "%AP_DIR%\custom_worlds\croc2.apworld"
    )
    if defined YAML_SRC (
        if not exist "%AP_DIR%\Players\Templates" mkdir "%AP_DIR%\Players\Templates"
        copy /Y "%YAML_SRC%" "%AP_DIR%\Players\Templates\Croc 2.yaml" >nul
        echo [+] Installed YAML:     "%AP_DIR%\Players\Templates\Croc 2.yaml"
    )
) else (
    echo [!] Archipelago directory "%AP_DIR%" not found.
    echo     Please copy croc2.apworld into your Archipelago custom_worlds folder.
)

echo.
echo ============================================================
echo   Installation Complete!
echo   - Note: The mod runs in Dormant Vanilla Mode automatically
echo     whenever the launcher is not connected, or press F10 in-game.
echo ============================================================
pause

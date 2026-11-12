@echo off
setlocal EnableDelayedExpansion
title Croc 2 Archipelago Randomizer - Uninstaller

:: Ensure working directory is the script directory
cd /d "%~dp0"

:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting Administrator privileges to uninstall from Steam and Archipelago folders...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b
)

set "GAME_DIR=C:\Program Files (x86)\Steam\steamapps\common\Croc 2"
set "AP_DIR=C:\ProgramData\Archipelago"

echo ============================================================
echo   Croc 2 Archipelago Randomizer - Mod Uninstaller
echo ============================================================
echo.

if exist "%GAME_DIR%\version.dll" (
    del /F /Q "%GAME_DIR%\version.dll"
    echo [-] Removed "%GAME_DIR%\version.dll"
)

if exist "%GAME_DIR%\croc2_ap_hook.dll" (
    del /F /Q "%GAME_DIR%\croc2_ap_hook.dll"
    echo [-] Removed "%GAME_DIR%\croc2_ap_hook.dll"
)

if exist "%GAME_DIR%\croc2_mod.ini" (
    del /F /Q "%GAME_DIR%\croc2_mod.ini"
    echo [-] Removed "%GAME_DIR%\croc2_mod.ini"
)

if exist "%GAME_DIR%\version.dll.vanilla_backup" (
    move /Y "%GAME_DIR%\version.dll.vanilla_backup" "%GAME_DIR%\version.dll" >nul
    echo [+] Restored original "%GAME_DIR%\version.dll" from backup
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$dir = '%GAME_DIR%\remaster\models'; if (Test-Path $dir) { Get-ChildItem -Path $dir -Recurse -Filter '*.zmod.vanilla' -ErrorAction SilentlyContinue | ForEach-Object { $orig = $_.FullName.Substring(0, $_.FullName.Length - 8); Copy-Item -Path $_.FullName -Destination $orig -Force; Remove-Item $_.FullName -Force; Write-Host ('[+] Restored vanilla model ' + $orig) } }"

if exist "%AP_DIR%\custom_worlds\croc2.apworld" (
    del /F /Q "%AP_DIR%\custom_worlds\croc2.apworld"
    echo [-] Removed "%AP_DIR%\custom_worlds\croc2.apworld"
)

if exist "%AP_DIR%\Players\Templates\Croc 2.yaml" (
    del /F /Q "%AP_DIR%\Players\Templates\Croc 2.yaml"
    echo [-] Removed "%AP_DIR%\Players\Templates\Croc 2.yaml"
)

echo.
echo ============================================================
echo   Uninstallation Complete! Croc 2 is 100%% Vanilla.
echo ============================================================
pause


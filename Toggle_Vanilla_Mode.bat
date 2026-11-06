@echo off
setlocal EnableDelayedExpansion
title Croc 2 Archipelago Randomizer - Toggle Vanilla / Mod Mode

cd /d "%~dp0"

net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b
)

set "INI_FILE=C:\Program Files (x86)\Steam\steamapps\common\Croc 2\croc2_mod.ini"

if not exist "%INI_FILE%" (
    echo [!] "%INI_FILE%" not found. Please run Install_Croc2AP.bat first.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$p = '%INI_FILE%'; $c = Get-Content $p -Raw; if ($c -match '(?m)^\s*enabled\s*=\s*true') { $c = $c -replace '(?m)^(\s*enabled\s*=\s*)true', '${1}false'; Set-Content $p $c -NoNewline; Write-Host '[+] Switched croc2_mod.ini -> enabled = false (100% Vanilla Mode)' } else { $c = $c -replace '(?m)^(\s*enabled\s*=\s*)false', '${1}true'; Set-Content $p $c -NoNewline; Write-Host '[+] Switched croc2_mod.ini -> enabled = true (Archipelago Randomizer Mode)' }"

pause


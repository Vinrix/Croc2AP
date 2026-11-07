@echo off
setlocal EnableDelayedExpansion
echo ============================================================
echo   Compiling Croc 2 Archipelago 1-Click Installer Setup EXE...
echo ============================================================

set CSC="C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"

if not exist build\version.dll (
    echo [ERROR] build\version.dll is missing! Please build hook first.
    exit /b 1
)

if not exist Croc2ArchipelagoLauncher.exe (
    echo [ERROR] Croc2ArchipelagoLauncher.exe is missing! Please build launcher first.
    exit /b 1
)

if not exist dist\croc2.apworld (
    echo [ERROR] dist\croc2.apworld is missing! Please build apworld first.
    exit /b 1
)

%CSC% /target:winexe /out:Croc2AP_Setup.exe /win32icon:icon.ico ^
  /r:System.dll /r:System.Core.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll ^
  /resource:build\version.dll,version.dll ^
  /resource:croc2_mod.ini,croc2_mod.ini ^
  /resource:Croc2ArchipelagoLauncher.exe,Croc2ArchipelagoLauncher.exe ^
  /resource:dist\croc2.apworld,croc2.apworld ^
  "/resource:templates\Croc 2.yaml,Croc 2.yaml" ^
  /resource:INSTALL_INSTRUCTIONS.txt,INSTALL_INSTRUCTIONS.txt ^
  /resource:assets\fonts\DarumadropOne-Regular.ttf,DarumadropOne-Regular.ttf ^
  /resource:assets\icon.png,icon.png ^
  /resource:icon.ico,icon.ico ^
  /recurse:src\setup\*.cs

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo BUILD SUCCESSFUL: Croc2AP_Setup.exe created
    echo ============================================================
) else (
    echo.
    echo [ERROR] Setup build failed with code %ERRORLEVEL%
)

exit /b %ERRORLEVEL%

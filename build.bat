@echo off
setlocal
echo Compiling Croc 2 Archipelago Standalone Launcher...

set CSC="C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"

%CSC% /target:winexe /out:Croc2ArchipelagoLauncher.exe /r:System.dll /r:System.Core.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Web.Extensions.dll /recurse:src\launcher\*.cs

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo BUILD SUCCESSFUL: Croc2ArchipelagoLauncher.exe created!
    echo ============================================================
) else (
    echo.
    echo [ERROR] Build failed with error code %ERRORLEVEL%
)

exit /b %ERRORLEVEL%


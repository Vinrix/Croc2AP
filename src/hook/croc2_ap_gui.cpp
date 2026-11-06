#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

using InitFn = void(__cdecl*)(const char*);
using StartUIFn = void(__cdecl*)();
using ShowMenuFn = void(__cdecl*)(int);
using ShutFn = void(__cdecl*)();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Load croc2_ap_hook.dll from build/ or current directory
    HMODULE hMod = LoadLibraryA("build\\croc2_ap_hook.dll");
    if (!hMod) {
        hMod = LoadLibraryA("croc2_ap_hook.dll");
    }
    if (!hMod) {
        hMod = LoadLibraryA("version.dll");
    }
    if (!hMod) {
        MessageBoxA(nullptr, "Could not load build\\croc2_ap_hook.dll!", "Croc 2 Archipelago GUI", MB_ICONERROR);
        return 1;
    }

    auto fnInit = reinterpret_cast<InitFn>(reinterpret_cast<void*>(GetProcAddress(hMod, "Croc2AP_Initialize")));
    auto fnStartUI = reinterpret_cast<StartUIFn>(reinterpret_cast<void*>(GetProcAddress(hMod, "Croc2AP_StartOverlayUI")));
    auto fnShowMenu = reinterpret_cast<ShowMenuFn>(reinterpret_cast<void*>(GetProcAddress(hMod, "Croc2AP_ShowConnectionMenu")));
    auto fnShut = reinterpret_cast<ShutFn>(reinterpret_cast<void*>(GetProcAddress(hMod, "Croc2AP_Shutdown")));

    if (!fnInit || !fnStartUI || !fnShowMenu || !fnShut) {
        MessageBoxA(nullptr, "Missing entrypoints in croc2_ap_hook.dll!", "Croc 2 Archipelago GUI", MB_ICONERROR);
        FreeLibrary(hMod);
        return 1;
    }

    // Use the installed Steam Croc 2 croc2_mod.ini if available, otherwise local croc2_mod.ini
    const char* steam_ini = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Croc 2\\croc2_mod.ini";
    DWORD attr = GetFileAttributesA(steam_ini);
    const char* target_ini = (attr != INVALID_FILE_ATTRIBUTES) ? steam_ini : "croc2_mod.ini";

    fnInit(target_ini);
    fnStartUI();
    fnShowMenu(1);

    // Keep alive while the Croc-themed overlay/menu is open
    while (FindWindowA("Croc2APMenuWndClass", nullptr) != nullptr) {
        HWND hMenu = FindWindowA("Croc2APMenuWndClass", nullptr);
        if (hMenu && !IsWindowVisible(hMenu)) {
            break;
        }
        Sleep(100);
    }

    fnShut();
    FreeLibrary(hMod);
    return 0;
}


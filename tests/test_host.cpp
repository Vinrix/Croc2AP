#include <windows.h>
#include <cstdio>

using InitFn = void(__cdecl*)(const char*);
using ShutFn = void(__cdecl*)();

int main(int argc, char** argv) {
    const char* ini = (argc > 1) ? argv[1] : "croc2_mod.ini";
    HMODULE h = LoadLibraryA("build/croc2_ap_hook.dll");
    if (!h) {
        h = LoadLibraryA("build/version.dll");
    }
    if (!h) {
        h = LoadLibraryA("version.dll");
    }
    if (!h) {
        std::fprintf(stderr, "Failed to load hook DLL\n");
        return 1;
    }

    auto init = reinterpret_cast<InitFn>(reinterpret_cast<void*>(GetProcAddress(h, "Croc2AP_Initialize")));
    auto shut = reinterpret_cast<ShutFn>(reinterpret_cast<void*>(GetProcAddress(h, "Croc2AP_Shutdown")));
    if (!init) {
        std::fprintf(stderr, "Failed to locate Croc2AP_Initialize\n");
        FreeLibrary(h);
        return 2;
    }

    init(ini);
    Sleep(12000);
    if (shut) {
        shut();
    }
    FreeLibrary(h);
    return 0;
}

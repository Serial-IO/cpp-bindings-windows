#include <windows.h>

extern "C" __declspec(dllexport) int TestFunction() {
    return 42;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    return TRUE;
}

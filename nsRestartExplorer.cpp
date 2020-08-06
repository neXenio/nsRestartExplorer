#include "nsRestartExplorer.h"

unsigned int g_stringLength = 0;
stack_t **   g_stackTop     = NULL;

void nsRestartExplorer(HWND, int stringLength, TCHAR *, stack_t **stackTop)
{
    g_stackTop     = stackTop;
    g_stringLength = stringLength;
    auto result    = RestartExplorer();

    if (result) {
        pushString(TEXT("Successful explorer restarted."));
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

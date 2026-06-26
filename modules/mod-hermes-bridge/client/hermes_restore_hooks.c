#include <windows.h>

#define LOG_PATH "E:\\azerothcore-wotlk\\modules\\mod-hermes-bridge\\client\\build\\hermes_restore_hooks.log"

static void Log(const char* message)
{
    HANDLE file = CreateFileA(LOG_PATH, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(file, message, lstrlenA(message), &written, NULL);
        WriteFile(file, "\r\n", 2, &written, NULL);
        CloseHandle(file);
    }
}

static void RestoreBytes(DWORD address, const BYTE* bytes, DWORD length, const char* name)
{
    DWORD oldProtect = 0;
    if (!VirtualProtect((void*)address, length, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        Log(name);
        Log("VirtualProtect failed");
        return;
    }

    CopyMemory((void*)address, bytes, length);
    FlushInstructionCache(GetCurrentProcess(), (void*)address, length);
    VirtualProtect((void*)address, length, oldProtect, &oldProtect);
    Log(name);
}

static DWORD WINAPI Worker(LPVOID parameter)
{
    (void)parameter;
    static const BYTE upperSendOriginal[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x60 };
    static const BYTE afterOpcodeOriginal[5] = { 0x8B, 0x17, 0x8B, 0x42, 0x4C };
    static const BYTE sendAddonOriginal[9] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xD4, 0x0B, 0x00, 0x00 };
    Log("restore hooks attach");
    RestoreBytes(0x004675F0, upperSendOriginal, sizeof(upperSendOriginal), "restored upper-send");
    RestoreBytes(0x00632001, afterOpcodeOriginal, sizeof(afterOpcodeOriginal), "restored after-opcode");
    RestoreBytes(0x00500560, sendAddonOriginal, sizeof(sendAddonOriginal), "restored SendAddonMessage");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}

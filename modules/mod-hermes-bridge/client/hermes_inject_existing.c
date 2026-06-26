#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static void PrintError(const char* step)
{
    DWORD error = GetLastError();
    char* buffer = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        0,
        (LPSTR)&buffer,
        0,
        NULL);
    fprintf(stderr, "%s failed, error=%lu %s\n", step, error, buffer ? buffer : "");
    if (buffer)
        LocalFree(buffer);
}

int main(int argc, char** argv)
{
    const char* dllPath = "E:\\azerothcore-wotlk\\modules\\mod-hermes-bridge\\client\\build\\hermes_bridge.dll";
    DWORD pid = 0;

    if (argc >= 2)
        pid = (DWORD)strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        dllPath = argv[2];

    if (!pid)
    {
        fprintf(stderr, "usage: hermes_inject_existing.exe <wow-pid> [dll-path]\n");
        return 1;
    }

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        pid);
    if (!process)
    {
        PrintError("OpenProcess");
        return 2;
    }

    SIZE_T pathLen = lstrlenA(dllPath) + 1;
    LPVOID remotePath = VirtualAllocEx(process, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath)
    {
        PrintError("VirtualAllocEx");
        CloseHandle(process);
        return 3;
    }

    if (!WriteProcessMemory(process, remotePath, dllPath, pathLen, NULL))
    {
        PrintError("WriteProcessMemory");
        CloseHandle(process);
        return 4;
    }

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(kernel32, "LoadLibraryA");
    if (!loadLibrary)
    {
        PrintError("GetProcAddress LoadLibraryA");
        CloseHandle(process);
        return 5;
    }

    HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibrary, remotePath, 0, NULL);
    if (!thread)
    {
        PrintError("CreateRemoteThread");
        CloseHandle(process);
        return 6;
    }

    WaitForSingleObject(thread, 10000);
    DWORD remoteResult = 0;
    GetExitCodeThread(thread, &remoteResult);
    CloseHandle(thread);
    CloseHandle(process);

    printf("LoadLibraryA remote result=0x%08lX\n", remoteResult);
    return remoteResult ? 0 : 7;
}

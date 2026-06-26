#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define DEFAULT_WOW_EXE ""
#define DEFAULT_DLL_PATH ""
#define DEFAULT_TARGET_DLL_NAME "HermesBridge.dll"
#define DEFAULT_STATE_PATH ""
#define DEFAULT_LOG_PATH ""

typedef struct AutoInjectConfig
{
    int once;
    DWORD waitMs;
    DWORD pollMs;
    DWORD settleMs;
    int anyWow;
    DWORD targetPid;
    char wowExe[MAX_PATH];
    char dllPath[MAX_PATH];
    char targetDllName[MAX_PATH];
    char statePath[MAX_PATH];
    char logPath[MAX_PATH];
} AutoInjectConfig;

typedef struct WindowSearch
{
    DWORD pid;
    HWND hwnd;
} WindowSearch;

typedef struct TargetProcess
{
    DWORD pid;
    char path[MAX_PATH];
} TargetProcess;

typedef struct ScanStats
{
    int found;
    int alreadyLoaded;
    int handled;
    int notReadyOrFailed;
} ScanStats;

static void CopyString(char* destination, size_t destinationSize, const char* source)
{
    if (!destination || destinationSize == 0)
        return;

    destination[0] = '\0';
    if (!source)
        return;

    lstrcpynA(destination, source, (int)destinationSize);
}

static void NormalizePath(const char* input, char* output, DWORD outputSize)
{
    DWORD length;
    char original[MAX_PATH];
    char full[MAX_PATH];
    DWORD i;

    if (!input || !output || outputSize == 0)
        return;

    CopyString(original, sizeof(original), input);
    output[0] = '\0';
    length = GetFullPathNameA(original, sizeof(full), full, NULL);
    if (length > 0 && length < sizeof(full))
        CopyString(output, outputSize, full);
    else
        CopyString(output, outputSize, original);

    for (i = 0; output[i]; ++i)
    {
        if (output[i] == '/')
            output[i] = '\\';
    }
}

static int PathEquals(const char* left, const char* right)
{
    char normalizedLeft[MAX_PATH];
    char normalizedRight[MAX_PATH];

    NormalizePath(left, normalizedLeft, sizeof(normalizedLeft));
    NormalizePath(right, normalizedRight, sizeof(normalizedRight));
    return lstrcmpiA(normalizedLeft, normalizedRight) == 0;
}

static int ContainsNoCase(const char* text, const char* needle)
{
    size_t textLen;
    size_t needleLen;
    size_t i;

    if (!text || !needle)
        return 0;

    textLen = strlen(text);
    needleLen = strlen(needle);
    if (needleLen == 0 || textLen < needleLen)
        return 0;

    for (i = 0; i <= textLen - needleLen; ++i)
    {
        if (_strnicmp(text + i, needle, needleLen) == 0)
            return 1;
    }

    return 0;
}

static int GetExecutableDirectory(char* directory, DWORD directorySize)
{
    char path[MAX_PATH];
    DWORD length;
    DWORD i;
    DWORD lastSlash = 0;

    if (!directory || directorySize == 0)
        return 0;

    directory[0] = '\0';
    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (length == 0 || length >= sizeof(path))
        return 0;

    for (i = 0; path[i]; ++i)
    {
        if (path[i] == '\\' || path[i] == '/')
            lastSlash = i;
    }

    if (lastSlash == 0 || lastSlash >= directorySize)
        return 0;

    CopyMemory(directory, path, lastSlash);
    directory[lastSlash] = '\0';
    return 1;
}

static int BuildPathInDirectory(const char* directory, const char* filename, char* output, DWORD outputSize)
{
    int written;

    if (!directory || !directory[0] || !filename || !filename[0] || !output || outputSize == 0)
        return 0;

    written = _snprintf_s(output, outputSize, _TRUNCATE, "%s\\%s", directory, filename);
    return written > 0 && (DWORD)written < outputSize;
}

static void LogMessage(const AutoInjectConfig* config, const char* format, ...)
{
    FILE* file = NULL;
    va_list args;
    va_list copy;
    time_t now;
    struct tm localTime;
    char stamp[64];

    time(&now);
    localtime_s(&localTime, &now);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &localTime);

    if (config && config->logPath[0])
        fopen_s(&file, config->logPath, "a");

    printf("[%s] ", stamp);
    va_start(args, format);
    va_copy(copy, args);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);

    if (file)
    {
        fprintf(file, "[%s] ", stamp);
        vfprintf(file, format, copy);
        fprintf(file, "\n");
        fclose(file);
    }
    va_end(copy);
}

static void FormatLastErrorMessage(DWORD error, char* buffer, DWORD bufferSize)
{
    DWORD length;

    if (!buffer || bufferSize == 0)
        return;

    buffer[0] = '\0';
    length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        0,
        buffer,
        bufferSize,
        NULL);

    if (length == 0)
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "error=%lu", error);
}

static void PrintUsage(void)
{
    printf("Hermes Auto Injector\n");
    printf("Usage: hermes_auto_injector.exe [options]\n\n");
    printf("Options:\n");
    printf("  --once                 Inject once and exit after the target is handled.\n");
    printf("  --monitor              Keep monitoring for future wow.exe processes (default).\n");
    printf("  --any-wow              Accept any running wow.exe path (default).\n");
    printf("  --wow-exe <path>       Restrict injection to one trusted wow.exe path.\n");
    printf("  --pid <pid>            Restrict injection to one running wow.exe process id.\n");
    printf("  --dll <path>           Source HermesBridge DLL path. Default: injector-dir\\%s\n", DEFAULT_TARGET_DLL_NAME);
    printf("  --target-dll-name <n>  DLL filename copied into the detected WoW directory. Default: %s\n", DEFAULT_TARGET_DLL_NAME);
    printf("  --state <path>         Active-state file path. Default: injector-dir\\hermes_auto_injector.state\n");
    printf("  --log <path>           Auto injector log path. Default: injector-dir\\hermes_auto_injector.log\n");
    printf("  --wait-ms <number>     Max wait in --once mode. Default: 120000.\n");
    printf("  --poll-ms <number>     Process scan interval. Default: 1000.\n");
    printf("  --settle-ms <number>   Extra wait after the WoW window appears. Default: 5000.\n");
    printf("  --help                 Show this help.\n");
}

static int ReadNumberArgument(int argc, char** argv, int* index, DWORD* value)
{
    char* end = NULL;
    unsigned long parsed;

    if (*index + 1 >= argc)
        return 0;

    parsed = strtoul(argv[++(*index)], &end, 10);
    if (!end || *end != '\0')
        return 0;

    *value = (DWORD)parsed;
    return 1;
}

static int ReadPathArgument(int argc, char** argv, int* index, char* destination, size_t destinationSize)
{
    if (*index + 1 >= argc)
        return 0;

    CopyString(destination, destinationSize, argv[++(*index)]);
    return 1;
}

static int ParseArguments(int argc, char** argv, AutoInjectConfig* config)
{
    int i;

    ZeroMemory(config, sizeof(*config));
    config->once = 0;
    config->waitMs = 120000;
    config->pollMs = 1000;
    config->settleMs = 5000;
    config->anyWow = 1;
    config->targetPid = 0;
    CopyString(config->wowExe, sizeof(config->wowExe), DEFAULT_WOW_EXE);
    CopyString(config->dllPath, sizeof(config->dllPath), DEFAULT_DLL_PATH);
    CopyString(config->targetDllName, sizeof(config->targetDllName), DEFAULT_TARGET_DLL_NAME);
    CopyString(config->statePath, sizeof(config->statePath), DEFAULT_STATE_PATH);
    CopyString(config->logPath, sizeof(config->logPath), DEFAULT_LOG_PATH);

    for (i = 1; i < argc; ++i)
    {
        if (lstrcmpiA(argv[i], "--help") == 0 || lstrcmpiA(argv[i], "/?") == 0)
        {
            PrintUsage();
            exit(0);
        }
        else if (lstrcmpiA(argv[i], "--once") == 0)
        {
            config->once = 1;
        }
        else if (lstrcmpiA(argv[i], "--monitor") == 0)
        {
            config->once = 0;
        }
        else if (lstrcmpiA(argv[i], "--any-wow") == 0)
        {
            config->anyWow = 1;
            config->wowExe[0] = '\0';
        }
        else if (lstrcmpiA(argv[i], "--wow-exe") == 0)
        {
            if (!ReadPathArgument(argc, argv, &i, config->wowExe, sizeof(config->wowExe)))
                return 0;
            config->anyWow = 0;
        }
        else if (lstrcmpiA(argv[i], "--pid") == 0)
        {
            if (!ReadNumberArgument(argc, argv, &i, &config->targetPid))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--dll") == 0)
        {
            if (!ReadPathArgument(argc, argv, &i, config->dllPath, sizeof(config->dllPath)))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--target-dll-name") == 0)
        {
            if (!ReadPathArgument(argc, argv, &i, config->targetDllName, sizeof(config->targetDllName)))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--state") == 0)
        {
            if (!ReadPathArgument(argc, argv, &i, config->statePath, sizeof(config->statePath)))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--log") == 0)
        {
            if (!ReadPathArgument(argc, argv, &i, config->logPath, sizeof(config->logPath)))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--wait-ms") == 0)
        {
            if (!ReadNumberArgument(argc, argv, &i, &config->waitMs))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--poll-ms") == 0)
        {
            if (!ReadNumberArgument(argc, argv, &i, &config->pollMs))
                return 0;
        }
        else if (lstrcmpiA(argv[i], "--settle-ms") == 0)
        {
            if (!ReadNumberArgument(argc, argv, &i, &config->settleMs))
                return 0;
        }
        else
        {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return 0;
        }
    }

    if (config->pollMs == 0)
        config->pollMs = 1000;

    if (config->targetDllName[0] == '\0' ||
        strchr(config->targetDllName, '\\') ||
        strchr(config->targetDllName, '/') ||
        strchr(config->targetDllName, ':'))
    {
        fprintf(stderr, "invalid --target-dll-name: %s\n", config->targetDllName);
        return 0;
    }

    if (config->wowExe[0])
        NormalizePath(config->wowExe, config->wowExe, sizeof(config->wowExe));
    else
        config->anyWow = 1;

    if (!config->dllPath[0] || !config->statePath[0] || !config->logPath[0])
    {
        char exeDirectory[MAX_PATH];
        if (!GetExecutableDirectory(exeDirectory, sizeof(exeDirectory)))
            return 0;

        if (!config->dllPath[0] &&
            !BuildPathInDirectory(exeDirectory, DEFAULT_TARGET_DLL_NAME, config->dllPath, sizeof(config->dllPath)))
            return 0;

        if (!config->statePath[0] &&
            !BuildPathInDirectory(exeDirectory, "hermes_auto_injector.state", config->statePath, sizeof(config->statePath)))
            return 0;

        if (!config->logPath[0] &&
            !BuildPathInDirectory(exeDirectory, "hermes_auto_injector.log", config->logPath, sizeof(config->logPath)))
            return 0;
    }

    NormalizePath(config->dllPath, config->dllPath, sizeof(config->dllPath));
    NormalizePath(config->statePath, config->statePath, sizeof(config->statePath));
    NormalizePath(config->logPath, config->logPath, sizeof(config->logPath));

    return 1;
}

static int QueryProcessPath(DWORD pid, char* path, DWORD pathSize)
{
    HANDLE process;
    DWORD size;
    BOOL ok;

    if (!path || pathSize == 0)
        return 0;

    path[0] = '\0';
    process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process)
        return 0;

    size = pathSize;
    ok = QueryFullProcessImageNameA(process, 0, path, &size);
    CloseHandle(process);

    return ok ? 1 : 0;
}

static int FindTargetWowProcesses(const AutoInjectConfig* config, TargetProcess* targets, int maxTargets)
{
    HANDLE snapshot;
    PROCESSENTRY32 entry;
    int count = 0;

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!Process32First(snapshot, &entry))
    {
        CloseHandle(snapshot);
        return 0;
    }

    do
    {
        char candidatePath[MAX_PATH];

        if (lstrcmpiA(entry.szExeFile, "wow.exe") != 0)
            continue;

        if (config->targetPid != 0 && entry.th32ProcessID != config->targetPid)
            continue;

        if (!QueryProcessPath(entry.th32ProcessID, candidatePath, sizeof(candidatePath)))
            continue;

        if (config->anyWow || PathEquals(candidatePath, config->wowExe))
        {
            if (count < maxTargets)
            {
                targets[count].pid = entry.th32ProcessID;
                CopyString(targets[count].path, sizeof(targets[count].path), candidatePath);
                ++count;
            }
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return count;
}

static BOOL CALLBACK FindMainWindowProc(HWND hwnd, LPARAM parameter)
{
    WindowSearch* search = (WindowSearch*)parameter;
    DWORD windowPid = 0;
    RECT rect;

    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != search->pid)
        return TRUE;

    if (!IsWindowVisible(hwnd))
        return TRUE;

    if (GetWindow(hwnd, GW_OWNER) != NULL)
        return TRUE;

    if (!GetWindowRect(hwnd, &rect))
        return TRUE;

    if (rect.right <= rect.left || rect.bottom <= rect.top)
        return TRUE;

    search->hwnd = hwnd;
    return FALSE;
}

static HWND FindMainWindow(DWORD pid)
{
    WindowSearch search;

    ZeroMemory(&search, sizeof(search));
    search.pid = pid;
    EnumWindows(FindMainWindowProc, (LPARAM)&search);
    return search.hwnd;
}

static int IsProcessAlive(DWORD pid)
{
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    DWORD waitResult;

    if (!process)
        return 0;

    waitResult = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return waitResult == WAIT_TIMEOUT;
}

static int IsBridgeModuleLoaded(DWORD pid, const char* dllPath, char* loadedPath, DWORD loadedPathSize)
{
    HANDLE snapshot;
    MODULEENTRY32 module;

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    ZeroMemory(&module, sizeof(module));
    module.dwSize = sizeof(module);
    if (!Module32First(snapshot, &module))
    {
        CloseHandle(snapshot);
        return 0;
    }

    do
    {
        if (PathEquals(module.szExePath, dllPath) ||
            ContainsNoCase(module.szModule, "HermesBridge") ||
            ContainsNoCase(module.szModule, "hermes_bridge"))
        {
            CopyString(loadedPath, loadedPathSize, module.szExePath);
            CloseHandle(snapshot);
            return 1;
        }
    } while (Module32Next(snapshot, &module));

    CloseHandle(snapshot);
    return 0;
}

static int GetParentDirectory(const char* path, char* directory, DWORD directorySize)
{
    DWORD i;
    DWORD lastSlash = 0;

    if (!path || !directory || directorySize == 0)
        return 0;

    directory[0] = '\0';
    for (i = 0; path[i]; ++i)
    {
        if (path[i] == '\\' || path[i] == '/')
            lastSlash = i;
    }

    if (lastSlash == 0 || lastSlash >= directorySize)
        return 0;

    CopyMemory(directory, path, lastSlash);
    directory[lastSlash] = '\0';
    return 1;
}

static int BuildTargetDllPath(const AutoInjectConfig* config, const char* wowPath, char* targetDllPath, DWORD targetDllPathSize)
{
    char wowDirectory[MAX_PATH];
    int written;

    if (!GetParentDirectory(wowPath, wowDirectory, sizeof(wowDirectory)))
        return 0;

    written = _snprintf_s(targetDllPath, targetDllPathSize, _TRUNCATE, "%s\\%s", wowDirectory, config->targetDllName);
    return written > 0 && (DWORD)written < targetDllPathSize;
}

static int DeployBridgeDll(const AutoInjectConfig* config, const char* targetDllPath)
{
    char errorText[512];
    DWORD error;
    DWORD sourceAttributes;
    DWORD targetAttributes;

    if (PathEquals(config->dllPath, targetDllPath))
        return 1;

    sourceAttributes = GetFileAttributesA(config->dllPath);
    targetAttributes = GetFileAttributesA(targetDllPath);
    if (sourceAttributes == INVALID_FILE_ATTRIBUTES)
    {
        if (targetAttributes != INVALID_FILE_ATTRIBUTES)
        {
            LogMessage(config, "source dll missing, using bundled target dll target=%s source=%s", targetDllPath, config->dllPath);
            return 1;
        }

        LogMessage(config, "source dll missing and target dll not found source=%s target=%s", config->dllPath, targetDllPath);
        return 0;
    }

    if (CopyFileA(config->dllPath, targetDllPath, FALSE))
    {
        LogMessage(config, "deployed bridge dll source=%s target=%s", config->dllPath, targetDllPath);
        return 1;
    }

    error = GetLastError();
    FormatLastErrorMessage(error, errorText, sizeof(errorText));
    if (targetAttributes != INVALID_FILE_ATTRIBUTES &&
        (error == ERROR_SHARING_VIOLATION || error == ERROR_ACCESS_DENIED))
    {
        LogMessage(config, "deploy skipped, target exists but is locked target=%s %s", targetDllPath, errorText);
        return 1;
    }

    LogMessage(config, "deploy failed source=%s target=%s %s", config->dllPath, targetDllPath, errorText);
    return 0;
}

static int StateMatchesCurrentProcess(const AutoInjectConfig* config, DWORD pid, const char* wowPath, const char* targetDllPath)
{
    FILE* file = NULL;
    char line[1024];
    DWORD statePid = 0;
    char stateWow[MAX_PATH] = "";
    char stateDll[MAX_PATH] = "";

    fopen_s(&file, config->statePath, "r");
    if (!file)
        return 0;

    while (fgets(line, sizeof(line), file))
    {
        char* newline = strpbrk(line, "\r\n");
        if (newline)
            *newline = '\0';

        if (strncmp(line, "pid=", 4) == 0)
            statePid = (DWORD)strtoul(line + 4, NULL, 10);
        else if (strncmp(line, "wowExe=", 7) == 0)
            CopyString(stateWow, sizeof(stateWow), line + 7);
        else if (strncmp(line, "dllPath=", 8) == 0)
            CopyString(stateDll, sizeof(stateDll), line + 8);
    }

    fclose(file);

    if (statePid != pid)
        return 0;

    if (stateWow[0] && !PathEquals(stateWow, wowPath))
        return 0;

    if (stateDll[0] && !PathEquals(stateDll, targetDllPath))
        return 0;

    return 1;
}

static void WriteState(const AutoInjectConfig* config, DWORD pid, const char* wowPath, const char* targetDllPath, const char* status)
{
    FILE* file = NULL;
    time_t now;
    struct tm utcTime;
    char stamp[64];

    fopen_s(&file, config->statePath, "w");
    if (!file)
    {
        LogMessage(config, "state write skipped path=%s", config->statePath);
        return;
    }

    time(&now);
    gmtime_s(&utcTime, &now);
    strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utcTime);

    fprintf(file, "pid=%lu\n", (unsigned long)pid);
    fprintf(file, "status=%s\n", status ? status : "unknown");
    fprintf(file, "wowExe=%s\n", wowPath ? wowPath : "");
    fprintf(file, "dllPath=%s\n", targetDllPath ? targetDllPath : "");
    fprintf(file, "sourceDllPath=%s\n", config->dllPath);
    fprintf(file, "updatedAt=%s\n", stamp);
    fclose(file);
}

static int InjectDll(const AutoInjectConfig* config, DWORD pid, const char* dllPath, DWORD* remoteResult)
{
    HANDLE process;
    SIZE_T pathLen;
    LPVOID remotePath;
    HMODULE kernel32;
    FARPROC loadLibrary;
    HANDLE thread;
    DWORD waitResult;
    char errorText[512];

    *remoteResult = 0;
    process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        pid);
    if (!process)
    {
        FormatLastErrorMessage(GetLastError(), errorText, sizeof(errorText));
        LogMessage(config, "OpenProcess failed pid=%lu %s", (unsigned long)pid, errorText);
        return 0;
    }

    pathLen = lstrlenA(dllPath) + 1;
    remotePath = VirtualAllocEx(process, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath)
    {
        FormatLastErrorMessage(GetLastError(), errorText, sizeof(errorText));
        LogMessage(config, "VirtualAllocEx failed pid=%lu %s", (unsigned long)pid, errorText);
        CloseHandle(process);
        return 0;
    }

    if (!WriteProcessMemory(process, remotePath, dllPath, pathLen, NULL))
    {
        FormatLastErrorMessage(GetLastError(), errorText, sizeof(errorText));
        LogMessage(config, "WriteProcessMemory failed pid=%lu %s", (unsigned long)pid, errorText);
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    kernel32 = GetModuleHandleA("kernel32.dll");
    loadLibrary = kernel32 ? GetProcAddress(kernel32, "LoadLibraryA") : NULL;
    if (!loadLibrary)
    {
        FormatLastErrorMessage(GetLastError(), errorText, sizeof(errorText));
        LogMessage(config, "GetProcAddress LoadLibraryA failed %s", errorText);
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibrary, remotePath, 0, NULL);
    if (!thread)
    {
        FormatLastErrorMessage(GetLastError(), errorText, sizeof(errorText));
        LogMessage(config, "CreateRemoteThread failed pid=%lu %s", (unsigned long)pid, errorText);
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    waitResult = WaitForSingleObject(thread, 15000);
    if (waitResult != WAIT_OBJECT_0)
    {
        LogMessage(config, "LoadLibraryA remote thread timed out pid=%lu", (unsigned long)pid);
        CloseHandle(thread);
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    GetExitCodeThread(thread, remoteResult);
    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);

    return *remoteResult != 0;
}

static int HandleProcess(const AutoInjectConfig* config, DWORD pid, const char* wowPath)
{
    char targetDllPath[MAX_PATH] = "";
    char loadedPath[MAX_PATH] = "";
    DWORD remoteResult = 0;

    if (!BuildTargetDllPath(config, wowPath, targetDllPath, sizeof(targetDllPath)))
    {
        LogMessage(config, "could not build target dll path wowExe=%s targetName=%s", wowPath, config->targetDllName);
        return 0;
    }

    if (StateMatchesCurrentProcess(config, pid, wowPath, targetDllPath))
    {
        LogMessage(config, "already loaded by state pid=%lu", (unsigned long)pid);
        return 1;
    }

    if (IsBridgeModuleLoaded(pid, targetDllPath, loadedPath, sizeof(loadedPath)))
    {
        LogMessage(config, "already loaded pid=%lu module=%s", (unsigned long)pid, loadedPath);
        WriteState(config, pid, wowPath, targetDllPath, "already_loaded");
        return 1;
    }

    if (!DeployBridgeDll(config, targetDllPath))
        return 0;

    LogMessage(config, "injecting pid=%lu dll=%s", (unsigned long)pid, targetDllPath);
    if (!InjectDll(config, pid, targetDllPath, &remoteResult))
        return 0;

    LogMessage(config, "inject succeeded pid=%lu remote=0x%08lX", (unsigned long)pid, (unsigned long)remoteResult);
    WriteState(config, pid, wowPath, targetDllPath, "injected");
    return 1;
}

static int HandleReadyTarget(const AutoInjectConfig* config, DWORD pid, const char* wowPath)
{
    HWND hwnd = FindMainWindow(pid);

    if (!hwnd)
        return 0;

    LogMessage(config, "wow.exe detected pid=%lu hwnd=0x%08lX path=%s",
        (unsigned long)pid,
        (unsigned long)(uintptr_t)hwnd,
        wowPath);

    if (config->settleMs > 0)
        Sleep(config->settleMs);

    if (!IsProcessAlive(pid))
    {
        LogMessage(config, "wow.exe exited during settle pid=%lu", (unsigned long)pid);
        return 0;
    }

    return HandleProcess(config, pid, wowPath);
}

static ScanStats ScanAndHandleTargets(const AutoInjectConfig* config)
{
    TargetProcess targets[64];
    ScanStats stats;
    int count;
    int i;

    ZeroMemory(&stats, sizeof(stats));
    count = FindTargetWowProcesses(config, targets, (int)(sizeof(targets) / sizeof(targets[0])));
    stats.found = count;

    for (i = 0; i < count; ++i)
    {
        char targetDllPath[MAX_PATH] = "";
        char loadedPath[MAX_PATH] = "";

        if (BuildTargetDllPath(config, targets[i].path, targetDllPath, sizeof(targetDllPath)) &&
            IsBridgeModuleLoaded(targets[i].pid, targetDllPath, loadedPath, sizeof(loadedPath)))
        {
            ++stats.alreadyLoaded;
            continue;
        }

        if (HandleReadyTarget(config, targets[i].pid, targets[i].path))
            ++stats.handled;
        else
            ++stats.notReadyOrFailed;
    }

    return stats;
}

static int WaitForAnyHandledTarget(const AutoInjectConfig* config)
{
    DWORD start = GetTickCount();
    DWORD lastLog = 0;

    for (;;)
    {
        ScanStats stats = ScanAndHandleTargets(config);
        if (stats.handled > 0 || stats.alreadyLoaded > 0)
            return stats.handled + stats.alreadyLoaded;

        if (config->once && config->targetPid != 0 && !IsProcessAlive(config->targetPid))
        {
            LogMessage(config, "target wow.exe exited before injection pid=%lu", (unsigned long)config->targetPid);
            return 0;
        }

        if (GetTickCount() - lastLog > 5000)
        {
            if (config->anyWow)
                LogMessage(config, "waiting for any wow.exe");
            else
                LogMessage(config, "waiting for trusted wow.exe path=%s", config->wowExe);
            lastLog = GetTickCount();
        }

        if (config->once && config->waitMs > 0 && GetTickCount() - start >= config->waitMs)
        {
            if (config->anyWow)
                LogMessage(config, "timeout waiting for any wow.exe");
            else
                LogMessage(config, "timeout waiting for wow.exe path=%s", config->wowExe);
            return 0;
        }

        Sleep(config->pollMs);
    }
}

int main(int argc, char** argv)
{
    AutoInjectConfig config;
    DWORD lastLog = 0;

    if (!ParseArguments(argc, argv, &config))
    {
        PrintUsage();
        return 1;
    }

    LogMessage(&config, "Hermes Auto Injector start mode=%s wow=%s sourceDll=%s targetDllName=%s",
        config.once ? "once" : "monitor",
        config.anyWow ? "<any>" : config.wowExe,
        config.dllPath,
        config.targetDllName);

    if (GetFileAttributesA(config.dllPath) == INVALID_FILE_ATTRIBUTES)
    {
        LogMessage(&config, "HermesBridge source DLL missing path=%s; bundled target DLLs will still be used when present", config.dllPath);
    }

    if (config.once)
    {
        if (!WaitForAnyHandledTarget(&config))
            return 3;
        return 0;
    }

    for (;;)
    {
        ScanStats stats = ScanAndHandleTargets(&config);
        if (stats.handled > 0)
        {
            LogMessage(&config, "monitor scan found=%d injected=%d alreadyLoaded=%d pending=%d",
                stats.found,
                stats.handled,
                stats.alreadyLoaded,
                stats.notReadyOrFailed);
            lastLog = GetTickCount();
        }
        else if (GetTickCount() - lastLog > 5000)
        {
            if (stats.found > 0)
            {
                LogMessage(&config, "monitor idle found=%d alreadyLoaded=%d pending=%d",
                    stats.found,
                    stats.alreadyLoaded,
                    stats.notReadyOrFailed);
            }
            else if (config.anyWow)
                LogMessage(&config, "monitor waiting for any wow.exe");
            else
                LogMessage(&config, "monitor waiting for wow.exe path=%s", config.wowExe);
            lastLog = GetTickCount();
        }

        Sleep(config.pollMs);
    }
}

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hermes_bridge_client_pump_gate.h"
#include "hermes_bridge_latency_policy.h"
#include "hermes_bridge_recv_queue_policy.h"
#include "hermes_bridge_send_queue_policy.h"

#define HERMES_BRIDGE_VERSION "0.7.84-recvpump-watchdog"
#define HERMES_STRINGIFY_VALUE(x) #x
#define HERMES_STRINGIFY(x) HERMES_STRINGIFY_VALUE(x)
#define HERMES_FRAME_SCRIPT_EXECUTE_RVA 0x00419210
#define HERMES_PACKET_CREATE_ADDR 0x00467540
#define HERMES_UPPER_SEND_ADDR 0x004675F0
#define HERMES_AFTER_OPCODE_ADDR 0x00632001
#define HERMES_SEND_ADDON_MESSAGE_ADDR 0x00500560
#define HERMES_LUA_CHECK_STRING_ADDR 0x0084E0E0
#define HERMES_LUA_SET_TOP_ADDR 0x0084DBF0
#define HERMES_LUA_PCALL_ADDR 0x0084EC50
#define HERMES_LUA_LOAD_BUFFER_ADDR 0x0084F860
#define HERMES_EXPECTED_WOW_SHA256 "a0a9d8bdcb7915dcd6c1f6b9dd37c2c28e2db32cc337f4a33946c3af0711cd89"
#define HERMES_CMSG_OPCODE 0x0990
#define HERMES_SMSG_OPCODE 0x0991
#define HERMES_FRAME_MAGIC 0x4842
#define HERMES_FRAME_VERSION 2
#define HERMES_FRAME_HEADER_SIZE 24
#define HERMES_LANE_CONTROL 0
#define HERMES_LANE_RPC 1
#define HERMES_LANE_EVENT 2
#define HERMES_LANE_SNAPSHOT 3
#define HERMES_LANE_BULK 4
#define HERMES_MESSAGE_REQUEST 1
#define HERMES_MESSAGE_RESPONSE 2
#define HERMES_MESSAGE_EVENT 3
#define HERMES_MESSAGE_ERROR 4
#define HERMES_MESSAGE_CHUNK 6
#define HERMES_CODEC_JSON 1
#define HERMES_CODEC_BINARY 3
#define HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1 1000
#define HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1 1001
#define HERMES_SCHEMA_BULK_CHUNK_V1 1002
#define HERMES_METHOD_HELLO 1
#define HERMES_METHOD_PING 2
#define HERMES_METHOD_DEBUG_EMIT 3
#define HERMES_METHOD_GET_SCHEMA_REGISTRY 4
#define HERMES_METHOD_GET_METHOD_REGISTRY 5
#define HERMES_METHOD_DEBUG_BINARY_EMIT 6
#define HERMES_METHOD_DEBUG_VITALS_BINARY_EMIT 7
#define HERMES_METHOD_DEBUG_BULK_CHUNK_EMIT 8
#define HERMES_METHOD_SERVER_GET_STATUS 10
#define HERMES_METHOD_ADDON_DISPATCH 20
#define HERMES_METHOD_ADDON_MESSAGE 21
#define HERMES_METHOD_PLAYER_GET_BASIC_INFO 100
#define HERMES_METHOD_PLAYER_GET_POSITION 101
#define HERMES_METHOD_PLAYER_GET_VITALS 102
#define HERMES_METHOD_PLAYER_GET_SNAPSHOT 103
#define HERMES_METHOD_SERVER_GET_NUMERIC_LIMITS 104
#define HERMES_METHOD_PLAYER_GET_ATTRIBUTES 105
#define HERMES_METHOD_PLAYER_GET_TARGET_SNAPSHOT 106
#define HERMES_METHOD_PLAYER_EMIT_DAMAGE_EVENT 107
#define HERMES_METHOD_PLAYER_EMIT_VITALS_BINARY 108
#define HERMES_METHOD_PLAYER_EMIT_SNAPSHOT_BULK 109
#define HERMES_METHOD_UI_GET_DASHBOARD 200
#define HERMES_METHOD_UI_GET_MODULE_STATUS 201
#define HERMES_METHOD_ABYSS_GET_EQUIPMENT_PAGE 300
#define HERMES_METHOD_ABYSS_GET_SET_OVERVIEW 301
#define HERMES_METHOD_ABYSS_GET_SET_BONUSES 302
#define HERMES_METHOD_ABYSS_GET_RELICS 303
#define HERMES_METHOD_CULTIVATION_GET_ALL 320
#define HERMES_METHOD_CULTIVATION_GET_STATE 321
#define HERMES_METHOD_BOUNDARY_GET_ALL 340
#define HERMES_METHOD_BOUNDARY_GET_DETAIL 341
#define HERMES_METHOD_BREAKTHROUGH_GET_ALL 360
#define HERMES_METHOD_BREAKTHROUGH_GET_SYSTEM_DATA 361
#define HERMES_METHOD_BREAKTHROUGH_GET_INFO 362
#define HERMES_METHOD_BREAKTHROUGH_GET_SKILLS 363
#define HERMES_METHOD_BREAKTHROUGH_GET_SKILL_DETAIL 364
#define HERMES_METHOD_BREAKTHROUGH_GET_LEADERBOARD 365
#define HERMES_METHOD_BREAKTHROUGH_GET_EXP_SOURCES 366
#define HERMES_METHOD_BREAKTHROUGH_UPGRADE 367
#define HERMES_METHOD_BREAKTHROUGH_LEARN_SKILL 368
#define HERMES_METHOD_BREAKTHROUGH_UPGRADE_SKILL 369
#define HERMES_METHOD_BREAKTHROUGH_RESET_SKILLS 370
#define HERMES_METHOD_SYNTHESIS_LIST 380
#define HERMES_METHOD_SYNTHESIS_DO 381
#define HERMES_METHOD_TOOLTIP_QUERY 500
#define HERMES_METHOD_TOOLTIP_QUERY_TEMPLATE 501
#define HERMES_METHOD_TOOLTIP_INSPECT_ITEM_GUID 502
#define HERMES_METHOD_TOOLTIP_LIST_PENDING 503
#define HERMES_METHOD_MALL_GET_CATEGORIES 520
#define HERMES_METHOD_MALL_GET_ITEMS 521
#define HERMES_METHOD_MALL_PURCHASE 522
#define HERMES_SEND_QUEUE_CAPACITY 256
#define HERMES_SEND_QUEUE_PAYLOAD_SIZE 512
#define HERMES_SEND_QUEUE_DIAGNOSTIC_RESERVE 64
#define HERMES_SEND_QUEUE_SELF_WAIT_ATTEMPTS 240
#define HERMES_SEND_QUEUE_SELF_WAIT_MS 250
#define HERMES_LUA_MAX_REQUEST_PAYLOAD_SIZE 384
#define HERMES_FRAME_MAX_PAYLOAD_SIZE 65536
#define HERMES_RECV_QUEUE_CAPACITY 8192
#define HERMES_RECV_QUEUE_PAYLOAD_SIZE 65536
#define HERMES_RECV_COALESCE_SCAN_LIMIT 512
#define HERMES_RECV_LUA_ESCAPED_SIZE 131072
#define HERMES_RECV_LUA_SCRIPT_SIZE 196608
/* Keep the generated FrameScript source below WoW 3.3.5's unreliable long-line range. */
#define HERMES_RECV_LUA_CHUNK_THRESHOLD 384
#define HERMES_RECV_LUA_CHUNK_RAW_SIZE 384
#define HERMES_RECV_PUMP_TIMER_ID 0x4842
#define HERMES_AUTO_SNAPSHOT_TIMER_ID 0x4844
#define HERMES_BOOTSTRAP_TIMER_ID 0x4845
#define HERMES_RECV_PUMP_INTERVAL_MS 5
#define HERMES_RECV_PUMP_MAX_PER_TICK 128
#define HERMES_RECV_PUMP_BACKLOG_PER_TICK 512
#define HERMES_RECV_QUEUE_PRIORITY_THRESHOLD 64
#define HERMES_RECV_QUEUE_PRIORITY_SCAN_LIMIT 512
#define HERMES_LOG_ROTATE_BYTES (4u * 1024u * 1024u)

#ifndef HERMES_ENABLE_STARTUP_SELF_TEST
#define HERMES_ENABLE_STARTUP_SELF_TEST 0
#endif

#ifndef HERMES_ENABLE_PLAYER_READY_EVENT
#define HERMES_ENABLE_PLAYER_READY_EVENT 1
#endif

#ifndef HERMES_ENABLE_RECV_PUMP_TIMER
#define HERMES_ENABLE_RECV_PUMP_TIMER 1
#endif

#ifndef HERMES_RECV_PUMP_TIMER_DELAY_MS
#define HERMES_RECV_PUMP_TIMER_DELAY_MS 250
#endif

#ifndef HERMES_RECV_PUMP_TIMER_BACKLOG_DELAY_MS
#define HERMES_RECV_PUMP_TIMER_BACKLOG_DELAY_MS 50
#endif

/* recv pump timer 绑定在 WoW 主窗口上。玩家调整分辨率/全屏切换时 WoW 可能销毁并
   重建主窗口，旧 HWND 失效后挂在其上的 WM_TIMER 永不再派发，导致服务器回包泵不进
   Lua、Lua API 也不再被重装，表现为静默失联。watchdog 线程按此间隔巡检并自愈。 */
#ifndef HERMES_RECV_PUMP_WATCHDOG_INTERVAL_MS
#define HERMES_RECV_PUMP_WATCHDOG_INTERVAL_MS 1000
#endif

#ifndef HERMES_AUTO_SNAPSHOT_TIMER_DELAY_MS
#define HERMES_AUTO_SNAPSHOT_TIMER_DELAY_MS 1500
#endif

#ifndef HERMES_AUTO_SNAPSHOT_RETRY_MS
#define HERMES_AUTO_SNAPSHOT_RETRY_MS 3000
#endif

#ifndef HERMES_BOOTSTRAP_TIMER_DELAY_MS
#define HERMES_BOOTSTRAP_TIMER_DELAY_MS 500
#endif

#ifndef HERMES_BOOTSTRAP_STALE_TIMER_DELAY_MS
#define HERMES_BOOTSTRAP_STALE_TIMER_DELAY_MS 250
#endif

#ifndef HERMES_LUA_API_CHAIN_DELAY_MS
#define HERMES_LUA_API_CHAIN_DELAY_MS 50
#endif

#ifndef HERMES_WORLD_BOOTSTRAP_RETRY_DELAY_MS
#define HERMES_WORLD_BOOTSTRAP_RETRY_DELAY_MS 100
#endif

#ifndef HERMES_WORLD_LUA_READY_BOOTSTRAP_DELAY_MS
#define HERMES_WORLD_LUA_READY_BOOTSTRAP_DELAY_MS 250
#endif

#ifndef HERMES_AUTO_HANDSHAKE_ATTEMPTS
#define HERMES_AUTO_HANDSHAKE_ATTEMPTS 120
#endif

#ifndef HERMES_AUTO_HANDSHAKE_RETRY_MS
#define HERMES_AUTO_HANDSHAKE_RETRY_MS 500
#endif

#ifndef HERMES_LUA_WARMUP_DELAY_SECONDS
#define HERMES_LUA_WARMUP_DELAY_SECONDS "0.05"
#endif

#ifndef HERMES_BOOTSTRAP_MAX_RETRIES
#define HERMES_BOOTSTRAP_MAX_RETRIES 120
#endif

#ifndef HERMES_LUA_API_ENSURE_MIN_INTERVAL_MS
#define HERMES_LUA_API_ENSURE_MIN_INTERVAL_MS 2000
#endif

typedef int (__cdecl* FrameScriptExecuteFn)(const char* script, const char* source, int unknown);
typedef void* (__thiscall* PacketCreateFn)(void* self, const void* payload, uint32_t payloadLen, uint32_t rawMode);
typedef uint32_t (__thiscall* UpperSendFn)(void* self, void* packet, uint32_t mode);
typedef int (__cdecl* LuaCFunctionFn)(void* luaState);
typedef const char* (__cdecl* LuaCheckStringFn)(void* luaState, int index, int unused, int* typeOut);
typedef void (__cdecl* LuaSetTopFn)(void* luaState, int index);
typedef int (__cdecl* LuaPCallFn)(void* luaState, int nargs, int nresults, int errfunc);
typedef int (__cdecl* LuaLoadBufferFn)(void* luaState, const char* buffer, size_t size, const char* name);

static HMODULE g_wowModule = NULL;
static uintptr_t g_wowBase = 0;
static uintptr_t g_frameScriptExecute = 0;
static BYTE g_upperSendOriginal[6];
static BYTE* g_upperSendTrampoline = NULL;
static BYTE g_recvOriginal[5];
static BYTE* g_recvTrampoline = NULL;
static BYTE g_sendAddonOriginal[9];
static BYTE* g_sendAddonTrampoline = NULL;
static volatile LONG g_upperSendHitCount = 0;
static volatile LONG g_upperSendLogBudget = 12;
static volatile LONG g_recvHitCount = 0;
static volatile LONG g_sendAddonHitCount = 0;
static volatile LONG g_sendAddonLogBudget = 4;
static CRITICAL_SECTION g_sendQueueLock;
static BOOL g_sendQueueLockInitialized = FALSE;
static char g_sendQueue[HERMES_SEND_QUEUE_CAPACITY][HERMES_SEND_QUEUE_PAYLOAD_SIZE];
static LONG g_sendQueueHead = 0;
static LONG g_sendQueueTail = 0;
static LONG g_sendQueueCount = 0;
static volatile LONG g_sendQueueWorkerRunning = 0;
static volatile LONG g_sendQueueDropped = 0;
typedef struct HermesRecvQueueItem
{
    WORD lane;
    BYTE messageType;
    BYTE codec;
    WORD schemaId;
    WORD methodId;
    DWORD requestId;
    DWORD sequence;
    DWORD payloadSize;
    char* payload;
} HermesRecvQueueItem;
static CRITICAL_SECTION g_recvQueueLock;
static BOOL g_recvQueueLockInitialized = FALSE;
static HermesRecvQueueItem g_recvQueue[HERMES_RECV_QUEUE_CAPACITY];
static LONG g_recvQueueHead = 0;
static LONG g_recvQueueTail = 0;
static LONG g_recvQueueCount = 0;
static volatile LONG g_recvQueueWorkerRunning = 0;
static volatile LONG g_recvQueueDropped = 0;
static volatile LONG g_recvQueueCoalesced = 0;
static volatile LONG g_recvQueueDropLogBudget = 16;
static volatile LONG g_recvQueueCoalesceLogBudget = 8;
static volatile LONG g_recvFrameLogBudget = 0;
static volatile LONG g_recvDispatching = 0;
static volatile LONG g_recvPumpLogBudget = 16;
static volatile LONG g_recvChunkLogBudget = 8;
static volatile LONG g_recvAddonLogBudget = 80;
static volatile LONG g_recvLuaDispatchLogBudget = 48;
static volatile LONG g_reloadTraceLogBudget = 120;
static volatile LONG g_recvPumpDeferLogBudget = 24;
static volatile LONG g_recvBinaryPayloadLogBudget = 4;
static volatile LONG g_recvPriorityPopLogBudget = 8;
static volatile LONG g_nativeSendLogBudget = 8;
static volatile LONG g_luaExecuteLogBudget = 0;
static volatile LONG g_luaStateExceptionLogBudget = 8;
static volatile LONG g_luaFallbackLogBudget = 8;
static volatile LONG g_luaCaptureLogBudget = 12;
static volatile LONG g_bootstrapWaitingLogBudget = 8;
static volatile LONG g_recvPumped = 0;
static HWND g_recvPumpWindow = NULL;
static UINT_PTR g_recvPumpTimer = 0;
static CRITICAL_SECTION g_recvPumpTimerLock;
static BOOL g_recvPumpTimerLockInitialized = FALSE;
static volatile LONG g_recvPumpWatchdogStarted = 0;
static volatile LONG g_recvPumpWatchdogStop = 0;
static volatile LONG g_recvPumpTimerReinstalls = 0;
static HWND g_autoSnapshotWindow = NULL;
static UINT_PTR g_autoSnapshotTimer = 0;
static HWND g_bootstrapWindow = NULL;
static UINT_PTR g_bootstrapTimer = 0;
static volatile LONG g_bootstrapInstalling = 0;
static volatile LONG g_bootstrapRetryAttempts = 0;
static volatile LONG g_autoSnapshotAttempts = 0;
static volatile LONG g_nativeSelfReady = 0;
static volatile LONG g_playerReadySent = 0;
static volatile LONG g_autoHandshakeThreadStarted = 0;
static volatile LONG g_worldBootstrapThreadStarted = 0;
static volatile LONG g_connectionGeneration = 0;
static volatile LONG g_luaWorldResetGeneration = 0;
static volatile LONG g_worldLuaReady = 0;
static volatile LONG g_luaApiEnsureRunning = 0;
static volatile LONG g_luaApiEnsureAttempts = 0;
static volatile LONG g_luaApiEnsureSuccesses = 0;
static void* g_realLuaState = NULL;
static DWORD g_luaApiEnsureLastTick = 0;
static volatile LONG g_nextRequestId = 1000;
static volatile LONG g_nextSequence = 0;
static void* g_connectionSelf = NULL;
static BOOL g_upperSendHookInstalled = FALSE;
static BOOL g_recvHookInstalled = FALSE;
static BOOL g_sendAddonHookInstalled = FALSE;
static BOOL g_nativeSelfTestSent = FALSE;
static char g_logPath[MAX_PATH] = "HermesBridge.log";
static volatile LONG g_logRotateLock = 0;
static volatile LONG g_logWriteLock = 0;

#define HERMES_LUA_STATE_EXCEPTION_RESULT (-10001)

static BOOL SendHermesPayload(const char* text);
static BOOL InstallLuaApi(void);
static BOOL InstallLuaStateApi(void);
static BOOL InstallLuaStateAccessors(void);
static BOOL InstallLuaStateHelpers(void);
static BOOL InstallLuaLifecycleApi(void);
static BOOL InstallLuaDebugPanelApi(void);
static BOOL EnsureLuaApiInstalled(const char* reason);
static BOOL EnsureLuaApiInstalledNow(const char* reason);
static BOOL InstallBootstrapTimer(DWORD delayMs);
static BOOL ScheduleBootstrapRetry(const char* reason);
static BOOL CaptureRealLuaState(void* luaState, const char* source);
static DWORD WINAPI DelayedRecvPumpTimerThread(LPVOID parameter);
static DWORD WINAPI DelayedAutoSnapshotTimerThread(LPVOID parameter);
static DWORD WINAPI RecvPumpWatchdogThread(LPVOID parameter);

static void InitializeLogPath(HINSTANCE instance)
{
    char modulePath[MAX_PATH];
    DWORD length = GetModuleFileNameA(instance, modulePath, sizeof(modulePath));
    if (length == 0 || length >= sizeof(modulePath))
        return;

    for (DWORD i = length; i > 0; --i)
    {
        if (modulePath[i - 1] == '\\' || modulePath[i - 1] == '/')
        {
            modulePath[i] = '\0';
            lstrcpynA(g_logPath, modulePath, sizeof(g_logPath));
            {
                int used = lstrlenA(g_logPath);
                if (used < (int)sizeof(g_logPath) - 1)
                    lstrcpynA(g_logPath + used, "HermesBridge.log", sizeof(g_logPath) - used);
            }
            return;
        }
    }
}

static void WriteLog(const char* message)
{
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    while (InterlockedCompareExchange(&g_logWriteLock, 1, 0) != 0)
        Sleep(0);

    if (GetFileAttributesExA(g_logPath, GetFileExInfoStandard, &fileData))
    {
        if (fileData.nFileSizeHigh > 0 || fileData.nFileSizeLow >= HERMES_LOG_ROTATE_BYTES)
        {
            if (InterlockedCompareExchange(&g_logRotateLock, 1, 0) == 0)
            {
                char backupPath[MAX_PATH];
                int used = lstrlenA(g_logPath);
                if (used > 0 && used < (int)sizeof(backupPath) - 2)
                {
                    lstrcpynA(backupPath, g_logPath, sizeof(backupPath));
                    lstrcpynA(backupPath + used, ".1", sizeof(backupPath) - used);
                    DeleteFileA(backupPath);
                    MoveFileExA(g_logPath, backupPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
                }
                InterlockedExchange(&g_logRotateLock, 0);
            }
        }
    }

    HANDLE file = CreateFileA(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(file, message, lstrlenA(message), &written, NULL);
        WriteFile(file, "\r\n", 2, &written, NULL);
        CloseHandle(file);
    }
    InterlockedExchange(&g_logWriteLock, 0);
}

static void WriteLogFormat(const char* format, DWORD a)
{
    char buffer[256];
    wsprintfA(buffer, format, a);
    WriteLog(buffer);
}

static void WriteLogFormat2(const char* format, DWORD a, DWORD b)
{
    char buffer[256];
    wsprintfA(buffer, format, a, b);
    WriteLog(buffer);
}

static void WriteLogFormat4(const char* format, DWORD a, DWORD b, DWORD c, DWORD d)
{
    char buffer[512];
    wsprintfA(buffer, format, a, b, c, d);
    WriteLog(buffer);
}

static void CopyTracePreview(const char* input, char* output, DWORD outputSize)
{
    DWORD pos = 0;

    if (!output || outputSize == 0)
        return;

    output[0] = 0;
    if (!input)
        return;

    while (*input && pos + 1 < outputSize)
    {
        unsigned char ch = (unsigned char)*input++;
        if (ch == '\r' || ch == '\n' || ch == '\t')
            output[pos++] = ' ';
        else if (ch < 32 || ch == 127)
            output[pos++] = '?';
        else
            output[pos++] = (char)ch;
    }

    output[pos] = 0;
}

static void WriteHermesClientTrace(const char* scope, const char* detail)
{
    char buffer[768];

    if (InterlockedDecrement(&g_reloadTraceLogBudget) < 0)
        return;

    _snprintf(buffer, sizeof(buffer), "HermesBridge TRACE client %s %s", scope ? scope : "unknown", detail ? detail : "");
    buffer[sizeof(buffer) - 1] = 0;
    WriteLog(buffer);
}

static BOOL CaptureRealLuaState(void* luaState, const char* source)
{
    void* previousLuaState = NULL;
    BOOL changed = FALSE;

    if (!luaState)
        return FALSE;

    previousLuaState = InterlockedCompareExchangePointer((PVOID volatile*)&g_realLuaState, luaState, NULL);
    if (!previousLuaState)
        changed = TRUE;
    else if (previousLuaState != luaState)
    {
        previousLuaState = InterlockedExchangePointer((PVOID volatile*)&g_realLuaState, luaState);
        changed = TRUE;
    }

    if (!changed)
        return FALSE;

    g_luaApiEnsureLastTick = 0;
    InterlockedExchange(&g_bootstrapRetryAttempts, 0);

    if (InterlockedDecrement(&g_luaCaptureLogBudget) >= 0)
    {
        char trace[256];
        wsprintfA(trace,
            "HermesBridge captured real addon lua_State source=%s old=0x%08lX new=0x%08lX",
            source ? source : "",
            (DWORD)(uintptr_t)previousLuaState,
            (DWORD)(uintptr_t)luaState);
        WriteLog(trace);
    }

    ScheduleBootstrapRetry("HermesBridge real lua_State captured; retrying bootstrap");
    return TRUE;
}

static DWORD SafeReadDword(void* base, DWORD offset)
{
    DWORD value = 0;
    __try { value = *(DWORD*)((BYTE*)base + offset); }
    __except (EXCEPTION_EXECUTE_HANDLER) { value = 0; }
    return value;
}

static WORD SafeReadWordAt(DWORD address)
{
    WORD value = 0;
    __try { value = *(WORD*)address; }
    __except (EXCEPTION_EXECUTE_HANDLER) { value = 0xFFFFu; }
    return value;
}

static BYTE SafeReadByteAt(DWORD address)
{
    BYTE value = 0;
    __try { value = *(BYTE*)address; }
    __except (EXCEPTION_EXECUTE_HANDLER) { value = 0xFFu; }
    return value;
}

static DWORD SafeReadDwordAt(DWORD address)
{
    DWORD value = 0;
    __try { value = *(DWORD*)address; }
    __except (EXCEPTION_EXECUTE_HANDLER) { value = 0xFFFFFFFFu; }
    return value;
}

static void ResolveClientAddresses(void)
{
    g_wowModule = GetModuleHandleA(NULL);
    g_wowBase = (uintptr_t)g_wowModule;
    g_frameScriptExecute = g_wowBase + HERMES_FRAME_SCRIPT_EXECUTE_RVA;

    WriteLogFormat("HermesBridge wow base=0x%08lX", (DWORD)g_wowBase);
    WriteLogFormat("HermesBridge FrameScript::Execute=0x%08lX", (DWORD)g_frameScriptExecute);
    WriteLogFormat("HermesBridge packet factory=0x%08lX", (DWORD)HERMES_PACKET_CREATE_ADDR);
    WriteLogFormat("HermesBridge upper send=0x%08lX", (DWORD)HERMES_UPPER_SEND_ADDR);
}

static int ExecuteLuaInState(void* luaState, const char* script, const char* source)
{
    LuaLoadBufferFn loadBuffer = (LuaLoadBufferFn)HERMES_LUA_LOAD_BUFFER_ADDR;
    LuaPCallFn pcall = (LuaPCallFn)HERMES_LUA_PCALL_ADDR;
    LuaSetTopFn setTop = (LuaSetTopFn)HERMES_LUA_SET_TOP_ADDR;
    int result = -1;

    if (!luaState || !script)
        return -1;

    __try
    {
        result = loadBuffer(luaState, script, (size_t)lstrlenA(script), source ? source : "HermesBridge");
        if (result == 0)
            result = pcall(luaState, 0, 0, 0);
        if (result != 0)
            setTop(luaState, -2);
        return result;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        LONG budget = InterlockedDecrement(&g_luaStateExceptionLogBudget);
        if (budget >= 0)
        {
            char trace[256];
            wsprintfA(trace, "HermesBridge lua_State execute raised exception state=0x%08lX source=%s", (DWORD)(uintptr_t)luaState, source ? source : "");
            WriteLog(trace);
        }
        return HERMES_LUA_STATE_EXCEPTION_RESULT;
    }
}

static int ExecuteLua(const char* script, const char* source)
{
    void* realLuaState = g_realLuaState;
    LONG budget = InterlockedDecrement(&g_luaExecuteLogBudget);
    BOOL worldLuaReady = InterlockedCompareExchange(&g_worldLuaReady, 0, 0) == 1;

    if (budget >= 0)
    {
        char trace[256];
        wsprintfA(trace, "HermesBridge ExecuteLua begin source=%s realLuaState=0x%08lX frameExecute=0x%08lX worldReady=%ld", source ? source : "", (DWORD)(uintptr_t)realLuaState, (DWORD)g_frameScriptExecute, (LONG)worldLuaReady);
        WriteLog(trace);
    }

    if (realLuaState)
    {
        int stateResult = ExecuteLuaInState(realLuaState, script, source);
        if (stateResult != HERMES_LUA_STATE_EXCEPTION_RESULT)
        {
            if (budget >= 0)
            {
                char trace[256];
                wsprintfA(trace, "HermesBridge ExecuteLua state result=%ld source=%s state=0x%08lX", (LONG)stateResult, source ? source : "", (DWORD)(uintptr_t)realLuaState);
                WriteLog(trace);
            }
            return stateResult;
        }

        InterlockedExchangePointer((PVOID volatile*)&g_realLuaState, NULL);
        g_luaApiEnsureLastTick = 0;
        LONG fallbackBudget = InterlockedDecrement(&g_luaFallbackLogBudget);
        if (fallbackBudget >= 0)
        {
            char trace[256];
            wsprintfA(trace, "HermesBridge invalidated stale lua_State=0x%08lX source=%s; waiting for fresh capture", (DWORD)(uintptr_t)realLuaState, source ? source : "");
            WriteLog(trace);
        }

        if (HermesBridge_ShouldScheduleLuaStateRecovery(realLuaState != NULL, stateResult == HERMES_LUA_STATE_EXCEPTION_RESULT))
            ScheduleBootstrapRetry("HermesBridge stale lua_State invalidated; retrying bootstrap");

        return HERMES_LUA_STATE_EXCEPTION_RESULT;
    }

    if (HermesBridge_ShouldUseFrameScriptFallback(FALSE, worldLuaReady))
    {
        FrameScriptExecuteFn execute = (FrameScriptExecuteFn)g_frameScriptExecute;
        int result = -1;
        __try
        {
            result = execute(script, source, 0);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            WriteLog("HermesBridge FrameScript::Execute fallback raised exception");
            result = -1;
        }

        if (budget >= 0)
            WriteLogFormat("HermesBridge ExecuteLua FrameScript fallback returned %lu", (DWORD)result);
        return result;
    }

    if (budget >= 0)
    {
        char trace[256];
        wsprintfA(trace, "HermesBridge ExecuteLua skipped without real lua_State source=%s", source ? source : "");
        WriteLog(trace);
    }

    (void)script;
    return -1;
}

static BOOL IsLuaApiReady(void)
{
    const char* script =
        "local h=_G and _G.HermesDLL; "
        "if type(h)=='table' and type(h.Request)=='function' and type(h._NativeReceive)=='function' and type(h._DispatchAddonCompat)=='function' and type(h._WarmupWorld)=='function' and type(h._ResetForWorld)=='function' then "
        "h._probeRequestReady=true; "
        "h._probeReceiveReady=true; "
        "h._probeCompatReady=true; "
        "return true; "
        "end "
        "error('HermesDLL Lua API is not ready')";
    int result = 0;

    if (!HermesBridge_ShouldAttemptLuaApiInstall(g_realLuaState != NULL) &&
        !HermesBridge_ShouldUseFrameScriptFallback(g_realLuaState != NULL, InterlockedCompareExchange(&g_worldLuaReady, 0, 0) == 1))
        return FALSE;

    __try { result = ExecuteLua(script, "HermesBridgeLuaApiProbe"); }
    __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge Lua API probe raised exception"); result = -1; }

    return result == 0;
}

static BOOL InstallFullLuaApiBundle(const char* reason)
{
    BOOL ok = TRUE;

    WriteLog(reason ? reason : "HermesBridge Lua API reinstall requested");

    ok = InstallLuaApi() && ok;
    ok = InstallLuaStateApi() && ok;
    ok = InstallLuaStateAccessors() && ok;
    ok = InstallLuaStateHelpers() && ok;
    ok = InstallLuaLifecycleApi() && ok;
    ok = InstallLuaDebugPanelApi() && ok;

    if (ok)
    {
        InterlockedIncrement(&g_luaApiEnsureSuccesses);
        WriteLog("HermesBridge Lua API reinstall completed");
    }
    else
        WriteLog("HermesBridge Lua API reinstall incomplete");

    return ok;
}

static BOOL EnsureLuaApiInstalled(const char* reason)
{
    DWORD now = GetTickCount();
    DWORD last = g_luaApiEnsureLastTick;
    LONG traceBudget = InterlockedDecrement(&g_luaFallbackLogBudget);

    if (!HermesBridge_ShouldAttemptLuaApiInstall(g_realLuaState != NULL) &&
        !HermesBridge_ShouldUseFrameScriptFallback(g_realLuaState != NULL, InterlockedCompareExchange(&g_worldLuaReady, 0, 0) == 1))
    {
        if (traceBudget >= 0)
        {
            char trace[256];
            _snprintf(trace, sizeof(trace), "HermesBridge Lua API install deferred: no real lua_State reason=%s", reason ? reason : "");
            trace[sizeof(trace) - 1] = 0;
            WriteLog(trace);
        }
        return FALSE;
    }

    if (last != 0 && now - last < HERMES_LUA_API_ENSURE_MIN_INTERVAL_MS)
        return TRUE;

    if (InterlockedCompareExchange(&g_luaApiEnsureRunning, 1, 0) != 0)
        return TRUE;

    g_luaApiEnsureLastTick = now;

    if (IsLuaApiReady())
    {
        if (traceBudget >= 0)
        {
            char trace[256];
            wsprintfA(trace, "HermesBridge Lua API already ready reason=%s state=0x%08lX", reason ? reason : "", (DWORD)(uintptr_t)g_realLuaState);
            WriteLog(trace);
        }
        InterlockedExchange(&g_luaApiEnsureRunning, 0);
        return TRUE;
    }

    InterlockedIncrement(&g_luaApiEnsureAttempts);
    {
        char trace[256];
        wsprintfA(trace, "HermesBridge ensuring Lua API attempt=%lu reason=%s state=0x%08lX", (DWORD)g_luaApiEnsureAttempts, reason ? reason : "", (DWORD)(uintptr_t)g_realLuaState);
        WriteLog(trace);
    }
    BOOL ok = InstallFullLuaApiBundle(reason);
    InterlockedExchange(&g_luaApiEnsureRunning, 0);
    return ok;
}

static BOOL EnsureLuaApiInstalledNow(const char* reason)
{
    g_luaApiEnsureLastTick = 0;
    return EnsureLuaApiInstalled(reason);
}

static void EscapeLuaString(const char* input, DWORD inputLen, char* output, DWORD outputSize)
{
    DWORD pos = 0;
    for (DWORD i = 0; i < inputLen && pos + 2 < outputSize; ++i)
    {
        unsigned char ch = (unsigned char)input[i];
        if (ch == '\\' || ch == '\'')
        {
            output[pos++] = '\\';
            output[pos++] = (char)ch;
        }
        else if (ch == '\r' || ch == '\n')
        {
            output[pos++] = '\\';
            output[pos++] = ch == '\r' ? 'r' : 'n';
        }
        else if (ch >= 32 && ch < 127)
        {
            output[pos++] = (char)ch;
        }
        else
        {
            if (pos + 4 >= outputSize)
                break;
            output[pos++] = '\\';
            output[pos++] = (char)('0' + (ch / 100));
            output[pos++] = (char)('0' + ((ch / 10) % 10));
            output[pos++] = (char)('0' + (ch % 10));
        }
    }
    output[pos] = 0;
}

static void BytesToHexString(const char* input, DWORD inputLen, char* output, DWORD outputSize)
{
    static const char* hex = "0123456789ABCDEF";
    DWORD pos = 0;

    if (!output || outputSize == 0)
        return;

    if (!input)
        inputLen = 0;

    for (DWORD i = 0; i < inputLen && pos + 2 < outputSize; ++i)
    {
        unsigned char ch = (unsigned char)input[i];
        output[pos++] = hex[(ch >> 4) & 0x0F];
        output[pos++] = hex[ch & 0x0F];
    }

    output[pos] = 0;
}

static WORD ReadUInt16LE(const char* input, DWORD inputLen, DWORD offset)
{
    if (!input || offset + 2 > inputLen)
        return 0;

    return (WORD)(((BYTE)input[offset]) | (((WORD)(BYTE)input[offset + 1]) << 8));
}

static DWORD ReadUInt32LE(const char* input, DWORD inputLen, DWORD offset)
{
    if (!input || offset + 4 > inputLen)
        return 0;

    return ((DWORD)(BYTE)input[offset]) |
        (((DWORD)(BYTE)input[offset + 1]) << 8) |
        (((DWORD)(BYTE)input[offset + 2]) << 16) |
        (((DWORD)(BYTE)input[offset + 3]) << 24);
}

static BOOL HasBinaryMagic(const char* input, DWORD inputLen, const char* magic)
{
    if (!input || !magic || inputLen < 4)
        return FALSE;

    return input[0] == magic[0] && input[1] == magic[1] && input[2] == magic[2] && input[3] == magic[3];
}

static BOOL ContainsAsciiText(const char* input, DWORD inputLen, const char* needle)
{
    DWORD needleLen = needle ? (DWORD)lstrlenA(needle) : 0;

    if (!input || !needle || needleLen == 0 || inputLen < needleLen)
        return FALSE;

    for (DWORD i = 0; i <= inputLen - needleLen; ++i)
    {
        if (memcmp(input + i, needle, needleLen) == 0)
            return TRUE;
    }

    return FALSE;
}

static void EscapeJsonString(const char* input, DWORD inputLen, char* output, DWORD outputSize)
{
    DWORD pos = 0;
    for (DWORD i = 0; i < inputLen && pos + 2 < outputSize; ++i)
    {
        unsigned char ch = (unsigned char)input[i];
        if (ch == '\\' || ch == '"')
        {
            output[pos++] = '\\';
            output[pos++] = (char)ch;
        }
        else if (ch == '\r' || ch == '\n' || ch == '\t')
        {
            output[pos++] = '\\';
            output[pos++] = ch == '\r' ? 'r' : (ch == '\n' ? 'n' : 't');
        }
        else if (ch >= 32 && ch < 127)
        {
            output[pos++] = (char)ch;
        }
        else
        {
            output[pos++] = '?';
        }
    }
    output[pos] = 0;
}

static void BuildDecodedBinaryFields(HermesRecvQueueItem const* item, char* output, DWORD outputSize)
{
    if (!output || outputSize == 0)
        return;

    output[0] = 0;
    if (!item || item->codec != HERMES_CODEC_BINARY)
        return;

    if (item->schemaId == HERMES_SCHEMA_CUSTOM_DAMAGE_EVENT_V1 && HasBinaryMagic(item->payload, item->payloadSize, "HDMG") && item->payloadSize >= 18)
    {
        WORD version = ReadUInt16LE(item->payload, item->payloadSize, 4);
        DWORD sourceRequestId = ReadUInt32LE(item->payload, item->payloadSize, 6);
        DWORD amount = ReadUInt32LE(item->payload, item->payloadSize, 10);
        WORD flags = ReadUInt16LE(item->payload, item->payloadSize, 14);
        WORD markerLen = ReadUInt16LE(item->payload, item->payloadSize, 16);
        DWORD available = item->payloadSize > 18 ? item->payloadSize - 18 : 0;
        DWORD markerCopyLen = markerLen;
        char markerEscaped[512];

        if (markerCopyLen > available)
            markerCopyLen = available;
        EscapeJsonString(item->payload + 18, markerCopyLen, markerEscaped, sizeof(markerEscaped));
        wsprintfA(output,
            ",\"schemaName\":\"custom.damage.v1\",\"version\":%lu,\"sourceRequestId\":%lu,\"amount\":%lu,\"flags\":%lu,\"marker\":\"%s\"",
            (DWORD)version,
            sourceRequestId,
            amount,
            (DWORD)flags,
            markerEscaped);
        return;
    }

    if (item->schemaId == HERMES_SCHEMA_UNIT_VITALS_SNAPSHOT_V1 && HasBinaryMagic(item->payload, item->payloadSize, "HVIT") && item->payloadSize >= 42)
    {
        WORD version = ReadUInt16LE(item->payload, item->payloadSize, 4);
        DWORD sourceRequestId = ReadUInt32LE(item->payload, item->payloadSize, 6);
        DWORD guidLow = ReadUInt32LE(item->payload, item->payloadSize, 10);
        DWORD mapId = ReadUInt32LE(item->payload, item->payloadSize, 14);
        DWORD zoneId = ReadUInt32LE(item->payload, item->payloadSize, 18);
        DWORD health = ReadUInt32LE(item->payload, item->payloadSize, 22);
        DWORD maxHealth = ReadUInt32LE(item->payload, item->payloadSize, 26);
        DWORD power = ReadUInt32LE(item->payload, item->payloadSize, 30);
        DWORD maxPower = ReadUInt32LE(item->payload, item->payloadSize, 34);
        WORD powerType = ReadUInt16LE(item->payload, item->payloadSize, 38);
        WORD flags = ReadUInt16LE(item->payload, item->payloadSize, 40);

        wsprintfA(output,
            ",\"schemaName\":\"unit.vitals.snapshot.v1\",\"version\":%lu,\"sourceRequestId\":%lu,\"guidLow\":%lu,\"mapId\":%lu,\"zoneId\":%lu,\"health\":%lu,\"maxHealth\":%lu,\"power\":%lu,\"maxPower\":%lu,\"powerType\":%lu,\"flags\":%lu,\"alive\":%s,\"inCombat\":%s,\"healthExact\":%s,\"maxHealthExact\":%s,\"powerExact\":%s,\"maxPowerExact\":%s",
            (DWORD)version,
            sourceRequestId,
            guidLow,
            mapId,
            zoneId,
            health,
            maxHealth,
            power,
            maxPower,
            (DWORD)powerType,
            (DWORD)flags,
            (flags & 0x0001) ? "true" : "false",
            (flags & 0x0002) ? "true" : "false",
            (flags & 0x0004) ? "true" : "false",
            (flags & 0x0008) ? "true" : "false",
            (flags & 0x0010) ? "true" : "false",
            (flags & 0x0020) ? "true" : "false");
        return;
    }

    if (item->schemaId == HERMES_SCHEMA_BULK_CHUNK_V1 && HasBinaryMagic(item->payload, item->payloadSize, "HBLK") && item->payloadSize >= 24)
    {
        WORD version = ReadUInt16LE(item->payload, item->payloadSize, 4);
        DWORD sourceRequestId = ReadUInt32LE(item->payload, item->payloadSize, 6);
        DWORD transferId = ReadUInt32LE(item->payload, item->payloadSize, 10);
        WORD chunkIndex = ReadUInt16LE(item->payload, item->payloadSize, 14);
        WORD chunkCount = ReadUInt16LE(item->payload, item->payloadSize, 16);
        WORD chunkFlags = ReadUInt16LE(item->payload, item->payloadSize, 18);
        WORD markerLen = ReadUInt16LE(item->payload, item->payloadSize, 20);
        WORD dataLen = ReadUInt16LE(item->payload, item->payloadSize, 22);
        DWORD available = item->payloadSize > 24 ? item->payloadSize - 24 : 0;
        DWORD markerCopyLen = markerLen;
        DWORD dataOffset = 24 + markerLen;
        DWORD dataAvailable = item->payloadSize > dataOffset ? item->payloadSize - dataOffset : 0;
        DWORD dataCopyLen = dataLen;
        char markerEscaped[512];
        char previewEscaped[512];

        if (markerCopyLen > available)
            markerCopyLen = available;
        if (dataCopyLen > dataAvailable)
            dataCopyLen = dataAvailable;
        if (dataCopyLen > 160)
            dataCopyLen = 160;

        EscapeJsonString(item->payload + 24, markerCopyLen, markerEscaped, sizeof(markerEscaped));
        EscapeJsonString(item->payload + dataOffset, dataCopyLen, previewEscaped, sizeof(previewEscaped));
        wsprintfA(output,
            ",\"schemaName\":\"bulk.chunk.v1\",\"version\":%lu,\"sourceRequestId\":%lu,\"transferId\":%lu,\"chunkIndex\":%lu,\"chunkCount\":%lu,\"chunkFlags\":%lu,\"marker\":\"%s\",\"dataLen\":%lu,\"chunkPreview\":\"%s\"",
            (DWORD)version,
            sourceRequestId,
            transferId,
            (DWORD)chunkIndex,
            (DWORD)chunkCount,
            (DWORD)chunkFlags,
            markerEscaped,
            (DWORD)dataLen,
            previewEscaped);
    }
}

static WORD ResolveHermesMethodId(const char* method)
{
    if (!method)
        return HERMES_METHOD_PING;

    if (lstrcmpiA(method, "hermes.hello") == 0)
        return HERMES_METHOD_HELLO;

    if (lstrcmpiA(method, "hermes.ping") == 0)
        return HERMES_METHOD_PING;

    if (lstrcmpiA(method, "hermes.debugEmit") == 0)
        return HERMES_METHOD_DEBUG_EMIT;

    if (lstrcmpiA(method, "hermes.getSchemaRegistry") == 0)
        return HERMES_METHOD_GET_SCHEMA_REGISTRY;

    if (lstrcmpiA(method, "hermes.getMethodRegistry") == 0)
        return HERMES_METHOD_GET_METHOD_REGISTRY;

    if (lstrcmpiA(method, "hermes.debugBinaryEmit") == 0)
        return HERMES_METHOD_DEBUG_BINARY_EMIT;

    if (lstrcmpiA(method, "hermes.debugVitalsBinaryEmit") == 0)
        return HERMES_METHOD_DEBUG_VITALS_BINARY_EMIT;

    if (lstrcmpiA(method, "hermes.debugBulkChunkEmit") == 0)
        return HERMES_METHOD_DEBUG_BULK_CHUNK_EMIT;

    if (lstrcmpiA(method, "server.getStatus") == 0)
        return HERMES_METHOD_SERVER_GET_STATUS;

    if (lstrcmpiA(method, "addon.dispatch") == 0)
        return HERMES_METHOD_ADDON_DISPATCH;

    if (lstrcmpiA(method, "server.getNumericLimits") == 0)
        return HERMES_METHOD_SERVER_GET_NUMERIC_LIMITS;

    if (lstrcmpiA(method, "player.getBasicInfo") == 0)
        return HERMES_METHOD_PLAYER_GET_BASIC_INFO;

    if (lstrcmpiA(method, "player.getPosition") == 0)
        return HERMES_METHOD_PLAYER_GET_POSITION;

    if (lstrcmpiA(method, "player.getVitals") == 0)
        return HERMES_METHOD_PLAYER_GET_VITALS;

    if (lstrcmpiA(method, "player.getSnapshot") == 0)
        return HERMES_METHOD_PLAYER_GET_SNAPSHOT;

    if (lstrcmpiA(method, "player.getAttributes") == 0)
        return HERMES_METHOD_PLAYER_GET_ATTRIBUTES;

    if (lstrcmpiA(method, "player.getTargetSnapshot") == 0)
        return HERMES_METHOD_PLAYER_GET_TARGET_SNAPSHOT;

    if (lstrcmpiA(method, "player.emitDamageEvent") == 0)
        return HERMES_METHOD_PLAYER_EMIT_DAMAGE_EVENT;

    if (lstrcmpiA(method, "player.emitVitalsBinary") == 0)
        return HERMES_METHOD_PLAYER_EMIT_VITALS_BINARY;

    if (lstrcmpiA(method, "player.emitSnapshotBulk") == 0)
        return HERMES_METHOD_PLAYER_EMIT_SNAPSHOT_BULK;

    if (lstrcmpiA(method, "ui.getDashboard") == 0)
        return HERMES_METHOD_UI_GET_DASHBOARD;

    if (lstrcmpiA(method, "ui.getModuleStatus") == 0)
        return HERMES_METHOD_UI_GET_MODULE_STATUS;

    if (lstrcmpiA(method, "abyss.getEquipmentPage") == 0)
        return HERMES_METHOD_ABYSS_GET_EQUIPMENT_PAGE;

    if (lstrcmpiA(method, "abyss.getSetOverview") == 0)
        return HERMES_METHOD_ABYSS_GET_SET_OVERVIEW;

    if (lstrcmpiA(method, "abyss.getSetBonuses") == 0)
        return HERMES_METHOD_ABYSS_GET_SET_BONUSES;

    if (lstrcmpiA(method, "abyss.getRelics") == 0)
        return HERMES_METHOD_ABYSS_GET_RELICS;

    if (lstrcmpiA(method, "cultivation.getAll") == 0)
        return HERMES_METHOD_CULTIVATION_GET_ALL;

    if (lstrcmpiA(method, "cultivation.getState") == 0)
        return HERMES_METHOD_CULTIVATION_GET_STATE;

    if (lstrcmpiA(method, "boundary.getAll") == 0)
        return HERMES_METHOD_BOUNDARY_GET_ALL;

    if (lstrcmpiA(method, "boundary.getDetail") == 0)
        return HERMES_METHOD_BOUNDARY_GET_DETAIL;

    if (lstrcmpiA(method, "breakthrough.getAll") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_ALL;

    if (lstrcmpiA(method, "breakthrough.getSystemData") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_SYSTEM_DATA;

    if (lstrcmpiA(method, "breakthrough.getInfo") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_INFO;

    if (lstrcmpiA(method, "breakthrough.getSkills") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_SKILLS;

    if (lstrcmpiA(method, "breakthrough.getSkillDetail") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_SKILL_DETAIL;

    if (lstrcmpiA(method, "breakthrough.getLeaderboard") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_LEADERBOARD;

    if (lstrcmpiA(method, "breakthrough.getExpSources") == 0)
        return HERMES_METHOD_BREAKTHROUGH_GET_EXP_SOURCES;

    if (lstrcmpiA(method, "breakthrough.upgrade") == 0)
        return HERMES_METHOD_BREAKTHROUGH_UPGRADE;

    if (lstrcmpiA(method, "breakthrough.learnSkill") == 0)
        return HERMES_METHOD_BREAKTHROUGH_LEARN_SKILL;

    if (lstrcmpiA(method, "breakthrough.upgradeSkill") == 0)
        return HERMES_METHOD_BREAKTHROUGH_UPGRADE_SKILL;

    if (lstrcmpiA(method, "breakthrough.resetSkills") == 0)
        return HERMES_METHOD_BREAKTHROUGH_RESET_SKILLS;

    if (lstrcmpiA(method, "synthesis.list") == 0)
        return HERMES_METHOD_SYNTHESIS_LIST;

    if (lstrcmpiA(method, "synthesis.do") == 0)
        return HERMES_METHOD_SYNTHESIS_DO;

    if (lstrcmpiA(method, "tooltip.query") == 0)
        return HERMES_METHOD_TOOLTIP_QUERY;

    if (lstrcmpiA(method, "tooltip.queryTemplate") == 0)
        return HERMES_METHOD_TOOLTIP_QUERY_TEMPLATE;

    if (lstrcmpiA(method, "tooltip.inspectItemGuid") == 0)
        return HERMES_METHOD_TOOLTIP_INSPECT_ITEM_GUID;

    if (lstrcmpiA(method, "tooltip.listPending") == 0)
        return HERMES_METHOD_TOOLTIP_LIST_PENDING;

    if (lstrcmpiA(method, "mall.getCategories") == 0)
        return HERMES_METHOD_MALL_GET_CATEGORIES;

    if (lstrcmpiA(method, "mall.getItems") == 0)
        return HERMES_METHOD_MALL_GET_ITEMS;

    if (lstrcmpiA(method, "mall.purchase") == 0)
        return HERMES_METHOD_MALL_PURCHASE;

    return 0;
}

static BOOL IsHermesAddonTakeoverPrefix(const char* prefix)
{
    static const char* prefixes[] =
    {
        "PLUGMGR",
        "PATTRPANEL",
        "ASCENSION",
        "ABYSS_UI",
        "Breakthrough",
        "Boundary",
        "MIRAGEUI",
        "XIANMEN",
        "XIANQI",
        "WUHUN_SYS",
        "SYNTHSYS",
        "UITQ",
        "ITEMENHANCE",
        "RUNESYSTEM",
        "VIPSYS",
        "MALLSYSTEM",
        "REALMONEY",
        "PROMOREWARD",
        "QuestRewardAttrUI",
        "REINCARNATION",
        "MATWH",
        "CUT_SYS",
        "CULT_SYS",
        "HEAL_RUNE",
        "FASHION_SYS",
        "ZDYUI_CH",
        "ZDYUI_TJ",
        "MAGICHIT",
        "TALENTSOUL",
        "POPUPTPL",
        "DarkHardcore",
        "HBUI",
        "ITEMRECYCLE",
        "RedemptionCode",
        "VIP_DATA",
        "HuanJingLevelUI",
        "ReincarnationUI"
    };
    DWORD i = 0;

    if (!prefix || !prefix[0])
        return FALSE;

    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
    {
        if (lstrcmpiA(prefix, prefixes[i]) == 0)
            return TRUE;
    }

    return FALSE;
}

static const char* SkipSpaces(const char* text)
{
    while (text && *text == ' ')
        ++text;
    return text;
}

static BOOL TryParseDwordToken(const char** cursor, DWORD* valueOut)
{
    const char* text = cursor ? *cursor : NULL;
    DWORD value = 0;
    BOOL hasDigit = FALSE;

    if (!text || !valueOut)
        return FALSE;

    while (*text >= '0' && *text <= '9')
    {
        DWORD digit = (DWORD)(*text - '0');
        DWORD next = value * 10 + digit;
        if (next < value)
            return FALSE;
        value = next;
        hasDigit = TRUE;
        ++text;
    }

    if (!hasDigit)
        return FALSE;

    *cursor = text;
    *valueOut = value;
    return TRUE;
}

static void BuildJsonRpcPayload(const char* method, const char* text, DWORD requestId, char* output, DWORD outputSize)
{
    (void)outputSize;
    char escaped[768];
    char escapedMethod[128];
    DWORD textLen = text ? (DWORD)lstrlenA(text) : 0;
    DWORD methodLen = method ? (DWORD)lstrlenA(method) : 0;
    EscapeJsonString(method ? method : "hermes.ping", methodLen, escapedMethod, sizeof(escapedMethod));
    EscapeJsonString(text ? text : "", textLen, escaped, sizeof(escaped));
    wsprintfA(output, "{\"jsonrpc\":\"2.0\",\"id\":%lu,\"method\":\"%s\",\"params\":{\"payload\":\"%s\"}}", requestId, escapedMethod, escaped);
}

static void InitializeRecvQueue(void)
{
    if (!g_recvQueueLockInitialized)
    {
        InitializeCriticalSection(&g_recvQueueLock);
        g_recvQueueLockInitialized = TRUE;
        WriteLog("HermesBridge recv queue initialized");
    }
    if (!g_recvPumpTimerLockInitialized)
    {
        InitializeCriticalSection(&g_recvPumpTimerLock);
        g_recvPumpTimerLockInitialized = TRUE;
    }
}

static void DispatchHermesMessageToLua(WORD lane, const char* payload, DWORD payloadSize)
{
    char escaped[HERMES_RECV_LUA_ESCAPED_SIZE];
    char script[HERMES_RECV_LUA_SCRIPT_SIZE];
    DWORD copySize = payloadSize < (HERMES_RECV_QUEUE_PAYLOAD_SIZE - 1) ? payloadSize : (HERMES_RECV_QUEUE_PAYLOAD_SIZE - 1);
    EscapeLuaString(payload, copySize, escaped, sizeof(escaped));
    wsprintfA(script, "if HermesDLL and HermesDLL._NativeReceive then HermesDLL._NativeReceive(%lu, '%s'); end", (DWORD)lane, escaped);

    __try { ExecuteLua(script, "HermesBridgeRecv"); }
    __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge recv Lua dispatch raised exception"); }
}

static BOOL BuildAddonMessageCoalesceKey(const char* payload, DWORD payloadSize, char* output, DWORD outputSize);

static char* AllocRecvQueuePayload(const char* payload, DWORD payloadSize, DWORD* copySizeOut)
{
    DWORD copySize = payloadSize < (HERMES_RECV_QUEUE_PAYLOAD_SIZE - 1) ? payloadSize : (HERMES_RECV_QUEUE_PAYLOAD_SIZE - 1);
    char* buffer = NULL;

    if (copySizeOut)
        *copySizeOut = copySize;

    if (copySize == 0)
        return NULL;

    buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, copySize + 1);
    if (!buffer)
        return NULL;

    if (payload)
        CopyMemory(buffer, payload, copySize);
    buffer[copySize] = 0;
    return buffer;
}

static void ClearRecvQueueItem(HermesRecvQueueItem* item)
{
    if (!item)
        return;

    if (item->payload)
        HeapFree(GetProcessHeap(), 0, item->payload);
    ZeroMemory(item, sizeof(*item));
}

static BOOL SetRecvQueueItemPayload(HermesRecvQueueItem* item, const char* payload, DWORD payloadSize)
{
    DWORD copySize = 0;
    char* copy = AllocRecvQueuePayload(payload, payloadSize, &copySize);

    if (!item)
        return FALSE;

    if (copySize > 0 && !copy)
        return FALSE;

    item->payload = copy;
    item->payloadSize = copySize;
    return TRUE;
}

static BOOL ReplaceRecvQueueItemPayload(HermesRecvQueueItem* item, const char* payload, DWORD payloadSize)
{
    DWORD copySize = 0;
    char* copy = AllocRecvQueuePayload(payload, payloadSize, &copySize);

    if (!item)
        return FALSE;

    if (copySize > 0 && !copy)
        return FALSE;

    if (item->payload)
        HeapFree(GetProcessHeap(), 0, item->payload);
    item->payload = copy;
    item->payloadSize = copySize;
    return TRUE;
}

static BOOL IsPriorityRecvQueueItem(HermesRecvQueueItem const* item)
{
    if (!item)
        return FALSE;

    return HermesBridge_IsPriorityRecvFrame(
        item->messageType == HERMES_MESSAGE_RESPONSE,
        item->messageType == HERMES_MESSAGE_ERROR,
        item->lane == HERMES_LANE_RPC,
        item->methodId == HERMES_METHOD_ADDON_MESSAGE);
}

static LONG FindRecvQueuePopIndexLocked(void)
{
    LONG index = g_recvQueueHead;
    DWORD scanned = 0;
    DWORD scanLimit = HERMES_RECV_QUEUE_PRIORITY_SCAN_LIMIT;

    if (!HermesBridge_ShouldPriorityPopRecvQueue((unsigned long)g_recvQueueCount, HERMES_RECV_QUEUE_PRIORITY_THRESHOLD))
        return g_recvQueueHead;

    while (scanned < (DWORD)g_recvQueueCount && scanned < scanLimit)
    {
        if (IsPriorityRecvQueueItem(&g_recvQueue[index]))
            return index;

        index = (index + 1) % HERMES_RECV_QUEUE_CAPACITY;
        ++scanned;
    }

    return g_recvQueueHead;
}

static BOOL PopRecvQueueItemAtLocked(LONG popIndex, HermesRecvQueueItem* item)
{
    LONG lastIndex;
    LONG index;

    if (g_recvQueueCount <= 0 || !item)
        return FALSE;

    CopyMemory(item, &g_recvQueue[popIndex], sizeof(*item));

    lastIndex = (g_recvQueueTail + HERMES_RECV_QUEUE_CAPACITY - 1) % HERMES_RECV_QUEUE_CAPACITY;
    index = popIndex;
    while (index != lastIndex)
    {
        LONG next = (index + 1) % HERMES_RECV_QUEUE_CAPACITY;
        g_recvQueue[index] = g_recvQueue[next];
        index = next;
    }

    ZeroMemory(&g_recvQueue[lastIndex], sizeof(g_recvQueue[lastIndex]));
    g_recvQueueTail = lastIndex;
    --g_recvQueueCount;
    return TRUE;
}

static BOOL PopRecvQueueItem(HermesRecvQueueItem* item)
{
    BOOL hasItem = FALSE;

    if (!g_recvQueueLockInitialized || !item)
        return FALSE;

    ZeroMemory(item, sizeof(*item));
    EnterCriticalSection(&g_recvQueueLock);
    if (g_recvQueueCount > 0)
    {
        LONG popIndex = FindRecvQueuePopIndexLocked();
        BOOL priorityPop = popIndex != g_recvQueueHead;
        if (priorityPop)
        {
            hasItem = PopRecvQueueItemAtLocked(popIndex, item);
            if (hasItem)
            {
                LONG budget = InterlockedDecrement(&g_recvPriorityPopLogBudget);
                if (budget >= 0)
                    WriteLogFormat4("HermesBridge recv queue priority pop methodId=%lu lane=%lu type=%lu queued=%lu", (DWORD)item->methodId, (DWORD)item->lane, (DWORD)item->messageType, (DWORD)g_recvQueueCount);
            }
        }
        else
        {
            CopyMemory(item, &g_recvQueue[g_recvQueueHead], sizeof(*item));
            ZeroMemory(&g_recvQueue[g_recvQueueHead], sizeof(g_recvQueue[g_recvQueueHead]));
            g_recvQueueHead = (g_recvQueueHead + 1) % HERMES_RECV_QUEUE_CAPACITY;
            --g_recvQueueCount;
            hasItem = TRUE;
        }
    }
    LeaveCriticalSection(&g_recvQueueLock);

    return hasItem;
}

static BOOL IsDroppableQueuedRecvFrame(HermesRecvQueueItem const* item)
{
    if (!item)
        return FALSE;

    if (item->lane == HERMES_LANE_SNAPSHOT)
        return TRUE;

    if (item->methodId == HERMES_METHOD_PLAYER_GET_VITALS ||
        item->methodId == HERMES_METHOD_PLAYER_GET_SNAPSHOT ||
        item->methodId == HERMES_METHOD_PLAYER_GET_TARGET_SNAPSHOT ||
        item->methodId == HERMES_METHOD_PLAYER_EMIT_VITALS_BINARY ||
        item->methodId == HERMES_METHOD_PLAYER_EMIT_SNAPSHOT_BULK)
        return TRUE;

    return FALSE;
}

static LONG FindRecvQueueDropIndex(void)
{
    LONG index = g_recvQueueHead;

    for (LONG i = 0; i < g_recvQueueCount; ++i)
    {
        if (IsDroppableQueuedRecvFrame(&g_recvQueue[index]))
            return index;
        index = (index + 1) % HERMES_RECV_QUEUE_CAPACITY;
    }

    return g_recvQueueHead;
}

static void RemoveRecvQueueItemAt(LONG removeIndex)
{
    LONG lastIndex;
    LONG index;

    if (g_recvQueueCount <= 0)
        return;

    ClearRecvQueueItem(&g_recvQueue[removeIndex]);
    lastIndex = (g_recvQueueTail + HERMES_RECV_QUEUE_CAPACITY - 1) % HERMES_RECV_QUEUE_CAPACITY;
    index = removeIndex;
    while (index != lastIndex)
    {
        LONG next = (index + 1) % HERMES_RECV_QUEUE_CAPACITY;
        g_recvQueue[index] = g_recvQueue[next];
        index = next;
    }

    ZeroMemory(&g_recvQueue[lastIndex], sizeof(g_recvQueue[lastIndex]));
    g_recvQueueTail = lastIndex;
    --g_recvQueueCount;
}

static DWORD GetRecvQueueCount(void)
{
    DWORD count = 0;
    if (!g_recvQueueLockInitialized)
        return 0;

    EnterCriticalSection(&g_recvQueueLock);
    count = (DWORD)g_recvQueueCount;
    LeaveCriticalSection(&g_recvQueueLock);
    return count;
}

static BOOL AppendLuaBatchText(char* script, DWORD scriptSize, DWORD* position, const char* text)
{
    DWORD len = text ? (DWORD)lstrlenA(text) : 0;
    if (!script || !position || *position >= scriptSize)
        return FALSE;

    if (len >= scriptSize - *position)
        return FALSE;

    CopyMemory(script + *position, text, len);
    *position += len;
    script[*position] = 0;
    return TRUE;
}

static BOOL BuildHermesReceiveLuaPayload(HermesRecvQueueItem const* item, char* output, DWORD outputSize, DWORD* outputLen)
{
    char hexPayload[HERMES_RECV_LUA_ESCAPED_SIZE];
    char decodedFields[2048];
    int written = 0;

    if (!item || !output || outputSize == 0 || !outputLen)
        return FALSE;

    output[0] = 0;
    *outputLen = 0;

    if (item->codec == HERMES_CODEC_JSON)
    {
        DWORD copySize = item->payloadSize;
        if (copySize >= outputSize)
            return FALSE;

        if (copySize > 0)
            CopyMemory(output, item->payload, copySize);
        output[copySize] = 0;
        *outputLen = copySize;
        return TRUE;
    }

    BytesToHexString(item->payload, item->payloadSize, hexPayload, sizeof(hexPayload));
    BuildDecodedBinaryFields(item, decodedFields, sizeof(decodedFields));
    written = _snprintf(output, outputSize,
        "{\"jsonrpc\":\"2.0\",\"method\":\"hermes.binaryEvent\",\"params\":{\"event\":\"hermes.binaryEvent\",\"schemaId\":%lu,\"codec\":%lu,\"messageType\":%lu,\"methodId\":%lu,\"requestId\":%lu,\"sequence\":%lu,\"payloadSize\":%lu,\"payloadHex\":\"%s\"%s}}",
        (DWORD)item->schemaId,
        (DWORD)item->codec,
        (DWORD)item->messageType,
        (DWORD)item->methodId,
        item->requestId,
        item->sequence,
        item->payloadSize,
        hexPayload,
        decodedFields);
    if (written < 0 || (DWORD)written >= outputSize)
    {
        output[outputSize - 1] = 0;
        return FALSE;
    }

    *outputLen = (DWORD)written;
    if (InterlockedDecrement(&g_recvBinaryPayloadLogBudget) >= 0)
        WriteLog(output);
    return TRUE;
}

static BOOL AppendHermesReceiveLuaPayloadCall(char* script, DWORD scriptSize, DWORD* position, WORD lane, const char* luaPayload, DWORD luaPayloadSize)
{
    char escaped[HERMES_RECV_LUA_ESCAPED_SIZE];
    char call[HERMES_RECV_LUA_ESCAPED_SIZE + 512];
    DWORD callLen = 0;
    DWORD endReserve = 8;

    if (!luaPayload)
        return FALSE;

    EscapeLuaString(luaPayload, luaPayloadSize, escaped, sizeof(escaped));
    callLen = (DWORD)_snprintf(call, sizeof(call),
        "do local ok,err=pcall(HermesDLL._NativeReceive,%lu,'%s'); if not ok then HermesDLL.recvLuaErrors=(HermesDLL.recvLuaErrors or 0)+1; HermesDLL.lastRecvLuaError=tostring(err); end end; ",
        (DWORD)lane,
        escaped);
    if ((int)callLen < 0 || callLen >= sizeof(call))
        return FALSE;

    if (!position || *position >= scriptSize || callLen + endReserve >= scriptSize - *position)
        return FALSE;

    return AppendLuaBatchText(script, scriptSize, position, call);
}

static BOOL AppendHermesReceiveLuaCall(char* script, DWORD scriptSize, DWORD* position, HermesRecvQueueItem const* item)
{
    char luaPayload[HERMES_RECV_LUA_ESCAPED_SIZE];
    DWORD luaPayloadSize = 0;

    if (!item)
        return FALSE;

    if (!BuildHermesReceiveLuaPayload(item, luaPayload, sizeof(luaPayload), &luaPayloadSize))
        return FALSE;

    if (item->codec == HERMES_CODEC_JSON && (ContainsAsciiText(item->payload, item->payloadSize, "stress-complete-v2") || ContainsAsciiText(item->payload, item->payloadSize, "stress-incomplete-v2")))
        WriteLog(item->payload);

    return AppendHermesReceiveLuaPayloadCall(script, scriptSize, position, item->lane, luaPayload, luaPayloadSize);
}

static BOOL ResetRecvLuaBatch(char* script, DWORD scriptSize, DWORD* position)
{
    if (!script || !position || scriptSize == 0)
        return FALSE;

    script[0] = 0;
    *position = 0;
    return AppendLuaBatchText(script, scriptSize, position, "if HermesDLL and HermesDLL._NativeReceive then ");
}

static BOOL FlushRecvLuaBatch(char* script, DWORD scriptSize, DWORD* position, DWORD* batchCount)
{
    int result = 0;

    if (!script || !position || !batchCount || *batchCount == 0)
        return TRUE;

    if (!AppendLuaBatchText(script, scriptSize, position, " end"))
        return FALSE;

    __try { result = ExecuteLua(script, "HermesBridgeRecvPump"); }
    __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge recv Lua pump raised exception"); result = -1; }

    {
        LONG budget = InterlockedDecrement(&g_recvLuaDispatchLogBudget);
        if (budget >= 0)
            WriteLogFormat2("HermesBridge DEBUG client lua-pump result=%lu batch=%lu", (DWORD)result, *batchCount);
    }

    if (result != 0)
        WriteLogFormat2("HermesBridge recv Lua pump returned=%lu batch=%lu", (DWORD)result, *batchCount);

    InterlockedExchangeAdd(&g_recvPumped, (LONG)*batchCount);
    *batchCount = 0;
    return ResetRecvLuaBatch(script, scriptSize, position);
}

static BOOL DispatchHermesReceiveLuaPayloadChunks(DWORD token, WORD lane, const char* payload, DWORD payloadSize, DWORD requestId, WORD methodId)
{
    DWORD offset = 0;
    DWORD chunkCount = 0;

    if (!payload)
        return FALSE;

    while (offset < payloadSize)
    {
        char escaped[HERMES_RECV_LUA_ESCAPED_SIZE];
        char script[HERMES_RECV_LUA_SCRIPT_SIZE];
        DWORD remaining = payloadSize - offset;
        DWORD chunkSize = remaining > HERMES_RECV_LUA_CHUNK_RAW_SIZE ? HERMES_RECV_LUA_CHUNK_RAW_SIZE : remaining;
        BOOL done = (offset + chunkSize) >= payloadSize;
        int result = 0;

        EscapeLuaString(payload + offset, chunkSize, escaped, sizeof(escaped));
        int written = _snprintf(script, sizeof(script),
            "if HermesDLL and HermesDLL._NativeReceiveChunk then local ok,err=pcall(HermesDLL._NativeReceiveChunk,%lu,%lu,'%s',%s); if not ok then HermesDLL.recvLuaErrors=(HermesDLL.recvLuaErrors or 0)+1; HermesDLL.lastRecvLuaError=tostring(err); end end",
            token,
            (DWORD)lane,
            escaped,
            done ? "true" : "false");
        if (written < 0 || (DWORD)written >= sizeof(script))
        {
            WriteLogFormat2("HermesBridge recv Lua chunk script too large requestId=%lu bytes=%lu", requestId, chunkSize);
            return FALSE;
        }

        __try { result = ExecuteLua(script, "HermesBridgeRecvChunk"); }
        __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge recv Lua chunk dispatch raised exception"); return FALSE; }

        if (result != 0)
        {
            WriteLogFormat2("HermesBridge recv Lua chunk dispatch returned=%lu requestId=%lu", (DWORD)result, requestId);
            return FALSE;
        }

        offset += chunkSize;
        ++chunkCount;
    }

    if (InterlockedDecrement(&g_recvChunkLogBudget) >= 0)
        WriteLogFormat4("HermesBridge recv Lua chunked requestId=%lu bytes=%lu chunks=%lu methodId=%lu",
            requestId,
            payloadSize,
            chunkCount,
            methodId);
    return TRUE;
}

static BOOL DispatchHermesReceiveLuaChunks(HermesRecvQueueItem const* item)
{
    DWORD token = 0;

    if (!item || !item->payload)
        return FALSE;

    token = item->sequence ? item->sequence : item->requestId;
    return DispatchHermesReceiveLuaPayloadChunks(token, item->lane, item->payload, item->payloadSize, item->requestId, item->methodId);
}

static DWORD PumpRecvQueueToLua(DWORD maxItems)
{
    DWORD pumped = 0;
    DWORD batchCount = 0;
    DWORD position = 0;
    DWORD pendingBefore = 0;
    BOOL luaApiReady = FALSE;
    char script[HERMES_RECV_LUA_SCRIPT_SIZE];

    if (maxItems == 0)
        maxItems = HERMES_RECV_PUMP_MAX_PER_TICK;

    pendingBefore = GetRecvQueueCount();
    if (pendingBefore == 0)
        return 0;

    luaApiReady = EnsureLuaApiInstalledNow("HermesBridge recv pump reinstalling Lua API");
    if (!luaApiReady)
    {
        char trace[256];
        _snprintf(trace, sizeof(trace), "api-not-ready pending=%lu maxItems=%lu luaState=0x%08lX", pendingBefore, maxItems, (DWORD)(uintptr_t)g_realLuaState);
        trace[sizeof(trace) - 1] = 0;
        WriteHermesClientTrace("recv-pump", trace);
        return 0;
    }

    if (!HermesBridge_ShouldPumpRecvQueue(
            pendingBefore != 0,
            luaApiReady,
            g_realLuaState != NULL,
            g_luaApiEnsureRunning != 0,
            g_recvDispatching != 0))
    {
        LONG deferBudget = InterlockedDecrement(&g_recvPumpDeferLogBudget);
        if (deferBudget >= 0)
        {
            char trace[256];
            const char* reason = (g_realLuaState == NULL) ? "no-real-lua-state" :
                ((g_luaApiEnsureRunning != 0) ? "lua-api-install-running" :
                ((g_recvDispatching != 0) ? "dispatch-running" : "gate"));
            _snprintf(trace, sizeof(trace), "defer reason=%s pending=%lu maxItems=%lu luaState=0x%08lX ensureRunning=%ld dispatching=%ld",
                reason,
                pendingBefore,
                maxItems,
                (DWORD)(uintptr_t)g_realLuaState,
                (LONG)g_luaApiEnsureRunning,
                (LONG)g_recvDispatching);
            trace[sizeof(trace) - 1] = 0;
            WriteHermesClientTrace("recv-pump", trace);
        }
        return 0;
    }

    {
        char trace[256];
        _snprintf(trace, sizeof(trace), "begin pending=%lu maxItems=%lu luaState=0x%08lX", pendingBefore, maxItems, (DWORD)(uintptr_t)g_realLuaState);
        trace[sizeof(trace) - 1] = 0;
        WriteHermesClientTrace("recv-pump", trace);
    }

    if (InterlockedCompareExchange(&g_recvDispatching, 1, 0) != 0)
    {
        WriteHermesClientTrace("recv-pump", "skip dispatch already running");
        return 0;
    }

    if (!HermesBridge_ShouldPumpRecvQueue(TRUE, TRUE, g_realLuaState != NULL, g_luaApiEnsureRunning != 0, FALSE))
    {
        LONG deferBudget = InterlockedDecrement(&g_recvPumpDeferLogBudget);
        InterlockedExchange(&g_recvDispatching, 0);
        if (deferBudget >= 0)
        {
            char trace[256];
            _snprintf(trace, sizeof(trace), "defer reason=state-changed pending=%lu maxItems=%lu luaState=0x%08lX ensureRunning=%ld",
                pendingBefore,
                maxItems,
                (DWORD)(uintptr_t)g_realLuaState,
                (LONG)g_luaApiEnsureRunning);
            trace[sizeof(trace) - 1] = 0;
            WriteHermesClientTrace("recv-pump", trace);
        }
        return 0;
    }

    if (!ResetRecvLuaBatch(script, sizeof(script), &position))
    {
        HermesRecvQueueItem item;
        ZeroMemory(&item, sizeof(item));
        InterlockedExchange(&g_recvDispatching, 0);
        return 0;
    }

    while (pumped < maxItems)
    {
        HermesRecvQueueItem item;
        if (!PopRecvQueueItem(&item))
            break;

        if (item.codec != HERMES_CODEC_JSON)
        {
            char luaPayload[HERMES_RECV_LUA_ESCAPED_SIZE];
            DWORD luaPayloadSize = 0;
            DWORD token = item.sequence ? item.sequence : item.requestId;

            if (!FlushRecvLuaBatch(script, sizeof(script), &position, &batchCount))
            {
                WriteLog("HermesBridge recv Lua batch flush failed before binary dispatch");
                break;
            }

            if (!BuildHermesReceiveLuaPayload(&item, luaPayload, sizeof(luaPayload), &luaPayloadSize) ||
                !DispatchHermesReceiveLuaPayloadChunks(token, item.lane, luaPayload, luaPayloadSize, item.requestId, item.methodId))
            {
                DWORD dropped = (DWORD)InterlockedIncrement(&g_recvQueueDropped);
                WriteLogFormat("HermesBridge recv Lua binary dispatch failed dropped=%lu", dropped);
            }

            ClearRecvQueueItem(&item);
            ++pumped;
            continue;
        }

        if (item.codec == HERMES_CODEC_JSON && item.payloadSize > HERMES_RECV_LUA_CHUNK_THRESHOLD)
        {
            if (!FlushRecvLuaBatch(script, sizeof(script), &position, &batchCount))
            {
                WriteLog("HermesBridge recv Lua batch flush failed before chunk dispatch");
                break;
            }

            if (!DispatchHermesReceiveLuaChunks(&item))
            {
                DWORD dropped = (DWORD)InterlockedIncrement(&g_recvQueueDropped);
                WriteLogFormat("HermesBridge recv Lua chunk dispatch failed dropped=%lu", dropped);
            }

            ClearRecvQueueItem(&item);
            ++pumped;
            continue;
        }

        if (!AppendHermesReceiveLuaCall(script, sizeof(script), &position, &item))
        {
            if (!FlushRecvLuaBatch(script, sizeof(script), &position, &batchCount))
            {
                WriteLog("HermesBridge recv Lua batch flush failed");
                ClearRecvQueueItem(&item);
                break;
            }

            if (!AppendHermesReceiveLuaCall(script, sizeof(script), &position, &item))
            {
                DWORD dropped = (DWORD)InterlockedIncrement(&g_recvQueueDropped);
                WriteLogFormat("HermesBridge recv Lua item too large dropped=%lu", dropped);
                ClearRecvQueueItem(&item);
                ++pumped;
                continue;
            }
        }

        ClearRecvQueueItem(&item);
        ++batchCount;
        ++pumped;
    }

    if (!FlushRecvLuaBatch(script, sizeof(script), &position, &batchCount))
        WriteLog("HermesBridge recv Lua final batch flush failed");

    if (pumped > 0)
    {
        LONG budget = InterlockedDecrement(&g_recvPumpLogBudget);
        if (budget >= 0)
            WriteLogFormat2("HermesBridge recv pump main-thread pumped=%lu pending=%lu", pumped, GetRecvQueueCount());
    }
    {
        char trace[256];
        _snprintf(trace, sizeof(trace), "end pumped=%lu pending=%lu batchLeft=%lu luaState=0x%08lX", pumped, GetRecvQueueCount(), batchCount, (DWORD)(uintptr_t)g_realLuaState);
        trace[sizeof(trace) - 1] = 0;
        WriteHermesClientTrace("recv-pump", trace);
    }

    InterlockedExchange(&g_recvDispatching, 0);
    return pumped;
}

static VOID CALLBACK HermesRecvPumpTimerProc(HWND hwnd, UINT message, UINT_PTR eventId, DWORD time)
{
    (void)hwnd;
    (void)message;
    (void)time;

    if (eventId == g_recvPumpTimer)
    {
        DWORD pending = GetRecvQueueCount();
        DWORD limit = pending > HERMES_RECV_PUMP_MAX_PER_TICK ? HERMES_RECV_PUMP_BACKLOG_PER_TICK : HERMES_RECV_PUMP_MAX_PER_TICK;
        PumpRecvQueueToLua(limit);
    }
}

typedef struct HermesWindowSearch
{
    DWORD processId;
    HWND window;
} HermesWindowSearch;

static BOOL CALLBACK FindWowMainWindowProc(HWND hwnd, LPARAM parameter)
{
    HermesWindowSearch* search = (HermesWindowSearch*)parameter;
    DWORD processId = 0;

    if (!search)
        return FALSE;

    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == search->processId && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL)
    {
        search->window = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND FindWowMainWindow(void)
{
    HermesWindowSearch search;
    ZeroMemory(&search, sizeof(search));
    search.processId = GetCurrentProcessId();
    EnumWindows(FindWowMainWindowProc, (LPARAM)&search);
    return search.window;
}

static BOOL InstallRecvPumpTimer(void)
{
    HWND freshWindow;
    UINT_PTR newTimer;
    BOOL locked = FALSE;
    BOOL rebuild;

    if (g_recvPumpTimerLockInitialized)
    {
        EnterCriticalSection(&g_recvPumpTimerLock);
        locked = TRUE;
    }

    freshWindow = FindWowMainWindow();

    /* 已装且健康则短路：timer 存在、记录窗口仍存活、且它仍是当前主窗口。切换瞬间可能
       暂时枚举不到可见顶层窗口(freshWindow==NULL)，此时只要旧窗口还活着就不动，避免误杀。 */
    if (g_recvPumpTimer != 0 && g_recvPumpWindow != NULL && IsWindow(g_recvPumpWindow) &&
        (freshWindow == NULL || freshWindow == g_recvPumpWindow))
    {
        if (locked)
            LeaveCriticalSection(&g_recvPumpTimerLock);
        return TRUE;
    }

    rebuild = (g_recvPumpTimer != 0);

    /* 杀掉绑在旧窗口上的过期 timer（旧窗口已销毁则无需 KillTimer）。 */
    if (g_recvPumpTimer != 0 && g_recvPumpWindow != NULL && IsWindow(g_recvPumpWindow))
        KillTimer(g_recvPumpWindow, g_recvPumpTimer);
    g_recvPumpTimer = 0;

    if (!freshWindow)
    {
        if (locked)
            LeaveCriticalSection(&g_recvPumpTimerLock);
        WriteLog("HermesBridge recv pump timer skipped: WoW window not found");
        return FALSE;
    }

    newTimer = SetTimer(freshWindow, HERMES_RECV_PUMP_TIMER_ID, HERMES_RECV_PUMP_INTERVAL_MS, HermesRecvPumpTimerProc);
    if (!newTimer)
    {
        if (locked)
            LeaveCriticalSection(&g_recvPumpTimerLock);
        WriteLog("HermesBridge recv pump timer install failed");
        return FALSE;
    }

    g_recvPumpWindow = freshWindow;
    g_recvPumpTimer = newTimer;
    if (rebuild)
        InterlockedIncrement(&g_recvPumpTimerReinstalls);

    if (locked)
        LeaveCriticalSection(&g_recvPumpTimerLock);

    if (rebuild)
        WriteLogFormat2("HermesBridge recv pump timer reinstalled hwnd=0x%08lX reinstalls=%lu", (DWORD)(uintptr_t)freshWindow, (DWORD)g_recvPumpTimerReinstalls);
    else
        WriteLogFormat2("HermesBridge recv pump timer installed hwnd=0x%08lX interval=%lu", (DWORD)(uintptr_t)freshWindow, (DWORD)HERMES_RECV_PUMP_INTERVAL_MS);
    return TRUE;
}

static VOID CALLBACK HermesAutoSnapshotTimerProc(HWND hwnd, UINT message, UINT_PTR eventId, DWORD time)
{
    int result = 0;
    const char* script =
        "if HermesDLL and HermesDLL._EnsureSlash then HermesDLL._EnsureSlash(); end; "
        "if HermesDLL and HermesDLL.Request and not HermesDLL.autoSnapshotSent then "
        "HermesDLL.autoSnapshotSent=true; "
        "HermesDLL.Request('player.getSnapshot','auto-state-snapshot-v2',function(msg,pending) HermesDLL.autoSnapshotReady=true; end); "
        "end";

    (void)message;
    (void)time;

    if (eventId != g_autoSnapshotTimer)
        return;

    if (InterlockedCompareExchange(&g_nativeSelfReady, 1, 1) != 1)
    {
        LONG attempts = InterlockedIncrement(&g_autoSnapshotAttempts);
        WriteLogFormat("HermesBridge auto snapshot waiting for connection self attempt=%lu", (DWORD)attempts);
        if (attempts < 6)
            return;

        KillTimer(hwnd, eventId);
        g_autoSnapshotTimer = 0;
        WriteLog("HermesBridge auto snapshot skipped: connection self not ready");
        return;
    }

    if (!EnsureLuaApiInstalledNow("HermesBridge auto snapshot reinstalling Lua API"))
    {
        WriteLog("HermesBridge auto snapshot waiting for Lua API reinstall");
        return;
    }

    KillTimer(hwnd, eventId);
    g_autoSnapshotTimer = 0;

    __try { result = ExecuteLua(script, "HermesBridgeAutoSnapshot"); }
    __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge auto snapshot Lua raised exception"); result = -1; }

    WriteLogFormat("HermesBridge auto snapshot request returned %lu", (DWORD)result);
}

static BOOL InstallAutoSnapshotTimer(void)
{
    if (g_autoSnapshotTimer)
        return TRUE;

    g_autoSnapshotWindow = FindWowMainWindow();
    if (!g_autoSnapshotWindow)
    {
        WriteLog("HermesBridge auto snapshot timer skipped: WoW window not found");
        return FALSE;
    }

    g_autoSnapshotTimer = SetTimer(g_autoSnapshotWindow, HERMES_AUTO_SNAPSHOT_TIMER_ID, HERMES_AUTO_SNAPSHOT_RETRY_MS, HermesAutoSnapshotTimerProc);
    if (!g_autoSnapshotTimer)
    {
        WriteLog("HermesBridge auto snapshot timer install failed");
        return FALSE;
    }

    InterlockedExchange(&g_autoSnapshotAttempts, 0);
    WriteLogFormat2("HermesBridge auto snapshot timer installed hwnd=0x%08lX interval=%lu", (DWORD)(uintptr_t)g_autoSnapshotWindow, (DWORD)HERMES_AUTO_SNAPSHOT_RETRY_MS);
    return TRUE;
}

static BOOL ExtractJsonStringField(const char* payload, DWORD payloadSize, const char* field, char* output, DWORD outputSize)
{
    char pattern[64];
    const char* start = NULL;
    DWORD position = 0;

    if (!payload || !field || !output || outputSize == 0)
        return FALSE;

    output[0] = 0;
    _snprintf(pattern, sizeof(pattern), "\"%s\":\"", field);
    pattern[sizeof(pattern) - 1] = 0;
    start = strstr(payload, pattern);
    if (!start || (DWORD)(start - payload) >= payloadSize)
        return FALSE;

    start += lstrlenA(pattern);
    while (*start && (DWORD)(start - payload) < payloadSize && *start != '"' && position + 1 < outputSize)
    {
        output[position++] = *start++;
    }

    output[position] = 0;
    return position > 0;
}

static BOOL ExtractJsonNumberField(const char* payload, DWORD payloadSize, const char* field, DWORD* valueOut)
{
    char pattern[64];
    const char* start = NULL;
    DWORD value = 0;
    BOOL hasDigit = FALSE;

    if (!payload || !field || !valueOut)
        return FALSE;

    _snprintf(pattern, sizeof(pattern), "\"%s\":", field);
    pattern[sizeof(pattern) - 1] = 0;
    start = strstr(payload, pattern);
    if (!start || (DWORD)(start - payload) >= payloadSize)
        return FALSE;

    start += lstrlenA(pattern);
    while (*start == ' ' || *start == '\t')
        ++start;

    while (*start >= '0' && *start <= '9' && (DWORD)(start - payload) < payloadSize)
    {
        value = value * 10 + (DWORD)(*start - '0');
        hasDigit = TRUE;
        ++start;
    }

    if (!hasDigit)
        return FALSE;

    *valueOut = value;
    return TRUE;
}

static void LogAddonFrameTrace(const char* stage, WORD lane, BYTE messageType, BYTE codec, WORD methodId, DWORD requestId, DWORD sequence, const char* payload, DWORD payloadSize)
{
    char prefix[64];
    char message[160];
    DWORD payloadBytes = payloadSize;
    LONG budget = InterlockedDecrement(&g_recvAddonLogBudget);

    if (budget < 0)
        return;

    prefix[0] = 0;
    message[0] = 0;
    ExtractJsonStringField(payload, payloadSize, "prefix", prefix, sizeof(prefix));
    ExtractJsonStringField(payload, payloadSize, "payload", message, sizeof(message));
    ExtractJsonNumberField(payload, payloadSize, "payloadBytes", &payloadBytes);

    {
        char trace[512];
        wsprintfA(trace,
            "HermesBridge DEBUG client %s lane=%lu type=%lu codec=%lu methodId=%lu requestId=%lu seq=%lu frameBytes=%lu payloadBytes=%lu prefix=%s msg=%s",
            stage ? stage : "",
            (DWORD)lane,
            (DWORD)messageType,
            (DWORD)codec,
            (DWORD)methodId,
            requestId,
            sequence,
            payloadSize,
            payloadBytes,
            prefix[0] ? prefix : "(none)",
            message[0] ? message : "(empty)");
        WriteLog(trace);
    }
}

static BOOL ShouldTraceClientFrame(WORD methodId)
{
    return methodId == HERMES_METHOD_HELLO ||
        methodId == HERMES_METHOD_ADDON_DISPATCH ||
        methodId == HERMES_METHOD_ADDON_MESSAGE ||
        methodId == HERMES_METHOD_PLAYER_GET_SNAPSHOT ||
        methodId == HERMES_METHOD_PLAYER_GET_ATTRIBUTES ||
        methodId == HERMES_METHOD_ABYSS_GET_EQUIPMENT_PAGE ||
        methodId == HERMES_METHOD_ABYSS_GET_SET_OVERVIEW ||
        methodId == HERMES_METHOD_ABYSS_GET_SET_BONUSES ||
        methodId == HERMES_METHOD_ABYSS_GET_RELICS ||
        methodId == HERMES_METHOD_TOOLTIP_QUERY ||
        methodId == HERMES_METHOD_TOOLTIP_QUERY_TEMPLATE ||
        methodId == HERMES_METHOD_TOOLTIP_INSPECT_ITEM_GUID ||
        methodId == HERMES_METHOD_TOOLTIP_LIST_PENDING ||
        methodId == HERMES_METHOD_MALL_GET_CATEGORIES ||
        methodId == HERMES_METHOD_MALL_GET_ITEMS ||
        methodId == HERMES_METHOD_SYNTHESIS_LIST ||
        methodId == HERMES_METHOD_SYNTHESIS_DO ||
        methodId == HERMES_METHOD_MALL_PURCHASE;
}

static BOOL BuildAddonMessageCoalesceKey(const char* payload, DWORD payloadSize, char* output, DWORD outputSize)
{
    char prefix[64];
    char message[128];
    char messageType[48];
    DWORD i = 0;

    if (!output || outputSize == 0)
        return FALSE;

    output[0] = 0;
    if (!ExtractJsonStringField(payload, payloadSize, "prefix", prefix, sizeof(prefix)))
        return FALSE;

    if (!ExtractJsonStringField(payload, payloadSize, "payload", message, sizeof(message)))
    {
        _snprintf(output, outputSize, "%s", prefix);
        output[outputSize - 1] = 0;
        return TRUE;
    }

    if (_strnicmp(message, "CHUNK:", 6) == 0)
        return FALSE;

    while (message[i] && message[i] != ':' && message[i] != '|' && message[i] != '\t' && i + 1 < sizeof(messageType))
    {
        messageType[i] = message[i];
        ++i;
    }
    messageType[i] = 0;

    if (!messageType[0])
        lstrcpynA(messageType, "message", sizeof(messageType));

    _snprintf(output, outputSize, "%s:%s", prefix, messageType);
    output[outputSize - 1] = 0;
    return TRUE;
}

static BOOL CoalesceQueuedAddonMessageFrame(WORD lane, BYTE messageType, BYTE codec, WORD methodId, DWORD requestId, DWORD sequence, const char* payload, DWORD payloadSize)
{
    (void)lane;
    (void)messageType;
    (void)codec;
    (void)methodId;
    (void)requestId;
    (void)sequence;
    (void)payload;
    (void)payloadSize;
    return FALSE;
}

static BOOL QueueHermesReceivedFrame(WORD lane, BYTE messageType, BYTE codec, WORD schemaId, WORD methodId, DWORD requestId, DWORD sequence, const char* payload, DWORD payloadSize)
{
    HermesRecvQueueItem item;
    ZeroMemory(&item, sizeof(item));

    item.lane = lane;
    item.messageType = messageType;
    item.codec = codec;
    item.schemaId = schemaId;
    item.methodId = methodId;
    item.requestId = requestId;
    item.sequence = sequence;

    if (!SetRecvQueueItemPayload(&item, payload, payloadSize))
    {
        DWORD dropped = (DWORD)InterlockedIncrement(&g_recvQueueDropped);
        WriteLogFormat("HermesBridge recv queue payload alloc failed dropped=%lu", dropped);
        return FALSE;
    }

    InitializeRecvQueue();

    EnterCriticalSection(&g_recvQueueLock);
    if (CoalesceQueuedAddonMessageFrame(lane, messageType, codec, methodId, requestId, sequence, item.payload, item.payloadSize))
    {
        LeaveCriticalSection(&g_recvQueueLock);
        ClearRecvQueueItem(&item);
        return TRUE;
    }

    if (g_recvQueueCount >= HERMES_RECV_QUEUE_CAPACITY)
    {
        LONG dropIndex = FindRecvQueueDropIndex();
        DWORD dropped = (DWORD)InterlockedIncrement(&g_recvQueueDropped);
        LONG budget = InterlockedDecrement(&g_recvQueueDropLogBudget);
        RemoveRecvQueueItemAt(dropIndex);
        if (budget >= 0)
            WriteLogFormat("HermesBridge recv queue full dropped queued=%lu", dropped);
    }

    g_recvQueue[g_recvQueueTail] = item;
    ZeroMemory(&item, sizeof(item));
    g_recvQueueTail = (g_recvQueueTail + 1) % HERMES_RECV_QUEUE_CAPACITY;
    ++g_recvQueueCount;
    LeaveCriticalSection(&g_recvQueueLock);

    return TRUE;
}

static BOOL TryHandleHermesSmsg(void* dispatchSelf, void* packet)
{
    DWORD cursor = SafeReadDword(packet, 0x14);
    DWORD end = SafeReadDword(packet, 0x10);
    DWORD bufferBase = SafeReadDword(packet, 0x04);
    DWORD bufferBias = SafeReadDword(packet, 0x08);
    DWORD buffer = bufferBase - bufferBias;
    DWORD address;
    DWORD remaining;
    WORD magic;
    BYTE version;
    BYTE headerSize;
    BYTE lane;
    BYTE messageType;
    BYTE codec;
    BYTE flags;
    WORD schemaId;
    WORD methodId;
    DWORD requestId;
    DWORD sequence;
    DWORD payloadSize;
    char payload[HERMES_RECV_QUEUE_PAYLOAD_SIZE];

    if (!buffer || cursor > end || end - cursor < HERMES_FRAME_HEADER_SIZE)
        return FALSE;

    address = buffer + cursor;
    remaining = end - cursor;
    magic = SafeReadWordAt(address);
    version = SafeReadByteAt(address + 2);
    headerSize = SafeReadByteAt(address + 3);
    lane = SafeReadByteAt(address + 4);
    messageType = SafeReadByteAt(address + 5);
    codec = SafeReadByteAt(address + 6);
    flags = SafeReadByteAt(address + 7);
    schemaId = SafeReadWordAt(address + 8);
    methodId = SafeReadWordAt(address + 10);
    requestId = SafeReadDwordAt(address + 12);
    sequence = SafeReadDwordAt(address + 16);
    payloadSize = SafeReadDwordAt(address + 20);
    (void)flags;
    if (magic != HERMES_FRAME_MAGIC || version != HERMES_FRAME_VERSION || headerSize < HERMES_FRAME_HEADER_SIZE || headerSize > remaining || payloadSize > remaining - headerSize || payloadSize >= sizeof(payload))
    {
        WriteLogFormat4("HermesBridge recv v2 parse miss magic=0x%04lX version=%lu size=%lu remaining=%lu", magic, version, payloadSize, remaining);
        return FALSE;
    }

    ZeroMemory(payload, sizeof(payload));
    __try { CopyMemory(payload, (void*)(address + headerSize), payloadSize); }
    __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge recv payload read raised exception"); return FALSE; }

    if (ShouldTraceClientFrame((WORD)methodId))
        LogAddonFrameTrace("recv", (WORD)lane, messageType, codec, (WORD)methodId, requestId, sequence, payload, payloadSize);

    if (lane == HERMES_LANE_RPC &&
        messageType == HERMES_MESSAGE_RESPONSE &&
        methodId == HERMES_METHOD_HELLO &&
        InterlockedCompareExchange(&g_worldLuaReady, 1, 0) == 0)
    {
        WriteLogFormat("HermesBridge world Lua ready after hello response requestId=%lu", requestId);
        InstallBootstrapTimer(HERMES_WORLD_LUA_READY_BOOTSTRAP_DELAY_MS);
    }

    LONG frameLogBudget = InterlockedDecrement(&g_recvFrameLogBudget);
    if (frameLogBudget >= 0)
    {
        WriteLogFormat4("HermesBridge recv v2 lane=%lu type=%lu codec=%lu bytes=%lu", lane, messageType, codec, payloadSize);
        WriteLogFormat4("HermesBridge recv v2 schemaId=%lu methodId=%lu requestId=%lu seq=%lu", schemaId, methodId, requestId, sequence);
        WriteLogFormat("HermesBridge recv v2 self=0x%08lX", (DWORD)(uintptr_t)dispatchSelf);
        if (codec == HERMES_CODEC_JSON && payloadSize < HERMES_RECV_QUEUE_PAYLOAD_SIZE)
            WriteLog(payload);
        else if (payloadSize < HERMES_RECV_QUEUE_PAYLOAD_SIZE)
        {
            char hexPayload[512];
            BytesToHexString(payload, payloadSize, hexPayload, sizeof(hexPayload));
            WriteLog("HermesBridge recv v2 binary payloadHex=");
            WriteLog(hexPayload);
        }
        else
            WriteLogFormat("HermesBridge recv v2 payload log skipped bytes=%lu", payloadSize);
    }

    QueueHermesReceivedFrame((WORD)lane, messageType, codec, schemaId, methodId, requestId, sequence, payload, payloadSize);
    return TRUE;
}

static void LogAfterOpcode(void* dispatchSelf, DWORD opcode, void* packet)
{
    LONG count = InterlockedIncrement(&g_recvHitCount);
    if (opcode == HERMES_SMSG_OPCODE)
        TryHandleHermesSmsg(dispatchSelf, packet);
    else if (count <= 8)
        WriteLogFormat4("HermesBridge recv dispatch hit=%lu opcode=0x%04lX self=0x%08lX packet=0x%08lX", (DWORD)count, opcode, (DWORD)(uintptr_t)dispatchSelf, (DWORD)(uintptr_t)packet);
}

static DWORD WINAPI AutoHandshakeThread(LPVOID parameter)
{
    DWORD attempt;

    (void)parameter;
    Sleep(250);

    for (attempt = 1; attempt <= HERMES_AUTO_HANDSHAKE_ATTEMPTS; ++attempt)
    {
        if (InterlockedCompareExchange(&g_worldLuaReady, 0, 0) == 1)
        {
            WriteLogFormat("HermesBridge auto handshake stopped world-ready attempt=%lu", attempt);
            return 0;
        }

        if (InterlockedCompareExchange(&g_nativeSelfReady, 1, 1) != 1)
        {
            if (attempt == HERMES_AUTO_HANDSHAKE_ATTEMPTS)
                WriteLog("HermesBridge auto handshake skipped: self not ready");
            Sleep(HERMES_AUTO_HANDSHAKE_RETRY_MS);
            continue;
        }

        WriteLogFormat("HermesBridge auto handshake sending native hello attempt=%lu", attempt);
        if (SendHermesPayload("rpc hermes.hello auto-handshake-native"))
            InterlockedExchange(&g_playerReadySent, 1);
        else
            WriteLogFormat("HermesBridge auto handshake native hello failed attempt=%lu", attempt);

        Sleep(HERMES_AUTO_HANDSHAKE_RETRY_MS);
    }

    return 0;
}

static void RestartAutoHandshakeForWorldLifecycle(const char* reason)
{
    HANDLE handshakeThread = NULL;
    InterlockedExchange(&g_worldLuaReady, 0);
    InterlockedExchange(&g_playerReadySent, 0);
    InterlockedExchange(&g_autoHandshakeThreadStarted, 0);

    if (InterlockedCompareExchange(&g_autoHandshakeThreadStarted, 1, 0) != 0)
        return;

    handshakeThread = CreateThread(NULL, 0, AutoHandshakeThread, NULL, 0, NULL);
    if (handshakeThread)
    {
        char trace[160];
        CloseHandle(handshakeThread);
        _snprintf(trace, sizeof(trace), "HermesBridge lifecycle restarted auto handshake reason=%s", reason ? reason : "");
        trace[sizeof(trace) - 1] = 0;
        WriteLog(trace);
    }
    else
    {
        InterlockedExchange(&g_autoHandshakeThreadStarted, 0);
        WriteLog("HermesBridge lifecycle auto handshake thread create failed");
    }
}

static DWORD WINAPI WorldBootstrapRetryThread(LPVOID parameter)
{
    DWORD attempts = 0;
    (void)parameter;

    Sleep(HERMES_WORLD_BOOTSTRAP_RETRY_DELAY_MS);
    WriteLog("HermesBridge world bootstrap retry after connection self");

    for (attempts = 0; attempts < 8; ++attempts)
    {
        if (InstallBootstrapTimer(HERMES_BOOTSTRAP_TIMER_DELAY_MS))
            return 0;
        Sleep(1000);
    }

    WriteLog("HermesBridge world bootstrap retry abandoned after retries");
    return 0;
}

__declspec(naked) static void HookAfterOpcode(void)
{
    __asm {
        pushfd
        pushad
        push ebx
        push esi
        push edi
        call LogAfterOpcode
        add esp, 0Ch
        popad
        popfd
        jmp g_recvTrampoline
    }
}

static uint32_t __fastcall HookedUpperSend(void* self, void* edxValue, void* packet, uint32_t mode)
{
    (void)edxValue;
    LONG count = InterlockedIncrement(&g_upperSendHitCount);
    LONG budget = InterlockedDecrement(&g_upperSendLogBudget);
    void* previousConnection = g_connectionSelf;
    BOOL newConnection = previousConnection != self;

    g_connectionSelf = self;
    InterlockedExchange(&g_nativeSelfReady, 1);

    if (newConnection)
    {
        LONG generation = InterlockedIncrement(&g_connectionGeneration);
        InterlockedExchange(&g_playerReadySent, 0);
        InterlockedExchange(&g_autoHandshakeThreadStarted, 0);
        InterlockedExchange(&g_worldBootstrapThreadStarted, 0);
        InterlockedExchange(&g_autoSnapshotAttempts, 0);
        InterlockedExchange(&g_worldLuaReady, 0);
        g_luaApiEnsureLastTick = 0;

        if (g_autoSnapshotTimer && g_autoSnapshotWindow)
        {
            KillTimer(g_autoSnapshotWindow, g_autoSnapshotTimer);
            g_autoSnapshotTimer = 0;
            g_autoSnapshotWindow = NULL;
        }

        WriteLogFormat2("HermesBridge connection self changed generation=%lu self=0x%08lX", (DWORD)generation, (DWORD)(uintptr_t)self);
    }

    if (InterlockedCompareExchange(&g_worldBootstrapThreadStarted, 1, 0) == 0)
    {
        HANDLE bootstrapThread = CreateThread(NULL, 0, WorldBootstrapRetryThread, NULL, 0, NULL);
        if (bootstrapThread)
            CloseHandle(bootstrapThread);
        else
            WriteLog("HermesBridge world bootstrap retry thread create failed");
    }
    if (InterlockedCompareExchange(&g_autoHandshakeThreadStarted, 1, 0) == 0)
    {
        HANDLE handshakeThread = CreateThread(NULL, 0, AutoHandshakeThread, NULL, 0, NULL);
        if (handshakeThread)
            CloseHandle(handshakeThread);
        else
            WriteLog("HermesBridge auto handshake thread create failed");
    }
    if (budget >= 0)
        WriteLogFormat2("HermesBridge upper send hit count=%lu self=0x%08lX", (DWORD)count, (DWORD)(uintptr_t)self);

    UpperSendFn original = (UpperSendFn)g_upperSendTrampoline;
    return original(self, packet, mode);
}

static void InitializeSendQueue(void)
{
    if (!g_sendQueueLockInitialized)
    {
        InitializeCriticalSection(&g_sendQueueLock);
        g_sendQueueLockInitialized = TRUE;
        WriteLog("HermesBridge SendAddonMessage queue initialized");
    }
}

static BOOL PopSendQueuePayload(char* payload, DWORD payloadSize)
{
    BOOL hasPayload = FALSE;

    if (!g_sendQueueLockInitialized || !payload || payloadSize == 0)
        return FALSE;

    ZeroMemory(payload, payloadSize);
    EnterCriticalSection(&g_sendQueueLock);
    if (g_sendQueueCount > 0)
    {
        lstrcpynA(payload, g_sendQueue[g_sendQueueHead], payloadSize);
        ZeroMemory(g_sendQueue[g_sendQueueHead], HERMES_SEND_QUEUE_PAYLOAD_SIZE);
        g_sendQueueHead = (g_sendQueueHead + 1) % HERMES_SEND_QUEUE_CAPACITY;
        --g_sendQueueCount;
        hasPayload = TRUE;
    }
    LeaveCriticalSection(&g_sendQueueLock);

    return hasPayload;
}

static DWORD GetSendQueueCount(void)
{
    DWORD count = 0;
    if (!g_sendQueueLockInitialized)
        return 0;

    EnterCriticalSection(&g_sendQueueLock);
    count = (DWORD)g_sendQueueCount;
    LeaveCriticalSection(&g_sendQueueLock);
    return count;
}

static BOOL RemoveSendQueueItemAtLocked(LONG logicalIndex, char* removedPayload, DWORD removedPayloadSize)
{
    LONG i;

    if (logicalIndex < 0 || logicalIndex >= g_sendQueueCount)
        return FALSE;

    if (removedPayload && removedPayloadSize > 0)
    {
        LONG removeIndex = (g_sendQueueHead + logicalIndex) % HERMES_SEND_QUEUE_CAPACITY;
        ZeroMemory(removedPayload, removedPayloadSize);
        lstrcpynA(removedPayload, g_sendQueue[removeIndex], removedPayloadSize);
    }

    for (i = logicalIndex; i < g_sendQueueCount - 1; ++i)
    {
        LONG current = (g_sendQueueHead + i) % HERMES_SEND_QUEUE_CAPACITY;
        LONG next = (g_sendQueueHead + i + 1) % HERMES_SEND_QUEUE_CAPACITY;
        ZeroMemory(g_sendQueue[current], HERMES_SEND_QUEUE_PAYLOAD_SIZE);
        lstrcpynA(g_sendQueue[current], g_sendQueue[next], HERMES_SEND_QUEUE_PAYLOAD_SIZE);
    }

    g_sendQueueTail = (g_sendQueueTail + HERMES_SEND_QUEUE_CAPACITY - 1) % HERMES_SEND_QUEUE_CAPACITY;
    ZeroMemory(g_sendQueue[g_sendQueueTail], HERMES_SEND_QUEUE_PAYLOAD_SIZE);
    --g_sendQueueCount;
    return TRUE;
}

static BOOL RemoveFirstDiagnosticSendQueueItemLocked(char* removedPayload, DWORD removedPayloadSize)
{
    LONG i;

    for (i = 0; i < g_sendQueueCount; ++i)
    {
        LONG index = (g_sendQueueHead + i) % HERMES_SEND_QUEUE_CAPACITY;
        if (HermesBridge_IsDiagnosticSendPayload(g_sendQueue[index]))
            return RemoveSendQueueItemAtLocked(i, removedPayload, removedPayloadSize);
    }

    return FALSE;
}

static void LogSendQueueDrop(const char* reason, const char* payload, DWORD queued)
{
    DWORD dropped = (DWORD)InterlockedIncrement(&g_sendQueueDropped);
    char preview[160];
    char trace[384];

    if (!HermesBridge_ShouldLogSendQueueDrop(dropped))
        return;

    CopyTracePreview(payload ? payload : "", preview, sizeof(preview));
    _snprintf(trace,
        sizeof(trace),
        "HermesBridge SendAddonMessage queue drop reason=%s dropped=%lu queued=%lu capacity=%lu diagnostic=%ld payload=%s",
        reason ? reason : "queue-full",
        dropped,
        queued,
        (DWORD)HERMES_SEND_QUEUE_CAPACITY,
        (LONG)HermesBridge_IsDiagnosticSendPayload(payload),
        preview);
    trace[sizeof(trace) - 1] = 0;
    WriteLog(trace);
}

static DWORD WINAPI SendAddonQueueWorkerThread(LPVOID parameter)
{
    (void)parameter;
    Sleep(20);

    for (;;)
    {
        char payload[HERMES_SEND_QUEUE_PAYLOAD_SIZE];
        if (!PopSendQueuePayload(payload, sizeof(payload)))
        {
            InterlockedExchange(&g_sendQueueWorkerRunning, 0);
            if (GetSendQueueCount() > 0 && InterlockedCompareExchange(&g_sendQueueWorkerRunning, 1, 0) == 0)
                continue;
            return 0;
        }

        if (payload[0])
        {
            int waitAttempt = 0;
            if (!g_connectionSelf)
                WriteLog("HermesBridge SendAddonMessage queued send waiting for connection self");
            while (!g_connectionSelf && waitAttempt < HERMES_SEND_QUEUE_SELF_WAIT_ATTEMPTS)
            {
                Sleep(HERMES_SEND_QUEUE_SELF_WAIT_MS);
                ++waitAttempt;
            }
            if (!g_connectionSelf)
            {
                WriteLog("HermesBridge SendAddonMessage queued send dropped: no connection self after wait");
            }
            else
            {
                SendHermesPayload(payload);
            }
        }
        else
            WriteLog("HermesBridge SendAddonMessage queued send skipped empty payload");

        Sleep(10);
    }
}

static BOOL QueueSendAddonPayload(const char* payload)
{
    BOOL queued = FALSE;
    BOOL shouldStartWorker = FALSE;
    BOOL droppedDiagnostic = FALSE;
    BOOL evictedDiagnostic = FALSE;
    BOOL diagnosticPayload = FALSE;
    DWORD queuedBefore = 0;
    char evictedPayload[HERMES_SEND_QUEUE_PAYLOAD_SIZE];
    HANDLE thread = NULL;

    if (!payload)
        payload = "";
    diagnosticPayload = HermesBridge_IsDiagnosticSendPayload(payload);
    ZeroMemory(evictedPayload, sizeof(evictedPayload));

    if (!g_sendQueueLockInitialized)
        InitializeSendQueue();

    EnterCriticalSection(&g_sendQueueLock);
    queuedBefore = (DWORD)g_sendQueueCount;
    if (HermesBridge_ShouldDropDiagnosticSendPayload(payload, queuedBefore, HERMES_SEND_QUEUE_CAPACITY, HERMES_SEND_QUEUE_DIAGNOSTIC_RESERVE))
    {
        droppedDiagnostic = TRUE;
    }
    else
    {
        if (g_sendQueueCount >= HERMES_SEND_QUEUE_CAPACITY && !diagnosticPayload)
            evictedDiagnostic = RemoveFirstDiagnosticSendQueueItemLocked(evictedPayload, sizeof(evictedPayload));

        if (g_sendQueueCount < HERMES_SEND_QUEUE_CAPACITY)
        {
            ZeroMemory(g_sendQueue[g_sendQueueTail], HERMES_SEND_QUEUE_PAYLOAD_SIZE);
            lstrcpynA(g_sendQueue[g_sendQueueTail], payload, HERMES_SEND_QUEUE_PAYLOAD_SIZE);
            g_sendQueueTail = (g_sendQueueTail + 1) % HERMES_SEND_QUEUE_CAPACITY;
            ++g_sendQueueCount;
            queued = TRUE;
        }
    }
    LeaveCriticalSection(&g_sendQueueLock);

    if (evictedDiagnostic)
        LogSendQueueDrop("evict-diagnostic-for-business", evictedPayload, queuedBefore);

    if (droppedDiagnostic)
    {
        LogSendQueueDrop("diagnostic-reserve", payload, queuedBefore);
        return FALSE;
    }

    if (!queued)
    {
        LogSendQueueDrop("queue-full", payload, queuedBefore);
        return FALSE;
    }

    shouldStartWorker = InterlockedCompareExchange(&g_sendQueueWorkerRunning, 1, 0) == 0;
    if (shouldStartWorker)
    {
        thread = CreateThread(NULL, 0, SendAddonQueueWorkerThread, NULL, 0, NULL);
        if (!thread)
        {
            InterlockedExchange(&g_sendQueueWorkerRunning, 0);
            WriteLog("HermesBridge SendAddonMessage queue worker create failed");
            return FALSE;
        }
        CloseHandle(thread);
    }

    return TRUE;
}

static int __cdecl HookedSendAddonMessage(void* luaState)
{
    LONG count = InterlockedIncrement(&g_sendAddonHitCount);
    LONG budget = InterlockedDecrement(&g_sendAddonLogBudget);
    LuaCheckStringFn checkString = (LuaCheckStringFn)HERMES_LUA_CHECK_STRING_ADDR;
    const char* prefix = NULL;
    const char* payload = NULL;
    const char* channel = NULL;
    int typeOut = 0;
    BOOL capturedRealLuaState = FALSE;

    capturedRealLuaState = CaptureRealLuaState(luaState, "SendAddonMessage");

    if (luaState)
        EnsureLuaApiInstalled("HermesBridge installing Lua API into real addon lua_State");

    if (capturedRealLuaState)
        PumpRecvQueueToLua(HERMES_RECV_PUMP_BACKLOG_PER_TICK);

    __try
    {
        prefix = checkString(luaState, 1, 0, &typeOut);
        payload = checkString(luaState, 2, 0, &typeOut);
        channel = checkString(luaState, 3, 0, &typeOut);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WriteLog("HermesBridge SendAddonMessage hook argument read raised exception");
    }

    if (prefix && lstrcmpiA(prefix, "HERMESDLL") == 0)
    {
        char preview[160];
        char trace[320];
        if (!payload)
            payload = "";
        CopyTracePreview(payload, preview, sizeof(preview));
        _snprintf(trace, sizeof(trace), "prefix=HERMESDLL channel=%s bytes=%lu payload=%s", channel ? channel : "(nil)", (DWORD)lstrlenA(payload), preview);
        trace[sizeof(trace) - 1] = 0;
        WriteHermesClientTrace("send-addon", trace);
        if (budget >= 0)
            WriteLogFormat2("HermesBridge SendAddonMessage intercepted hit=%lu bytes=%lu", (DWORD)count, (DWORD)lstrlenA(payload));
        if (_strnicmp(payload, "rpc hermes.ping trace lifecycle ", 32) == 0 &&
            (strstr(payload, "reason=reset-") || strstr(payload, "reason=event detail=PLAYER_ENTERING_WORLD")))
            RestartAutoHandshakeForWorldLifecycle("lua-lifecycle");
        QueueSendAddonPayload(payload);
        return 0;
    }

    if (prefix && IsHermesAddonTakeoverPrefix(prefix) && (!channel || lstrcmpiA(channel, "WHISPER") == 0))
    {
        char takeoverPayload[HERMES_SEND_QUEUE_PAYLOAD_SIZE];
        DWORD headerLen = (DWORD)lstrlenA("rpc addon.dispatch ");
        DWORD prefixLen = (DWORD)lstrlenA(prefix);
        DWORD payloadLen = payload ? (DWORD)lstrlenA(payload) : 0;
        if (!payload)
            payload = "";

        {
            char preview[160];
            char trace[320];
            CopyTracePreview(payload, preview, sizeof(preview));
            _snprintf(trace, sizeof(trace), "takeover prefix=%s channel=%s bytes=%lu payload=%s", prefix ? prefix : "(nil)", channel ? channel : "(nil)", payloadLen, preview);
            trace[sizeof(trace) - 1] = 0;
            WriteHermesClientTrace("send-addon", trace);
        }

        if (headerLen + prefixLen + 1 + payloadLen >= sizeof(takeoverPayload))
        {
            char droppedTrace[256];
            wsprintfA(droppedTrace, "HermesBridge AddOn takeover dropped prefix='%s' bytes=%lu", prefix, payloadLen);
            WriteLog(droppedTrace);
            return 0;
        }

        ZeroMemory(takeoverPayload, sizeof(takeoverPayload));
        wsprintfA(takeoverPayload, "rpc addon.dispatch %s\t%s", prefix, payload);
        if (budget >= 0)
        {
            char trace[256];
            wsprintfA(trace, "HermesBridge AddOn takeover prefix='%s' bytes=%lu", prefix, payloadLen);
            WriteLog(trace);
        }
        QueueSendAddonPayload(takeoverPayload);
        return 0;
    }

    if (budget >= 0 && prefix)
        WriteLogFormat2("HermesBridge SendAddonMessage pass-through hit=%lu prefixPtr=0x%08lX", (DWORD)count, (DWORD)(uintptr_t)prefix);

    LuaCFunctionFn original = (LuaCFunctionFn)g_sendAddonTrampoline;
    return original(luaState);
}

static BOOL InstallJumpHook(uintptr_t target, void* detour, BYTE original[6], BYTE** trampolineOut)
{
    DWORD oldProtect = 0;
    BYTE* trampoline = (BYTE*)VirtualAlloc(NULL, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return FALSE;

    CopyMemory(original, (void*)target, 6);
    if (original[0] == 0xE9)
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }

    CopyMemory(trampoline, original, 6);
    trampoline[6] = 0xE9;
    *(DWORD*)(trampoline + 7) = (DWORD)((target + 6) - ((uintptr_t)trampoline + 11));

    if (!VirtualProtect((void*)target, 6, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }

    BYTE patch[6];
    patch[0] = 0xE9;
    *(DWORD*)(patch + 1) = (DWORD)((uintptr_t)detour - (target + 5));
    patch[5] = 0x90;
    CopyMemory((void*)target, patch, 6);
    FlushInstructionCache(GetCurrentProcess(), (void*)target, 6);
    VirtualProtect((void*)target, 6, oldProtect, &oldProtect);

    *trampolineOut = trampoline;
    return TRUE;
}

static BOOL InstallJumpHook5(uintptr_t target, void* detour, BYTE original[5], BYTE** trampolineOut)
{
    DWORD oldProtect = 0;
    BYTE* trampoline = (BYTE*)VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return FALSE;

    CopyMemory(original, (void*)target, 5);
    if (original[0] == 0xE9)
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }

    CopyMemory(trampoline, original, 5);
    trampoline[5] = 0xE9;
    *(DWORD*)(trampoline + 6) = (DWORD)((target + 5) - ((uintptr_t)trampoline + 10));

    if (!VirtualProtect((void*)target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }

    ((BYTE*)target)[0] = 0xE9;
    *(DWORD*)((BYTE*)target + 1) = (DWORD)((uintptr_t)detour - (target + 5));
    FlushInstructionCache(GetCurrentProcess(), (void*)target, 5);
    VirtualProtect((void*)target, 5, oldProtect, &oldProtect);

    *trampolineOut = trampoline;
    return TRUE;
}

static BOOL InstallJumpHookN(uintptr_t target, void* detour, BYTE* original, DWORD patchSize, BYTE** trampolineOut)
{
    DWORD oldProtect = 0;
    BYTE* trampoline = (BYTE*)VirtualAlloc(NULL, patchSize + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline || patchSize < 5)
        return FALSE;

    CopyMemory(original, (void*)target, patchSize);
    if (original[0] == 0xE9)
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }

    CopyMemory(trampoline, original, patchSize);
    trampoline[patchSize] = 0xE9;
    *(DWORD*)(trampoline + patchSize + 1) = (DWORD)((target + patchSize) - ((uintptr_t)trampoline + patchSize + 5));

    if (!VirtualProtect((void*)target, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }

    ((BYTE*)target)[0] = 0xE9;
    *(DWORD*)((BYTE*)target + 1) = (DWORD)((uintptr_t)detour - (target + 5));
    for (DWORD i = 5; i < patchSize; ++i)
        ((BYTE*)target)[i] = 0x90;
    FlushInstructionCache(GetCurrentProcess(), (void*)target, patchSize);
    VirtualProtect((void*)target, patchSize, oldProtect, &oldProtect);

    *trampolineOut = trampoline;
    return TRUE;
}

static BOOL InstallNativeHooks(void)
{
    BOOL ok = TRUE;

    if (!g_upperSendHookInstalled && InstallJumpHook(HERMES_UPPER_SEND_ADDR, HookedUpperSend, g_upperSendOriginal, &g_upperSendTrampoline))
    {
        g_upperSendHookInstalled = TRUE;
        WriteLog("HermesBridge upper send hook installed");
    }
    else if (!g_upperSendHookInstalled)
    {
        ok = FALSE;
        WriteLog("HermesBridge upper send hook install failed");
    }

    if (!g_recvHookInstalled && InstallJumpHook5(HERMES_AFTER_OPCODE_ADDR, HookAfterOpcode, g_recvOriginal, &g_recvTrampoline))
    {
        g_recvHookInstalled = TRUE;
        WriteLog("HermesBridge recv hook installed");
    }
    else if (!g_recvHookInstalled)
    {
        ok = FALSE;
        WriteLog("HermesBridge recv hook install failed");
    }

    if (!g_sendAddonHookInstalled && InstallJumpHookN(HERMES_SEND_ADDON_MESSAGE_ADDR, HookedSendAddonMessage, g_sendAddonOriginal, sizeof(g_sendAddonOriginal), &g_sendAddonTrampoline))
    {
        g_sendAddonHookInstalled = TRUE;
        WriteLog("HermesBridge SendAddonMessage hook installed");
    }
    else if (!g_sendAddonHookInstalled)
    {
        ok = FALSE;
        WriteLog("HermesBridge SendAddonMessage hook install failed");
    }

    return ok;
}

static BOOL SendHermesPayload(const char* text)
{
    void* connection = g_connectionSelf;
    if (!connection)
    {
        char preview[160];
        char trace[320];
        CopyTracePreview(text ? text : "", preview, sizeof(preview));
        _snprintf(trace, sizeof(trace), "no-connection payload=%s", preview);
        trace[sizeof(trace) - 1] = 0;
        WriteHermesClientTrace("native-send", trace);
        WriteLog("HermesBridge native send skipped: no connection self yet");
        return FALSE;
    }

    const char* method = "hermes.ping";
    const char* methodPayload = text ? text : "";
    char methodBuffer[96];
    DWORD requestId = (DWORD)InterlockedIncrement(&g_nextRequestId);
    if (text && _strnicmp(text, "rpcid ", 6) == 0)
    {
        const char* cursor = SkipSpaces(text + 6);
        DWORD explicitRequestId = 0;
        if (TryParseDwordToken(&cursor, &explicitRequestId) && explicitRequestId > 0)
        {
            const char* methodStart = SkipSpaces(cursor);
            const char* methodEnd = methodStart;
            while (*methodEnd && *methodEnd != ' ')
                ++methodEnd;

            DWORD methodLen = (DWORD)(methodEnd - methodStart);
            if (methodLen > 0 && methodLen < sizeof(methodBuffer))
            {
                ZeroMemory(methodBuffer, sizeof(methodBuffer));
                CopyMemory(methodBuffer, methodStart, methodLen);
                method = methodBuffer;
                methodPayload = *methodEnd == ' ' ? methodEnd + 1 : "";
                requestId = explicitRequestId;
            }
        }
    }
    else if (text && _strnicmp(text, "rpc ", 4) == 0)
    {
        const char* methodStart = text + 4;
        const char* methodEnd = methodStart;
        while (*methodEnd && *methodEnd != ' ')
            ++methodEnd;

        DWORD methodLen = (DWORD)(methodEnd - methodStart);
        if (methodLen > 0 && methodLen < sizeof(methodBuffer))
        {
            ZeroMemory(methodBuffer, sizeof(methodBuffer));
            CopyMemory(methodBuffer, methodStart, methodLen);
            method = methodBuffer;
            methodPayload = *methodEnd == ' ' ? methodEnd + 1 : "";
        }
    }

    WORD methodId = ResolveHermesMethodId(method);
    DWORD methodPayloadLen = methodPayload ? (DWORD)lstrlenA(methodPayload) : 0;
    if (methodPayload && _strnicmp(methodPayload, "trace ", 6) == 0)
    {
        char scope[64];
        char detail[384];
        const char* scopeStart = methodPayload + 6;
        const char* scopeEnd = scopeStart;
        while (*scopeEnd && *scopeEnd != ' ')
            ++scopeEnd;
        ZeroMemory(scope, sizeof(scope));
        ZeroMemory(detail, sizeof(detail));
        if (scopeEnd > scopeStart)
        {
            DWORD scopeLen = (DWORD)(scopeEnd - scopeStart);
            if (scopeLen >= sizeof(scope))
                scopeLen = sizeof(scope) - 1;
            CopyMemory(scope, scopeStart, scopeLen);
            CopyTracePreview(*scopeEnd == ' ' ? scopeEnd + 1 : "", detail, sizeof(detail));
            WriteHermesClientTrace(scope, detail);
        }
    }
    if (methodPayload && (_strnicmp(methodPayload, "trace ", 6) == 0 || _strnicmp(methodPayload, "addon-compat ", 13) == 0))
    {
        char preview[320];
        char trace[512];
        CopyTracePreview(methodPayload, preview, sizeof(preview));
        _snprintf(trace, sizeof(trace), "method=%s requestId=%lu methodId=%lu payload=%s", method ? method : "", requestId, (DWORD)methodId, preview);
        trace[sizeof(trace) - 1] = 0;
        WriteHermesClientTrace("native-send", trace);
    }
    if (methodPayload && _strnicmp(methodPayload, "addon-compat ", 13) == 0)
    {
        char compatTrace[512];
        _snprintf(compatTrace, sizeof(compatTrace), "HermesBridge DEBUG client %s", methodPayload);
        compatTrace[sizeof(compatTrace) - 1] = 0;
        WriteHermesClientTrace("addon-compat", methodPayload + 13);
        WriteLog(compatTrace);
    }
    LONG sendLogBudget = InterlockedDecrement(&g_nativeSendLogBudget);
    if (sendLogBudget >= 0)
    {
        char sendTrace[256];
        wsprintfA(sendTrace, "HermesBridge native v2 prepare method='%s' methodId=%lu requestId=%lu payloadBytes=%lu", method ? method : "", (DWORD)methodId, requestId, methodPayloadLen);
        WriteLog(sendTrace);
    }
    if (methodId == 0)
    {
        char droppedTrace[256];
        wsprintfA(droppedTrace, "HermesBridge native v2 dropped unknown method requestId=%lu method='%s'", requestId, method ? method : "");
        WriteLog(droppedTrace);
        return FALSE;
    }

    DWORD sequence = (DWORD)InterlockedIncrement(&g_nextSequence);
    char jsonPayload[1024];
    ZeroMemory(jsonPayload, sizeof(jsonPayload));
    BuildJsonRpcPayload(method, methodPayload, requestId, jsonPayload, sizeof(jsonPayload));

    BYTE payload[1536];
    ZeroMemory(payload, sizeof(payload));
    uint32_t jsonLen = (uint32_t)lstrlenA(jsonPayload);
    if (jsonLen > sizeof(payload) - 4 - HERMES_FRAME_HEADER_SIZE)
        jsonLen = sizeof(payload) - 4 - HERMES_FRAME_HEADER_SIZE;

    DWORD pos = 0;
    uint32_t opcode = HERMES_CMSG_OPCODE;
    WORD magic = HERMES_FRAME_MAGIC;
    BYTE frameVersion = HERMES_FRAME_VERSION;
    BYTE headerSize = HERMES_FRAME_HEADER_SIZE;
    BYTE lane = HERMES_LANE_RPC;
    BYTE messageType = HERMES_MESSAGE_REQUEST;
    BYTE codec = HERMES_CODEC_JSON;
    BYTE flags = 0;
    WORD schemaId = 0;
    CopyMemory(payload + pos, &opcode, sizeof(opcode));
    pos += sizeof(opcode);
    CopyMemory(payload + pos, &magic, sizeof(magic));
    pos += sizeof(magic);
    CopyMemory(payload + pos, &frameVersion, sizeof(frameVersion));
    pos += sizeof(frameVersion);
    CopyMemory(payload + pos, &headerSize, sizeof(headerSize));
    pos += sizeof(headerSize);
    CopyMemory(payload + pos, &lane, sizeof(lane));
    pos += sizeof(lane);
    CopyMemory(payload + pos, &messageType, sizeof(messageType));
    pos += sizeof(messageType);
    CopyMemory(payload + pos, &codec, sizeof(codec));
    pos += sizeof(codec);
    CopyMemory(payload + pos, &flags, sizeof(flags));
    pos += sizeof(flags);
    CopyMemory(payload + pos, &schemaId, sizeof(schemaId));
    pos += sizeof(schemaId);
    CopyMemory(payload + pos, &methodId, sizeof(methodId));
    pos += sizeof(methodId);
    CopyMemory(payload + pos, &requestId, sizeof(requestId));
    pos += sizeof(requestId);
    CopyMemory(payload + pos, &sequence, sizeof(sequence));
    pos += sizeof(sequence);
    CopyMemory(payload + pos, &jsonLen, sizeof(jsonLen));
    pos += sizeof(jsonLen);
    CopyMemory(payload + pos, jsonPayload, jsonLen);
    pos += jsonLen;

    __try
    {
        PacketCreateFn createPacket = (PacketCreateFn)HERMES_PACKET_CREATE_ADDR;
        UpperSendFn sendPacket = (UpperSendFn)HERMES_UPPER_SEND_ADDR;
        void* packet = createPacket(connection, payload, pos, 0);
        if (!packet)
        {
            WriteLog("HermesBridge native packet create returned null");
            return FALSE;
        }

        BYTE sendObject[64];
        ZeroMemory(sendObject, sizeof(sendObject));
        DWORD packetBuffer = *(DWORD*)((BYTE*)packet + 0x08) + 2;
        DWORD packetLength = *(DWORD*)((BYTE*)packet + 0x0C) - 2;
        *(DWORD*)(sendObject + 0x00) = *(DWORD*)packet;
        *(DWORD*)(sendObject + 0x04) = packetBuffer;
        *(DWORD*)(sendObject + 0x08) = 0;
        *(DWORD*)(sendObject + 0x0C) = packetLength;
        *(DWORD*)(sendObject + 0x10) = packetLength;
        *(DWORD*)(sendObject + 0x14) = 0;
        *(DWORD*)(sendObject + 0x18) = packetBuffer;
        *(DWORD*)(sendObject + 0x1C) = *(DWORD*)((BYTE*)packet + 0x1C);
        *(DWORD*)(sendObject + 0x20) = *(DWORD*)((BYTE*)packet + 0x20);

        uint32_t result = sendPacket(connection, sendObject, 0);
        if (sendLogBudget >= 0)
            WriteLogFormat4("HermesBridge native v2 send result=%lu methodId=%lu requestId=%lu bytes=%lu", result, methodId, requestId, jsonLen);
        if (result == 0 && lstrcmpiA(method, "hermes.hello") == 0 && methodPayload && _strnicmp(methodPayload, "auto-handshake", 14) == 0)
        {
            InterlockedExchange(&g_playerReadySent, 1);
            WriteLog("HermesBridge auto handshake sent");
        }
        return result == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WriteLog("HermesBridge native send raised exception");
        return FALSE;
    }
}

static DWORD WINAPI SelfTestThread(LPVOID parameter)
{
    (void)parameter;
#if HERMES_ENABLE_STARTUP_SELF_TEST
    for (DWORD i = 0; i < 20; ++i)
    {
        if (InterlockedCompareExchange(&g_nativeSelfReady, 1, 1) == 1)
        {
            if (!g_nativeSelfTestSent)
            {
                g_nativeSelfTestSent = TRUE;
                SendHermesPayload("hello-from-hermes-dll");
            }
            return 0;
        }
        Sleep(1500);
    }
    WriteLog("HermesBridge native self test skipped: self not observed");
#endif
    return 0;
}

static DWORD WINAPI PlayerReadyThread(LPVOID parameter)
{
    (void)parameter;
    Sleep(30000);

    if (InterlockedCompareExchange(&g_nativeSelfReady, 1, 1) != 1)
    {
        WriteLog("HermesBridge player ready skipped: self not observed after delay");
        return 0;
    }

    if (InterlockedCompareExchange(&g_playerReadySent, 1, 0) == 0)
        SendHermesPayload("rpc hermes.hello player-entered-world");

    return 0;
}

static BOOL InstallLuaApi(void)
{
    const char* script1Core =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL.version = '" HERMES_BRIDGE_VERSION "'; "
        "HermesDLL.expectedWowSha256 = '" HERMES_EXPECTED_WOW_SHA256 "'; "
        "HermesDLL.CMSG_OPCODE = 0x0990; HermesDLL.SMSG_OPCODE = 0x0991; HermesDLL.protocolVersion = 2; HermesDLL.frameVersion = 2; "
        "HermesDLL.nativeHooks = HermesDLL.nativeHooks or {}; HermesDLL.nativeHooks.upperSend = true; HermesDLL.nativeHooks.send = true; HermesDLL.nativeHooks.recv = true; ";

    const char* script1State =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL.callbacks = HermesDLL.callbacks or {}; HermesDLL.events = HermesDLL.events or {}; HermesDLL.pending = HermesDLL.pending or {}; HermesDLL.pendingById = HermesDLL.pendingById or {}; HermesDLL.inbox = HermesDLL.inbox or {}; HermesDLL.streamQueue = HermesDLL.streamQueue or {}; HermesDLL.state = HermesDLL.state or {basic={},position={},vitals={},attributes={}}; HermesDLL.luaApiInstalls=(HermesDLL.luaApiInstalls or 0)+1; HermesDLL.stateUpdates = HermesDLL.stateUpdates or 0; HermesDLL.nextRequestId = HermesDLL.nextRequestId or 100000; HermesDLL.maxInbox = HermesDLL.maxInbox or 64; HermesDLL.maxPending = HermesDLL.maxPending or 128; HermesDLL.maxStreamQueue = HermesDLL.maxStreamQueue or 512; HermesDLL.maxPayloadSize = " HERMES_STRINGIFY(HERMES_LUA_MAX_REQUEST_PAYLOAD_SIZE) "; HermesDLL.maxFramePayloadSize = " HERMES_STRINGIFY(HERMES_FRAME_MAX_PAYLOAD_SIZE) "; HermesDLL.requestTimeout = HermesDLL.requestTimeout or 15; HermesDLL.callbackHits = HermesDLL.callbackHits or 0; HermesDLL.eventHits = HermesDLL.eventHits or 0; HermesDLL.streamDropped = HermesDLL.streamDropped or 0; HermesDLL.pumped = HermesDLL.pumped or 0; HermesDLL.addonCompatDispatches = HermesDLL.addonCompatDispatches or 0; HermesDLL.addonCompatErrors = HermesDLL.addonCompatErrors or 0; HermesDLL.addonCompatDebugSeq = HermesDLL.addonCompatDebugSeq or 0; HermesDLL.addonCompatServerTraceBudget = 8; HermesDLL.lifecycleTraceBudget = 8; HermesDLL.lastAddonCompatPrefix = HermesDLL.lastAddonCompatPrefix or nil; HermesDLL.lastAddonCompatCount = HermesDLL.lastAddonCompatCount or 0; HermesDLL.lastAddonCompatHandlerCount = HermesDLL.lastAddonCompatHandlerCount or 0; HermesDLL.lastAddonCompatHandlerOk = HermesDLL.lastAddonCompatHandlerOk or 0; HermesDLL.lastAddonCompatHandlerErrors = HermesDLL.lastAddonCompatHandlerErrors or 0; HermesDLL.lastAddonCompatPath = HermesDLL.lastAddonCompatPath or nil; HermesDLL.lastAddonCompatResult = HermesDLL.lastAddonCompatResult or nil; HermesDLL.lastAddonCompatPayloadBytes = HermesDLL.lastAddonCompatPayloadBytes or 0; HermesDLL.lastAddonCompatPayloadHead = HermesDLL.lastAddonCompatPayloadHead or nil; HermesDLL.lastHermesCompatPresent = HermesDLL.lastHermesCompatPresent or false; HermesDLL.lastHermesCompatHasDispatch = HermesDLL.lastHermesCompatHasDispatch or false; HermesDLL.lastHermesCompatHasHandlers = HermesDLL.lastHermesCompatHasHandlers or false; HermesDLL.lastError = nil; ";

    const char* script1Functions =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._Print(text) if DEFAULT_CHAT_FRAME then DEFAULT_CHAT_FRAME:AddMessage(tostring(text)); end end; "
        "HermesDLL.debug = HermesDLL.debug or false; "
        "function HermesDLL._DebugPrint(text) if HermesDLL.debug then HermesDLL._Print(text); end end; "
        "function HermesDLL._Now() if time then return time(); end return 0; end; "
        "function HermesDLL._NextRequestId() local id=HermesDLL.nextRequestId or 100000; HermesDLL.nextRequestId=id+1; if HermesDLL.nextRequestId>2147000000 then HermesDLL.nextRequestId=100000; end return id; end; "
        "function HermesDLL._TrimInbox() while table.getn(HermesDLL.inbox)>(HermesDLL.maxInbox or 64) do table.remove(HermesDLL.inbox,1); end end; "
        "function HermesDLL._TrimPending() while table.getn(HermesDLL.pending)>(HermesDLL.maxPending or 128) do local old=table.remove(HermesDLL.pending,1); if old and old.id then HermesDLL.pendingById[old.id]=nil; end end end; "
        "function HermesDLL._RemovePending(id) id=tonumber(id or 0); local pending=HermesDLL.pendingById[id]; if pending then HermesDLL.pendingById[id]=nil; for i=table.getn(HermesDLL.pending),1,-1 do local item=HermesDLL.pending[i]; if item and item.id==id then table.remove(HermesDLL.pending,i); break; end end end return pending; end; "
        "function HermesDLL.PrunePending(maxAge) local now=HermesDLL._Now(); local age=tonumber(maxAge or HermesDLL.requestTimeout or 15); local removed=0; for i=table.getn(HermesDLL.pending),1,-1 do local item=HermesDLL.pending[i]; if item and item.time and now-item.time>age then if item.id then HermesDLL.pendingById[item.id]=nil; end table.remove(HermesDLL.pending,i); removed=removed+1; end end return removed; end; "
        "function HermesDLL._SetError(code,message,details) local err=details or {}; err.hermesCode=tostring(code or 'UNKNOWN'); err.message=tostring(message or ''); err.time=HermesDLL._Now(); HermesDLL.lastError=err; return err; end; "
        "function HermesDLL._ClearError() HermesDLL.lastError=nil; end; "
        "function HermesDLL._RejectPayload(method,payloadLen,maxLen) HermesDLL._SetError('PAYLOAD_TOO_LARGE','Payload too large',{method=tostring(method or ''),payloadBytes=tonumber(payloadLen or 0) or 0,maxPayloadSize=tonumber(maxLen or 0) or 0}); HermesDLL._Print('|cffff3333HermesDLL.Request rejected|r hermesCode=PAYLOAD_TOO_LARGE method='..tostring(method)..' len='..tostring(payloadLen)..' max='..tostring(maxLen)); return nil; end; ";

    const char* script2 =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._JsonNumber(payload,key) local pat='\"'..key..'\"%s*:%s*(%d+)'; return tonumber(string.match(payload,pat) or nil); end; "
        "function HermesDLL._JsonString(payload,key) local pat='\"'..key..'\"%s*:%s*\"([^\"]*)\"'; return string.match(payload,pat); end; "
        "function HermesDLL._JsonBool(payload,key) local pat='\"'..key..'\"%s*:%s*(true)'; if string.match(payload,pat) then return true; end pat='\"'..key..'\"%s*:%s*(false)'; if string.match(payload,pat) then return false; end return nil; end; "
        "function HermesDLL._ParseJsonRpc(payload) payload=tostring(payload or ''); local id=HermesDLL._JsonNumber(payload,'id'); local method=HermesDLL._JsonString(payload,'method'); local event=HermesDLL._JsonString(payload,'event') or method; local err=string.find(payload, '\"error\"%s*:')~=nil; local res=string.find(payload, '\"result\"%s*:')~=nil; local params={event=event}; local nums={'schemaId','codec','messageType','methodId','requestId','sequence','payloadSize','payloadBytes','version','sourceRequestId','amount','flags','guidLow','accountId','level','race','class','teamId','mapId','zoneId','areaId','health','maxHealth','power','maxPower','powerType','money','transferId','chunkIndex','chunkCount','chunkFlags','dataLen'}; for _,k in ipairs(nums) do local v=HermesDLL._JsonNumber(payload,k); if v then params[k]=v; end end local strs={'schemaName','prefix','payload','payloadHex','marker','chunkPreview','guid','name','healthText','maxHealthText','powerText','maxPowerText','moneyText'}; for _,k in ipairs(strs) do local v=HermesDLL._JsonString(payload,k); if v then params[k]=v; end end local bools={'playerReady','alive','inCombat','healthExact','maxHealthExact','powerExact','maxPowerExact','moneyExact'}; for _,k in ipairs(bools) do local v=HermesDLL._JsonBool(payload,k); if v~=nil then params[k]=v; end end return { id=tonumber(id or 0), method=method, event=event, params=params, isError=err, hasResult=res, payload=payload, time=HermesDLL._Now() }; end; "
        "function HermesDLL._ResolveCallback(callback) if type(callback)=='function' then return callback; end if callback and _G[tostring(callback)] and type(_G[tostring(callback)])=='function' then return _G[tostring(callback)]; end return nil; end; "
        "function HermesDLL._CountMap(map) local n=0; if map then for _ in pairs(map) do n=n+1; end end return n; end; "
        "function HermesDLL._InvokeResponseCallback(callback, msg, pending) local fn=HermesDLL._ResolveCallback(callback); if not fn then return false; end local ok,err=pcall(fn,msg,pending); if not ok then HermesDLL._Print('|cffff3333HermesDLL callback error|r '..tostring(err)); return false; end HermesDLL.callbackHits=(HermesDLL.callbackHits or 0)+1; return true; end; "
        "function HermesDLL.NativeStatus() return { version=HermesDLL.version, frameVersion=HermesDLL.frameVersion, upperSendHook=HermesDLL.nativeHooks.upperSend, sendHook=HermesDLL.nativeHooks.send, recvHook=HermesDLL.nativeHooks.recv, recvPump='timer-delayed', cmsg=HermesDLL.CMSG_OPCODE, smsg=HermesDLL.SMSG_OPCODE, inbox=table.getn(HermesDLL.inbox), streamQueue=table.getn(HermesDLL.streamQueue), streamDropped=HermesDLL.streamDropped or 0, pumped=HermesDLL.pumped or 0, pending=table.getn(HermesDLL.pending), events=HermesDLL._CountMap(HermesDLL.events), nextRequestId=HermesDLL.nextRequestId, callbackHits=HermesDLL.callbackHits or 0, eventHits=HermesDLL.eventHits or 0, stateUpdates=HermesDLL.stateUpdates or 0, luaApiInstalls=HermesDLL.luaApiInstalls or 0, playerReady=(HermesDLL.state and HermesDLL.state.playerReady) or false, nativeSendQueue=" HERMES_STRINGIFY(HERMES_SEND_QUEUE_CAPACITY) ", nativeRecvQueue=" HERMES_STRINGIFY(HERMES_RECV_QUEUE_CAPACITY) ", maxPayloadSize=HermesDLL.maxPayloadSize, maxFramePayloadSize=HermesDLL.maxFramePayloadSize, addonCompatDispatches=HermesDLL.addonCompatDispatches or 0, addonCompatErrors=HermesDLL.addonCompatErrors or 0, addonCompatDebugSeq=HermesDLL.addonCompatDebugSeq or 0, lastAddonCompatPrefix=HermesDLL.lastAddonCompatPrefix, lastAddonCompatCount=HermesDLL.lastAddonCompatCount or 0, lastAddonCompatHandlerCount=HermesDLL.lastAddonCompatHandlerCount or 0, lastAddonCompatHandlerOk=HermesDLL.lastAddonCompatHandlerOk or 0, lastAddonCompatHandlerErrors=HermesDLL.lastAddonCompatHandlerErrors or 0, lastAddonCompatPath=HermesDLL.lastAddonCompatPath, lastAddonCompatResult=HermesDLL.lastAddonCompatResult, lastAddonCompatPayloadBytes=HermesDLL.lastAddonCompatPayloadBytes or 0, lastAddonCompatPayloadHead=HermesDLL.lastAddonCompatPayloadHead, lastHermesCompatPresent=HermesDLL.lastHermesCompatPresent or false, lastHermesCompatHasDispatch=HermesDLL.lastHermesCompatHasDispatch or false, lastHermesCompatHasHandlers=HermesDLL.lastHermesCompatHasHandlers or false, lastAddonCompatError=HermesDLL.lastAddonCompatError, lastErrorCode=HermesDLL.lastError and HermesDLL.lastError.hermesCode or nil }; end; "
        "function HermesDLL.Ping() HermesDLL._Print('|cff00ff00HermesDLL Ping OK v'..HermesDLL.version..'|r'); return 'pong:'..HermesDLL.version; end; "
        "function HermesDLL.ClearInbox() local n=table.getn(HermesDLL.inbox); HermesDLL.inbox={}; HermesDLL.lastReceive=nil; return n; end; ";

    const char* script2Attributes =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL._ParseJsonRpcBase = HermesDLL._ParseJsonRpcBase or HermesDLL._ParseJsonRpc; "
        "function HermesDLL._ParseJsonRpc(payload) local msg=HermesDLL._ParseJsonRpcBase(payload); local p=msg.params or {}; msg.params=p; local n=HermesDLL._JsonNumber(payload,'attributeCount'); if n then p.attributeCount=n; end local keys={'attributesPayload','targetPayload','source','schema','module','status','requestToken'}; for _,k in ipairs(keys) do local v=HermesDLL._JsonString(payload,k); if v then p[k]=v; end end if string.find(payload,'\"actions\"%s*:') then p.actions=true; end return msg; end; ";

    const char* script3Events =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL.On(event, callback) event=tostring(event or ''); if event=='' or not callback then return false; end HermesDLL.events[event]=callback; HermesDLL._DebugPrint('|cff66ccffHermesDLL.On|r '..event); return true; end; "
        "function HermesDLL.Off(event) event=tostring(event or ''); HermesDLL.events[event]=nil; return true; end; "
        "function HermesDLL.Emit(event, msg, pending) event=tostring(event or ''); local fn=HermesDLL._ResolveCallback(HermesDLL.events[event]); if not fn then return false; end local ok,err=pcall(fn,msg,pending,event); if not ok then HermesDLL._Print('|cffff3333HermesDLL event error|r '..event..' '..tostring(err)); return false; end HermesDLL.eventHits=(HermesDLL.eventHits or 0)+1; return true; end; ";

    const char* script3Request =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL.Request(method, payload, callback) method=tostring(method or 'hermes.ping'); payload=tostring(payload or ''); local payloadLen=string.len(payload); local maxLen=tonumber(HermesDLL.maxPayloadSize or " HERMES_STRINGIFY(HERMES_LUA_MAX_REQUEST_PAYLOAD_SIZE) ") or " HERMES_STRINGIFY(HERMES_LUA_MAX_REQUEST_PAYLOAD_SIZE) "; if payloadLen>maxLen then return HermesDLL._RejectPayload(method,payloadLen,maxLen); end if type(SendAddonMessage)~='function' then HermesDLL._SetError('SEND_ADDON_UNAVAILABLE','SendAddonMessage unavailable',{method=method,payloadBytes=payloadLen}); HermesDLL._Print('|cffff3333HermesDLL.Request deferred|r SendAddonMessage unavailable method='..method); return nil; end HermesDLL._ClearError(); HermesDLL.PrunePending(); local id=HermesDLL._NextRequestId(); local req={id=id,channel='rpc',method=method,payload=payload,callback=callback,time=HermesDLL._Now()}; HermesDLL.pendingById[id]=req; table.insert(HermesDLL.pending,req); HermesDLL.lastSend=req; HermesDLL._TrimPending(); local target=(UnitName and UnitName('player')) or nil; SendAddonMessage('HERMESDLL', 'rpcid '..id..' '..method..' '..payload, 'WHISPER', target); HermesDLL._DebugPrint('|cff66ccffHermesDLL.Request v2 queued|r id='..id..' '..method..' len='..payloadLen); return id; end; "
        "function HermesDLL.Send(channel, payload, callback) channel=tostring(channel or 'debug'); payload=tostring(payload or ''); if string.len(channel)>64 then channel=string.sub(channel,1,64); end local id=HermesDLL.Request('hermes.ping', payload, callback); if not id then HermesDLL._Print('|cffff3333HermesDLL.Send rejected|r '..channel..' code='..tostring(HermesDLL.lastError and HermesDLL.lastError.hermesCode)); return nil; end local req=HermesDLL.pendingById[id]; if req then req.channel=channel; end HermesDLL._DebugPrint('|cff66ccffHermesDLL.Send v2 queued|r id='..id..' '..channel..' len='..string.len(payload)); return id; end; "
        "function HermesDLL.Register(channel, callbackName) channel=tostring(channel or 'default'); callbackName=tostring(callbackName or ''); HermesDLL.callbacks[channel]=callbackName; HermesDLL._DebugPrint('|cff66ccffHermesDLL.Register|r '..channel..' -> '..callbackName); return true; end; "
        "function HermesDLL.Dispatch(channel, payload) channel=tostring(channel or 'default'); payload=tostring(payload or ''); local fn=HermesDLL._ResolveCallback(HermesDLL.callbacks[channel]); if fn then local ok,err=pcall(fn,channel,payload); if not ok then HermesDLL._Print('|cffff3333HermesDLL dispatch error|r '..tostring(err)); return false; end return true; end HermesDLL._DebugPrint('|cff66ccffHermesDLL.Dispatch|r '..channel..' len='..string.len(payload)); return false; end; ";

    const char* script4Queue =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._TrimStreamQueue() while table.getn(HermesDLL.streamQueue)>(HermesDLL.maxStreamQueue or 512) do table.remove(HermesDLL.streamQueue,1); HermesDLL.streamDropped=(HermesDLL.streamDropped or 0)+1; end end; "
        "function HermesDLL._QueueStreamMessage(msg) table.insert(HermesDLL.streamQueue,msg); HermesDLL._TrimStreamQueue(); HermesDLL.lastStream=msg; return true; end; ";
    const char* addonCompatLua =
        "function HermesDLL._DispatchAddonCompat(msg) local p=msg and msg.params or {}; local prefix=tostring(p.prefix or ''); local payload=tostring(p.payload or ''); if prefix=='' or HermesDLL.addonCompatDispatching then return false; end local channel='WHISPER'; local sender=(UnitName and UnitName('player')) or nil; if sender=='' then sender=nil; end local count=0; local function noteError(err) HermesDLL.addonCompatErrors=(HermesDLL.addonCompatErrors or 0)+1; HermesDLL.lastAddonCompatError=tostring(err); end; local function callKnownClient() if prefix=='PLUGMGR' and _G and _G.PluginManagerClient and _G.PluginManagerClient.DispatchMessage then local ok,result=pcall(_G.PluginManagerClient.DispatchMessage,_G.PluginManagerClient,payload); if ok then if result then count=count+1; end else noteError(result); end end end; local function callFrame(frame) if frame and frame.GetScript then local okHandler,handler=pcall(frame.GetScript,frame,'OnEvent'); if okHandler and handler then local oldThis,oldEvent,oldArg1,oldArg2,oldArg3,oldArg4=this,event,arg1,arg2,arg3,arg4; this=frame; event='CHAT_MSG_ADDON'; arg1=prefix; arg2=payload; arg3=channel; arg4=sender; local ok,err=pcall(handler,frame,'CHAT_MSG_ADDON',prefix,payload,channel,sender); this=oldThis; event=oldEvent; arg1=oldArg1; arg2=oldArg2; arg3=oldArg3; arg4=oldArg4; count=count+1; if not ok then noteError(err); end end end end HermesDLL.addonCompatDispatching=true; HermesDLL.lastAddonCompatPrefix=prefix; callKnownClient(); if EnumerateFrames then local frame=EnumerateFrames(); while frame do local registered=false; if frame.IsEventRegistered then local ok,value=pcall(frame.IsEventRegistered,frame,'CHAT_MSG_ADDON'); registered=ok and value; end if registered then callFrame(frame); end frame=EnumerateFrames(frame); end if count==0 then frame=EnumerateFrames(); while frame do callFrame(frame); frame=EnumerateFrames(frame); end end end HermesDLL.addonCompatDispatching=false; HermesDLL.lastAddonCompatCount=count; HermesDLL.addonCompatByPrefix=HermesDLL.addonCompatByPrefix or {}; HermesDLL.addonCompatByPrefix[prefix]=(HermesDLL.addonCompatByPrefix[prefix] or 0)+count; HermesDLL.addonCompatDispatches=(HermesDLL.addonCompatDispatches or 0)+count; return count>0; end; ";
    const char* addonCompatFastLua =
        "HermesDLL.addonCompatFrames=HermesDLL.addonCompatFrames or nil; HermesDLL.addonCompatAllFrames=HermesDLL.addonCompatAllFrames or nil; HermesDLL.addonCompatFrameCacheUntil=HermesDLL.addonCompatFrameCacheUntil or 0; "
        "function HermesDLL._RefreshAddonCompatFrames(force) local now=(GetTime and GetTime()) or 0; if not force and HermesDLL.addonCompatFrames and now<(HermesDLL.addonCompatFrameCacheUntil or 0) then return HermesDLL.addonCompatFrames,HermesDLL.addonCompatAllFrames or {}; end local frames={}; local all={}; if EnumerateFrames then local frame=EnumerateFrames(); while frame do table.insert(all,frame); local registered=false; if frame.IsEventRegistered then local ok,value=pcall(frame.IsEventRegistered,frame,'CHAT_MSG_ADDON'); registered=ok and value; end if registered then table.insert(frames,frame); end frame=EnumerateFrames(frame); end end HermesDLL.addonCompatFrames=frames; HermesDLL.addonCompatAllFrames=all; HermesDLL.addonCompatFrameCacheUntil=now+2; HermesDLL.addonCompatFrameCacheSize=table.getn(frames); return frames,all; end; "
        "function HermesDLL._DispatchAddonCompat(msg) local p=msg and msg.params or {}; local prefix=tostring(p.prefix or ''); local payload=tostring(p.payload or ''); if prefix=='' or HermesDLL.addonCompatDispatching then return false; end local channel='WHISPER'; local sender=(UnitName and UnitName('player')) or nil; if sender=='' then sender=nil; end local count=0; local function noteError(err) HermesDLL.addonCompatErrors=(HermesDLL.addonCompatErrors or 0)+1; HermesDLL.lastAddonCompatError=tostring(err); end; local function callKnownClient() if prefix=='PLUGMGR' and _G and _G.PluginManagerClient and _G.PluginManagerClient.DispatchMessage then local ok,result=pcall(_G.PluginManagerClient.DispatchMessage,_G.PluginManagerClient,payload); if ok then if result then count=count+1; end else noteError(result); end end end; local function callFrame(frame) if frame and frame.GetScript then local okHandler,handler=pcall(frame.GetScript,frame,'OnEvent'); if okHandler and handler then local oldThis,oldEvent,oldArg1,oldArg2,oldArg3,oldArg4=this,event,arg1,arg2,arg3,arg4; this=frame; event='CHAT_MSG_ADDON'; arg1=prefix; arg2=payload; arg3=channel; arg4=sender; local ok,err=pcall(handler,frame,'CHAT_MSG_ADDON',prefix,payload,channel,sender); this=oldThis; event=oldEvent; arg1=oldArg1; arg2=oldArg2; arg3=oldArg3; arg4=oldArg4; count=count+1; if not ok then noteError(err); end end end end HermesDLL.addonCompatDispatching=true; HermesDLL.lastAddonCompatPrefix=prefix; callKnownClient(); local frames,all=HermesDLL._RefreshAddonCompatFrames(false); for i=1,table.getn(frames) do callFrame(frames[i]); end if count==0 then frames,all=HermesDLL._RefreshAddonCompatFrames(true); for i=1,table.getn(all) do callFrame(all[i]); end end HermesDLL.addonCompatDispatching=false; HermesDLL.lastAddonCompatCount=count; HermesDLL.addonCompatByPrefix=HermesDLL.addonCompatByPrefix or {}; HermesDLL.addonCompatByPrefix[prefix]=(HermesDLL.addonCompatByPrefix[prefix] or 0)+count; HermesDLL.addonCompatDispatches=(HermesDLL.addonCompatDispatches or 0)+count; return count>0; end; ";

    const char* addonCompatDirectLua =
        "function HermesDLL._DispatchAddonCompat(msg) local p=msg and msg.params or {}; local prefix=tostring(p.prefix or ''); local payload=tostring(p.payload or ''); if prefix=='' or HermesDLL.addonCompatDispatching then return false; end local channel='WHISPER'; local sender=(UnitName and UnitName('player')) or nil; if sender=='' then sender=nil; end local count=0; "
        "local handlerTotal=0; local handlerOk=0; local handlerErrors=0; local frameCalls=0; "
        "local function noteError(err) HermesDLL.addonCompatErrors=(HermesDLL.addonCompatErrors or 0)+1; handlerErrors=handlerErrors+1; HermesDLL.lastAddonCompatError=tostring(err); end; "
        "local function markPath(path) path=tostring(path or ''); if path=='' then return; end if HermesDLL.lastAddonCompatPath and HermesDLL.lastAddonCompatPath~='' then HermesDLL.lastAddonCompatPath=HermesDLL.lastAddonCompatPath..'+'..path; else HermesDLL.lastAddonCompatPath=path; end end; "
        "local function listCount(list) if type(list)~='table' then return 0; end local n=table.getn(list); if n and n>0 then return n; end local c=0; for _,fn in pairs(list) do if type(fn)=='function' then c=c+1; end end return c; end; "
        "local function callHandler(fn,label) if type(fn)~='function' then noteError(tostring(label)..':not_function'); return false; end local ok,err=pcall(fn,prefix,payload,channel,sender); if ok then count=count+1; handlerOk=handlerOk+1; return true; end noteError(tostring(label)..':'..tostring(err)); return false; end; "
        "local function callHermesCompatHandlers() local compat=(_G and _G.HermesCompat) or nil; HermesDLL.lastHermesCompatPresent=type(compat)=='table'; HermesDLL.lastHermesCompatHasDispatch=HermesDLL.lastHermesCompatPresent and type(compat.Dispatch)=='function' or false; HermesDLL.lastHermesCompatHasHandlers=HermesDLL.lastHermesCompatPresent and type(compat.handlers)=='table' or false; if not HermesDLL.lastHermesCompatHasHandlers then return false; end local list=compat.handlers[prefix]; handlerTotal=listCount(list); HermesDLL.lastAddonCompatHandlerCount=handlerTotal; if handlerTotal<=0 or type(list)~='table' then return false; end markPath('handlers'); local n=table.getn(list); if n and n>0 then for i=1,n do callHandler(list[i],'handler#'..i); end else local i=0; for _,fn in pairs(list) do i=i+1; callHandler(fn,'handler@'..i); end end return true; end; "
        "local function callHermesCompatDispatch() local compat=(_G and _G.HermesCompat) or nil; if type(compat)=='table' and type(compat.Dispatch)=='function' then HermesDLL.lastHermesCompatDispatchCalled=true; local ok,result=pcall(compat.Dispatch,prefix,payload,channel,sender); HermesDLL.lastHermesCompatDispatchResult=tostring(result); if ok then if result then count=count+1; markPath('dispatch'); return true; end return false; end noteError('dispatch:'..tostring(result)); end return false; end; "
        "local function callKnownClient() if prefix=='PLUGMGR' and _G and _G.PluginManagerClient and _G.PluginManagerClient.DispatchMessage then HermesDLL.lastKnownClientCalled=true; local ok,result=pcall(_G.PluginManagerClient.DispatchMessage,_G.PluginManagerClient,payload); HermesDLL.lastKnownClientResult=tostring(result); if ok then if result then count=count+1; markPath('known'); return true; end return false; end noteError('known:'..tostring(result)); end return false; end; "
        "local function callFrame(frame) if frame and frame.GetScript then local okHandler,handler=pcall(frame.GetScript,frame,'OnEvent'); if okHandler and handler then local oldThis,oldEvent,oldArg1,oldArg2,oldArg3,oldArg4=this,event,arg1,arg2,arg3,arg4; this=frame; event='CHAT_MSG_ADDON'; arg1=prefix; arg2=payload; arg3=channel; arg4=sender; local ok,err=pcall(handler,frame,'CHAT_MSG_ADDON',prefix,payload,channel,sender); this=oldThis; event=oldEvent; arg1=oldArg1; arg2=oldArg2; arg3=oldArg3; arg4=oldArg4; frameCalls=frameCalls+1; if ok then count=count+1; markPath('frame'); else noteError('frame:'..tostring(err)); end end end end; "
        "local function emitServerTrace() local b=tonumber(HermesDLL.addonCompatServerTraceBudget or 0) or 0; if b<=0 or type(SendAddonMessage)~='function' then return; end HermesDLL.addonCompatServerTraceBudget=b-1; local path=tostring(HermesDLL.lastAddonCompatPath or 'none'); local result=tostring(HermesDLL.lastAddonCompatResult or 'unknown'); local head=string.sub(payload,1,48); local trace='addon-compat seq='..tostring(HermesDLL.addonCompatDebugSeq or 0)..' prefix='..prefix..' result='..result..' path='..path..' count='..tostring(count)..' handlers='..tostring(handlerOk)..'/'..tostring(handlerTotal)..' errors='..tostring(handlerErrors)..' compat='..tostring(HermesDLL.lastHermesCompatPresent)..' dispatch='..tostring(HermesDLL.lastHermesCompatHasDispatch)..' bytes='..tostring(string.len(payload))..' head='..head; local target=(UnitName and UnitName('player')) or nil; pcall(SendAddonMessage,'HERMESDLL','rpc hermes.ping '..trace,'WHISPER',target); end; "
        "HermesDLL.addonCompatDispatching=true; HermesDLL.addonCompatDebugSeq=(HermesDLL.addonCompatDebugSeq or 0)+1; HermesDLL.lastAddonCompatPrefix=prefix; HermesDLL.lastAddonCompatPayloadBytes=string.len(payload); HermesDLL.lastAddonCompatPayloadHead=string.sub(payload,1,96); HermesDLL.lastAddonCompatCount=0; HermesDLL.lastAddonCompatHandlerCount=0; HermesDLL.lastAddonCompatHandlerOk=0; HermesDLL.lastAddonCompatHandlerErrors=0; HermesDLL.lastAddonCompatFrameCalls=0; HermesDLL.lastAddonCompatPath=''; HermesDLL.lastAddonCompatResult='running'; HermesDLL.lastAddonCompatError=nil; HermesDLL.lastHermesCompatDispatchCalled=false; HermesDLL.lastHermesCompatDispatchResult=nil; HermesDLL.lastKnownClientCalled=false; HermesDLL.lastKnownClientResult=nil; local sawHandlers=callHermesCompatHandlers(); if not sawHandlers and count==0 then callHermesCompatDispatch(); end if count==0 then callKnownClient(); end local frames,all={},{}; if count==0 and HermesDLL._RefreshAddonCompatFrames then frames,all=HermesDLL._RefreshAddonCompatFrames(false); for i=1,table.getn(frames or {}) do callFrame(frames[i]); end end if count==0 and HermesDLL._RefreshAddonCompatFrames then frames,all=HermesDLL._RefreshAddonCompatFrames(true); for i=1,table.getn(all or {}) do callFrame(all[i]); end end HermesDLL.addonCompatDispatching=false; HermesDLL.lastAddonCompatCount=count; HermesDLL.lastAddonCompatHandlerCount=handlerTotal; HermesDLL.lastAddonCompatHandlerOk=handlerOk; HermesDLL.lastAddonCompatHandlerErrors=handlerErrors; HermesDLL.lastAddonCompatFrameCalls=frameCalls; if not HermesDLL.lastAddonCompatPath or HermesDLL.lastAddonCompatPath=='' then HermesDLL.lastAddonCompatPath='none'; end HermesDLL.lastAddonCompatResult=(count>0 and 'handled' or 'miss'); HermesDLL.addonCompatByPrefix=HermesDLL.addonCompatByPrefix or {}; HermesDLL.addonCompatByPrefix[prefix]=(HermesDLL.addonCompatByPrefix[prefix] or 0)+count; HermesDLL.addonCompatDispatches=(HermesDLL.addonCompatDispatches or 0)+count; emitServerTrace(); return count>0; end; ";
    const char* script4Pump =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL.bulkTransfers = HermesDLL.bulkTransfers or {}; "
        "function HermesDLL._HandleBulkChunk(msg,pending) local p=msg and msg.params; if not p or p.schemaName~='bulk.chunk.v1' or tonumber(p.messageType or 0)~=6 then return false; end local transferId=tostring(p.transferId or p.sourceRequestId or 0); local chunkCount=tonumber(p.chunkCount or 0) or 0; local chunkIndex=tonumber(p.chunkIndex or 0) or 0; local data=tostring(p.chunkPreview or ''); local e=HermesDLL.bulkTransfers[transferId]; if not e then e={parts={},received=0,count=chunkCount,marker=p.marker,time=HermesDLL._Now()}; HermesDLL.bulkTransfers[transferId]=e; end if not e.parts[chunkIndex+1] then e.received=e.received+1; end e.parts[chunkIndex+1]=data; if chunkCount>0 then e.count=chunkCount; end if p.marker then e.marker=p.marker; end if e.count and e.count>0 and e.received>=e.count then local joined={}; for i=1,e.count do joined[i]=e.parts[i] or ''; end p.data=table.concat(joined,''); p.complete=true; p.marker=p.marker or e.marker; HermesDLL.bulkTransfers[transferId]=nil; HermesDLL.lastBulkComplete=msg; HermesDLL.bulkCompleteHits=(HermesDLL.bulkCompleteHits or 0)+1; msg.bulkCompleteEvent='bulk.complete'; return false; end return false; end; "
        "function HermesDLL._HandleMessage(msg,pending) local handled=false; if HermesDLL._UpdateStateFromMessage then HermesDLL._UpdateStateFromMessage(msg,pending); end if HermesDLL._HandleBulkChunk then handled=HermesDLL._HandleBulkChunk(msg,pending) or handled; end if pending then handled=HermesDLL._InvokeResponseCallback(pending.callback,msg,pending) or handled; if pending.method then handled=HermesDLL.Emit(pending.method,msg,pending) or handled; end handled=HermesDLL.Emit('response',msg,pending) or handled; if msg.isError then handled=HermesDLL.Emit('error',msg,pending) or handled; end else local eventName=msg.event or msg.method; if eventName=='hermes.addon.message' and HermesDLL._DispatchAddonCompat then handled=HermesDLL._DispatchAddonCompat(msg) or handled; end if eventName and eventName~='' then handled=HermesDLL.Emit(eventName,msg,nil) or handled; end local bulkEvent=msg.bulkCompleteEvent; if bulkEvent and bulkEvent~='' then handled=HermesDLL.Emit(bulkEvent,msg,nil) or handled; end handled=HermesDLL.Emit(msg.isError and 'error' or 'message',msg,nil) or handled; end if not handled and msg.hasResult and not msg.isError then handled=true; end if not handled then HermesDLL.Dispatch(msg.channel or 'default',msg.payload or ''); end return handled; end; "
        "function HermesDLL.Pump(maxEvents,maxMs) local limit=tonumber(maxEvents or 64) or 64; if limit<1 then limit=1; end if limit>256 then limit=256; end local deadline=nil; local budget=tonumber(maxMs or 8) or 8; if GetTime and budget>0 then deadline=GetTime()+(budget/1000); end local n=0; while n<limit and table.getn(HermesDLL.streamQueue)>0 do if deadline and GetTime and GetTime()>deadline then break; end local msg=table.remove(HermesDLL.streamQueue,1); HermesDLL._HandleMessage(msg,nil); n=n+1; end HermesDLL.pumped=(HermesDLL.pumped or 0)+n; return n; end; ";

    const char* script4AddonDispatchOverride =
        "function HermesDLL._HandleMessage(msg,pending) local handled=false; if HermesDLL._UpdateStateFromMessage then HermesDLL._UpdateStateFromMessage(msg,pending); end if HermesDLL._HandleBulkChunk then handled=HermesDLL._HandleBulkChunk(msg,pending) or handled; end if pending then handled=HermesDLL._InvokeResponseCallback(pending.callback,msg,pending) or handled; if pending.method then handled=HermesDLL.Emit(pending.method,msg,pending) or handled; end handled=HermesDLL.Emit('response',msg,pending) or handled; if msg.isError then handled=HermesDLL.Emit('error',msg,pending) or handled; end else local eventName=msg.event or msg.method; if eventName=='hermes.addon.message' then if HermesDLL._DispatchAddonCompat then handled=HermesDLL._DispatchAddonCompat(msg) or handled; end if not handled then handled=HermesDLL.Emit(eventName,msg,nil) or handled; end elseif eventName and eventName~='' then handled=HermesDLL.Emit(eventName,msg,nil) or handled; end local bulkEvent=msg.bulkCompleteEvent; if bulkEvent and bulkEvent~='' then handled=HermesDLL.Emit(bulkEvent,msg,nil) or handled; end handled=HermesDLL.Emit(msg.isError and 'error' or 'message',msg,nil) or handled; end if not handled and msg.hasResult and not msg.isError then handled=true; end if not handled then HermesDLL.Dispatch(msg.channel or 'default',msg.payload or ''); end return handled; end; ";
    const char* script4Native =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL.recvChunks = HermesDLL.recvChunks or {}; "
        "function HermesDLL._NativeReceiveChunk(token,channel,part,done) token=tostring(token or '0'); HermesDLL.recvChunks=HermesDLL.recvChunks or {}; local e=HermesDLL.recvChunks[token]; if not e then e={parts={},n=0,channel=channel,time=HermesDLL._Now()}; HermesDLL.recvChunks[token]=e; end e.n=e.n+1; e.parts[e.n]=tostring(part or ''); if done then local payload=table.concat(e.parts,''); local lane=e.channel or channel; HermesDLL.recvChunks[token]=nil; return HermesDLL._NativeReceive(lane,payload); end return true; end; "
        "function HermesDLL._NativeReceive(channel, payload) local lane=tonumber(channel or 0) or 0; payload=tostring(payload or ''); local msg=HermesDLL._ParseJsonRpc(payload); msg.channel=lane; table.insert(HermesDLL.inbox,msg); HermesDLL._TrimInbox(); HermesDLL.lastReceive=msg; local pending=nil; if msg.id and msg.id>0 then pending=HermesDLL._RemovePending(msg.id); msg.pending=pending; end if lane==1 or pending then HermesDLL._HandleMessage(msg,pending); else local eventName=msg.event or msg.method; if eventName=='hermes.addon.message' then HermesDLL._HandleMessage(msg,nil); else HermesDLL._QueueStreamMessage(msg); end end return true; end; "
        "function HermesDLL.Pop() local n=table.getn(HermesDLL.inbox); if n==0 then return nil; end local msg=HermesDLL.inbox[1]; table.remove(HermesDLL.inbox,1); return msg; end; ";

    const char* script4Slash =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._EnsureSlash() if type(SlashCmdList)~='table' then HermesDLL.slashInstallPending=true; return false; end SLASH_HERMESDLL1='/hdll'; SlashCmdList['HERMESDLL']=function(msg) msg=tostring(msg or ''); if msg=='' or msg=='ping' then HermesDLL.Ping(); return; end if msg=='clear' then local n=HermesDLL.ClearInbox(); HermesDLL._Print('|cffffff00HermesDLL cleared inbox|r '..n); return; end if msg=='pump' then local n=HermesDLL.Pump(64,8); HermesDLL._Print('|cffffff00HermesDLL pumped|r '..n); return; end if msg=='panel' or msg=='debug' or msg=='debug panel' then if HermesDLL.ToggleDebugPanel then HermesDLL.ToggleDebugPanel(); else HermesDLL._Print('|cffff3333HermesDLL debug panel not installed|r'); end return; end if string.sub(msg,1,5)=='send ' then HermesDLL.Send('debug', string.sub(msg,6)); return; end if string.sub(msg,1,4)=='rpc ' then local rest=string.sub(msg,5); local sp=string.find(rest,' '); if sp then HermesDLL.Request(string.sub(rest,1,sp-1), string.sub(rest,sp+1)); else HermesDLL.Request(rest,''); end return; end if msg=='inbox' then local n=table.getn(HermesDLL.inbox); HermesDLL._Print('|cffffff00HermesDLL inbox|r '..n); if n>0 then local m=HermesDLL.inbox[n]; HermesDLL._Print('|cff66ccfflast|r id='..tostring(m.id)..' lane='..tostring(m.channel)..' err='..tostring(m.isError)..' len='..string.len(tostring(m.payload))..' '..tostring(m.payload)); end return; end if msg=='state' then local s=HermesDLL.NativeStatus(); HermesDLL._Print('|cffffff00HermesDLL|r version='..s.version..' frame='..s.frameVersion..' upper='..tostring(s.upperSendHook)..' send='..tostring(s.sendHook)..' recv='..tostring(s.recvHook)..' pending='..s.pending..' inbox='..s.inbox..' stream='..s.streamQueue..' pumped='..s.pumped); HermesDLL._Print('|cffffff00HermesDLL addon|r seq='..tostring(s.addonCompatDebugSeq)..' prefix='..tostring(s.lastAddonCompatPrefix)..' result='..tostring(s.lastAddonCompatResult)..' path='..tostring(s.lastAddonCompatPath)..' count='..tostring(s.lastAddonCompatCount)..' handlers='..tostring(s.lastAddonCompatHandlerOk)..'/'..tostring(s.lastAddonCompatHandlerCount)..' handlerErrors='..tostring(s.lastAddonCompatHandlerErrors)..' compat='..tostring(s.lastHermesCompatPresent)..' dispatch='..tostring(s.lastHermesCompatHasDispatch)..' handlersTable='..tostring(s.lastHermesCompatHasHandlers)..' bytes='..tostring(s.lastAddonCompatPayloadBytes)); HermesDLL._Print('|cffffff00HermesDLL addon payload|r '..tostring(s.lastAddonCompatPayloadHead)..' error='..tostring(s.lastAddonCompatError)); return; end HermesDLL._Print('|cffffff00/hdll ping|r, |cffffff00/hdll send text|r, |cffffff00/hdll rpc method payload|r, |cffffff00/hdll pump|r, |cffffff00/hdll inbox|r, |cffffff00/hdll clear|r, |cffffff00/hdll state|r, |cffffff00/hdll panel|r'); end; HermesDLL.slashInstallPending=false; return true; end; HermesDLL._EnsureSlash(); ";

    WriteLog("installing HermesDLL Lua API v" HERMES_BRIDGE_VERSION);
    int result1Core = ExecuteLua(script1Core, "HermesBridgeBootstrapCore");
    WriteLogFormat("HermesDLL bridge core install returned %lu", (DWORD)result1Core);
    int result1State = (result1Core == 0) ? ExecuteLua(script1State, "HermesBridgeBootstrapCoreState") : -1;
    WriteLogFormat("HermesDLL bridge core state install returned %lu", (DWORD)result1State);
    int result1Functions = (result1State == 0) ? ExecuteLua(script1Functions, "HermesBridgeBootstrapCoreFunctions") : -1;
    WriteLogFormat("HermesDLL bridge core functions install returned %lu", (DWORD)result1Functions);
    int result2 = (result1Functions == 0) ? ExecuteLua(script2, "HermesBridgeBootstrapJson") : -1;
    WriteLogFormat("HermesDLL bridge json install returned %lu", (DWORD)result2);
    int result2Attributes = (result2 == 0) ? ExecuteLua(script2Attributes, "HermesBridgeBootstrapAttributeJson") : -1;
    WriteLogFormat("HermesDLL bridge attribute json install returned %lu", (DWORD)result2Attributes);
    int result3Events = (result2Attributes == 0) ? ExecuteLua(script3Events, "HermesBridgeBootstrapRpcEvents") : -1;
    WriteLogFormat("HermesDLL bridge rpc events install returned %lu", (DWORD)result3Events);
    int result3Request = (result3Events == 0) ? ExecuteLua(script3Request, "HermesBridgeBootstrapRpcRequest") : -1;
    WriteLogFormat("HermesDLL bridge rpc request install returned %lu", (DWORD)result3Request);
    int result4Queue = (result3Request == 0) ? ExecuteLua(script4Queue, "HermesBridgeBootstrapPumpQueue") : -1;
    WriteLogFormat("HermesDLL bridge pump queue install returned %lu", (DWORD)result4Queue);
    int result4AddonCompat = (result4Queue == 0) ? ExecuteLua(addonCompatLua, "HermesBridgeBootstrapAddonCompat") : -1;
    if (result4AddonCompat == 0)
        result4AddonCompat = ExecuteLua(addonCompatFastLua, "HermesBridgeBootstrapAddonCompatFast");
    if (result4AddonCompat == 0)
        result4AddonCompat = ExecuteLua(addonCompatDirectLua, "HermesBridgeBootstrapAddonCompatDirect");
    WriteLogFormat("HermesDLL bridge addon compat install returned %lu", (DWORD)result4AddonCompat);
    int result4Pump = (result4AddonCompat == 0) ? ExecuteLua(script4Pump, "HermesBridgeBootstrapPumpDispatch") : -1;
    if (result4Pump == 0)
        result4Pump = ExecuteLua(script4AddonDispatchOverride, "HermesBridgeBootstrapAddonDispatchOverride");
    WriteLogFormat("HermesDLL bridge pump dispatch install returned %lu", (DWORD)result4Pump);
    int result4Native = (result4Pump == 0) ? ExecuteLua(script4Native, "HermesBridgeBootstrapPumpNative") : -1;
    WriteLogFormat("HermesDLL bridge pump native install returned %lu", (DWORD)result4Native);
    int result4Slash = (result4Native == 0) ? ExecuteLua(script4Slash, "HermesBridgeBootstrapSlash") : -1;
    WriteLogFormat("HermesDLL bridge slash install returned %lu", (DWORD)result4Slash);
    if (result1Core == 0 && result1State == 0 && result1Functions == 0 && result2 == 0 && result2Attributes == 0 && result3Events == 0 && result3Request == 0 && result4Queue == 0 && result4AddonCompat == 0 && result4Pump == 0 && result4Native == 0 && result4Slash == 0)
        return TRUE;

    WriteLog("HermesDLL Lua API install failed; bootstrap will retry");
    return FALSE;
}

static BOOL InstallLuaStateApi(void)
{
    const char* script =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL.state = HermesDLL.state or {basic={},position={},vitals={},attributes={}}; HermesDLL.stateUpdates = HermesDLL.stateUpdates or 0; "
        "function HermesDLL._CopyFields(dst,src,keys) if not dst or not src then return; end for _,k in ipairs(keys) do if src[k]~=nil then dst[k]=src[k]; end end end; "
        "function HermesDLL._UpdateStateFromMessage(msg,pending) if not msg then return false; end HermesDLL.state=HermesDLL.state or {basic={},position={},vitals={},attributes={}}; local s=HermesDLL.state; local p=msg.params or {}; local method=(pending and pending.method) or msg.method or msg.event or ''; local changed=false; local basic=false; local position=false; local vitals=false; local attributes=false; if p.playerReady~=nil then s.playerReady=p.playerReady; changed=true; end if method=='hermes.hello' or method=='player.getBasicInfo' or method=='player.getSnapshot' then s.basic=s.basic or {}; HermesDLL._CopyFields(s.basic,p,{'accountId','guid','name','level','race','class','teamId'}); changed=true; basic=true; end if method=='player.getPosition' or method=='player.getSnapshot' then s.position=s.position or {}; HermesDLL._CopyFields(s.position,p,{'mapId','zoneId','areaId'}); changed=true; position=true; end if method=='player.getVitals' or method=='player.getSnapshot' or p.schemaName=='unit.vitals.snapshot.v1' then s.vitals=s.vitals or {}; HermesDLL._CopyFields(s.vitals,p,{'alive','inCombat','health','healthText','healthExact','maxHealth','maxHealthText','maxHealthExact','powerType','power','powerText','powerExact','maxPower','maxPowerText','maxPowerExact','money','moneyText','moneyExact','guidLow','flags'}); changed=true; vitals=true; end if method=='player.getAttributes' or method=='player.getSnapshot' then s.attributes=s.attributes or {}; HermesDLL._CopyFields(s.attributes,p,{'attributesPayload','attributeCount','source'}); if p.attributesPayload then changed=true; attributes=true; end end if method=='player.getSnapshot' and s.vitals and s.vitals.healthText and s.vitals.maxHealthText then HermesDLL.autoSnapshotReady=true; end if changed then s.lastUpdate=HermesDLL._Now(); s.lastMethod=method; s.lastEvent=msg.event; HermesDLL.stateUpdates=(HermesDLL.stateUpdates or 0)+1; if basic then HermesDLL.Emit('state.basic',msg,pending); if HermesDLL._FireStateBindings then HermesDLL._FireStateBindings('basic',msg,pending); end end if position then HermesDLL.Emit('state.position',msg,pending); if HermesDLL._FireStateBindings then HermesDLL._FireStateBindings('position',msg,pending); end end if vitals then HermesDLL.Emit('state.vitals',msg,pending); if HermesDLL._FireStateBindings then HermesDLL._FireStateBindings('vitals',msg,pending); end end if attributes then HermesDLL.Emit('state.attributes',msg,pending); if HermesDLL._FireStateBindings then HermesDLL._FireStateBindings('attributes',msg,pending); end end HermesDLL.Emit('state',msg,pending); if HermesDLL._FireStateBindings then HermesDLL._FireStateBindings('state',msg,pending); end end return changed; end; ";

    WriteLog("installing HermesDLL state cache API v" HERMES_BRIDGE_VERSION);
    int result = ExecuteLua(script, "HermesBridgeStateBootstrap");
    WriteLogFormat("HermesDLL state cache install returned %lu", (DWORD)result);
    return result == 0;
}

static BOOL InstallLuaStateAccessors(void)
{
    const char* script =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL.GetState(path) local s=HermesDLL.state; if not path or path=='' then return s; end path=tostring(path); if not s then return nil; end local d=string.find(path,'%.'); if not d then return s[path]; end local root=string.sub(path,1,d-1); local key=string.sub(path,d+1); local node=s[root]; if type(node)~='table' then return nil; end return node[key]; end; "
        "function HermesDLL.GetBasicInfo() return HermesDLL.GetState('basic'); end; "
        "function HermesDLL.GetPosition() return HermesDLL.GetState('position'); end; "
        "function HermesDLL.GetVitals() return HermesDLL.GetState('vitals'); end; "
        "function HermesDLL.GetAttributes() return HermesDLL.GetState('attributes'); end; ";

    WriteLog("installing HermesDLL state accessor API v" HERMES_BRIDGE_VERSION);
    int result = ExecuteLua(script, "HermesBridgeStateAccessors");
    WriteLogFormat("HermesDLL state accessor install returned %lu", (DWORD)result);
    return result == 0;
}

static BOOL InstallLuaStateHelpers(void)
{
    BOOL basicOk = FALSE;
    BOOL bindingCoreOk = FALSE;
    BOOL bindingApiOk = FALSE;
    BOOL bindingDispatchOk = FALSE;
    BOOL refreshOk = FALSE;

    const char* basicScript =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL.IsStateReady(section) section=tostring(section or 'vitals'); if section=='basic' then local b=HermesDLL.GetBasicInfo and HermesDLL.GetBasicInfo(); return b and b.name and b.level; end if section=='position' then local p=HermesDLL.GetPosition and HermesDLL.GetPosition(); return p and p.mapId and p.zoneId; end if section=='attributes' then local a=HermesDLL.GetAttributes and HermesDLL.GetAttributes(); return a and a.attributesPayload; end if section=='all' or section=='snapshot' then return HermesDLL.IsStateReady('basic') and HermesDLL.IsStateReady('position') and HermesDLL.IsStateReady('vitals'); end local v=HermesDLL.GetVitals and HermesDLL.GetVitals(); return v and v.healthText and v.maxHealthText; end; "
        "function HermesDLL.GetHealthText() local v=HermesDLL.GetVitals and HermesDLL.GetVitals(); if not v then return nil; end return v.healthText or (v.health and tostring(v.health)) or nil; end; "
        "function HermesDLL.GetMaxHealthText() local v=HermesDLL.GetVitals and HermesDLL.GetVitals(); if not v then return nil; end return v.maxHealthText or (v.maxHealth and tostring(v.maxHealth)) or nil; end; ";

    const char* bindingCoreScript =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "HermesDLL.stateBindings = HermesDLL.stateBindings or {}; "
        "function HermesDLL._NormalizeStateSection(section) section=tostring(section or 'vitals'); if section=='state.basic' then return 'basic'; end if section=='state.position' then return 'position'; end if section=='state.vitals' then return 'vitals'; end if section=='state.attributes' then return 'attributes'; end return section; end; "
        "function HermesDLL._StateForSection(section) section=HermesDLL._NormalizeStateSection(section); if section=='basic' then return HermesDLL.GetBasicInfo and HermesDLL.GetBasicInfo(); end if section=='position' then return HermesDLL.GetPosition and HermesDLL.GetPosition(); end if section=='attributes' then return HermesDLL.GetAttributes and HermesDLL.GetAttributes(); end if section=='state' or section=='all' or section=='snapshot' then return HermesDLL.GetState and HermesDLL.GetState(); end return HermesDLL.GetVitals and HermesDLL.GetVitals(); end; ";

    const char* bindingApiScript =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL.BindState(section,callback,fireNow) section=HermesDLL._NormalizeStateSection(section); if not callback then return nil; end HermesDLL.stateBindings=HermesDLL.stateBindings or {}; HermesDLL.stateBindings[section]=HermesDLL.stateBindings[section] or {}; local list=HermesDLL.stateBindings[section]; table.insert(list,callback); local token=table.getn(list); if fireNow and HermesDLL.IsStateReady and HermesDLL.IsStateReady(section) then local fn=HermesDLL._ResolveCallback(callback); if fn then pcall(fn,HermesDLL._StateForSection(section),section,nil,nil); end end return token; end; "
        "function HermesDLL.UnbindState(section,token) section=HermesDLL._NormalizeStateSection(section); local list=HermesDLL.stateBindings and HermesDLL.stateBindings[section]; token=tonumber(token or 0); if list and token>0 and token<=table.getn(list) then list[token]=nil; return true; end return false; end; ";

    const char* bindingDispatchScript =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._FireStateBindings(section,msg,pending) section=HermesDLL._NormalizeStateSection(section); local list=HermesDLL.stateBindings and HermesDLL.stateBindings[section]; if not list then return 0; end local fired=0; for _,cb in pairs(list) do local fn=HermesDLL._ResolveCallback(cb); if fn then local ok,err=pcall(fn,HermesDLL._StateForSection(section),section,msg,pending); if not ok then HermesDLL._Print('|cffff3333HermesDLL state binding error|r '..tostring(err)); else fired=fired+1; end end end return fired; end; "
        "HermesDLL.BindStateReady = true; ";

    const char* refreshScript =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL.RefreshSnapshot(callback) if not HermesDLL.Request then return nil; end return HermesDLL.Request('player.getSnapshot','refresh-state-snapshot-v2',callback); end; ";

    WriteLog("installing HermesDLL state basic helper API v" HERMES_BRIDGE_VERSION);
    int basicResult = ExecuteLua(basicScript, "HermesBridgeStateBasicHelpers");
    WriteLogFormat("HermesDLL state basic helper install returned %lu", (DWORD)basicResult);
    basicOk = basicResult == 0;

    WriteLog("installing HermesDLL state binding core API v" HERMES_BRIDGE_VERSION);
    int bindingCoreResult = ExecuteLua(bindingCoreScript, "HermesBridgeStateBindingCore");
    WriteLogFormat("HermesDLL state binding core install returned %lu", (DWORD)bindingCoreResult);
    bindingCoreOk = bindingCoreResult == 0;

    if (bindingCoreOk)
    {
        WriteLog("installing HermesDLL state binding API v" HERMES_BRIDGE_VERSION);
        int bindingApiResult = ExecuteLua(bindingApiScript, "HermesBridgeStateBindingApi");
        WriteLogFormat("HermesDLL state binding API install returned %lu", (DWORD)bindingApiResult);
        bindingApiOk = bindingApiResult == 0;

        WriteLog("installing HermesDLL state binding dispatch API v" HERMES_BRIDGE_VERSION);
        int bindingDispatchResult = ExecuteLua(bindingDispatchScript, "HermesBridgeStateBindingDispatch");
        WriteLogFormat("HermesDLL state binding dispatch install returned %lu", (DWORD)bindingDispatchResult);
        bindingDispatchOk = bindingDispatchResult == 0;
    }
    else
        WriteLog("HermesDLL state binding dependent APIs skipped because binding core failed");

    WriteLog("installing HermesDLL state refresh helper API v" HERMES_BRIDGE_VERSION);
    int refreshResult = ExecuteLua(refreshScript, "HermesBridgeStateRefreshHelper");
    WriteLogFormat("HermesDLL state refresh helper install returned %lu", (DWORD)refreshResult);
    refreshOk = refreshResult == 0;

    return basicOk && bindingCoreOk && bindingApiOk && bindingDispatchOk && refreshOk;
}

static BOOL InstallLuaLifecycleApi(void)
{
    const char* script =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._Trace(reason,detail) HermesDLL.traceSeq=(HermesDLL.traceSeq or 0)+1; local msg='trace lifecycle seq='..tostring(HermesDLL.traceSeq)..' reason='..tostring(reason or '')..' detail='..tostring(detail or '')..' hasRequest='..tostring(type(HermesDLL.Request)=='function')..' hasReceive='..tostring(type(HermesDLL._NativeReceive)=='function')..' frames='..tostring(HermesDLL.addonCompatFrameCacheSize or -1)..' pending='..tostring(HermesDLL.pending and table.getn(HermesDLL.pending) or -1)..' inbox='..tostring(HermesDLL.inbox and table.getn(HermesDLL.inbox) or -1); local b=tonumber(HermesDLL.lifecycleTraceBudget or 0) or 0; if b>0 and type(SendAddonMessage)=='function' then HermesDLL.lifecycleTraceBudget=b-1; local target=(UnitName and UnitName('player')) or nil; pcall(SendAddonMessage,'HERMESDLL','rpc hermes.ping '..msg,'WHISPER',target); end return msg; end; "
        "function HermesDLL._ResetForWorld(reason) if HermesDLL._Trace then HermesDLL._Trace('reset-before',reason); end HermesDLL.pending={}; HermesDLL.pendingById={}; HermesDLL.recvChunks={}; HermesDLL.bulkTransfers={}; HermesDLL.lastSend=nil; HermesDLL.lastReceive=nil; HermesDLL.autoSnapshotSent=false; HermesDLL.autoSnapshotReady=false; HermesDLL.state={basic={},position={},vitals={},attributes={},playerReady=false}; HermesDLL.worldResets=(HermesDLL.worldResets or 0)+1; HermesDLL.worldResetReason=tostring(reason or ''); HermesDLL.worldResetAt=HermesDLL._Now and HermesDLL._Now() or 0; if HermesDLL._Trace then HermesDLL._Trace('reset-after',reason); end return true; end; "
        "function HermesDLL._WarmupWorld(reason) if not HermesDLL.Request then if HermesDLL._Trace then HermesDLL._Trace('warmup-no-request',reason); end return false; end local name=(UnitName and UnitName('player')) or nil; if not name or name=='' then if HermesDLL._Trace then HermesDLL._Trace('warmup-no-player',reason); end return false; end local tag=tostring(reason or 'world-entering'); if HermesDLL._Trace then HermesDLL._Trace('warmup-send',tag); end HermesDLL.Request('hermes.hello',tag); HermesDLL.Request('player.getSnapshot',tag..'-snapshot',function(msg,pending) HermesDLL.autoSnapshotReady=true; if HermesDLL._Trace then HermesDLL._Trace('snapshot-callback',tag); end end); HermesDLL.autoSnapshotSent=true; return true; end; "
        "function HermesDLL._ScheduleWarmup(reason) local f=HermesDLL.worldFrame; if not f or not f.SetScript then if HermesDLL._Trace then HermesDLL._Trace('schedule-no-frame',reason); end return false; end f.pendingWarmup=tostring(reason or 'scheduled'); f.pendingWarmupAt=(GetTime and (GetTime()+" HERMES_LUA_WARMUP_DELAY_SECONDS ")) or 0; if HermesDLL._Trace then HermesDLL._Trace('schedule',f.pendingWarmup); end f:SetScript('OnUpdate',function(self) if GetTime and self.pendingWarmupAt and GetTime()<self.pendingWarmupAt then return; end local pending=self.pendingWarmup; self.pendingWarmup=nil; self.pendingWarmupAt=nil; self:SetScript('OnUpdate',nil); if HermesDLL._EnsureSlash then HermesDLL._EnsureSlash(); end local frameCount=-1; if HermesDLL._RefreshAddonCompatFrames then local frames=HermesDLL._RefreshAddonCompatFrames(true); frameCount=frames and table.getn(frames) or -1; end if HermesDLL._Trace then HermesDLL._Trace('onupdate-warmup','reason='..tostring(pending)..' frames='..tostring(frameCount)); end if HermesDLL._WarmupWorld then HermesDLL._WarmupWorld(pending or 'scheduled'); end end); return true; end; "
        "if CreateFrame then local f=HermesDLL.worldFrame; if not f then f=CreateFrame('Frame','HermesDLLWorldLifecycleFrame'); HermesDLL.worldFrame=f; if HermesDLL._Trace then HermesDLL._Trace('frame-created',''); end else if HermesDLL._Trace then HermesDLL._Trace('frame-reused',''); end end f:RegisterEvent('PLAYER_ENTERING_WORLD'); f:RegisterEvent('PLAYER_LOGOUT'); f:RegisterEvent('ADDON_LOADED'); f:SetScript('OnEvent',function(self,event,...) local a1=...; if HermesDLL._Trace then HermesDLL._Trace('event',tostring(event)..':'..tostring(a1 or '')); end if event=='PLAYER_LOGOUT' then if HermesDLL._ResetForWorld then HermesDLL._ResetForWorld('player-logout'); end return; end if event=='PLAYER_ENTERING_WORLD' then if HermesDLL._ResetForWorld then HermesDLL._ResetForWorld('player-entering-world'); end if HermesDLL._ScheduleWarmup then HermesDLL._ScheduleWarmup('player-entering-world'); end return; end if event=='ADDON_LOADED' then if HermesDLL._EnsureSlash then HermesDLL._EnsureSlash(); end if HermesDLL._RefreshAddonCompatFrames then HermesDLL._RefreshAddonCompatFrames(true); end if HermesDLL._ScheduleWarmup then HermesDLL._ScheduleWarmup('addon-loaded:'..tostring(a1 or '')); end end end); if HermesDLL._ScheduleWarmup then HermesDLL._ScheduleWarmup('lifecycle-install'); end end; ";

    WriteLog("installing HermesDLL lifecycle API v" HERMES_BRIDGE_VERSION);
    int result = ExecuteLua(script, "HermesBridgeLifecycleApi");
    WriteLogFormat("HermesDLL lifecycle API install returned %lu", (DWORD)result);
    return result == 0;
}

static void ResetLuaForConnectionGenerationIfNeeded(void)
{
    LONG generation = InterlockedCompareExchange(&g_connectionGeneration, 0, 0);
    LONG lastReset = InterlockedCompareExchange(&g_luaWorldResetGeneration, 0, 0);
    int result = 0;

    if (generation <= 0 || lastReset == generation)
        return;

    const char* script =
        "if HermesDLL and HermesDLL._ResetForWorld then "
        "HermesDLL._ResetForWorld('native-connection-change'); "
        "if HermesDLL._WarmupWorld then HermesDLL._WarmupWorld('native-connection-change'); end "
        "end";

    __try { result = ExecuteLua(script, "HermesBridgeConnectionReset"); }
    __except (EXCEPTION_EXECUTE_HANDLER) { WriteLog("HermesBridge connection Lua reset raised exception"); result = -1; }

    if (result == 0)
    {
        InterlockedExchange(&g_luaWorldResetGeneration, generation);
        WriteLogFormat("HermesBridge Lua world state reset generation=%lu", (DWORD)generation);
    }
    else
        WriteLogFormat("HermesBridge Lua world state reset failed generation=%lu", (DWORD)generation);
}

static BOOL InstallLuaDebugPanelApi(void)
{
    const char* script =
        "if type(_G)~='table' then _G=getfenv(0); end; if type(_G.HermesDLL)~='table' then _G.HermesDLL={}; end; HermesDLL=_G.HermesDLL; "
        "function HermesDLL._DebugPanelText() local s=HermesDLL.NativeStatus and HermesDLL.NativeStatus() or {}; local st=HermesDLL.state or {}; local last=st.lastMethod or (HermesDLL.lastSend and HermesDLL.lastSend.method) or 'n/a'; return '|cff66ccffHermesDLL|r v'..tostring(HermesDLL.version)..'\\nframe='..tostring(HermesDLL.frameVersion)..' pending='..tostring(s.pending or 0)..' inbox='..tostring(s.inbox or 0)..' stream='..tostring(s.streamQueue or 0)..'\\nstateUpdates='..tostring(s.stateUpdates or 0)..' playerReady='..tostring(s.playerReady)..'\\nlast='..tostring(last)..' pumped='..tostring(s.pumped or 0)..' dropped='..tostring(s.streamDropped or 0); end; "
        "function HermesDLL.ToggleDebugPanel() if not CreateFrame or not UIParent then return false; end local f=HermesDLLDebugPanel; if not f then f=CreateFrame('Frame','HermesDLLDebugPanel',UIParent); f:SetWidth(380); f:SetHeight(120); f:SetPoint('CENTER',UIParent,'CENTER',0,0); if f.SetBackdrop then f:SetBackdrop({bgFile='Interface\\\\Tooltips\\\\UI-Tooltip-Background',edgeFile='Interface\\\\Tooltips\\\\UI-Tooltip-Border',tile=true,tileSize=16,edgeSize=16,insets={left=4,right=4,top=4,bottom=4}}); f:SetBackdropColor(0,0,0,0.86); end f.text=f:CreateFontString(nil,'OVERLAY','GameFontNormalSmall'); f.text:SetPoint('TOPLEFT',f,'TOPLEFT',12,-12); f.text:SetJustifyH('LEFT'); _G.HermesDLLDebugPanel=f; end if f.text then f.text:SetText(HermesDLL._DebugPanelText()); end f:Show(); return true; end; "
        "function HermesDLL.HideDebugPanel() if HermesDLLDebugPanel then HermesDLLDebugPanel:Hide(); return true; end return false; end; ";

    WriteLog("installing HermesDLL debug panel API v" HERMES_BRIDGE_VERSION);
    int result = ExecuteLua(script, "HermesBridgeDebugPanelApi");
    WriteLogFormat("HermesDLL debug panel API install returned %lu", (DWORD)result);
    return result == 0;
}

static void RunBootstrapOnMainThread(void)
{
    __try
    {
        BOOL stateCacheOk = FALSE;
        BOOL stateAccessorsOk = FALSE;
        BOOL stateHelpersOk = FALSE;
        BOOL lifecycleOk = FALSE;
        BOOL debugPanelOk = FALSE;
        BOOL nativeHooksOk = FALSE;
        BOOL luaApiOk = FALSE;
        BOOL worldLuaReady = InterlockedCompareExchange(&g_worldLuaReady, 0, 0) == 1;

        WriteLog("HermesBridge bootstrap main-thread install started");
        nativeHooksOk = InstallNativeHooks();
        if (!nativeHooksOk)
        {
            WriteLog("HermesBridge native hook install incomplete before Lua API bootstrap");
            ScheduleBootstrapRetry("HermesBridge native hooks unavailable; retrying bootstrap");
            return;
        }

        if (!HermesBridge_ShouldBootstrapInstallLuaApi(g_realLuaState != NULL) &&
            !HermesBridge_ShouldUseFrameScriptFallback(g_realLuaState != NULL, worldLuaReady))
        {
            if (InterlockedDecrement(&g_bootstrapWaitingLogBudget) >= 0)
                WriteLog("HermesBridge bootstrap waiting for real addon lua_State; native hooks active");
            return;
        }

        luaApiOk = InstallLuaApi();
        if (HermesBridge_ShouldBootstrapRetryLuaApi(nativeHooksOk, g_realLuaState != NULL, luaApiOk))
        {
            ScheduleBootstrapRetry("HermesBridge Lua API unavailable after lua_State capture; retrying bootstrap");
            return;
        }

        InterlockedExchange(&g_bootstrapRetryAttempts, 0);
        Sleep(HERMES_LUA_API_CHAIN_DELAY_MS);
        stateCacheOk = InstallLuaStateApi();
        if (!stateCacheOk)
            WriteLog("HermesDLL state cache API install failed; bridge continues without state cache");
        else
        {
            stateAccessorsOk = InstallLuaStateAccessors();
            if (!stateAccessorsOk)
                WriteLog("HermesDLL state accessor API install failed; bridge continues without state accessors");
            else
            {
                stateHelpersOk = InstallLuaStateHelpers();
                if (!stateHelpersOk)
                    WriteLog("HermesDLL state helper API install incomplete; bridge continues with state cache/accessors");
            }
        }

        lifecycleOk = InstallLuaLifecycleApi();
        if (!lifecycleOk)
            WriteLog("HermesDLL lifecycle API install failed; bridge continues without automatic world reset events");

        debugPanelOk = InstallLuaDebugPanelApi();
        if (!debugPanelOk)
            WriteLog("HermesDLL debug panel API install failed; bridge continues without debug panel");

        if (lifecycleOk)
            ResetLuaForConnectionGenerationIfNeeded();

        if (stateCacheOk && stateAccessorsOk)
        {
            HANDLE snapshotThread = CreateThread(NULL, 0, DelayedAutoSnapshotTimerThread, NULL, 0, NULL);
            if (snapshotThread)
            {
                CloseHandle(snapshotThread);
                WriteLogFormat("HermesBridge auto snapshot timer delayed by %lu ms", (DWORD)HERMES_AUTO_SNAPSHOT_TIMER_DELAY_MS);
            }
            else
                WriteLog("HermesBridge auto snapshot delay thread create failed");
        }
#if HERMES_ENABLE_RECV_PUMP_TIMER
        {
            if (InstallRecvPumpTimer())
            {
                WriteLog("HermesBridge recv pump timer installed without delay");
            }
            else
            {
                HANDLE pumpThread = CreateThread(NULL, 0, DelayedRecvPumpTimerThread, NULL, 0, NULL);
                if (pumpThread)
                {
                    CloseHandle(pumpThread);
                    WriteLogFormat("HermesBridge recv pump timer retry delayed by %lu ms", (DWORD)HERMES_RECV_PUMP_TIMER_DELAY_MS);
                }
                else
                    WriteLog("HermesBridge recv pump timer delay thread create failed");
            }
        }
#else
        WriteLog("HermesBridge recv pump timer disabled; use HermesDLL.Pump");
#endif

        HANDLE thread = CreateThread(NULL, 0, SelfTestThread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
#if HERMES_ENABLE_PLAYER_READY_EVENT
        HANDLE readyThread = CreateThread(NULL, 0, PlayerReadyThread, NULL, 0, NULL);
        if (readyThread)
            CloseHandle(readyThread);
#endif
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WriteLog("HermesBridge bootstrap raised exception");
    }
}

static VOID CALLBACK HermesBootstrapTimerProc(HWND hwnd, UINT message, UINT_PTR eventId, DWORD time)
{
    (void)message;
    (void)time;

    if (eventId != g_bootstrapTimer)
        return;

    KillTimer(hwnd, eventId);
    g_bootstrapTimer = 0;

    if (InterlockedCompareExchange(&g_bootstrapInstalling, 1, 0) != 0)
    {
        WriteLog("HermesBridge bootstrap timer skipped: install already running");
        return;
    }

    RunBootstrapOnMainThread();
    InterlockedExchange(&g_bootstrapInstalling, 0);
}

static BOOL InstallBootstrapTimer(DWORD delayMs)
{
    DWORD actualDelay = delayMs ? delayMs : 1;

    if (g_bootstrapTimer)
        return TRUE;

    g_bootstrapWindow = FindWowMainWindow();
    if (!g_bootstrapWindow)
    {
        WriteLog("HermesBridge bootstrap timer skipped: WoW window not found");
        return FALSE;
    }

    g_bootstrapTimer = SetTimer(g_bootstrapWindow, HERMES_BOOTSTRAP_TIMER_ID, actualDelay, HermesBootstrapTimerProc);
    if (!g_bootstrapTimer)
    {
        WriteLog("HermesBridge bootstrap timer install failed");
        return FALSE;
    }

    WriteLogFormat2("HermesBridge bootstrap timer installed hwnd=0x%08lX delay=%lu", (DWORD)(uintptr_t)g_bootstrapWindow, actualDelay);
    return TRUE;
}

static BOOL ScheduleBootstrapRetry(const char* reason)
{
    LONG attempt = InterlockedIncrement(&g_bootstrapRetryAttempts);
    BOOL staleRecovery = reason && strstr(reason, "stale lua_State") != NULL;
    DWORD delayMs = (DWORD)HermesBridge_SelectBootstrapRetryDelayMs(
        staleRecovery,
        HERMES_BOOTSTRAP_TIMER_DELAY_MS,
        HERMES_BOOTSTRAP_STALE_TIMER_DELAY_MS);

    if (reason)
        WriteLog(reason);

    if (attempt > HERMES_BOOTSTRAP_MAX_RETRIES)
    {
        WriteLogFormat("HermesBridge bootstrap retry abandoned attempts=%lu", (DWORD)attempt);
        return FALSE;
    }

    WriteLogFormat2("HermesBridge bootstrap retry scheduled attempt=%lu delay=%lu", (DWORD)attempt, delayMs);
    return InstallBootstrapTimer(delayMs);
}

static DWORD WINAPI BootstrapThread(LPVOID parameter)
{
    DWORD attempts = 0;
    (void)parameter;
    WriteLog("HermesBridge bootstrap thread started");
    Sleep(500);

    __try
    {
        ResolveClientAddresses();
        InitializeSendQueue();
        InitializeRecvQueue();

        if (InterlockedCompareExchange(&g_recvPumpWatchdogStarted, 1, 0) == 0)
        {
            HANDLE watchdogThread = CreateThread(NULL, 0, RecvPumpWatchdogThread, NULL, 0, NULL);
            if (watchdogThread)
                CloseHandle(watchdogThread);
            else
                WriteLog("HermesBridge recv pump watchdog thread create failed");
        }

        for (attempts = 0; attempts < 10; ++attempts)
        {
            if (InstallBootstrapTimer(HERMES_BOOTSTRAP_TIMER_DELAY_MS))
                return 0;
            Sleep(1000);
        }

        WriteLog("HermesBridge bootstrap timer install abandoned after retries");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WriteLog("HermesBridge bootstrap setup raised exception");
    }

    return 0;
}

static DWORD WINAPI DelayedRecvPumpTimerThread(LPVOID parameter)
{
    DWORD delayMs = (DWORD)HermesBridge_SelectRecvPumpTimerDelayMs(
        GetRecvQueueCount(),
        HERMES_RECV_PUMP_TIMER_DELAY_MS,
        HERMES_RECV_PUMP_TIMER_BACKLOG_DELAY_MS);

    (void)parameter;
    Sleep(delayMs);
    InstallRecvPumpTimer();
    return 0;
}

/* 独立后台线程：定期巡检 recv pump timer 的宿主窗口。分辨率/全屏切换重建 WoW 主窗口
   后，绑在旧 HWND 上的 WM_TIMER 不再派发，此处检测到并重建 timer，使回包泵送、Lua API
   重装、连接重置整条恢复链自愈。不在此线程直接执行 Lua（历史上后台线程执行 Lua 会崩溃），
   只重建 timer，真正的泵送仍由主线程 timer proc 完成。 */
static DWORD WINAPI RecvPumpWatchdogThread(LPVOID parameter)
{
    (void)parameter;
    WriteLog("HermesBridge recv pump watchdog thread started");

    for (;;)
    {
        HWND current;
        HWND fresh;
        BOOL needFix;

        Sleep(HERMES_RECV_PUMP_WATCHDOG_INTERVAL_MS);

        if (InterlockedCompareExchange(&g_recvPumpWatchdogStop, 0, 0) != 0)
            break;

        /* recv 队列未初始化说明 bootstrap 尚未推进到 recv pump，暂不干预。 */
        if (!g_recvQueueLockInitialized)
            continue;

        __try
        {
            current = g_recvPumpWindow;
            fresh = FindWowMainWindow();

            /* 需要修复：timer 从未装成、记录窗口已销毁、或主窗口已换成新的 HWND
               （分辨率/全屏切换重建窗口的典型表现）。 */
            needFix = (g_recvPumpTimer == 0) ||
                      (current == NULL) ||
                      (!IsWindow(current)) ||
                      (fresh != NULL && fresh != current);

            if (needFix)
            {
                WriteLogFormat4("HermesBridge recv pump watchdog repairing timer=%lu curHwnd=0x%08lX curAlive=%lu freshHwnd=0x%08lX",
                    (DWORD)g_recvPumpTimer,
                    (DWORD)(uintptr_t)current,
                    (DWORD)(current != NULL && IsWindow(current)),
                    (DWORD)(uintptr_t)fresh);
                InstallRecvPumpTimer();
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            WriteLog("HermesBridge recv pump watchdog raised exception");
        }
    }

    WriteLog("HermesBridge recv pump watchdog thread exiting");
    return 0;
}

static DWORD WINAPI DelayedAutoSnapshotTimerThread(LPVOID parameter)
{
    (void)parameter;
    Sleep(HERMES_AUTO_SNAPSHOT_TIMER_DELAY_MS);
    InstallAutoSnapshotTimer();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        InitializeLogPath(instance);
        WriteLog("HermesBridge DLL_PROCESS_ATTACH reached");
        HANDLE thread = CreateThread(NULL, 0, BootstrapThread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
        else
            WriteLog("HermesBridge CreateThread failed");
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        InterlockedExchange(&g_recvPumpWatchdogStop, 1);
        if (g_sendQueueLockInitialized)
        {
            DeleteCriticalSection(&g_sendQueueLock);
            g_sendQueueLockInitialized = FALSE;
        }
        if (g_recvPumpTimer && g_recvPumpWindow)
        {
            KillTimer(g_recvPumpWindow, g_recvPumpTimer);
            g_recvPumpTimer = 0;
        }
        g_recvPumpWindow = NULL;
        if (g_recvPumpTimerLockInitialized)
        {
            DeleteCriticalSection(&g_recvPumpTimerLock);
            g_recvPumpTimerLockInitialized = FALSE;
        }
        if (g_recvQueueLockInitialized)
        {
            DeleteCriticalSection(&g_recvQueueLock);
            g_recvQueueLockInitialized = FALSE;
        }
    }

    return TRUE;
}

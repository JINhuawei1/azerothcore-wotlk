#ifndef HERMES_BRIDGE_CLIENT_PUMP_GATE_H
#define HERMES_BRIDGE_CLIENT_PUMP_GATE_H

static int HermesBridge_ShouldPumpRecvQueue(int hasPending, int luaApiReady, int realLuaStatePresent, int luaApiEnsureRunning, int recvDispatching)
{
    return hasPending && luaApiReady && realLuaStatePresent && !luaApiEnsureRunning && !recvDispatching;
}

static int HermesBridge_ShouldScheduleLuaStateRecovery(int hadRealLuaState, int luaStateExecuteRaisedException)
{
    return hadRealLuaState && luaStateExecuteRaisedException;
}

static int HermesBridge_ShouldAttemptLuaApiInstall(int realLuaStatePresent)
{
    return realLuaStatePresent;
}

static int HermesBridge_ShouldBootstrapInstallLuaApi(int realLuaStatePresent)
{
    return realLuaStatePresent;
}

static int HermesBridge_ShouldBootstrapRetryLuaApi(int nativeHooksInstalled, int realLuaStatePresent, int luaApiInstalled)
{
    if (!nativeHooksInstalled)
        return 1;

    if (!realLuaStatePresent)
        return 0;

    return !luaApiInstalled;
}

static int HermesBridge_ShouldUseFrameScriptFallback(int realLuaStatePresent, int worldLuaReady)
{
    return !realLuaStatePresent && worldLuaReady;
}

#endif

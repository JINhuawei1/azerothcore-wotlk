#include "../client/hermes_bridge_client_pump_gate.h"

#include <cassert>

int main()
{
    assert(!HermesBridge_ShouldPumpRecvQueue(false, true, true, false, false));
    assert(!HermesBridge_ShouldPumpRecvQueue(true, false, true, false, false));
    assert(!HermesBridge_ShouldPumpRecvQueue(true, true, false, false, false));
    assert(!HermesBridge_ShouldPumpRecvQueue(true, true, true, true, false));
    assert(!HermesBridge_ShouldPumpRecvQueue(true, true, true, false, true));
    assert(HermesBridge_ShouldPumpRecvQueue(true, true, true, false, false));

    assert(!HermesBridge_ShouldScheduleLuaStateRecovery(false, true));
    assert(!HermesBridge_ShouldScheduleLuaStateRecovery(true, false));
    assert(HermesBridge_ShouldScheduleLuaStateRecovery(true, true));

    assert(!HermesBridge_ShouldAttemptLuaApiInstall(false));
    assert(HermesBridge_ShouldAttemptLuaApiInstall(true));

    assert(!HermesBridge_ShouldBootstrapInstallLuaApi(false));
    assert(HermesBridge_ShouldBootstrapInstallLuaApi(true));

    assert(HermesBridge_ShouldBootstrapRetryLuaApi(false, false, false));
    assert(!HermesBridge_ShouldBootstrapRetryLuaApi(true, false, false));
    assert(HermesBridge_ShouldBootstrapRetryLuaApi(true, true, false));
    assert(!HermesBridge_ShouldBootstrapRetryLuaApi(true, true, true));

    assert(!HermesBridge_ShouldUseFrameScriptFallback(false, false));
    assert(!HermesBridge_ShouldUseFrameScriptFallback(true, false));
    assert(HermesBridge_ShouldUseFrameScriptFallback(false, true));

    return 0;
}

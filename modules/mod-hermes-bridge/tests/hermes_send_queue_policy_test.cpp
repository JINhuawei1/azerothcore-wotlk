#include "../client/hermes_bridge_send_queue_policy.h"

#include <cassert>

int main()
{
    assert(HermesBridge_IsDiagnosticSendPayload("rpc hermes.ping trace lifecycle seq=1"));
    assert(HermesBridge_IsDiagnosticSendPayload("rpc hermes.ping addon-compat seq=1 prefix=MAGICHIT"));

    assert(!HermesBridge_IsDiagnosticSendPayload("rpcid 100030 player.getSnapshot refresh-state-snapshot-v2"));
    assert(!HermesBridge_IsDiagnosticSendPayload("rpc addon.dispatch MAGICHIT\tBOSS_MAP_RESET"));
    assert(!HermesBridge_IsDiagnosticSendPayload("rpcid 100027 addon.dispatch REALMONEY REQ"));

    assert(!HermesBridge_ShouldDropDiagnosticSendPayload("rpc hermes.ping trace lifecycle seq=1", 23, 32, 8));
    assert(HermesBridge_ShouldDropDiagnosticSendPayload("rpc hermes.ping trace lifecycle seq=1", 24, 32, 8));
    assert(!HermesBridge_ShouldDropDiagnosticSendPayload("rpcid 100030 player.getSnapshot refresh-state-snapshot-v2", 31, 32, 8));

    assert(HermesBridge_ShouldLogSendQueueDrop(1));
    assert(HermesBridge_ShouldLogSendQueueDrop(3));
    assert(!HermesBridge_ShouldLogSendQueueDrop(4));
    assert(HermesBridge_ShouldLogSendQueueDrop(32));

    return 0;
}

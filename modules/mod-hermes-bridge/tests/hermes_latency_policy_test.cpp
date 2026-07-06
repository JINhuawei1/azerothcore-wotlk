#include "../client/hermes_bridge_latency_policy.h"
#include "../client/hermes_bridge_recv_queue_policy.h"

#include <cassert>

int main()
{
    assert(HermesBridge_SelectBootstrapRetryDelayMs(0, 1000, 100) == 1000);
    assert(HermesBridge_SelectBootstrapRetryDelayMs(1, 1000, 100) == 100);

    assert(HermesBridge_SelectRecvPumpTimerDelayMs(0, 3000, 100) == 3000);
    assert(HermesBridge_SelectRecvPumpTimerDelayMs(12, 3000, 100) == 100);

    assert(!HermesBridge_ShouldPriorityPopRecvQueue(63, 64));
    assert(HermesBridge_ShouldPriorityPopRecvQueue(64, 64));

    assert(HermesBridge_IsPriorityRecvFrame(1, 0, 0, 1));
    assert(HermesBridge_IsPriorityRecvFrame(0, 1, 0, 1));
    assert(HermesBridge_IsPriorityRecvFrame(0, 0, 1, 0));
    assert(!HermesBridge_IsPriorityRecvFrame(0, 0, 0, 1));

    return 0;
}

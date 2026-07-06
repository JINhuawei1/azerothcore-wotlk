#ifndef HERMES_BRIDGE_LATENCY_POLICY_H
#define HERMES_BRIDGE_LATENCY_POLICY_H

static unsigned long HermesBridge_SelectBootstrapRetryDelayMs(int staleLuaStateRecovery, unsigned long defaultDelayMs, unsigned long staleDelayMs)
{
    return staleLuaStateRecovery ? staleDelayMs : defaultDelayMs;
}

static unsigned long HermesBridge_SelectRecvPumpTimerDelayMs(unsigned long pendingRecvCount, unsigned long defaultDelayMs, unsigned long backlogDelayMs)
{
    return pendingRecvCount > 0 ? backlogDelayMs : defaultDelayMs;
}

#endif

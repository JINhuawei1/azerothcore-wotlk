#ifndef HERMES_BRIDGE_RECV_QUEUE_POLICY_H
#define HERMES_BRIDGE_RECV_QUEUE_POLICY_H

static int HermesBridge_ShouldPriorityPopRecvQueue(unsigned long queued, unsigned long threshold)
{
    return threshold > 0 && queued >= threshold;
}

static int HermesBridge_IsPriorityRecvFrame(int isResponse, int isError, int isRpcLane, int isAddonMessage)
{
    return isResponse || isError || (isRpcLane && !isAddonMessage);
}

#endif

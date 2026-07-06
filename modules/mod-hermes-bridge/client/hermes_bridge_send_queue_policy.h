#ifndef HERMES_BRIDGE_SEND_QUEUE_POLICY_H
#define HERMES_BRIDGE_SEND_QUEUE_POLICY_H

static int HermesBridge_StartsWith(const char* value, const char* prefix)
{
    if (!value || !prefix)
        return 0;

    while (*prefix)
    {
        if (*value != *prefix)
            return 0;
        ++value;
        ++prefix;
    }

    return 1;
}

static int HermesBridge_IsDiagnosticSendPayload(const char* payload)
{
    return HermesBridge_StartsWith(payload, "rpc hermes.ping trace ") ||
        HermesBridge_StartsWith(payload, "rpc hermes.ping addon-compat ");
}

static int HermesBridge_ShouldDropDiagnosticSendPayload(const char* payload, unsigned long queued, unsigned long capacity, unsigned long reserve)
{
    unsigned long threshold;

    if (!HermesBridge_IsDiagnosticSendPayload(payload))
        return 0;

    if (capacity == 0)
        return 1;

    threshold = capacity > reserve ? capacity - reserve : 0;
    return queued >= threshold;
}

static int HermesBridge_ShouldLogSendQueueDrop(unsigned long dropped)
{
    return dropped <= 3 || (dropped % 32ul) == 0;
}

#endif

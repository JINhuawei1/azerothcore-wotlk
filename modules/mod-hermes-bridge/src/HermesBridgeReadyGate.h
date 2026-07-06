#ifndef HERMES_BRIDGE_READY_GATE_H
#define HERMES_BRIDGE_READY_GATE_H

#include <cstdint>
#include <string>

inline bool HermesBridge_ShouldTakeoverLegacyAddonPacket(bool bridgeEnabled, bool addonReady, bool prefixReady, bool takeoverPrefix)
{
    return bridgeEnabled && (addonReady || prefixReady) && takeoverPrefix;
}

inline bool HermesBridge_ShouldMarkAddonReadyForMethod(std::uint16_t methodId)
{
    return methodId != 0;
}

inline bool HermesBridge_ShouldMarkAddonPrefixReadyForMethod(std::uint16_t methodId)
{
    return methodId == 20;
}

inline bool HermesBridge_IsLifecycleResetTrace(std::string const& payload)
{
    (void)payload;
    return false;
}

#endif

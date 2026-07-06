#include "../src/HermesBridgeReadyGate.h"

#include <cassert>
#include <cstdint>
#include <string>

int main()
{
    assert(!HermesBridge_ShouldTakeoverLegacyAddonPacket(false, true, true, true));
    assert(!HermesBridge_ShouldTakeoverLegacyAddonPacket(true, false, false, true));
    assert(!HermesBridge_ShouldTakeoverLegacyAddonPacket(true, true, true, false));
    assert(HermesBridge_ShouldTakeoverLegacyAddonPacket(true, true, false, true));
    assert(HermesBridge_ShouldTakeoverLegacyAddonPacket(true, false, true, true));

    assert(HermesBridge_ShouldMarkAddonReadyForMethod(1));
    assert(HermesBridge_ShouldMarkAddonReadyForMethod(2));
    assert(HermesBridge_ShouldMarkAddonReadyForMethod(20));
    assert(!HermesBridge_ShouldMarkAddonReadyForMethod(0));
    assert(!HermesBridge_ShouldMarkAddonPrefixReadyForMethod(1));
    assert(!HermesBridge_ShouldMarkAddonPrefixReadyForMethod(2));
    assert(HermesBridge_ShouldMarkAddonPrefixReadyForMethod(20));

    assert(!HermesBridge_IsLifecycleResetTrace("trace lifecycle seq=9 reason=reset-before detail=player-logout hasRequest=true"));
    assert(!HermesBridge_IsLifecycleResetTrace("trace lifecycle seq=10 reason=reset-after detail=player-entering-world hasRequest=true"));
    assert(!HermesBridge_IsLifecycleResetTrace("trace lifecycle seq=11 reason=warmup-send detail=player-entering-world hasRequest=true"));
    assert(!HermesBridge_IsLifecycleResetTrace("addon-compat seq=1 prefix=PATTRPANEL result=handled"));

    return 0;
}

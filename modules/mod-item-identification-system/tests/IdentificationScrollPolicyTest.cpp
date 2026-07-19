#include "../src/IdentificationScrollPolicy.h"

#include <cassert>
#include <string>

int main()
{
    IdentificationScrollTargetState emptyTarget{};
    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Identify, emptyTarget) ==
           IdentificationScrollAction::RunFullIdentificationAndOverwriteMultiplier);

    IdentificationScrollTargetState identifiedWithoutAdditional{ true, false, true };
    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Identify, identifiedWithoutAdditional) ==
           IdentificationScrollAction::SupplementAdditionalAndOverwriteMultiplier);

    IdentificationScrollTargetState identifiedWithAdditionalAndMultiplier{ true, true, true };
    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Identify, identifiedWithAdditionalAndMultiplier) ==
           IdentificationScrollAction::PreserveIdentificationAndOverwriteMultiplier);

    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Cleanup, identifiedWithAdditionalAndMultiplier) ==
           IdentificationScrollAction::ClearAllCustomIdentificationData);

    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Cleanup, emptyTarget) ==
           IdentificationScrollAction::RejectNotEligible);

    IdentificationScrollTargetState identifiedWithoutMultiplier{ true, true, false };
    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Cleanup, identifiedWithoutMultiplier) ==
           IdentificationScrollAction::ClearAllCustomIdentificationData);

    IdentificationScrollTargetState pendingWithoutMultiplier{ false, false, false };
    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Cleanup, pendingWithoutMultiplier) ==
           IdentificationScrollAction::RejectNotEligible);

    IdentificationScrollTargetState pendingWithStaleMultiplier{ false, false, true };
    assert(ResolveIdentificationScrollAction(IdentificationScrollType::Cleanup, pendingWithStaleMultiplier) ==
           IdentificationScrollAction::RejectNotEligible);

    assert(BuildIdentificationCleanupRefreshPayload(50775, 12633883) ==
           std::string("IDENTIFY_CLEANUP_REFRESH:50775:12633883"));

    assert(BuildIdentificationRefreshPayload(90204, 12115214) ==
           std::string("IDENTIFY_REFRESH:90204:12115214"));

    return 0;
}

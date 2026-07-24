#include "../src/PromotionRewardPolicy.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main()
{
    using namespace PromotionRewardPolicy;

    static_assert(static_cast<std::uint8_t>(ReviewStatus::Pending) == 0);
    static_assert(static_cast<std::uint8_t>(ReviewDecision::Approve) == 0);
    static_assert(static_cast<std::uint8_t>(GrantMode::Cdk) == 0);
    static_assert(static_cast<std::uint8_t>(GrantStatus::Requested) == 0);
    static_assert(static_cast<std::uint8_t>(RollbackStatus::None) == 0);

    assert(NextReviewStatus(ReviewStatus::Pending, ReviewDecision::Approve) == ReviewStatus::Approved);
    assert(NextReviewStatus(ReviewStatus::Pending, ReviewDecision::Reject) == ReviewStatus::Rejected);
    assert(NextReviewStatus(ReviewStatus::Approved, ReviewDecision::Reject) == ReviewStatus::Approved);
    assert(NextReviewStatus(ReviewStatus::Rejected, ReviewDecision::Approve) == ReviewStatus::Rejected);

    assert(NextInvalidStreak(2, ReviewDecision::Reject, 3) == 3u);
    assert(NextInvalidStreak(2, ReviewDecision::Approve, 3) == 0u);
    assert(NextInvalidStreak(std::numeric_limits<std::uint32_t>::max(), ReviewDecision::Reject, 3) ==
        std::numeric_limits<std::uint32_t>::max());

    assert(ShouldBanAfterReject(3, 3));
    assert(ShouldBanAfterReject(4, 3));
    assert(!ShouldBanAfterReject(2, 3));
    assert(!ShouldBanAfterReject(3, 0));

    assert(BuildClaimMarker("abc123") == "CLAIM:abc123");
    assert(OwnsGrantClaim("CLAIM:abc123", "abc123"));
    assert(!OwnsGrantClaim("CLAIM:other", "abc123"));
    assert(!OwnsGrantClaim("", "abc123"));

    assert(ProcessingRecoveryFor(GrantMode::Cdk, false, false, false) == ProcessingRecoveryAction::RetryPending);
    assert(ProcessingRecoveryFor(GrantMode::Cdk, true, false, true) == ProcessingRecoveryAction::RetryPending);
    assert(ProcessingRecoveryFor(GrantMode::Item, true, false, false) == ProcessingRecoveryAction::FinalizeIssued);
    assert(ProcessingRecoveryFor(GrantMode::Item, false, false, false) == ProcessingRecoveryAction::RecoveryDebt);

    assert(ProcessingRecoveryFor(GrantMode::Cdk, false, true, false) ==
        ProcessingRecoveryAction::RevokeWithoutIssue);
    assert(ProcessingRecoveryFor(GrantMode::Cdk, false, true, true) ==
        ProcessingRecoveryAction::FinalizeIssued);
    assert(ProcessingRecoveryFor(GrantMode::Item, true, true, false) ==
        ProcessingRecoveryAction::FinalizeIssued);
    assert(ProcessingRecoveryFor(GrantMode::Item, false, true, false) ==
        ProcessingRecoveryAction::RecoveryDebt);

    assert(HasReliableItemReceipt(true, false, true));
    assert(HasReliableItemReceipt(false, true, true));
    assert(!HasReliableItemReceipt(true, false, false));
    assert(!HasReliableItemReceipt(false, true, false));
    assert(!HasReliableItemReceipt(false, false, true));

    assert(DetermineRollbackAction(GrantMode::Cdk, false, false, false) ==
        RollbackAction::CompleteWithoutIssue);
    assert(DetermineRollbackAction(GrantMode::Cdk, true, false, false) ==
        RollbackAction::RevokeUnusedCdk);
    assert(DetermineRollbackAction(GrantMode::Cdk, true, true, false) ==
        RollbackAction::RollbackRedeemedCdk);
    assert(DetermineRollbackAction(GrantMode::Item, true, false, true) ==
        RollbackAction::RollbackDirectItem);
    assert(DetermineRollbackAction(GrantMode::Item, true, false, false) ==
        RollbackAction::RecoveryDebt);
    assert(DetermineRollbackAction(GrantMode::Item, true, false, true, true) ==
        RollbackAction::Defer);

    assert(CanReverseResourceDelta(100, 60));
    assert(CanReverseResourceDelta(60, 60));
    assert(!CanReverseResourceDelta(59, 60));
    assert(CanReverseResourceDelta(0, 0));
    assert(CanReverseResourceDelta(0, -60));

    return 0;
}

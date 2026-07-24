#ifndef PROMOTION_REWARD_POLICY_H
#define PROMOTION_REWARD_POLICY_H

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace PromotionRewardPolicy
{
enum class ReviewStatus : std::uint8_t
{
    Pending,
    Approved,
    Rejected
};

enum class ReviewDecision : std::uint8_t
{
    Approve,
    Reject
};

enum class GrantMode : std::uint8_t
{
    Cdk,
    Item
};

enum class GrantStatus : std::uint8_t
{
    Requested,
    Issued,
    Redeemed,
    Final,
    Revoked,
    RecoveryDebt
};

enum class RollbackStatus : std::uint8_t
{
    None,
    Requested,
    Running,
    Completed,
    RecoveryDebt,
    Failed
};

enum class ProcessingRecoveryAction : std::uint8_t
{
    RetryPending,
    FinalizeIssued,
    RevokeWithoutIssue,
    RecoveryDebt
};

enum class RollbackAction : std::uint8_t
{
    Defer,
    CompleteWithoutIssue,
    RevokeUnusedCdk,
    RollbackRedeemedCdk,
    RollbackDirectItem,
    RecoveryDebt
};

constexpr ReviewStatus NextReviewStatus(ReviewStatus current, ReviewDecision decision)
{
    if (current != ReviewStatus::Pending)
        return current;

    return decision == ReviewDecision::Approve ? ReviewStatus::Approved : ReviewStatus::Rejected;
}

constexpr std::uint32_t NextInvalidStreak(
    std::uint32_t current,
    ReviewDecision decision,
    std::uint32_t /*banThreshold*/)
{
    if (decision == ReviewDecision::Approve)
        return 0;

    if (current == std::numeric_limits<std::uint32_t>::max())
        return current;

    return current + 1;
}

constexpr bool ShouldBanAfterReject(std::uint32_t invalidStreak, std::uint32_t banThreshold)
{
    return banThreshold > 0 && invalidStreak >= banThreshold;
}

inline std::string BuildClaimMarker(std::string_view claimToken)
{
    return std::string("CLAIM:") + std::string(claimToken);
}

inline bool OwnsGrantClaim(std::string_view storedMarker, std::string_view claimToken)
{
    return storedMarker == BuildClaimMarker(claimToken);
}

constexpr ProcessingRecoveryAction ProcessingRecoveryFor(
    GrantMode mode,
    bool hasReliableReceipt,
    bool rollbackPending,
    bool hasCdk)
{
    if (mode == GrantMode::Cdk)
    {
        if (rollbackPending)
            return hasCdk
                ? ProcessingRecoveryAction::FinalizeIssued
                : ProcessingRecoveryAction::RevokeWithoutIssue;

        return ProcessingRecoveryAction::RetryPending;
    }

    return hasReliableReceipt
        ? ProcessingRecoveryAction::FinalizeIssued
        : ProcessingRecoveryAction::RecoveryDebt;
}

constexpr bool HasReliableItemReceipt(bool hasPrimaryGuid, bool hasGuidList, bool hasResourceSnapshot)
{
    return (hasPrimaryGuid || hasGuidList) && hasResourceSnapshot;
}

constexpr RollbackAction DetermineRollbackAction(
    GrantMode mode,
    bool issued,
    bool redeemed,
    bool hasExactItemReceipt,
    bool processing = false)
{
    if (processing)
        return RollbackAction::Defer;

    if (!issued)
        return RollbackAction::CompleteWithoutIssue;

    if (mode == GrantMode::Cdk)
        return redeemed ? RollbackAction::RollbackRedeemedCdk : RollbackAction::RevokeUnusedCdk;

    return hasExactItemReceipt ? RollbackAction::RollbackDirectItem : RollbackAction::RecoveryDebt;
}

constexpr bool CanReverseResourceDelta(std::uint64_t currentValue, std::int64_t grantedDelta)
{
    if (grantedDelta <= 0)
        return true;

    return currentValue >= static_cast<std::uint64_t>(grantedDelta);
}
}

#endif // PROMOTION_REWARD_POLICY_H

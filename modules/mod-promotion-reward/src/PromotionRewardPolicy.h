#ifndef PROMOTION_REWARD_POLICY_H
#define PROMOTION_REWARD_POLICY_H

#include <cstdint>
#include <limits>

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
}

#endif // PROMOTION_REWARD_POLICY_H

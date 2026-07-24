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

    return 0;
}

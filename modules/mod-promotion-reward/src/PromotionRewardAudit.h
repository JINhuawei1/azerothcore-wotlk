#ifndef PROMOTION_REWARD_AUDIT_H
#define PROMOTION_REWARD_AUDIT_H

#include "PromotionRewardPolicy.h"
#include "../../interfaces/RewardReceipt.h"

#include <cstdint>
#include <string>
#include <vector>

class Player;

struct PromotionRedeemSnapshot
{
    std::uint64_t submissionId = 0;
    std::uint64_t grantId = 0;
    std::uint32_t taskId = 0;
    std::uint32_t groupId = 0;
    std::string requestId;
    std::string code;
    std::string redeemToken;
    std::uint32_t accountId = 0;
    std::uint32_t characterGuid = 0;
    std::uint32_t beforeDays = 0;
    std::vector<std::uint32_t> oldItemGuids;
    std::vector<std::uint32_t> newItemGuids;
    RewardGrantReceipt rewardReceipt;
    bool receiptComplete = true;
    std::string receiptError;
};

class PromotionRewardAuditMgr
{
public:
    static PromotionRewardAuditMgr* instance();

    bool ConsumeGrantQueue(std::uint32_t limit = 10);
    bool ConsumeReviewQueue(std::uint32_t limit = 10);
    bool ConsumeRollbackQueue(std::uint32_t limit = 10);
    bool ConsumeBanQueue(std::uint32_t limit = 10);

    bool BeginCodeRedeem(Player* player, std::string const& code, std::uint32_t groupId,
        PromotionRedeemSnapshot& snapshot);
    void CompleteCodeRedeem(Player* player, std::string const& code, std::uint32_t rewardId,
        PromotionRedeemSnapshot const& snapshot, bool success);

    bool RequestRollback(std::uint64_t submissionId, std::string const& reason);
    bool RollbackGrant(std::uint64_t grantId);
    bool RetryRollback(std::uint64_t submissionId);
    bool ClearRecoveryDebt(std::uint64_t submissionId, std::uint32_t operatorAccountId,
        std::string const& reason);
    bool GetRollbackSummary(std::uint64_t submissionId, std::string& summary) const;
    bool ApplyReviewDecision(std::uint64_t submissionId, bool approved, std::uint32_t reviewerAccountId,
        std::string const& reason);

private:
    PromotionRewardAuditMgr() = default;
    ~PromotionRewardAuditMgr() = default;
    PromotionRewardAuditMgr(PromotionRewardAuditMgr const&) = delete;
    PromotionRewardAuditMgr& operator=(PromotionRewardAuditMgr const&) = delete;

    bool IssueCdkGrant(std::uint64_t grantId, std::uint64_t submissionId, std::uint32_t taskId,
        std::string const& requestId);
    bool IssueItemGrant(std::uint64_t grantId, std::uint64_t submissionId, std::uint32_t taskId,
        std::uint32_t accountId, std::uint32_t characterGuid, std::string const& requestId);
};

#define sPromotionRewardAuditMgr PromotionRewardAuditMgr::instance()

#endif // PROMOTION_REWARD_AUDIT_H

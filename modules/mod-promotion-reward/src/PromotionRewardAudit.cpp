#include "PromotionRewardAudit.h"

#include "PromotionRewardModule.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <utility>

namespace
{
using PromotionRewardPolicy::GrantMode;

struct PromotionTaskGrantConfig
{
    bool enabled = false;
    GrantMode mode = GrantMode::Cdk;
    uint32 groupId = 0;
    uint32 requireId = 0;
    uint32 rewardId = 0;
    uint32 itemEntry = 0;
    uint32 itemCount = 0;
};

bool LoadTaskGrantConfig(uint32 taskId, PromotionTaskGrantConfig& config)
{
    QueryResult result = WorldDatabase.Query(
        "SELECT `启用`,`奖励模式`,`奖励组`,`需求ID`,`奖励ID`,`物品entry`,`物品数量` "
        "FROM `_宣传审核任务` WHERE `任务ID`={}", taskId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    config.enabled = fields[0].Get<bool>();

    std::string mode = fields[1].Get<std::string>();
    if (mode == "CDK")
        config.mode = GrantMode::Cdk;
    else if (mode == "ITEM")
        config.mode = GrantMode::Item;
    else
        return false;

    config.groupId = fields[2].Get<uint32>();
    config.requireId = fields[3].Get<uint32>();
    config.rewardId = fields[4].Get<uint32>();
    config.itemEntry = fields[5].Get<uint32>();
    config.itemCount = fields[6].Get<uint32>();
    return true;
}

std::string EscapeCharacterString(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return value;
}

std::string EscapeWorldString(std::string value)
{
    WorldDatabase.EscapeString(value);
    return value;
}

bool IsSafeCode(std::string const& code)
{
    if (code.empty() || code.size() > 64)
        return false;

    return std::all_of(code.begin(), code.end(), [](char value)
    {
        return (value >= 'A' && value <= 'Z') ||
            (value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9') || value == '_' || value == '-';
    });
}

std::string JoinGuids(std::vector<uint32> const& itemGuids)
{
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < itemGuids.size(); ++index)
    {
        if (index != 0)
            stream << ',';
        stream << itemGuids[index];
    }
    stream << ']';
    return stream.str();
}

std::string BuildItemSnapshot(Player* player, ItemPosCountVec const& destinations)
{
    std::ostringstream stream;
    stream << "{\"items\":[";
    bool first = true;
    for (ItemPosCount const& destination : destinations)
    {
        Item* item = player ? player->GetItemByPos(destination.pos) : nullptr;
        if (!item)
            continue;

        if (!first)
            stream << ',';
        first = false;
        stream << "{\"guid\":" << item->GetGUID().GetCounter()
               << ",\"count\":" << destination.count << '}';
    }
    stream << "]}";
    return stream.str();
}

std::string GenerateClaimToken(uint64 grantId)
{
    static std::atomic<uint64> sequence{ 0 };
    static thread_local std::mt19937_64 generator{ std::random_device{}() };

    uint64 timestamp = static_cast<uint64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::ostringstream stream;
    stream << std::hex << grantId << '-' << timestamp << '-'
           << sequence.fetch_add(1, std::memory_order_relaxed) << '-' << generator();
    return stream.str();
}

bool ClaimPendingGrant(uint64 grantId, std::string const& requestId, std::string& claimToken)
{
    claimToken = GenerateClaimToken(grantId);
    std::string claimMarker = PromotionRewardPolicy::BuildClaimMarker(claimToken);
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeClaimMarker = EscapeCharacterString(claimMarker);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='PROCESSING',`处理令牌`='{}' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PENDING'",
        safeClaimMarker, grantId, safeRequestId);

    QueryResult result = CharacterDatabase.Query(
        "SELECT `发放状态`,`处理令牌` FROM `_宣传奖励流水` "
        "WHERE `流水ID`={} AND `request_id`='{}'",
        grantId, safeRequestId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    return fields[0].Get<std::string>() == "PROCESSING" &&
        PromotionRewardPolicy::OwnsGrantClaim(fields[1].Get<std::string>(), claimToken);
}

void RestorePendingGrant(uint64 grantId, std::string const& requestId, std::string const& claimToken)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeClaimMarker = EscapeCharacterString(PromotionRewardPolicy::BuildClaimMarker(claimToken));
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='PENDING',`处理令牌`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        grantId, safeRequestId, safeClaimMarker);
}

void DeferPendingGrant(uint64 grantId, std::string const& requestId)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `更新时间`=NOW() "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PENDING'",
        grantId, safeRequestId);
}

void MarkPendingGrantFailed(uint64 grantId, std::string const& requestId, std::string const& error)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeError = EscapeCharacterString(error);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='FAILED',`回滚错误`='{}' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PENDING'",
        safeError, grantId, safeRequestId);
}

void MarkClaimedGrantFailed(
    uint64 grantId,
    std::string const& requestId,
    std::string const& claimToken,
    std::string const& error)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeClaimMarker = EscapeCharacterString(PromotionRewardPolicy::BuildClaimMarker(claimToken));
    std::string safeError = EscapeCharacterString(error);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='FAILED',`回滚错误`='{}',`处理令牌`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        safeError, grantId, safeRequestId, safeClaimMarker);
}

void MarkClaimedItemRecoveryDebt(
    uint64 grantId,
    std::string const& requestId,
    std::string const& claimToken)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeClaimMarker = EscapeCharacterString(PromotionRewardPolicy::BuildClaimMarker(claimToken));
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='FAILED',`回滚状态`='DEBT',`回滚时间`=NOW(),"
        "`回滚错误`='可能已发物品，需人工核查',`处理令牌`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        grantId, safeRequestId, safeClaimMarker);
}

uint32 RecoverTimedOutProcessingGrants(uint32 limit)
{
    if (limit == 0)
        return 0;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`request_id`,`发放模式`,`CDK`,`物品GUID`,`新增奖励GUID`,`资源快照`,"
        "`回滚状态`,`处理令牌` "
        "FROM `_宣传奖励流水` WHERE `发放状态`='PROCESSING' "
        "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE) "
        "ORDER BY `更新时间`,`流水ID` LIMIT {}", limit);
    if (!result)
        return 0;

    uint32 recovered = 0;
    do
    {
        Field* fields = result->Fetch();
        uint64 grantId = fields[0].Get<uint64>();
        std::string requestId = fields[1].Get<std::string>();
        std::string modeText = fields[2].Get<std::string>();
        std::string code = fields[3].Get<std::string>();
        uint64 itemGuid = fields[4].Get<uint64>();
        std::string newItemGuids = fields[5].Get<std::string>();
        std::string resourceSnapshot = fields[6].Get<std::string>();
        bool rollbackPending = fields[7].Get<std::string>() == "PENDING";
        std::string storedMarker = fields[8].Get<std::string>();

        GrantMode mode = modeText == "CDK" ? GrantMode::Cdk : GrantMode::Item;
        bool hasReliableReceipt = PromotionRewardPolicy::HasReliableItemReceipt(
            itemGuid != 0,
            !newItemGuids.empty() && newItemGuids != "[]",
            !resourceSnapshot.empty());
        PromotionRewardPolicy::ProcessingRecoveryAction action =
            PromotionRewardPolicy::ProcessingRecoveryFor(mode, hasReliableReceipt, rollbackPending, !code.empty());

        std::string safeRequestId = EscapeCharacterString(requestId);
        std::string safeStoredMarker = EscapeCharacterString(storedMarker);
        if (action == PromotionRewardPolicy::ProcessingRecoveryAction::RetryPending)
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `发放状态`='PENDING',`回滚错误`='',`处理令牌`='' "
                "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' "
                "AND `处理令牌`='{}' AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
                grantId, safeRequestId, safeStoredMarker);
        }
        else if (action == PromotionRewardPolicy::ProcessingRecoveryAction::FinalizeIssued)
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `发放状态`='ISSUED',"
                "`发放时间`=COALESCE(`发放时间`,NOW()),`回滚错误`='',`处理令牌`='' "
                "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' "
                "AND `处理令牌`='{}' AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
                grantId, safeRequestId, safeStoredMarker);
        }
        else if (action == PromotionRewardPolicy::ProcessingRecoveryAction::RevokeWithoutIssue)
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
                "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
                "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' "
                "AND `回滚状态`='PENDING' AND `处理令牌`='{}' "
                "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
                grantId, safeRequestId, safeStoredMarker);
        }
        else
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `发放状态`='FAILED',`回滚状态`='DEBT',"
                "`回滚时间`=NOW(),`回滚错误`='可能已发物品，需人工核查',`处理令牌`='' "
                "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' "
                "AND `处理令牌`='{}' AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
                grantId, safeRequestId, safeStoredMarker);
        }

        ++recovered;
    } while (recovered < limit && result->NextRow());

    return recovered;
}
}

PromotionRewardAuditMgr* PromotionRewardAuditMgr::instance()
{
    static PromotionRewardAuditMgr instance;
    return &instance;
}

bool PromotionRewardAuditMgr::ConsumeGrantQueue(std::uint32_t limit)
{
    limit = std::min<std::uint32_t>(limit, 10);
    if (limit == 0)
        return false;

    uint32 consumedCount = RecoverTimedOutProcessingGrants(limit);
    if (consumedCount >= limit)
        return true;

    uint32 pendingLimit = limit - consumedCount;
    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`提交ID`,`任务ID`,`账号ID`,`角色GUID`,`request_id` "
        "FROM `_宣传奖励流水` WHERE `发放状态`='PENDING' "
        "ORDER BY `更新时间`,`流水ID` LIMIT {}", pendingLimit);
    if (!result)
        return consumedCount > 0;

    do
    {
        Field* fields = result->Fetch();
        uint64 grantId = fields[0].Get<uint64>();
        uint64 submissionId = fields[1].Get<uint64>();
        uint32 taskId = fields[2].Get<uint32>();
        uint32 accountId = fields[3].Get<uint32>();
        uint32 characterGuid = fields[4].Get<uint32>();
        std::string requestId = fields[5].Get<std::string>();

        PromotionTaskGrantConfig config;
        if (!LoadTaskGrantConfig(taskId, config) || !config.enabled)
        {
            MarkPendingGrantFailed(grantId, requestId, "宣传任务不存在、未启用或奖励模式无效");
            ++consumedCount;
            continue;
        }

        bool rowConsumed = false;
        if (config.mode == GrantMode::Cdk)
            rowConsumed = IssueCdkGrant(grantId, submissionId, taskId, requestId);
        else
            rowConsumed = IssueItemGrant(grantId, submissionId, taskId, accountId, characterGuid, requestId);

        if (rowConsumed)
            ++consumedCount;
    } while (consumedCount < limit && result->NextRow());

    return consumedCount > 0;
}

bool PromotionRewardAuditMgr::IssueCdkGrant(
    std::uint64_t grantId,
    std::uint64_t submissionId,
    std::uint32_t taskId,
    std::string const& requestId)
{
    PromotionTaskGrantConfig config;
    if (!LoadTaskGrantConfig(taskId, config) || !config.enabled || config.mode != GrantMode::Cdk || config.rewardId == 0)
    {
        MarkPendingGrantFailed(grantId, requestId, "CDK奖励任务配置无效");
        return true;
    }

    std::string claimToken;
    if (!ClaimPendingGrant(grantId, requestId, claimToken))
        return false;

    std::string claimMarker = PromotionRewardPolicy::BuildClaimMarker(claimToken);
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeClaimMarker = EscapeCharacterString(claimMarker);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放模式`='CDK',`奖励ID`={},`物品entry`=0,`物品数量`=0 "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        config.rewardId, grantId, safeRequestId, safeClaimMarker);

    std::string remark = "宣传审核-提交ID:" + std::to_string(submissionId) +
        "-流水ID:" + std::to_string(grantId) + "-request_id:" + requestId;
    std::string safeRemark = EscapeWorldString(remark);

    std::string code;
    if (QueryResult existingCode = WorldDatabase.Query(
        "SELECT `兑换码` FROM `_奖励_兑换码` WHERE `注释`='{}' LIMIT 1", safeRemark))
    {
        code = existingCode->Fetch()[0].Get<std::string>();
    }

    if (code.empty())
    {
        if (QueryResult storedCode = CharacterDatabase.Query(
            "SELECT `CDK` FROM `_宣传奖励流水` WHERE `流水ID`={} AND `request_id`='{}'",
            grantId, safeRequestId))
        {
            code = storedCode->Fetch()[0].Get<std::string>();
        }
    }

    if (code.empty())
    {
        code = sPromotionRewardMgr->GenerateUniqueCode();
        if (code.empty())
        {
            RestorePendingGrant(grantId, requestId, claimToken);
            return false;
        }

        std::string safeCode = EscapeCharacterString(code);
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `CDK`='{}' "
            "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' "
            "AND `处理令牌`='{}' AND `CDK`=''",
            safeCode, grantId, safeRequestId, safeClaimMarker);
    }

    if (!IsSafeCode(code))
    {
        MarkClaimedGrantFailed(grantId, requestId, claimToken, "流水中的CDK格式无效");
        return true;
    }

    std::string safeCode = EscapeWorldString(code);
    WorldDatabase.DirectExecute(
        "INSERT IGNORE INTO `_奖励_兑换码` "
        "(`注释`,`兑换码`,`组`,`需求`,`奖励`,`兑换次数`) "
        "VALUES ('{}','{}',{},{},{},1)",
        safeRemark, safeCode, config.groupId, config.requireId, config.rewardId);

    QueryResult inserted = WorldDatabase.Query(
        "SELECT `注释`,`组`,`需求`,`奖励` FROM `_奖励_兑换码` WHERE `兑换码`='{}'", safeCode);
    if (!inserted)
    {
        RestorePendingGrant(grantId, requestId, claimToken);
        return false;
    }

    Field* insertedFields = inserted->Fetch();
    if (insertedFields[0].Get<std::string>() != remark ||
        insertedFields[1].Get<uint32>() != config.groupId ||
        insertedFields[2].Get<uint32>() != config.requireId ||
        insertedFields[3].Get<uint32>() != config.rewardId)
    {
        MarkClaimedGrantFailed(grantId, requestId, claimToken, "CDK唯一键冲突，未覆盖已有兑换码");
        return true;
    }

    std::string safeCharacterCode = EscapeCharacterString(code);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放模式`='CDK',`CDK`='{}',`奖励ID`={},"
        "`物品entry`=0,`物品数量`=0,`发放状态`='ISSUED',`发放时间`=NOW(),`回滚错误`='',`处理令牌`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        safeCharacterCode, config.rewardId, grantId, safeRequestId, safeClaimMarker);
    return true;
}

bool PromotionRewardAuditMgr::IssueItemGrant(
    std::uint64_t grantId,
    std::uint64_t /*submissionId*/,
    std::uint32_t taskId,
    std::uint32_t accountId,
    std::uint32_t characterGuid,
    std::string const& requestId)
{
    PromotionTaskGrantConfig config;
    if (!LoadTaskGrantConfig(taskId, config) || !config.enabled || config.mode != GrantMode::Item ||
        config.itemEntry == 0 || config.itemCount == 0)
    {
        MarkPendingGrantFailed(grantId, requestId, "直接物品奖励任务配置无效");
        return true;
    }

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(config.itemEntry);
    if (!itemTemplate)
    {
        MarkPendingGrantFailed(grantId, requestId, "直接物品模板不存在");
        return true;
    }

    Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, characterGuid));
    if (!player || !player->GetSession())
    {
        DeferPendingGrant(grantId, requestId);
        return false; // 离线不消费，保持 PENDING，玩家上线后重试。
    }

    if (player->GetSession()->GetAccountId() != accountId)
    {
        MarkPendingGrantFailed(grantId, requestId, "角色与提交账号不匹配");
        return true;
    }

    ItemPosCountVec destinations;
    uint32 noSpaceForCount = 0;
    InventoryResult inventoryResult = player->CanStoreNewItem(
        NULL_BAG, NULL_SLOT, destinations, config.itemEntry, config.itemCount, &noSpaceForCount);
    if (inventoryResult != EQUIP_ERR_OK || noSpaceForCount != 0 || destinations.empty())
    {
        DeferPendingGrant(grantId, requestId);
        return false;
    }

    // 不把奖励叠加到玩家已有堆叠，避免绑定或追回时影响原有同类物品。
    for (ItemPosCount const& destination : destinations)
        if (player->GetItemByPos(destination.pos))
        {
            DeferPendingGrant(grantId, requestId);
            return false;
        }

    std::string claimToken;
    if (!ClaimPendingGrant(grantId, requestId, claimToken))
        return false;

    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeClaimMarker = EscapeCharacterString(PromotionRewardPolicy::BuildClaimMarker(claimToken));
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放模式`='ITEM',`奖励ID`=0,`物品entry`={},`物品数量`={} "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        config.itemEntry, config.itemCount, grantId, safeRequestId, safeClaimMarker);

    if (!sPromotionRewardMgr->BeginItemCapture(player, grantId))
    {
        RestorePendingGrant(grantId, requestId, claimToken);
        return false;
    }

    Item* firstItem = player->StoreNewItem(
        destinations, config.itemEntry, true, Item::GenerateItemRandomPropertyId(config.itemEntry));
    if (!firstItem)
    {
        sPromotionRewardMgr->CancelItemCapture(player, grantId);
        MarkClaimedItemRecoveryDebt(grantId, requestId, claimToken);
        return true;
    }

    for (ItemPosCount const& destination : destinations)
    {
        if (Item* item = player->GetItemByPos(destination.pos))
        {
            item->SetState(ITEM_CHANGED, player);
            item->SetBinding(true);
            sPromotionRewardMgr->RecordCapturedItem(player, item);
        }
    }

    std::string resourceSnapshot = BuildItemSnapshot(player, destinations);
    std::vector<uint32> itemGuids = sPromotionRewardMgr->EndItemCapture(player, grantId);
    if (itemGuids.empty())
    {
        itemGuids.push_back(firstItem->GetGUID().GetCounter());
    }

    std::string guidList = EscapeCharacterString(JoinGuids(itemGuids));
    std::string safeSnapshot = EscapeCharacterString(resourceSnapshot);

    // 物品已创建后，先持久化可靠回执。若此时崩溃，超时恢复只补完成 ISSUED，不会再次发物品。
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放模式`='ITEM',`CDK`='',`奖励ID`=0,"
        "`物品entry`={},`物品数量`={},`物品GUID`={},`新增奖励GUID`='{}',`资源快照`='{}' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        config.itemEntry, config.itemCount, itemGuids.front(), guidList, safeSnapshot,
        grantId, safeRequestId, safeClaimMarker);

    QueryResult persisted = CharacterDatabase.Query(
        "SELECT `发放状态`,`处理令牌`,`物品GUID`,`新增奖励GUID`,`资源快照` FROM `_宣传奖励流水` "
        "WHERE `流水ID`={} AND `request_id`='{}'",
        grantId, safeRequestId);
    bool reliableReceipt = false;
    if (persisted)
    {
        Field* persistedFields = persisted->Fetch();
        reliableReceipt = persistedFields[0].Get<std::string>() == "PROCESSING" &&
            PromotionRewardPolicy::OwnsGrantClaim(persistedFields[1].Get<std::string>(), claimToken) &&
            PromotionRewardPolicy::HasReliableItemReceipt(
                persistedFields[2].Get<uint64>() != 0,
                !persistedFields[3].Get<std::string>().empty() && persistedFields[3].Get<std::string>() != "[]",
                !persistedFields[4].Get<std::string>().empty());
    }

    if (!reliableReceipt)
    {
        MarkClaimedItemRecoveryDebt(grantId, requestId, claimToken);
        return true;
    }

    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='ISSUED',`发放时间`=NOW(),`回滚错误`='',`处理令牌`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        grantId, safeRequestId, safeClaimMarker);

    player->SendNewItem(firstItem, config.itemCount, true, false);
    return true;
}

bool PromotionRewardAuditMgr::ConsumeReviewQueue(std::uint32_t limit)
{
    limit = std::min<std::uint32_t>(limit, 10);
    if (limit == 0)
        return false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT s.`提交ID`,s.`审核状态`,g.`流水ID`,s.`审核理由` "
        "FROM `_宣传提交记录` s "
        "INNER JOIN `_宣传奖励流水` g ON g.`提交ID`=s.`提交ID` "
        "WHERE (s.`审核状态`='APPROVED' AND g.`发放状态` IN ('ISSUED','REDEEMED')) "
        "OR (s.`审核状态`='REJECTED' AND g.`回滚状态` IN ('NONE','PENDING')) "
        "ORDER BY s.`提交ID` LIMIT {}", limit);
    if (!result)
        return false;

    bool consumed = false;
    do
    {
        Field* fields = result->Fetch();
        uint64 submissionId = fields[0].Get<uint64>();
        std::string reviewStatus = fields[1].Get<std::string>();
        uint64 grantId = fields[2].Get<uint64>();
        std::string reviewReason = fields[3].Get<std::string>();

        if (reviewStatus == "APPROVED")
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `发放状态`='FINAL' "
                "WHERE `流水ID`={} AND `发放状态` IN ('ISSUED','REDEEMED')", grantId);
            consumed = true;
        }
        else if (reviewStatus == "REJECTED" && RequestRollback(
            submissionId, reviewReason.empty() ? "宣传审核未通过" : reviewReason))
        {
            RollbackGrant(grantId);
            consumed = true;
        }
    } while (result->NextRow());

    return consumed;
}

bool PromotionRewardAuditMgr::BeginCodeRedeem(
    Player* player,
    std::string const& code,
    PromotionRedeemSnapshot& snapshot)
{
    snapshot = {};
    if (!player || !player->GetSession() || !IsSafeCode(code))
        return false;

    std::string safeCode = EscapeCharacterString(code);
    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`提交ID` FROM `_宣传奖励流水` "
        "WHERE `CDK`='{}' AND `发放状态` IN ('ISSUED','FINAL') AND `回滚状态`<>'SUCCESS' LIMIT 1",
        safeCode);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    snapshot.grantId = fields[0].Get<uint64>();
    snapshot.submissionId = fields[1].Get<uint64>();
    snapshot.code = code;
    snapshot.accountId = player->GetSession()->GetAccountId();
    snapshot.characterGuid = player->GetGUID().GetCounter();
    if (PromotionPlayerData* data = sPromotionRewardMgr->GetPlayerData(snapshot.characterGuid))
        snapshot.beforeDays = data->days;

    return sPromotionRewardMgr->BeginItemCapture(player, snapshot.grantId);
}

void PromotionRewardAuditMgr::CompleteCodeRedeem(
    Player* player,
    std::string const& /*code*/,
    std::uint32_t /*rewardId*/,
    PromotionRedeemSnapshot const& snapshot,
    bool success)
{
    if (!player || snapshot.grantId == 0)
        return;

    if (success)
        (void)sPromotionRewardMgr->EndItemCapture(player, snapshot.grantId);
    else
        sPromotionRewardMgr->CancelItemCapture(player, snapshot.grantId);
}

bool PromotionRewardAuditMgr::RequestRollback(std::uint64_t submissionId, std::string const& reason)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`回滚状态` FROM `_宣传奖励流水` WHERE `提交ID`={}", submissionId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    uint64 grantId = fields[0].Get<uint64>();
    std::string rollbackStatus = fields[1].Get<std::string>();
    if (rollbackStatus == "PENDING" || rollbackStatus == "SUCCESS" || rollbackStatus == "DEBT")
        return true;

    std::string safeReason = EscapeCharacterString(reason);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `回滚状态`='PENDING',`回滚错误`='{}' "
        "WHERE `流水ID`={} AND `回滚状态` IN ('NONE','FAILED')",
        safeReason, grantId);
    return true;
}

bool PromotionRewardAuditMgr::RollbackGrant(std::uint64_t grantId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT `发放模式`,`CDK`,`发放状态`,`回滚状态` "
        "FROM `_宣传奖励流水` WHERE `流水ID`={}", grantId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    std::string grantMode = fields[0].Get<std::string>();
    std::string code = fields[1].Get<std::string>();
    std::string grantStatus = fields[2].Get<std::string>();
    std::string rollbackStatus = fields[3].Get<std::string>();

    if (rollbackStatus == "SUCCESS")
        return true;

    if (grantStatus == "PROCESSING")
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `回滚错误`='奖励正在发放，等待发放完成后追回' "
            "WHERE `流水ID`={} AND `发放状态`='PROCESSING' AND `回滚状态`='PENDING'",
            grantId);
        return false;
    }

    if (grantStatus == "PENDING" || grantStatus == "FAILED")
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
            "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
            "WHERE `流水ID`={} AND `发放状态` IN ('PENDING','FAILED')",
            grantId);
        QueryResult revoked = CharacterDatabase.Query(
            "SELECT `发放状态`,`回滚状态` FROM `_宣传奖励流水` WHERE `流水ID`={}", grantId);
        if (!revoked)
            return false;

        Field* revokedFields = revoked->Fetch();
        return revokedFields[0].Get<std::string>() == "REVOKED" &&
            revokedFields[1].Get<std::string>() == "SUCCESS";
    }

    if (grantMode == "CDK" && IsSafeCode(code))
    {
        QueryResult redeemed = CharacterDatabase.Query(
            "SELECT 1 FROM `_宣传兑换流水` WHERE `奖励流水ID`={} LIMIT 1", grantId);
        if (!redeemed)
        {
            std::string safeCode = EscapeWorldString(code);
            WorldDatabase.DirectExecute(
                "UPDATE `_奖励_兑换码` SET `兑换次数`=0 WHERE `兑换码`='{}'", safeCode);
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
                "`回滚时间`=NOW(),`回滚错误`='' WHERE `流水ID`={}", grantId);
            return true;
        }

        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `回滚错误`='CDK已兑换，等待精确追回队列处理' "
            "WHERE `流水ID`={} AND `回滚状态`='PENDING'", grantId);
        return false;
    }

    if (grantMode == "ITEM")
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `回滚错误`='直接物品等待按GUID精确追回队列处理' "
            "WHERE `流水ID`={} AND `回滚状态`='PENDING'", grantId);
        return false;
    }

    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `回滚状态`='FAILED',`回滚时间`=NOW(),"
        "`回滚错误`='未知发放模式' WHERE `流水ID`={}", grantId);
    return false;
}

void PromotionRewardAuditMgr::ApplyReviewDecision(
    std::uint64_t submissionId,
    bool approved,
    std::uint32_t reviewerAccountId,
    std::string const& reason)
{
    QueryResult current = CharacterDatabase.Query(
        "SELECT `审核状态` FROM `_宣传提交记录` WHERE `提交ID`={}", submissionId);
    if (!current || current->Fetch()[0].Get<std::string>() != "PENDING")
        return;

    PromotionRewardPolicy::ReviewDecision decision = approved
        ? PromotionRewardPolicy::ReviewDecision::Approve
        : PromotionRewardPolicy::ReviewDecision::Reject;
    PromotionRewardPolicy::ReviewStatus next = PromotionRewardPolicy::NextReviewStatus(
        PromotionRewardPolicy::ReviewStatus::Pending, decision);
    std::string nextStatus = next == PromotionRewardPolicy::ReviewStatus::Approved ? "APPROVED" : "REJECTED";
    std::string operation = approved ? "APPROVE" : "REJECT";
    std::string reviewerName = "account:" + std::to_string(reviewerAccountId);
    std::string safeReason = EscapeCharacterString(reason);
    std::string safeReviewerName = EscapeCharacterString(reviewerName);

    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传提交记录` SET `审核状态`='{}',`审核人账号ID`={},`审核人名称`='{}',"
        "`审核理由`='{}' WHERE `提交ID`={} AND `审核状态`='PENDING'",
        nextStatus, reviewerAccountId, safeReviewerName, safeReason, submissionId);

    QueryResult updated = CharacterDatabase.Query(
        "SELECT `审核状态`,`审核人账号ID` FROM `_宣传提交记录` WHERE `提交ID`={}", submissionId);
    if (!updated)
        return;

    Field* updatedFields = updated->Fetch();
    if (updatedFields[0].Get<std::string>() != nextStatus || updatedFields[1].Get<uint32>() != reviewerAccountId)
        return;

    QueryResult logged = CharacterDatabase.Query(
        "SELECT 1 FROM `_宣传审核日志` WHERE `提交ID`={} AND `操作类型`='{}' LIMIT 1",
        submissionId, operation);
    if (logged)
        return;

    CharacterDatabase.DirectExecute(
        "INSERT INTO `_宣传审核日志` "
        "(`提交ID`,`操作类型`,`原状态`,`新状态`,`审核人账号ID`,`审核人名称`,`审核理由`) "
        "VALUES ({},'{}','PENDING','{}',{},'{}','{}')",
        submissionId, operation, nextStatus, reviewerAccountId, safeReviewerName, safeReason);
}

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

bool ClaimPendingGrant(uint64 grantId, std::string const& requestId)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='PROCESSING' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PENDING'",
        grantId, safeRequestId);

    QueryResult result = CharacterDatabase.Query(
        "SELECT `发放状态` FROM `_宣传奖励流水` WHERE `流水ID`={} AND `request_id`='{}'",
        grantId, safeRequestId);
    return result && result->Fetch()[0].Get<std::string>() == "PROCESSING";
}

void RestorePendingGrant(uint64 grantId, std::string const& requestId)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='PENDING' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING'",
        grantId, safeRequestId);
}

void DeferPendingGrant(uint64 grantId, std::string const& requestId)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `更新时间`=NOW() "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PENDING'",
        grantId, safeRequestId);
}

void MarkGrantFailed(uint64 grantId, std::string const& requestId, std::string const& error)
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeError = EscapeCharacterString(error);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放状态`='FAILED',`回滚错误`='{}' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态` IN ('PENDING','PROCESSING')",
        safeError, grantId, safeRequestId);
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

    uint32 candidateLimit = limit * 10;
    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`提交ID`,`任务ID`,`账号ID`,`角色GUID`,`request_id` "
        "FROM `_宣传奖励流水` WHERE `发放状态`='PENDING' "
        "ORDER BY `更新时间`,`流水ID` LIMIT {}", candidateLimit);
    if (!result)
        return false;

    uint32 consumedCount = 0;
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
            MarkGrantFailed(grantId, requestId, "宣传任务不存在、未启用或奖励模式无效");
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
        MarkGrantFailed(grantId, requestId, "CDK奖励任务配置无效");
        return true;
    }

    if (!ClaimPendingGrant(grantId, requestId))
        return false;

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
        std::string safeRequestId = EscapeCharacterString(requestId);
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
            RestorePendingGrant(grantId, requestId);
            return false;
        }

        std::string safeCode = EscapeCharacterString(code);
        std::string safeRequestId = EscapeCharacterString(requestId);
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `CDK`='{}' "
            "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `CDK`=''",
            safeCode, grantId, safeRequestId);
    }

    if (!IsSafeCode(code))
    {
        MarkGrantFailed(grantId, requestId, "流水中的CDK格式无效");
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
        RestorePendingGrant(grantId, requestId);
        return false;
    }

    Field* insertedFields = inserted->Fetch();
    if (insertedFields[0].Get<std::string>() != remark ||
        insertedFields[1].Get<uint32>() != config.groupId ||
        insertedFields[2].Get<uint32>() != config.requireId ||
        insertedFields[3].Get<uint32>() != config.rewardId)
    {
        MarkGrantFailed(grantId, requestId, "CDK唯一键冲突，未覆盖已有兑换码");
        return true;
    }

    std::string safeCharacterCode = EscapeCharacterString(code);
    std::string safeRequestId = EscapeCharacterString(requestId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放模式`='CDK',`CDK`='{}',`奖励ID`={},"
        "`物品entry`=0,`物品数量`=0,`发放状态`='ISSUED',`发放时间`=NOW(),`回滚错误`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING'",
        safeCharacterCode, config.rewardId, grantId, safeRequestId);
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
        MarkGrantFailed(grantId, requestId, "直接物品奖励任务配置无效");
        return true;
    }

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(config.itemEntry);
    if (!itemTemplate)
    {
        MarkGrantFailed(grantId, requestId, "直接物品模板不存在");
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
        MarkGrantFailed(grantId, requestId, "角色与提交账号不匹配");
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

    if (!ClaimPendingGrant(grantId, requestId))
        return false;

    if (!sPromotionRewardMgr->BeginItemCapture(player, grantId))
    {
        RestorePendingGrant(grantId, requestId);
        return false;
    }

    Item* firstItem = player->StoreNewItem(
        destinations, config.itemEntry, true, Item::GenerateItemRandomPropertyId(config.itemEntry));
    if (!firstItem)
    {
        sPromotionRewardMgr->CancelItemCapture(player, grantId);
        RestorePendingGrant(grantId, requestId);
        return false;
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

    player->SendNewItem(firstItem, config.itemCount, true, false);

    std::string guidList = EscapeCharacterString(JoinGuids(itemGuids));
    std::string safeSnapshot = EscapeCharacterString(resourceSnapshot);
    std::string safeRequestId = EscapeCharacterString(requestId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `发放模式`='ITEM',`CDK`='',`奖励ID`=0,"
        "`物品entry`={},`物品数量`={},`物品GUID`={},`新增奖励GUID`='{}',`资源快照`='{}',"
        "`发放状态`='ISSUED',`发放时间`=NOW(),`回滚错误`='' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING'",
        config.itemEntry, config.itemCount, itemGuids.front(), guidList, safeSnapshot, grantId, safeRequestId);
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
        "OR (s.`审核状态`='REJECTED' AND g.`回滚状态`='NONE') "
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

    if (grantStatus == "PENDING" || grantStatus == "FAILED")
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
            "`回滚时间`=NOW(),`回滚错误`='' WHERE `流水ID`={}", grantId);
        return true;
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

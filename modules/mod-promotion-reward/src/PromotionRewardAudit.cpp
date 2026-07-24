#include "PromotionRewardAudit.h"

#include "PromotionRewardModule.h"
#include "AccountMgr.h"
#include "Bag.h"
#include "BanMgr.h"
#include "CurrencySystem.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <charconv>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
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

void AppendUniqueGuid(std::vector<uint32>& itemGuids, uint32 itemGuid)
{
    if (itemGuid != 0 && std::find(itemGuids.begin(), itemGuids.end(), itemGuid) == itemGuids.end())
        itemGuids.push_back(itemGuid);
}

std::vector<uint32> CollectPromotionItemGuids(Player* player)
{
    std::vector<uint32> itemGuids;
    if (!player)
        return itemGuids;

    auto collect = [&itemGuids](Item* item)
    {
        if (item && sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
            AppendUniqueGuid(itemGuids, item->GetGUID().GetCounter());
    };

    for (uint16 slot = PLAYER_SLOT_START; slot < PLAYER_SLOT_END; ++slot)
        collect(player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot)));

    auto collectBag = [&collect, player](uint8 bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                collect(bag->GetItemByPos(static_cast<uint8>(slot)));
    };

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        collectBag(bagSlot);
    for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
        collectBag(bagSlot);

    std::sort(itemGuids.begin(), itemGuids.end());
    return itemGuids;
}

std::string EscapeJsonString(std::string const& value)
{
    std::ostringstream stream;
    for (char character : value)
    {
        switch (character)
        {
            case '\\': stream << "\\\\"; break;
            case '"': stream << "\\\""; break;
            case '\n': stream << "\\n"; break;
            case '\r': stream << "\\r"; break;
            case '\t': stream << "\\t"; break;
            default: stream << character; break;
        }
    }
    return stream.str();
}

std::string BuildRedeemResourceSnapshot(
    uint32 rewardId,
    uint32 beforeDays,
    uint32 afterDays,
    std::vector<uint32> const& oldPromotionGuids,
    std::vector<uint32> const& newItemGuids,
    std::map<uint32, uint32> const& itemCounts,
    RewardGrantReceipt const& receipt,
    bool receiptComplete,
    std::string const& receiptError)
{
    std::ostringstream stream;
    stream << "{\"rewardId\":" << rewardId
           << ",\"beforeDays\":" << beforeDays
           << ",\"afterDays\":" << afterDays
           << ",\"moneyDelta\":" << receipt.moneyDelta
           << ",\"complete\":" << (receiptComplete ? "true" : "false")
           << ",\"error\":\"" << EscapeJsonString(receiptError) << '"'
           << ",\"oldPromotionGuids\":" << JoinGuids(oldPromotionGuids)
           << ",\"newItemGuids\":" << JoinGuids(newItemGuids)
           << ",\"itemCounts\":{";

    bool firstItem = true;
    for (auto const& [guid, count] : itemCounts)
    {
        if (!firstItem)
            stream << ',';
        firstItem = false;
        stream << '"' << guid << "\":" << count;
    }
    stream << '}'
           << ",\"resources\":{";

    bool first = true;
    for (auto const& [name, delta] : receipt.resourceDeltas)
    {
        if (!first)
            stream << ',';
        first = false;
        stream << '"' << EscapeJsonString(name) << "\":" << delta;
    }
    stream << "}}";
    return stream.str();
}

uint32 FindPromotionItemGuid(Player* player, std::vector<uint32> const& itemGuids)
{
    if (!player)
        return 0;

    for (uint32 itemGuid : itemGuids)
        if (Item* item = player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid)))
            if (sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
                return itemGuid;

    return 0;
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

bool ParseGuidList(std::string const& text, std::vector<uint32>& guids)
{
    guids.clear();
    std::size_t cursor = 0;
    while (cursor < text.size())
    {
        while (cursor < text.size() && (text[cursor] < '0' || text[cursor] > '9'))
            ++cursor;
        if (cursor >= text.size())
            break;

        std::size_t end = cursor;
        while (end < text.size() && text[end] >= '0' && text[end] <= '9')
            ++end;

        uint64 value = 0;
        auto parsed = std::from_chars(text.data() + cursor, text.data() + end, value);
        if (parsed.ec != std::errc() || value == 0 || value > std::numeric_limits<uint32>::max())
            return false;

        AppendUniqueGuid(guids, static_cast<uint32>(value));
        cursor = end;
    }

    return !guids.empty();
}

bool ExtractJsonInt64(std::string const& json, std::string_view key, int64& value)
{
    std::string marker = "\"" + std::string(key) + "\":";
    std::size_t start = json.find(marker);
    if (start == std::string::npos)
        return false;

    start += marker.size();
    std::size_t end = start;
    if (end < json.size() && json[end] == '-')
        ++end;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9')
        ++end;
    if (end == start || (end == start + 1 && json[start] == '-'))
        return false;

    auto parsed = std::from_chars(json.data() + start, json.data() + end, value);
    return parsed.ec == std::errc();
}

bool ExtractJsonBool(std::string const& json, std::string_view key, bool& value)
{
    std::string marker = "\"" + std::string(key) + "\":";
    std::size_t start = json.find(marker);
    if (start == std::string::npos)
        return false;

    start += marker.size();
    if (json.compare(start, 4, "true") == 0)
    {
        value = true;
        return true;
    }
    if (json.compare(start, 5, "false") == 0)
    {
        value = false;
        return true;
    }
    return false;
}

bool ComputeReversedUnsigned(uint64 current, int64 grantedDelta, uint64& reversed)
{
    if (grantedDelta >= 0)
    {
        if (!PromotionRewardPolicy::CanReverseResourceDelta(current, grantedDelta))
            return false;
        reversed = current - static_cast<uint64>(grantedDelta);
        return true;
    }

    uint64 add = grantedDelta == std::numeric_limits<int64>::min()
        ? static_cast<uint64>(std::numeric_limits<int64>::max()) + 1
        : static_cast<uint64>(-grantedDelta);
    if (current > std::numeric_limits<uint64>::max() - add)
        return false;

    reversed = current + add;
    return true;
}

bool LoadResourceRollback(
    std::string const& snapshot,
    int64& moneyDelta,
    std::map<std::string, int64>& resourceDeltas,
    std::string& error)
{
    bool complete = false;
    if (!ExtractJsonBool(snapshot, "complete", complete) || !complete)
    {
        error = "奖励回执不完整";
        return false;
    }

    if (!ExtractJsonInt64(snapshot, "moneyDelta", moneyDelta))
    {
        error = "奖励回执缺少金币差额";
        return false;
    }

    static constexpr std::array<char const*, 6> currencyNames =
        { "泡点", "积分", "妖币", "魔币", "仙币", "神币" };
    for (char const* name : currencyNames)
    {
        int64 delta = 0;
        if (!ExtractJsonInt64(snapshot, name, delta))
        {
            error = std::string("奖励回执缺少资源差额:") + name;
            return false;
        }
        resourceDeltas[name] = delta;
    }

    return true;
}

bool LoadExpectedItemCounts(
    std::string const& snapshot,
    std::vector<uint32> const& itemGuids,
    std::map<uint32, uint32>& itemCounts,
    std::string& error)
{
    itemCounts.clear();
    for (uint32 guid : itemGuids)
    {
        int64 count = 0;
        if (!ExtractJsonInt64(snapshot, std::to_string(guid), count) ||
            count <= 0 || count > std::numeric_limits<uint32>::max())
        {
            error = "奖励回执缺少物品数量GUID:" + std::to_string(guid);
            return false;
        }
        itemCounts[guid] = static_cast<uint32>(count);
    }
    return true;
}

struct ExactOwnedItem
{
    uint32 guid = 0;
    uint32 entry = 0;
    uint32 count = 0;
    uint32 flags = 0;
};

bool LoadExactOwnedInventoryItem(uint32 characterGuid, uint32 itemGuid, ExactOwnedItem& item)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT ii.`guid`,ii.`itemEntry`,ii.`count`,ii.`flags` "
        "FROM `item_instance` ii INNER JOIN `character_inventory` ci ON ci.`item`=ii.`guid` "
        "WHERE ii.`guid`={} AND ii.`owner_guid`={} AND ci.`guid`={} LIMIT 1",
        itemGuid, characterGuid, characterGuid);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    item.guid = fields[0].Get<uint32>();
    item.entry = fields[1].Get<uint32>();
    item.count = fields[2].Get<uint32>();
    item.flags = fields[3].Get<uint32>();
    return item.guid == itemGuid;
}

bool LoadExactOwnedItems(
    uint32 characterGuid,
    std::vector<uint32> const& guids,
    bool requireSoulbound,
    std::vector<ExactOwnedItem>& items,
    std::string& error)
{
    items.clear();
    for (uint32 guid : guids)
    {
        ExactOwnedItem item;
        if (!LoadExactOwnedInventoryItem(characterGuid, guid, item))
        {
            error = "缺失或已转移物品GUID:" + std::to_string(guid);
            return false;
        }
        if (requireSoulbound && (item.flags & ITEM_FIELD_FLAG_SOULBOUND) == 0)
        {
            error = "物品未绑定角色GUID:" + std::to_string(guid);
            return false;
        }
        items.push_back(item);
    }
    return !items.empty();
}

bool HasAccountRecoveryDebt(uint32 accountId)
{
    return CharacterDatabase.Query(
        "SELECT 1 FROM `_宣传奖励流水` WHERE `账号ID`={} AND `回滚状态`='DEBT' LIMIT 1",
        accountId) != nullptr;
}

void MarkRollbackDebt(uint64 grantId, std::string const& rollbackMarker, std::string const& error)
{
    std::string safeMarker = EscapeCharacterString(rollbackMarker);
    std::string safeError = EscapeCharacterString(error);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `回滚状态`='DEBT',`回滚时间`=NOW(),"
        "`回滚错误`='{}',`处理令牌`='' WHERE `流水ID`={} AND `处理令牌`='{}'",
        safeError, grantId, safeMarker);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传兑换流水` SET `回滚状态`='DEBT',`回滚时间`=NOW(),`回滚错误`='{}' "
        "WHERE `奖励流水ID`={} AND `回滚状态`<>'SUCCESS'",
        safeError, grantId);
}

bool ClaimRollback(uint64 grantId, std::string& rollbackToken, std::string& rollbackMarker)
{
    rollbackToken = GenerateClaimToken(grantId);
    rollbackMarker = "ROLLBACK:" + rollbackToken;
    std::string safeMarker = EscapeCharacterString(rollbackMarker);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `处理令牌`='{}' "
        "WHERE `流水ID`={} AND `回滚状态`='PENDING' AND `处理令牌`='' "
        "AND `发放状态`<>'PROCESSING'",
        safeMarker, grantId);

    QueryResult claimed = CharacterDatabase.Query(
        "SELECT `处理令牌` FROM `_宣传奖励流水` WHERE `流水ID`={}", grantId);
    return claimed && claimed->Fetch()[0].Get<std::string>() == rollbackMarker;
}

uint32 RecoverTimedOutRollbackClaims(uint32 limit)
{
    if (limit == 0)
        return 0;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`处理令牌` FROM `_宣传奖励流水` "
        "WHERE `回滚状态`='PENDING' AND `处理令牌` LIKE 'ROLLBACK:%' "
        "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE) "
        "ORDER BY `更新时间`,`流水ID` LIMIT {}", limit);
    if (!result)
        return 0;

    uint32 recovered = 0;
    do
    {
        Field* fields = result->Fetch();
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `处理令牌`='',`回滚错误`='' "
            "WHERE `流水ID`={} AND `回滚状态`='PENDING' AND `处理令牌`='{}' "
            "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
            fields[0].Get<uint64>(), EscapeCharacterString(fields[1].Get<std::string>()));
        ++recovered;
    } while (recovered < limit && result->NextRow());

    return recovered;
}

uint32 RecoverTimedOutReviewClaims(uint32 limit)
{
    if (limit == 0)
        return 0;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `提交ID`,`审核处理令牌` FROM `_宣传提交记录` "
        "WHERE `审核处理状态`='PROCESSING' AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE) "
        "ORDER BY `更新时间`,`提交ID` LIMIT {}", limit);
    if (!result)
        return 0;

    uint32 recovered = 0;
    do
    {
        Field* fields = result->Fetch();
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传提交记录` SET `审核处理状态`='PENDING',`审核处理令牌`='' "
            "WHERE `提交ID`={} AND `审核处理状态`='PROCESSING' AND `审核处理令牌`='{}' "
            "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
            fields[0].Get<uint64>(), EscapeCharacterString(fields[1].Get<std::string>()));
        ++recovered;
    } while (recovered < limit && result->NextRow());

    return recovered;
}

bool ClaimReview(uint64 submissionId, std::string const& reviewStatus, std::string& marker)
{
    marker = "REVIEW:" + GenerateClaimToken(submissionId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传提交记录` SET `审核处理状态`='PROCESSING',`审核处理令牌`='{}' "
        "WHERE `提交ID`={} AND `审核状态`='{}' AND `审核处理状态`='PENDING'",
        EscapeCharacterString(marker), submissionId, EscapeCharacterString(reviewStatus));
    QueryResult claimed = CharacterDatabase.Query(
        "SELECT `审核处理状态`,`审核处理令牌` FROM `_宣传提交记录` WHERE `提交ID`={}",
        submissionId);
    if (!claimed)
        return false;

    Field* fields = claimed->Fetch();
    return fields[0].Get<std::string>() == "PROCESSING" && fields[1].Get<std::string>() == marker;
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

void DeferPendingGrant(uint64 grantId, std::string const& requestId, std::string const& error = {})
{
    std::string safeRequestId = EscapeCharacterString(requestId);
    std::string safeError = EscapeCharacterString(error);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `更新时间`=NOW(),`回滚错误`='{}' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PENDING'",
        safeError, grantId, safeRequestId);
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

uint32 RecoverTimedOutRedeemClaims(uint32 limit)
{
    if (limit == 0)
        return 0;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`处理令牌` FROM `_宣传奖励流水` "
        "WHERE `处理令牌` LIKE 'REDEEM:%' AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE) "
        "ORDER BY `更新时间`,`流水ID` LIMIT {}",
        limit);
    if (!result)
        return 0;

    uint32 recovered = 0;
    do
    {
        Field* fields = result->Fetch();
        uint64 grantId = fields[0].Get<uint64>();
        std::string redeemMarker = fields[1].Get<std::string>();
        std::string safeRedeemMarker = EscapeCharacterString(redeemMarker);

        QueryResult receipt = CharacterDatabase.Query(
            "SELECT 1 FROM `_宣传兑换流水` WHERE `奖励流水ID`={} LIMIT 1",
            grantId);
        if (receipt)
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `处理令牌`='',"
                "`回滚错误`=IF(`回滚状态`='PENDING','',`回滚错误`) "
                "WHERE `流水ID`={} AND `处理令牌`='{}' "
                "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
                grantId, safeRedeemMarker);
        }
        else
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `处理令牌`='',`回滚状态`='DEBT',`回滚时间`=NOW(),"
                "`回滚错误`='CDK兑换过程超时且无兑换回执，奖励是否已发放未知，禁止重复兑换' "
                "WHERE `流水ID`={} AND `处理令牌`='{}' "
                "AND `更新时间` < DATE_SUB(NOW(), INTERVAL 5 MINUTE)",
                grantId, safeRedeemMarker);
        }

        ++recovered;
    } while (recovered < limit && result->NextRow());

    return recovered;
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

    uint32 consumedCount = RecoverTimedOutRedeemClaims(limit);
    if (consumedCount < limit)
        consumedCount += RecoverTimedOutProcessingGrants(limit - consumedCount);
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

        if (HasAccountRecoveryDebt(accountId))
        {
            DeferPendingGrant(grantId, requestId, "账号存在宣传奖励追回欠账，等待管理员处理");
            ++consumedCount;
            continue;
        }

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

    if (itemTemplate->GetMaxStackSize() != 1)
    {
        MarkPendingGrantFailed(grantId, requestId, "直接物品奖励必须配置为不可堆叠，才能按GUID精确追回");
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
    bool captureExact = true;
    std::string captureError;
    std::vector<uint32> itemGuids = sPromotionRewardMgr->EndItemCapture(
        player, grantId, &captureExact, &captureError);
    if (!captureExact)
    {
        MarkClaimedItemRecoveryDebt(grantId, requestId, claimToken);
        return true;
    }
    if (itemGuids.empty())
    {
        itemGuids.push_back(firstItem->GetGUID().GetCounter());
    }

    if (itemGuids.size() != destinations.size())
    {
        MarkClaimedItemRecoveryDebt(grantId, requestId, claimToken);
        return true;
    }

    std::string guidList = EscapeCharacterString(JoinGuids(itemGuids));
    std::string safeSnapshot = EscapeCharacterString(resourceSnapshot);

    // 物品已创建后，先持久化可靠回执。若此时崩溃，超时恢复只补完成 ISSUED，不会再次发物品。
    CharacterDatabaseTransaction receiptTrans = CharacterDatabase.BeginTransaction();
    player->SaveInventoryAndGoldToDB(receiptTrans);
    receiptTrans->Append(
        "UPDATE `_宣传奖励流水` SET `发放模式`='ITEM',`CDK`='',`奖励ID`=0,"
        "`物品entry`={},`物品数量`={},`物品GUID`={},`新增奖励GUID`='{}',`资源快照`='{}' "
        "WHERE `流水ID`={} AND `request_id`='{}' AND `发放状态`='PROCESSING' AND `处理令牌`='{}'",
        config.itemEntry, config.itemCount, itemGuids.front(), guidList, safeSnapshot,
        grantId, safeRequestId, safeClaimMarker);
    CharacterDatabase.DirectCommitTransaction(receiptTrans);

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

    uint32 consumedCount = RecoverTimedOutReviewClaims(limit);
    if (consumedCount >= limit)
        return true;

    QueryResult result = CharacterDatabase.Query(
        "SELECT s.`提交ID`,s.`审核状态`,g.`流水ID`,s.`账号ID`,s.`账号名`,s.`来源IP`,s.`任务ID`,"
        "s.`审核人账号ID`,s.`审核人名称`,s.`审核理由` "
        "FROM `_宣传提交记录` s "
        "INNER JOIN `_宣传奖励流水` g ON g.`提交ID`=s.`提交ID` "
        "WHERE s.`审核状态` IN ('APPROVED','REJECTED') AND s.`审核处理状态`='PENDING' "
        "ORDER BY s.`更新时间`,s.`提交ID` LIMIT {}", limit - consumedCount);
    if (!result)
        return consumedCount > 0;

    do
    {
        Field* fields = result->Fetch();
        uint64 submissionId = fields[0].Get<uint64>();
        std::string reviewStatus = fields[1].Get<std::string>();
        uint64 grantId = fields[2].Get<uint64>();
        uint32 accountId = fields[3].Get<uint32>();
        std::string accountName = fields[4].Get<std::string>();
        std::string sourceIp = fields[5].Get<std::string>();
        uint32 taskId = fields[6].Get<uint32>();
        uint32 reviewerAccountId = fields[7].Get<uint32>();
        std::string reviewerName = fields[8].Get<std::string>();
        std::string reviewReason = fields[9].Get<std::string>();

        std::string reviewMarker;
        if (!ClaimReview(submissionId, reviewStatus, reviewMarker))
            continue;

        uint32 banThreshold = 3;
        if (QueryResult task = WorldDatabase.Query(
            "SELECT `连续无效封号次数` FROM `_宣传审核任务` WHERE `任务ID`={}", taskId))
            banThreshold = task->Fetch()[0].Get<uint32>();

        uint32 currentStreak = 0;
        uint32 currentBanStatus = 0;
        if (QueryResult stats = CharacterDatabase.Query(
            "SELECT `连续无效次数`,`封禁状态` FROM `_宣传账号统计` "
            "WHERE `账号ID`={} ORDER BY `统计日期` DESC LIMIT 1", accountId))
        {
            currentStreak = stats->Fetch()[0].Get<uint32>();
            currentBanStatus = stats->Fetch()[1].Get<uint32>();
        }

        PromotionRewardPolicy::ReviewDecision decision = reviewStatus == "APPROVED"
            ? PromotionRewardPolicy::ReviewDecision::Approve
            : PromotionRewardPolicy::ReviewDecision::Reject;
        uint32 nextStreak = PromotionRewardPolicy::NextInvalidStreak(currentStreak, decision, banThreshold);
        bool shouldBan = decision == PromotionRewardPolicy::ReviewDecision::Reject &&
            PromotionRewardPolicy::ShouldBanAfterReject(nextStreak, banThreshold);
        uint32 nextBanStatus = currentBanStatus == 1 ? 1 : (shouldBan ? 2 : currentBanStatus);

        if (accountName.empty())
            AccountMgr::GetName(accountId, accountName);
        if (reviewerName.empty())
            reviewerName = "account:" + std::to_string(reviewerAccountId);
        if (reviewReason.empty())
            reviewReason = reviewStatus == "APPROVED" ? "宣传审核通过" : "宣传审核未通过";

        std::string safeAccountName = EscapeCharacterString(accountName);
        std::string safeSourceIp = EscapeCharacterString(sourceIp);
        std::string safeReviewerName = EscapeCharacterString(reviewerName);
        std::string safeReason = EscapeCharacterString(reviewReason);
        std::string safeReviewMarker = EscapeCharacterString(reviewMarker);
        uint32 invalidIncrement = decision == PromotionRewardPolicy::ReviewDecision::Reject ? 1 : 0;

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append(
            "INSERT INTO `_宣传账号统计` "
            "(`账号ID`,`统计日期`,`账号名`,`今日无效次数`,`连续无效次数`,`封禁状态`,"
            "`封禁原因`,`最后审核时间`,`最后IP`) VALUES ({},CURDATE(),'{}',{},{},{},'{}',NOW(),'{}') "
            "ON DUPLICATE KEY UPDATE `账号名`=VALUES(`账号名`),"
            "`今日无效次数`=`今日无效次数`+{},`连续无效次数`=VALUES(`连续无效次数`),"
            "`封禁状态`=IF(`封禁状态`=1,1,GREATEST(`封禁状态`,VALUES(`封禁状态`))),"
            "`封禁原因`=IF(VALUES(`封禁状态`)=2,VALUES(`封禁原因`),`封禁原因`),"
            "`最后审核时间`=NOW(),`最后IP`=VALUES(`最后IP`)",
            accountId, safeAccountName, invalidIncrement, nextStreak, nextBanStatus,
            shouldBan ? safeReason : std::string(), safeSourceIp, invalidIncrement);

        if (decision == PromotionRewardPolicy::ReviewDecision::Reject)
        {
            if (!sourceIp.empty())
            {
                trans->Append(
                    "INSERT INTO `_宣传IP统计` (`IP地址`,`统计日期`,`今日无效次数`,`最后账号ID`) "
                    "VALUES ('{}',CURDATE(),1,{}) ON DUPLICATE KEY UPDATE "
                    "`今日无效次数`=`今日无效次数`+1,`最后账号ID`=VALUES(`最后账号ID`)",
                    safeSourceIp, accountId);
            }
            trans->Append(
                "UPDATE `_宣传奖励流水` SET "
                "`回滚错误`=IF(`回滚状态` IN ('NONE','FAILED'),'{}',`回滚错误`),"
                "`回滚状态`=IF(`回滚状态` IN ('NONE','FAILED'),'PENDING',`回滚状态`) "
                "WHERE `流水ID`={}", safeReason, grantId);
        }
        else
        {
            trans->Append(
                "UPDATE `_宣传奖励流水` SET `发放状态`='FINAL' "
                "WHERE `流水ID`={} AND `发放状态` IN ('ISSUED','REDEEMED') AND `回滚状态`='NONE'",
                grantId);
        }

        std::string operation = decision == PromotionRewardPolicy::ReviewDecision::Approve ? "APPROVE" : "REJECT";
        trans->Append(
            "INSERT INTO `_宣传审核日志` "
            "(`提交ID`,`奖励流水ID`,`操作类型`,`原状态`,`新状态`,`审核人账号ID`,`审核人名称`,`审核理由`) "
            "SELECT {},{},'{}','PENDING','{}',{},'{}','{}' FROM DUAL "
            "WHERE NOT EXISTS (SELECT 1 FROM `_宣传审核日志` WHERE `提交ID`={} AND `操作类型`='{}')",
            submissionId, grantId, operation, reviewStatus, reviewerAccountId,
            safeReviewerName, safeReason, submissionId, operation);
        trans->Append(
            "UPDATE `_宣传提交记录` SET `审核处理状态`='APPLIED',`审核处理令牌`='',`审核处理时间`=NOW() "
            "WHERE `提交ID`={} AND `审核处理状态`='PROCESSING' AND `审核处理令牌`='{}'",
            submissionId, safeReviewMarker);
        CharacterDatabase.DirectCommitTransaction(trans);

        ++consumedCount;
    } while (consumedCount < limit && result->NextRow());

    return consumedCount > 0;
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
        "SELECT `流水ID`,`提交ID`,`任务ID`,`request_id`,`发放状态`,`回滚状态` "
        "FROM `_宣传奖励流水` WHERE `CDK`='{}' LIMIT 1",
        safeCode);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    snapshot.grantId = fields[0].Get<uint64>();
    snapshot.submissionId = fields[1].Get<uint64>();
    snapshot.taskId = fields[2].Get<uint32>();
    snapshot.requestId = fields[3].Get<std::string>();
    std::string grantStatus = fields[4].Get<std::string>();
    std::string rollbackStatus = fields[5].Get<std::string>();
    snapshot.code = code;

    if ((grantStatus != "ISSUED" && grantStatus != "FINAL") || rollbackStatus != "NONE")
        return false;

    snapshot.redeemToken = GenerateClaimToken(snapshot.grantId);
    snapshot.accountId = player->GetSession()->GetAccountId();
    snapshot.characterGuid = player->GetGUID().GetCounter();
    if (PromotionPlayerData* data = sPromotionRewardMgr->GetPlayerData(snapshot.characterGuid))
        snapshot.beforeDays = data->days;
    snapshot.oldItemGuids = CollectPromotionItemGuids(player);

    std::string redeemMarker = "REDEEM:" + snapshot.redeemToken;
    std::string safeRedeemMarker = EscapeCharacterString(redeemMarker);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `处理令牌`='{}' "
        "WHERE `流水ID`={} AND `CDK`='{}' AND `发放状态` IN ('ISSUED','FINAL') "
        "AND `回滚状态`='NONE' AND `处理令牌`='' "
        "AND NOT EXISTS (SELECT 1 FROM `_宣传兑换流水` r WHERE r.`奖励流水ID`={})",
        safeRedeemMarker, snapshot.grantId, safeCode, snapshot.grantId);

    QueryResult claimed = CharacterDatabase.Query(
        "SELECT `处理令牌` FROM `_宣传奖励流水` WHERE `流水ID`={}", snapshot.grantId);
    if (!claimed || claimed->Fetch()[0].Get<std::string>() != redeemMarker)
        return false;

    if (!sPromotionRewardMgr->BeginItemCapture(player, snapshot.grantId))
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `处理令牌`='' "
            "WHERE `流水ID`={} AND `处理令牌`='{}'",
            snapshot.grantId, safeRedeemMarker);
        return false;
    }

    return true;
}

void PromotionRewardAuditMgr::CompleteCodeRedeem(
    Player* player,
    std::string const& code,
    std::uint32_t rewardId,
    PromotionRedeemSnapshot const& snapshot,
    bool success)
{
    if (!player || snapshot.grantId == 0)
        return;

    std::string redeemMarker = "REDEEM:" + snapshot.redeemToken;
    std::string safeRedeemMarker = EscapeCharacterString(redeemMarker);

    if (!player->GetSession())
    {
        sPromotionRewardMgr->CancelItemCapture(player, snapshot.grantId);
        if (success)
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `回滚状态`='DEBT',`回滚时间`=NOW(),"
                "`回滚错误`='CDK奖励已发放，但玩家会话中断导致兑换回执缺失' "
                "WHERE `流水ID`={} AND `回滚状态`='NONE' AND `处理令牌`='{}'",
                snapshot.grantId, safeRedeemMarker);
        }
        else
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `处理令牌`='' "
                "WHERE `流水ID`={} AND `处理令牌`='{}'",
                snapshot.grantId, safeRedeemMarker);
        }
        return;
    }

    if (!success)
    {
        sPromotionRewardMgr->CancelItemCapture(player, snapshot.grantId);
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `处理令牌`='' "
            "WHERE `流水ID`={} AND `处理令牌`='{}'",
            snapshot.grantId, safeRedeemMarker);
        return;
    }

    bool captureExact = true;
    std::string captureError;
    std::vector<uint32> newItemGuids = sPromotionRewardMgr->EndItemCapture(
        player, snapshot.grantId, &captureExact, &captureError);
    for (uint32 itemGuid : snapshot.rewardReceipt.itemGuids)
        AppendUniqueGuid(newItemGuids, itemGuid);
    std::sort(newItemGuids.begin(), newItemGuids.end());

    std::map<uint32, uint32> itemCounts;
    for (uint32 itemGuid : newItemGuids)
    {
        Item* item = player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid));
        if (!item)
        {
            captureExact = false;
            if (!captureError.empty())
                captureError += "; ";
            captureError += "发奖完成时缺少物品GUID:" + std::to_string(itemGuid);
            continue;
        }
        itemCounts[itemGuid] = item->GetCount();
    }

    bool receiptComplete = snapshot.receiptComplete && captureExact;
    std::string receiptError = snapshot.receiptError;
    if (!captureExact)
    {
        if (!receiptError.empty())
            receiptError += "; ";
        receiptError += captureError.empty() ? "物品GUID捕获不完整" : captureError;
    }

    uint32 afterDays = snapshot.beforeDays;
    if (PromotionPlayerData* data = sPromotionRewardMgr->GetPlayerData(player->GetGUID().GetCounter()))
        afterDays = data->days;

    uint32 oldPromotionGuid = snapshot.oldItemGuids.empty() ? 0 : snapshot.oldItemGuids.front();
    uint32 newPromotionGuid = FindPromotionItemGuid(player, newItemGuids);
    std::string guidList = JoinGuids(newItemGuids);
    std::string resourceSnapshot = BuildRedeemResourceSnapshot(
        rewardId, snapshot.beforeDays, afterDays, snapshot.oldItemGuids, newItemGuids,
        itemCounts, snapshot.rewardReceipt, receiptComplete, receiptError);

    uint32 actualAccountId = player->GetSession()->GetAccountId();
    uint32 actualCharacterGuid = player->GetGUID().GetCounter();
    std::string accountName;
    if (QueryResult account = LoginDatabase.Query("SELECT `username` FROM `account` WHERE `id`={}", actualAccountId))
        accountName = account->Fetch()[0].Get<std::string>();

    std::string actualCode = code.empty() ? snapshot.code : code;
    std::string safeRequestId = EscapeCharacterString(snapshot.requestId);
    std::string safeCode = EscapeCharacterString(actualCode);
    std::string safeCharacterName = EscapeCharacterString(player->GetName());
    std::string safeAccountName = EscapeCharacterString(accountName);
    std::string safeIp = EscapeCharacterString(player->GetSession()->GetRemoteAddress());
    std::string safeGuidList = EscapeCharacterString(guidList);
    std::string safeResourceSnapshot = EscapeCharacterString(resourceSnapshot);
    std::string exchangeRollbackStatus = receiptComplete ? "NONE" : "DEBT";
    std::string safeReceiptError = EscapeCharacterString(receiptError);

    CharacterDatabaseTransaction receiptTrans = CharacterDatabase.BeginTransaction();
    player->SaveInventoryAndGoldToDB(receiptTrans);
    receiptTrans->Append(
        "INSERT INTO `_宣传兑换流水` "
        "(`奖励流水ID`,`提交ID`,`任务ID`,`账号ID`,`request_id`,`CDK`,`兑换角色GUID`,`兑换角色名`,"
        "`兑换账号名`,`兑换IP`,`兑换时间`,`兑换前宣传天数`,`兑换后宣传天数`,`旧宣传物品GUID`,"
        "`新宣传物品GUID`,`新增奖励GUID`,`资源快照`,`回滚状态`,`回滚错误`) VALUES "
        "({},{},{},{},'{}','{}',{},'{}','{}','{}',NOW(),{},{},{},{},'{}','{}','{}','{}') "
        "ON DUPLICATE KEY UPDATE `奖励流水ID`=VALUES(`奖励流水ID`)",
        snapshot.grantId, snapshot.submissionId, snapshot.taskId, actualAccountId,
        safeRequestId, safeCode, actualCharacterGuid, safeCharacterName, safeAccountName, safeIp,
        snapshot.beforeDays, afterDays, oldPromotionGuid, newPromotionGuid, safeGuidList, safeResourceSnapshot,
        exchangeRollbackStatus, safeReceiptError);
    CharacterDatabase.DirectCommitTransaction(receiptTrans);

    QueryResult recorded = CharacterDatabase.Query(
        "SELECT `兑换角色GUID`,`账号ID`,`CDK` FROM `_宣传兑换流水` WHERE `奖励流水ID`={}",
        snapshot.grantId);
    bool receiptRecorded = false;
    if (recorded)
    {
        Field* recordedFields = recorded->Fetch();
        receiptRecorded = recordedFields[0].Get<uint64>() == actualCharacterGuid &&
            recordedFields[1].Get<uint32>() == actualAccountId &&
            recordedFields[2].Get<std::string>() == actualCode;
    }

    if (!receiptRecorded)
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `回滚状态`='DEBT',`回滚时间`=NOW(),"
            "`回滚错误`='CDK奖励已发放，但实际兑换回执写入失败' "
            "WHERE `流水ID`={} AND `回滚状态`='NONE' AND `处理令牌`='{}'",
            snapshot.grantId, safeRedeemMarker);
        return;
    }

    if (!receiptComplete)
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `处理令牌`='',`回滚状态`='DEBT',`回滚时间`=NOW(),"
            "`回滚错误`='{}' WHERE `流水ID`={} AND `处理令牌`='{}'",
            safeReceiptError, snapshot.grantId, safeRedeemMarker);
        return;
    }

    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `处理令牌`='',`回滚错误`='' "
        "WHERE `流水ID`={} AND `处理令牌`='{}'",
        snapshot.grantId, safeRedeemMarker);
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
        "UPDATE `_宣传奖励流水` SET `回滚状态`='PENDING',`回滚错误`='{}',`处理令牌`='' "
        "WHERE `流水ID`={} AND `回滚状态` IN ('NONE','FAILED')",
        safeReason, grantId);
    return true;
}

bool PromotionRewardAuditMgr::ConsumeRollbackQueue(std::uint32_t limit)
{
    limit = std::min<std::uint32_t>(limit, 10);
    if (limit == 0)
        return false;

    uint32 consumedCount = RecoverTimedOutRollbackClaims(limit);
    if (consumedCount >= limit)
        return true;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID` FROM `_宣传奖励流水` "
        "WHERE `回滚状态`='PENDING' AND `处理令牌`='' AND `发放状态`<>'PROCESSING' "
        "ORDER BY `更新时间`,`流水ID` LIMIT {}", limit - consumedCount);
    if (!result)
        return consumedCount > 0;

    do
    {
        uint64 grantId = result->Fetch()[0].Get<uint64>();
        RollbackGrant(grantId);
        ++consumedCount;
    } while (consumedCount < limit && result->NextRow());

    return consumedCount > 0;
}

bool PromotionRewardAuditMgr::ConsumeBanQueue(std::uint32_t limit)
{
    limit = std::min<std::uint32_t>(limit, 10);
    if (limit == 0)
        return false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `账号ID`,`账号名`,`封禁原因` FROM `_宣传账号统计` "
        "WHERE `封禁状态`=2 ORDER BY `统计日期`,`账号ID` LIMIT {}", limit);
    if (!result)
        return false;

    uint32 consumedCount = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 accountId = fields[0].Get<uint32>();
        std::string accountName = fields[1].Get<std::string>();
        std::string reason = fields[2].Get<std::string>();
        if (accountName.empty())
            AccountMgr::GetName(accountId, accountName);
        if (reason.empty())
            reason = "连续宣传审核无效达到封禁阈值";

        BanReturn banResult = accountName.empty()
            ? BAN_NOTFOUND
            : sBan->BanAccount(accountName, "0s", reason, "PromotionAudit");
        if (banResult == BAN_SUCCESS || banResult == BAN_LONGER_EXISTS)
        {
            std::string safeReason = EscapeCharacterString(reason);
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            trans->Append(
                "UPDATE `_宣传账号统计` SET `封禁状态`=1,`封禁时间`=NOW(),`封禁原因`='{}' "
                "WHERE `账号ID`={} AND `封禁状态`=2", safeReason, accountId);
            trans->Append(
                "INSERT INTO `_宣传审核日志` "
                "(`提交ID`,`操作类型`,`原状态`,`新状态`,`审核人名称`,`审核理由`) "
                "SELECT s.`提交ID`,'BAN','REJECTED','BANNED','PromotionAudit','{}' "
                "FROM `_宣传提交记录` s WHERE s.`账号ID`={} AND s.`审核状态`='REJECTED' "
                "ORDER BY s.`更新时间` DESC LIMIT 1",
                safeReason, accountId);
            CharacterDatabase.DirectCommitTransaction(trans);
            LOG_INFO("server.loading", "[宣传审核] 账号 {}({}) 连续无效达到阈值，已封禁并断开连接",
                accountName, accountId);
        }
        else
        {
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传账号统计` SET `封禁原因`='自动封禁失败，等待重试' "
                "WHERE `账号ID`={} AND `封禁状态`=2", accountId);
        }

        ++consumedCount;
    } while (consumedCount < limit && result->NextRow());

    return consumedCount > 0;
}

bool PromotionRewardAuditMgr::RollbackGrant(std::uint64_t grantId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT `发放模式`,`CDK`,`发放状态`,`回滚状态`,`处理令牌`,`账号ID`,`角色GUID`,"
        "`物品entry`,`物品数量`,`新增奖励GUID` "
        "FROM `_宣传奖励流水` WHERE `流水ID`={}", grantId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    std::string grantMode = fields[0].Get<std::string>();
    std::string code = fields[1].Get<std::string>();
    std::string grantStatus = fields[2].Get<std::string>();
    std::string rollbackStatus = fields[3].Get<std::string>();
    std::string processingToken = fields[4].Get<std::string>();
    uint32 grantAccountId = fields[5].Get<uint32>();
    uint32 grantCharacterGuid = fields[6].Get<uint32>();
    uint32 grantItemEntry = fields[7].Get<uint32>();
    uint32 grantItemCount = fields[8].Get<uint32>();
    std::string grantItemGuidsText = fields[9].Get<std::string>();

    if (rollbackStatus == "SUCCESS")
        return true;
    if (rollbackStatus == "DEBT")
        return false;

    if (!processingToken.empty())
    {
        if (processingToken.rfind("REDEEM:", 0) == 0)
            CharacterDatabase.DirectExecute(
                "UPDATE `_宣传奖励流水` SET `回滚错误`='CDK正在兑换，等待兑换回执完成后追回' "
                "WHERE `流水ID`={} AND `处理令牌`='{}' AND `回滚状态`='PENDING'",
                grantId, EscapeCharacterString(processingToken));
        return false;
    }

    if (grantStatus == "PROCESSING")
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `回滚错误`='奖励正在发放，等待发放完成后追回' "
            "WHERE `流水ID`={} AND `发放状态`='PROCESSING' AND `回滚状态`='PENDING'",
            grantId);
        return false;
    }

    std::string rollbackToken;
    std::string rollbackMarker;
    if (!ClaimRollback(grantId, rollbackToken, rollbackMarker))
        return false;

    bool issued = grantStatus != "PENDING" && grantStatus != "FAILED";
    QueryResult exchange = CharacterDatabase.Query(
        "SELECT `兑换流水ID`,`账号ID`,`兑换角色GUID`,`兑换前宣传天数`,`旧宣传物品GUID`,"
        "`新宣传物品GUID`,`新增奖励GUID`,`资源快照`,`回滚状态` "
        "FROM `_宣传兑换流水` WHERE `奖励流水ID`={} LIMIT 1", grantId);
    bool redeemed = exchange != nullptr;
    std::vector<uint32> directItemGuids;
    bool hasExactDirectReceipt = grantMode == "ITEM" && ParseGuidList(grantItemGuidsText, directItemGuids);
    GrantMode mode = grantMode == "CDK" ? GrantMode::Cdk : GrantMode::Item;
    PromotionRewardPolicy::RollbackAction action = PromotionRewardPolicy::DetermineRollbackAction(
        mode, issued, redeemed, hasExactDirectReceipt);

    if (action == PromotionRewardPolicy::RollbackAction::CompleteWithoutIssue)
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
            "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
            "WHERE `流水ID`={} AND `处理令牌`='{}' AND `发放状态` IN ('PENDING','FAILED')",
            grantId, EscapeCharacterString(rollbackMarker));
        return true;
    }

    if (action == PromotionRewardPolicy::RollbackAction::RevokeUnusedCdk)
    {
        if (!IsSafeCode(code))
        {
            MarkRollbackDebt(grantId, rollbackMarker, "CDK格式无效，无法作废");
            return false;
        }

        std::string safeCode = EscapeWorldString(code);
        WorldDatabase.DirectExecute(
            "UPDATE `_奖励_兑换码` SET `兑换次数`=0 WHERE `兑换码`='{}'", safeCode);
        QueryResult revokedCode = WorldDatabase.Query(
            "SELECT `兑换次数` FROM `_奖励_兑换码` WHERE `兑换码`='{}'", safeCode);
        if (!revokedCode || revokedCode->Fetch()[0].Get<uint32>() != 0)
        {
            MarkRollbackDebt(grantId, rollbackMarker, "CDK记录缺失或无法作废");
            return false;
        }

        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
            "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
            "WHERE `流水ID`={} AND `处理令牌`='{}'",
            grantId, EscapeCharacterString(rollbackMarker));
        return true;
    }

    if (action == PromotionRewardPolicy::RollbackAction::RecoveryDebt)
    {
        MarkRollbackDebt(grantId, rollbackMarker, "直接物品缺少可靠GUID回执");
        return false;
    }

    if (action == PromotionRewardPolicy::RollbackAction::RollbackDirectItem)
    {
        if (grantCharacterGuid == 0 || grantItemEntry == 0 || grantItemCount == 0)
        {
            MarkRollbackDebt(grantId, rollbackMarker, "直接物品流水缺少角色或物品配置");
            return false;
        }

        std::vector<ExactOwnedItem> items;
        std::string error;
        if (!LoadExactOwnedItems(grantCharacterGuid, directItemGuids, true, items, error))
        {
            MarkRollbackDebt(grantId, rollbackMarker, error);
            return false;
        }

        uint64 totalCount = 0;
        for (ExactOwnedItem const& item : items)
        {
            if (item.entry != grantItemEntry)
            {
                MarkRollbackDebt(grantId, rollbackMarker,
                    "物品entry不匹配GUID:" + std::to_string(item.guid));
                return false;
            }
            totalCount += item.count;
        }
        if (totalCount != grantItemCount)
        {
            MarkRollbackDebt(grantId, rollbackMarker, "物品数量已变化，无法精确追回");
            return false;
        }

        Player* online = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, grantCharacterGuid));
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        if (online)
        {
            for (ExactOwnedItem const& itemRow : items)
            {
                Item* item = online->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemRow.guid));
                if (!item || item->IsInTrade() || !item->IsSoulBound() ||
                    (item->IsEquipped() && online->CanUnequipItem(item->GetPos(), false) != EQUIP_ERR_OK))
                {
                    MarkRollbackDebt(grantId, rollbackMarker,
                        "在线物品无法安全回收GUID:" + std::to_string(itemRow.guid));
                    return false;
                }
            }
            for (ExactOwnedItem const& itemRow : items)
            {
                Item* item = online->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemRow.guid));
                online->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            }
            online->SaveInventoryAndGoldToDB(trans);
        }
        else
        {
            for (ExactOwnedItem const& itemRow : items)
            {
                Item::DeleteFromInventoryDB(trans, itemRow.guid);
                Item::DeleteFromDB(trans, itemRow.guid);
            }
        }

        trans->Append(
            "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
            "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
            "WHERE `流水ID`={} AND `处理令牌`='{}'",
            grantId, EscapeCharacterString(rollbackMarker));
        CharacterDatabase.DirectCommitTransaction(trans);
        return true;
    }

    if (action != PromotionRewardPolicy::RollbackAction::RollbackRedeemedCdk || !exchange)
    {
        MarkRollbackDebt(grantId, rollbackMarker, "未知追回动作");
        return false;
    }

    Field* exchangeFields = exchange->Fetch();
    uint64 exchangeId = exchangeFields[0].Get<uint64>();
    uint32 redeemAccountId = exchangeFields[1].Get<uint32>();
    uint32 redeemCharacterGuid = exchangeFields[2].Get<uint32>();
    uint32 beforeDays = exchangeFields[3].Get<uint32>();
    uint32 oldPromotionGuid = exchangeFields[4].Get<uint32>();
    uint32 newPromotionGuid = exchangeFields[5].Get<uint32>();
    std::string newItemGuidsText = exchangeFields[6].Get<std::string>();
    std::string resourceSnapshot = exchangeFields[7].Get<std::string>();
    std::string exchangeRollbackStatus = exchangeFields[8].Get<std::string>();

    if (exchangeRollbackStatus == "SUCCESS")
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
            "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
            "WHERE `流水ID`={} AND `处理令牌`='{}'",
            grantId, EscapeCharacterString(rollbackMarker));
        return true;
    }
    if (exchangeRollbackStatus == "DEBT")
    {
        MarkRollbackDebt(grantId, rollbackMarker, "兑换流水已经是追回欠账");
        return false;
    }

    if (redeemCharacterGuid == 0 && IsSafeCode(code))
    {
        if (QueryResult fallback = WorldDatabase.Query(
            "SELECT `兑换角色` FROM `_奖励_兑换码` WHERE `兑换码`='{}'", EscapeWorldString(code)))
        {
            std::vector<uint32> fallbackGuids;
            if (ParseGuidList(fallback->Fetch()[0].Get<std::string>(), fallbackGuids))
                redeemCharacterGuid = fallbackGuids.back();
        }
    }

    Player* player = redeemCharacterGuid == 0 ? nullptr :
        ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, redeemCharacterGuid));
    if (!player || !player->GetSession())
    {
        CharacterDatabase.DirectExecute(
            "UPDATE `_宣传奖励流水` SET `处理令牌`='',`回滚错误`='实际兑换角色离线，等待上线后精确追回',"
            "`更新时间`=NOW() WHERE `流水ID`={} AND `处理令牌`='{}' AND `回滚状态`='PENDING'",
            grantId, EscapeCharacterString(rollbackMarker));
        return false;
    }
    if (redeemAccountId != 0 && player->GetSession()->GetAccountId() != redeemAccountId)
    {
        MarkRollbackDebt(grantId, rollbackMarker, "实际兑换角色与兑换账号不匹配");
        return false;
    }

    std::vector<uint32> newItemGuids;
    if (!ParseGuidList(newItemGuidsText, newItemGuids))
    {
        MarkRollbackDebt(grantId, rollbackMarker, "兑换流水缺少新增物品GUID");
        return false;
    }
    AppendUniqueGuid(newItemGuids, newPromotionGuid);

    std::vector<ExactOwnedItem> items;
    std::string error;
    if (!LoadExactOwnedItems(redeemCharacterGuid, newItemGuids, false, items, error))
    {
        MarkRollbackDebt(grantId, rollbackMarker, error);
        return false;
    }
    std::map<uint32, uint32> expectedItemCounts;
    if (!LoadExpectedItemCounts(resourceSnapshot, newItemGuids, expectedItemCounts, error))
    {
        MarkRollbackDebt(grantId, rollbackMarker, error);
        return false;
    }
    for (ExactOwnedItem const& itemRow : items)
    {
        if (itemRow.count != expectedItemCounts[itemRow.guid])
        {
            MarkRollbackDebt(grantId, rollbackMarker,
                "物品数量已变化GUID:" + std::to_string(itemRow.guid));
            return false;
        }
        Item* item = player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemRow.guid));
        if (!item || item->IsInTrade() ||
            (item->IsEquipped() && player->CanUnequipItem(item->GetPos(), false) != EQUIP_ERR_OK))
        {
            MarkRollbackDebt(grantId, rollbackMarker,
                "在线物品无法安全回收GUID:" + std::to_string(itemRow.guid));
            return false;
        }
    }

    int64 moneyDelta = 0;
    std::map<std::string, int64> resourceDeltas;
    if (!LoadResourceRollback(resourceSnapshot, moneyDelta, resourceDeltas, error))
    {
        MarkRollbackDebt(grantId, rollbackMarker, error);
        return false;
    }

    int256 currentMoney = player->GetMoney();
    if (moneyDelta > 0 && currentMoney < static_cast<int256>(moneyDelta))
    {
        MarkRollbackDebt(grantId, rollbackMarker, "金币已消耗，无法精确追回");
        return false;
    }
    int256 reversedMoney = currentMoney - static_cast<int256>(moneyDelta);
    if (reversedMoney < 0 || reversedMoney > Acore::Number::GetDecimal65SignedMax())
    {
        MarkRollbackDebt(grantId, rollbackMarker, "金币回滚结果越界");
        return false;
    }

    std::map<std::string, uint64> reversedResources;
    bool hasNonZeroResource = std::any_of(resourceDeltas.begin(), resourceDeltas.end(),
        [](auto const& entry) { return entry.second != 0; });
    bool writeCurrencyRollback = sCurrencySystem->IsEnabled();
    if (hasNonZeroResource && !writeCurrencyRollback)
    {
        MarkRollbackDebt(grantId, rollbackMarker, "货币系统未启用，无法追回资源");
        return false;
    }
    if (writeCurrencyRollback)
    {
        for (auto const& [name, delta] : resourceDeltas)
        {
            uint64 current = sCurrencySystem->GetCurrency(player, name);
            uint64 reversed = 0;
            if (!ComputeReversedUnsigned(current, delta, reversed))
            {
                MarkRollbackDebt(grantId, rollbackMarker, name + "已消耗或回滚越界");
                return false;
            }
            reversedResources[name] = reversed;
        }
    }

    uint32 restoreEntry = beforeDays == 0 ? 0 : sPromotionRewardMgr->GetWeaponEntryForLevel(beforeDays);
    if (restoreEntry != 0 && !sObjectMgr->GetItemTemplate(restoreEntry))
    {
        MarkRollbackDebt(grantId, rollbackMarker, "旧宣传神器模板不存在");
        return false;
    }

    Item* pendingRestoreItem = nullptr;
    uint16 restorePosition = 0;
    if (restoreEntry != 0)
    {
        Item* currentPromotionItem = newPromotionGuid == 0 ? nullptr :
            player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(newPromotionGuid));
        if (!currentPromotionItem)
        {
            MarkRollbackDebt(grantId, rollbackMarker, "缺少本次新宣传神器，无法恢复旧等级");
            return false;
        }

        restorePosition = currentPromotionItem->GetPos();
        pendingRestoreItem = Item::CreateItem(
            restoreEntry, 1, player, false, Item::GenerateItemRandomPropertyId(restoreEntry));
        if (!pendingRestoreItem)
        {
            MarkRollbackDebt(grantId, rollbackMarker, "预创建旧宣传神器失败");
            return false;
        }
        pendingRestoreItem->SetBinding(true);
    }

    for (ExactOwnedItem const& itemRow : items)
    {
        Item* item = player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemRow.guid));
        player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
    }

    Item* restoredItem = nullptr;
    if (pendingRestoreItem)
    {
        ItemPosCountVec restoreDest;
        restoreDest.emplace_back(restorePosition, 1);
        restoredItem = player->StoreItem(restoreDest, pendingRestoreItem, true);
        if (!restoredItem)
        {
            delete pendingRestoreItem;
            MarkRollbackDebt(grantId, rollbackMarker, "恢复旧宣传神器失败");
            return false;
        }
        restoredItem->SetBinding(true);
        restoredItem->SetState(ITEM_CHANGED, player);
    }

    player->SetMoney(reversedMoney);
    PromotionPlayerData* promotionData = sPromotionRewardMgr->GetPlayerData(redeemCharacterGuid, true);
    promotionData->days = beforeDays;

    static constexpr std::array<char const*, 6> currencyNames =
        { "泡点", "积分", "妖币", "魔币", "仙币", "神币" };
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    player->SaveInventoryAndGoldToDB(trans);
    if (writeCurrencyRollback)
    {
        trans->Append(
            "INSERT INTO `玩家货币` (`玩家角色编号`,`泡点`,`积分`,`妖币`,`魔币`,`仙币`,`神币`) "
            "VALUES ({},{},{},{},{},{},{}) ON DUPLICATE KEY UPDATE "
            "`泡点`=VALUES(`泡点`),`积分`=VALUES(`积分`),`妖币`=VALUES(`妖币`),"
            "`魔币`=VALUES(`魔币`),`仙币`=VALUES(`仙币`),`神币`=VALUES(`神币`)",
            redeemCharacterGuid,
            reversedResources[currencyNames[0]], reversedResources[currencyNames[1]],
            reversedResources[currencyNames[2]], reversedResources[currencyNames[3]],
            reversedResources[currencyNames[4]], reversedResources[currencyNames[5]]);
    }
    trans->Append(
        "INSERT INTO `_宣传奖励系统玩家` (`玩家GUID`,`宣传天数`) VALUES ({},{}) "
        "ON DUPLICATE KEY UPDATE `宣传天数`=VALUES(`宣传天数`)",
        redeemCharacterGuid, beforeDays);
    trans->Append(
        "UPDATE `_宣传兑换流水` SET `回滚状态`='SUCCESS',`回滚时间`=NOW(),`回滚错误`='' "
        "WHERE `兑换流水ID`={} AND `回滚状态` IN ('NONE','PENDING','FAILED')",
        exchangeId);
    trans->Append(
        "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
        "`回滚时间`=NOW(),`回滚错误`='',`处理令牌`='' "
        "WHERE `流水ID`={} AND `处理令牌`='{}'",
        grantId, EscapeCharacterString(rollbackMarker));
    CharacterDatabase.DirectCommitTransaction(trans);

    if (writeCurrencyRollback)
        sCurrencySystem->LoadPlayerCurrencies(player);
    sPromotionRewardMgr->SendInfoToClient(player);
    LOG_INFO("server.loading",
        "[宣传审核] 已精确追回CDK奖励 流水={} 角色={} 旧物品GUID={} 恢复物品GUID={}",
        grantId, redeemCharacterGuid, oldPromotionGuid,
        restoredItem ? restoredItem->GetGUID().GetCounter() : 0);
    return true;
}

bool PromotionRewardAuditMgr::RetryRollback(std::uint64_t submissionId)
{
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传奖励流水` SET `回滚状态`='PENDING',`回滚错误`='',`处理令牌`='' "
        "WHERE `提交ID`={} AND `回滚状态` IN ('FAILED','DEBT')", submissionId);
    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传兑换流水` SET `回滚状态`='PENDING',`回滚错误`='' "
        "WHERE `提交ID`={} AND `回滚状态` IN ('FAILED','DEBT')", submissionId);
    QueryResult result = CharacterDatabase.Query(
        "SELECT `回滚状态` FROM `_宣传奖励流水` WHERE `提交ID`={}", submissionId);
    return result && result->Fetch()[0].Get<std::string>() == "PENDING";
}

bool PromotionRewardAuditMgr::ClearRecoveryDebt(
    std::uint64_t submissionId,
    std::uint32_t operatorAccountId,
    std::string const& reason)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT `流水ID`,`回滚状态` FROM `_宣传奖励流水` WHERE `提交ID`={}", submissionId);
    if (!result || result->Fetch()[1].Get<std::string>() != "DEBT")
        return false;

    uint64 grantId = result->Fetch()[0].Get<uint64>();
    std::string safeReason = EscapeCharacterString(reason.empty() ? "管理员确认欠账已线下处理" : reason);
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append(
        "UPDATE `_宣传奖励流水` SET `发放状态`='REVOKED',`回滚状态`='SUCCESS',"
        "`回滚时间`=NOW(),`回滚错误`='{}',`处理令牌`='' WHERE `流水ID`={} AND `回滚状态`='DEBT'",
        safeReason, grantId);
    trans->Append(
        "UPDATE `_宣传兑换流水` SET `回滚状态`='SUCCESS',`回滚时间`=NOW(),`回滚错误`='{}' "
        "WHERE `奖励流水ID`={} AND `回滚状态`='DEBT'",
        safeReason, grantId);
    trans->Append(
        "INSERT INTO `_宣传审核日志` "
        "(`提交ID`,`奖励流水ID`,`操作类型`,`原状态`,`新状态`,`审核人账号ID`,`审核人名称`,`审核理由`) "
        "VALUES ({},{},'DEBT_CLEAR','DEBT','SUCCESS',{},'account:{}','{}')",
        submissionId, grantId, operatorAccountId, operatorAccountId, safeReason);
    CharacterDatabase.DirectCommitTransaction(trans);
    return true;
}

bool PromotionRewardAuditMgr::GetRollbackSummary(std::uint64_t submissionId, std::string& summary) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT s.`审核状态`,g.`流水ID`,g.`发放模式`,g.`发放状态`,g.`回滚状态`,g.`回滚错误`,"
        "g.`CDK`,g.`物品GUID`,COALESCE(r.`兑换角色GUID`,0),COALESCE(r.`回滚状态`,'NONE') "
        "FROM `_宣传提交记录` s INNER JOIN `_宣传奖励流水` g ON g.`提交ID`=s.`提交ID` "
        "LEFT JOIN `_宣传兑换流水` r ON r.`奖励流水ID`=g.`流水ID` WHERE s.`提交ID`={}",
        submissionId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    std::ostringstream stream;
    stream << "提交=" << submissionId
           << " 审核=" << fields[0].Get<std::string>()
           << " 流水=" << fields[1].Get<uint64>()
           << " 模式=" << fields[2].Get<std::string>()
           << " 发放=" << fields[3].Get<std::string>()
           << " 回收=" << fields[4].Get<std::string>()
           << " 错误=" << fields[5].Get<std::string>()
           << " CDK=" << fields[6].Get<std::string>()
           << " 物品GUID=" << fields[7].Get<uint64>()
           << " 兑换角色=" << fields[8].Get<uint64>()
           << " 兑换回收=" << fields[9].Get<std::string>();
    summary = stream.str();
    return true;
}

bool PromotionRewardAuditMgr::ApplyReviewDecision(
    std::uint64_t submissionId,
    bool approved,
    std::uint32_t reviewerAccountId,
    std::string const& reason)
{
    QueryResult current = CharacterDatabase.Query(
        "SELECT `审核状态` FROM `_宣传提交记录` WHERE `提交ID`={}", submissionId);
    if (!current)
        return false;

    std::string currentStatus = current->Fetch()[0].Get<std::string>();
    std::string nextStatus = approved ? "APPROVED" : "REJECTED";
    if (currentStatus == nextStatus)
        return true;
    if (currentStatus != "PENDING")
        return false;

    std::string reviewerName;
    AccountMgr::GetName(reviewerAccountId, reviewerName);
    if (reviewerName.empty())
        reviewerName = "account:" + std::to_string(reviewerAccountId);

    CharacterDatabase.DirectExecute(
        "UPDATE `_宣传提交记录` SET `审核状态`='{}',`审核人账号ID`={},`审核人名称`='{}',"
        "`审核理由`='{}',`审核处理状态`='PENDING',`审核处理令牌`='' "
        "WHERE `提交ID`={} AND `审核状态`='PENDING'",
        nextStatus, reviewerAccountId, EscapeCharacterString(reviewerName),
        EscapeCharacterString(reason), submissionId);

    QueryResult updated = CharacterDatabase.Query(
        "SELECT `审核状态`,`审核人账号ID` FROM `_宣传提交记录` WHERE `提交ID`={}", submissionId);
    return updated && updated->Fetch()[0].Get<std::string>() == nextStatus &&
        updated->Fetch()[1].Get<uint32>() == reviewerAccountId;
}

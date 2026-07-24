#include "AddonThrottle.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "HermesBridgeAddonApi.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "RequirementSystem.h"
#include "RewardTemplate.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"

#if __has_include("WearControl.h")
    #include "WearControl.h"
    #define SYNTHESIS_HAS_WEAR_CONTROL 1
#else
    #define SYNTHESIS_HAS_WEAR_CONTROL 0
#endif

#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
constexpr char SYNTHESIS_ADDON_PREFIX[] = "SYNTHSYS";
constexpr size_t SYNTHESIS_MAX_ADDON_PAYLOAD = 220;

struct SynthesisEntry
{
    uint32 itemId = 0;
    uint32 upgradeLevel = 0;
    uint8 classType = 0;
    uint32 requirementId = 0;
    uint32 rewardId = 0;
    float successChance = 100.0f;
    uint32 boosterItemId = 0;
    float boosterChance = 0.0f;
    bool destroyOnFail = false;
    uint32 unlockWearLevel = 0;
    uint32 unlockItemId = 0;
};

std::string TrimAddonText(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '|' || ch == '^' || ch == '~' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }
    return value;
}

std::string LimitAddonText(std::string value, size_t maxLength)
{
    value = TrimAddonText(value);
    if (value.length() <= maxLength)
        return value;

    if (maxLength <= 3)
        return value.substr(0, maxLength);

    size_t end = maxLength - 3;
    while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80)
        --end;

    return value.substr(0, end) + "...";
}

std::string FormatChance(float value)
{
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(value == static_cast<uint32>(value) ? 0 : 1);
    ss << value;
    return ss.str();
}

std::string GetItemName(uint32 itemId)
{
    if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId))
        return TrimAddonText(itemTemplate->Name1);

    return "未知物品";
}

std::string GetItemIconPath(uint32 /*itemId*/)
{
    return "";
}

std::string GetRewardItemList(uint32 rewardId)
{
    if (rewardId == 0)
        return "";

    QueryResult result = WorldDatabase.Query("SELECT `奖励物品` FROM `_模板_奖励` WHERE `id` = {}", rewardId);
    if (!result)
        return "";

    return TrimAddonText((*result)[0].Get<std::string>());
}

std::pair<uint32, uint32> ParseFirstRewardItem(std::string const& rewardItems)
{
    std::istringstream ss(rewardItems);
    uint32 itemId = 0;
    uint32 count = 0;
    ss >> itemId >> count;
    return { itemId, count ? count : 1 };
}

std::string FormatMoney(uint64 copper)
{
    if (!copper)
        return "";

    std::ostringstream ss;
    ss << "金币 " << (copper / 10000);
    uint64 silver = (copper % 10000) / 100;
    uint64 remainCopper = copper % 100;
    if (silver)
        ss << "金" << silver << "银";
    if (remainCopper)
        ss << remainCopper << "铜";
    return ss.str();
}

void AppendRequirementPart(std::vector<std::string>& parts, std::string const& label, uint64 value)
{
    if (value == 0)
        return;

    std::ostringstream ss;
    ss << label << " " << value;
    parts.push_back(ss.str());
}

std::string BuildRequirementSummary(uint32 requirementId)
{
    if (requirementId == 0)
        return "无额外材料";

    QueryResult result = WorldDatabase.Query(
        "SELECT `需要人物等级`, `消耗金币`, `消耗泡点`, `消耗积分`, `消耗妖币`, `消耗魔币`, "
        "`消耗仙币`, `消耗神币`, `消耗战场分数`, `消耗荣誉点数`, `消耗成就点数`, `是否消耗物品`, "
        "`消耗物品` FROM `_模板_需求` WHERE `id` = {}", requirementId);

    if (!result)
        return "需求模板未找到";

    Field* fields = result->Fetch();
    std::vector<std::string> parts;

    std::string level = fields[0].Get<std::string>();
    if (!level.empty() && level != "0")
        parts.push_back("角色等级 " + level);

    std::string money = FormatMoney(fields[1].Get<uint64>());
    if (!money.empty())
        parts.push_back(money);

    AppendRequirementPart(parts, "泡点", fields[2].Get<uint32>());
    AppendRequirementPart(parts, "积分", fields[3].Get<uint32>());
    AppendRequirementPart(parts, "妖币", fields[4].Get<uint32>());
    AppendRequirementPart(parts, "魔币", fields[5].Get<uint32>());
    AppendRequirementPart(parts, "仙币", fields[6].Get<uint32>());
    AppendRequirementPart(parts, "神币", fields[7].Get<uint32>());
    AppendRequirementPart(parts, "战场分数", fields[8].Get<uint32>());
    AppendRequirementPart(parts, "荣誉点数", fields[9].Get<uint32>());
    AppendRequirementPart(parts, "成就点数", fields[10].Get<uint32>());

    std::string items = TrimAddonText(fields[12].Get<std::string>());
    if (!items.empty())
    {
        std::vector<std::string> itemParts;
        std::istringstream itemPairs(items);
        std::string itemPair;
        while (std::getline(itemPairs, itemPair, ','))
        {
            std::istringstream itemStream(itemPair);
            uint32 itemId = 0;
            uint32 count = 0;
            itemStream >> itemId >> count;
            if (itemId)
            {
                std::ostringstream itemText;
                itemText << GetItemName(itemId) << " x" << (count ? count : 1);
                itemParts.push_back(itemText.str());
            }
        }

        if (!itemParts.empty())
        {
            std::ostringstream ss;
            ss << (fields[11].Get<uint32>() == 1 ? "持有" : "消耗") << "物品 ";
            for (size_t i = 0; i < itemParts.size(); ++i)
            {
                if (i)
                    ss << ", ";
                ss << itemParts[i];
            }
            parts.push_back(ss.str());
        }
    }

    if (parts.empty())
        return "无额外材料";

    std::ostringstream summary;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
            summary << " / ";
        summary << parts[i];
    }
    return summary.str();
}

std::string BuildRequirementItems(Player* player, uint32 requirementId)
{
    if (!player || requirementId == 0)
        return "";

    QueryResult result = WorldDatabase.Query(
        "SELECT `消耗物品` FROM `_模板_需求` WHERE `id` = {}", requirementId);
    if (!result)
        return "";

    std::string items = TrimAddonText((*result)[0].Get<std::string>());
    if (items.empty())
        return "";

    std::ostringstream details;
    bool first = true;
    std::istringstream itemPairs(items);
    std::string itemPair;
    while (std::getline(itemPairs, itemPair, ','))
    {
        std::istringstream itemStream(itemPair);
        uint32 itemId = 0;
        uint32 count = 0;
        itemStream >> itemId >> count;
        if (!itemId)
            continue;

        if (!first)
            details << ';';
        first = false;

        details << itemId << ':'
                << (count ? count : 1) << ':'
                << player->GetItemCount(itemId, true) << ':'
                << LimitAddonText(GetItemName(itemId), 64) << ':'
                << GetItemIconPath(itemId);
    }

    return details.str();
}

void SendSynthesisPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() > SYNTHESIS_MAX_ADDON_PAYLOAD)
    {
        size_t totalChunks = (payload.length() + SYNTHESIS_MAX_ADDON_PAYLOAD - 1) / SYNTHESIS_MAX_ADDON_PAYLOAD;
        for (size_t index = 0; index < totalChunks; ++index)
        {
            size_t start = index * SYNTHESIS_MAX_ADDON_PAYLOAD;
            size_t length = std::min(SYNTHESIS_MAX_ADDON_PAYLOAD, payload.length() - start);

            std::ostringstream chunkPayload;
            chunkPayload << "CHUNK:" << (index + 1) << ':' << totalChunks << ':'
                         << payload.substr(start, length);
            if (HermesBridge_SendAddonMessage(player, SYNTHESIS_ADDON_PREFIX, chunkPayload.str()))
                continue;

            std::string fullChunkMessage = std::string(SYNTHESIS_ADDON_PREFIX) + '\t' + chunkPayload.str();
            WorldPacket chunkData;
            ChatHandler::BuildChatPacket(chunkData, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullChunkMessage, 0);
            player->SendDirectMessage(&chunkData);
        }
        return;
    }

    if (HermesBridge_SendAddonMessage(player, SYNTHESIS_ADDON_PREFIX, payload))
        return;

    std::string fullMessage = std::string(SYNTHESIS_ADDON_PREFIX) + '\t' + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
}

std::vector<std::string> SplitFields(std::string const& text, char delimiter)
{
    std::vector<std::string> fields;
    std::string current;
    std::istringstream ss(text);
    while (std::getline(ss, current, delimiter))
        fields.push_back(current);
    return fields;
}

uint32 ToUInt32(std::string const& value)
{
    return static_cast<uint32>(std::strtoul(value.c_str(), nullptr, 10));
}

bool SynthesisColumnExists(char const* columnName)
{
    QueryResult result = WorldDatabase.Query(
        "SELECT COUNT(*) FROM `information_schema`.`COLUMNS` "
        "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '_物品合成' AND `COLUMN_NAME` = '{}'",
        columnName);

    if (!result)
        return false;

    return result->Fetch()[0].Get<uint64>() > 0;
}

bool CanUnlockWearLevelFromSynthesis(Player* player, uint32 unlockItemId, uint32 wearLevel, std::string& message)
{
    if (wearLevel == 0)
        return true;

#if SYNTHESIS_HAS_WEAR_CONTROL
    uint8 const limitType = WearControl::GetExclusiveLimit(unlockItemId);
    std::vector<uint8> const slots = WearControl::GetItemWearSlots(unlockItemId, limitType);
    if (limitType == WEAR_LIMIT_NONE || slots.empty())
    {
        message = "产物没有配置可解锁的专属槽位";
        return false;
    }

    for (uint8 slotPosition : slots)
        if (!WearControl::CanUnlockPlayerWearLevel(player, limitType, slotPosition, wearLevel, &message))
            return false;

    return true;
#else
    message = "当前未编译穿戴控制模块，无法校验穿戴等级";
    return false;
#endif
}

bool UnlockWearLevelFromSynthesis(Player* player, uint32 unlockItemId, uint32 wearLevel, std::string& message)
{
    LOG_INFO("server.loading", "[穿戴权限-合成解锁入口] 玩家={} GUID={} 产物={} 请求等级={}",
        player ? player->GetName() : "<null>", player ? player->GetGUID().GetCounter() : 0, unlockItemId, wearLevel);

    if (wearLevel == 0)
        return true;

#if SYNTHESIS_HAS_WEAR_CONTROL
    uint8 const limitType = WearControl::GetExclusiveLimit(unlockItemId);
    std::vector<uint8> const slots = WearControl::GetItemWearSlots(unlockItemId, limitType);
    LOG_INFO("server.loading", "[穿戴权限-槽位解析] 产物={} 限制类型={} 槽位数量={}",
        unlockItemId, uint32(limitType), uint32(slots.size()));
    if (limitType == WEAR_LIMIT_NONE || slots.empty())
    {
        message = "合成成功，但产物没有配置可解锁的专属槽位";
        return false;
    }

    for (uint8 slotPosition : slots)
    {
        bool const unlocked = WearControl::UnlockPlayerWearLevel(player, limitType, slotPosition, wearLevel, false);
        LOG_INFO("server.loading", "[穿戴权限-槽位解锁结果] 玩家={} 产物={} 类型={} 槽位={} 请求等级={} 结果={}",
            player ? player->GetName() : "<null>", unlockItemId, uint32(limitType), uint32(slotPosition), wearLevel, unlocked);
        if (!unlocked)
        {
            message = "合成成功，但相关槽位穿戴权限解锁失败，请检查 `_穿戴等级权限` 表结构";
            return false;
        }
    }

    message = "合成成功，已解锁仙器穿戴权限";
    return true;
#else
    message = "合成成功，但当前未编译穿戴控制模块，无法解锁穿戴等级";
    return false;
#endif
}

class SynthesisSystemMgr
{
public:
    static SynthesisSystemMgr* instance()
    {
        static SynthesisSystemMgr instance;
        return &instance;
    }

    void LoadConfig(bool /*reload*/)
    {
        _enabled = sConfigMgr->GetOption<bool>("SynthesisSystem.Enable", true);
        _announceStartup = sConfigMgr->GetOption<bool>("SynthesisSystem.AnnounceStartup", true);
        _throttleKey = sConfigMgr->GetOption<std::string>("SynthesisSystem.AddonThrottleKey", "SYNTHSYS");
    }

    bool IsEnabled() const { return _enabled; }

    uint32 LoadEntries()
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _entries.clear();

        bool const hasUnlockWearLevelColumn = SynthesisColumnExists("解锁穿戴等级");
        LOG_INFO("server.loading", "[穿戴权限-配方字段检测] `_物品合成`.`解锁穿戴等级` 存在={}", hasUnlockWearLevelColumn);
        if (!hasUnlockWearLevelColumn)
            LOG_INFO("server.loading", "合成系统: `_物品合成`.`解锁穿戴等级` 字段不存在，合成成功不授予穿戴等级");

        QueryResult result = hasUnlockWearLevelColumn
            ? WorldDatabase.Query(
                "SELECT `物品id`, `升级等级`, `职业类型`, `需求id`, `升级成功奖励id`, `成功几率`, "
                "`合成几率物品id`, `合成几率提升`, `失败是否摧毁`, `解锁穿戴等级` "
                "FROM `_物品合成` ORDER BY `物品id`, `升级等级`")
            : WorldDatabase.Query(
                "SELECT `物品id`, `升级等级`, `职业类型`, `需求id`, `升级成功奖励id`, `成功几率`, "
                "`合成几率物品id`, `合成几率提升`, `失败是否摧毁`, 0 AS `解锁穿戴等级` "
                "FROM `_物品合成` ORDER BY `物品id`, `升级等级`");

        if (!result)
            return 0;

        uint32 count = 0;
        uint32 unlockCount = 0;
        do
        {
            Field* fields = result->Fetch();

            SynthesisEntry entry;
            entry.itemId = fields[0].Get<uint32>();
            entry.upgradeLevel = fields[1].Get<uint32>();
            entry.classType = fields[2].Get<uint8>();
            entry.requirementId = fields[3].Get<uint32>();
            entry.rewardId = fields[4].Get<uint32>();
            entry.successChance = std::clamp(fields[5].Get<float>(), 0.0f, 100.0f);
            entry.boosterItemId = fields[6].Get<uint32>();
            entry.boosterChance = std::clamp(fields[7].Get<float>(), 0.0f, 100.0f);
            entry.destroyOnFail = fields[8].Get<uint8>() != 0;
            entry.unlockWearLevel = fields[9].Get<uint32>();
            entry.unlockItemId = ParseFirstRewardItem(GetRewardItemList(entry.rewardId)).first;

            if (entry.unlockWearLevel > 0)
                ++unlockCount;
            if (entry.itemId == 95276 || entry.itemId == 95476)
            {
                LOG_INFO("server.loading", "[穿戴权限-配方样本] 原料={} 升级等级={} 奖励模板={} 产物={} 解锁等级={}",
                    entry.itemId, entry.upgradeLevel, entry.rewardId, entry.unlockItemId, entry.unlockWearLevel);
            }

            if (!sObjectMgr->GetItemTemplate(entry.itemId))
            {
                LOG_WARN("server.loading", "合成系统: 物品 {} 不存在，跳过配置", entry.itemId);
                continue;
            }

            if (entry.boosterItemId && !sObjectMgr->GetItemTemplate(entry.boosterItemId))
            {
                LOG_WARN("server.loading", "合成系统: 几率提升物品 {} 不存在，跳过配置 {}-{}",
                    entry.boosterItemId, entry.itemId, entry.upgradeLevel);
                continue;
            }

            _entries[{ entry.itemId, entry.upgradeLevel }] = entry;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", "[穿戴权限-配方加载完成] 总数={} 含解锁配置={}", count, unlockCount);
        return count;
    }

    void SendList(Player* player)
    {
        if (!player)
            return;

        std::vector<SynthesisEntry> entries;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            for (auto const& pair : _entries)
                entries.push_back(pair.second);
        }

        std::ostringstream payload;
        payload << "SS_LIST:";
        bool first = true;

        for (SynthesisEntry const& entry : entries)
        {
            uint32 sourceCount = player->GetItemCount(entry.itemId, true);
            uint32 boosterCount = entry.boosterItemId ? player->GetItemCount(entry.boosterItemId, true) : 0;
            bool requirementOk = true;
            if (entry.requirementId != 0)
            {
                RequirementSystem* requirementSystem = sRequirementSystem;
                requirementOk = requirementSystem && requirementSystem->CheckRequirements(player, entry.requirementId, false);
            }

            std::vector<std::string> rewardDescriptions;
            if (entry.rewardId != 0 && sRewardTemplate->IsEnabled())
                rewardDescriptions = sRewardTemplate->GetRewardDescription(player, entry.rewardId);

            std::string rewardText = "无奖励";
            if (!rewardDescriptions.empty())
            {
                rewardText.clear();
                for (size_t i = 0; i < rewardDescriptions.size(); ++i)
                {
                    if (i)
                        rewardText += " / ";
                    rewardText += rewardDescriptions[i];
                }
            }

            std::string rewardItems = GetRewardItemList(entry.rewardId);
            auto [nextItemId, nextItemCount] = ParseFirstRewardItem(rewardItems);
            std::string nextItemName = nextItemId ? GetItemName(nextItemId) : rewardText;
            std::string requirementSummary = BuildRequirementSummary(entry.requirementId);
            std::string requirementItems = BuildRequirementItems(player, entry.requirementId);

            if (!first)
                payload << '~';
            first = false;

            payload << entry.itemId << '^'
                    << entry.upgradeLevel << '^'
                    << entry.requirementId << '^'
                    << entry.rewardId << '^'
                    << FormatChance(entry.successChance) << '^'
                    << entry.boosterItemId << '^'
                    << FormatChance(entry.boosterChance) << '^'
                    << (entry.destroyOnFail ? 1 : 0) << '^'
                    << LimitAddonText(GetItemName(entry.itemId), 64) << '^'
                    << GetItemIconPath(entry.itemId) << '^'
                    << nextItemId << '^'
                    << LimitAddonText(nextItemName, 64) << '^'
                    << GetItemIconPath(nextItemId) << '^'
                    << nextItemCount << '^'
                    << (requirementOk ? 1 : 0) << '^'
                    << sourceCount << '^'
                    << boosterCount << '^'
                    << LimitAddonText(requirementSummary, 180) << '^'
                    << LimitAddonText(rewardText, 110) << '^'
                    << static_cast<uint32>(entry.classType) << '^'
                    << requirementItems << '^'
                    << entry.unlockWearLevel;
        }

        SendSynthesisPayload(player, payload.str());
    }

    void TrySynthesize(Player* player, uint32 itemId, uint32 upgradeLevel, bool useBooster)
    {
        if (!player)
            return;

        SynthesisEntry entry;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto itr = _entries.find({ itemId, upgradeLevel });
            if (itr == _entries.end())
            {
                SendResult(player, false, itemId, upgradeLevel, "未找到该合成配置");
                return;
            }
            entry = itr->second;
        }

        if (player->GetItemCount(entry.itemId, true) == 0)
        {
            SendResult(player, false, entry.itemId, entry.upgradeLevel, "背包中没有需要合成的物品");
            return;
        }

        if (entry.rewardId == 0 || !sRewardTemplate->IsEnabled() || !sRewardTemplate->GetRewardTemplate(entry.rewardId))
        {
            SendResult(player, false, entry.itemId, entry.upgradeLevel, "合成奖励模板不存在或奖励系统未启用");
            return;
        }

        if (entry.unlockWearLevel > 0)
        {
            std::string prerequisiteError;
            if (!CanUnlockWearLevelFromSynthesis(player, entry.unlockItemId, entry.unlockWearLevel, prerequisiteError))
            {
                SendResult(player, false, entry.itemId, entry.upgradeLevel, prerequisiteError);
                return;
            }
        }

        if (entry.requirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            if (!requirementSystem)
            {
                SendResult(player, false, entry.itemId, entry.upgradeLevel, "需求系统未初始化");
                return;
            }

            if (!requirementSystem->CheckRequirements(player, entry.requirementId, true))
            {
                SendResult(player, false, entry.itemId, entry.upgradeLevel, "未满足合成需求");
                return;
            }
        }

        if (useBooster)
        {
            if (entry.boosterItemId == 0 || entry.boosterChance <= 0.0f)
            {
                SendResult(player, false, entry.itemId, entry.upgradeLevel, "该配置没有可用的几率提升物品");
                return;
            }

            if (player->GetItemCount(entry.boosterItemId, true) == 0)
            {
                SendResult(player, false, entry.itemId, entry.upgradeLevel, "缺少几率提升物品");
                return;
            }
        }

        if (entry.requirementId != 0)
        {
            RequirementSystem* requirementSystem = sRequirementSystem;
            if (!requirementSystem || !requirementSystem->ConsumeRequirements(player, entry.requirementId))
            {
                SendResult(player, false, entry.itemId, entry.upgradeLevel, "消耗合成需求失败");
                return;
            }
        }

        float finalChance = std::clamp(entry.successChance + (useBooster ? entry.boosterChance : 0.0f), 0.0f, 100.0f);
        bool success = roll_chance_f(finalChance);

        if (success)
        {
            bool rewarded = sRewardTemplate->GiveReward(player, entry.rewardId, false, true);
            if (rewarded)
            {
                std::string resultMessage = "合成成功";
                LOG_INFO("server.loading", "[穿戴权限-奖励成功] 玩家={} GUID={} 原料={} 升级等级={} 产物={} 解锁等级={}",
                    player->GetName(), player->GetGUID().GetCounter(), entry.itemId, entry.upgradeLevel,
                    entry.unlockItemId, entry.unlockWearLevel);
                if (entry.unlockWearLevel > 0)
                    UnlockWearLevelFromSynthesis(player, entry.unlockItemId, entry.unlockWearLevel, resultMessage);
                SendResult(player, true, entry.itemId, entry.upgradeLevel, resultMessage);
            }
            else
                SendResult(player, false, entry.itemId, entry.upgradeLevel, "合成成功但发放奖励失败，请检查奖励模板或背包空间");
        }
        else
        {
            SendResult(player, false, entry.itemId, entry.upgradeLevel, "合成失败");
        }

        SendList(player);
    }

    bool AllowAddon(Player* player) const
    {
        if (!player)
            return false;
        return ModuleAddon::Throttle::Allow(player->GetGUID(), _throttleKey.c_str());
    }

    bool ShouldAnnounceStartup() const { return _announceStartup; }

private:
    void SendResult(Player* player, bool success, uint32 itemId, uint32 upgradeLevel, std::string const& message)
    {
        std::ostringstream payload;
        payload << "SS_RESULT:DO^" << (success ? "OK" : "FAIL") << '^'
                << itemId << '^'
                << upgradeLevel << '^'
                << LimitAddonText(message, 120);
        SendSynthesisPayload(player, payload.str());
    }

    bool _enabled = true;
    bool _announceStartup = true;
    std::string _throttleKey = "SYNTHSYS";
    std::map<std::pair<uint32, uint32>, SynthesisEntry> _entries;
    std::mutex _mutex;
};

#define sSynthesisSystemMgr SynthesisSystemMgr::instance()

class SynthesisSystemWorldScript : public WorldScript
{
public:
    SynthesisSystemWorldScript() : WorldScript("SynthesisSystemWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sSynthesisSystemMgr->LoadConfig(reload);
    }

    void OnStartup() override
    {
        if (!sSynthesisSystemMgr->IsEnabled())
        {
            LOG_INFO("server.loading", "→合成系统已禁用");
            return;
        }

        uint32 count = sSynthesisSystemMgr->LoadEntries();
        if (sSynthesisSystemMgr->ShouldAnnounceStartup())
            LOG_INFO("server.loading", "→合成系统√ 已加载 {} 条配置", count);
    }
};

class SynthesisSystemPlayerScript : public PlayerScript
{
public:
    SynthesisSystemPlayerScript()
        : PlayerScript("SynthesisSystemPlayerScript", {
            PLAYERHOOK_ON_CHAT_WITH_RECEIVER
        })
    {
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !sSynthesisSystemMgr->IsEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != SYNTHESIS_ADDON_PREFIX)
            return;

        if (!sSynthesisSystemMgr->AllowAddon(player))
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            sSynthesisSystemMgr->SendList(player);
            return;
        }

        if (command.rfind("DO:", 0) == 0)
        {
            std::vector<std::string> fields = SplitFields(command.substr(3), '^');
            if (fields.size() < 3)
            {
                SendSynthesisPayload(player, "SS_RESULT:DO^FAIL^0^0^合成请求格式错误");
                return;
            }

            uint32 itemId = ToUInt32(fields[0]);
            uint32 upgradeLevel = ToUInt32(fields[1]);
            bool useBooster = ToUInt32(fields[2]) != 0;
            sSynthesisSystemMgr->TrySynthesize(player, itemId, upgradeLevel, useBooster);
            return;
        }
    }
};
}

void AddSC_mod_synthesis_system()
{
    new SynthesisSystemWorldScript();
    new SynthesisSystemPlayerScript();
}

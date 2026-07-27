/*
 * 宣传奖励系统 (mod-promotion-reward) - 主实现
 */

#include "PromotionRewardModule.h"
#include "PromotionRewardAudit.h"
#include "AddonThrottle.h"
#include "Bag.h"
#include "loader.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "HermesBridgeAddonApi.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSessionMgr.h"
#include <algorithm>
#include <limits>
#include <random>
#include <sstream>
#include <utility>

extern void AddSC_PromotionReward_CommandScript();

constexpr char const* PROMO_ADDON_PREFIX = "PROMOREWARD";

namespace
{
void SnapshotItemCounts(Player* player, std::unordered_map<uint32, uint32>& counts)
{
    counts.clear();
    if (!player)
        return;

    auto collect = [&counts](Item* item)
    {
        if (item)
            counts[item->GetGUID().GetCounter()] = item->GetCount();
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
}
}

PromotionRewardMgr* PromotionRewardMgr::instance()
{
    static PromotionRewardMgr s;
    return &s;
}

uint64 PromotionRewardMgr::BuildPlayerKey(uint32 guid, uint32 groupId)
{
    return (static_cast<uint64>(groupId) << 32) | static_cast<uint64>(guid);
}

PromotionConfig const& PromotionRewardMgr::GetConfig() const
{
    if (PromotionConfig const* config = GetConfigForGroup(_defaultGroupId))
        return *config;
    return _disabledConfig;
}

PromotionConfig const* PromotionRewardMgr::GetConfigForGroup(uint32 groupId) const
{
    auto it = _configs.find(groupId);
    return it == _configs.end() ? nullptr : &it->second;
}

void PromotionRewardMgr::LoadConfig()
{
    _enabled = false;
    _debugLog = sConfigMgr->GetOption<bool>("PromotionReward.DebugLog", false);
    _defaultGroupId = 0;
    _configs.clear();
    _disabledConfig = {};
    _disabledConfig.enabled = false;
    _disabledConfig.debugLog = _debugLog;

    if (!sConfigMgr->GetOption<bool>("PromotionReward.Enable", true))
    {
        LOG_INFO("server.loading", "宣传奖励系统已在配置文件中禁用");
        return;
    }

    QueryResult r = WorldDatabase.Query(
        "SELECT `id`,`启用`,`默认活动`,`武器名称`,`武器entry`,`最大等级`,"
        "`初始全属性值`,`每日增量`,`对接奖励组`,`对接需求ID`,`对接奖励ID`,`领取公告` "
        "FROM `_宣传奖励系统` ORDER BY `默认活动` DESC,`id`");

    if (!r)
    {
        LOG_WARN("server.loading",
            "[宣传奖励] world.`_宣传奖励系统` 没有可用活动配置,模块禁用。请导入最新 SQL");
        return;
    }

    bool defaultSelected = false;
    do
    {
        Field* f = r->Fetch();
        PromotionConfig config;
        config.id              = f[0].Get<uint32>();
        config.enabled         = f[1].Get<bool>();
        config.isDefault       = f[2].Get<bool>();
        config.weaponName      = f[3].Get<std::string>();
        config.weaponEntry     = f[4].Get<uint32>();
        config.maxLevel        = f[5].Get<uint32>();
        config.baseAttrValue   = f[6].Get<int256>();
        config.perDayAttrValue = f[7].Get<int256>();
        config.groupId         = f[8].Get<uint32>();
        config.requireId       = f[9].Get<uint32>();
        config.rewardId        = f[10].Get<uint32>();
        config.announceType    = f[11].Get<uint32>();
        config.debugLog        = _debugLog;

        uint64 maxEntry = config.maxLevel == 0 ? 0 :
            static_cast<uint64>(config.weaponEntry) + static_cast<uint64>(config.maxLevel) - 1;
        uint64 maxRewardId = config.maxLevel == 0 ? 0 :
            static_cast<uint64>(config.rewardId) + static_cast<uint64>(config.maxLevel) - 1;
        if (!config.enabled)
            continue;
        if (config.groupId == 0 || config.weaponEntry == 0 || config.maxLevel == 0 || config.rewardId == 0 ||
            maxEntry > std::numeric_limits<uint32>::max() || maxRewardId > std::numeric_limits<uint32>::max())
        {
            LOG_WARN("server.loading", "[宣传奖励] 活动ID={} 配置无效,已跳过", config.id);
            continue;
        }
        if (_configs.find(config.groupId) != _configs.end())
        {
            LOG_WARN("server.loading", "[宣传奖励] 奖励组={} 重复配置,活动ID={} 已跳过", config.groupId, config.id);
            continue;
        }

        if (config.weaponName.empty())
            config.weaponName = "宣传武器";
        std::replace(config.weaponName.begin(), config.weaponName.end(), '|', ' ');
        _configs.emplace(config.groupId, std::move(config));
        PromotionConfig const& stored = _configs.at(f[8].Get<uint32>());
        if (stored.isDefault && !defaultSelected)
        {
            _defaultGroupId = stored.groupId;
            defaultSelected = true;
        }
        else if (_defaultGroupId == 0)
            _defaultGroupId = stored.groupId;
    } while (r->NextRow());

    for (auto& [groupId, config] : _configs)
    {
        uint32 maxEntry = config.weaponEntry + config.maxLevel - 1;
        QueryResult attrResult = WorldDatabase.Query(
            "SELECT `entry`,`stat_value1` FROM `item_template` "
            "WHERE `entry` BETWEEN {} AND {} ORDER BY `entry`",
            config.weaponEntry, maxEntry);

        bool validCurve = attrResult != nullptr;
        uint32 expectedEntry = config.weaponEntry;
        config.levelAttrValues.clear();
        config.levelAttrValues.reserve(config.maxLevel);
        if (attrResult)
        {
            do
            {
                Field* fields = attrResult->Fetch();
                uint32 entry = fields[0].Get<uint32>();
                int256 attrValue = fields[1].Get<int256>();
                if (entry != expectedEntry || attrValue <= 0)
                {
                    validCurve = false;
                    break;
                }
                config.levelAttrValues.push_back(attrValue);
                ++expectedEntry;
            } while (attrResult->NextRow());
        }

        if (!validCurve || config.levelAttrValues.size() != config.maxLevel)
        {
            config.levelAttrValues.clear();
            LOG_WARN("server.loading",
                "[宣传奖励] 活动ID={} 奖励组={} 的逐级物品属性不完整,回退到初始值+线性增量",
                config.id, groupId);
        }
    }

    if (_configs.empty() || _defaultGroupId == 0)
    {
        LOG_WARN("server.loading", "[宣传奖励] 没有通过校验的活动配置,模块禁用");
        return;
    }

    _enabled = true;
    PromotionConfig const& defaultConfig = GetConfig();
    LOG_INFO("server.loading", "[宣传奖励] 已加载 {} 个活动,默认活动={} 奖励组={} 武器={} 最大等级={}",
        _configs.size(), defaultConfig.id, defaultConfig.groupId, defaultConfig.weaponName, defaultConfig.maxLevel);
}

void PromotionRewardMgr::LoadAllPlayers()
{
    _players.clear();

    if (QueryResult r = CharacterDatabase.Query(
        "SELECT `奖励组`,`玩家GUID`,`宣传天数` FROM `_宣传奖励系统玩家`"))
    {
        do
        {
            Field* f = r->Fetch();
            uint32 groupId = f[0].Get<uint32>();
            uint32 guid = f[1].Get<uint32>();
            _players[BuildPlayerKey(guid, groupId)].days = f[2].Get<uint32>();
        } while (r->NextRow());
    }
}

PromotionPlayerData* PromotionRewardMgr::GetPlayerData(uint32 guid, bool createIfMissing)
{
    return GetPlayerDataForGroup(guid, _defaultGroupId, createIfMissing);
}

PromotionPlayerData* PromotionRewardMgr::GetPlayerDataForGroup(uint32 guid, uint32 groupId, bool createIfMissing)
{
    if (guid == 0 || !GetConfigForGroup(groupId))
        return nullptr;

    uint64 key = BuildPlayerKey(guid, groupId);
    auto it = _players.find(key);
    if (it != _players.end())
        return &it->second;

    if (!createIfMissing)
        return nullptr;

    return &_players[key];
}

void PromotionRewardMgr::SavePlayerData(uint32 guid)
{
    SavePlayerDataForGroup(guid, _defaultGroupId);
}

void PromotionRewardMgr::SavePlayerDataForGroup(uint32 guid, uint32 groupId)
{
    auto it = _players.find(BuildPlayerKey(guid, groupId));
    if (it == _players.end())
        return;

    CharacterDatabase.Execute(
        "INSERT INTO `_宣传奖励系统玩家` (`奖励组`,`玩家GUID`,`宣传天数`) "
        "VALUES ({},{},{}) "
        "ON DUPLICATE KEY UPDATE `宣传天数`=VALUES(`宣传天数`)",
        groupId, guid, it->second.days);
}

bool PromotionRewardMgr::BeginItemCapture(Player* player, uint64 operationId)
{
    if (!player || operationId == 0)
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    if (_itemCaptures.find(guid) != _itemCaptures.end())
        return false;

    PromotionItemCaptureContext context;
    context.operationId = operationId;
    SnapshotItemCounts(player, context.initialItemCounts);
    _itemCaptures.emplace(guid, std::move(context));
    return true;
}

std::vector<uint32> PromotionRewardMgr::EndItemCapture(
    Player* player,
    uint64 operationId,
    bool* receiptExact,
    std::string* receiptError)
{
    if (!player || operationId == 0)
        return {};

    uint32 guid = player->GetGUID().GetCounter();
    auto it = _itemCaptures.find(guid);
    if (it == _itemCaptures.end() || it->second.operationId != operationId)
        return {};

    if (receiptExact)
        *receiptExact = it->second.receiptExact;
    if (receiptError)
        *receiptError = it->second.receiptError;

    std::vector<uint32> itemGuids = std::move(it->second.itemGuids);
    _itemCaptures.erase(it);
    return itemGuids;
}

void PromotionRewardMgr::CancelItemCapture(Player* player, uint64 operationId)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    auto it = _itemCaptures.find(guid);
    if (it == _itemCaptures.end())
        return;

    if (operationId == 0 || it->second.operationId == operationId)
        _itemCaptures.erase(it);
}

void PromotionRewardMgr::RecordCapturedItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    auto it = _itemCaptures.find(player->GetGUID().GetCounter());
    if (it == _itemCaptures.end())
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    if (itemGuid == 0)
        return;

    auto initial = it->second.initialItemCounts.find(itemGuid);
    if (initial != it->second.initialItemCounts.end())
    {
        if (item->GetCount() > initial->second)
        {
            it->second.receiptExact = false;
            it->second.receiptError = "奖励物品合并到玩家原有堆叠，无法按新增GUID精确追回";
        }
        return;
    }

    std::vector<uint32>& itemGuids = it->second.itemGuids;
    if (std::find(itemGuids.begin(), itemGuids.end(), itemGuid) == itemGuids.end())
        itemGuids.push_back(itemGuid);
}

void PromotionRewardMgr::ClearAllItemCaptures()
{
    _itemCaptures.clear();
}

std::string PromotionRewardMgr::GenerateUniqueCode()
{
    // 兑换码生成参数(内置,不暴露为配置):
    //   长度 12,去除易混字符 I/L/O/0/1,组合空间 31^12 ≈ 7.9e17,足够防碰撞
    static constexpr char const* kCharset    = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    static constexpr size_t      kCharsetLen = 31;
    static constexpr uint32      kCodeLength = 12;

    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::string code;
    int safety = 0;
    do
    {
        code.clear();
        std::uniform_int_distribution<size_t> dis(0, kCharsetLen - 1);
        for (uint32 i = 0; i < kCodeLength; ++i)
            code += kCharset[dis(gen)];

        QueryResult exists = WorldDatabase.Query(
            "SELECT 1 FROM `_奖励_兑换码` WHERE `兑换码`='{}'", code);
        if (!exists)
            return code;
    }
    while (++safety < 32);

    return code;
}

bool PromotionRewardMgr::IssueCodes(uint32 targetGuid, std::string const& targetName,
                                    uint32 count, std::vector<std::string>& outCodes, std::string& errMsg)
{
    outCodes.clear();
    PromotionConfig const& config = GetConfig();

    if (!_enabled || !config.enabled) { errMsg = "宣传奖励系统已禁用"; return false; }
    if (targetGuid == 0)          { errMsg = "无效的玩家GUID";    return false; }
    if (count == 0 || count > 100){ errMsg = "数量必须在 1-100";  return false; }
    if (config.rewardId == 0)
    {
        errMsg = "对接奖励ID 未配置,请先在 world.`_宣传奖励系统` 设置 对接奖励ID(关联 _模板_奖励)";
        return false;
    }

    PromotionPlayerData* d = GetPlayerDataForGroup(targetGuid, config.groupId, true);
    uint32 currentDays = d ? d->days : 0;

    outCodes.reserve(count);
    for (uint32 i = 1; i <= count; ++i)
    {
        std::string code = GenerateUniqueCode();
        if (code.empty())
        {
            errMsg = "生成兑换码失败";
            return false;
        }

        std::string remark = "宣传奖励-" + targetName + "-通用CDK(" + std::to_string(i) + "/" + std::to_string(count) + ")";

        WorldDatabase.Execute(
            "INSERT INTO `_奖励_兑换码` "
            "(`注释`,`兑换码`,`组`,`需求`,`奖励`,`兑换次数`,`领取公告`) "
            "VALUES ('{}','{}',{},{},{},1,{})",
            remark, code, config.groupId, config.requireId, config.rewardId, config.announceType);

        outCodes.push_back(std::move(code));

        if (_debugLog)
            LOG_INFO("module.promotion", "[宣传奖励] 通用CDK {} 已生成,兑换时按角色宣传天数升级", outCodes.back());
    }

    if (_debugLog)
        LOG_INFO("module.promotion", "[宣传奖励] 为 {} 生成 {} 张通用CDK,当前宣传天数仍为 {}", targetName, count, currentDays);

    return true;
}

int256 PromotionRewardMgr::CalcTotalAttr(uint32 days) const
{
    return CalcTotalAttr(days, _defaultGroupId);
}

int256 PromotionRewardMgr::CalcTotalAttr(uint32 days, uint32 groupId) const
{
    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return 0;

    uint32 level = GetWeaponLevelForDays(days, groupId);
    if (level == 0)
        return 0;
    if (config->levelAttrValues.size() >= level)
        return config->levelAttrValues[level - 1];
    return config->baseAttrValue + (static_cast<int256>(level - 1) * config->perDayAttrValue);
}

uint32 PromotionRewardMgr::GetWeaponLevelForDays(uint32 days) const
{
    return GetWeaponLevelForDays(days, _defaultGroupId);
}

uint32 PromotionRewardMgr::GetWeaponLevelForDays(uint32 days, uint32 groupId) const
{
    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return 0;
    if (days == 0)
        return 0;
    if (days > config->maxLevel)
        return config->maxLevel;
    return days;
}

uint32 PromotionRewardMgr::GetWeaponEntryForLevel(uint32 level) const
{
    return GetWeaponEntryForLevel(level, _defaultGroupId);
}

uint32 PromotionRewardMgr::GetWeaponEntryForLevel(uint32 level, uint32 groupId) const
{
    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return 0;
    if (level == 0)
        return 0;
    if (level > config->maxLevel)
        level = config->maxLevel;
    return config->weaponEntry + level - 1;
}

uint32 PromotionRewardMgr::GetNextWeaponEntry(uint32 days) const
{
    return GetNextWeaponEntry(days, _defaultGroupId);
}

uint32 PromotionRewardMgr::GetNextWeaponEntry(uint32 days, uint32 groupId) const
{
    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return 0;
    uint32 nextLevel = days >= config->maxLevel ? config->maxLevel : days + 1;
    return GetWeaponEntryForLevel(nextLevel, groupId);
}

bool PromotionRewardMgr::IsPromotionWeaponEntry(uint32 entry) const
{
    for (auto const& [groupId, config] : _configs)
        if (IsPromotionWeaponEntry(entry, groupId))
            return true;
    return false;
}

bool PromotionRewardMgr::IsPromotionWeaponEntry(uint32 entry, uint32 groupId) const
{
    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return false;
    uint32 maxEntry = config->weaponEntry + config->maxLevel - 1;
    return entry >= config->weaponEntry && entry <= maxEntry;
}

bool PromotionRewardMgr::IsPromotionCodeGroup(uint32 groupId) const
{
    return groupId != 0 && GetConfigForGroup(groupId) != nullptr;
}

bool PromotionRewardMgr::IsWeaponHeld(Player* player) const
{
    return IsWeaponHeld(player, _defaultGroupId);
}

bool PromotionRewardMgr::IsWeaponHeld(Player* player, uint32 groupId) const
{
    if (!player)
        return false;

    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return false;

    uint32 maxEntry = config->weaponEntry + config->maxLevel - 1;
    for (uint32 entry = config->weaponEntry; entry <= maxEntry; ++entry)
    {
        if (player->HasItemCount(entry, 1, true))
            return true;
    }

    return false;
}

bool PromotionRewardMgr::HasPromotionWeaponInBags(Player* player, uint32 groupId) const
{
    if (!player)
        return false;

    PromotionConfig const* config = GetConfigForGroup(groupId);
    if (!config)
        return false;

    uint32 equippedCount = 0;
    uint8 slots[] = { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED };
    for (uint8 slot : slots)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && IsPromotionWeaponEntry(item->GetEntry(), groupId))
            equippedCount += item->GetCount();
    }

    uint32 totalCount = 0;
    uint32 maxEntry = config->weaponEntry + config->maxLevel - 1;
    for (uint32 entry = config->weaponEntry; entry <= maxEntry; ++entry)
        totalCount += player->GetItemCount(entry, false);

    return totalCount > equippedCount;
}

bool PromotionRewardMgr::IsPromotionWeaponEquipped(Player* player) const
{
    return IsPromotionWeaponEquipped(player, _defaultGroupId);
}

bool PromotionRewardMgr::IsPromotionWeaponEquipped(Player* player, uint32 groupId) const
{
    if (!player)
        return false;

    uint8 slots[] = { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED };
    for (uint8 slot : slots)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        if (IsPromotionWeaponEntry(item->GetEntry(), groupId))
            return true;
    }

    return false;
}

bool PromotionRewardMgr::RedeemPromotionCode(Player* player, uint32 groupId, uint32& outRewardId)
{
    PromotionConfig const* config = GetConfigForGroup(groupId);
    outRewardId = config ? config->rewardId : 0;

    if (!_enabled || !config || !player)
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    PromotionPlayerData* d = GetPlayerDataForGroup(guid, groupId, true);
    if (!d)
        return false;

    if (d->days >= config->maxLevel)
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff5555[宣传奖励]|r {}已达到最高等级{},本次CDK未消耗。", config->weaponName, config->maxLevel);
        return false;
    }

    uint32 newLevel = d->days + 1;
    uint32 newEntry = GetWeaponEntryForLevel(newLevel, groupId);
    outRewardId = config->rewardId + newLevel - 1;

    if (!sObjectMgr->GetItemTemplate(newEntry))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r {}物品模板不存在: {}", config->weaponName, newEntry);
        return false;
    }

    bool hasPromotionWeaponInBags = HasPromotionWeaponInBags(player, groupId);
    if (!hasPromotionWeaponInBags)
    {
        ItemPosCountVec dest;
        uint32 noSpaceForCount = 0;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, newEntry, 1, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r 背包空间不足,请至少空出 1 格后再兑换。");
            return false;
        }
    }

    uint32 removedCount = 0;
    uint32 maxEntry = config->weaponEntry + config->maxLevel - 1;
    for (uint32 entry = config->weaponEntry; entry <= maxEntry; ++entry)
    {
        uint32 itemCount = player->GetItemCount(entry, true);
        if (itemCount == 0)
            continue;

        removedCount += itemCount;
        player->DestroyItemCount(entry, itemCount, true, true);
    }

    ItemPosCountVec dest;
    uint32 noSpaceForCount = 0;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, newEntry, 1, &noSpaceForCount);
    if (msg != EQUIP_ERR_OK)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r 背包空间不足,请至少空出 1 格后再兑换。");
        return false;
    }

    Item* newItem = player->StoreNewItem(dest, newEntry, true, Item::GenerateItemRandomPropertyId(newEntry));
    if (!newItem)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r 发放{}失败,请联系管理员。", config->weaponName);
        return false;
    }

    player->SendNewItem(newItem, 1, true, false);

    d->days = newLevel;
    SavePlayerDataForGroup(guid, groupId);

    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cff00ff00[宣传奖励]|r 宣传次数 +1,已{}{}{},获得 {}{}。",
        removedCount > 0 ? "回收旧武器并升级为" : "发放",
        config->weaponName,
        newLevel,
        config->weaponName,
        newLevel);

    SendInfoToClient(player);

    if (_debugLog)
        LOG_INFO("module.promotion", "[宣传奖励] 玩家 {} 使用组 {} 宣传CDK升级到 {} 级,物品={}",
            player->GetName(), groupId, newLevel, newEntry);

    return true;
}

// ============================================================
// WorldScript - 启动加载
// ============================================================

PromotionReward_WorldScript::PromotionReward_WorldScript()
    : WorldScript("PromotionReward_WorldScript") { }

void PromotionReward_WorldScript::OnAfterConfigLoad(bool /*reload*/)
{
    sPromotionRewardMgr->LoadConfig();
}

void PromotionReward_WorldScript::OnStartup()
{
    _auditTimerMs = 0;
    sPromotionRewardMgr->ClearAllItemCaptures();
    sPromotionRewardMgr->LoadConfig();
    sPromotionRewardMgr->LoadAllPlayers();
    LOG_INFO("server.loading", "→宣传奖励系统√");
}

void PromotionReward_WorldScript::OnUpdate(uint32 diff)
{
    if (!sPromotionRewardMgr->IsEnabled())
        return;

    _auditTimerMs += diff;
    if (_auditTimerMs < 1000)
        return;

    _auditTimerMs = 0;

    // 发奖、审核副作用、精确回收和封禁各有独立有界队列，避免长期回收欠账挤占审核窗口。
    sPromotionRewardAuditMgr->ConsumeGrantQueue(5);
    sPromotionRewardAuditMgr->ConsumeReviewQueue(5);
    sPromotionRewardAuditMgr->ConsumeRollbackQueue(5);
    sPromotionRewardAuditMgr->ConsumeBanQueue(5);
}

void PromotionReward_WorldScript::OnShutdown()
{
    sPromotionRewardMgr->ClearAllItemCaptures();
}

// ============================================================
// PlayerScript - 只刷新 UI 状态,属性交给物品/背包加成模块
// ============================================================

PromotionReward_PlayerScript::PromotionReward_PlayerScript()
    : PlayerScript("PromotionReward_PlayerScript") { }

void PromotionReward_PlayerScript::OnPlayerLogin(Player* player)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player)
        return;
    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerLogout(Player* player)
{
    sPromotionRewardMgr->CancelItemCapture(player);
}

void PromotionReward_PlayerScript::OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/)
{
    sPromotionRewardMgr->RecordCapturedItem(player, item);

    if (!sPromotionRewardMgr->IsEnabled() || !player || !item)
        return;
    if (!sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
        return;
    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player || !item)
        return;
    if (!sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
        return;
    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player || !item)
        return;
    if (!sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
        return;

    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/)
{
    if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        return;

    size_t tab = msg.find('\t');
    if (tab == std::string::npos)
        return;

    std::string prefix = msg.substr(0, tab);
    if (prefix != PROMO_ADDON_PREFIX)
        return;

    // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
    if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "PROMO"))
        return;

    std::string cmd = msg.substr(tab + 1);

    if (cmd == "REQ_INFO")
    {
        sPromotionRewardMgr->SendInfoToClient(player);
        return;
    }
    if (cmd.compare(0, 7, "REDEEM:") == 0)
    {
        std::string code = cmd.substr(7);
        std::string err;
        if (sPromotionRewardMgr->ClientRedeem(player, code, err))
        {
            sPromotionRewardMgr->SendAddonMsg(player, "REDEEM_OK:" + code);
            sPromotionRewardMgr->SendInfoToClient(player);
        }
        else
        {
            sPromotionRewardMgr->SendAddonMsg(player, "REDEEM_FAIL:" + code + ":" + err);
        }
        return;
    }
}

// ============================================================
// 客户端 UI 通信 (Addon Message)
//   prefix: PROMOREWARD
//   Client → Server:  REQ_INFO | REDEEM:<CDK>
//   Server → Client:  INFO:days|attr|weapon|base|perDay|claimed|nextWeapon|level|nextLevel|nextAttr|minDmg|maxDmg|nextMinDmg|nextMaxDmg|maxLevel|firstWeapon|weaponName|maxAttr
//                     OPEN
//                     REDEEM_OK:CDK | REDEEM_FAIL:CDK:reason
// ============================================================

void PromotionRewardMgr::SendAddonMsg(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession() || payload.empty())
        return;

    if (HermesBridge_SendAddonMessage(player, PROMO_ADDON_PREFIX, payload))
        return;

    std::string full = std::string(PROMO_ADDON_PREFIX) + '\t' + payload;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, full, 0);
    player->SendDirectMessage(&data);
}

void PromotionRewardMgr::SendInfoToClient(Player* player)
{
    if (!player)
        return;

    PromotionConfig const& config = GetConfig();
    if (!config.enabled)
        return;

    uint32 days = 0;
    if (PromotionPlayerData* d = GetPlayerDataForGroup(player->GetGUID().GetCounter(), config.groupId))
        days = d->days;

    int256 attr  = CalcTotalAttr(days, config.groupId);
    bool  hold   = IsWeaponHeld(player, config.groupId);
    uint32 currentLevel = GetWeaponLevelForDays(days, config.groupId);
    uint32 nextLevel = days >= config.maxLevel ? config.maxLevel : days + 1;
    uint32 currentWeapon = GetWeaponEntryForLevel(currentLevel, config.groupId);
    uint32 nextWeapon = GetNextWeaponEntry(days, config.groupId);
    int256 currentMinDmg = currentLevel == 0 ? 0 : CalcTotalAttr(currentLevel, config.groupId);
    int256 currentMaxDmg = currentLevel == 0 ? 0 :
        (!config.levelAttrValues.empty()
            ? CalcTotalAttr(std::min(currentLevel + 1, config.maxLevel), config.groupId)
            : currentMinDmg + config.perDayAttrValue);
    int256 nextAttr = CalcTotalAttr(nextLevel, config.groupId);
    int256 nextMinDmg = nextAttr;
    int256 nextMaxDmg = !config.levelAttrValues.empty()
        ? CalcTotalAttr(std::min(nextLevel + 1, config.maxLevel), config.groupId)
        : nextAttr + config.perDayAttrValue;
    int256 maxAttr = CalcTotalAttr(config.maxLevel, config.groupId);

    std::ostringstream o;
    o << "INFO:" << days
      << '|' << Acore::ToString(attr)
      << '|' << currentWeapon
      << '|' << Acore::ToString(config.baseAttrValue)
      << '|' << Acore::ToString(config.perDayAttrValue)
      << '|' << (hold ? 1 : 0)
      << '|' << nextWeapon
      << '|' << currentLevel
      << '|' << nextLevel
      << '|' << Acore::ToString(nextAttr)
      << '|' << Acore::ToString(currentMinDmg)
      << '|' << Acore::ToString(currentMaxDmg)
      << '|' << Acore::ToString(nextMinDmg)
      << '|' << Acore::ToString(nextMaxDmg)
      << '|' << config.maxLevel
      << '|' << config.weaponEntry
      << '|' << config.weaponName
      << '|' << Acore::ToString(maxAttr);
    SendAddonMsg(player, o.str());
}

void PromotionRewardMgr::SendOpenUIToClient(Player* player)
{
    SendAddonMsg(player, "OPEN");
}

bool PromotionRewardMgr::ClientRedeem(Player* player, std::string const& code, std::string& errMsg)
{
    if (!player || code.empty())
    {
        errMsg = "无效请求";
        return false;
    }

    if (code.size() > 64 || code.find_first_of(" \t\n\r;'\"\\`") != std::string::npos)
    {
        errMsg = "兑换码格式不合法";
        return false;
    }

    std::string cmd = ".兑换码 兑换 " + code;
    bool ok = ChatHandler(player->GetSession()).ParseCommands(cmd);
    if (!ok)
    {
        errMsg = "兑换失败,请检查兑换码或联系管理员";
        return false;
    }
    return true;
}

// ============================================================
// 入口
// ============================================================

void AddSC_PromotionReward_WorldScript()
{
    new PromotionReward_WorldScript();
    new PromotionReward_PlayerScript();
}

void AddPromotionRewardScripts()
{
    AddSC_PromotionReward_WorldScript();
    AddSC_PromotionReward_CommandScript();
}

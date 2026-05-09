/*
 * 宣传奖励系统 (mod-promotion-reward) - 主实现
 */

#include "PromotionRewardModule.h"
#include "loader.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSessionMgr.h"
#include <random>
#include <sstream>

extern void AddSC_PromotionReward_CommandScript();

constexpr char const* PROMO_ADDON_PREFIX = "PROMOREWARD";
constexpr uint32 PROMO_WEAPON_ENTRY_BASE = 997000;
constexpr uint32 PROMO_WEAPON_MIN_ENTRY  = 997001;
constexpr uint32 PROMO_WEAPON_MAX_ENTRY  = 998000;
constexpr uint32 PROMO_WEAPON_MAX_LEVEL  = 1000;

PromotionRewardMgr* PromotionRewardMgr::instance()
{
    static PromotionRewardMgr s;
    return &s;
}

void PromotionRewardMgr::LoadConfig()
{
    _cfg.debugLog = sConfigMgr->GetOption<bool>("PromotionReward.DebugLog", false);

    if (!sConfigMgr->GetOption<bool>("PromotionReward.Enable", true))
    {
        _cfg.enabled = false;
        LOG_INFO("server.loading", "宣传奖励系统已在配置文件中禁用");
        return;
    }

    QueryResult r = WorldDatabase.Query(
        "SELECT `武器entry`,`初始全属性值`,`每日增量`,"
        "`对接奖励组`,`对接需求ID`,`对接奖励ID`,`领取公告` "
        "FROM `_宣传奖励系统` WHERE `id`=1");

    if (!r)
    {
        _cfg.enabled = false;
        LOG_WARN("server.loading",
            "[宣传奖励] world.`_宣传奖励系统` 表中没有 id=1 的配置行,模块禁用。请导入 SQL 或插入一行配置以启用");
        return;
    }

    Field* f = r->Fetch();
    _cfg.weaponEntry     = f[0].Get<uint32>();
    _cfg.baseAttrValue   = f[1].Get<int32>();
    _cfg.perDayAttrValue = f[2].Get<int32>();
    _cfg.groupId         = f[3].Get<uint32>();
    _cfg.requireId       = f[4].Get<uint32>();
    _cfg.rewardId        = f[5].Get<uint32>();
    _cfg.announceType    = f[6].Get<uint32>();

    _cfg.enabled = true;

    LOG_INFO("server.loading",
        "[宣传奖励] 武器entry={} 初始={} 每日+={} 对接[组={},需求={},奖励={}]",
        _cfg.weaponEntry, _cfg.baseAttrValue, _cfg.perDayAttrValue,
        _cfg.groupId, _cfg.requireId, _cfg.rewardId);
}

void PromotionRewardMgr::LoadAllPlayers()
{
    _players.clear();

    if (QueryResult r = CharacterDatabase.Query(
        "SELECT `玩家GUID`,`宣传天数` FROM `_宣传奖励系统玩家`"))
    {
        do
        {
            Field* f = r->Fetch();
            uint32 guid = f[0].Get<uint32>();
            _players[guid].days = f[1].Get<uint32>();
        } while (r->NextRow());
    }

    LOG_INFO("server.loading", "[宣传奖励] 已加载 {} 个玩家的宣传记录", _players.size());
}

PromotionPlayerData* PromotionRewardMgr::GetPlayerData(uint32 guid, bool createIfMissing)
{
    auto it = _players.find(guid);
    if (it != _players.end())
        return &it->second;

    if (!createIfMissing)
        return nullptr;

    return &_players[guid];
}

void PromotionRewardMgr::SavePlayerData(uint32 guid)
{
    auto it = _players.find(guid);
    if (it == _players.end())
        return;

    CharacterDatabase.Execute(
        "INSERT INTO `_宣传奖励系统玩家` (`玩家GUID`,`宣传天数`) "
        "VALUES ({},{}) "
        "ON DUPLICATE KEY UPDATE `宣传天数`=VALUES(`宣传天数`)",
        guid, it->second.days);
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

    if (!_cfg.enabled)            { errMsg = "宣传奖励系统已禁用"; return false; }
    if (targetGuid == 0)          { errMsg = "无效的玩家GUID";    return false; }
    if (count == 0 || count > 100){ errMsg = "数量必须在 1-100";  return false; }
    if (_cfg.rewardId == 0)
    {
        errMsg = "对接奖励ID 未配置,请先在 world.`_宣传奖励系统` 设置 对接奖励ID(关联 _模板_奖励)";
        return false;
    }

    PromotionPlayerData* d = GetPlayerData(targetGuid, true);
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
            remark, code, _cfg.groupId, _cfg.requireId, _cfg.rewardId, _cfg.announceType);

        outCodes.push_back(std::move(code));

        if (_cfg.debugLog)
            LOG_INFO("module.promotion", "[宣传奖励] 通用CDK {} 已生成,兑换时按角色宣传天数升级", outCodes.back());
    }

    if (_cfg.debugLog)
        LOG_INFO("module.promotion", "[宣传奖励] 为 {} 生成 {} 张通用CDK,当前宣传天数仍为 {}", targetName, count, currentDays);

    return true;
}

int32 PromotionRewardMgr::CalcTotalAttr(uint32 days) const
{
    uint32 level = GetWeaponLevelForDays(days);
    if (level == 0)
        return 0;
    return _cfg.baseAttrValue + int32(level - 1) * _cfg.perDayAttrValue;
}

uint32 PromotionRewardMgr::GetWeaponLevelForDays(uint32 days) const
{
    if (days == 0)
        return 0;
    if (days > PROMO_WEAPON_MAX_LEVEL)
        return PROMO_WEAPON_MAX_LEVEL;
    return days;
}

uint32 PromotionRewardMgr::GetWeaponEntryForLevel(uint32 level) const
{
    if (level == 0)
        return 0;
    if (level > PROMO_WEAPON_MAX_LEVEL)
        level = PROMO_WEAPON_MAX_LEVEL;
    return PROMO_WEAPON_ENTRY_BASE + level;
}

uint32 PromotionRewardMgr::GetNextWeaponEntry(uint32 days) const
{
    uint32 nextLevel = days >= PROMO_WEAPON_MAX_LEVEL ? PROMO_WEAPON_MAX_LEVEL : days + 1;
    return GetWeaponEntryForLevel(nextLevel);
}

bool PromotionRewardMgr::IsPromotionWeaponEntry(uint32 entry) const
{
    if (entry >= PROMO_WEAPON_MIN_ENTRY && entry <= PROMO_WEAPON_MAX_ENTRY)
        return true;
    return _cfg.weaponEntry != 0 && entry == _cfg.weaponEntry;
}

bool PromotionRewardMgr::IsPromotionCodeGroup(uint32 groupId) const
{
    return groupId != 0 && groupId == _cfg.groupId;
}

bool PromotionRewardMgr::IsWeaponHeld(Player* player) const
{
    if (!player)
        return false;

    if (_cfg.weaponEntry != 0 && player->HasItemCount(_cfg.weaponEntry, 1, false))
        return true;

    for (uint32 entry = PROMO_WEAPON_MIN_ENTRY; entry <= PROMO_WEAPON_MAX_ENTRY; ++entry)
    {
        if (player->HasItemCount(entry, 1, false))
            return true;
    }

    return false;
}

bool PromotionRewardMgr::HasPromotionWeaponInBags(Player* player) const
{
    if (!player)
        return false;

    uint32 equippedCount = 0;
    uint8 slots[] = { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED };
    for (uint8 slot : slots)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && IsPromotionWeaponEntry(item->GetEntry()))
            equippedCount += item->GetCount();
    }

    uint32 totalCount = 0;
    for (uint32 entry = PROMO_WEAPON_MIN_ENTRY; entry <= PROMO_WEAPON_MAX_ENTRY; ++entry)
        totalCount += player->GetItemCount(entry, false);

    return totalCount > equippedCount;
}

bool PromotionRewardMgr::IsPromotionWeaponEquipped(Player* player) const
{
    if (!player)
        return false;

    uint8 slots[] = { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED };
    for (uint8 slot : slots)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint32 entry = item->GetEntry();
        if (entry >= PROMO_WEAPON_MIN_ENTRY && entry <= PROMO_WEAPON_MAX_ENTRY)
            return true;
    }

    return false;
}

bool PromotionRewardMgr::RedeemPromotionCode(Player* player, uint32& outRewardId)
{
    outRewardId = _cfg.rewardId;

    if (!_cfg.enabled || !player)
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    PromotionPlayerData* d = GetPlayerData(guid, true);
    if (!d)
        return false;

    if (d->days >= PROMO_WEAPON_MAX_LEVEL)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r 宣传神器已达到最高等级,本次CDK未消耗。");
        return false;
    }

    uint32 newLevel = d->days + 1;
    uint32 newEntry = GetWeaponEntryForLevel(newLevel);
    outRewardId = _cfg.rewardId + newLevel - 1;

    if (!sObjectMgr->GetItemTemplate(newEntry))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r 宣传神器物品模板不存在: {}", newEntry);
        return false;
    }

    bool hasPromotionWeaponInBags = HasPromotionWeaponInBags(player);
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

    RemoveHeldBuff(player);

    uint32 removedCount = 0;
    for (uint32 entry = PROMO_WEAPON_MIN_ENTRY; entry <= PROMO_WEAPON_MAX_ENTRY; ++entry)
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
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff5555[宣传奖励]|r 发放宣传神器失败,请联系管理员。");
        return false;
    }

    player->SendNewItem(newItem, 1, true, false);

    d->days = newLevel;
    SavePlayerData(guid);

    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cff00ff00[宣传奖励]|r 宣传天数 +1,已{}宣传神器{},获得 宣传神器{}。",
        removedCount > 0 ? "回收旧武器并升级为" : "发放",
        newLevel,
        newLevel);

    RefreshHeldBuff(player);
    SendInfoToClient(player);

    if (_cfg.debugLog)
        LOG_INFO("module.promotion", "[宣传奖励] 玩家 {} 使用宣传CDK升级到 {} 级,物品={}", player->GetName(), newLevel, newEntry);

    return true;
}

void PromotionRewardMgr::ApplyAllStats(Player* player, int32 value, bool apply)
{
    if (!player || value == 0)
        return;

    float fv = float(value);

    player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, fv, apply);
    player->ApplyStatBuffMod(STAT_STRENGTH, fv, apply);
    player->UpdateStats(STAT_STRENGTH);

    player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, fv, apply);
    player->ApplyStatBuffMod(STAT_AGILITY, fv, apply);
    player->UpdateStats(STAT_AGILITY);

    player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, fv, apply);
    player->ApplyStatBuffMod(STAT_INTELLECT, fv, apply);
    player->UpdateStats(STAT_INTELLECT);
    player->UpdateMaxPower(POWER_MANA);

    player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, fv, apply);
    player->ApplyStatBuffMod(STAT_SPIRIT, fv, apply);
    player->UpdateStats(STAT_SPIRIT);

    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER,        TOTAL_VALUE, fv, apply);
    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, fv, apply);
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);

    player->ApplyRatingMod(CR_HIT_MELEE,    value, apply);
    player->ApplyRatingMod(CR_HIT_RANGED,   value, apply);
    player->ApplyRatingMod(CR_HIT_SPELL,    value, apply);
    player->ApplyRatingMod(CR_CRIT_MELEE,   value, apply);
    player->ApplyRatingMod(CR_CRIT_RANGED,  value, apply);
    player->ApplyRatingMod(CR_CRIT_SPELL,   value, apply);
    player->ApplyRatingMod(CR_HASTE_MELEE,  value, apply);
    player->ApplyRatingMod(CR_HASTE_RANGED, value, apply);
    player->ApplyRatingMod(CR_HASTE_SPELL,  value, apply);

    player->ApplySpellPowerBonus(value, apply);
    player->ApplySpellPenetrationBonus(value, apply);

    player->UpdateAllStats();
}

void PromotionRewardMgr::ApplyHeldBuff(Player* player)
{
    if (!_cfg.enabled || !player)
        return;

    if (!IsWeaponHeld(player) || IsPromotionWeaponEquipped(player))
    {
        RemoveHeldBuff(player);
        return;
    }

    uint32 guid = player->GetGUID().GetCounter();
    PromotionPlayerData* d = GetPlayerData(guid);
    if (!d || d->days == 0)
    {
        RemoveHeldBuff(player);
        return;
    }

    int32 attr = CalcTotalAttr(d->days);
    if (attr <= 0)
    {
        RemoveHeldBuff(player);
        return;
    }

    auto it = _appliedAttr.find(guid);
    if (it != _appliedAttr.end())
    {
        if (it->second == attr)
            return;
        ApplyAllStats(player, it->second, false);
        _appliedAttr.erase(it);
    }

    ApplyAllStats(player, attr, true);
    _appliedAttr[guid] = attr;

    if (_cfg.debugLog)
        LOG_INFO("module.promotion", "[宣传奖励] 玩家 {} 应用全属性 +{}", player->GetName(), attr);
}

void PromotionRewardMgr::RemoveHeldBuff(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    auto it = _appliedAttr.find(guid);
    if (it == _appliedAttr.end())
        return;

    int32 prev = it->second;
    _appliedAttr.erase(it);

    if (prev > 0)
        ApplyAllStats(player, prev, false);

    if (_cfg.debugLog)
        LOG_INFO("module.promotion", "[宣传奖励] 玩家 {} 移除全属性 -{}", player->GetName(), prev);
}

void PromotionRewardMgr::RefreshHeldBuff(Player* player)
{
    if (!player)
        return;

    if (IsWeaponHeld(player) && !IsPromotionWeaponEquipped(player))
        ApplyHeldBuff(player);
    else
        RemoveHeldBuff(player);
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
    sPromotionRewardMgr->LoadConfig();
    sPromotionRewardMgr->LoadAllPlayers();
    LOG_INFO("server.loading", "→ 宣传奖励系统加载成功");
}

// ============================================================
// PlayerScript - 持有武器即生效
// ============================================================

PromotionReward_PlayerScript::PromotionReward_PlayerScript()
    : PlayerScript("PromotionReward_PlayerScript") { }

void PromotionReward_PlayerScript::OnPlayerLogin(Player* player)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player)
        return;
    sPromotionRewardMgr->RefreshHeldBuff(player);
    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerLogout(Player* player)
{
    if (!player)
        return;
    sPromotionRewardMgr->RemoveHeldBuff(player);
}

void PromotionReward_PlayerScript::OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player || !item)
        return;
    if (!sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
        return;
    sPromotionRewardMgr->RefreshHeldBuff(player);
    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player || !item)
        return;
    if (!sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
        return;
    sPromotionRewardMgr->RefreshHeldBuff(player);
    sPromotionRewardMgr->SendInfoToClient(player);
}

void PromotionReward_PlayerScript::OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/)
{
    if (!sPromotionRewardMgr->IsEnabled() || !player || !item)
        return;
    if (!sPromotionRewardMgr->IsPromotionWeaponEntry(item->GetEntry()))
        return;

    sPromotionRewardMgr->RefreshHeldBuff(player);
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

    std::string cmd = msg.substr(tab + 1);

    if (cmd == "REQ_INFO")
    {
        sPromotionRewardMgr->SendInfoToClient(player);
        return;
    }
    if (cmd == "REQ_CODES")
    {
        sPromotionRewardMgr->SendCodesToClient(player);
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
//   Client → Server:  REQ_INFO | REQ_CODES | REDEEM:<CDK>
//   Server → Client:  INFO:days|attr|weapon|base|perDay|claimed|nextWeapon|level|nextLevel|nextAttr|minDmg|maxDmg|nextMinDmg|nextMaxDmg
//                     CODES:CDK1,CDK2,...   (可能多包)
//                     CODES_END
//                     OPEN
//                     REDEEM_OK:CDK | REDEEM_FAIL:CDK:reason
// ============================================================

void PromotionRewardMgr::SendAddonMsg(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession() || payload.empty())
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

    uint32 days = 0;
    if (PromotionPlayerData* d = GetPlayerData(player->GetGUID().GetCounter()))
        days = d->days;

    int32 attr   = CalcTotalAttr(days);
    bool  hold   = IsWeaponHeld(player);
    uint32 currentLevel = GetWeaponLevelForDays(days);
    uint32 nextLevel = days >= PROMO_WEAPON_MAX_LEVEL ? PROMO_WEAPON_MAX_LEVEL : days + 1;
    uint32 currentWeapon = GetWeaponEntryForLevel(currentLevel);
    uint32 nextWeapon = GetNextWeaponEntry(days);
    int32 currentMinDmg = currentLevel == 0 ? 0 : CalcTotalAttr(currentLevel);
    int32 currentMaxDmg = currentLevel == 0 ? 0 : currentMinDmg + _cfg.perDayAttrValue;
    int32 nextAttr = CalcTotalAttr(nextLevel);
    int32 nextMinDmg = nextAttr;
    int32 nextMaxDmg = nextAttr + _cfg.perDayAttrValue;

    std::ostringstream o;
    o << "INFO:" << days
      << '|' << attr
      << '|' << currentWeapon
      << '|' << _cfg.baseAttrValue
      << '|' << _cfg.perDayAttrValue
      << '|' << (hold ? 1 : 0)
      << '|' << nextWeapon
      << '|' << currentLevel
      << '|' << nextLevel
      << '|' << nextAttr
      << '|' << currentMinDmg
      << '|' << currentMaxDmg
      << '|' << nextMinDmg
      << '|' << nextMaxDmg;
    SendAddonMsg(player, o.str());
}

void PromotionRewardMgr::SendCodesToClient(Player* player)
{
    if (!player)
        return;

    uint32 myGuid = player->GetGUID().GetCounter();
    QueryResult r = WorldDatabase.Query(
        "SELECT `兑换码` FROM `_奖励_兑换码` "
        "WHERE `组`={} AND `兑换次数`>0 AND `注释` LIKE '宣传奖励-%' "
        "AND (`兑换角色` IS NULL OR `兑换角色` NOT LIKE '%,{},%') "
        "LIMIT 50",
        _cfg.groupId, myGuid);

    if (!r)
    {
        SendAddonMsg(player, "CODES:");
        return;
    }

    constexpr size_t MaxPayload = 190;
    std::string payload = "CODES:";
    bool first = true;
    do
    {
        Field* f = r->Fetch();
        std::string code = f[0].Get<std::string>();

        size_t addLen = code.size() + (first ? 0 : 1);
        if (payload.size() + addLen > MaxPayload && payload != "CODES:")
        {
            SendAddonMsg(player, payload);
            payload = "CODES:";
            first = true;
            addLen = code.size();
        }

        if (!first)
            payload += ',';
        payload += code;
        first = false;
    } while (r->NextRow());

    SendAddonMsg(player, payload);
    SendAddonMsg(player, "CODES_END");
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

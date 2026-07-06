/*
 * 仙门系统 - 仙器/扩展装备槽位
 *
 * 在官方 19 个装备槽之外提供 29 个独立虚拟槽位：
 *   槽位 1-10  仙器槽（只能放 _仙门_仙器物品 注册表中登记的仙器，槽位一一对应）
 *   槽位 11-29 扩展槽（与官方 19 装备槽一一镜像：头/颈/肩/衬衣/胸/腰/腿/脚/腕/手/
 *                      戒指1/戒指2/饰品1/饰品2/披风/主手/副手/远程/战袍）
 *
 * 机制参考 mod-ascension-system：物品实体借核心 character_inventory 表以
 * 虚拟背包 bag=202 存放（核心 PlayerStorage::_LoadInventory 跳过该 bag），
 * 运行期 Item* 由本模块持有（OwnerGUID 置空 + ITEM_UNCHANGED，核心存档忽略）。
 * 属性应用为 int256 逐项 stat-mod + 按槽位记账精确回滚。
 */

#include "AllItemScript.h"
#include "Bag.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "HermesBridgeAddonApi.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Mail.h"
#include "ModuleManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RequirementInterface.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Util.h"
#if __has_include("WearControl.h")
    #ifndef MODULE_WEAR_CONTROL
        #define MODULE_WEAR_CONTROL
    #endif
    #include "WearControl.h"
#endif
#include "WorldSession.h"
#include "XianmenArtifactSlots.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{

constexpr uint32 XIANQI_VIRTUAL_BAG = 202;          // 200=飞升 201=武魂 202=仙器，须与核心 PlayerStorage.cpp 白名单一致
constexpr uint8 XIANQI_SLOT_FIRST = 1;
constexpr uint8 XIANQI_SLOT_LAST = 29;
constexpr uint8 XIANQI_SLOT_TYPE_EXTENSION = 1;
constexpr uint8 XIANQI_SLOT_TYPE_ARTIFACT = 2;
constexpr char XIANQI_ADDON_PREFIX[] = "XIANQI";
constexpr char XIANQI_PATTR_PANEL_PREFIX[] = "PATTRPANEL";

// 扩展槽部位编号（_仙门_仙器槽位.部位）= 官方装备槽 EquipmentSlots + 1，扩展槽ID = 部位 + 10
enum XianqiBodyPart : uint8
{
    XIANQI_PART_NONE     = 0,    // 仙器槽
    XIANQI_PART_HEAD     = 1,    // 头部
    XIANQI_PART_NECK     = 2,    // 颈部
    XIANQI_PART_SHOULDER = 3,    // 肩部
    XIANQI_PART_BODY     = 4,    // 衬衣
    XIANQI_PART_CHEST    = 5,    // 胸部
    XIANQI_PART_WAIST    = 6,    // 腰带
    XIANQI_PART_LEGS     = 7,    // 腿部
    XIANQI_PART_FEET     = 8,    // 脚部
    XIANQI_PART_WRIST    = 9,    // 护腕
    XIANQI_PART_HANDS    = 10,   // 手套
    XIANQI_PART_FINGER1  = 11,   // 戒指1
    XIANQI_PART_FINGER2  = 12,   // 戒指2
    XIANQI_PART_TRINKET1 = 13,   // 饰品1
    XIANQI_PART_TRINKET2 = 14,   // 饰品2
    XIANQI_PART_BACK     = 15,   // 披风
    XIANQI_PART_MAINHAND = 16,   // 主手
    XIANQI_PART_OFFHAND  = 17,   // 副手
    XIANQI_PART_RANGED   = 18,   // 远程
    XIANQI_PART_TABARD   = 19    // 战袍
};

// 主手/副手扩展槽（26/27）不限制武器类型：CanEquipItemInSlot 中主/副手部位互通

struct XianqiSlotConfig
{
    uint8 slotId = 0;
    std::string name;
    uint8 slotType = XIANQI_SLOT_TYPE_EXTENSION;
    uint32 unlockRequirementId = 0;
    uint8 bodyPart = XIANQI_PART_NONE;
};

struct XianqiSlotData
{
    uint32 itemId = 0;
    uint32 itemGuid = 0;
    Item* itemPtr = nullptr;    // 仅在线时有效，由本模块持有
};

struct XianqiAppliedStat
{
    uint32 statType = 0;
    int256 statValue = 0;
};

struct PlayerXianqiStatus
{
    uint32 playerGuid = 0;
    std::map<uint8, XianqiSlotData> slots;
    std::set<uint8> unlockedSlots;
    std::map<uint8, std::vector<XianqiAppliedStat>> slotStats;
    std::map<uint8, std::vector<uint32>> slotSpells;
    std::map<uint8, uint32> slotItemSets;
};

RequirementInterface* GetXianqiRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    return mgr ? mgr->GetRequirementModule() : nullptr;
}

bool CanEquipByWearControl(Player* player, uint32 itemId, uint8 slot, std::string& error)
{
#ifdef MODULE_WEAR_CONTROL
    return WearControl::CanEquipItem(player, itemId, WEAR_LIMIT_XIANQI, slot, &error, true);
#else
    (void)player;
    (void)itemId;
    (void)slot;
    (void)error;
    return true;
#endif
}

void NotifyXianqiAttributePanelRefresh(Player* player)
{
    if (!player || !player->GetSession())
        return;

    if (HermesBridge_SendAddonMessage(player, XIANQI_PATTR_PANEL_PREFIX, "REFRESH"))
        return;

    WorldPacket data;
    std::string fullMessage = std::string(XIANQI_PATTR_PANEL_PREFIX) + "\tREFRESH";
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
}

int32 ClampXianqiInt256ToInt32(int256 const& value)
{
    int64 v = Acore::Number::ToInt64Saturated(value);
    if (v > std::numeric_limits<int32>::max())
        return std::numeric_limits<int32>::max();
    if (v < std::numeric_limits<int32>::min())
        return std::numeric_limits<int32>::min();
    return static_cast<int32>(v);
}

bool IsValidXianqiSlot(uint8 slot)
{
    return slot >= XIANQI_SLOT_FIRST && slot <= XIANQI_SLOT_LAST;
}

// InventoryType → 扩展槽默认部位（戒指/饰品/单手武器的双槽兼容在 CanEquipItemInSlot 中特判）
// 映射语义与飞升系统 GetSlotForItemClass 一致；虚拟槽只给属性，不参与攻击动画
uint8 MapXianqiInventoryTypeToBodyPart(uint32 inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:              return XIANQI_PART_HEAD;
        case INVTYPE_NECK:              return XIANQI_PART_NECK;
        case INVTYPE_SHOULDERS:         return XIANQI_PART_SHOULDER;
        case INVTYPE_BODY:              return XIANQI_PART_BODY;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:              return XIANQI_PART_CHEST;
        case INVTYPE_WAIST:             return XIANQI_PART_WAIST;
        case INVTYPE_LEGS:              return XIANQI_PART_LEGS;
        case INVTYPE_FEET:              return XIANQI_PART_FEET;
        case INVTYPE_WRISTS:            return XIANQI_PART_WRIST;
        case INVTYPE_HANDS:             return XIANQI_PART_HANDS;
        case INVTYPE_FINGER:            return XIANQI_PART_FINGER1;
        case INVTYPE_TRINKET:           return XIANQI_PART_TRINKET1;
        case INVTYPE_CLOAK:             return XIANQI_PART_BACK;
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_2HWEAPON:          return XIANQI_PART_MAINHAND;
        // 单手武器默认主手；主/副手扩展槽互通，无双持/双手限制
        case INVTYPE_WEAPON:            return XIANQI_PART_MAINHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:          return XIANQI_PART_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_RELIC:             return XIANQI_PART_RANGED;
        case INVTYPE_TABARD:            return XIANQI_PART_TABARD;
        default:                        return XIANQI_PART_NONE;
    }
}

void MakeXianqiAuraPermanent(Player* player, uint32 spellId)
{
    if (!player || !spellId)
        return;

    if (Aura* aura = player->GetAura(spellId))
    {
        aura->SetMaxDuration(-1);
        aura->SetDuration(-1);
    }
}

void MakeXianqiEquipSpellPermanent(Player* player, uint32 spellId)
{
    MakeXianqiAuraPermanent(player, spellId);

    if (std::vector<int32> const* linkedSpells = sSpellMgr->GetSpellLinked(spellId + SPELL_LINK_AURA))
        for (int32 linkedSpellId : *linkedSpells)
            if (linkedSpellId > 0)
                MakeXianqiAuraPermanent(player, uint32(linkedSpellId));
}

void ApplyXianqiEquipSpell(Player* player, Item* item, SpellInfo const* spellInfo)
{
    if (!player || !spellInfo)
        return;

    player->ApplyEquipSpell(spellInfo, item, true);
    MakeXianqiEquipSpellPermanent(player, spellInfo->Id);
}

void RemoveXianqiEquipSpell(Player* player, Item* item, uint32 spellId)
{
    if (!player || !spellId)
        return;

    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
        player->ApplyEquipSpell(spellInfo, item, false);
    else if (item)
        player->RemoveAurasDueToItemSpell(spellId, item->GetGUID());
    else
        player->RemoveAurasDueToSpell(spellId);
}

uint8 GetXianqiAttackBodyPart(WeaponAttackType attType)
{
    switch (attType)
    {
        case BASE_ATTACK:
            return XIANQI_PART_MAINHAND;
        case OFF_ATTACK:
            return XIANQI_PART_OFFHAND;
        case RANGED_ATTACK:
            return XIANQI_PART_RANGED;
        default:
            return XIANQI_PART_NONE;
    }
}

bool IsXianqiDamageTriggeredCombatSpellId(uint32 spellId)
{
    return spellId >= 383001 && spellId <= 383084;
}

bool IsXianqiFeatureSpellId(uint32 spellId)
{
    return spellId >= 385001 && spellId <= 385050;
}

bool HasXianqiFeatureSpell(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];
        if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_CHANCE_ON_HIT
            && IsXianqiFeatureSpellId(spellData.SpellId))
            return true;
    }

    return false;
}

bool HasXianqiDamageTriggeredCombatSpell(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];
        if (spellData.SpellTrigger == ITEM_SPELLTRIGGER_CHANCE_ON_HIT
            && IsXianqiDamageTriggeredCombatSpellId(spellData.SpellId))
            return true;
    }

    return false;
}

    bool CastXianqiDamageTriggeredCombatSpell(Player* player, Unit* target, Item* item, SpellInfo const* spellInfo)
    {
        if (!player || !target || !target->IsAlive() || target == player || !spellInfo)
            return false;

        return player->TriggerDamageTriggeredArtifactItemProcSpell(target, item, spellInfo);
    }

bool CastXianqiDamageTriggeredCombatSpells(Player* player, Unit* target, Item* item, ItemTemplate const* proto)
{
    if (!player || !target || !proto)
        return false;

    bool triggered = false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT || !IsXianqiDamageTriggeredCombatSpellId(spellData.SpellId))
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellData.SpellId);
        if (!spellInfo)
            continue;

        if (CastXianqiDamageTriggeredCombatSpell(player, target, item, spellInfo))
            triggered = true;
    }

    return triggered;
}

bool CanXianqiItemProcForAttack(XianqiSlotConfig const* config, ItemTemplate const* proto, WeaponAttackType attType)
{
    if (!config || !proto)
        return false;

    // 385001-385050 是仙器特色技能配置，由 XianqiFeatureSpells 统一按玩家伤害事件结算。
    // 如果也走普通物品 CastItemCombatSpell，会只播放 DBC dummy 法术视觉，不会产生特色伤害和仇恨。
    if (HasXianqiFeatureSpell(proto))
        return false;

    // 383001-383084 按“任意玩家伤害触发”处理，避免依赖射击/武器命中入口并防止重复触发。
    if (HasXianqiDamageTriggeredCombatSpell(proto))
        return false;

    if (proto->Class != ITEM_CLASS_WEAPON)
        return true;

    // 专属仙器槽没有官方手位语义，按装备特效处理；扩展武器槽按攻击手位处理。
    if (config->bodyPart == XIANQI_PART_NONE)
        return true;

    return config->bodyPart == GetXianqiAttackBodyPart(attType);
}

bool HasXianqiItemUseSpell(Item* item, ItemTemplate const* proto)
{
    if (!item || !proto)
        return false;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        if (proto->Spells[i].SpellId && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
            return true;

    for (uint8 enchantSlot = 0; enchantSlot < MAX_ENCHANTMENT_SLOT; ++enchantSlot)
    {
        uint32 enchantId = item->GetEnchantmentId(EnchantmentSlot(enchantSlot));
        if (!enchantId)
            continue;

        SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!enchant)
            continue;

        for (uint8 effectIndex = 0; effectIndex < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++effectIndex)
            if (enchant->type[effectIndex] == ITEM_ENCHANTMENT_TYPE_USE_SPELL && enchant->spellid[effectIndex])
                return true;
    }

    return false;
}

//=============================================================================
// XianqiSlotMgr - 仙器槽位管理器
//=============================================================================
class XianqiSlotMgr
{
public:
    static XianqiSlotMgr* instance()
    {
        static XianqiSlotMgr instance;
        return &instance;
    }

    void LoadConfig()
    {
        _enabled = sConfigMgr->GetOption<bool>("仙器系统.启用", true);
        _debugMode = sConfigMgr->GetOption<bool>("仙器系统.调试模式", false);
        _statMultiplier = sConfigMgr->GetOption<float>("仙器系统.属性倍率", 1.0f);
        _requiredLevel = sConfigMgr->GetOption<uint32>("仙器系统.需要等级", 1);
        _autoUnlock = sConfigMgr->GetOption<bool>("仙器系统.自动解锁", false);
    }

    bool IsEnabled() const { return _enabled; }
    bool IsDebugMode() const { return _debugMode; }

    void Initialize()
    {
        LoadSlotConfigs();
        LoadArtifactItems();

        LOG_INFO("server.loading", "→仙器槽位系统√ 槽位={} 仙器注册={}",
            static_cast<uint32>(_slotConfigs.size()), static_cast<uint32>(_artifactItems.size()));
    }

    void LoadSlotConfigs()
    {
        _slotConfigs.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `槽位ID`, `槽位名称`, `槽位类型`, `解锁需求ID`, `部位` FROM `_仙门_仙器槽位` ORDER BY `槽位ID`");

        if (!result)
        {
            LOG_WARN("module", "仙器系统: 未找到槽位配置，请导入 sql/world/_仙门系统_配置.sql");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            XianqiSlotConfig config;
            config.slotId = fields[0].Get<uint8>();
            config.name = fields[1].Get<std::string>();
            config.slotType = fields[2].Get<uint8>();
            config.unlockRequirementId = fields[3].Get<uint32>();
            config.bodyPart = fields[4].Get<uint8>();

            if (!IsValidXianqiSlot(config.slotId))
                continue;

            _slotConfigs[config.slotId] = config;
        } while (result->NextRow());
    }

    void LoadArtifactItems()
    {
        _artifactItems.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `物品ID`, `仙器槽位ID` FROM `_仙门_仙器物品`");

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].Get<uint32>();
            uint8 slotId = fields[1].Get<uint8>();
            if (itemId && IsValidXianqiSlot(slotId))
                _artifactItems[itemId] = slotId;
        } while (result->NextRow());
    }

    XianqiSlotConfig const* GetSlotConfig(uint8 slot) const
    {
        auto itr = _slotConfigs.find(slot);
        return itr == _slotConfigs.end() ? nullptr : &itr->second;
    }

    std::string GetSlotName(uint8 slot) const
    {
        if (XianqiSlotConfig const* config = GetSlotConfig(slot))
            return config->name;
        return "未知槽位";
    }

    bool IsArtifactItem(uint32 itemId) const
    {
        return _artifactItems.find(itemId) != _artifactItems.end();
    }

    uint8 GetArtifactSlotForItem(uint32 itemId) const
    {
        auto itr = _artifactItems.find(itemId);
        return itr == _artifactItems.end() ? 0 : itr->second;
    }

    PlayerXianqiStatus* GetPlayerStatus(uint32 playerGuid)
    {
        auto itr = _playerStatus.find(playerGuid);
        return itr == _playerStatus.end() ? nullptr : &itr->second;
    }

    //=========================================================================
    // 玩家数据
    //=========================================================================

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        ClearPlayerData(playerGuid);

        PlayerXianqiStatus status;
        status.playerGuid = playerGuid;

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `槽位1`,`槽位2`,`槽位3`,`槽位4`,`槽位5`,`槽位6`,`槽位7`,`槽位8`,`槽位9`,`槽位10`,"
            "`槽位11`,`槽位12`,`槽位13`,`槽位14`,`槽位15`,`槽位16`,`槽位17`,`槽位18`,`槽位19`,`槽位20`,"
            "`槽位21`,`槽位22`,`槽位23`,`槽位24`,`槽位25`,`槽位26`,`槽位27`,`槽位28`,`槽位29` "
            "FROM `_仙门_仙器玩家槽位` WHERE `角色GUID` = {}", playerGuid))
        {
            Field* fields = result->Fetch();
            for (uint8 slot = XIANQI_SLOT_FIRST; slot <= XIANQI_SLOT_LAST; ++slot)
                if (fields[slot - XIANQI_SLOT_FIRST].Get<uint8>())
                    status.unlockedSlots.insert(slot);
        }

        if (_autoUnlock)
            for (uint8 slot = XIANQI_SLOT_FIRST; slot <= XIANQI_SLOT_LAST; ++slot)
                status.unlockedSlots.insert(slot);

        _playerStatus[playerGuid] = status;

        LoadXianqiItems(player);
    }

    void LoadXianqiItems(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status)
            return;

        QueryResult invResult = CharacterDatabase.Query(
            "SELECT ci.slot, ci.item, ii.itemEntry, ii.creatorGuid, ii.giftCreatorGuid, ii.count, "
            "ii.duration, ii.charges, ii.flags, ii.enchantments, ii.randomPropertyId, ii.durability, "
            "ii.playedTime, ii.text "
            "FROM character_inventory ci "
            "JOIN item_instance ii ON ci.item = ii.guid "
            "WHERE ci.guid = {} AND ci.bag = {}", playerGuid, XIANQI_VIRTUAL_BAG);

        if (!invResult)
            return;

        status->slots.clear();

        do
        {
            Field* fields = invResult->Fetch();
            uint8 slot = fields[0].Get<uint8>();
            uint32 itemGuid = fields[1].Get<uint32>();
            uint32 itemEntry = fields[2].Get<uint32>();

            if (!IsValidXianqiSlot(slot))
            {
                LOG_ERROR("module", "仙器系统: 无效槽位 {} 物品GUID={}", slot, itemGuid);
                continue;
            }

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
            if (!proto)
            {
                LOG_ERROR("module", "仙器系统: 物品模板不存在 itemEntry={} itemGuid={} 槽位={}，清理记录",
                    itemEntry, itemGuid, slot);
                CharacterDatabase.Execute(
                    "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                    playerGuid, XIANQI_VIRTUAL_BAG, slot);
                continue;
            }

            Item* item = NewItemOrBag(proto);
            if (!item)
            {
                LOG_ERROR("module", "仙器系统: 无法创建物品实例 itemEntry={}", itemEntry);
                CharacterDatabase.Execute(
                    "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                    playerGuid, XIANQI_VIRTUAL_BAG, slot);
                CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
                continue;
            }

            if (!item->LoadFromDB(itemGuid, player->GetGUID(), fields + 3, itemEntry))
            {
                LOG_ERROR("module", "仙器系统: 从数据库加载物品失败 GUID={}", itemGuid);
                delete item;
                CharacterDatabase.Execute(
                    "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                    playerGuid, XIANQI_VIRTUAL_BAG, slot);
                CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
                continue;
            }

            // 清空 OwnerGUID + ITEM_UNCHANGED：让核心存档/更新队列完全忽略，由本模块自管
            item->SetOwnerGUID(ObjectGuid::Empty);
            item->FSetState(ITEM_UNCHANGED);

            XianqiSlotData slotData;
            slotData.itemId = itemEntry;
            slotData.itemGuid = itemGuid;
            slotData.itemPtr = item;
            status->slots[slot] = slotData;
        } while (invResult->NextRow());
    }

    void ValidateEquippedItems(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status)
            return;

        std::vector<uint8> slotsToRemove;

        for (auto& pair : status->slots)
        {
            uint8 slot = pair.first;
            XianqiSlotData& slotData = pair.second;

            if (!slotData.itemPtr)
            {
                slotsToRemove.push_back(slot);
                LOG_WARN("module", "仙器系统: 玩家 {} 槽位 {} 的物品指针无效，将移除记录", player->GetName(), slot);
                continue;
            }

            QueryResult result = CharacterDatabase.Query(
                "SELECT guid FROM item_instance WHERE guid = {}", slotData.itemGuid);
            if (!result)
            {
                slotsToRemove.push_back(slot);
                delete slotData.itemPtr;
                slotData.itemPtr = nullptr;
                LOG_WARN("module", "仙器系统: 玩家 {} 槽位 {} 的物品(GUID:{}) 已不存在，将移除记录",
                    player->GetName(), slot, slotData.itemGuid);
                continue;
            }

            std::string wearControlError;
            if (!CanEquipByWearControl(player, slotData.itemId, slot, wearControlError))
            {
                slotsToRemove.push_back(slot);
                LOG_WARN("module", "仙器系统: 玩家 {} 槽位 {} 的物品 itemId={} 不满足穿戴控制 reason={}，将移除记录",
                    player->GetName(), slot, slotData.itemId, wearControlError);
            }
        }

        for (uint8 slot : slotsToRemove)
        {
            RollbackSlotEffects(player, status, slot);

            CharacterDatabase.Execute(
                "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                playerGuid, XIANQI_VIRTUAL_BAG, slot);

            status->slots.erase(slot);
        }

        if (!slotsToRemove.empty())
            UpdatePlayerStats(player);
    }

    void SaveXianqiItems(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status || status->slots.empty())
            return;

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (auto& pair : status->slots)
        {
            Item* item = pair.second.itemPtr;
            if (!item)
            {
                LOG_ERROR("module", "仙器系统: 槽位 {} 物品指针为空，无法保存", pair.first);
                continue;
            }

            // 不调用 SaveToDB（ITEM_REMOVED 状态下会删物品），也不恢复 OwnerGUID（防进更新队列），
            // 直接 REPLACE item_instance，owner 用玩家 GUID 落库
            uint8 index = 0;
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_INSTANCE);
            stmt->SetData(  index, item->GetEntry());
            stmt->SetData(++index, playerGuid);
            stmt->SetData(++index, item->GetGuidValue(ITEM_FIELD_CREATOR).GetCounter());
            stmt->SetData(++index, item->GetGuidValue(ITEM_FIELD_GIFTCREATOR).GetCounter());
            stmt->SetData(++index, item->GetCount());
            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_DURATION));

            std::ostringstream ssSpells;
            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                ssSpells << item->GetSpellCharges(i) << ' ';
            stmt->SetData(++index, ssSpells.str());

            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_FLAGS));

            std::ostringstream ssEnchants;
            for (uint8 i = 0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                ssEnchants << item->GetEnchantmentId(EnchantmentSlot(i)) << ' ';
                ssEnchants << item->GetEnchantmentDuration(EnchantmentSlot(i)) << ' ';
                ssEnchants << item->GetEnchantmentCharges(EnchantmentSlot(i)) << ' ';
            }
            stmt->SetData(++index, ssEnchants.str());

            stmt->SetData(++index, item->GetItemRandomPropertyId());
            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_DURABILITY));
            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_CREATE_PLAYED_TIME));
            stmt->SetData(++index, item->GetText());
            stmt->SetData(++index, pair.second.itemGuid);
            trans->Append(stmt);
        }

        CharacterDatabase.CommitTransaction(trans);
    }

    void ClearPlayerData(uint32 playerGuid)
    {
        auto itr = _playerStatus.find(playerGuid);
        if (itr == _playerStatus.end())
            return;

        for (auto& pair : itr->second.slots)
        {
            if (pair.second.itemPtr)
            {
                delete pair.second.itemPtr;
                pair.second.itemPtr = nullptr;
            }
        }
        _playerStatus.erase(itr);
    }

    void DeletePlayerData(uint32 playerGuid)
    {
        ClearPlayerData(playerGuid);

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT item FROM character_inventory WHERE guid = {} AND bag = {}",
            playerGuid, XIANQI_VIRTUAL_BAG))
        {
            do
            {
                uint32 itemGuid = result->Fetch()[0].Get<uint32>();
                CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
                CharacterDatabase.Execute("DELETE FROM item_text WHERE guid = {}", itemGuid);
            } while (result->NextRow());
        }

        CharacterDatabase.Execute(
            "DELETE FROM character_inventory WHERE guid = {} AND bag = {}",
            playerGuid, XIANQI_VIRTUAL_BAG);
        CharacterDatabase.Execute("DELETE FROM `_仙门_仙器玩家槽位` WHERE `角色GUID` = {}", playerGuid);
    }

    //=========================================================================
    // 槽位解锁
    //=========================================================================

    bool IsSlotUnlocked(Player* player, uint8 slot)
    {
        if (!player || !IsValidXianqiSlot(slot))
            return false;

        if (!GetSlotConfig(slot))
            return false;

        if (_autoUnlock)
            return true;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        return status && status->unlockedSlots.find(slot) != status->unlockedSlots.end();
    }

    bool UnlockSlot(Player* player, uint8 slot)
    {
        if (!player || !IsValidXianqiSlot(slot))
            return false;

        if (IsSlotUnlocked(player, slot))
        {
            ChatHandler(player->GetSession()).SendSysMessage("该槽位已经解锁。");
            SendXianqiDataToClient(player);
            return false;
        }

        if (!CheckSlotUnlockRequirement(player, slot))
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status)
        {
            LoadPlayerData(player);
            status = GetPlayerStatus(playerGuid);
            if (!status)
                return false;
        }

        status->unlockedSlots.insert(slot);

        // slot 已由 IsValidXianqiSlot 限定在 1-29，列名拼接无注入风险
        std::string col = "槽位" + std::to_string(slot);
        CharacterDatabase.Execute(
            "INSERT INTO `_仙门_仙器玩家槽位` (`角色GUID`, `{}`) "
            "VALUES ({}, 1) "
            "ON DUPLICATE KEY UPDATE `{}` = 1",
            col, playerGuid, col);

        ChatHandler(player->GetSession()).PSendSysMessage("|cff66ffcc[仙器系统]|r 成功解锁槽位: {}", GetSlotName(slot));
        SendXianqiDataToClient(player);
        return true;
    }

    bool CheckSlotUnlockRequirement(Player* player, uint8 slot)
    {
        XianqiSlotConfig const* config = GetSlotConfig(slot);
        if (!player || !config)
            return false;

        if (!config->unlockRequirementId)
            return true;

        RequirementInterface* reqModule = GetXianqiRequirementModule();
        if (!reqModule)
        {
            LOG_ERROR("module", "仙器系统: 需求模板系统未初始化，无法检查解锁需求");
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙器系统]|r 需求模板系统未加载，无法解锁槽位");
            return false;
        }

        if (!reqModule->CheckRequirements(player, config->unlockRequirementId, false))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[仙器系统]|r 不满足解锁条件，请查看需求详情：");
            reqModule->CheckRequirements(player, config->unlockRequirementId, true);
            return false;
        }

        if (!reqModule->ConsumeRequirements(player, config->unlockRequirementId))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙器系统]|r 消耗需求资源失败！");
            return false;
        }

        return true;
    }

    //=========================================================================
    // 装备校验
    //=========================================================================

    bool CanEquipItemInSlot(Player* player, uint8 slot, uint32 itemId, std::string& error)
    {
        XianqiSlotConfig const* config = GetSlotConfig(slot);
        if (!player || !config)
        {
            error = "槽位配置不存在。";
            return false;
        }

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            error = "物品模板不存在。";
            return false;
        }

        if (!CanEquipByWearControl(player, itemId, slot, error))
            return false;

        if (proto->RequiredLevel > player->GetLevel())
        {
            error = "等级不足，无法装备该物品。";
            return false;
        }

        if (proto->AllowableClass != 0 && proto->AllowableClass != -1
            && !(proto->AllowableClass & player->getClassMask()))
        {
            error = "职业不符，无法装备该物品。";
            return false;
        }

        if (proto->AllowableRace != 0 && proto->AllowableRace != -1
            && !(proto->AllowableRace & player->getRaceMask()))
        {
            error = "种族不符，无法装备该物品。";
            return false;
        }

        if (proto->RequiredSkill != 0
            && player->GetSkillValue(proto->RequiredSkill) < proto->RequiredSkillRank)
        {
            error = "技能不足，无法装备该物品。";
            return false;
        }

        if (proto->RequiredReputationFaction != 0
            && uint32(player->GetReputationRank(proto->RequiredReputationFaction)) < proto->RequiredReputationRank)
        {
            error = "声望不足，无法装备该物品。";
            return false;
        }

        if (proto->RequiredSpell != 0 && !player->HasSpell(proto->RequiredSpell))
        {
            error = "缺少所需技能，无法装备该物品。";
            return false;
        }

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());

        if (config->slotType == XIANQI_SLOT_TYPE_ARTIFACT)
        {
            // 仙器槽：只能放注册表中登记到该槽位的仙器
            uint8 expectedSlot = GetArtifactSlotForItem(itemId);
            if (!expectedSlot)
            {
                error = "该物品不是仙器，无法放入仙器槽。";
                return false;
            }
            if (expectedSlot != slot)
            {
                error = "该仙器属于槽位【" + GetSlotName(expectedSlot) + "】。";
                return false;
            }

            // 仙器同 entry 全槽位唯一
            if (status)
            {
                for (auto const& pair : status->slots)
                {
                    if (pair.first != slot && pair.second.itemId == itemId)
                    {
                        error = "该仙器已装备在其他槽位。";
                        return false;
                    }
                }
            }

            return true;
        }

        // 扩展槽：与官方装备槽一一镜像
        if (IsArtifactItem(itemId))
        {
            error = "仙器只能放入对应的仙器槽。";
            return false;
        }

        if (proto->InventoryType == 0)
        {
            error = "该物品不是可装备物品。";
            return false;
        }

        uint8 expectedPart = MapXianqiInventoryTypeToBodyPart(proto->InventoryType);
        if (expectedPart == XIANQI_PART_NONE)
        {
            error = "该物品无法装备到扩展槽。";
            return false;
        }

        uint8 const targetPart = config->bodyPart;
        bool partMatches = (expectedPart == targetPart);

        // 戒指/饰品可放入两个镜像槽
        if (!partMatches && expectedPart == XIANQI_PART_FINGER1 && targetPart == XIANQI_PART_FINGER2)
            partMatches = true;
        if (!partMatches && expectedPart == XIANQI_PART_TRINKET1 && targetPart == XIANQI_PART_TRINKET2)
            partMatches = true;

        // 主手/副手扩展槽不做武器类型限制：单手/双手/主手专用/副手专用/盾牌均可双持
        // （虚拟槽只给属性不参与攻击，无需双持技能、无双手互斥）
        if (!partMatches
            && (targetPart == XIANQI_PART_MAINHAND || targetPart == XIANQI_PART_OFFHAND)
            && (expectedPart == XIANQI_PART_MAINHAND || expectedPart == XIANQI_PART_OFFHAND))
            partMatches = true;

        if (!partMatches)
        {
            error = "该物品部位与此扩展槽不匹配。";
            return false;
        }

        // Unique-Equipped：官方装备栏 + 全部仙器/扩展槽合并查重
        if (proto->Flags & ITEM_FLAG_UNIQUE_EQUIPPABLE)
        {
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (equipped && equipped->GetEntry() == itemId)
                {
                    error = "唯一装备：该物品已在官方装备栏装备。";
                    return false;
                }
            }

            if (status)
            {
                for (auto const& pair : status->slots)
                {
                    if (pair.first != slot && pair.second.itemId == itemId)
                    {
                        error = "唯一装备：该物品已在其他槽位装备。";
                        return false;
                    }
                }
            }
        }

        // ItemLimitCategory 跨两套装备计数
        if (proto->ItemLimitCategory)
        {
            if (ItemLimitCategoryEntry const* limitEntry = sItemLimitCategoryStore.LookupEntry(proto->ItemLimitCategory))
            {
                uint32 count = 0;
                for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
                {
                    Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                    if (equipped && equipped->GetTemplate()
                        && equipped->GetTemplate()->ItemLimitCategory == proto->ItemLimitCategory)
                        ++count;
                }

                if (status)
                {
                    for (auto const& pair : status->slots)
                    {
                        if (pair.first == slot)
                            continue;
                        ItemTemplate const* slotProto = sObjectMgr->GetItemTemplate(pair.second.itemId);
                        if (slotProto && slotProto->ItemLimitCategory == proto->ItemLimitCategory)
                            ++count;
                    }
                }

                if (count >= limitEntry->maxCount)
                {
                    error = "同类物品装备数量已达上限。";
                    return false;
                }
            }
        }

        return true;
    }

    //=========================================================================
    // 穿戴 / 卸下
    //=========================================================================

    Item* FindItemInBags(Player* player, uint32 itemGuid)
    {
        if (!player)
            return nullptr;

        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item = bag->GetItemByPos(j);
                    if (item && item->GetGUID().GetCounter() == itemGuid)
                        return item;
                }
            }
        }

        return nullptr;
    }

    bool IsItemEquippedInXianqi(Player* player, uint32 itemGuid)
    {
        if (!player)
            return false;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return false;

        for (auto const& pair : status->slots)
            if (pair.second.itemGuid == itemGuid)
                return true;

        return false;
    }

    void CastDamageTriggeredItemCombatSpells(Player* player, Unit* target, uint32 procVictim, uint32 procEx,
        bool includeOfficialItems, bool officialRangedOnly, bool includeXianqiItems)
    {
        if (!player || !target || !target->IsAlive() || target == player)
            return;

        if (includeOfficialItems)
        {
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                if (officialRangedOnly && slot != EQUIPMENT_SLOT_RANGED)
                    continue;

                Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (!equippedItem || equippedItem->IsBroken())
                    continue;

                ItemTemplate const* equippedProto = equippedItem->GetTemplate();
                if (!HasXianqiDamageTriggeredCombatSpell(equippedProto))
                    continue;

                CastXianqiDamageTriggeredCombatSpells(player, target, equippedItem, equippedProto);
            }
        }

        if (!includeXianqiItems)
            return;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status || status->slots.empty())
            return;

        for (auto const& slotPair : status->slots)
        {
            Item* xianqiItem = slotPair.second.itemPtr;
            if (!xianqiItem || xianqiItem->IsBroken())
                continue;

            ItemTemplate const* xianqiProto = xianqiItem->GetTemplate();
            if (!HasXianqiDamageTriggeredCombatSpell(xianqiProto))
                continue;

            CastXianqiDamageTriggeredCombatSpells(player, target, xianqiItem, xianqiProto);
        }
    }

    bool EquipItem(Player* player, uint8 slot, uint32 itemId, uint32 itemGuid)
    {
        if (!player || !IsValidXianqiSlot(slot))
            return false;

        if (player->GetLevel() < _requiredLevel)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("需要达到 {} 级才能使用仙器系统。", _requiredLevel);
            return false;
        }

        if (!IsSlotUnlocked(player, slot))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[仙器系统]|r 该槽位尚未解锁。");
            return false;
        }

        std::string error;
        if (!CanEquipItemInSlot(player, slot, itemId, error))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[仙器系统]|r {}", error);
            return false;
        }

        Item* item = FindItemInBags(player, itemGuid);
        if (!item || item->GetEntry() != itemId)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙器系统]|r 背包中未找到该物品。");
            return false;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status)
        {
            LoadPlayerData(player);
            status = GetPlayerStatus(playerGuid);
            if (!status)
                return false;
        }

        // 同一物品实例不能装两个槽
        for (auto const& pair : status->slots)
        {
            if (pair.second.itemGuid == itemGuid)
            {
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙器系统]|r 该物品已装备在其他槽位。");
                return false;
            }
        }

        // 槽位已有装备则先卸下
        if (status->slots.find(slot) != status->slots.end())
        {
            if (!UnequipItem(player, slot))
                return false;
        }

        uint8 bagSlot = item->GetBagSlot();
        uint8 itemSlot = item->GetSlot();

        XianqiSlotData slotData;
        slotData.itemId = itemId;
        slotData.itemGuid = itemGuid;
        slotData.itemPtr = item;
        status->slots[slot] = slotData;

        // 先脱离更新队列，防止 _SaveInventory 把它标记 ITEM_REMOVED 删除
        item->RemoveFromUpdateQueueOf(player);
        player->MoveItemFromInventory(bagSlot, itemSlot, true);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        item->DeleteFromInventoryDB(trans);

        CharacterDatabasePreparedStatement* invStmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
        invStmt->SetData(0, playerGuid);
        invStmt->SetData(1, XIANQI_VIRTUAL_BAG);
        invStmt->SetData(2, slot);
        invStmt->SetData(3, itemGuid);
        trans->Append(invStmt);

        {
            uint8 index = 0;
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_INSTANCE);
            stmt->SetData(  index, item->GetEntry());
            stmt->SetData(++index, playerGuid);
            stmt->SetData(++index, item->GetGuidValue(ITEM_FIELD_CREATOR).GetCounter());
            stmt->SetData(++index, item->GetGuidValue(ITEM_FIELD_GIFTCREATOR).GetCounter());
            stmt->SetData(++index, item->GetCount());
            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_DURATION));

            std::ostringstream ssSpells;
            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                ssSpells << item->GetSpellCharges(i) << ' ';
            stmt->SetData(++index, ssSpells.str());

            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_FLAGS));

            std::ostringstream ssEnchants;
            for (uint8 i = 0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                ssEnchants << item->GetEnchantmentId(EnchantmentSlot(i)) << ' ';
                ssEnchants << item->GetEnchantmentDuration(EnchantmentSlot(i)) << ' ';
                ssEnchants << item->GetEnchantmentCharges(EnchantmentSlot(i)) << ' ';
            }
            stmt->SetData(++index, ssEnchants.str());

            stmt->SetData(++index, item->GetItemRandomPropertyId());
            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_DURABILITY));
            stmt->SetData(++index, item->GetUInt32Value(ITEM_FIELD_CREATE_PLAYED_TIME));
            stmt->SetData(++index, item->GetText());
            stmt->SetData(++index, itemGuid);
            trans->Append(stmt);
        }

        CharacterDatabase.CommitTransaction(trans);

        ApplyItemEffect(player, itemId, slot, true);

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        ChatHandler(player->GetSession()).PSendSysMessage("|cff66ffcc[仙器系统]|r {} 已装备到 {}",
            proto ? proto->Name1 : "未知物品", GetSlotName(slot));

        SendXianqiDataToClient(player);
        NotifyXianqiAttributePanelRefresh(player);
        return true;
    }

    bool UnequipItem(Player* player, uint8 slot)
    {
        if (!player || !IsValidXianqiSlot(slot))
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status)
            return false;

        auto itr = status->slots.find(slot);
        if (itr == status->slots.end())
        {
            ChatHandler(player->GetSession()).SendSysMessage("该槽位没有装备。");
            return false;
        }

        uint32 itemId = itr->second.itemId;
        uint32 itemGuid = itr->second.itemGuid;
        Item* item = itr->second.itemPtr;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);

        RollbackSlotEffects(player, status, slot);
        UpdatePlayerStats(player);

        if (item)
        {
            // 物品放回背包由核心管理，必须恢复 OwnerGUID
            item->SetOwnerGUID(player->GetGUID());

            ItemPosCountVec dest;
            InventoryResult result = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
            if (result == EQUIP_ERR_OK)
            {
                player->MoveItemToInventory(dest, item, true, true);

                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
                delStmt->SetData(0, XIANQI_VIRTUAL_BAG);
                delStmt->SetData(1, slot);
                delStmt->SetData(2, playerGuid);
                trans->Append(delStmt);
                player->SaveInventoryAndGoldToDB(trans);
                CharacterDatabase.CommitTransaction(trans);
            }
            else
            {
                MailDraft draft("仙器系统", "您的背包已满，仙器装备已通过邮件返还。");
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
                delStmt->SetData(0, XIANQI_VIRTUAL_BAG);
                delStmt->SetData(1, slot);
                delStmt->SetData(2, playerGuid);
                trans->Append(delStmt);
                draft.AddItem(item);
                draft.SendMailTo(trans, MailReceiver(player, playerGuid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
                CharacterDatabase.CommitTransaction(trans);

                ChatHandler(player->GetSession()).SendSysMessage("背包已满，物品已通过邮件返还。");
            }
        }
        else
        {
            CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
            CharacterDatabase.Execute(
                "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                playerGuid, XIANQI_VIRTUAL_BAG, slot);
            ChatHandler(player->GetSession()).SendSysMessage("物品数据异常，已清除仙器装备记录。");
            LOG_ERROR("module", "仙器系统: 卸下装备时物品指针无效 itemId={} itemGuid={}", itemId, itemGuid);
        }

        status->slots.erase(itr);

        ChatHandler(player->GetSession()).PSendSysMessage("|cff66ffcc[仙器系统]|r {} 已从 {} 卸下",
            proto ? proto->Name1 : "未知物品", GetSlotName(slot));

        SendXianqiDataToClient(player);
        NotifyXianqiAttributePanelRefresh(player);
        return true;
    }

    void UnequipAllItems(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerXianqiStatus* status = GetPlayerStatus(playerGuid);
        if (!status || status->slots.empty())
            return;

        RemoveAllEffects(player);

        std::vector<Item*> itemsToMail;
        CharacterDatabaseTransaction inventoryTrans = CharacterDatabase.BeginTransaction();

        for (auto& pair : status->slots)
        {
            CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
            delStmt->SetData(0, XIANQI_VIRTUAL_BAG);
            delStmt->SetData(1, pair.first);
            delStmt->SetData(2, playerGuid);
            inventoryTrans->Append(delStmt);

            Item* item = pair.second.itemPtr;
            if (!item)
                continue;

            item->SetOwnerGUID(player->GetGUID());

            ItemPosCountVec dest;
            if (player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) == EQUIP_ERR_OK)
                player->MoveItemToInventory(dest, item, true, true);
            else
                itemsToMail.push_back(item);
        }

        player->SaveInventoryAndGoldToDB(inventoryTrans);
        CharacterDatabase.CommitTransaction(inventoryTrans);

        if (!itemsToMail.empty())
        {
            size_t totalItems = itemsToMail.size();
            for (size_t i = 0; i < totalItems; i += MAX_MAIL_ITEMS)
            {
                MailDraft draft("仙器系统", "您的背包已满，仙器装备已通过邮件返还。");
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

                size_t endIndex = std::min(i + static_cast<size_t>(MAX_MAIL_ITEMS), totalItems);
                for (size_t j = i; j < endIndex; ++j)
                    draft.AddItem(itemsToMail[j]);

                draft.SendMailTo(trans, MailReceiver(player, playerGuid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
                CharacterDatabase.CommitTransaction(trans);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("背包已满，{} 件物品已通过邮件返还。", totalItems);
        }

        status->slots.clear();

        ChatHandler(player->GetSession()).SendSysMessage("|cff66ffcc[仙器系统]|r 已卸下所有仙器装备。");
        SendXianqiDataToClient(player);
        NotifyXianqiAttributePanelRefresh(player);
    }

    //=========================================================================
    // 属性应用
    //=========================================================================

    void ApplyAllEffects(Player* player)
    {
        if (!player)
            return;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return;

        bool restoreCanModifyStats = player->CanModifyStats();
        if (restoreCanModifyStats)
            player->SetCanModifyStats(false);

        for (auto const& pair : status->slots)
            ApplyItemEffect(player, pair.second.itemId, pair.first, true, false);

        if (restoreCanModifyStats)
            player->SetCanModifyStats(true);

        if (!status->slots.empty())
            UpdatePlayerStats(player);
    }

    void RemoveAllEffects(Player* player)
    {
        if (!player)
            return;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return;

        for (auto const& slotPair : status->slotSpells)
        {
            Item* item = nullptr;
            auto slotItr = status->slots.find(slotPair.first);
            if (slotItr != status->slots.end())
                item = slotItr->second.itemPtr;

            for (uint32 spellId : slotPair.second)
                RemoveXianqiEquipSpell(player, item, spellId);
        }
        status->slotSpells.clear();

        for (auto const& itemSetPair : status->slotItemSets)
        {
            auto slotItr = status->slots.find(itemSetPair.first);
            if (slotItr == status->slots.end())
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(slotItr->second.itemId);
            if (proto && proto->ItemSet)
                RemoveItemsSetItem(player, proto);
        }
        status->slotItemSets.clear();

        for (auto const& slotPair : status->slotStats)
            for (XianqiAppliedStat const& effect : slotPair.second)
                RemoveStatEffect(player, effect.statType, effect.statValue);
        status->slotStats.clear();

        UpdatePlayerStats(player);
    }

    void RefreshEffects(Player* player)
    {
        if (!player)
            return;

        RemoveAllEffects(player);
        ApplyAllEffects(player);
        NotifyXianqiAttributePanelRefresh(player);
    }

    void ApplyItemEffect(Player* player, uint32 itemId, uint8 slot, bool apply, bool updateStats = true)
    {
        if (!player)
            return;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return;

        Item* item = nullptr;
        auto slotItr = status->slots.find(slot);
        if (slotItr != status->slots.end())
            item = slotItr->second.itemPtr;

        if (apply && item && item->IsBroken())
            return;

        if (item && proto->ItemSet)
        {
            if (apply)
            {
                AddItemsSetItem(player, item);
                status->slotItemSets[slot] = proto->ItemSet;
            }
            else
            {
                RemoveItemsSetItem(player, proto);
                status->slotItemSets.erase(slot);
            }
        }

        float totalMultiplier = _statMultiplier;

        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (i >= proto->StatsCount)
                break;

            int256 val = proto->ItemStatValue256[i];
            if (val == 0)
                continue;

            val = Acore::Number::ToInt256Saturated(Acore::Number::ToLongDouble(val) * static_cast<long double>(totalMultiplier));
            uint32 statType = proto->ItemStat[i].ItemStatType;

            ApplyStatByType(player, statType, val, apply);

            if (apply)
                status->slotStats[slot].push_back({ statType, val });
        }

        if (proto->Armor256 > 0)
        {
            int256 armorVal = Acore::Number::ToInt256Saturated(Acore::Number::ToLongDouble(proto->Armor256) * static_cast<long double>(totalMultiplier));
            player->HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, Acore::Number::ToFloat(armorVal), apply);
            if (apply)
                status->slotStats[slot].push_back({ 1000, armorVal });
        }

        if (proto->Block > 0)
        {
            int32 blockVal = int32(proto->Block * totalMultiplier);
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(blockVal), apply);
            if (apply)
                status->slotStats[slot].push_back({ 1001, blockVal });
        }

        auto applyResistance = [&](int32 resistance, UnitMods mod, uint32 marker)
        {
            if (resistance <= 0)
                return;

            int32 val = int32(resistance * totalMultiplier);
            player->HandleStatModifier(mod, BASE_VALUE, float(val), apply);
            if (apply)
                status->slotStats[slot].push_back({ marker, val });
        };
        applyResistance(proto->HolyRes, UNIT_MOD_RESISTANCE_HOLY, 1002);
        applyResistance(proto->FireRes, UNIT_MOD_RESISTANCE_FIRE, 1003);
        applyResistance(proto->NatureRes, UNIT_MOD_RESISTANCE_NATURE, 1004);
        applyResistance(proto->FrostRes, UNIT_MOD_RESISTANCE_FROST, 1005);
        applyResistance(proto->ShadowRes, UNIT_MOD_RESISTANCE_SHADOW, 1006);
        applyResistance(proto->ArcaneRes, UNIT_MOD_RESISTANCE_ARCANE, 1007);

        // 附魔（不乘倍率）
        if (item)
        {
            for (uint8 enchantSlot = 0; enchantSlot < MAX_ENCHANTMENT_SLOT; ++enchantSlot)
            {
                uint32 enchantId = item->GetEnchantmentId(EnchantmentSlot(enchantSlot));
                if (!enchantId)
                    continue;

                SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
                if (!enchant)
                    continue;

                for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
                {
                    uint32 enchType = enchant->type[s];
                    int32 amount = enchant->amount[s];

                    switch (enchType)
                    {
                        case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:
                        case ITEM_ENCHANTMENT_TYPE_USE_SPELL:
                        case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                            // 命中、使用、武器白字类附魔由对应官方触发入口处理，装备时不直接施放。
                            break;
                        case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
                            if (enchant->spellid[s])
                                ApplySlotSpell(player, status, slot, enchant->spellid[s], apply);
                            break;
                        case ITEM_ENCHANTMENT_TYPE_STAT:
                            if (amount && enchant->spellid[s] < MAX_ITEM_MOD)
                            {
                                uint32 statType = enchant->spellid[s];
                                ApplyStatByType(player, statType, amount, apply);
                                if (apply)
                                    status->slotStats[slot].push_back({ statType, amount });
                            }
                            break;
                        case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                            if (amount && enchant->spellid[s] < MAX_SPELL_SCHOOL)
                            {
                                SpellSchools school = SpellSchools(enchant->spellid[s]);
                                if (school == SPELL_SCHOOL_NORMAL)
                                {
                                    // school 0 的抗性附魔语义为护甲
                                    player->HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(amount), apply);
                                    if (apply)
                                        status->slotStats[slot].push_back({ 1000, amount });
                                }
                                else
                                {
                                    player->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + school), BASE_VALUE, float(amount), apply);
                                    if (apply)
                                        status->slotStats[slot].push_back({ 1001u + uint32(school), amount });
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (proto->Spells[i].SpellId <= 0)
                continue;

            uint32 const spellId = uint32(proto->Spells[i].SpellId);
            if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
                ApplySlotSpell(player, status, slot, spellId, apply);
        }

        if (updateStats)
            UpdatePlayerStats(player);
    }

    void SendXianqiDataToClient(Player* player)
    {
        if (!player || !player->GetSession())
            return;

        PlayerXianqiStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return;

        uint32 unlockedBitmap = 0;
        for (uint8 slot = XIANQI_SLOT_FIRST; slot <= XIANQI_SLOT_LAST; ++slot)
            if (IsSlotUnlocked(player, slot))
                unlockedBitmap |= (1u << (slot - 1));

        std::ostringstream equipStr;
        bool first = true;
        for (auto const& pair : status->slots)
        {
            if (!first)
                equipStr << ",";

            uint32 itemSet = 0;
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(pair.second.itemId))
                itemSet = proto->ItemSet;

            equipStr << uint32(pair.first) << ":" << pair.second.itemId << ":" << pair.second.itemGuid << ":" << itemSet;
            first = false;
        }

        std::string prefix = std::string(XIANQI_ADDON_PREFIX) + "\t";
        std::string fullMessage = prefix + "U=" + std::to_string(unlockedBitmap) + ";E=" + equipStr.str();

        constexpr size_t MAX_ADDON_MSG_LEN = 250;

        auto sendMessage = [&](std::string const& message)
        {
            if (message.rfind(prefix, 0) == 0 && HermesBridge_SendAddonMessage(player, XIANQI_ADDON_PREFIX, message.substr(prefix.length())))
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message, 0);
            player->SendDirectMessage(&data);
        };

        if (fullMessage.length() <= MAX_ADDON_MSG_LEN)
        {
            sendMessage(fullMessage);
            return;
        }

        // 超长拆分：先发 U=（携带 E= 清空语义），再把装备数据按逗号边界分多条 E= 续包
        sendMessage(prefix + "U=" + std::to_string(unlockedBitmap));

        std::string equipData = equipStr.str();
        if (equipData.empty())
            return;

        std::string msg = prefix + "E=" + equipData;
        size_t const headerLen = prefix.length() + 2;
        while (msg.length() > MAX_ADDON_MSG_LEN)
        {
            size_t cutPos = msg.rfind(',', MAX_ADDON_MSG_LEN - 1);
            if (cutPos == std::string::npos || cutPos <= headerLen)
            {
                LOG_WARN("module", "仙器系统: 单条装备数据过长，可能导致客户端解析失败");
                break;
            }

            sendMessage(msg.substr(0, cutPos));
            msg = prefix + "E=" + msg.substr(cutPos + 1);
        }

        sendMessage(msg);
    }

private:
    // 卸下/失效时按记账回滚该槽位的属性与法术
    void RollbackSlotEffects(Player* player, PlayerXianqiStatus* status, uint8 slot)
    {
        Item* item = nullptr;
        auto slotItr = status->slots.find(slot);
        if (slotItr != status->slots.end())
            item = slotItr->second.itemPtr;

        auto statItr = status->slotStats.find(slot);
        if (statItr != status->slotStats.end())
        {
            for (XianqiAppliedStat const& effect : statItr->second)
                RemoveStatEffect(player, effect.statType, effect.statValue);
            status->slotStats.erase(statItr);
        }

        auto spellItr = status->slotSpells.find(slot);
        if (spellItr != status->slotSpells.end())
        {
            for (uint32 spellId : spellItr->second)
                RemoveXianqiEquipSpell(player, item, spellId);
            status->slotSpells.erase(spellItr);
        }

        auto itemSetItr = status->slotItemSets.find(slot);
        if (itemSetItr != status->slotItemSets.end())
        {
            ItemTemplate const* proto = nullptr;
            if (slotItr != status->slots.end())
                proto = sObjectMgr->GetItemTemplate(slotItr->second.itemId);

            if (proto && proto->ItemSet)
                RemoveItemsSetItem(player, proto);

            status->slotItemSets.erase(itemSetItr);
        }
    }

    void ApplySlotSpell(Player* player, PlayerXianqiStatus* status, uint8 slot, uint32 spellId, bool apply)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        Item* item = nullptr;
        auto slotItr = status->slots.find(slot);
        if (slotItr != status->slots.end())
            item = slotItr->second.itemPtr;

        if (apply)
        {
            ApplyXianqiEquipSpell(player, item, spellInfo);
            status->slotSpells[slot].push_back(spellId);
        }
        else
        {
            RemoveXianqiEquipSpell(player, item, spellId);

            auto& slotSpellList = status->slotSpells[slot];
            slotSpellList.erase(std::remove(slotSpellList.begin(), slotSpellList.end(), spellId), slotSpellList.end());
        }
    }

    // 物品属性/附魔属性统一按类型应用（apply=false 时反向应用，与 RemoveStatEffect 等价）
    void ApplyStatByType(Player* player, uint32 statType, int256 const& value, bool apply)
    {
        float statModValue = Acore::Number::ToFloat(value);
        int256 legacyVal = value;

        switch (statType)
        {
            case ITEM_MOD_MANA:
                player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, statModValue, apply);
                break;
            case ITEM_MOD_HEALTH:
                player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, statModValue, apply);
                break;
            case ITEM_MOD_AGILITY:
                player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, statModValue, apply);
                player->ApplyStatBuffMod(STAT_AGILITY, statModValue, apply);
                break;
            case ITEM_MOD_STRENGTH:
                player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, statModValue, apply);
                player->ApplyStatBuffMod(STAT_STRENGTH, statModValue, apply);
                break;
            case ITEM_MOD_INTELLECT:
                player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, statModValue, apply);
                player->ApplyStatBuffMod(STAT_INTELLECT, statModValue, apply);
                break;
            case ITEM_MOD_SPIRIT:
                player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, statModValue, apply);
                player->ApplyStatBuffMod(STAT_SPIRIT, statModValue, apply);
                break;
            case ITEM_MOD_STAMINA:
                player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, statModValue, apply);
                player->ApplyStatBuffMod(STAT_STAMINA, statModValue, apply);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                player->ApplyRatingMod(CR_DEFENSE_SKILL, legacyVal, apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                player->ApplyRatingMod(CR_DODGE, legacyVal, apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                player->ApplyRatingMod(CR_PARRY, legacyVal, apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                player->ApplyRatingMod(CR_BLOCK, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                player->ApplyRatingMod(CR_HIT_RANGED, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                player->ApplyRatingMod(CR_HIT_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                player->ApplyRatingMod(CR_CRIT_RANGED, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                player->ApplyRatingMod(CR_CRIT_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, legacyVal, apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                player->ApplyRatingMod(CR_HASTE_RANGED, legacyVal, apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                player->ApplyRatingMod(CR_HASTE_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, legacyVal, apply);
                player->ApplyRatingMod(CR_HIT_RANGED, legacyVal, apply);
                player->ApplyRatingMod(CR_HIT_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, legacyVal, apply);
                player->ApplyRatingMod(CR_CRIT_RANGED, legacyVal, apply);
                player->ApplyRatingMod(CR_CRIT_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, legacyVal, apply);
                player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, legacyVal, apply);
                player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
            case ITEM_MOD_RESILIENCE_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, legacyVal, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, legacyVal, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, legacyVal, apply);
                player->ApplyRatingMod(CR_HASTE_RANGED, legacyVal, apply);
                player->ApplyRatingMod(CR_HASTE_SPELL, legacyVal, apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                player->ApplyRatingMod(CR_EXPERTISE, legacyVal, apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, statModValue, apply);
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, statModValue, apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, statModValue, apply);
                break;
            case ITEM_MOD_MANA_REGENERATION:
                player->ApplyManaRegenBonus(ClampXianqiInt256ToInt32(value), apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                player->ApplyRatingMod(CR_ARMOR_PENETRATION, legacyVal, apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                player->ApplySpellPowerBonus(value, apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                player->ApplyHealthRegenBonus(ClampXianqiInt256ToInt32(value), apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                player->ApplySpellPenetrationBonus(ClampXianqiInt256ToInt32(value), apply);
                break;
            case ITEM_MOD_BLOCK_VALUE:
                player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, statModValue, apply);
                break;
            // 自定义记账类型（1000+），仅在回滚路径出现
            case 1000:
                player->HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, statModValue, apply);
                break;
            case 1001:
                player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, statModValue, apply);
                break;
            case 1002:
                player->HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, statModValue, apply);
                break;
            case 1003:
                player->HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, statModValue, apply);
                break;
            case 1004:
                player->HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, statModValue, apply);
                break;
            case 1005:
                player->HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, statModValue, apply);
                break;
            case 1006:
                player->HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, statModValue, apply);
                break;
            case 1007:
                player->HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, statModValue, apply);
                break;
            default:
                break;
        }
    }

    void RemoveStatEffect(Player* player, uint32 statType, int256 const& statValue)
    {
        if (!player || statValue == 0)
            return;

        ApplyStatByType(player, statType, statValue, false);
    }

    void UpdatePlayerStats(Player* player)
    {
        if (!player)
            return;

        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }

    bool _enabled = true;
    bool _debugMode = false;
    bool _autoUnlock = false;
    float _statMultiplier = 1.0f;
    uint32 _requiredLevel = 1;

    std::map<uint8, XianqiSlotConfig> _slotConfigs;
    std::unordered_map<uint32, uint8> _artifactItems;   // 物品ID → 仙器槽位ID
    std::map<uint32, PlayerXianqiStatus> _playerStatus;
};

#define sXianqiSlotMgr XianqiSlotMgr::instance()

//=============================================================================
// 脚本类
//=============================================================================

class XianqiWorldScript : public WorldScript
{
public:
    XianqiWorldScript() : WorldScript("XianqiWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sXianqiSlotMgr->LoadConfig();

        if (reload && _initialized && sXianqiSlotMgr->IsEnabled())
        {
            sXianqiSlotMgr->LoadSlotConfigs();
            sXianqiSlotMgr->LoadArtifactItems();
            LOG_INFO("module", "仙器系统: 配置已重新加载");
        }
    }

    void OnUpdate(uint32 diff) override
    {
        if (_initialized)
            return;

        _loadTimer += diff;
        if (_loadTimer >= 1000)
        {
            if (sXianqiSlotMgr->IsEnabled())
                sXianqiSlotMgr->Initialize();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTimer = 0;
};

class XianqiPlayerScript : public PlayerScript
{
public:
    XianqiPlayerScript() : PlayerScript("XianqiPlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_CAN_EQUIP_ITEM,
        PLAYERHOOK_CAN_CAST_ITEM_COMBAT_SPELL
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sXianqiSlotMgr->IsEnabled() || !player)
            return;

        sXianqiSlotMgr->LoadPlayerData(player);
        sXianqiSlotMgr->ValidateEquippedItems(player);
        sXianqiSlotMgr->ApplyAllEffects(player);
        sXianqiSlotMgr->SendXianqiDataToClient(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!sXianqiSlotMgr->IsEnabled() || !player)
            return;

        // 登出不回滚属性：玩家对象即将析构，回滚是纯浪费且实测有 2s 级卡顿（见飞升系统同款优化）
        sXianqiSlotMgr->SaveXianqiItems(player);
        sXianqiSlotMgr->ClearPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        if (!sXianqiSlotMgr->IsEnabled())
            return;

        sXianqiSlotMgr->DeletePlayerData(guid.GetCounter());
    }

    bool OnPlayerCanEquipItem(Player* player, uint8 /*slot*/, uint16& /*dest*/, Item* pItem, bool /*swap*/, bool not_loading) override
    {
        if (!sXianqiSlotMgr->IsEnabled() || !player || !pItem || !not_loading)
            return true;

#ifdef MODULE_WEAR_CONTROL
        if (WearControl::HasExclusiveLimit(pItem->GetEntry()))
        {
            if (player->GetSession())
            {
                if (WearControl::IsLimitedTo(pItem->GetEntry(), WEAR_LIMIT_XIANQI))
                    ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[仙器系统]|r 仙器系统装备只能放入仙器/扩展槽位，不能装备到官方装备栏。");
                else
                    ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[穿戴控制]|r 该物品只能放入对应专属系统槽位，不能装备到官方装备栏。");
            }

            return false;
        }
#endif

        if (!sXianqiSlotMgr->IsArtifactItem(pItem->GetEntry()))
            return true;

        if (player->GetSession())
            ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[仙器系统]|r 仙器只能放入仙器槽位，不能装备到官方装备栏。");

        return false;
    }

    bool OnPlayerCanCastItemCombatSpell(Player* player, Unit* target, WeaponAttackType attType, uint32 procVictim, uint32 procEx, Item* item, ItemTemplate const* proto) override
    {
        if (!sXianqiSlotMgr->IsEnabled() || !player || !target)
            return true;

        // 核心用空 item/proto 通知“本次命中开始”；真实装备逐件触发时直接放行。
        if (item || proto)
            return true;

        static thread_local bool applyingXianqiCombatSpells = false;
        if (applyingXianqiCombatSpells)
            return true;

        applyingXianqiCombatSpells = true;

        PlayerXianqiStatus* status = sXianqiSlotMgr->GetPlayerStatus(player->GetGUID().GetCounter());
        if (status && !status->slots.empty())
        {
            for (auto const& slotPair : status->slots)
            {
                uint8 xianqiSlot = slotPair.first;
                XianqiSlotConfig const* config = sXianqiSlotMgr->GetSlotConfig(xianqiSlot);
                if (!config)
                    continue;

                Item* xianqiItem = slotPair.second.itemPtr;
                if (!xianqiItem || xianqiItem->IsBroken())
                    continue;

                ItemTemplate const* xianqiProto = xianqiItem->GetTemplate();
                if (!xianqiProto)
                    continue;

                if (!CanXianqiItemProcForAttack(config, xianqiProto, attType))
                    continue;

                player->CastItemCombatSpell(target, attType, procVictim, procEx, xianqiItem, xianqiProto);
            }
        }

        applyingXianqiCombatSpells = false;
        return true;
    }
};

class XianqiItemScript : public AllItemScript
{
public:
    XianqiItemScript() : AllItemScript("XianqiItemScript") { }

    bool CanItemRemove(Player* player, Item* item) override
    {
        if (!sXianqiSlotMgr->IsEnabled() || !player || !item)
            return true;

        uint32 itemGuid = item->GetGUID().GetCounter();

        if (sXianqiSlotMgr->IsItemEquippedInXianqi(player, itemGuid))
            return false;

        // 内存中存在该玩家的仙器状态时以内存为准，直接放行。
        // 此前任意物品删除（吃食物/卖灰装/消耗品）都会落到下面的同步查库，全服高频。
        if (sXianqiSlotMgr->GetPlayerStatus(player->GetGUID().GetCounter()))
            return true;

        // 仅在状态已清理但核心仍在保存的窗口期才回退查库兜底
        // 仙器装备实际落库于 character_inventory 虚拟背包，故以此为准
        if (QueryResult result = CharacterDatabase.Query(
            "SELECT 1 FROM character_inventory WHERE guid = {} AND bag = {} AND item = {}",
            player->GetGUID().GetCounter(), XIANQI_VIRTUAL_BAG, itemGuid))
            return false;

        return true;
    }
};

class XianqiDamageTriggeredItemProcScript : public UnitScript
{
public:
    XianqiDamageTriggeredItemProcScript() : UnitScript("XianqiDamageTriggeredItemProcScript", true,
    {
        UNITHOOK_ON_DAMAGE
    }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint256& damage) override
    {
        TriggerDamageProcs(victim, attacker, damage);
    }

private:
    static void TriggerDamageProcs(Unit* target, Unit* attacker, uint256& damage)
    {
        if (!sXianqiSlotMgr->IsEnabled() || !target || !attacker)
            return;

        if (Player::IsTriggeringDamageTriggeredArtifactItemProcSpell())
            return;

        static thread_local bool triggeringXianqiDamageItemProcs = false;
        if (triggeringXianqiDamageItemProcs)
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player || player == target)
            return;

        triggeringXianqiDamageItemProcs = true;
        sXianqiSlotMgr->CastDamageTriggeredItemCombatSpells(player, target, PROC_FLAG_TAKEN_DAMAGE, PROC_EX_NORMAL_HIT, false, false, true);
        triggeringXianqiDamageItemProcs = false;
    }
};

class XianqiCommandScript : public CommandScript
{
public:
    XianqiCommandScript() : CommandScript("XianqiCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subTable =
        {
            { "查看", HandleViewCommand,    SEC_PLAYER,        Console::No },
            { "装备", HandleEquipCommand,   SEC_PLAYER,        Console::No },
            { "卸下", HandleUnequipCommand, SEC_PLAYER,        Console::No },
            { "使用", HandleUseCommand,     SEC_PLAYER,        Console::No },
            { "清空", HandleClearCommand,   SEC_PLAYER,        Console::No },
            { "刷新", HandleRefreshCommand, SEC_PLAYER,        Console::No },
            { "解锁", HandleUnlockCommand,  SEC_PLAYER,        Console::No },
            { "重载", HandleReloadCommand,  SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable rootTable =
        {
            { "仙器", subTable },
            { "xianqi", subTable },
        };

        return rootTable;
    }

private:
    static bool CheckEnabled(ChatHandler* handler)
    {
        if (sXianqiSlotMgr->IsEnabled())
            return true;

        handler->SendSysMessage("仙器系统已禁用。");
        return false;
    }

    static Player* GetPlayer(ChatHandler* handler)
    {
        return handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    }

    static bool HandleViewCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sXianqiSlotMgr->SendXianqiDataToClient(player);
        return true;
    }

    static bool HandleEquipCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .仙器 装备 <槽位1-29> <背包0-4> <背包槽位从1> [物品GUID]");
            handler->SendSysMessage("槽位: 1-10=仙器槽, 11-29=扩展槽(头/颈/肩/衬衣/胸/腰/腿/脚/腕/手/戒指1/戒指2/饰品1/饰品2/披风/主手/副手/远程/战袍)");
            return true;
        }

        int slotInt = 0;
        int bagIdInt = 0;
        int bagSlotInt = 0;
        uint32 requestedItemGuid = 0;

        std::istringstream iss(args);
        if (!(iss >> slotInt >> bagIdInt >> bagSlotInt))
        {
            handler->SendSysMessage("参数错误。用法: .仙器 装备 <槽位> <背包ID> <背包槽位> [物品GUID]");
            return true;
        }
        iss >> requestedItemGuid;

        if (slotInt < XIANQI_SLOT_FIRST || slotInt > XIANQI_SLOT_LAST)
        {
            handler->PSendSysMessage("无效槽位，有效范围: {}-{}", XIANQI_SLOT_FIRST, XIANQI_SLOT_LAST);
            return true;
        }
        if (bagIdInt < 0 || bagIdInt > 4)
        {
            handler->SendSysMessage("无效背包ID，有效范围: 0-4 (0=主背包, 1-4=额外背包)");
            return true;
        }
        if (bagSlotInt <= 0)
        {
            handler->SendSysMessage("无效背包槽位，槽位从1开始");
            return true;
        }

        // 客户端 bag=0 主背包 / 1-4 额外背包；slot 从 1 开始
        Item* item = nullptr;
        if (bagIdInt == 0)
        {
            uint8 serverSlot = INVENTORY_SLOT_ITEM_START + static_cast<uint8>(bagSlotInt) - 1;
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
        }
        else if (Bag* bag = player->GetBagByPos(INVENTORY_SLOT_BAG_START + static_cast<uint8>(bagIdInt) - 1))
            item = bag->GetItemByPos(static_cast<uint8>(bagSlotInt) - 1);

        if (!item)
        {
            handler->SendSysMessage("背包中未找到该物品。");
            return true;
        }

        // 客户端位置与 GUID 不一致时按 GUID 纠偏
        if (requestedItemGuid && item->GetGUID().GetCounter() != requestedItemGuid)
        {
            Item* requestedItem = sXianqiSlotMgr->FindItemInBags(player, requestedItemGuid);
            if (!requestedItem)
            {
                handler->PSendSysMessage("背包中未找到指定GUID的物品: {}", requestedItemGuid);
                return true;
            }
            item = requestedItem;
        }

        sXianqiSlotMgr->EquipItem(player, static_cast<uint8>(slotInt), item->GetEntry(), item->GetGUID().GetCounter());
        return true;
    }

    static bool HandleUnequipCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .仙器 卸下 <槽位1-29>");
            return true;
        }

        int slotInt = atoi(args);
        if (slotInt < XIANQI_SLOT_FIRST || slotInt > XIANQI_SLOT_LAST)
        {
            handler->PSendSysMessage("无效槽位，有效范围: {}-{}", XIANQI_SLOT_FIRST, XIANQI_SLOT_LAST);
            return true;
        }

        sXianqiSlotMgr->UnequipItem(player, static_cast<uint8>(slotInt));
        return true;
    }

    static bool HandleUseCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .仙器 使用 <槽位1-29>");
            return true;
        }

        int slotInt = atoi(args);
        if (slotInt < XIANQI_SLOT_FIRST || slotInt > XIANQI_SLOT_LAST)
        {
            handler->PSendSysMessage("无效槽位，有效范围: {}-{}", XIANQI_SLOT_FIRST, XIANQI_SLOT_LAST);
            return true;
        }

        PlayerXianqiStatus* status = sXianqiSlotMgr->GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return true;

        auto slotItr = status->slots.find(static_cast<uint8>(slotInt));
        if (slotItr == status->slots.end() || !slotItr->second.itemPtr)
        {
            handler->SendSysMessage("该槽位没有可使用的仙器装备。");
            return true;
        }

        Item* item = slotItr->second.itemPtr;
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
        {
            handler->SendSysMessage("仙器装备模板不存在，无法使用。");
            return true;
        }

        if (item->IsBroken())
        {
            handler->SendSysMessage("该仙器装备已损坏，无法使用。");
            return true;
        }

        if (!HasXianqiItemUseSpell(item, proto))
        {
            handler->SendSysMessage("该仙器装备没有使用触发效果。");
            return true;
        }

        if (!player->IsAlive())
        {
            handler->SendSysMessage("死亡状态不能使用仙器装备。");
            return true;
        }

        if ((proto->Bonding == BIND_WHEN_USE || proto->Bonding == BIND_WHEN_PICKED_UP || proto->Bonding == BIND_QUEST_ITEM) && !item->IsSoulBound())
        {
            item->SetState(ITEM_CHANGED, player);
            item->SetBinding(true);
        }

        Unit* target = player->GetSelectedUnit();
        if (!target)
            target = player;

        SpellCastTargets targets;
        targets.SetUnitTarget(target);
        targets.SetSrc(*player);
        targets.SetDst(*target);

        player->CastItemUseSpell(item, targets, 1, 0);
        return true;
    }

    static bool HandleClearCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sXianqiSlotMgr->UnequipAllItems(player);
        return true;
    }

    static bool HandleRefreshCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sXianqiSlotMgr->RefreshEffects(player);
        handler->SendSysMessage("|cff66ffcc[仙器系统]|r 仙器装备属性已刷新。");
        return true;
    }

    static bool HandleUnlockCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .仙器 解锁 <槽位1-29>");
            return true;
        }

        int slotInt = atoi(args);
        if (slotInt < XIANQI_SLOT_FIRST || slotInt > XIANQI_SLOT_LAST)
        {
            handler->PSendSysMessage("无效槽位，有效范围: {}-{}", XIANQI_SLOT_FIRST, XIANQI_SLOT_LAST);
            return true;
        }

        sXianqiSlotMgr->UnlockSlot(player, static_cast<uint8>(slotInt));
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sXianqiSlotMgr->LoadSlotConfigs();
        sXianqiSlotMgr->LoadArtifactItems();
        handler->SendSysMessage("仙器系统配置已重新加载。");
        return true;
    }
};

} // namespace

namespace XianmenArtifactSlots
{
bool IsRegisteredArtifactItem(uint32 itemId)
{
    return sXianqiSlotMgr->IsEnabled() && sXianqiSlotMgr->IsArtifactItem(itemId);
}

bool HasEquippedArtifactFeatureSpell(Player* player, uint32 spellId)
{
    if (!player || !sXianqiSlotMgr->IsEnabled() || !IsXianqiFeatureSpellId(spellId))
        return false;

    PlayerXianqiStatus* status = sXianqiSlotMgr->GetPlayerStatus(player->GetGUID().GetCounter());
    if (!status || status->slots.empty())
        return false;

    for (auto const& slotPair : status->slots)
    {
        Item* item = slotPair.second.itemPtr;
        if (!item || item->IsBroken())
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            continue;

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            _Spell const& spellData = proto->Spells[i];
            if (spellData.SpellId == static_cast<int32>(spellId)
                && spellData.SpellTrigger == ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
                return true;
        }
    }

    return false;
}

void ForEachEquippedWeaponItem(Player* player, std::function<void(Item*)> const& visitor)
{
    if (!player || !visitor || !sXianqiSlotMgr->IsEnabled())
        return;

    PlayerXianqiStatus* status = sXianqiSlotMgr->GetPlayerStatus(player->GetGUID().GetCounter());
    if (!status || status->slots.empty())
        return;

    for (auto const& slotPair : status->slots)
    {
        XianqiSlotConfig const* config = sXianqiSlotMgr->GetSlotConfig(slotPair.first);
        if (!config)
            continue;

        if (config->bodyPart != XIANQI_PART_MAINHAND
            && config->bodyPart != XIANQI_PART_OFFHAND
            && config->bodyPart != XIANQI_PART_RANGED)
            continue;

        Item* item = slotPair.second.itemPtr;
        if (!item || item->IsBroken())
            continue;

        visitor(item);
    }
}
}

void AddSC_xianmen_artifact_slots()
{
    new XianqiWorldScript();
    new XianqiPlayerScript();
    new XianqiItemScript();
    new XianqiDamageTriggeredItemProcScript();
    new XianqiCommandScript();
}

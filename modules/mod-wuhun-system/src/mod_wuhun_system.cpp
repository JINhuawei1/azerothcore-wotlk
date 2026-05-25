/*
 * 武魂系统 - 可培养战斗分身
 */

#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "Player.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "ScriptedCreature.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Bag.h"
#include "Mail.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "MotionMaster.h"
#include "WorldPacket.h"
#include "ModuleManager.h"
#include "RequirementInterface.h"
#include "StringConvert.h"
#include "Util.h"

#if __has_include("ItemAttributesDBHelper.h")
#define WUHUN_HAS_ITEM_ATTRIBUTES 1
#include "ItemAttributesDBHelper.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr char const* WUHUN_ADDON_PREFIX = "WUHUN_SYS";
constexpr char const* WUHUN_PANEL_ADDON_PREFIX = "PATTRPANEL";
constexpr size_t WUHUN_MAX_ADDON_PAYLOAD = 220;
constexpr uint32 WUHUN_VIRTUAL_BAG = 201;
constexpr uint8 WUHUN_RING_COUNT = 9;
constexpr uint8 WUHUN_SKILL_SLOT_COUNT = 9;
constexpr uint8 WUHUN_SKILL_MODE_SYNC = 1;
constexpr uint8 WUHUN_SKILL_MODE_AUTO = 2;
constexpr uint64 WUHUN_CLIENT_VISIBLE_HEALTH_LIMIT = 2147483520ULL;
constexpr uint32 WUHUN_RANGED_MIRROR_MELEE_DELAY_MS = 2000;
constexpr uint32 WUHUN_SKILL_SCAN_INTERVAL_MS = 100;
constexpr uint32 WUHUN_MIN_SKILL_COOLDOWN_MS = 100;
constexpr long double WUHUN_DIRECT_SKILL_POWER_COEFFICIENT = 1.0L;
constexpr long double WUHUN_PERIODIC_SKILL_POWER_COEFFICIENT = 0.35L;
constexpr TriggerCastFlags WUHUN_SPELL_CAST_FLAGS = TriggerCastFlags(
    TRIGGERED_FULL_MASK |
    TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD |
    TRIGGERED_IGNORE_POWER_AND_REAGENT_COST |
    TRIGGERED_IGNORE_CAST_IN_PROGRESS |
    TRIGGERED_CAST_DIRECTLY |
    TRIGGERED_IGNORE_EQUIPPED_ITEM_REQUIREMENT);
constexpr float WUHUN_FOLLOW_ANGLE = 1.57079632679f;

RequirementInterface* GetRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    if (!mgr)
        return nullptr;

    return mgr->GetRequirementModule();
}

enum WuhunTemplateType : uint8
{
    WUHUN_TEMPLATE_SYSTEM = 1,
    WUHUN_TEMPLATE_DEFINITION = 2,
    WUHUN_TEMPLATE_RING_LEVEL = 3,
    WUHUN_TEMPLATE_SKILL = 4,
    WUHUN_TEMPLATE_AI = 5,
    WUHUN_TEMPLATE_EQUIP_SLOT = 6
};

struct WuhunDefinition
{
    uint32 id = 1;
    uint32 creatureEntry = 930001;
    std::string name = "本命武魂";
    float scale = 1.0f;
    uint128 activationSoulCost = 1000;
    uint128 evolveSoulCost = 0;
    uint128 dungeonBossSoulPower = 1;
    uint128 worldBossSoulPower = 1;
};

struct WuhunSkillTemplate
{
    uint32 skillId = 0;
    uint32 spellId = 0;
    std::string name;
    std::string description;
    uint32 cooldownMs = 5000;
    uint128 learnCost = 100;
    uint128 upgradeCost = 100;
    uint32 maxLevel = 10;
    uint8 targetType = 0;          // 0=敌方目标 1=武魂自身 2=主人
    uint8 minTargetHealthPct = 0;  // 0=不限制
    float range = 30.0f;
    float damageBonusPerLevel = 1.0f;
};

struct WuhunEquipmentSlot
{
    uint32 itemEntry = 0;
    ObjectGuid::LowType itemGuid = 0;
    Item* item = nullptr;
};

struct WuhunEquipSlotConfig
{
    uint8 slot = 0;
    std::string name;
    uint128 soulCost = 0;
    uint32 requirementId = 0;
};

struct PlayerWuhunData
{
    uint32 wuhunId = 1;
    bool activated = false;
    bool summonRequested = false;
    uint128 soulPower = 0;
    std::array<uint32, WUHUN_RING_COUNT> ringLevels = { };
    std::array<uint32, WUHUN_SKILL_SLOT_COUNT> skillSlots = { };
    std::array<bool, EQUIPMENT_SLOT_END> unlockedEquipSlots = { };
    std::array<uint32, EQUIPMENT_SLOT_END> equipSlotUnlockTimes = { };
    uint8 skillMode = WUHUN_SKILL_MODE_SYNC;
    std::unordered_map<uint32, uint32> skillLevels;
    std::map<uint8, WuhunEquipmentSlot> equipment;
    ObjectGuid avatarGuid;
    ObjectGuid targetGuid;
    ObjectGuid rangedMirrorTargetGuid;
    uint32 targetHoldTimer = 0;
    uint32 rangedMirrorHoldTimer = 0;
    uint32 syncTimer = 0;
    uint32 statRefreshTimer = 0;
    uint32 uiStateTimer = 0;
    double avatarAttackPower = 0.0;
    double avatarRangedAttackPower = 0.0;
    double avatarSpellPower = 0.0;
};

struct WuhunSkillRuntime
{
    uint32 skillId = 0;
    uint32 level = 0;
    uint8 slot = 0;
};

struct WuhunEquipmentBonus
{
    std::array<int128, MAX_STATS> stats = { };
    uint128 health = 0;
    uint128 mana = 0;
    uint128 armor = 0;
    double attackPower = 0.0;
    double rangedAttackPower = 0.0;
    double spellPower = 0.0;
    double minDamage = 0.0;
    double maxDamage = 0.0;
    int64 meleeHasteRating = 0;
    int64 rangedHasteRating = 0;
    int64 spellHasteRating = 0;
    uint32 itemCount = 0;
    uint32 enhancedItemCount = 0;
};

struct WuhunItemAttributeMultiplier
{
    float value = 1.0f;
    char mode = 'x';
    bool active = false;
};

uint32 ToWuhunClientHealth(uint128 const& value)
{
    return value > WUHUN_CLIENT_VISIBLE_HEALTH_LIMIT ? static_cast<uint32>(WUHUN_CLIENT_VISIBLE_HEALTH_LIMIT) : Acore::Number::ToUInt32Saturated(value);
}

uint128 ScaleUInt128(uint128 const& value, float percent)
{
    if (value == 0 || percent <= 0.0f)
        return 0;

    return Acore::Number::ToUInt128Saturated(Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / 100.0L);
}

int64 SaturatingAddInt64(int64 left, int64 right)
{
    if (right > 0 && left > std::numeric_limits<int64>::max() - right)
        return std::numeric_limits<int64>::max();
    if (right < 0 && left < std::numeric_limits<int64>::min() - right)
        return std::numeric_limits<int64>::min();

    return left + right;
}

int128 SaturatingAddInt128(int128 const& left, int128 const& right)
{
    if (right > 0 && left > std::numeric_limits<int128>::max() - right)
        return std::numeric_limits<int128>::max();
    if (right < 0 && left < std::numeric_limits<int128>::min() - right)
        return std::numeric_limits<int128>::min();

    return left + right;
}

uint128 SaturatingAddUInt128(uint128 const& left, uint128 const& right)
{
    if (right > std::numeric_limits<uint128>::max() - left)
        return std::numeric_limits<uint128>::max();

    return left + right;
}

uint128 SaturatingMultiplyUInt128(uint128 const& value, uint32 multiplier)
{
    if (value == 0 || multiplier == 0)
        return 0;
    if (value > std::numeric_limits<uint128>::max() / multiplier)
        return std::numeric_limits<uint128>::max();

    return value * multiplier;
}

uint128 AbsInt128ToUInt128(int128 const& value)
{
    if (value >= 0)
        return static_cast<uint128>(value);

    if (value == std::numeric_limits<int128>::min())
        return static_cast<uint128>(std::numeric_limits<int128>::max()) + 1;

    return static_cast<uint128>(-value);
}

uint128 DefaultRingSoulCost(uint32 level)
{
    return static_cast<uint128>(level) * 100;
}

uint32 ToUInt32FromPositiveInt128(int128 const& value)
{
    if (value <= 0)
        return 0;

    return Acore::Number::ToUInt32Saturated(static_cast<uint128>(value));
}

int128 ScaleInt128(int128 const& value, float percent)
{
    if (value <= 0 || percent <= 0.0f)
        return 0;

    return Acore::Number::ToInt128Saturated(Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / 100.0L);
}

uint128 AddDamageSaturated(uint128 const& damage, long double bonus)
{
    if (bonus <= 0.0L || !std::isfinite(static_cast<double>(bonus)))
        return damage;

    uint128 flatBonus = Acore::Number::ToUInt128Saturated(bonus);
    if (!flatBonus && bonus > 0.0L)
        flatBonus = 1;

    return SaturatingAddUInt128(damage, flatBonus);
}

char NormalizeWuhunItemAttributeMode(char mode)
{
    if (mode == '+' || mode == 1)
        return '+';
    if (mode == '-' || mode == 'n')
        return '-';
    return 'x';
}

char DbValueToWuhunItemAttributeMode(int32 value)
{
    if (value == 1)
        return '+';
    if (value == -1)
        return '-';
    return 'x';
}

bool HasWuhunItemAttributeMultiplierEffect(float value, char mode)
{
    mode = NormalizeWuhunItemAttributeMode(mode);
    if (mode == '-')
        return false;

    return mode == '+' ? value > 0.0f : value > 1.0f;
}

int128 CalculateWuhunEnhancedAttributeValue(int128 const& originalValue, WuhunItemAttributeMultiplier const& multiplier)
{
    if (!multiplier.active)
        return originalValue;

    char mode = NormalizeWuhunItemAttributeMode(multiplier.mode);
    if (mode == '-')
        return originalValue;

    long double enhanced = Acore::Number::ToLongDouble(originalValue);
    if (mode == '+')
        enhanced += static_cast<long double>(multiplier.value);
    else
        enhanced *= static_cast<long double>(multiplier.value);

    return Acore::Number::ToInt128Saturated(enhanced);
}

uint128 CalculateWuhunEnhancedAttributeValue(uint128 const& originalValue, WuhunItemAttributeMultiplier const& multiplier)
{
    if (!multiplier.active)
        return originalValue;

    char mode = NormalizeWuhunItemAttributeMode(multiplier.mode);
    if (mode == '-')
        return originalValue;

    long double enhanced = Acore::Number::ToLongDouble(originalValue);
    if (mode == '+')
        enhanced += static_cast<long double>(multiplier.value);
    else
        enhanced *= static_cast<long double>(multiplier.value);

    return Acore::Number::ToUInt128Saturated(enhanced);
}

float CalculateWuhunEnhancedAttributeValue(float originalValue, WuhunItemAttributeMultiplier const& multiplier)
{
    if (!multiplier.active)
        return originalValue;

    char mode = NormalizeWuhunItemAttributeMode(multiplier.mode);
    if (mode == '-')
        return originalValue;
    if (mode == '+')
        return originalValue + multiplier.value;

    return originalValue * multiplier.value;
}

bool TryParseUInt(std::string const& text, uint32& value)
{
    if (text.empty())
        return false;

    try
    {
        size_t read = 0;
        unsigned long parsed = std::stoul(text, &read, 10);
        if (read != text.size() || parsed > std::numeric_limits<uint32>::max())
            return false;
        value = static_cast<uint32>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool TryParseUInt128(std::string const& text, uint128& value)
{
    if (text.empty())
        return false;

    if (Optional<uint128> parsed = Acore::StringTo<uint128>(text))
    {
        value = *parsed;
        return true;
    }

    return false;
}

bool TryParseInt128(std::string const& text, int128& value)
{
    if (text.empty())
        return false;

    if (Optional<int128> parsed = Acore::StringTo<int128>(text))
    {
        value = *parsed;
        return true;
    }

    return false;
}

std::vector<std::string> Split(std::string const& text, char delimiter)
{
    std::vector<std::string> out;
    std::string current;
    std::istringstream stream(text);
    while (std::getline(stream, current, delimiter))
        out.push_back(current);
    return out;
}

bool NormalizeUiSlot(uint32 rawSlot, uint8 maxSlots, uint8& slot)
{
    if (rawSlot == 0)
    {
        slot = 0;
        return true;
    }

    if (rawSlot >= 1 && rawSlot <= maxSlots)
    {
        slot = static_cast<uint8>(rawSlot - 1);
        return true;
    }

    return false;
}

uint8 NormalizeSkillMode(uint32 rawMode)
{
    return rawMode == WUHUN_SKILL_MODE_AUTO ? WUHUN_SKILL_MODE_AUTO : WUHUN_SKILL_MODE_SYNC;
}

std::string GetWuhunEquipmentSlotName(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: return "头部";
        case EQUIPMENT_SLOT_NECK: return "项链";
        case EQUIPMENT_SLOT_SHOULDERS: return "肩部";
        case EQUIPMENT_SLOT_BODY: return "衬衣";
        case EQUIPMENT_SLOT_CHEST: return "胸甲";
        case EQUIPMENT_SLOT_WAIST: return "腰带";
        case EQUIPMENT_SLOT_LEGS: return "腿部";
        case EQUIPMENT_SLOT_FEET: return "脚部";
        case EQUIPMENT_SLOT_WRISTS: return "护腕";
        case EQUIPMENT_SLOT_HANDS: return "手套";
        case EQUIPMENT_SLOT_FINGER1: return "戒指1";
        case EQUIPMENT_SLOT_FINGER2: return "戒指2";
        case EQUIPMENT_SLOT_TRINKET1: return "饰品1";
        case EQUIPMENT_SLOT_TRINKET2: return "饰品2";
        case EQUIPMENT_SLOT_BACK: return "披风";
        case EQUIPMENT_SLOT_MAINHAND: return "主手";
        case EQUIPMENT_SLOT_OFFHAND: return "副手";
        case EQUIPMENT_SLOT_RANGED: return "远程";
        case EQUIPMENT_SLOT_TABARD: return "战袍";
        default: return "装备槽";
    }
}

void AppendReplaceItemInstance(CharacterDatabaseTransaction trans, Item* item, ObjectGuid::LowType ownerGuid)
{
    if (!trans || !item)
        return;

    uint8 index = 0;
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_INSTANCE);
    stmt->SetData(index, item->GetEntry());
    stmt->SetData(++index, ownerGuid);
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
    stmt->SetData(++index, item->GetGUID().GetCounter());
    trans->Append(stmt);

    item->FSetState(ITEM_UNCHANGED);
}

bool IsValidWuhunEquipmentSlot(ItemTemplate const* proto, uint8 slot)
{
    if (!proto || slot >= EQUIPMENT_SLOT_END)
        return false;

    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD:
            return proto->InventoryType == INVTYPE_HEAD;
        case EQUIPMENT_SLOT_NECK:
            return proto->InventoryType == INVTYPE_NECK;
        case EQUIPMENT_SLOT_SHOULDERS:
            return proto->InventoryType == INVTYPE_SHOULDERS;
        case EQUIPMENT_SLOT_BODY:
            return proto->InventoryType == INVTYPE_BODY;
        case EQUIPMENT_SLOT_CHEST:
            return proto->InventoryType == INVTYPE_CHEST || proto->InventoryType == INVTYPE_ROBE;
        case EQUIPMENT_SLOT_WAIST:
            return proto->InventoryType == INVTYPE_WAIST;
        case EQUIPMENT_SLOT_LEGS:
            return proto->InventoryType == INVTYPE_LEGS;
        case EQUIPMENT_SLOT_FEET:
            return proto->InventoryType == INVTYPE_FEET;
        case EQUIPMENT_SLOT_WRISTS:
            return proto->InventoryType == INVTYPE_WRISTS;
        case EQUIPMENT_SLOT_HANDS:
            return proto->InventoryType == INVTYPE_HANDS;
        case EQUIPMENT_SLOT_FINGER1:
        case EQUIPMENT_SLOT_FINGER2:
            return proto->InventoryType == INVTYPE_FINGER;
        case EQUIPMENT_SLOT_TRINKET1:
        case EQUIPMENT_SLOT_TRINKET2:
            return proto->InventoryType == INVTYPE_TRINKET;
        case EQUIPMENT_SLOT_BACK:
            return proto->InventoryType == INVTYPE_CLOAK;
        case EQUIPMENT_SLOT_MAINHAND:
            return proto->InventoryType == INVTYPE_WEAPON || proto->InventoryType == INVTYPE_2HWEAPON || proto->InventoryType == INVTYPE_WEAPONMAINHAND;
        case EQUIPMENT_SLOT_OFFHAND:
            return proto->InventoryType == INVTYPE_WEAPON || proto->InventoryType == INVTYPE_WEAPONOFFHAND || proto->InventoryType == INVTYPE_SHIELD || proto->InventoryType == INVTYPE_HOLDABLE;
        case EQUIPMENT_SLOT_RANGED:
            return proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_RANGEDRIGHT || proto->InventoryType == INVTYPE_THROWN || proto->InventoryType == INVTYPE_RELIC;
        case EQUIPMENT_SLOT_TABARD:
            return proto->InventoryType == INVTYPE_TABARD;
        default:
            return false;
    }
}

bool IsWuhunBoss(Creature* creature)
{
    if (!creature)
        return false;

    CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
    return creature->IsDungeonBoss()
        || creature->isWorldBoss()
        || (creatureTemplate && creatureTemplate->rank == CREATURE_ELITE_WORLDBOSS);
}

uint32 CalculateWuhunAttackTime(uint32 baseAttackTime, float hasteBonusPct)
{
    if (!baseAttackTime)
        baseAttackTime = BASE_ATTACK_TIME;

    float speedPct = std::clamp(100.0f - hasteBonusPct, 5.0f, 500.0f);
    uint32 attackTime = static_cast<uint32>(std::round(static_cast<float>(baseAttackTime) * speedPct / 100.0f));
    return std::max<uint32>(100, attackTime);
}
}

class WuhunSystemMgr
{
public:
    static WuhunSystemMgr* Instance()
    {
        static WuhunSystemMgr instance;
        return &instance;
    }

    bool IsEnabled() const
    {
        return sConfigMgr->GetOption<bool>("WuhunSystem.Enable", true);
    }

    bool IsDebug() const
    {
        return sConfigMgr->GetOption<bool>("WuhunSystem.Debug", false);
    }

    bool IsAvatarSkillAiEnabled() const
    {
        return GetUIntParam("AVATAR_SKILL_AI", 1) != 0;
    }

    float GetFollowDistance() const
    {
        return GetFloatParam("FOLLOW_DISTANCE", 2.5f);
    }

    float GetRecallDistance() const
    {
        return GetFloatParam("RECALL_DISTANCE", 55.0f);
    }

    void LoadTemplates()
    {
        _definitions.clear();
        _skills.clear();
        _ringCosts.clear();
        _ringInheritBonuses.clear();
        _equipSlotConfigs.clear();
        _intParams.clear();
        _uint128Params.clear();
        _floatParams.clear();

        SeedBuiltInDefaults();

        bool loadedNewTables = LoadTemplateDefinitions();
        loadedNewTables = LoadRingTemplates() || loadedNewTables;
        loadedNewTables = LoadSkillTemplates() || loadedNewTables;
        loadedNewTables = LoadEquipSlotTemplates() || loadedNewTables;

        if (!loadedNewTables && !LoadLegacyTemplateRows())
        {
            LOG_WARN("server.loading", "武魂系统: 未读取到 `_武魂系统_*` 配置表或旧 `_武魂模板` 数据，使用内置默认配置");
            return;
        }

        if (IsDebug())
            LOG_INFO("server.loading", "武魂系统: 已加载 {} 个武魂定义、{} 个魂环等级、{} 个技能模板、{} 个装备槽配置", _definitions.size(), _ringCosts.size(), _skills.size(), _equipSlotConfigs.size());
    }

private:
    bool LoadLegacyTemplateRows()
    {
        _definitions.clear();
        _skills.clear();
        _ringCosts.clear();
        _ringInheritBonuses.clear();
        _equipSlotConfigs.clear();
        _intParams.clear();
        _floatParams.clear();

        SeedBuiltInDefaults();

        if (!HasWorldTable("_武魂模板"))
            return false;

        QueryResult result = WorldDatabase.Query(
            "SELECT `配置类型`, `配置键`, `子键`, `名称`, `数值1`, `数值2`, `数值3`, `数值4`, `数值5`, `数值6`, "
            "`浮点1`, `浮点2`, `文本1`, `文本2` FROM `_武魂模板` WHERE `启用` = 1 ORDER BY `ID`");

        if (!result)
            return false;

        do
        {
            Field* fields = result->Fetch();
            uint8 type = fields[0].Get<uint8>();
            std::string key = fields[1].Get<std::string>();
            std::string name = fields[3].Get<std::string>();
            int128 value1 = fields[4].Get<int128>();
            int128 value2 = fields[5].Get<int128>();
            int128 value3 = fields[6].Get<int128>();
            int128 value4 = fields[7].Get<int128>();
            int128 value5 = fields[8].Get<int128>();
            int128 value6 = fields[9].Get<int128>();
            float float1 = fields[10].Get<float>();
            float float2 = fields[11].Get<float>();
            std::string text1 = fields[12].Get<std::string>();
            std::string text2 = fields[13].Get<std::string>();

            switch (type)
            {
                case WUHUN_TEMPLATE_SYSTEM:
                case WUHUN_TEMPLATE_AI:
                    _intParams[key] = Acore::Number::ToInt64Saturated(value1);
                    _uint128Params[key] = Acore::Number::ToUInt128Saturated(value1);
                    _floatParams[key] = float1;
                    break;
                case WUHUN_TEMPLATE_DEFINITION:
                {
                    uint32 id = 0;
                    if (!TryParseUInt(key, id) || !id)
                        id = value1 > 0 ? ToUInt32FromPositiveInt128(value1) : 1;

                    WuhunDefinition def;
                    def.id = id;
                    def.creatureEntry = value1 > 0 ? ToUInt32FromPositiveInt128(value1) : GetUIntParam("AVATAR_ENTRY", 930001);
                    def.scale = float1 > 0.0f ? float1 : 1.0f;
                    def.name = !name.empty() ? name : (!text1.empty() ? text1 : "本命武魂");
                    _definitions[id] = def;
                    break;
                }
                case WUHUN_TEMPLATE_RING_LEVEL:
                {
                    uint32 level = 0;
                    if (TryParseUInt(key, level) && level > 0)
                    {
                        _ringCosts[level] = value1 > 0 ? Acore::Number::ToUInt128Saturated(value1) : DefaultRingSoulCost(level);
                        _ringInheritBonuses[level] = float1 > 0.0f ? float1 : static_cast<float>(level);
                    }
                    break;
                }
                case WUHUN_TEMPLATE_SKILL:
                {
                    uint32 skillId = 0;
                    if (!TryParseUInt(key, skillId) || !skillId)
                        break;

                    WuhunSkillTemplate skill;
                    skill.skillId = skillId;
                    skill.name = !name.empty() ? name : ("武魂技能" + std::to_string(skillId));
                    skill.spellId = value1 > 0 ? ToUInt32FromPositiveInt128(value1) : 0;
                    skill.cooldownMs = value2 > 0 ? ToUInt32FromPositiveInt128(value2) : 5000;
                    skill.learnCost = value3 > 0 ? Acore::Number::ToUInt128Saturated(value3) : 100;
                    skill.upgradeCost = value4 > 0 ? Acore::Number::ToUInt128Saturated(value4) : 100;
                    skill.maxLevel = GetUIntParam("MAX_SKILL_LEVEL", 10);
                    skill.targetType = value5 >= 0 && value5 <= 2 ? static_cast<uint8>(value5) : 0;
                    skill.minTargetHealthPct = value6 > 0 && value6 <= 100 ? static_cast<uint8>(value6) : 0;
                    skill.range = float1 > 0.0f ? float1 : 30.0f;
                    skill.damageBonusPerLevel = float2 > 0.0f ? float2 : 1.0f;
                    skill.description = !text2.empty() ? text2 : text1;
                    _skills[skill.skillId] = skill;
                    break;
                }
                case WUHUN_TEMPLATE_EQUIP_SLOT:
                {
                    uint32 slotId = 0;
                    if (!TryParseUInt(key, slotId) || slotId >= EQUIPMENT_SLOT_END)
                        break;

                    WuhunEquipSlotConfig config;
                    config.slot = static_cast<uint8>(slotId);
                    config.name = !name.empty() ? name : GetWuhunEquipmentSlotName(config.slot);
                    config.soulCost = value1 > 0 ? Acore::Number::ToUInt128Saturated(value1) : 0;
                    config.requirementId = value2 > 0 ? ToUInt32FromPositiveInt128(value2) : 0;
                    _equipSlotConfigs[config.slot] = config;
                    break;
                }
                default:
                    break;
            }
        } while (result->NextRow());

        LOG_INFO("server.loading", "武魂系统: 从旧 `_武魂模板` 加载技能模板 {} 个", _skills.size());
        return true;
    }

public:
    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto old = _players.find(playerGuid);
        if (old != _players.end())
            ClearEquipmentItems(old->second);

        PlayerWuhunData data;
        data.wuhunId = GetUIntParam("DEFAULT_WUHUN_ID", 1);
        data.skillMode = IsAvatarSkillAiEnabled() ? WUHUN_SKILL_MODE_AUTO : WUHUN_SKILL_MODE_SYNC;

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `武魂ID`, `已激活`, `已召唤`, `魂力`, "
            "`魂环1等级`, `魂环2等级`, `魂环3等级`, `魂环4等级`, `魂环5等级`, "
            "`魂环6等级`, `魂环7等级`, `魂环8等级`, `魂环9等级` "
            "FROM `_玩家武魂数据` WHERE `角色id` = {}", playerGuid))
        {
            Field* fields = result->Fetch();
            data.wuhunId = fields[0].Get<uint32>();
            data.activated = fields[1].Get<uint8>() != 0;
            data.summonRequested = fields[2].Get<uint8>() != 0;
            data.soulPower = fields[3].Get<uint128>();

            for (uint8 i = 0; i < WUHUN_RING_COUNT; ++i)
                data.ringLevels[i] = fields[4 + i].Get<uint32>();
        }
        else
        {
            CharacterDatabase.Execute(
                "INSERT IGNORE INTO `_玩家武魂数据` "
                "(`角色id`, `武魂ID`, `已激活`, `已召唤`, `魂力`, "
                "`魂环1等级`, `魂环2等级`, `魂环3等级`, `魂环4等级`, `魂环5等级`, "
                "`魂环6等级`, `魂环7等级`, `魂环8等级`, `魂环9等级`) "
                "VALUES ({}, {}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)",
                playerGuid, data.wuhunId);
        }

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `技能ID`, `等级`, `槽位` FROM `_玩家武魂技能` WHERE `角色id` = {}", playerGuid))
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 skillId = fields[0].Get<uint32>();
                uint32 level = fields[1].Get<uint32>();
                if (skillId && level)
                {
                    data.skillLevels[skillId] = level;

                    if (!fields[2].IsNull())
                    {
                        uint8 slot = fields[2].Get<uint8>();
                        if (slot < WUHUN_SKILL_SLOT_COUNT)
                            data.skillSlots[slot] = skillId;
                    }
                }
            } while (result->NextRow());
        }

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `槽位`, `解锁时间` FROM `_玩家武魂装备` WHERE `角色id` = {}", playerGuid))
        {
            do
            {
                Field* fields = result->Fetch();
                uint8 slot = fields[0].Get<uint8>();
                uint32 unlockTime = fields[1].Get<uint32>();
                if (slot < EQUIPMENT_SLOT_END && unlockTime)
                {
                    data.unlockedEquipSlots[slot] = true;
                    data.equipSlotUnlockTimes[slot] = unlockTime;
                }
            } while (result->NextRow());
        }

        _players[playerGuid] = std::move(data);
        LoadWuhunItems(player);

    }

    void SavePlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerWuhunData* data = GetPlayerData(playerGuid);
        if (!data)
            return;

        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家武魂数据` "
            "(`角色id`, `武魂ID`, `已激活`, `已召唤`, `魂力`, "
            "`魂环1等级`, `魂环2等级`, `魂环3等级`, `魂环4等级`, `魂环5等级`, "
            "`魂环6等级`, `魂环7等级`, `魂环8等级`, `魂环9等级`) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
            playerGuid, data->wuhunId, data->activated ? 1 : 0, data->summonRequested ? 1 : 0, Acore::ToString(data->soulPower),
            data->ringLevels[0], data->ringLevels[1], data->ringLevels[2],
            data->ringLevels[3], data->ringLevels[4], data->ringLevels[5],
            data->ringLevels[6], data->ringLevels[7], data->ringLevels[8]);

        for (auto const& pair : data->skillLevels)
        {
            uint8 assignedSlot = WUHUN_SKILL_SLOT_COUNT;
            for (uint8 i = 0; i < WUHUN_SKILL_SLOT_COUNT; ++i)
            {
                if (data->skillSlots[i] == pair.first)
                {
                    assignedSlot = i;
                    break;
                }
            }

            if (assignedSlot < WUHUN_SKILL_SLOT_COUNT)
            {
                CharacterDatabase.Execute(
                    "INSERT INTO `_玩家武魂技能` (`角色id`, `技能ID`, `等级`, `槽位`) VALUES ({}, {}, {}, {}) "
                    "ON DUPLICATE KEY UPDATE `等级` = VALUES(`等级`), `槽位` = VALUES(`槽位`)",
                    playerGuid, pair.first, pair.second, assignedSlot);
            }
            else
            {
                CharacterDatabase.Execute(
                    "INSERT INTO `_玩家武魂技能` (`角色id`, `技能ID`, `等级`, `槽位`) VALUES ({}, {}, {}, NULL) "
                    "ON DUPLICATE KEY UPDATE `等级` = VALUES(`等级`), `槽位` = NULL",
                    playerGuid, pair.first, pair.second);
            }
        }

        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        for (uint8 i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            auto equipItr = data->equipment.find(i);
            bool hasEquipment = equipItr != data->equipment.end();
            if (!data->unlockedEquipSlots[i] && !hasEquipment)
                continue;

            uint32 unlockTime = data->equipSlotUnlockTimes[i] ? data->equipSlotUnlockTimes[i] : now;
            uint32 itemEntry = hasEquipment ? equipItr->second.itemEntry : 0;
            ObjectGuid::LowType itemGuid = hasEquipment ? equipItr->second.itemGuid : ObjectGuid::LowType(0);
            CharacterDatabase.Execute(
                "INSERT INTO `_玩家武魂装备` (`角色id`, `槽位`, `解锁时间`, `物品ID`, `物品GUID`) VALUES ({}, {}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE `解锁时间` = VALUES(`解锁时间`), `物品ID` = VALUES(`物品ID`), `物品GUID` = VALUES(`物品GUID`)",
                playerGuid, i, unlockTime, itemEntry, itemGuid);
        }
    }

    void UnloadPlayerData(uint32 playerGuid)
    {
        auto itr = _players.find(playerGuid);
        if (itr == _players.end())
            return;

        ClearEquipmentItems(itr->second);
        _players.erase(itr);
    }

    void DeletePlayerData(uint32 playerGuid)
    {
        auto itr = _players.find(playerGuid);
        if (itr != _players.end())
        {
            ClearEquipmentItems(itr->second);
            _players.erase(itr);
        }

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `item` FROM `character_inventory` WHERE `guid` = {} AND `bag` = {}", playerGuid, WUHUN_VIRTUAL_BAG))
        {
            do
            {
                ObjectGuid::LowType itemGuid = static_cast<ObjectGuid::LowType>(result->Fetch()[0].Get<uint64>());
                CharacterDatabase.Execute("DELETE FROM `item_instance` WHERE `guid` = {}", itemGuid);
            } while (result->NextRow());
        }

        CharacterDatabase.Execute("DELETE FROM `character_inventory` WHERE `guid` = {} AND `bag` = {}", playerGuid, WUHUN_VIRTUAL_BAG);
        CharacterDatabase.Execute("DELETE FROM `_玩家武魂数据` WHERE `角色id` = {}", playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家武魂技能` WHERE `角色id` = {}", playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家武魂装备` WHERE `角色id` = {}", playerGuid);
    }

    Creature* SummonAvatar(Player* player, bool silent = false)
    {
        if (!player || !IsEnabled())
            return nullptr;

        if (!player->IsAlive())
        {
            if (!silent)
                SendResult(player, "SUMMON", false, "死亡状态不能召唤武魂");
            return nullptr;
        }

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return nullptr;

        if (!data->activated)
        {
            if (!silent)
                SendResult(player, "SUMMON", false, "请先激活武魂分身");
            SendState(player);
            return nullptr;
        }

        DismissAvatar(player, true, false);

        WuhunDefinition const* definition = GetDefinition(data->wuhunId);
        uint32 entry = definition ? definition->creatureEntry : GetUIntParam("AVATAR_ENTRY", 930001);

        float o = player->GetOrientation();
        float x = player->GetPositionX() + 2.0f * std::cos(o);
        float y = player->GetPositionY() + 2.0f * std::sin(o);
        float z = player->GetPositionZ();

        Creature* avatar = player->SummonCreature(entry, x, y, z, o, TEMPSUMMON_MANUAL_DESPAWN, 0);
        if (!avatar)
        {
            if (!silent)
                SendResult(player, "SUMMON", false, "武魂生物模板不存在或召唤失败");
            return nullptr;
        }

        avatar->SetOwnerGUID(player->GetGUID());
        avatar->SetCreatorGUID(player->GetGUID());
        avatar->SetFaction(player->GetFaction());
        avatar->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
        avatar->SetLevel(player->GetLevel());
        avatar->SetDisplayId(player->GetDisplayId());
        avatar->SetNativeDisplayId(player->GetDisplayId());
        avatar->SetObjectScale(definition ? definition->scale : player->GetObjectScale());
        avatar->SetReactState(REACT_PASSIVE);
        avatar->SetPvP(player->IsPvP());
        avatar->setPowerType(POWER_MANA);
        avatar->GetMotionMaster()->MoveFollow(player, GetFloatParam("FOLLOW_DISTANCE", 2.5f), WUHUN_FOLLOW_ANGLE);

        data->summonRequested = true;
        data->avatarGuid = avatar->GetGUID();
        UpdateAvatarVisuals(player, avatar);
        UpdateAvatarStats(player);

        if (Unit* target = ResolveOwnerTarget(player))
            avatar->AI()->AttackStart(target);

        if (!silent)
            SendResult(player, "SUMMON", true, "武魂已召唤");
        SendState(player);
        return avatar;
    }

    bool ActivateAvatar(Player* player)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!player || !data)
            return false;

        if (data->activated)
        {
            SendResult(player, "ACTIVATE", true, "武魂分身已激活");
            SendState(player);
            return true;
        }

        WuhunDefinition const* definition = GetDefinition(data->wuhunId);
        uint128 activationCost = definition ? definition->activationSoulCost : GetUInt128Param("ACTIVATION_SOUL_COST", 1000);
        if (data->soulPower < activationCost)
        {
            std::ostringstream message;
            message << "魂力不足，激活武魂分身需要 " << Acore::ToString(activationCost) << " 点魂力";
            SendResult(player, "ACTIVATE", false, message.str());
            SendState(player);
            return false;
        }

        data->soulPower -= activationCost;
        data->activated = true;
        SavePlayerData(player);
        SendResult(player, "ACTIVATE", true, "武魂分身激活成功，现在可以召唤");
        SendState(player);
        return true;
    }

    bool EvolveWuhun(Player* player)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!player || !data)
            return false;

        if (!data->activated)
        {
            SendResult(player, "EVOLVE_WUHUN", false, "请先激活武魂分身");
            SendState(player);
            return false;
        }

        WuhunDefinition const* next = GetNextDefinition(data->wuhunId);
        if (!next)
        {
            SendResult(player, "EVOLVE_WUHUN", false, "武魂已经进化到最高阶段");
            SendState(player);
            return false;
        }

        if (data->soulPower < next->evolveSoulCost)
        {
            std::ostringstream message;
            message << "魂力不足，进化到 " << next->name << " 需要 " << Acore::ToString(next->evolveSoulCost) << " 点魂力";
            SendResult(player, "EVOLVE_WUHUN", false, message.str());
            SendState(player);
            return false;
        }

        data->soulPower -= next->evolveSoulCost;
        data->wuhunId = next->id;
        SavePlayerData(player);
        DismissAvatar(player, true);

        SendResult(player, "EVOLVE_WUHUN", true, "武魂进化成功，请重新召唤");
        SendAll(player);
        return true;
    }

    void DismissAvatar(Player* player, bool silent = false, bool clearSummonRequest = true)
    {
        if (!player)
            return;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return;

        if (!data->avatarGuid.IsEmpty())
        {
            if (Creature* avatar = ObjectAccessor::GetCreature(*player, data->avatarGuid))
                avatar->DespawnOrUnsummon();
        }

        if (clearSummonRequest)
            data->summonRequested = false;

        data->avatarGuid.Clear();
        data->targetGuid.Clear();
        data->rangedMirrorTargetGuid.Clear();
        data->targetHoldTimer = 0;
        data->rangedMirrorHoldTimer = 0;

        if (!silent)
        {
            SendResult(player, "DISMISS", true, "武魂已收回");
            SendState(player);
        }
    }

    void RestoreAvatarIfRequested(Player* player)
    {
        if (!player || !IsEnabled())
            return;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data || !data->summonRequested || !data->activated || GetAvatar(player))
            return;

        SummonAvatar(player, true);
    }

    void ForgetAvatar(ObjectGuid ownerGuid, ObjectGuid avatarGuid)
    {
        if (ownerGuid.IsEmpty() || avatarGuid.IsEmpty())
            return;

        PlayerWuhunData* data = GetPlayerData(ownerGuid.GetCounter());
        if (data && data->avatarGuid == avatarGuid)
        {
            data->avatarGuid.Clear();
            data->summonRequested = false;
        }
    }

    Creature* GetAvatar(Player* player)
    {
        if (!player)
            return nullptr;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data || data->avatarGuid.IsEmpty())
            return nullptr;

        return ObjectAccessor::GetCreature(*player, data->avatarGuid);
    }

    void UpdateAvatarStats(Player* player)
    {
        if (!player)
            return;

        Creature* avatar = GetAvatar(player);
        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!avatar || !data)
            return;

        float inheritPercent = GetInheritancePercent(*data);
        WuhunEquipmentBonus equipmentBonus = CalculateEquipmentBonus(*data);

        uint128 oldMaxHealth = avatar->GetMaxHealthForCombat128();
        uint128 oldHealth = avatar->GetHealthForCombat128();
        long double healthPct = oldMaxHealth != 0 ? Acore::Number::ToLongDouble(oldHealth) / Acore::Number::ToLongDouble(oldMaxHealth) : 1.0L;
        if (healthPct <= 0.0L || healthPct > 1.0L)
            healthPct = 1.0L;

        uint128 inheritedHealth = ScaleUInt128(player->GetMaxHealthForCombat128(), inheritPercent);
        uint128 newMaxHealth = std::max<uint128>(static_cast<uint128>(player->GetLevel() * 50), SaturatingAddUInt128(inheritedHealth, equipmentBonus.health));
        uint128 newHealth = std::max<uint128>(uint128(1), Acore::Number::ToUInt128Saturated(Acore::Number::ToLongDouble(newMaxHealth) * healthPct));
        uint32 clientHealth = ToWuhunClientHealth(newMaxHealth);
        avatar->SetMaxHealth(clientHealth);
        if (newMaxHealth > clientHealth)
        {
            avatar->SetExtendedMaxHealth(newMaxHealth);
            avatar->SetExtendedHealth(newHealth);
            avatar->SyncClientHealthFromExtended();
        }
        else
        {
            avatar->SetExtendedMaxHealth(0);
            avatar->SetExtendedHealth(0);
            avatar->SetHealth(Acore::Number::ToUInt32Saturated(newHealth));
        }

        uint128 inheritedMana = ScaleUInt128(player->GetMaxPowerForCombat128(POWER_MANA), inheritPercent);
        uint128 newMana = SaturatingAddUInt128(inheritedMana, equipmentBonus.mana);
        uint32 clientMana = Acore::Number::ToUInt32Saturated(newMana);
        avatar->SetMaxPower(POWER_MANA, clientMana);
        if (newMana > clientMana)
        {
            avatar->SetExtendedMaxPower(POWER_MANA, newMana);
            avatar->SetExtendedPower(POWER_MANA, newMana);
            avatar->SyncClientPowerFromExtended(POWER_MANA);
        }
        else
        {
            avatar->SetExtendedMaxPower(POWER_MANA, 0);
            avatar->SetPower(POWER_MANA, clientMana);
        }

        for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        {
            int128 playerStat = player->GetExtendedStat128(Stats(i));
            if (playerStat <= 0)
                playerStat = Acore::Number::ToInt128Saturated(static_cast<long double>(player->GetTotalStatValue(Stats(i))));

            int128 inheritedStat = ScaleInt128(playerStat, inheritPercent);
            int128 finalStat = SaturatingAddInt128(inheritedStat, equipmentBonus.stats[i]);
            uint32 displayStat = finalStat > 0 ? Acore::Number::ToUInt32Saturated(static_cast<uint128>(std::min<int128>(finalStat, int128(2000000000)))) : 0;
            displayStat = std::min<uint32>(displayStat, 2000000000);
            avatar->SetCreateStat(Stats(i), static_cast<float>(displayStat));
            avatar->SetStat(Stats(i), displayStat);
        }

        uint128 inheritedArmor = ScaleUInt128(player->GetExtendedArmor128() > 0 ? static_cast<uint128>(player->GetExtendedArmor128()) : 0, inheritPercent);
        uint128 armor = SaturatingAddUInt128(inheritedArmor, equipmentBonus.armor);
        uint32 displayArmor = Acore::Number::ToUInt32Saturated(std::min<uint128>(armor, uint128(2000000000)));
        avatar->SetArmor(static_cast<int32>(displayArmor));
        avatar->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, static_cast<double>(displayArmor));
        avatar->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
        avatar->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
        avatar->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

        double inheritedAP = player->GetExtendedTotalAttackPowerValue(BASE_ATTACK) * static_cast<double>(inheritPercent) / 100.0;
        double inheritedRangedAP = player->GetExtendedTotalAttackPowerValue(RANGED_ATTACK) * static_cast<double>(inheritPercent) / 100.0;
        long double inheritedSpellPower = Acore::Number::ToLongDouble(player->GetExtendedSpellPowerBonus128()) * static_cast<long double>(inheritPercent) / 100.0L;
        double ap = inheritedAP + equipmentBonus.attackPower + equipmentBonus.spellPower * 0.5;
        double rangedAP = inheritedRangedAP + equipmentBonus.rangedAttackPower;
        double spellPower = static_cast<double>(std::min<long double>(
            std::numeric_limits<double>::max(),
            std::max<long double>(0.0L, inheritedSpellPower + static_cast<long double>(equipmentBonus.spellPower))));
        data->avatarAttackPower = ap;
        data->avatarRangedAttackPower = rangedAP;
        data->avatarSpellPower = spellPower;
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, ap);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, 1.0f);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, 0.0f);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 1.0f);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, rangedAP);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_PCT, 1.0f);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, 0.0f);
        avatar->SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, 1.0f);

        float minDamage = static_cast<float>(std::max<double>(1.0, ap / 14.0 * 2.0 + equipmentBonus.minDamage));
        float maxDamage = static_cast<float>(std::max<double>(minDamage + 1.0, ap / 14.0 * 2.4 + equipmentBonus.maxDamage));
        avatar->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, minDamage);
        avatar->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, maxDamage);
        avatar->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, minDamage * 0.5f);
        avatar->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, maxDamage * 0.5f);
        avatar->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, minDamage);
        avatar->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, maxDamage);
        avatar->UpdateDamagePhysical(BASE_ATTACK);
        avatar->UpdateDamagePhysical(OFF_ATTACK);
        avatar->UpdateDamagePhysical(RANGED_ATTACK);

        auto calculateInheritedHasteBonus = [player, inheritPercent](CombatRating rating, int64 equipmentRating) -> float
        {
            long double hasteBonus = static_cast<long double>(player->GetRatingBonusValue(rating)) * static_cast<long double>(inheritPercent) / 100.0L;
            if (equipmentRating > 0)
                hasteBonus += static_cast<long double>(equipmentRating) * static_cast<long double>(player->GetRatingMultiplier(rating));

            if (hasteBonus <= 0.0L || !std::isfinite(static_cast<double>(hasteBonus)))
                return 0.0f;

            return static_cast<float>(std::min<long double>(hasteBonus, 10000.0L));
        };

        CreatureTemplate const* avatarTemplate = avatar->GetCreatureTemplate();
        uint32 baseAttackTime = avatarTemplate && avatarTemplate->BaseAttackTime ? avatarTemplate->BaseAttackTime : BASE_ATTACK_TIME;
        uint32 rangeAttackTime = avatarTemplate && avatarTemplate->RangeAttackTime ? avatarTemplate->RangeAttackTime : baseAttackTime;
        float meleeHasteBonus = calculateInheritedHasteBonus(CR_HASTE_MELEE, equipmentBonus.meleeHasteRating);
        float rangedHasteBonus = calculateInheritedHasteBonus(CR_HASTE_RANGED, equipmentBonus.rangedHasteRating);
        uint32 meleeAttackTime = CalculateWuhunAttackTime(baseAttackTime, meleeHasteBonus);
        uint32 rangedAttackTime = CalculateWuhunAttackTime(rangeAttackTime, rangedHasteBonus);

        avatar->SetAttackTime(BASE_ATTACK, meleeAttackTime);
        avatar->SetAttackTime(OFF_ATTACK, meleeAttackTime);
        avatar->SetAttackTime(RANGED_ATTACK, rangedAttackTime);
        if (avatar->getAttackTimer(BASE_ATTACK) > static_cast<int32>(meleeAttackTime))
            avatar->setAttackTimer(BASE_ATTACK, static_cast<int32>(meleeAttackTime));
        if (avatar->getAttackTimer(OFF_ATTACK) > static_cast<int32>(meleeAttackTime))
            avatar->setAttackTimer(OFF_ATTACK, static_cast<int32>(meleeAttackTime));
        if (avatar->getAttackTimer(RANGED_ATTACK) > static_cast<int32>(rangedAttackTime))
            avatar->setAttackTimer(RANGED_ATTACK, static_cast<int32>(rangedAttackTime));

        if (IsDebug())
        {
            LOG_INFO("server.loading",
                "武魂系统: 玩家 {} 武魂属性同步，继承={:.2f}%，装备数={}，增强装备数={}，AP={:.2f}，远程AP={:.2f}，法强={:.2f}，主人近战急速={:.2f}%，武魂装备近战急速等级={}，近战攻速={}ms，远程攻速={}ms",
                player->GetName(),
                inheritPercent,
                equipmentBonus.itemCount,
                equipmentBonus.enhancedItemCount,
                data->avatarAttackPower,
                data->avatarRangedAttackPower,
                data->avatarSpellPower,
                player->GetRatingBonusValue(CR_HASTE_MELEE),
                equipmentBonus.meleeHasteRating,
                meleeAttackTime,
                rangedAttackTime);
        }

        UpdateAvatarVisuals(player, avatar);
        SendAvatarState(player);
    }

    void SetOwnerTarget(Player* player, Unit* target, uint32 holdMs = 3000)
    {
        SetOwnerTarget(player, target, holdMs, true);
    }

    void SetOwnerTarget(Player* player, Unit* target, uint32 holdMs, bool allowMeleeAssist)
    {
        if (!player || !target)
            return;

        if (!IsLegalOwnerTarget(player, target))
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return;

        data->targetGuid = target->GetGUID();
        data->targetHoldTimer = holdMs;

        if (!allowMeleeAssist)
        {
            data->rangedMirrorTargetGuid = target->GetGUID();
            data->rangedMirrorHoldTimer = std::max<uint32>(holdMs, WUHUN_RANGED_MIRROR_MELEE_DELAY_MS);
        }

        if (Creature* avatar = GetAvatar(player))
        {
            if (IsDebug())
            {
                LOG_INFO("server.loading",
                    "武魂系统: 同步主人目标，主人={}，目标={}，目标GUID={}，保持={}ms，近战协助={}，分身当前目标={}，距离={:.2f}",
                    player->GetName(),
                    target->GetName(),
                    target->GetGUID().ToString(),
                    holdMs,
                    allowMeleeAssist ? 1 : 0,
                    avatar->GetVictim() ? avatar->GetVictim()->GetName() : "无",
                    avatar->GetDistance(target));
            }

            if (!allowMeleeAssist)
            {
                if (avatar->GetVictim() == target)
                    avatar->AttackStop();
                return;
            }

            if (avatar->GetVictim() != target)
                avatar->AI()->AttackStart(target);
        }
    }

    bool IsRangedMirrorSpell(SpellInfo const* spellInfo) const
    {
        if (!spellInfo)
            return false;

        if (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED || spellInfo->IsRangedWeaponSpell())
            return true;

        return spellInfo->GetMaxRange(spellInfo->IsPositive()) > 8.0f;
    }

    bool IsRangedMirrorTarget(Player* player, Unit* target)
    {
        if (!player || !target)
            return false;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        return data && data->rangedMirrorHoldTimer && data->rangedMirrorTargetGuid == target->GetGUID();
    }

    void MirrorOwnerSpell(Player* player, Spell* spell)
    {
        if (!player || !spell || !IsEnabled())
            return;

        if (IsAutoSkillMode(player))
            return;

        Creature* avatar = GetAvatar(player);
        if (!avatar || !avatar->IsAlive())
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (!spellInfo)
            return;

        if (spellInfo->IsPassive() || spellInfo->Id == 6603)
            return;

        uint32 targetMask = spell->m_targets.GetTargetMask();
        if ((targetMask & (TARGET_FLAG_GAMEOBJECT_MASK | TARGET_FLAG_ITEM_MASK | TARGET_FLAG_CORPSE_MASK)) ||
            spell->m_targets.GetGOTarget() ||
            spell->m_targets.GetItemTarget() ||
            (spell->m_targets.GetObjectTarget() && !spell->m_targets.GetObjectTarget()->ToUnit()))
        {
            if (IsDebug())
            {
                LOG_INFO("server.loading",
                    "武魂系统: 跳过非单位目标同步施法，主人={}，SpellID={}，目标掩码={}",
                    player->GetName(),
                    spellInfo->Id,
                    targetMask);
            }
            return;
        }

        bool rangedMirror = IsRangedMirrorSpell(spellInfo);
        TriggerCastFlags mirrorFlags = WUHUN_SPELL_CAST_FLAGS;
        Unit* originalTarget = spell->m_targets.GetUnitTarget();
        Unit* logTarget = originalTarget;
        if (originalTarget && IsLegalOwnerTarget(player, originalTarget))
            SetOwnerTarget(player, originalTarget, rangedMirror ? WUHUN_RANGED_MIRROR_MELEE_DELAY_MS : 3000, !rangedMirror);

        SpellCastResult result = SPELL_FAILED_BAD_TARGETS;

        if (originalTarget)
        {
            Unit* unitTarget = originalTarget == player ? avatar : originalTarget;
            logTarget = unitTarget;
            result = avatar->CastSpell(unitTarget, spellInfo, mirrorFlags, nullptr, nullptr, ObjectGuid::Empty);
        }
        else
        {
            SpellCastTargets mirrorTargets = spell->m_targets;
            mirrorTargets.SetSrc(*avatar);

            if (!mirrorTargets.GetObjectTarget() && !mirrorTargets.GetItemTarget() && !mirrorTargets.HasDst())
            {
                mirrorTargets.SetUnitTarget(avatar);
                mirrorTargets.SetDst(*avatar);
                logTarget = avatar;
            }

            targetMask = mirrorTargets.GetTargetMask();
            result = avatar->CastSpell(mirrorTargets, spellInfo, nullptr, mirrorFlags, nullptr, nullptr, ObjectGuid::Empty);
        }

        if (result != SPELL_CAST_OK && IsDebug())
        {
            LOG_INFO("server.loading",
                "武魂系统: 同步施法失败，主人={}，SpellID={}，目标={}，目标掩码={}，远程模式={}，触发={}，结果={}",
                player->GetName(),
                spellInfo->Id,
                logTarget ? logTarget->GetName() : "非单位目标",
                targetMask,
                rangedMirror ? 1 : 0,
                spell->IsTriggered() ? 1 : 0,
                static_cast<uint32>(result));
        }
    }

    Unit* ResolveOwnerTarget(Player* player)
    {
        if (!player)
            return nullptr;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return nullptr;

        if (Unit* victim = player->GetVictim())
        {
            if (IsLegalOwnerTarget(player, victim))
                return victim;
        }

        if (!data->targetGuid.IsEmpty())
        {
            if (Unit* target = ObjectAccessor::GetUnit(*player, data->targetGuid))
            {
                if (IsLegalOwnerTarget(player, target))
                    return target;
            }
        }

        return nullptr;
    }

    Unit* ResolveAvatarTarget(Player* player, Creature* avatar)
    {
        if (!player)
            return nullptr;

        if (Unit* target = ResolveOwnerTarget(player))
            return target;

        float assistRadius = GetFloatParam("AUTO_ASSIST_RADIUS", 30.0f);

        if (Unit* attacker = player->getAttackerForHelper())
        {
            if (IsLegalOwnerTarget(player, attacker) && player->IsWithinDistInMap(attacker, assistRadius))
                return attacker;
        }

        if (avatar)
        {
            if (Unit* victim = avatar->GetVictim())
            {
                if (IsLegalOwnerTarget(player, victim))
                    return victim;
            }

            if (Unit* attacker = avatar->getAttackerForHelper())
            {
                if (IsLegalOwnerTarget(player, attacker))
                    return attacker;
            }

            if (sConfigMgr->GetOption<bool>("WuhunSystem.AutoAttackSelectedTarget", true))
            {
                if (Unit* selected = player->GetSelectedUnit())
                {
                    if (IsLegalOwnerTarget(player, selected) && player->IsWithinDistInMap(selected, assistRadius))
                        return selected;
                }
            }

            if (sConfigMgr->GetOption<bool>("WuhunSystem.AutoAttackNearbyTarget", true) && (player->IsInCombat() || avatar->IsInCombat()))
            {
                if (Unit* target = avatar->SelectNearestTargetInAttackDistance(assistRadius))
                {
                    if (IsLegalOwnerTarget(player, target))
                        return target;
                }
            }
        }

        return nullptr;
    }

    void UpdatePlayer(Player* player, uint32 diff)
    {
        if (!player || !IsEnabled())
            return;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return;

        if (data->targetHoldTimer > diff)
            data->targetHoldTimer -= diff;
        else
        {
            data->targetHoldTimer = 0;
            Creature* avatar = GetAvatar(player);
            if (!player->GetVictim() && (!avatar || !avatar->GetVictim()))
                data->targetGuid.Clear();
        }

        if (data->rangedMirrorHoldTimer > diff)
            data->rangedMirrorHoldTimer -= diff;
        else
        {
            data->rangedMirrorHoldTimer = 0;
            data->rangedMirrorTargetGuid.Clear();
        }

        data->syncTimer += diff;
        if (data->syncTimer >= GetUIntParam("TARGET_SYNC_INTERVAL", 250))
        {
            data->syncTimer = 0;

            if (Unit* victim = player->GetVictim())
            {
                if (!IsRangedMirrorTarget(player, victim))
                    SetOwnerTarget(player, victim, 1000, true);
            }
            else if (data->targetHoldTimer == 0)
            {
                if (Creature* avatar = GetAvatar(player))
                {
                    if (Unit* target = ResolveAvatarTarget(player, avatar))
                        SetOwnerTarget(player, target, 1000, !IsRangedMirrorTarget(player, target));
                    else
                    {
                        data->targetGuid.Clear();
                        avatar->AttackStop();
                        avatar->GetMotionMaster()->MoveFollow(player, GetFloatParam("FOLLOW_DISTANCE", 2.5f), WUHUN_FOLLOW_ANGLE);
                    }
                }
                else
                    data->targetGuid.Clear();
            }
        }

        data->statRefreshTimer += diff;
        if (data->statRefreshTimer >= GetUIntParam("STAT_REFRESH_INTERVAL", 3000))
        {
            data->statRefreshTimer = 0;
            if (GetAvatar(player))
                UpdateAvatarStats(player);
        }

        data->uiStateTimer += diff;
        if (data->uiStateTimer >= 1000)
        {
            data->uiStateTimer = 0;
            SendState(player);
        }
    }

    void OnBossKilled(Player* player, Creature* creature)
    {
        if (!player || !creature || !IsEnabled() || !IsWuhunBoss(creature))
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        WuhunDefinition const* definition = data ? GetDefinition(data->wuhunId) : nullptr;
        uint128 reward = creature->isWorldBoss()
            ? (definition ? definition->worldBossSoulPower : GetUInt128Param("WORLD_BOSS_SOUL_POWER", 1))
            : (definition ? definition->dungeonBossSoulPower : GetUInt128Param("DUNGEON_BOSS_SOUL_POWER", 1));

        AddSoulPower(player, reward, creature->GetName());
    }

    void AddSoulPower(Player* player, uint128 const& amount, std::string const& source)
    {
        if (!player || !amount)
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return;

        data->soulPower = SaturatingAddUInt128(data->soulPower, amount);
        SavePlayerData(player);

        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff66ccff[武魂系统]|r 击杀 {} 获得 |cffffd700{}|r 点魂力。",
            source.empty() ? "Boss" : source, Acore::ToString(amount));

        SendState(player);
    }

    bool SetSkillMode(Player* player, uint32 rawMode)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!player || !data)
            return false;

        data->skillMode = NormalizeSkillMode(rawMode);
        data->rangedMirrorTargetGuid.Clear();
        data->rangedMirrorHoldTimer = 0;
        SendState(player);
        return true;
    }

    bool IsAutoSkillMode(Player* player)
    {
        PlayerWuhunData* data = player ? GetPlayerData(player->GetGUID().GetCounter()) : nullptr;
        return data && data->skillMode == WUHUN_SKILL_MODE_AUTO;
    }

    bool GetSoulPowerInfo(Player* player, uint128& soulPower, uint32& wuhunId)
    {
        if (!player)
            return false;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return false;

        soulPower = data->soulPower;
        wuhunId = data->wuhunId;
        return true;
    }

    bool SetSoulPower(Player* player, uint128 const& amount)
    {
        if (!player)
            return false;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return false;

        data->soulPower = amount;

        SavePlayerData(player);
        SendState(player);
        return true;
    }

    bool AdjustSoulPowerByCommand(Player* player, int128 const& delta)
    {
        if (!player)
            return false;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return false;

        if (delta > 0)
        {
            uint128 amount = static_cast<uint128>(delta);
            data->soulPower = SaturatingAddUInt128(data->soulPower, amount);
        }
        else if (delta < 0)
        {
            uint128 amount = AbsInt128ToUInt128(delta);
            data->soulPower = amount >= data->soulPower ? 0 : data->soulPower - amount;
        }

        SavePlayerData(player);
        SendState(player);
        return true;
    }

    bool ResetPlayerWuhun(Player* player)
    {
        if (!player)
            return false;

        DismissAvatar(player, true);
        DeletePlayerData(player->GetGUID().GetCounter());
        LoadPlayerData(player);
        SavePlayerData(player);
        SendAll(player);
        return true;
    }

    bool InfuseRing(Player* player, uint32 rawSlot, uint128 const& amount)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!player || !data)
            return false;

        uint8 slot = 0;
        if (!NormalizeUiSlot(rawSlot, WUHUN_RING_COUNT, slot))
        {
            SendResult(player, "INFUSE_RING", false, "魂环槽位无效");
            return false;
        }

        uint32 maxRingLevel = GetUIntParam("MAX_RING_LEVEL", 100);
        uint128 budget = amount ? std::min<uint128>(amount, data->soulPower) : data->soulPower;
        uint128 spent = 0;
        uint32 upgraded = 0;

        while (data->ringLevels[slot] < maxRingLevel)
        {
            uint32 nextLevel = data->ringLevels[slot] + 1;
            uint128 cost = GetRingCost(nextLevel);
            if (!cost)
                cost = 1;

            if (cost > budget - spent)
                break;

            spent += cost;
            ++data->ringLevels[slot];
            ++upgraded;
        }

        if (!upgraded)
        {
            SendResult(player, "INFUSE_RING", false, "魂力不足，无法提升魂环");
            return false;
        }

        data->soulPower -= spent;
        SavePlayerData(player);
        UpdateAvatarStats(player);
        SendResult(player, "INFUSE_RING", true, "魂环提升成功");
        SendState(player);
        return true;
    }

    bool LearnSkill(Player* player, uint32 skillId)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        WuhunSkillTemplate const* skill = GetSkill(skillId);
        if (!player || !data || !skill)
        {
            SendResult(player, "LEARN_SKILL", false, "技能模板不存在");
            return false;
        }

        if (data->skillLevels.find(skillId) != data->skillLevels.end())
        {
            SendResult(player, "LEARN_SKILL", false, "武魂已经学会该技能");
            return false;
        }

        if (data->soulPower < skill->learnCost)
        {
            SendResult(player, "LEARN_SKILL", false, "魂力不足，无法学习技能");
            return false;
        }

        data->soulPower -= skill->learnCost;
        data->skillLevels[skillId] = 1;

        for (uint8 i = 0; i < WUHUN_SKILL_SLOT_COUNT; ++i)
        {
            if (!data->skillSlots[i])
            {
                data->skillSlots[i] = skillId;
                break;
            }
        }

        SavePlayerData(player);
        UpdateAvatarStats(player);
        SendResult(player, "LEARN_SKILL", true, "技能学习成功");
        SendSkills(player);
        SendState(player);
        return true;
    }

    bool UpgradeSkill(Player* player, uint32 skillId)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        WuhunSkillTemplate const* skill = GetSkill(skillId);
        if (!player || !data || !skill)
        {
            SendResult(player, "UPGRADE_SKILL", false, "技能模板不存在");
            return false;
        }

        auto itr = data->skillLevels.find(skillId);
        if (itr == data->skillLevels.end())
        {
            SendResult(player, "UPGRADE_SKILL", false, "武魂尚未学会该技能");
            return false;
        }

        uint32 maxSkillLevel = skill->maxLevel ? skill->maxLevel : GetUIntParam("MAX_SKILL_LEVEL", 10);
        if (itr->second >= maxSkillLevel)
        {
            SendResult(player, "UPGRADE_SKILL", false, "技能已达到上限");
            return false;
        }

        uint32 nextLevel = itr->second + 1;
        uint128 cost = SaturatingMultiplyUInt128(skill->upgradeCost, nextLevel);
        if (data->soulPower < cost)
        {
            SendResult(player, "UPGRADE_SKILL", false, "魂力不足，无法升级技能");
            return false;
        }

        data->soulPower -= cost;
        itr->second = nextLevel;
        SavePlayerData(player);
        UpdateAvatarStats(player);
        SendResult(player, "UPGRADE_SKILL", true, "技能升级成功");
        SendSkills(player);
        SendState(player);
        return true;
    }

    bool SetSkill(Player* player, uint32 rawSlot, uint32 skillId)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!player || !data)
            return false;

        uint8 slot = 0;
        if (!NormalizeUiSlot(rawSlot, WUHUN_SKILL_SLOT_COUNT, slot))
        {
            SendResult(player, "SET_SKILL", false, "技能槽位无效");
            return false;
        }

        if (!skillId)
            return ClearSkill(player, rawSlot);

        if (data->skillLevels.find(skillId) == data->skillLevels.end())
        {
            SendResult(player, "SET_SKILL", false, "武魂尚未学会该技能");
            return false;
        }

        for (uint8 i = 0; i < WUHUN_SKILL_SLOT_COUNT; ++i)
        {
            if (i != slot && data->skillSlots[i] == skillId)
                data->skillSlots[i] = 0;
        }

        data->skillSlots[slot] = skillId;
        SavePlayerData(player);
        SendResult(player, "SET_SKILL", true, "技能装配成功");
        SendSkills(player);
        return true;
    }

    bool ClearSkill(Player* player, uint32 rawSlot)
    {
        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!player || !data)
            return false;

        uint8 slot = 0;
        if (!NormalizeUiSlot(rawSlot, WUHUN_SKILL_SLOT_COUNT, slot))
        {
            SendResult(player, "CLEAR_SKILL", false, "技能槽位无效");
            return false;
        }

        data->skillSlots[slot] = 0;
        SavePlayerData(player);
        SendResult(player, "CLEAR_SKILL", true, "技能槽已清空");
        SendSkills(player);
        return true;
    }

    bool UnlockEquipSlot(Player* player, uint32 rawWuhunSlot)
    {
        if (!player)
            return false;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return false;

        if (rawWuhunSlot >= EQUIPMENT_SLOT_END)
        {
            SendResult(player, "UNLOCK_EQUIP_SLOT", false, "武魂装备槽位无效");
            return false;
        }

        uint8 wuhunSlot = static_cast<uint8>(rawWuhunSlot);
        WuhunEquipSlotConfig const* config = GetEquipSlotConfig(wuhunSlot);
        if (!config)
        {
            SendResult(player, "UNLOCK_EQUIP_SLOT", false, "该武魂装备槽暂未开放");
            return false;
        }

        if (IsEquipSlotUnlocked(*data, wuhunSlot))
        {
            SendResult(player, "UNLOCK_EQUIP_SLOT", true, "该武魂装备槽已解锁");
            SendEquip(player);
            return true;
        }

        if (data->soulPower < config->soulCost)
        {
            SendResult(player, "UNLOCK_EQUIP_SLOT", false, "魂力不足，无法解锁该装备槽");
            SendEquip(player);
            return false;
        }

        if (config->requirementId)
        {
            RequirementInterface* reqModule = GetRequirementModule();
            if (!reqModule)
            {
                SendResult(player, "UNLOCK_EQUIP_SLOT", false, "需求模板系统未加载，无法解锁该装备槽");
                return false;
            }

            if (!reqModule->CheckRequirements(player, config->requirementId, false))
            {
                ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[武魂系统]|r 不满足装备槽解锁条件：");
                reqModule->CheckRequirements(player, config->requirementId, true);
                SendResult(player, "UNLOCK_EQUIP_SLOT", false, "不满足需求模板条件");
                return false;
            }

            if (!reqModule->ConsumeRequirements(player, config->requirementId))
            {
                SendResult(player, "UNLOCK_EQUIP_SLOT", false, "消耗需求模板资源失败");
                return false;
            }
        }

        data->soulPower -= config->soulCost;
        data->unlockedEquipSlots[wuhunSlot] = true;
        data->equipSlotUnlockTimes[wuhunSlot] = static_cast<uint32>(GameTime::GetGameTime().count());
        SavePlayerData(player);
        SendResult(player, "UNLOCK_EQUIP_SLOT", true, "武魂装备槽解锁成功");
        SendEquip(player);
        SendState(player);
        return true;
    }

    bool EquipItem(Player* player, uint32 rawBag, uint32 rawSlot, uint32 rawWuhunSlot)
    {
        if (!player)
            return false;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return false;

        if (rawBag > std::numeric_limits<uint8>::max() || rawSlot > std::numeric_limits<uint8>::max() || rawWuhunSlot >= EQUIPMENT_SLOT_END)
        {
            SendResult(player, "EQUIP_ITEM", false, "装备参数无效");
            return false;
        }

        uint8 bag = static_cast<uint8>(rawBag);
        uint8 itemSlot = static_cast<uint8>(rawSlot);
        uint8 wuhunSlot = static_cast<uint8>(rawWuhunSlot);
        if (!IsEquipSlotUnlocked(*data, wuhunSlot))
        {
            SendResult(player, "EQUIP_ITEM", false, "该武魂装备槽尚未解锁");
            SendEquip(player);
            return false;
        }

        Item* item = player->GetItemByPos(bag, itemSlot);
        if (!item)
        {
            SendResult(player, "EQUIP_ITEM", false, "背包物品不存在");
            return false;
        }

        ItemTemplate const* proto = item->GetTemplate();
        if (!IsValidWuhunEquipmentSlot(proto, wuhunSlot))
        {
            SendResult(player, "EQUIP_ITEM", false, "该物品不能装备到目标武魂槽位");
            return false;
        }

        if (data->equipment.find(wuhunSlot) != data->equipment.end())
        {
            if (!UnequipItem(player, wuhunSlot, true))
                return false;
        }

        ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();
        uint32 itemEntry = item->GetEntry();
        uint8 sourceBag = item->GetBagSlot();
        uint8 sourceSlot = item->GetSlot();

        item->RemoveFromUpdateQueueOf(player);
        player->MoveItemFromInventory(sourceBag, sourceSlot, true);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        item->DeleteFromInventoryDB(trans);

        CharacterDatabasePreparedStatement* invStmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
        invStmt->SetData(0, player->GetGUID().GetCounter());
        invStmt->SetData(1, WUHUN_VIRTUAL_BAG);
        invStmt->SetData(2, wuhunSlot);
        invStmt->SetData(3, itemGuid);
        trans->Append(invStmt);
        AppendReplaceItemInstance(trans, item, player->GetGUID().GetCounter());
        CharacterDatabase.CommitTransaction(trans);

        WuhunEquipmentSlot slotData;
        slotData.itemEntry = itemEntry;
        slotData.itemGuid = itemGuid;
        slotData.item = item;
        data->equipment[wuhunSlot] = slotData;

        SavePlayerData(player);
        UpdateAvatarStats(player);
        SendResult(player, "EQUIP_ITEM", true, "武魂装备成功");
        SendEquip(player);
        SendState(player);
        return true;
    }

    bool UnequipItem(Player* player, uint32 rawWuhunSlot, bool silent = false)
    {
        if (!player)
            return false;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return false;

        if (rawWuhunSlot >= EQUIPMENT_SLOT_END)
        {
            if (!silent)
                SendResult(player, "UNEQUIP_ITEM", false, "武魂装备槽位无效");
            return false;
        }

        uint8 wuhunSlot = static_cast<uint8>(rawWuhunSlot);
        auto itr = data->equipment.find(wuhunSlot);
        if (itr == data->equipment.end())
        {
            if (!silent)
                SendResult(player, "UNEQUIP_ITEM", false, "该槽位没有武魂装备");
            return false;
        }

        Item* item = itr->second.item;
        ObjectGuid::LowType itemGuid = itr->second.itemGuid;

        if (item)
        {
            item->SetOwnerGUID(player->GetGUID());

            ItemPosCountVec dest;
            InventoryResult result = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
            if (result == EQUIP_ERR_OK)
            {
                player->MoveItemToInventory(dest, item, true, true);

                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
                delStmt->SetData(0, WUHUN_VIRTUAL_BAG);
                delStmt->SetData(1, wuhunSlot);
                delStmt->SetData(2, player->GetGUID().GetCounter());
                trans->Append(delStmt);
                player->SaveInventoryAndGoldToDB(trans);
                CharacterDatabase.CommitTransaction(trans);
            }
            else
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
                delStmt->SetData(0, WUHUN_VIRTUAL_BAG);
                delStmt->SetData(1, wuhunSlot);
                delStmt->SetData(2, player->GetGUID().GetCounter());
                trans->Append(delStmt);

                MailDraft draft("武魂系统", "您的背包已满，武魂装备已通过邮件返还。");
                draft.AddItem(item);
                draft.SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
                CharacterDatabase.CommitTransaction(trans);

                if (!silent)
                    ChatHandler(player->GetSession()).SendSysMessage("背包已满，武魂装备已通过邮件返还。");
            }
        }
        else
        {
            CharacterDatabase.Execute("DELETE FROM `item_instance` WHERE `guid` = {}", itemGuid);
            CharacterDatabase.Execute("DELETE FROM `character_inventory` WHERE `guid` = {} AND `bag` = {} AND `slot` = {}",
                player->GetGUID().GetCounter(), WUHUN_VIRTUAL_BAG, wuhunSlot);
        }

        data->equipment.erase(itr);
        SavePlayerData(player);
        UpdateAvatarStats(player);

        if (!silent)
        {
            SendResult(player, "UNEQUIP_ITEM", true, "武魂装备已卸下");
            SendEquip(player);
            SendState(player);
        }

        return true;
    }

    WuhunSkillTemplate const* GetSkill(uint32 skillId) const
    {
        auto itr = _skills.find(skillId);
        return itr != _skills.end() ? &itr->second : nullptr;
    }

    std::vector<WuhunSkillRuntime> GetEquippedSkills(uint32 playerGuid) const
    {
        std::vector<WuhunSkillRuntime> skills;
        auto dataItr = _players.find(playerGuid);
        if (dataItr == _players.end())
            return skills;

        PlayerWuhunData const& data = dataItr->second;
        for (uint8 slot = 0; slot < data.skillSlots.size(); ++slot)
        {
            uint32 skillId = data.skillSlots[slot];
            if (!skillId)
                continue;

            auto levelItr = data.skillLevels.find(skillId);
            if (levelItr == data.skillLevels.end())
                continue;

            skills.push_back({ skillId, levelItr->second, slot });
        }

        return skills;
    }

    long double CalculateAvatarSkillPowerBonus(PlayerWuhunData const& data, SpellInfo const* spellInfo, bool periodic) const
    {
        if (!spellInfo)
            return 0.0L;

        long double meleePower = std::max<long double>(0.0L, static_cast<long double>(data.avatarAttackPower));
        long double rangedPower = std::max<long double>(0.0L, static_cast<long double>(data.avatarRangedAttackPower));
        long double spellPower = std::max<long double>(0.0L, static_cast<long double>(data.avatarSpellPower));
        long double weaponPower = std::max(meleePower, rangedPower);
        long double combatPower = spellPower;

        if (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE)
            combatPower = std::max(meleePower, spellPower * 0.5L);
        else if (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED)
            combatPower = std::max(rangedPower, spellPower * 0.5L);
        else if (spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
            combatPower = std::max(spellPower, weaponPower * 0.35L);
        else if (spellInfo->GetSchoolMask() == SPELL_SCHOOL_MASK_NORMAL)
            combatPower = weaponPower;
        else
            combatPower = std::max(spellPower, weaponPower * 0.35L);

        long double coefficient = periodic ? WUHUN_PERIODIC_SKILL_POWER_COEFFICIENT : WUHUN_DIRECT_SKILL_POWER_COEFFICIENT;
        return combatPower * coefficient;
    }

    void ApplySkillDamageBonus(Creature* avatar, SpellInfo const* spellInfo, uint128& damage, char const* source)
    {
        if (!avatar || !spellInfo || !damage)
            return;

        ObjectGuid ownerGuid = avatar->GetOwnerGUID();
        if (!ownerGuid.IsPlayer())
            return;

        PlayerWuhunData* data = GetPlayerData(ownerGuid.GetCounter());
        if (!data || data->avatarGuid != avatar->GetGUID())
            return;

        bool periodic = source && std::string(source) == "periodic";
        long double powerBonus = CalculateAvatarSkillPowerBonus(*data, spellInfo, periodic);
        float bonusPct = 0.0f;
        for (uint32 skillId : data->skillSlots)
        {
            if (!skillId)
                continue;

            auto levelItr = data->skillLevels.find(skillId);
            if (levelItr == data->skillLevels.end() || !levelItr->second)
                continue;

            WuhunSkillTemplate const* skill = GetSkill(skillId);
            if (!skill || skill->spellId != spellInfo->Id || skill->damageBonusPerLevel <= 0.0f)
                continue;

            bonusPct = std::max<float>(bonusPct, static_cast<float>(levelItr->second) * skill->damageBonusPerLevel);
        }

        if (bonusPct <= 0.0f && powerBonus <= 0.0L)
            return;

        uint128 oldDamage = damage;
        uint128 workingDamage = AddDamageSaturated(Acore::Number::ToUInt64Saturated(damage), powerBonus);

        if (bonusPct > 0.0f)
        {
            uint128 beforePctDamage = workingDamage;
            long double scaled = Acore::Number::ToLongDouble(workingDamage) * (100.0L + static_cast<long double>(bonusPct)) / 100.0L;
            workingDamage = !std::isfinite(static_cast<double>(scaled))
                ? std::numeric_limits<uint128>::max()
                : Acore::Number::ToUInt128Saturated(scaled);

            if (workingDamage == beforePctDamage && workingDamage < std::numeric_limits<uint128>::max() && scaled > Acore::Number::ToLongDouble(beforePctDamage))
                ++workingDamage;
        }

        damage = workingDamage;

        if (IsDebug())
        {
            LOG_INFO("server.loading",
                "武魂系统: 技能伤害加成，来源={}，主人GUID={}，SpellID={}，AP={:.2f}，远程AP={:.2f}，法强={:.2f}，属性伤害加成={:.2f}，技能等级加成={:.2f}%，伤害 {} -> {}",
                source ? source : "unknown",
                ownerGuid.GetCounter(),
                spellInfo->Id,
                data->avatarAttackPower,
                data->avatarRangedAttackPower,
                data->avatarSpellPower,
                static_cast<double>(powerBonus),
                bonusPct,
                oldDamage.convert_to<std::string>(),
                damage.convert_to<std::string>());
        }
    }

    std::string BuildSkillDebugSummary(uint32 playerGuid) const
    {
        auto dataItr = _players.find(playerGuid);
        if (dataItr == _players.end())
            return "playerData=missing";

        PlayerWuhunData const& data = dataItr->second;
        std::ostringstream out;
        uint32 equippedCount = 0;
        uint32 validCount = 0;
        out << "learned=" << data.skillLevels.size() << ", slots=[";

        for (uint8 i = 0; i < WUHUN_SKILL_SLOT_COUNT; ++i)
        {
            if (i)
                out << ';';

            uint32 skillId = data.skillSlots[i];
            if (!skillId)
            {
                out << (uint32(i) + 1) << ":empty";
                continue;
            }

            ++equippedCount;
            auto levelItr = data.skillLevels.find(skillId);
            WuhunSkillTemplate const* skill = GetSkill(skillId);
            SpellInfo const* spellInfo = skill && skill->spellId ? sSpellMgr->GetSpellInfo(skill->spellId) : nullptr;
            if (levelItr != data.skillLevels.end() && skill && spellInfo)
                ++validCount;

            out << (uint32(i) + 1) << ':' << skillId
                << "(lv=" << (levelItr != data.skillLevels.end() ? levelItr->second : 0)
                << ",tpl=" << (skill ? 1 : 0)
                << ",spell=" << (spellInfo ? (skill ? skill->spellId : 0) : 0)
                << ')';
        }

        out << "], equipped=" << equippedCount << ", valid=" << validCount;
        return out.str();
    }

    void SendAll(Player* player)
    {
        SendSkillList(player);
        SendState(player);
        SendSkills(player);
        SendEquip(player);
    }

    void SendOpenUI(Player* player)
    {
        SendPayload(player, "OPEN_UI");
    }

    void SendState(Player* player)
    {
        if (!player)
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return;

        std::ostringstream rings;
        for (uint8 i = 0; i < WUHUN_RING_COUNT; ++i)
        {
            if (i)
                rings << ',';
            rings << (i + 1) << ':' << data->ringLevels[i];
        }

        std::ostringstream percent;
        percent << std::fixed << std::setprecision(2) << GetInheritancePercent(*data);

        Creature* avatar = GetAvatar(player);
        std::string avatarHealth = avatar ? avatar->GetHealthForCombat128().convert_to<std::string>() : "0";
        std::string avatarMaxHealth = avatar ? avatar->GetMaxHealthForCombat128().convert_to<std::string>() : "0";
        std::string avatarMana = avatar ? avatar->GetPowerForCombat128(POWER_MANA).convert_to<std::string>() : "0";
        std::string avatarMaxMana = avatar ? avatar->GetMaxPowerForCombat128(POWER_MANA).convert_to<std::string>() : "0";
        uint32 avatarLevel = avatar ? avatar->GetLevel() : player->GetLevel();
        std::string avatarName = avatar ? std::string(avatar->GetName()) : std::string("武魂分身");
        WuhunDefinition const* definition = GetDefinition(data->wuhunId);
        WuhunDefinition const* nextDefinition = GetNextDefinition(data->wuhunId);

        std::ostringstream payload;
        payload << "STATE:" << data->wuhunId << '|'
                << Acore::ToString(data->soulPower) << '|'
                << Acore::ToString(data->soulPower) << '|'
                << percent.str() << '|'
                << (avatar ? 1 : 0) << '|'
                << rings.str() << '|'
                << avatarHealth << '|'
                << avatarMaxHealth << '|'
                << avatarMana << '|'
                << avatarMaxMana << '|'
                << static_cast<uint32>(player->getClass()) << '|'
                << avatarLevel << '|'
                << avatarName << '|'
                << static_cast<uint32>(data->skillMode) << '|'
                << (data->activated ? 1 : 0) << '|'
                << Acore::ToString(definition ? definition->activationSoulCost : GetUInt128Param("ACTIVATION_SOUL_COST", 1000)) << '|'
                << (nextDefinition ? nextDefinition->id : 0) << '|'
                << Acore::ToString(nextDefinition ? nextDefinition->evolveSoulCost : uint128(0));

        SendPayload(player, payload.str());
        SendAvatarState(player);
    }

    void SendAvatarState(Player* player)
    {
        if (!player)
            return;

        Creature* avatar = GetAvatar(player);
        std::string avatarHealth = avatar ? avatar->GetHealthForCombat128().convert_to<std::string>() : "0";
        std::string avatarMaxHealth = avatar ? avatar->GetMaxHealthForCombat128().convert_to<std::string>() : "0";
        std::string avatarMana = avatar ? avatar->GetPowerForCombat128(POWER_MANA).convert_to<std::string>() : "0";
        std::string avatarMaxMana = avatar ? avatar->GetMaxPowerForCombat128(POWER_MANA).convert_to<std::string>() : "0";
        uint32 avatarLevel = avatar ? avatar->GetLevel() : player->GetLevel();
        std::string avatarName = avatar ? std::string(avatar->GetName()) : std::string("武魂分身");

        std::ostringstream payload;
        payload << "AVATAR_STATE:"
                << (avatar ? 1 : 0) << '|'
                << avatarHealth << '|'
                << avatarMaxHealth << '|'
                << avatarMana << '|'
                << avatarMaxMana << '|'
                << static_cast<uint32>(player->getClass()) << '|'
                << avatarLevel << '|'
                << avatarName;

        SendPayload(player, payload.str());
        SendPanelAvatarState(player, avatarHealth, avatarMaxHealth, avatarMana, avatarMaxMana, avatar ? 1 : 0, avatarLevel);
    }

    void SendPanelAvatarState(Player* player, std::string const& avatarHealth, std::string const& avatarMaxHealth, std::string const& avatarMana, std::string const& avatarMaxMana, uint32 summoned, uint32 avatarLevel)
    {
        if (!player || !player->GetSession())
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        std::ostringstream inherit;
        inherit << std::fixed << std::setprecision(2) << (data ? GetInheritancePercent(*data) : 0.0f);

        std::ostringstream payload;
        payload << "STATS:"
                << "WUHUN_SUMMONED=" << summoned
                << "|WUHUN_HEALTH=" << avatarHealth
                << "|WUHUN_MAX_HEALTH=" << avatarMaxHealth
                << "|WUHUN_MANA=" << avatarMana
                << "|WUHUN_MAX_MANA=" << avatarMaxMana
                << "|WUHUN_CLASS=" << static_cast<uint32>(player->getClass())
                << "|WUHUN_LEVEL=" << avatarLevel
                << "|WUHUN_INHERIT=" << inherit.str();

        std::string fullMessage = std::string(WUHUN_PANEL_ADDON_PREFIX) + '\t' + payload.str();
        WorldPacket packet;
        ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&packet);
    }

    void SendSkillList(Player* player)
    {
        if (!player)
            return;

        std::ostringstream payload;
        payload << "SKILL_LIST:";

        bool first = true;
        for (auto const& pair : _skills)
        {
            WuhunSkillTemplate const& skill = pair.second;
            if (!first)
                payload << '^';
            first = false;

            payload << skill.skillId << ','
                    << skill.name << ','
                    << skill.spellId << ','
                    << skill.cooldownMs << ','
                    << skill.range << ','
                    << Acore::ToString(skill.learnCost) << ','
                    << Acore::ToString(skill.upgradeCost) << ','
                    << static_cast<uint32>(skill.targetType) << ','
                    << static_cast<uint32>(skill.minTargetHealthPct) << ','
                    << skill.description;
        }

        SendPayload(player, payload.str());
    }

    void SendSkills(Player* player)
    {
        if (!player)
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return;

        std::ostringstream learned;
        bool first = true;
        for (auto const& pair : data->skillLevels)
        {
            if (!first)
                learned << ',';
            first = false;
            learned << pair.first << ':' << pair.second;
        }

        std::ostringstream slots;
        for (uint8 i = 0; i < WUHUN_SKILL_SLOT_COUNT; ++i)
        {
            if (i)
                slots << ',';
            slots << (i + 1) << ':' << data->skillSlots[i];
        }

        std::ostringstream payload;
        payload << "SKILL_STATE:" << learned.str() << '|' << slots.str();
        SendPayload(player, payload.str());
    }

    void SendEquipSlotState(Player* player)
    {
        if (!player)
            return;

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return;

        std::ostringstream payload;
        payload << "EQUIP_SLOT_STATE:";

        bool first = true;
        for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            WuhunEquipSlotConfig const* config = GetEquipSlotConfig(slot);
            if (!config)
                continue;

            if (!first)
                payload << '^';
            first = false;

            payload << static_cast<uint32>(slot) << ','
                    << (IsEquipSlotUnlocked(*data, slot) ? 1 : 0) << ','
                    << Acore::ToString(config->soulCost) << ','
                    << config->requirementId << ','
                    << config->name;
        }

        SendPayload(player, payload.str());
    }

    void SendEquip(Player* player)
    {
        if (!player)
            return;

        ReloadEquipDataFromDB(player);

        PlayerWuhunData* data = EnsurePlayerData(player);
        if (!data)
            return;

        SendEquipSlotState(player);

        std::ostringstream payload;
        payload << "EQUIP_STATE:";

        bool first = true;
        for (auto const& pair : data->equipment)
        {
            if (!first)
                payload << '^';
            first = false;

            std::string itemName = "未知物品";
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(pair.second.itemEntry))
                itemName = proto->Name1;

            payload << static_cast<uint32>(pair.first) << ','
                    << pair.second.itemEntry << ','
                    << pair.second.itemGuid << ','
                    << itemName;
        }

        SendPayload(player, payload.str());
    }

    void SendResult(Player* player, std::string const& action, bool success, std::string const& message)
    {
        if (!player)
            return;

        std::ostringstream payload;
        payload << "RESULT:" << action << '^' << (success ? 1 : 0) << '^' << message;
        SendPayload(player, payload.str());
    }

private:
    WuhunSystemMgr() = default;

    bool HasWorldTable(char const* tableName) const
    {
        return static_cast<bool>(WorldDatabase.Query("SHOW TABLES LIKE '{}'", tableName));
    }

    bool HasWorldColumn(char const* tableName, char const* columnName) const
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT COUNT(*) FROM `INFORMATION_SCHEMA`.`COLUMNS` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '{}' AND `COLUMN_NAME` = '{}'",
            tableName, columnName);
        return result && (*result)[0].Get<uint64>() > 0;
    }

    bool LoadTemplateDefinitions()
    {
        if (!HasWorldTable("_武魂系统_模板"))
            return false;

        QueryResult result = WorldDatabase.Query(
            "SELECT `模板ID`, `名称`, `生物模板`, `模型缩放`, `默认模板`, `激活魂力消耗`, `进化魂力消耗`, `副本Boss魂力`, `世界Boss魂力`, "
            "`魂环等级上限`, `最大继承百分比`, `目标同步间隔毫秒`, `属性刷新间隔毫秒`, "
            "`跟随距离`, `超距召回距离`, `自动协助半径` "
            "FROM `_武魂系统_模板` ORDER BY `默认模板` DESC, `模板ID`");

        if (!result)
            return false;

        bool paramsLoaded = false;
        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            if (!id)
                continue;

            WuhunDefinition def;
            def.id = id;
            def.name = fields[1].Get<std::string>();
            def.creatureEntry = fields[2].Get<uint32>();
            def.scale = fields[3].Get<float>();
            if (!def.creatureEntry)
                def.creatureEntry = GetUIntParam("AVATAR_ENTRY", 930001);
            if (def.name.empty())
                def.name = "本命武魂";
            if (def.scale <= 0.0f)
                def.scale = 1.0f;
            def.activationSoulCost = fields[5].Get<uint128>();
            def.evolveSoulCost = fields[6].Get<uint128>();
            def.dungeonBossSoulPower = std::max<uint128>(uint128(1), fields[7].Get<uint128>());
            def.worldBossSoulPower = std::max<uint128>(uint128(1), fields[8].Get<uint128>());

            _definitions[id] = def;

            bool isDefault = fields[4].Get<uint8>() != 0;
            if (!paramsLoaded || isDefault)
            {
                _intParams["DEFAULT_WUHUN_ID"] = id;
                _intParams["AVATAR_ENTRY"] = def.creatureEntry;
                _uint128Params["ACTIVATION_SOUL_COST"] = def.activationSoulCost;
                _uint128Params["DUNGEON_BOSS_SOUL_POWER"] = def.dungeonBossSoulPower;
                _uint128Params["WORLD_BOSS_SOUL_POWER"] = def.worldBossSoulPower;
                _intParams["MAX_RING_LEVEL"] = fields[9].Get<int64>();
                _intParams["MAX_INHERIT_PERCENT"] = fields[10].Get<int64>();
                _intParams["TARGET_SYNC_INTERVAL"] = fields[11].Get<int64>();
                _intParams["STAT_REFRESH_INTERVAL"] = fields[12].Get<int64>();
                _floatParams["FOLLOW_DISTANCE"] = fields[13].Get<float>();
                _floatParams["RECALL_DISTANCE"] = fields[14].Get<float>();
                _floatParams["AUTO_ASSIST_RADIUS"] = fields[15].Get<float>();
                paramsLoaded = true;
            }
        } while (result->NextRow());

        return true;
    }

    bool LoadRingTemplates()
    {
        if (!HasWorldTable("_武魂系统_魂环"))
            return false;

        bool hasInheritBonus = HasWorldColumn("_武魂系统_魂环", "每级继承加成");
        QueryResult result = WorldDatabase.Query(
            hasInheritBonus
                ? "SELECT `等级`, `魂力消耗`, `每级继承加成` FROM `_武魂系统_魂环` ORDER BY `等级`"
                : "SELECT `等级`, `魂力消耗` FROM `_武魂系统_魂环` ORDER BY `等级`");

        if (!result)
            return false;

        do
        {
            Field* fields = result->Fetch();
            uint32 level = fields[0].Get<uint32>();
            uint128 soulCost = fields[1].Get<uint128>();
            if (level)
            {
                _ringCosts[level] = soulCost > 0 ? soulCost : DefaultRingSoulCost(level);
                _ringInheritBonuses[level] = hasInheritBonus ? std::max<float>(0.0f, fields[2].Get<float>()) : static_cast<float>(level);
            }
        } while (result->NextRow());

        return true;
    }

    bool LoadSkillTemplates()
    {
        if (!HasWorldTable("_武魂系统_技能"))
            return false;

        std::string damageBonusColumn;
        if (HasWorldColumn("_武魂系统_技能", "每级伤害加成"))
            damageBonusColumn = "`每级伤害加成`";
        else
            damageBonusColumn = "1";

        std::string maxLevelColumn = HasWorldColumn("_武魂系统_技能", "技能等级上限") ? "`技能等级上限`" : std::to_string(GetUIntParam("MAX_SKILL_LEVEL", 10));

        std::string query =
            "SELECT `技能ID`, `名称`, `技能SpellID`, `冷却毫秒`, `学习消耗`, `升级消耗`, "
            + maxLevelColumn + ", `目标类型`, `低血量阈值`, `施法距离`, " + damageBonusColumn + ", `描述` "
            "FROM `_武魂系统_技能` ORDER BY `技能ID`";

        QueryResult result = WorldDatabase.Query(query.c_str());

        if (!result)
            return false;

        do
        {
            Field* fields = result->Fetch();
            uint32 skillId = fields[0].Get<uint32>();
            if (!skillId)
                continue;

            WuhunSkillTemplate skill;
            skill.skillId = skillId;
            skill.name = fields[1].Get<std::string>();
            skill.spellId = fields[2].Get<uint32>();
            skill.cooldownMs = fields[3].Get<uint32>();
            skill.learnCost = fields[4].Get<uint128>();
            skill.upgradeCost = fields[5].Get<uint128>();
            skill.maxLevel = fields[6].Get<uint32>();
            skill.targetType = std::min<uint8>(fields[7].Get<uint8>(), 2);
            skill.minTargetHealthPct = std::min<uint8>(fields[8].Get<uint8>(), 100);
            skill.range = fields[9].Get<float>();
            skill.damageBonusPerLevel = fields[10].Get<float>();
            skill.description = fields[11].Get<std::string>();

            if (skill.name.empty())
                skill.name = "武魂技能" + std::to_string(skillId);
            if (!skill.cooldownMs)
                skill.cooldownMs = 5000;
            if (!skill.learnCost)
                skill.learnCost = 100;
            if (!skill.upgradeCost)
                skill.upgradeCost = 100;
            if (!skill.maxLevel)
                skill.maxLevel = GetUIntParam("MAX_SKILL_LEVEL", 10);
            if (skill.range <= 0.0f)
                skill.range = 30.0f;
            if (skill.damageBonusPerLevel <= 0.0f)
                skill.damageBonusPerLevel = 1.0f;

            _skills[skillId] = skill;
        } while (result->NextRow());

        LOG_INFO("server.loading", "武魂系统: 从 `_武魂系统_技能` 加载技能模板 {} 个", _skills.size());
        return true;
    }

    bool LoadEquipSlotTemplates()
    {
        if (!HasWorldTable("_武魂系统_装备"))
            return false;

        QueryResult result = WorldDatabase.Query(
            "SELECT `槽位`, `名称`, `魂力消耗`, `需求模板ID` FROM `_武魂系统_装备` ORDER BY `槽位`");

        if (!result)
            return false;

        do
        {
            Field* fields = result->Fetch();
            uint32 slotId = fields[0].Get<uint32>();
            if (slotId >= EQUIPMENT_SLOT_END)
                continue;

            WuhunEquipSlotConfig config;
            config.slot = static_cast<uint8>(slotId);
            config.name = fields[1].Get<std::string>();
            config.soulCost = fields[2].Get<uint128>();
            config.requirementId = fields[3].Get<uint32>();
            if (config.name.empty())
                config.name = GetWuhunEquipmentSlotName(config.slot);
            _equipSlotConfigs[config.slot] = config;
        } while (result->NextRow());

        return true;
    }

    void SeedBuiltInDefaults()
    {
        _intParams["DEFAULT_WUHUN_ID"] = 1;
        _intParams["AVATAR_ENTRY"] = 930001;
        _intParams["TARGET_SYNC_INTERVAL"] = 250;
        _intParams["STAT_REFRESH_INTERVAL"] = 3000;
        _intParams["MAX_RING_LEVEL"] = 100;
        _intParams["MAX_SKILL_LEVEL"] = 10;
        _intParams["MAX_INHERIT_PERCENT"] = 900;
        _intParams["AVATAR_SKILL_AI"] = 1;
        _uint128Params["DUNGEON_BOSS_SOUL_POWER"] = 1;
        _uint128Params["WORLD_BOSS_SOUL_POWER"] = 1;
        _uint128Params["ACTIVATION_SOUL_COST"] = 1000;
        _floatParams["FOLLOW_DISTANCE"] = 2.5f;
        _floatParams["RECALL_DISTANCE"] = 55.0f;
        _floatParams["AUTO_ASSIST_RADIUS"] = 30.0f;

        WuhunDefinition def;
        _definitions[1] = def;

        for (uint32 level = 1; level <= 100; ++level)
        {
            _ringCosts[level] = DefaultRingSoulCost(level);
            _ringInheritBonuses[level] = static_cast<float>(level);
        }

        AddDefaultEquipSlot(EQUIPMENT_SLOT_HEAD, 1200);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_NECK, 1500);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_SHOULDERS, 1200);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_BODY, 500);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_CHEST, 1800);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_WAIST, 900);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_LEGS, 1500);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_FEET, 900);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_WRISTS, 800);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_HANDS, 900);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_FINGER1, 2000);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_FINGER2, 2500);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_TRINKET1, 3000);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_TRINKET2, 4000);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_BACK, 1600);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_MAINHAND, 5000);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_OFFHAND, 3500);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_RANGED, 3500);
        AddDefaultEquipSlot(EQUIPMENT_SLOT_TABARD, 1000);

        AddDefaultSkill(1, "魂火", 133, 3500, 30.0f, 100, 100, 0, 0, "对主人目标释放火焰攻击");
        AddDefaultSkill(2, "冰魂箭", 116, 5000, 30.0f, 150, 120, 0, 0, "对主人目标释放寒冰攻击");
        AddDefaultSkill(3, "圣魂击", 585, 4000, 30.0f, 150, 120, 0, 0, "对主人目标释放神圣攻击");
        AddDefaultSkill(4, "暗魂蚀", 172, 8000, 30.0f, 200, 150, 0, 0, "对主人目标施加暗影伤害");
        AddDefaultSkill(5, "灼魂术", 348, 8000, 30.0f, 200, 150, 0, 0, "对主人目标施加灼烧伤害");
        AddDefaultSkill(6, "斩魂", 78, 4500, 5.0f, 250, 180, 0, 0, "近战打击主人目标");
        AddDefaultSkill(7, "魂佑", 2050, 12000, 30.0f, 300, 220, 2, 0, "自动辅助主人恢复生命");
        AddDefaultSkill(8, "魂盾", 17, 18000, 30.0f, 300, 220, 2, 0, "自动为主人提供护盾");
        AddDefaultSkill(9, "残魂收割", 53, 6000, 5.0f, 400, 300, 0, 35, "目标低生命时尝试近战终结");
    }

    void AddDefaultSkill(uint32 id, std::string const& name, uint32 spellId, uint32 cooldownMs, float range,
        uint32 learnCost, uint32 upgradeCost, uint8 targetType, uint8 minHealthPct, std::string const& description)
    {
        WuhunSkillTemplate skill;
        skill.skillId = id;
        skill.name = name;
        skill.spellId = spellId;
        skill.cooldownMs = cooldownMs;
        skill.range = range;
        skill.learnCost = learnCost;
        skill.upgradeCost = upgradeCost;
        skill.maxLevel = GetUIntParam("MAX_SKILL_LEVEL", 10);
        skill.targetType = targetType;
        skill.minTargetHealthPct = minHealthPct;
        skill.damageBonusPerLevel = 1.0f;
        skill.description = description;
        _skills[id] = skill;
    }

    void AddDefaultEquipSlot(uint8 slot, uint128 const& soulCost)
    {
        WuhunEquipSlotConfig config;
        config.slot = slot;
        config.name = GetWuhunEquipmentSlotName(slot);
        config.soulCost = soulCost;
        config.requirementId = 0;
        _equipSlotConfigs[slot] = config;
    }

    PlayerWuhunData* EnsurePlayerData(Player* player)
    {
        if (!player)
            return nullptr;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerWuhunData* data = GetPlayerData(playerGuid);
        if (data)
            return data;

        LoadPlayerData(player);
        return GetPlayerData(playerGuid);
    }

    PlayerWuhunData* GetPlayerData(uint32 playerGuid)
    {
        auto itr = _players.find(playerGuid);
        return itr != _players.end() ? &itr->second : nullptr;
    }

    WuhunDefinition const* GetDefinition(uint32 wuhunId) const
    {
        auto itr = _definitions.find(wuhunId);
        if (itr != _definitions.end())
            return &itr->second;

        itr = _definitions.find(1);
        return itr != _definitions.end() ? &itr->second : nullptr;
    }

    WuhunDefinition const* GetNextDefinition(uint32 wuhunId) const
    {
        WuhunDefinition const* next = nullptr;
        for (auto const& pair : _definitions)
        {
            if (pair.first <= wuhunId)
                continue;

            if (!next || pair.first < next->id)
                next = &pair.second;
        }

        return next;
    }

    uint32 GetUIntParam(std::string const& key, uint32 defaultValue) const
    {
        auto itr = _intParams.find(key);
        if (itr == _intParams.end() || itr->second < 0)
            return defaultValue;
        return static_cast<uint32>(itr->second);
    }

    uint128 GetUInt128Param(std::string const& key, uint128 const& defaultValue) const
    {
        auto itr = _uint128Params.find(key);
        if (itr == _uint128Params.end())
            return defaultValue;
        return itr->second;
    }

    float GetFloatParam(std::string const& key, float defaultValue) const
    {
        auto itr = _floatParams.find(key);
        if (itr == _floatParams.end())
            return defaultValue;
        return itr->second;
    }

    uint128 GetRingCost(uint32 level) const
    {
        auto itr = _ringCosts.find(level);
        if (itr != _ringCosts.end())
            return itr->second;
        return DefaultRingSoulCost(level);
    }

    float GetRingInheritBonus(uint32 level) const
    {
        auto itr = _ringInheritBonuses.find(level);
        if (itr != _ringInheritBonuses.end())
            return itr->second;
        return 0.0f;
    }

    WuhunEquipSlotConfig const* GetEquipSlotConfig(uint8 slot) const
    {
        auto itr = _equipSlotConfigs.find(slot);
        return itr != _equipSlotConfigs.end() ? &itr->second : nullptr;
    }

    bool IsEquipSlotUnlocked(PlayerWuhunData const& data, uint8 slot) const
    {
        return slot < EQUIPMENT_SLOT_END && data.unlockedEquipSlots[slot];
    }

    float GetInheritancePercent(PlayerWuhunData const& data) const
    {
        uint32 highestRingLevel = 0;
        for (uint32 ringLevel : data.ringLevels)
            highestRingLevel = std::max(highestRingLevel, ringLevel);

        float percent = GetRingInheritBonus(highestRingLevel);

        float maxPercent = static_cast<float>(GetUIntParam("MAX_INHERIT_PERCENT", 900));
        return std::min(percent, maxPercent);
    }

    bool IsLegalOwnerTarget(Player* player, Unit* target) const
    {
        return player
            && target
            && target != player
            && target->IsAlive()
            && target->IsInWorld()
            && player->IsValidAttackTarget(target);
    }

    void ReloadEquipDataFromDB(Player* player)
    {
        if (!player)
            return;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return;

        ClearEquipmentItems(*data);
        data->unlockedEquipSlots.fill(false);
        data->equipSlotUnlockTimes.fill(0);

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `槽位`, `解锁时间` FROM `_玩家武魂装备` WHERE `角色id` = {}", player->GetGUID().GetCounter()))
        {
            do
            {
                Field* fields = result->Fetch();
                uint8 slot = fields[0].Get<uint8>();
                uint32 unlockTime = fields[1].Get<uint32>();
                if (slot < EQUIPMENT_SLOT_END && unlockTime)
                {
                    data->unlockedEquipSlots[slot] = true;
                    data->equipSlotUnlockTimes[slot] = unlockTime;
                }
            } while (result->NextRow());
        }

        LoadWuhunItems(player);
        UpdateAvatarStats(player);
    }

    void LoadWuhunItems(Player* player)
    {
        if (!player)
            return;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return;

        data->equipment.clear();

        if (QueryResult stale = CharacterDatabase.Query(
            "SELECT ci.`slot`, ci.`item` "
            "FROM `character_inventory` ci "
            "LEFT JOIN `item_instance` ii ON ci.`item` = ii.`guid` "
            "LEFT JOIN `_玩家武魂装备` we ON we.`角色id` = ci.`guid` AND we.`槽位` = ci.`slot` "
            "AND we.`物品GUID` = ci.`item` "
            "WHERE ci.`guid` = {} AND ci.`bag` = {} "
            "AND (we.`角色id` IS NULL OR we.`解锁时间` = 0 OR we.`物品ID` = 0 OR ii.`guid` IS NULL OR we.`物品ID` <> ii.`itemEntry`)",
            player->GetGUID().GetCounter(), WUHUN_VIRTUAL_BAG))
        {
            do
            {
                Field* fields = stale->Fetch();
                uint8 slot = fields[0].Get<uint8>();
                ObjectGuid::LowType itemGuid = static_cast<ObjectGuid::LowType>(fields[1].Get<uint64>());
                CharacterDatabase.Execute("DELETE FROM `character_inventory` WHERE `guid` = {} AND `bag` = {} AND `slot` = {}",
                    player->GetGUID().GetCounter(), WUHUN_VIRTUAL_BAG, slot);
                CharacterDatabase.Execute("DELETE FROM `item_instance` WHERE `guid` = {}", itemGuid);
            } while (stale->NextRow());
        }

        QueryResult result = CharacterDatabase.Query(
            "SELECT ci.`slot`, ci.`item`, ii.`itemEntry`, ii.`creatorGuid`, ii.`giftCreatorGuid`, ii.`count`, "
            "ii.`duration`, ii.`charges`, ii.`flags`, ii.`enchantments`, ii.`randomPropertyId`, ii.`durability`, "
            "ii.`playedTime`, ii.`text` "
            "FROM `character_inventory` ci "
            "JOIN `item_instance` ii ON ci.`item` = ii.`guid` "
            "JOIN `_玩家武魂装备` we ON we.`角色id` = ci.`guid` AND we.`槽位` = ci.`slot` "
            "AND we.`物品GUID` = ci.`item` AND we.`物品ID` = ii.`itemEntry` "
            "WHERE ci.`guid` = {} AND ci.`bag` = {} AND we.`解锁时间` > 0 AND we.`物品ID` > 0",
            player->GetGUID().GetCounter(), WUHUN_VIRTUAL_BAG);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint8 slot = fields[0].Get<uint8>();
            ObjectGuid::LowType itemGuid = static_cast<ObjectGuid::LowType>(fields[1].Get<uint64>());
            uint32 itemEntry = fields[2].Get<uint32>();

            if (slot >= EQUIPMENT_SLOT_END)
            {
                LOG_ERROR("module", "武魂系统: 玩家 {} 存在无效武魂装备槽位 slot={} itemGuid={}", player->GetName(), slot, itemGuid);
                continue;
            }

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
            if (!proto)
            {
                CharacterDatabase.Execute("DELETE FROM `character_inventory` WHERE `guid` = {} AND `bag` = {} AND `slot` = {}",
                    player->GetGUID().GetCounter(), WUHUN_VIRTUAL_BAG, slot);
                CharacterDatabase.Execute("DELETE FROM `item_instance` WHERE `guid` = {}", itemGuid);
                continue;
            }

            Item* item = NewItemOrBag(proto);
            if (!item)
                continue;

            if (!item->LoadFromDB(itemGuid, player->GetGUID(), fields + 3, itemEntry))
            {
                delete item;
                CharacterDatabase.Execute("DELETE FROM `character_inventory` WHERE `guid` = {} AND `bag` = {} AND `slot` = {}",
                    player->GetGUID().GetCounter(), WUHUN_VIRTUAL_BAG, slot);
                CharacterDatabase.Execute("DELETE FROM `item_instance` WHERE `guid` = {}", itemGuid);
                continue;
            }

            item->SetOwnerGUID(ObjectGuid::Empty);
            item->FSetState(ITEM_UNCHANGED);

            WuhunEquipmentSlot slotData;
            slotData.itemEntry = itemEntry;
            slotData.itemGuid = itemGuid;
            slotData.item = item;
            data->equipment[slot] = slotData;
            data->unlockedEquipSlots[slot] = true;
            if (!data->equipSlotUnlockTimes[slot])
                data->equipSlotUnlockTimes[slot] = static_cast<uint32>(GameTime::GetGameTime().count());
        } while (result->NextRow());
    }

    void ClearEquipmentItems(PlayerWuhunData& data)
    {
        for (auto& pair : data.equipment)
        {
            if (pair.second.item)
            {
                delete pair.second.item;
                pair.second.item = nullptr;
            }
        }
        data.equipment.clear();
    }

    void ApplyWuhunItemStat(WuhunEquipmentBonus& bonus, uint32 statType, int128 const& rawValue) const
    {
        if (rawValue <= 0)
            return;

        int128 value = rawValue;
        uint128 unsignedValue = Acore::Number::ToUInt128Saturated(value);
        int64 legacyValue = Acore::Number::ToInt64Saturated(value);
        double numericValue = Acore::Number::ToDouble(value);

        switch (statType)
        {
            case ITEM_MOD_STRENGTH:
                bonus.stats[STAT_STRENGTH] = SaturatingAddInt128(bonus.stats[STAT_STRENGTH], value);
                bonus.attackPower += numericValue * 2.0;
                break;
            case ITEM_MOD_AGILITY:
                bonus.stats[STAT_AGILITY] = SaturatingAddInt128(bonus.stats[STAT_AGILITY], value);
                bonus.attackPower += numericValue;
                bonus.rangedAttackPower += numericValue;
                break;
            case ITEM_MOD_STAMINA:
                bonus.stats[STAT_STAMINA] = SaturatingAddInt128(bonus.stats[STAT_STAMINA], value);
                bonus.health = SaturatingAddUInt128(bonus.health, SaturatingMultiplyUInt128(unsignedValue, 10));
                break;
            case ITEM_MOD_INTELLECT:
                bonus.stats[STAT_INTELLECT] = SaturatingAddInt128(bonus.stats[STAT_INTELLECT], value);
                bonus.mana = SaturatingAddUInt128(bonus.mana, SaturatingMultiplyUInt128(unsignedValue, 15));
                bonus.spellPower += numericValue * 0.25;
                break;
            case ITEM_MOD_SPIRIT:
                bonus.stats[STAT_SPIRIT] = SaturatingAddInt128(bonus.stats[STAT_SPIRIT], value);
                bonus.mana = SaturatingAddUInt128(bonus.mana, SaturatingMultiplyUInt128(unsignedValue, 5));
                break;
            case ITEM_MOD_HEALTH:
                bonus.health = SaturatingAddUInt128(bonus.health, unsignedValue);
                break;
            case ITEM_MOD_MANA:
                bonus.mana = SaturatingAddUInt128(bonus.mana, unsignedValue);
                break;
            case ITEM_MOD_ATTACK_POWER:
                bonus.attackPower += numericValue;
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                bonus.rangedAttackPower += numericValue;
                break;
            case ITEM_MOD_SPELL_POWER:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
            case ITEM_MOD_SPELL_HEALING_DONE:
                bonus.spellPower += numericValue;
                break;
            case ITEM_MOD_TRUE_DAMAGE:
            case ITEM_MOD_CUTTING_DAMAGE:
            case ITEM_MOD_SKILL_DAMAGE:
                bonus.attackPower += numericValue * 2.0;
                bonus.rangedAttackPower += numericValue * 2.0;
                bonus.spellPower += numericValue;
                bonus.minDamage += numericValue * 0.35;
                bonus.maxDamage += numericValue * 0.55;
                break;
            case ITEM_MOD_COOLDOWN_REDUCTION:
                bonus.spellPower += numericValue;
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
            case ITEM_MOD_DODGE_RATING:
            case ITEM_MOD_PARRY_RATING:
            case ITEM_MOD_BLOCK_RATING:
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
            case ITEM_MOD_HIT_TAKEN_RATING:
            case ITEM_MOD_CRIT_TAKEN_RATING:
            case ITEM_MOD_RESILIENCE_RATING:
                bonus.armor = SaturatingAddUInt128(bonus.armor, SaturatingMultiplyUInt128(unsignedValue, 4));
                bonus.health = SaturatingAddUInt128(bonus.health, SaturatingMultiplyUInt128(unsignedValue, 5));
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
            case ITEM_MOD_HIT_RANGED_RATING:
            case ITEM_MOD_HIT_SPELL_RATING:
            case ITEM_MOD_CRIT_MELEE_RATING:
            case ITEM_MOD_CRIT_RANGED_RATING:
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_HIT_RATING:
            case ITEM_MOD_CRIT_RATING:
            case ITEM_MOD_EXPERTISE_RATING:
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                bonus.attackPower += numericValue;
                bonus.rangedAttackPower += numericValue;
                bonus.spellPower += numericValue * 0.5;
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                bonus.meleeHasteRating = SaturatingAddInt64(bonus.meleeHasteRating, legacyValue);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                bonus.rangedHasteRating = SaturatingAddInt64(bonus.rangedHasteRating, legacyValue);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                bonus.spellHasteRating = SaturatingAddInt64(bonus.spellHasteRating, legacyValue);
                break;
            case ITEM_MOD_HASTE_RATING:
                bonus.meleeHasteRating = SaturatingAddInt64(bonus.meleeHasteRating, legacyValue);
                bonus.rangedHasteRating = SaturatingAddInt64(bonus.rangedHasteRating, legacyValue);
                bonus.spellHasteRating = SaturatingAddInt64(bonus.spellHasteRating, legacyValue);
                break;
            case ITEM_MOD_MANA_REGENERATION:
                bonus.mana = SaturatingAddUInt128(bonus.mana, SaturatingMultiplyUInt128(unsignedValue, 5));
                bonus.spellPower += numericValue * 0.5;
                break;
            case ITEM_MOD_HEALTH_REGEN:
                bonus.health = SaturatingAddUInt128(bonus.health, SaturatingMultiplyUInt128(unsignedValue, 10));
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                bonus.spellPower += numericValue;
                break;
            case ITEM_MOD_BLOCK_VALUE:
                bonus.armor = SaturatingAddUInt128(bonus.armor, unsignedValue);
                break;
            default:
                break;
        }
    }

#ifdef WUHUN_HAS_ITEM_ATTRIBUTES
    void ApplyWuhunItemAttributeRows(WuhunEquipmentBonus& bonus, std::vector<uint32> const& attributeIds, std::vector<int128> const& attributeValues, WuhunItemAttributeMultiplier const& multiplier) const
    {
        size_t count = std::min(attributeIds.size(), attributeValues.size());
        for (size_t i = 0; i < count; ++i)
            ApplyWuhunItemStat(bonus, attributeIds[i], CalculateWuhunEnhancedAttributeValue(attributeValues[i], multiplier));
    }
#endif

    WuhunItemAttributeMultiplier GetWuhunItemAttributeMultiplier(ObjectGuid::LowType itemGuid) const
    {
        WuhunItemAttributeMultiplier result;
        if (!itemGuid)
            return result;

        QueryResult queryResult = CharacterDatabase.Query(
            "SELECT `属性倍率`, `属性倍率模式` FROM `玩家装备属性增强` WHERE `装备GUID` = {} LIMIT 1",
            static_cast<uint64>(itemGuid));
        if (!queryResult)
            return result;

        Field* fields = queryResult->Fetch();
        float value = static_cast<float>(fields[0].Get<int32>());
        char mode = DbValueToWuhunItemAttributeMode(fields[1].Get<int32>());
        if (!HasWuhunItemAttributeMultiplierEffect(value, mode))
            return result;

        result.value = value;
        result.mode = mode;
        result.active = true;
        return result;
    }

    WuhunEquipmentBonus CalculateEquipmentBonus(PlayerWuhunData const& data) const
    {
        WuhunEquipmentBonus bonus;

        for (auto const& pair : data.equipment)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(pair.second.itemEntry);
            if (!proto)
                continue;

            ++bonus.itemCount;
            WuhunItemAttributeMultiplier multiplier = GetWuhunItemAttributeMultiplier(pair.second.itemGuid);
            if (multiplier.active)
                ++bonus.enhancedItemCount;

            bonus.armor = SaturatingAddUInt128(bonus.armor, CalculateWuhunEnhancedAttributeValue(proto->Armor128, multiplier));
            for (uint32 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
                ApplyWuhunItemStat(bonus, proto->ItemStat[i].ItemStatType, CalculateWuhunEnhancedAttributeValue(proto->ItemStatValue128[i], multiplier));

            for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
            {
                bonus.minDamage += CalculateWuhunEnhancedAttributeValue(static_cast<float>(proto->Damage[i].DamageMin), multiplier);
                bonus.maxDamage += CalculateWuhunEnhancedAttributeValue(static_cast<float>(proto->Damage[i].DamageMax), multiplier);
            }

#ifdef WUHUN_HAS_ITEM_ATTRIBUTES
            if (pair.second.itemGuid)
            {
                if (auto attributeData = ItemAttributesDBHelper::LoadItemAttributes(pair.second.itemGuid))
                {
                    ApplyWuhunItemAttributeRows(bonus, attributeData->baseAttributeIds, attributeData->baseAttributeValues, multiplier);
                    ApplyWuhunItemAttributeRows(bonus, attributeData->additionalAttributeIds, attributeData->additionalAttributeValues, multiplier);
                }
            }
#endif
        }

        return bonus;
    }

    void UpdateAvatarVisuals(Player* player, Creature* avatar)
    {
        if (!player || !avatar)
            return;

        PlayerWuhunData* data = GetPlayerData(player->GetGUID().GetCounter());
        auto getWuhunItemEntry = [data](uint8 slot) -> uint32
        {
            if (!data)
                return 0;
            auto itr = data->equipment.find(slot);
            return itr != data->equipment.end() ? itr->second.itemEntry : 0;
        };

        uint32 mainHand = getWuhunItemEntry(EQUIPMENT_SLOT_MAINHAND);
        uint32 offHand = getWuhunItemEntry(EQUIPMENT_SLOT_OFFHAND);
        uint32 ranged = getWuhunItemEntry(EQUIPMENT_SLOT_RANGED);

        if (!mainHand)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
                mainHand = item->GetEntry();
        if (!offHand)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
                offHand = item->GetEntry();
        if (!ranged)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
                ranged = item->GetEntry();

        avatar->SetVirtualItem(0, mainHand);
        avatar->SetVirtualItem(1, offHand);
        avatar->SetVirtualItem(2, ranged);
    }

    void SendPayload(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        if (payload.length() <= WUHUN_MAX_ADDON_PAYLOAD)
        {
            std::string fullMessage = std::string(WUHUN_ADDON_PREFIX) + '\t' + payload;
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
            return;
        }

        size_t totalChunks = (payload.length() + WUHUN_MAX_ADDON_PAYLOAD - 1) / WUHUN_MAX_ADDON_PAYLOAD;
        for (size_t i = 0; i < totalChunks; ++i)
        {
            size_t start = i * WUHUN_MAX_ADDON_PAYLOAD;
            size_t len = std::min(WUHUN_MAX_ADDON_PAYLOAD, payload.length() - start);
            std::string chunk = payload.substr(start, len);

            std::ostringstream chunkMessage;
            chunkMessage << "CHUNK:" << (i + 1) << ':' << totalChunks << ':' << chunk;

            std::string fullMessage = std::string(WUHUN_ADDON_PREFIX) + '\t' + chunkMessage.str();
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
        }
    }

    std::unordered_map<uint32, WuhunDefinition> _definitions;
    std::unordered_map<uint32, WuhunSkillTemplate> _skills;
    std::unordered_map<uint32, uint128> _ringCosts;
    std::unordered_map<uint32, float> _ringInheritBonuses;
    std::unordered_map<uint8, WuhunEquipSlotConfig> _equipSlotConfigs;
    std::unordered_map<std::string, int64> _intParams;
    std::unordered_map<std::string, uint128> _uint128Params;
    std::unordered_map<std::string, float> _floatParams;
    std::unordered_map<uint32, PlayerWuhunData> _players;
};

#define sWuhunMgr WuhunSystemMgr::Instance()

class WuhunAvatarAI : public CreatureAI
{
public:
    explicit WuhunAvatarAI(Creature* creature) : CreatureAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        sWuhunMgr->ForgetAvatar(me->GetOwnerGUID(), me->GetGUID());
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType /*damageType*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (!victim || !victim->IsCreature() || !damage)
            return;

        Player* owner = ObjectAccessor::FindPlayer(me->GetOwnerGUID());
        if (!owner || !owner->IsInMap(victim))
            return;

        Creature* creature = victim->ToCreature();
        if (!creature->hasLootRecipient())
            creature->SetLootRecipient(owner);

        creature->LowerPlayerDamageReq(Acore::Number::ToUInt64Saturated(std::min<uint128>(victim->GetHealthForCombat128(), uint128(damage))), true);
    }

    void KilledUnit(Unit* victim) override
    {
        if (!victim || !victim->IsCreature())
            return;

        Creature* creature = victim->ToCreature();
        if (Player* owner = ObjectAccessor::FindPlayer(me->GetOwnerGUID()))
            if (owner->IsInMap(creature) && creature->isTappedBy(owner))
                sWuhunMgr->OnBossKilled(owner, creature);
    }

    void UpdateAI(uint32 diff) override
    {
        UpdateCooldowns(diff);
        UpdateDebugTimers(diff);

        if (!sWuhunMgr->IsEnabled())
        {
            me->DespawnOrUnsummon();
            return;
        }

        Player* owner = ObjectAccessor::FindPlayer(me->GetOwnerGUID());
        if (!owner || !owner->IsInWorld())
        {
            me->DespawnOrUnsummon();
            return;
        }

        if (!me->HasUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED))
            me->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);

        if (!owner->IsAlive())
        {
            me->AttackStop();
            me->DespawnOrUnsummon();
            return;
        }

        if (me->GetMapId() != owner->GetMapId())
        {
            me->DespawnOrUnsummon();
            return;
        }

        SyncOwnerFlightState(owner);
        bool const ownerFlying = IsOwnerFlying(owner);
        if (ownerFlying && !_flightSnapTimer && owner->HasUnitMovementFlag(MOVEMENTFLAG_ASCENDING) && owner->GetPositionZ() - me->GetPositionZ() > 5.0f)
        {
            SnapToOwnerFlightHeight(owner);
            UpdateOwnerFlightFollow(owner, 0);
        }

        float recallDistance = sWuhunMgr->GetRecallDistance();
        if (!me->IsWithinDistInMap(owner, recallDistance))
        {
            float angle = owner->GetOrientation() + WUHUN_FOLLOW_ANGLE;
            float x = owner->GetPositionX() + std::cos(angle) * 2.0f;
            float y = owner->GetPositionY() + std::sin(angle) * 2.0f;
            me->NearTeleportTo(x, y, owner->GetPositionZ(), owner->GetOrientation());
            if (ownerFlying)
            {
                _hasFlightFollowDest = false;
                _flightFollowTimer = 0;
                UpdateOwnerFlightFollow(owner, 0);
            }
            else
                me->GetMotionMaster()->MoveFollow(owner, sWuhunMgr->GetFollowDistance(), WUHUN_FOLLOW_ANGLE);
        }

        Unit* target = sWuhunMgr->ResolveAvatarTarget(owner, me);
        if (!target)
        {
            if (CanLogAiDebug())
            {
                LOG_INFO("server.loading",
                    "武魂系统: 武魂AI无目标，主人={}，分身GUID={}，主人战斗={}，分身战斗={}，技能摘要={}",
                    owner->GetName(),
                    me->GetGUID().ToString(),
                    owner->IsInCombat() ? 1 : 0,
                    me->IsInCombat() ? 1 : 0,
                    sWuhunMgr->BuildSkillDebugSummary(owner->GetGUID().GetCounter()));
            }

            if (me->GetVictim())
                me->AttackStop();

            if (ownerFlying)
            {
                UpdateOwnerFlightFollow(owner, diff);
                return;
            }

            if (!me->IsWithinDistInMap(owner, sWuhunMgr->GetFollowDistance() + 1.5f))
                me->GetMotionMaster()->MoveFollow(owner, sWuhunMgr->GetFollowDistance(), WUHUN_FOLLOW_ANGLE);
            return;
        }

        if (ownerFlying)
        {
            if (me->GetVictim())
                me->AttackStop();

            UpdateOwnerFlightFollow(owner, diff);
            if (sWuhunMgr->IsAutoSkillMode(owner))
                TryCastSkills(owner, target, diff);
            return;
        }

        bool autoSkillMode = sWuhunMgr->IsAutoSkillMode(owner);
        if (!autoSkillMode && sWuhunMgr->IsRangedMirrorTarget(owner, target))
        {
            if (me->GetVictim())
                me->AttackStop();

            if (ownerFlying)
            {
                UpdateOwnerFlightFollow(owner, diff);
                return;
            }

            if (!me->IsWithinDistInMap(owner, sWuhunMgr->GetFollowDistance() + 1.5f))
                me->GetMotionMaster()->MoveFollow(owner, sWuhunMgr->GetFollowDistance(), WUHUN_FOLLOW_ANGLE);

            if (CanLogAiDebug())
            {
                LOG_INFO("server.loading",
                    "武魂系统: 武魂远程同步模式，主人={}，目标={}，距离={:.2f}，不进入近战普攻",
                    owner->GetName(),
                    target->GetName(),
                    me->GetDistance(target));
            }
            return;
        }

        if (me->GetVictim() != target)
        {
            me->AttackStop();
            AttackStart(target);
        }

        if (!me->GetVictim())
            me->Attack(target, true);

        if (autoSkillMode)
            TryCastSkills(owner, target, diff);
        DoMeleeAttackIfReady();
    }

private:
    bool IsOwnerFlying(Player* owner) const
    {
        return owner && owner->CanFly() &&
            (owner->HasUnitMovementFlag(MOVEMENTFLAG_FLYING) ||
                owner->HasUnitMovementFlag(MOVEMENTFLAG_ASCENDING) ||
                owner->HasUnitMovementFlag(MOVEMENTFLAG_DESCENDING) ||
                owner->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY));
    }

    void SyncOwnerFlightState(Player* owner)
    {
        if (!owner)
            return;

        bool const ownerFlying = IsOwnerFlying(owner);
        bool changed = false;

        SyncOwnerMovementSpeed(owner, ownerFlying);

        if (me->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY) != ownerFlying)
            changed = me->SetCanFly(ownerFlying) || changed;

        if (me->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY) != ownerFlying)
            changed = me->SetDisableGravity(ownerFlying) || changed;

        uint32 desiredFlightMoveFlags = 0;
        if (ownerFlying)
            desiredFlightMoveFlags |= MOVEMENTFLAG_FLYING;

        uint32 const currentFlightMoveFlags = me->GetUnitMovementFlags() &
            MOVEMENTFLAG_FLYING;
        if (currentFlightMoveFlags != desiredFlightMoveFlags)
        {
            me->RemoveUnitMovementFlag(MOVEMENTFLAG_FLYING | MOVEMENTFLAG_ASCENDING | MOVEMENTFLAG_DESCENDING);
            if (desiredFlightMoveFlags)
                me->AddUnitMovementFlag(desiredFlightMoveFlags);
            changed = true;
        }

        if (!ownerFlying && (changed || _hasFlightFollowDest || std::fabs(me->GetPositionZ() - owner->GetPositionZ()) > 3.0f))
        {
            _hasFlightFollowDest = false;
            _flightFollowTimer = 0;

            float angle = owner->GetOrientation() + WUHUN_FOLLOW_ANGLE;
            float x = owner->GetPositionX() + std::cos(angle) * sWuhunMgr->GetFollowDistance();
            float y = owner->GetPositionY() + std::sin(angle) * sWuhunMgr->GetFollowDistance();
            me->StopMoving();
            me->NearTeleportTo(x, y, owner->GetPositionZ(), owner->GetOrientation());
            me->GetMotionMaster()->MoveFollow(owner, sWuhunMgr->GetFollowDistance(), WUHUN_FOLLOW_ANGLE);
            return;
        }

        if (changed)
        {
            me->StopMoving();
            _hasFlightFollowDest = false;
            _flightFollowTimer = 0;
            if (ownerFlying)
            {
                SnapToOwnerFlightHeight(owner);
                UpdateOwnerFlightFollow(owner, 0);
            }
            else
                me->GetMotionMaster()->MoveFollow(owner, sWuhunMgr->GetFollowDistance(), WUHUN_FOLLOW_ANGLE);
        }
    }

    void SnapToOwnerFlightHeight(Player* owner)
    {
        if (!owner)
            return;

        float angle = owner->GetOrientation() + WUHUN_FOLLOW_ANGLE;
        float x = owner->GetPositionX() + std::cos(angle) * sWuhunMgr->GetFollowDistance();
        float y = owner->GetPositionY() + std::sin(angle) * sWuhunMgr->GetFollowDistance();
        me->StopMoving();
        me->NearTeleportTo(x, y, owner->GetPositionZ(), owner->GetOrientation());
        _hasFlightFollowDest = false;
        _flightFollowTimer = 0;
        _flightSnapTimer = 1000;
    }

    void UpdateOwnerFlightFollow(Player* owner, uint32 diff)
    {
        if (!owner || !IsOwnerFlying(owner))
            return;

        if (_flightFollowTimer > diff)
        {
            _flightFollowTimer -= diff;
            return;
        }

        _flightFollowTimer = 250;

        float followDistance = sWuhunMgr->GetFollowDistance();
        float angle = owner->GetOrientation() + WUHUN_FOLLOW_ANGLE;
        float x = owner->GetPositionX() + std::cos(angle) * followDistance;
        float y = owner->GetPositionY() + std::sin(angle) * followDistance;
        float z = owner->GetPositionZ();

        float dx = me->GetPositionX() - x;
        float dy = me->GetPositionY() - y;
        float dz = me->GetPositionZ() - z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < 1.0f)
            return;

        if (_hasFlightFollowDest)
        {
            float lastDx = _flightFollowX - x;
            float lastDy = _flightFollowY - y;
            float lastDz = _flightFollowZ - z;
            if ((lastDx * lastDx + lastDy * lastDy + lastDz * lastDz) < 1.0f)
                return;
        }

        _flightFollowX = x;
        _flightFollowY = y;
        _flightFollowZ = z;
        _hasFlightFollowDest = true;
        me->GetMotionMaster()->MovePoint(0, x, y, z, false, true);
    }

    void SyncOwnerMovementSpeed(Player* owner, bool ownerFlying)
    {
        if (!owner)
            return;

        float const runCatchup = ownerFlying ? 1.20f : 1.35f;
        float const flightCatchup = ownerFlying ? 1.85f : 1.35f;

        SyncOwnerMoveSpeed(owner, MOVE_RUN, runCatchup);
        SyncOwnerMoveSpeed(owner, MOVE_RUN_BACK, runCatchup);
        SyncOwnerMoveSpeed(owner, MOVE_FLIGHT, flightCatchup);
        SyncOwnerMoveSpeed(owner, MOVE_FLIGHT_BACK, flightCatchup);
    }

    void SyncOwnerMoveSpeed(Player* owner, UnitMoveType moveType, float catchupMultiplier)
    {
        float const baseSpeed = baseMoveSpeed[moveType];
        if (baseSpeed <= 0.0f)
            return;

        float const ownerSpeed = owner->GetSpeed(moveType);
        float const desiredRate = std::max(0.1f, (ownerSpeed * catchupMultiplier) / baseSpeed);
        if (std::fabs(me->GetSpeedRate(moveType) - desiredRate) < 0.01f)
            return;

        me->SetSpeed(moveType, desiredRate);
    }

    void UpdateDebugTimers(uint32 diff)
    {
        if (_aiDebugTimer > diff)
            _aiDebugTimer -= diff;
        else
            _aiDebugTimer = 0;

        if (_skillDebugTimer > diff)
            _skillDebugTimer -= diff;
        else
            _skillDebugTimer = 0;

        if (_flightSnapTimer > diff)
            _flightSnapTimer -= diff;
        else
            _flightSnapTimer = 0;
    }

    bool CanLogAiDebug(uint32 intervalMs = 3000)
    {
        if (!sWuhunMgr->IsDebug() || _aiDebugTimer)
            return false;

        _aiDebugTimer = intervalMs;
        return true;
    }

    bool CanLogSkillDebug(uint32 intervalMs = 3000)
    {
        if (!sWuhunMgr->IsDebug() || _skillDebugTimer)
            return false;

        _skillDebugTimer = intervalMs;
        return true;
    }

    void UpdateCooldowns(uint32 diff)
    {
        for (auto itr = _cooldowns.begin(); itr != _cooldowns.end(); )
        {
            if (itr->second <= diff)
                itr = _cooldowns.erase(itr);
            else
            {
                itr->second -= diff;
                ++itr;
            }
        }
    }

    void TryCastSkills(Player* owner, Unit* target, uint32 diff)
    {
        if (!owner || !target)
            return;

        if (_skillTimer > diff)
        {
            _skillTimer -= diff;
            return;
        }
        _skillTimer = WUHUN_SKILL_SCAN_INTERVAL_MS;

        std::vector<WuhunSkillRuntime> skills = sWuhunMgr->GetEquippedSkills(owner->GetGUID().GetCounter());
        bool logScan = CanLogSkillDebug();
        if (logScan)
        {
            LOG_INFO("server.loading",
                "武魂系统: 武魂技能扫描开始，主人={}，目标={}，距离={:.2f}，可用技能数={}，{}",
                owner->GetName(),
                target->GetName(),
                me->GetDistance(target),
                skills.size(),
                sWuhunMgr->BuildSkillDebugSummary(owner->GetGUID().GetCounter()));
        }

        if (skills.empty())
        {
            if (logScan)
            {
                LOG_INFO("server.loading",
                    "武魂系统: 武魂技能扫描结束，原因=没有可用技能，主人={}，技能必须在 `_玩家武魂技能` 中有等级并已装配槽位",
                    owner->GetName());
            }
            return;
        }

        size_t startIndex = _nextSkillIndex % skills.size();
        for (size_t offset = 0; offset < skills.size(); ++offset)
        {
            size_t skillIndex = (startIndex + offset) % skills.size();
            WuhunSkillRuntime const& runtime = skills[skillIndex];

            if (_cooldowns.find(runtime.skillId) != _cooldowns.end())
            {
                if (logScan)
                {
                    LOG_INFO("server.loading",
                        "武魂系统: 武魂技能跳过，原因=冷却中，主人={}，技能ID={}，剩余={}ms",
                        owner->GetName(),
                        runtime.skillId,
                        _cooldowns[runtime.skillId]);
                }
                continue;
            }

            WuhunSkillTemplate const* skill = sWuhunMgr->GetSkill(runtime.skillId);
            if (!skill)
            {
                if (logScan)
                    LOG_INFO("server.loading", "武魂系统: 武魂技能跳过，原因=技能模板不存在，主人={}，技能ID={}", owner->GetName(), runtime.skillId);
                continue;
            }

            if (!skill->spellId)
            {
                if (logScan)
                    LOG_INFO("server.loading", "武魂系统: 武魂技能跳过，原因=SpellID为空，主人={}，技能ID={}", owner->GetName(), runtime.skillId);
                continue;
            }

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skill->spellId);
            if (!spellInfo)
            {
                if (logScan)
                    LOG_INFO("server.loading", "武魂系统: 武魂技能跳过，原因=SpellInfo不存在，主人={}，技能ID={}，SpellID={}", owner->GetName(), runtime.skillId, skill->spellId);
                continue;
            }

            Unit* castTarget = target;
            if (skill->targetType == 1)
                castTarget = me;
            else if (skill->targetType == 2)
                castTarget = owner;

            if (!castTarget || !castTarget->IsAlive())
            {
                if (logScan)
                {
                    LOG_INFO("server.loading",
                        "武魂系统: 武魂技能跳过，原因=施法目标无效，主人={}，技能ID={}，SpellID={}，目标类型={}",
                        owner->GetName(),
                        runtime.skillId,
                        skill->spellId,
                        static_cast<uint32>(skill->targetType));
                }
                continue;
            }

            if (skill->targetType == 0)
            {
                if (skill->minTargetHealthPct && target->GetHealthPct() > skill->minTargetHealthPct)
                {
                    if (logScan)
                    {
                        LOG_INFO("server.loading",
                            "武魂系统: 武魂技能跳过，原因=目标血量未达阈值，主人={}，技能ID={}，目标血量={:.2f}%，阈值={}%",
                            owner->GetName(),
                            runtime.skillId,
                            target->GetHealthPct(),
                            static_cast<uint32>(skill->minTargetHealthPct));
                    }
                    continue;
                }

                if (skill->range > 0.0f && !me->IsWithinDistInMap(target, skill->range))
                {
                    if (logScan)
                    {
                        LOG_INFO("server.loading",
                            "武魂系统: 武魂技能跳过，原因=距离过远，主人={}，技能ID={}，SpellID={}，距离={:.2f}，配置距离={:.2f}",
                            owner->GetName(),
                            runtime.skillId,
                            skill->spellId,
                            me->GetDistance(target),
                            skill->range);
                    }
                    continue;
                }
            }

            TriggerCastFlags autoFlags = WUHUN_SPELL_CAST_FLAGS;
            SpellCastResult result = me->CastSpell(castTarget, spellInfo, autoFlags, nullptr, nullptr, ObjectGuid::Empty);
            if (result != SPELL_CAST_OK)
            {
                if (logScan)
                {
                    LOG_INFO("server.loading",
                        "武魂系统: 武魂技能施法失败，主人={}，技能ID={}，SpellID={}，目标={}，目标类型={}，结果={}",
                        owner->GetName(),
                        runtime.skillId,
                        skill->spellId,
                        castTarget ? castTarget->GetName() : "无",
                        static_cast<uint32>(skill->targetType),
                        static_cast<uint32>(result));
                }
                continue;
            }

            uint32 cooldown = std::max<uint32>(WUHUN_MIN_SKILL_COOLDOWN_MS, skill->cooldownMs);
            if (runtime.level > 1)
            {
                uint64 reduction = static_cast<uint64>(runtime.level - 1) * 250ULL;
                cooldown = reduction >= cooldown ? WUHUN_MIN_SKILL_COOLDOWN_MS : std::max<uint32>(WUHUN_MIN_SKILL_COOLDOWN_MS, cooldown - static_cast<uint32>(reduction));
            }
            _cooldowns[runtime.skillId] = cooldown;
            _nextSkillIndex = (skillIndex + 1) % skills.size();
            break;
        }
    }

    uint32 _skillTimer = 0;
    uint32 _aiDebugTimer = 0;
    uint32 _skillDebugTimer = 0;
    uint32 _flightFollowTimer = 0;
    uint32 _flightSnapTimer = 0;
    bool _hasFlightFollowDest = false;
    float _flightFollowX = 0.0f;
    float _flightFollowY = 0.0f;
    float _flightFollowZ = 0.0f;
    size_t _nextSkillIndex = 0;
    std::unordered_map<uint32, uint32> _cooldowns;
};

class npc_wuhun_avatar : public CreatureScript
{
public:
    npc_wuhun_avatar() : CreatureScript("npc_wuhun_avatar") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new WuhunAvatarAI(creature);
    }
};

class WuhunWorldScript : public WorldScript
{
public:
    WuhunWorldScript() : WorldScript("WuhunWorldScript") { }

    void OnUpdate(uint32 diff) override
    {
        if (_loaded)
            return;

        _loadTimer += diff;
        if (_loadTimer < 1000)
            return;

        if (!sWuhunMgr->IsEnabled())
        {
            LOG_INFO("server.loading", ">> 武魂系统模块已禁用");
            _loaded = true;
            return;
        }

        sWuhunMgr->LoadTemplates();
        LOG_INFO("server.loading", "→武魂系统√");
        _loaded = true;
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (!reload || !_loaded)
            return;

        if (!sWuhunMgr->IsEnabled())
        {
            LOG_INFO("server.loading", "武魂系统模块已禁用");
            return;
        }

        sWuhunMgr->LoadTemplates();
        LOG_INFO("server.loading", "武魂系统配置已重载");
    }

private:
    bool _loaded = false;
    uint32 _loadTimer = 0;
};

class WuhunUnitScript : public UnitScript
{
public:
    WuhunUnitScript() : UnitScript("WuhunUnitScript", true,
    {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK
    }) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, uint128& damage, SpellInfo const* spellInfo) override
    {
        if (!sWuhunMgr->IsEnabled())
            return;

        Creature* avatar = attacker ? attacker->ToCreature() : nullptr;
        sWuhunMgr->ApplySkillDamageBonus(avatar, spellInfo, damage, "direct");
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint128& damage, SpellInfo const* spellInfo) override
    {
        if (!sWuhunMgr->IsEnabled() || (spellInfo && spellInfo->IsPositive()))
            return;

        Creature* avatar = attacker ? attacker->ToCreature() : nullptr;
        sWuhunMgr->ApplySkillDamageBonus(avatar, spellInfo, damage, "periodic");
    }
};

class WuhunPlayerScript : public PlayerScript
{
public:
    WuhunPlayerScript() : PlayerScript("WuhunPlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_SPELL_CAST,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER,
        PLAYERHOOK_ON_PLAYER_JUST_DIED,
        PLAYERHOOK_ON_MAP_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !sWuhunMgr->IsEnabled())
            return;

        sWuhunMgr->LoadPlayerData(player);
        sWuhunMgr->RestoreAvatarIfRequested(player);
        if (sConfigMgr->GetOption<bool>("WuhunSystem.AnnounceOnLogin", true))
            ChatHandler(player->GetSession()).SendSysMessage("|cff66ccff[武魂系统]|r 已加载。击杀 Boss 获得魂力，可召唤并培养战斗分身。");

        sWuhunMgr->SendAll(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        sWuhunMgr->DismissAvatar(player, true, false);
        if (sWuhunMgr->IsEnabled())
            sWuhunMgr->SavePlayerData(player);
        sWuhunMgr->UnloadPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        sWuhunMgr->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sWuhunMgr->UpdatePlayer(player, diff);
    }

    void OnPlayerCreatureKill(Player* player, Creature* creature) override
    {
        sWuhunMgr->OnBossKilled(player, creature);
    }

    void OnPlayerCreatureKilledByPet(Player* player, Creature* creature) override
    {
        sWuhunMgr->OnBossKilled(player, creature);
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell || !sWuhunMgr->IsEnabled())
            return;

        sWuhunMgr->MirrorOwnerSpell(player, spell);
    }

    void OnPlayerJustDied(Player* player) override
    {
        if (player)
            sWuhunMgr->DismissAvatar(player, true);
    }

    void OnPlayerMapChanged(Player* player) override
    {
        if (player)
        {
            sWuhunMgr->DismissAvatar(player, true, false);
            sWuhunMgr->RestoreAvatarIfRequested(player);
        }
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !sWuhunMgr->IsEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != WUHUN_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        HandleAddonCommand(player, command);
    }

private:
    void HandleAddonCommand(Player* player, std::string const& command)
    {
        if (command == "REQ_ALL")
        {
            sWuhunMgr->SendAll(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            sWuhunMgr->SendState(player);
            return;
        }

        if (command == "REQ_SKILLS")
        {
            sWuhunMgr->SendSkillList(player);
            sWuhunMgr->SendSkills(player);
            return;
        }

        if (command == "REQ_EQUIP")
        {
            sWuhunMgr->SendEquip(player);
            return;
        }

        if (command == "SUMMON")
        {
            sWuhunMgr->SummonAvatar(player);
            return;
        }

        if (command == "ACTIVATE")
        {
            sWuhunMgr->ActivateAvatar(player);
            return;
        }

        if (command == "EVOLVE_WUHUN")
        {
            sWuhunMgr->EvolveWuhun(player);
            return;
        }

        if (command == "DISMISS")
        {
            sWuhunMgr->DismissAvatar(player);
            return;
        }

        std::vector<std::string> parts = Split(command, ':');
        if (parts.empty())
            return;

        uint32 a = 0;
        uint32 b = 0;
        uint32 c = 0;
        uint128 amount = 0;

        if (parts[0] == "INFUSE_RING" && parts.size() >= 3 && TryParseUInt(parts[1], a) && TryParseUInt128(parts[2], amount))
        {
            sWuhunMgr->InfuseRing(player, a, amount);
            return;
        }

        if (parts[0] == "LEARN_SKILL" && parts.size() >= 2 && TryParseUInt(parts[1], a))
        {
            sWuhunMgr->LearnSkill(player, a);
            return;
        }

        if (parts[0] == "UPGRADE_SKILL" && parts.size() >= 2 && TryParseUInt(parts[1], a))
        {
            sWuhunMgr->UpgradeSkill(player, a);
            return;
        }

        if (parts[0] == "SET_SKILL" && parts.size() >= 3 && TryParseUInt(parts[1], a) && TryParseUInt(parts[2], b))
        {
            sWuhunMgr->SetSkill(player, a, b);
            return;
        }

        if (parts[0] == "CLEAR_SKILL" && parts.size() >= 2 && TryParseUInt(parts[1], a))
        {
            sWuhunMgr->ClearSkill(player, a);
            return;
        }

        if (parts[0] == "SET_SKILL_MODE" && parts.size() >= 2 && TryParseUInt(parts[1], a))
        {
            sWuhunMgr->SetSkillMode(player, a);
            return;
        }

        if (parts[0] == "UNLOCK_EQUIP_SLOT" && parts.size() >= 2 && TryParseUInt(parts[1], a))
        {
            sWuhunMgr->UnlockEquipSlot(player, a);
            return;
        }

        if (parts[0] == "EQUIP_ITEM" && parts.size() >= 4 && TryParseUInt(parts[1], a) && TryParseUInt(parts[2], b) && TryParseUInt(parts[3], c))
        {
            sWuhunMgr->EquipItem(player, a, b, c);
            return;
        }

        if (parts[0] == "UNEQUIP_ITEM" && parts.size() >= 2 && TryParseUInt(parts[1], a))
        {
            sWuhunMgr->UnequipItem(player, a);
            return;
        }

        sWuhunMgr->SendResult(player, "UNKNOWN", false, "未知武魂指令");
    }
};

class WuhunAllMapScript : public AllMapScript
{
public:
    WuhunAllMapScript() : AllMapScript("WuhunAllMapScript",
    {
        ALLMAPHOOK_ON_PLAYER_LEAVE_ALL
    }) { }

    void OnPlayerLeaveAll(Map* /*map*/, Player* player) override
    {
        if (player)
            sWuhunMgr->DismissAvatar(player, true, false);
    }
};

using namespace Acore::ChatCommands;

class WuhunSystemCommandScript : public CommandScript
{
public:
    WuhunSystemCommandScript() : CommandScript("WuhunSystemCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable wuhunCommandTable =
        {
            { "界面",     HandleOpenUICommand,         SEC_PLAYER,        Console::No  },
            { "ui",       HandleOpenUICommand,         SEC_PLAYER,        Console::No  },
            { "帮助",     HandleHelpCommand,           SEC_GAMEMASTER,    Console::No  },
            { "信息",     HandleInfoCommand,           SEC_GAMEMASTER,    Console::No  },
            { "同步",     HandleSyncCommand,           SEC_GAMEMASTER,    Console::No  },
            { "召唤",     HandleSummonCommand,         SEC_GAMEMASTER,    Console::No  },
            { "收回",     HandleDismissCommand,        SEC_GAMEMASTER,    Console::No  },
            { "魂力",     HandleSoulPowerCommand,      SEC_GAMEMASTER,    Console::No  },
            { "重置",     HandleResetCommand,          SEC_ADMINISTRATOR, Console::No  },
            { "重载",     HandleReloadCommand,         SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "武魂系统", wuhunCommandTable },
            { "wuhun",    wuhunCommandTable },
        };

        return commandTable;
    }

private:
    static bool EnsureEnabled(ChatHandler* handler)
    {
        if (sWuhunMgr->IsEnabled())
            return true;

        handler->SendSysMessage("武魂系统当前未启用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    static Player* GetTarget(ChatHandler* handler)
    {
        Player* target = handler ? handler->getSelectedPlayerOrSelf() : nullptr;
        if (!target)
        {
            if (handler)
            {
                handler->SendSysMessage("请选中一名在线玩家，或在游戏内对自己执行。");
                handler->SetSentErrorMessage(true);
            }
            return nullptr;
        }

        return target;
    }

    static bool ReadOptionalInt128Argument(ChatHandler* handler, char const* args, char const* usage, bool& hasValue, int128& value)
    {
        hasValue = false;
        value = 0;

        std::istringstream stream(args ? args : "");
        std::string token;
        if (!(stream >> token))
            return true;

        std::string extra;
        if (!TryParseInt128(token, value) || (stream >> extra))
        {
            handler->PSendSysMessage("用法: {}", usage);
            handler->SetSentErrorMessage(true);
            return false;
        }

        hasValue = true;
        return true;
    }

    static bool SendSoulPowerInfo(ChatHandler* handler, Player* target)
    {
        uint128 soulPower = 0;
        uint32 wuhunId = 0;
        if (!sWuhunMgr->GetSoulPowerInfo(target, soulPower, wuhunId))
            return false;

        handler->PSendSysMessage("|cff66ccff[武魂系统]|r {} 武魂ID: {}，魂力: {}。",
            handler->GetNameLink(target), wuhunId, Acore::ToString(soulPower));
        return true;
    }

    static void NotifySoulPowerChanged(ChatHandler* handler, Player* target)
    {
        uint128 soulPower = 0;
        uint32 wuhunId = 0;
        if (!target || !target->GetSession() || !sWuhunMgr->GetSoulPowerInfo(target, soulPower, wuhunId))
            return;

        Player* executor = handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (executor == target)
            return;

        ChatHandler(target->GetSession()).PSendSysMessage("|cff66ccff[武魂系统]|r GM 已调整你的魂力，当前魂力 {}。", Acore::ToString(soulPower));
    }

    static bool HandleHelpCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        handler->SendSysMessage("|cff66ccff[武魂系统]|r GM命令:");
        handler->SendSysMessage(".武魂系统 界面 - 打开武魂等级UI界面");
        handler->SendSysMessage(".武魂系统 信息 - 查看选中玩家或自己的武魂数据");
        handler->SendSysMessage(".武魂系统 魂力 [+#值|-#值] - 查看、增加或扣除当前魂力");
        handler->SendSysMessage(".武魂系统 同步 / 召唤 / 收回 - 同步客户端或控制武魂");
        handler->SendSysMessage(".武魂系统 重载 - 重载武魂模板配置");
        handler->SendSysMessage(".武魂系统 重置 - 重置选中玩家或自己的武魂数据");
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!sWuhunMgr->IsEnabled())
        {
            handler->SendSysMessage("武魂系统当前未启用，未执行模板重载。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sWuhunMgr->LoadTemplates();
        LOG_INFO("server.loading", "武魂系统: GM命令已手动重载模板");
        handler->SendSysMessage("武魂系统模板已重载。");
        return true;
    }

    static bool HandleInfoCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* target = GetTarget(handler);
        if (!target)
            return false;

        return SendSoulPowerInfo(handler, target);
    }

    static bool HandleSoulPowerCommand(ChatHandler* handler, char const* args)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* target = GetTarget(handler);
        if (!target)
            return false;

        int128 value = 0;
        bool hasValue = false;
        if (!ReadOptionalInt128Argument(handler, args, ".武魂系统 魂力 [+#值|-#值]", hasValue, value))
            return false;

        if (!hasValue)
            return SendSoulPowerInfo(handler, target);

        if (value == 0)
            return SendSoulPowerInfo(handler, target);

        if (!sWuhunMgr->AdjustSoulPowerByCommand(target, value))
            return false;

        uint128 amount = AbsInt128ToUInt128(value);
        handler->PSendSysMessage("|cff66ccff[武魂系统]|r 已为 {} {} {} 点魂力。",
            handler->GetNameLink(target), value > 0 ? "增加" : "扣除", Acore::ToString(amount));
        NotifySoulPowerChanged(handler, target);
        return true;
    }

    static bool HandleSyncCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* target = GetTarget(handler);
        if (!target)
            return false;

        sWuhunMgr->SendAll(target);
        handler->PSendSysMessage("|cff66ccff[武魂系统]|r 已同步 {} 的武魂客户端状态。", handler->GetNameLink(target));
        return true;
    }

    static bool HandleSummonCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* target = GetTarget(handler);
        if (!target)
            return false;

        sWuhunMgr->SummonAvatar(target);
        handler->PSendSysMessage("|cff66ccff[武魂系统]|r 已为 {} 执行武魂召唤。", handler->GetNameLink(target));
        return true;
    }

    static bool HandleDismissCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* target = GetTarget(handler);
        if (!target)
            return false;

        sWuhunMgr->DismissAvatar(target);
        handler->PSendSysMessage("|cff66ccff[武魂系统]|r 已为 {} 收回武魂。", handler->GetNameLink(target));
        return true;
    }

    static bool HandleResetCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* target = GetTarget(handler);
        if (!target)
            return false;

        if (!sWuhunMgr->ResetPlayerWuhun(target))
            return false;

        handler->PSendSysMessage("|cff66ccff[武魂系统]|r 已重置 {} 的武魂数据。", handler->GetNameLink(target));
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler || !EnsureEnabled(handler))
            return false;

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        sWuhunMgr->SendOpenUI(player);
        sWuhunMgr->SendAll(player);
        return true;
    }
};

void AddSC_mod_wuhun_system()
{
    new WuhunWorldScript();
    new WuhunUnitScript();
    new WuhunPlayerScript();
    new WuhunAllMapScript();
    new WuhunSystemCommandScript();
    new npc_wuhun_avatar();
}

#include "ScriptMgr.h"
#include "AddonThrottle.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "SpellMgr.h"
#include "HermesBridgeAddonApi.h"
#include "Util.h"

#if __has_include("RequirementSystem.h")
    #ifndef MODULE_REQUIREMENT_TEMPLATE
        #define MODULE_REQUIREMENT_TEMPLATE
    #endif
    #include "RequirementSystem.h"
#endif

#if __has_include("ItemSkillsManager.h")
    #ifndef MODULE_ITEM_SKILLS
        #define MODULE_ITEM_SKILLS
    #endif
    #include "ItemSkillsManager.h"
    #include "ItemSkillsEffects.h"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Acore::ChatCommands;

bool TujianSystem_Enable = true;
bool TujianSystem_Announce = true;

namespace
{
    constexpr uint8 VIRTUAL_TUJIAN_SLOT = EQUIPMENT_SLOT_BODY;
    constexpr char TUJIAN_SYSTEM_ADDON_PREFIX[] = "ZDYUI_TJ";
    constexpr size_t MAX_ADDON_PAYLOAD = 200;
    constexpr uint8 TUJIAN_ATTR_MODE_FIXED = 0;
    constexpr uint8 TUJIAN_ATTR_MODE_EQUIP = 1;
    constexpr uint8 TUJIAN_ATTR_MODE_PERCENT = 2;

    int32 ToInt32ForLegacyStatPath(int64 value)
    {
        if (value > std::numeric_limits<int32>::max())
            return std::numeric_limits<int32>::max();

        if (value < std::numeric_limits<int32>::min())
            return std::numeric_limits<int32>::min();

        return static_cast<int32>(value);
    }

    int64 ToInt64Saturated(uint256 const& value)
    {
        return Acore::Number::ToInt64Saturated(Acore::Number::ToInt256Saturated(value));
    }

    int64 ToInt64Saturated(int256 const& value)
    {
        return Acore::Number::ToInt64Saturated(value);
    }

    int32 ToInt32ForLegacyStatPath(int256 const& value)
    {
        return ToInt32ForLegacyStatPath(ToInt64Saturated(value));
    }

    int256 AddInt256Saturated(int256 const& left, int256 const& right)
    {
        if (right > 0 && left > std::numeric_limits<int256>::max() - right)
            return std::numeric_limits<int256>::max();
        if (right < 0 && left < std::numeric_limits<int256>::min() - right)
            return std::numeric_limits<int256>::min();
        return left + right;
    }

    int256 MultiplyInt256Saturated(int256 const& value, uint32 multiplier)
    {
        if (value == 0 || multiplier == 0)
            return 0;
        if (value > 0 && value > std::numeric_limits<int256>::max() / static_cast<int256>(multiplier))
            return std::numeric_limits<int256>::max();
        if (value < 0 && value < std::numeric_limits<int256>::min() / static_cast<int256>(multiplier))
            return std::numeric_limits<int256>::min();
        return value * static_cast<int256>(multiplier);
    }

    struct TuJianEntry
    {
        uint32 id = 0;
        std::string comment;
        std::string itemName;
        std::string menu1Name;
        std::string menu1Icon;
        std::string menu2Name1;
        std::string menu2Name2;
        std::string menu2Icon;
        uint32 page = 0;
        uint32 level = 0;
        uint32 maxLevel = 0;
        uint32 itemEntry = 0;
        uint32 setId = 0;
        uint32 activationRequirement = 0;
        std::string activationCommand;
        uint8 attributeEffectMode = TUJIAN_ATTR_MODE_EQUIP;
        int256 fixedAllStatsValue = 0;
        int256 allStatsPercent = 0;
    };

    struct TuJianSetEntry
    {
        uint32 id = 0;
        uint32 group = 0;
        uint32 requiredActivationCount = 0;
        uint32 effectiveRequiredActivationCount = 0;
        std::string comment;
        std::string activationDesc;
        std::string activationTemplates;
        std::vector<uint32> activationTemplateIds;
        std::string activationCommand;
    };

    struct PlayerActivationRecord
    {
        uint32 tuJianId = 0;
        uint32 setId = 0;
        uint32 currentLevel = 0;
        uint32 currentItemEntry = 0;
    };

    struct TuJianMenuEntry
    {
        uint32 chapterId = 0;
        std::string menu1Name;
        std::string menu2Name1;
        std::string menu2Name2;
        uint32 itemCount = 0;
    };

    struct VirtualAppliedItem
    {
        std::unique_ptr<Item> item;
        uint8 slot = VIRTUAL_TUJIAN_SLOT;
        uint32 tuJianId = 0;
        uint32 setId = 0;
        bool allowEquipSpell = false;
        bool applyItemMods = false;
        bool applyEquipSpell = false;
        bool hasExternalSkills = false;
    };

    struct DirectAppliedItem
    {
        uint32 itemEntry = 0;
        uint32 applyCount = 0;
    };

    struct AggregatedItemBonusCache
    {
        std::unordered_map<uint32, int256> itemStatTotals;
        std::array<int64, MAX_SPELL_SCHOOL> resistanceTotals = {};
        int256 armorBase = 0;
        int256 armorTotal = 0;
        int256 armorDamageModifierTotal = 0;
        int64 blockFromTemplate = 0;
        int64 feralApBonus = 0;

        bool HasAnyValue() const
        {
            if (armorBase != 0 || armorTotal != 0 || armorDamageModifierTotal != 0 || blockFromTemplate != 0 || feralApBonus != 0)
                return true;

            for (auto const& pair : itemStatTotals)
                if (pair.second != 0)
                    return true;

            for (int64 value : resistanceTotals)
                if (value != 0)
                    return true;

            return false;
        }
    };

    struct AggregatedItemSetContribution
    {
        uint32 setId = 0;
        uint32 itemCount = 0;
    };

    struct PlayerWeaponDamageBonusCache
    {
        float minDamage[MAX_ATTACK][MAX_ITEM_PROTO_DAMAGES] = {};
        float maxDamage[MAX_ATTACK][MAX_ITEM_PROTO_DAMAGES] = {};
    };

    std::unordered_map<uint32, TuJianEntry> tuJianEntries;
    std::unordered_map<uint32, TuJianSetEntry> tuJianSetEntries;
    std::vector<TuJianMenuEntry> tuJianMenuEntries;
    std::unordered_map<uint32, std::vector<uint32>> tuJianItemEntriesByChapter;
    std::unordered_map<uint32, std::set<uint32>> tuJianSetGroupsByChapter;
    std::unordered_map<uint32, uint32> tuJianActivationCountByGroup;
    std::unordered_map<uint32, std::vector<uint32>> tuJianSetEntryIdsByGroup;
    std::unordered_map<uint32, uint32> tuJianDefaultCarrierItemByGroup;
    std::unordered_map<uint32, std::vector<PlayerActivationRecord>> playerActivationCache;
    std::unordered_map<uint32, std::vector<VirtualAppliedItem>> playerVirtualItems;
    std::unordered_map<uint32, std::vector<DirectAppliedItem>> playerFallbackAppliedItems;
    std::unordered_map<uint32, AggregatedItemBonusCache> playerAggregatedItemBonuses;
    std::unordered_map<uint32, std::vector<AggregatedItemSetContribution>> playerItemSetContributions;
    std::unordered_map<uint32, PlayerWeaponDamageBonusCache> playerWeaponDamageBonuses;
    std::unordered_map<uint32, int256> playerFixedAllStatsBonus;
    std::unordered_map<uint32, int256> playerAllStatsPercentBonus;
    std::unordered_set<uint32> blockedVirtualEquipSpellItemGuids;

    uint8 GetAttackSlotForVirtualItem(ItemTemplate const* proto);
    uint32 ResolveSetGroupId(uint32 assignedSetId);

    std::string SanitizeAddonText(std::string text)
    {
        for (char& ch : text)
        {
            if (ch == '^' || ch == '~' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
                ch = ' ';
        }

        return text;
    }

    std::vector<uint32> ParseUint32List(std::string const& value)
    {
        std::vector<uint32> result;
        std::unordered_set<uint32> seenValues;
        std::istringstream iss(value);
        std::string token;

        while (std::getline(iss, token, ','))
        {
            token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch)
            {
                return std::isspace(ch) != 0;
            }), token.end());

            if (token.empty())
                continue;

            try
            {
                uint32 parsedValue = static_cast<uint32>(std::stoul(token));
                if (parsedValue != 0 && seenValues.insert(parsedValue).second)
                    result.push_back(parsedValue);
            }
            catch (...)
            {
            }
        }

        return result;
    }

    std::string FormatActivationTemplatesForClient(std::vector<uint32> const& templateIds)
    {
        if (templateIds.empty())
            return "";

        std::ostringstream displayText;
        bool first = true;

        for (uint32 templateId : templateIds)
        {
            std::string part = std::to_string(templateId);

#ifdef MODULE_ITEM_SKILLS
            if (sItemSkillsManager)
            {
                if (ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(templateId))
                {
                    if (!skillTemplate->clientDisplay.empty())
                        part = skillTemplate->clientDisplay;
                }
            }
#endif

            if (!first)
                displayText << ", ";
            first = false;

            displayText << SanitizeAddonText(part);
        }

        return displayText.str();
    }

    std::string FormatActivationTemplatesForClient(std::string const& value)
    {
        return FormatActivationTemplatesForClient(ParseUint32List(value));
    }

    uint32 GetTuJianApplyCount(TuJianEntry const& entry, uint32 currentLevel)
    {
        if (entry.attributeEffectMode != TUJIAN_ATTR_MODE_EQUIP)
            return 0;

        uint32 applyCount = std::max<uint32>(1, currentLevel);
        if (entry.maxLevel > 0)
            applyCount = std::min(applyCount, entry.maxLevel);

        return applyCount;
    }

    void ApplyFixedAllStatsBonus(Player* player, int256 const& amount, bool apply)
    {
        if (!player || amount == 0)
            return;

        float legacyAmount = Acore::Number::ToFloat(amount);
        for (uint8 stat = STAT_STRENGTH; stat < MAX_STATS; ++stat)
        {
            player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + stat), BASE_VALUE, legacyAmount, apply);
            player->ApplyStatBuffMod(Stats(stat), legacyAmount, apply);
        }
    }

    void ApplyAllStatsPercentBonus(Player* player, int256 const& percent, bool apply)
    {
        if (!player || percent == 0)
            return;

        float legacyPercent = Acore::Number::ToFloat(percent);
        for (uint8 stat = STAT_STRENGTH; stat < MAX_STATS; ++stat)
            player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + stat), TOTAL_PCT, legacyPercent, apply);
    }

    void ApplyAggregatedItemStat(Player* player, uint32 statType, int256 const& value, bool apply)
    {
        if (!player || value == 0)
            return;

        int256 val = value;
        float statModValue = Acore::Number::ToFloat(val);
        int256 legacyVal64 = val;
        int32 legacyVal = ToInt32ForLegacyStatPath(value);
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
            case ITEM_MOD_TRUE_DAMAGE:
                // 自定义属性：真实伤害
                player->ApplyTrueDamageBonus(Acore::Number::ToInt64Saturated(val), apply);
                break;
            case ITEM_MOD_CUTTING_DAMAGE:
                // 自定义属性：切割伤害
                player->ApplyCuttingDamageBonus(Acore::Number::ToInt64Saturated(val), apply);
                break;
            case ITEM_MOD_COOLDOWN_REDUCTION:
                // 自定义属性：冷却缩减
                player->ApplyCooldownReductionBonus(Acore::Number::ToInt64Saturated(val), apply);
                break;
            case ITEM_MOD_SKILL_DAMAGE:
                // 自定义属性：技能伤害
                player->ApplySkillDamageBonus(Acore::Number::ToInt64Saturated(val), apply);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                player->ApplyRatingMod(CR_DEFENSE_SKILL, legacyVal64, apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                player->ApplyRatingMod(CR_DODGE, legacyVal64, apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                player->ApplyRatingMod(CR_PARRY, legacyVal64, apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                player->ApplyRatingMod(CR_BLOCK, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                player->ApplyRatingMod(CR_HIT_RANGED, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                player->ApplyRatingMod(CR_HIT_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                player->ApplyRatingMod(CR_CRIT_RANGED, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                player->ApplyRatingMod(CR_CRIT_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, legacyVal64, apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                player->ApplyRatingMod(CR_HASTE_RANGED, legacyVal64, apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                player->ApplyRatingMod(CR_HASTE_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, legacyVal64, apply);
                player->ApplyRatingMod(CR_HIT_RANGED, legacyVal64, apply);
                player->ApplyRatingMod(CR_HIT_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, legacyVal64, apply);
                player->ApplyRatingMod(CR_CRIT_RANGED, legacyVal64, apply);
                player->ApplyRatingMod(CR_CRIT_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, legacyVal64, apply);
                player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, legacyVal64, apply);
                player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
            case ITEM_MOD_RESILIENCE_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, legacyVal64, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, legacyVal64, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, legacyVal64, apply);
                player->ApplyRatingMod(CR_HASTE_RANGED, legacyVal64, apply);
                player->ApplyRatingMod(CR_HASTE_SPELL, legacyVal64, apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                player->ApplyRatingMod(CR_EXPERTISE, legacyVal64, apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, statModValue, apply);
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, statModValue, apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, statModValue, apply);
                break;
            case ITEM_MOD_MANA_REGENERATION:
                player->ApplyManaRegenBonus(legacyVal, apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                player->ApplyRatingMod(CR_ARMOR_PENETRATION, legacyVal64, apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                player->ApplySpellPowerBonus(val, apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                player->ApplyHealthRegenBonus(legacyVal, apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                player->ApplySpellPenetrationBonus(legacyVal, apply);
                break;
            case ITEM_MOD_BLOCK_VALUE:
                player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, statModValue, apply);
                break;
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
                break;
            default:
                break;
        }
    }

    void ApplyAggregatedItemBonuses(Player* player, AggregatedItemBonusCache const& cache, bool apply)
    {
        if (!player || !cache.HasAnyValue())
            return;

        for (auto const& pair : cache.itemStatTotals)
            ApplyAggregatedItemStat(player, pair.first, pair.second, apply);

        if (cache.armorBase != 0)
            player->HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, Acore::Number::ToFloat(cache.armorBase), apply);
        if (cache.armorTotal != 0)
            player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, Acore::Number::ToFloat(cache.armorTotal), apply);
        if (cache.armorDamageModifierTotal != 0)
            player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, Acore::Number::ToFloat(cache.armorDamageModifierTotal), apply);
        if (cache.blockFromTemplate != 0)
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(cache.blockFromTemplate), apply);
        if (cache.resistanceTotals[SPELL_SCHOOL_HOLY] != 0)
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(cache.resistanceTotals[SPELL_SCHOOL_HOLY]), apply);
        if (cache.resistanceTotals[SPELL_SCHOOL_FIRE] != 0)
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(cache.resistanceTotals[SPELL_SCHOOL_FIRE]), apply);
        if (cache.resistanceTotals[SPELL_SCHOOL_NATURE] != 0)
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(cache.resistanceTotals[SPELL_SCHOOL_NATURE]), apply);
        if (cache.resistanceTotals[SPELL_SCHOOL_FROST] != 0)
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(cache.resistanceTotals[SPELL_SCHOOL_FROST]), apply);
        if (cache.resistanceTotals[SPELL_SCHOOL_SHADOW] != 0)
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(cache.resistanceTotals[SPELL_SCHOOL_SHADOW]), apply);
        if (cache.resistanceTotals[SPELL_SCHOOL_ARCANE] != 0)
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(cache.resistanceTotals[SPELL_SCHOOL_ARCANE]), apply);
        if (cache.feralApBonus != 0)
            player->ApplyFeralAPBonus(static_cast<int32>(cache.feralApBonus), apply);
    }

    bool CanUseFastItemBonusPath(ItemTemplate const* proto)
    {
        if (!proto)
            return false;

        return proto->ScalingStatDistribution == 0 && proto->ScalingStatValue == 0;
    }

    void AccumulateItemBonuses(Player* player, ItemTemplate const* proto, uint32 applyCount, AggregatedItemBonusCache& cache)
    {
        if (!player || !proto || applyCount == 0)
            return;

        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS && i < proto->StatsCount; ++i)
        {
            uint32 statType = proto->ItemStat[i].ItemStatType;
            int256 val = proto->ItemStatValue256[i];
            if (val == 0)
                continue;

            cache.itemStatTotals[statType] = AddInt256Saturated(cache.itemStatTotals[statType], MultiplyInt256Saturated(val, applyCount));
        }

        uint256 armor = proto->Armor256;
        if (armor != 0 && proto->ArmorDamageModifier)
            armor = proto->ArmorDamageModifier >= Acore::Number::ToDouble(armor) ? 0 : armor - Acore::Number::ToUInt256Saturated(static_cast<long double>(proto->ArmorDamageModifier));

        if (armor != 0)
        {
            bool isBaseArmor =
                proto->Class == ITEM_CLASS_ARMOR &&
                (proto->SubClass == ITEM_SUBCLASS_ARMOR_CLOTH ||
                 proto->SubClass == ITEM_SUBCLASS_ARMOR_LEATHER ||
                 proto->SubClass == ITEM_SUBCLASS_ARMOR_MAIL ||
                 proto->SubClass == ITEM_SUBCLASS_ARMOR_PLATE ||
                 proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD);

            int256 armorAmount = MultiplyInt256Saturated(Acore::Number::ToInt256Saturated(armor), applyCount);
            if (isBaseArmor)
                cache.armorBase = AddInt256Saturated(cache.armorBase, armorAmount);
            else
                cache.armorTotal = AddInt256Saturated(cache.armorTotal, armorAmount);
        }

        if (proto->ArmorDamageModifier > 0 && sScriptMgr->OnPlayerCanArmorDamageModifier(player))
        {
            int256 armorDamageModifier = Acore::Number::ToInt256Saturated(static_cast<long double>(proto->ArmorDamageModifier) * static_cast<long double>(applyCount));
            cache.armorDamageModifierTotal = AddInt256Saturated(cache.armorDamageModifierTotal, armorDamageModifier);
        }

        if (proto->Block)
            cache.blockFromTemplate += static_cast<int64>(proto->Block) * applyCount;

        if (proto->HolyRes)
            cache.resistanceTotals[SPELL_SCHOOL_HOLY] += static_cast<int64>(proto->HolyRes) * applyCount;
        if (proto->FireRes)
            cache.resistanceTotals[SPELL_SCHOOL_FIRE] += static_cast<int64>(proto->FireRes) * applyCount;
        if (proto->NatureRes)
            cache.resistanceTotals[SPELL_SCHOOL_NATURE] += static_cast<int64>(proto->NatureRes) * applyCount;
        if (proto->FrostRes)
            cache.resistanceTotals[SPELL_SCHOOL_FROST] += static_cast<int64>(proto->FrostRes) * applyCount;
        if (proto->ShadowRes)
            cache.resistanceTotals[SPELL_SCHOOL_SHADOW] += static_cast<int64>(proto->ShadowRes) * applyCount;
        if (proto->ArcaneRes)
            cache.resistanceTotals[SPELL_SCHOOL_ARCANE] += static_cast<int64>(proto->ArcaneRes) * applyCount;

        if (player->IsClass(CLASS_DRUID, CLASS_CONTEXT_STATS))
        {
            int32 dpsMod = 0;
            int32 feralBonus = proto->getFeralBonus(dpsMod);
            sScriptMgr->OnPlayerGetFeralApBonus(player, feralBonus, dpsMod, proto, nullptr);
            if (feralBonus != 0)
                cache.feralApBonus += static_cast<int64>(feralBonus) * applyCount;
        }
    }

    ItemSetEffect* FindItemSetEffect(Player* player, uint32 setId)
    {
        if (!player || setId == 0)
            return nullptr;

        for (ItemSetEffect* effect : player->ItemSetEff)
            if (effect && effect->setid == setId)
                return effect;

        return nullptr;
    }

    ItemSetEffect* FindOrCreateItemSetEffect(Player* player, uint32 setId)
    {
        if (!player || setId == 0)
            return nullptr;

        if (ItemSetEffect* effect = FindItemSetEffect(player, setId))
            return effect;

        ItemSetEffect* effect = new ItemSetEffect();
        effect->setid = setId;
        effect->item_count = 0;
        for (SpellInfo const*& spellInfo : effect->spells)
            spellInfo = nullptr;

        for (std::size_t index = 0; index < player->ItemSetEff.size(); ++index)
        {
            if (!player->ItemSetEff[index])
            {
                player->ItemSetEff[index] = effect;
                return effect;
            }
        }

        player->ItemSetEff.push_back(effect);
        return effect;
    }

    bool ApplyItemSetContribution(Player* player, uint32 setId, uint32 itemCount)
    {
        if (!player || setId == 0 || itemCount == 0)
            return false;

        ItemSetEntry const* set = sItemSetStore.LookupEntry(setId);
        if (!set)
        {
            LOG_ERROR("sql.sql", "mod-tujian-system: item set {} not found during aggregated apply.", setId);
            return false;
        }

        if (set->required_skill_id && player->GetSkillValue(set->required_skill_id) < set->required_skill_value)
            return false;

        ItemSetEffect* effect = FindOrCreateItemSetEffect(player, setId);
        if (!effect)
            return false;

        uint32 oldCount = effect->item_count;
        effect->item_count += itemCount;

        for (uint32 spellIndex = 0; spellIndex < MAX_ITEM_SET_SPELLS; ++spellIndex)
        {
            uint32 spellId = set->spells[spellIndex];
            uint32 requiredCount = set->items_to_triggerspell[spellIndex];
            if (!spellId || requiredCount == 0 || oldCount >= requiredCount || effect->item_count < requiredCount)
                continue;

            bool alreadyActive = false;
            for (SpellInfo const* activeSpell : effect->spells)
            {
                if (activeSpell && activeSpell->Id == spellId)
                {
                    alreadyActive = true;
                    break;
                }
            }

            if (alreadyActive)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo)
            {
                LOG_ERROR("entities.item", "mod-tujian-system: unknown spell id {} in aggregated item set {}.", spellId, setId);
                continue;
            }

            for (uint32 slot = 0; slot < MAX_ITEM_SET_SPELLS; ++slot)
            {
                if (!effect->spells[slot])
                {
                    effect->spells[slot] = spellInfo;
                    player->ApplyEquipSpell(spellInfo, nullptr, true);
                    break;
                }
            }
        }

        return true;
    }

    void RemoveItemSetContribution(Player* player, uint32 setId, uint32 itemCount)
    {
        if (!player || setId == 0 || itemCount == 0)
            return;

        ItemSetEntry const* set = sItemSetStore.LookupEntry(setId);
        if (!set)
            return;

        ItemSetEffect* effect = FindItemSetEffect(player, setId);
        if (!effect)
            return;

        uint32 oldCount = effect->item_count;
        effect->item_count = oldCount > itemCount ? oldCount - itemCount : 0;

        for (uint32 spellIndex = 0; spellIndex < MAX_ITEM_SET_SPELLS; ++spellIndex)
        {
            uint32 spellId = set->spells[spellIndex];
            uint32 requiredCount = set->items_to_triggerspell[spellIndex];
            if (!spellId || requiredCount == 0 || oldCount < requiredCount || effect->item_count >= requiredCount)
                continue;

            for (uint32 slot = 0; slot < MAX_ITEM_SET_SPELLS; ++slot)
            {
                if (effect->spells[slot] && effect->spells[slot]->Id == spellId)
                {
                    player->ApplyEquipSpell(effect->spells[slot], nullptr, false);
                    effect->spells[slot] = nullptr;
                    break;
                }
            }
        }

        if (effect->item_count == 0)
        {
            for (std::size_t index = 0; index < player->ItemSetEff.size(); ++index)
            {
                if (player->ItemSetEff[index] == effect)
                {
                    delete effect;
                    player->ItemSetEff[index] = nullptr;
                    break;
                }
            }
        }
    }

    bool ItemTemplateHasEquipSpell(ItemTemplate const* proto)
    {
        if (!proto)
            return false;

        for (uint8 spellIndex = 0; spellIndex < MAX_ITEM_SPELLS; ++spellIndex)
        {
            if (proto->Spells[spellIndex].SpellId != 0 &&
                proto->Spells[spellIndex].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
                return true;
        }

        return false;
    }

    void AddWeaponDamageBonus(PlayerWeaponDamageBonusCache& cache, ItemTemplate const* proto, uint32 applyCount)
    {
        if (!proto || applyCount == 0)
            return;

        uint8 attackType = GetAttackSlotForVirtualItem(proto);
        if (attackType == MAX_ATTACK)
            return;

        for (uint8 damageIndex = 0; damageIndex < MAX_ITEM_PROTO_DAMAGES; ++damageIndex)
        {
            cache.minDamage[attackType][damageIndex] += proto->Damage[damageIndex].DamageMin * applyCount;
            cache.maxDamage[attackType][damageIndex] += proto->Damage[damageIndex].DamageMax * applyCount;
        }
    }

    uint32 GetChapterIdFromPageValue(uint32 page)
    {
        if (page >= 10)
            return page / 10;

        return page;
    }

    uint32 GetChapterIdForEntry(TuJianEntry const& entry)
    {
        return GetChapterIdFromPageValue(entry.page);
    }

    uint32 GetChapterIdForItemEntry(uint32 itemEntry)
    {
        auto itr = tuJianEntries.find(itemEntry);
        if (itr == tuJianEntries.end())
            return 0;

        return GetChapterIdForEntry(itr->second);
    }

    void RebuildTuJianIndexes()
    {
        tuJianMenuEntries.clear();
        tuJianItemEntriesByChapter.clear();
        tuJianSetGroupsByChapter.clear();
        tuJianActivationCountByGroup.clear();
        tuJianSetEntryIdsByGroup.clear();
        tuJianDefaultCarrierItemByGroup.clear();

        std::unordered_map<uint32, TuJianMenuEntry> menuEntryByChapter;
        std::unordered_map<uint32, std::unordered_set<uint32>> activatedTuJianIdsByGroup;

        for (auto const& pair : tuJianEntries)
        {
            TuJianEntry const& entry = pair.second;
            uint32 chapterId = GetChapterIdForEntry(entry);
            if (chapterId == 0)
                continue;

            auto& menuEntry = menuEntryByChapter[chapterId];
            if (menuEntry.chapterId == 0)
            {
                menuEntry.chapterId = chapterId;
                menuEntry.menu1Name = entry.menu1Name;
                menuEntry.menu2Name1 = entry.menu2Name1;
                menuEntry.menu2Name2 = entry.menu2Name2;
            }

            ++menuEntry.itemCount;
            tuJianItemEntriesByChapter[chapterId].push_back(entry.itemEntry);

            if (entry.setId != 0)
                tuJianSetGroupsByChapter[chapterId].insert(entry.setId);

            uint32 groupId = ResolveSetGroupId(entry.setId);
            if (groupId == 0)
                continue;

            if (entry.id != 0)
                activatedTuJianIdsByGroup[groupId].insert(entry.id);

            if (entry.itemEntry != 0 && tuJianDefaultCarrierItemByGroup[groupId] == 0)
                tuJianDefaultCarrierItemByGroup[groupId] = entry.itemEntry;
        }

        for (auto& pair : menuEntryByChapter)
            tuJianMenuEntries.push_back(pair.second);

        std::sort(tuJianMenuEntries.begin(), tuJianMenuEntries.end(), [](TuJianMenuEntry const& left, TuJianMenuEntry const& right)
        {
            if (left.chapterId != right.chapterId)
                return left.chapterId < right.chapterId;
            if (left.menu1Name != right.menu1Name)
                return left.menu1Name < right.menu1Name;
            return left.menu2Name2 < right.menu2Name2;
        });

        for (auto& pair : tuJianItemEntriesByChapter)
        {
            std::vector<uint32>& itemEntries = pair.second;
            std::sort(itemEntries.begin(), itemEntries.end(), [](uint32 leftItemEntry, uint32 rightItemEntry)
            {
                auto leftItr = tuJianEntries.find(leftItemEntry);
                auto rightItr = tuJianEntries.find(rightItemEntry);

                if (leftItr == tuJianEntries.end() || rightItr == tuJianEntries.end())
                    return leftItemEntry < rightItemEntry;

                TuJianEntry const& left = leftItr->second;
                TuJianEntry const& right = rightItr->second;

                if (left.page != right.page)
                    return left.page < right.page;
                if (left.id != right.id)
                    return left.id < right.id;
                return left.itemEntry < right.itemEntry;
            });
        }

        for (auto const& pair : activatedTuJianIdsByGroup)
            tuJianActivationCountByGroup[pair.first] = static_cast<uint32>(pair.second.size());

        for (auto& pair : tuJianSetEntries)
        {
            TuJianSetEntry& entry = pair.second;
            uint32 totalGroupActivationCount = 0;

            auto countItr = tuJianActivationCountByGroup.find(entry.group);
            if (countItr != tuJianActivationCountByGroup.end())
                totalGroupActivationCount = countItr->second;

            if (totalGroupActivationCount == 0)
                entry.effectiveRequiredActivationCount = 0;
            else if (entry.requiredActivationCount == 0)
                entry.effectiveRequiredActivationCount = totalGroupActivationCount;
            else
                entry.effectiveRequiredActivationCount = std::min(entry.requiredActivationCount, totalGroupActivationCount);

            if (entry.group != 0)
                tuJianSetEntryIdsByGroup[entry.group].push_back(entry.id);
        }

        for (auto& pair : tuJianSetEntryIdsByGroup)
        {
            std::vector<uint32>& setEntryIds = pair.second;
            std::sort(setEntryIds.begin(), setEntryIds.end(), [](uint32 leftId, uint32 rightId)
            {
                auto leftItr = tuJianSetEntries.find(leftId);
                auto rightItr = tuJianSetEntries.find(rightId);
                if (leftItr == tuJianSetEntries.end() || rightItr == tuJianSetEntries.end())
                    return leftId < rightId;

                TuJianSetEntry const& left = leftItr->second;
                TuJianSetEntry const& right = rightItr->second;
                if (left.effectiveRequiredActivationCount != right.effectiveRequiredActivationCount)
                    return left.effectiveRequiredActivationCount < right.effectiveRequiredActivationCount;
                return left.id < right.id;
            });
        }
    }

    uint32 ResolveSetGroupId(uint32 assignedSetId)
    {
        if (assignedSetId == 0)
            return 0;

        auto setItr = tuJianSetEntries.find(assignedSetId);
        if (setItr != tuJianSetEntries.end() && setItr->second.group != 0)
            return setItr->second.group;

        return assignedSetId;
    }

    void SendAddonPayload(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        if (payload.length() <= MAX_ADDON_PAYLOAD)
        {
            if (HermesBridge_SendAddonMessage(player, TUJIAN_SYSTEM_ADDON_PREFIX, payload))
                return;

            std::string fullMessage = std::string(TUJIAN_SYSTEM_ADDON_PREFIX) + '\t' + payload;
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
            return;
        }

        size_t totalChunks = (payload.length() + MAX_ADDON_PAYLOAD - 1) / MAX_ADDON_PAYLOAD;
        for (size_t i = 0; i < totalChunks; ++i)
        {
            size_t start = i * MAX_ADDON_PAYLOAD;
            size_t len = std::min(MAX_ADDON_PAYLOAD, payload.length() - start);
            std::string chunk = payload.substr(start, len);

            std::ostringstream chunkMessage;
            chunkMessage << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << chunk;
            if (HermesBridge_SendAddonMessage(player, TUJIAN_SYSTEM_ADDON_PREFIX, chunkMessage.str()))
                continue;

            std::string fullMessage = std::string(TUJIAN_SYSTEM_ADDON_PREFIX) + '\t' + chunkMessage.str();
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
        }
    }

    void SendTuJianOpenUI(Player* player)
    {
        SendAddonPayload(player, "TJ_OPEN");
    }

    void SendTuJianMenuIndexToPlayer(Player* player)
    {
        std::ostringstream payload;
        payload << "TJ_INDEX:";

        bool first = true;
        for (TuJianMenuEntry const& entry : tuJianMenuEntries)
        {
            if (entry.chapterId == 0)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry.chapterId << '^'
                    << SanitizeAddonText(entry.menu1Name) << '^'
                    << SanitizeAddonText(entry.menu2Name1) << '^'
                    << SanitizeAddonText(entry.menu2Name2) << '^'
                    << entry.itemCount;
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianListToPlayer(Player* player)
    {
        std::vector<TuJianEntry const*> entries;
        entries.reserve(tuJianEntries.size());
        for (auto const& pair : tuJianEntries)
            entries.push_back(&pair.second);

        std::sort(entries.begin(), entries.end(), [](TuJianEntry const* left, TuJianEntry const* right)
        {
            if (left->page != right->page)
                return left->page < right->page;
            if (left->menu1Name != right->menu1Name)
                return left->menu1Name < right->menu1Name;
            if (left->menu2Name1 != right->menu2Name1)
                return left->menu2Name1 < right->menu2Name1;
            if (left->id != right->id)
                return left->id < right->id;
            return left->itemEntry < right->itemEntry;
        });

        std::ostringstream payload;
        payload << "TJ_LIST:";

        bool first = true;
        for (TuJianEntry const* entry : entries)
        {
            if (!entry)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry->itemEntry << '^'
                    << entry->id << '^'
                    << entry->page << '^'
                    << entry->level << '^'
                    << entry->maxLevel << '^'
                    << entry->setId << '^'
                    << entry->activationRequirement << '^'
                    << SanitizeAddonText(entry->menu1Name) << '^'
                    << SanitizeAddonText(entry->menu2Name1) << '^'
                    << SanitizeAddonText(entry->menu2Name2) << '^'
                    << SanitizeAddonText(entry->comment) << '^'
                    << SanitizeAddonText(entry->itemName);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianPageToPlayer(Player* player, uint32 chapterId)
    {
        std::ostringstream payload;
        payload << "TJ_PAGE:" << chapterId << ':';

        auto itemItr = tuJianItemEntriesByChapter.find(chapterId);
        if (itemItr == tuJianItemEntriesByChapter.end())
        {
            SendAddonPayload(player, payload.str());
            return;
        }

        bool first = true;
        for (uint32 itemEntry : itemItr->second)
        {
            auto entryItr = tuJianEntries.find(itemEntry);
            if (entryItr == tuJianEntries.end())
                continue;

            TuJianEntry const& entry = entryItr->second;

            if (!first)
                payload << '~';
            first = false;

            payload << entry.itemEntry << '^'
                    << entry.id << '^'
                    << entry.page << '^'
                    << entry.level << '^'
                    << entry.maxLevel << '^'
                    << entry.setId << '^'
                    << entry.activationRequirement << '^'
                    << SanitizeAddonText(entry.menu1Name) << '^'
                    << SanitizeAddonText(entry.menu2Name1) << '^'
                    << SanitizeAddonText(entry.menu2Name2) << '^'
                    << SanitizeAddonText(entry.comment) << '^'
                    << SanitizeAddonText(entry.itemName);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianSetListToPlayer(Player* player)
    {
        std::vector<TuJianSetEntry const*> entries;
        entries.reserve(tuJianSetEntries.size());
        for (auto const& pair : tuJianSetEntries)
            entries.push_back(&pair.second);

        std::sort(entries.begin(), entries.end(), [](TuJianSetEntry const* left, TuJianSetEntry const* right)
        {
            if (left->group != right->group)
                return left->group < right->group;
            if (left->requiredActivationCount != right->requiredActivationCount)
            {
                if (left->requiredActivationCount == 0)
                    return false;
                if (right->requiredActivationCount == 0)
                    return true;
                return left->requiredActivationCount < right->requiredActivationCount;
            }
            return left->id < right->id;
        });

        std::ostringstream payload;
        payload << "TJ_SET:";

        bool first = true;
        for (TuJianSetEntry const* entry : entries)
        {
            if (!entry)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry->id << '^'
                    << entry->group << '^'
                    << entry->requiredActivationCount << '^'
                    << SanitizeAddonText(entry->activationDesc) << '^'
                    << FormatActivationTemplatesForClient(entry->activationTemplateIds) << '^'
                    << SanitizeAddonText(entry->comment);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianPageSetsToPlayer(Player* player, uint32 chapterId)
    {
        std::ostringstream payload;
        payload << "TJ_PAGESET:" << chapterId << ':';

        auto setGroupItr = tuJianSetGroupsByChapter.find(chapterId);
        if (setGroupItr == tuJianSetGroupsByChapter.end() || setGroupItr->second.empty())
        {
            SendAddonPayload(player, payload.str());
            return;
        }

        std::vector<TuJianSetEntry const*> entries;
        entries.reserve(tuJianSetEntries.size());
        for (auto const& pair : tuJianSetEntries)
        {
            if (setGroupItr->second.find(pair.second.group) != setGroupItr->second.end())
                entries.push_back(&pair.second);
        }

        std::sort(entries.begin(), entries.end(), [](TuJianSetEntry const* left, TuJianSetEntry const* right)
        {
            if (left->group != right->group)
                return left->group < right->group;
            if (left->requiredActivationCount != right->requiredActivationCount)
            {
                if (left->requiredActivationCount == 0)
                    return false;
                if (right->requiredActivationCount == 0)
                    return true;
                return left->requiredActivationCount < right->requiredActivationCount;
            }
            return left->id < right->id;
        });

        bool first = true;
        for (TuJianSetEntry const* entry : entries)
        {
            if (!entry)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry->id << '^'
                    << entry->group << '^'
                    << entry->requiredActivationCount << '^'
                    << SanitizeAddonText(entry->activationDesc) << '^'
                    << FormatActivationTemplatesForClient(entry->activationTemplateIds) << '^'
                    << SanitizeAddonText(entry->comment);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianStateToPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        std::vector<PlayerActivationRecord> records;

        auto itr = playerActivationCache.find(playerGuid);
        if (itr != playerActivationCache.end())
            records = itr->second;

        std::sort(records.begin(), records.end(), [](PlayerActivationRecord const& left, PlayerActivationRecord const& right)
        {
            if (left.currentItemEntry != right.currentItemEntry)
                return left.currentItemEntry < right.currentItemEntry;
            return left.tuJianId < right.tuJianId;
        });

        std::ostringstream payload;
        payload << "TJ_STATE:";

        bool first = true;
        for (PlayerActivationRecord const& record : records)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << record.currentItemEntry << '^'
                    << record.tuJianId << '^'
                    << record.setId << '^'
                    << record.currentLevel;
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianPageStateToPlayer(Player* player, uint32 chapterId)
    {
        if (!player)
            return;

        std::ostringstream payload;
        payload << "TJ_PAGESTATE:" << chapterId << ':';

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr == playerActivationCache.end())
        {
            SendAddonPayload(player, payload.str());
            return;
        }

        std::vector<PlayerActivationRecord> records;
        records.reserve(activationItr->second.size());

        for (PlayerActivationRecord const& record : activationItr->second)
        {
            if (record.currentItemEntry == 0 || record.currentLevel == 0)
                continue;

            if (GetChapterIdForItemEntry(record.currentItemEntry) == chapterId)
                records.push_back(record);
        }

        std::sort(records.begin(), records.end(), [](PlayerActivationRecord const& left, PlayerActivationRecord const& right)
        {
            if (left.currentItemEntry != right.currentItemEntry)
                return left.currentItemEntry < right.currentItemEntry;
            return left.tuJianId < right.tuJianId;
        });

        bool first = true;
        for (PlayerActivationRecord const& record : records)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << record.currentItemEntry << '^'
                    << record.tuJianId << '^'
                    << record.setId << '^'
                    << record.currentLevel;
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianSummaryToPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 activeCount = 0;
        uint32 totalLevel = 0;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr != playerActivationCache.end())
        {
            for (PlayerActivationRecord const& record : activationItr->second)
            {
                if (record.currentItemEntry == 0 || record.currentLevel == 0)
                    continue;

                ++activeCount;
                totalLevel += record.currentLevel;
            }
        }

        std::ostringstream payload;
        payload << "TJ_SUMMARY:"
                << tuJianEntries.size() << '^'
                << activeCount << '^'
                << totalLevel;
        SendAddonPayload(player, payload.str());
    }

    void SendTuJianBootstrapDataToPlayer(Player* player)
    {
        SendTuJianMenuIndexToPlayer(player);
        SendTuJianSummaryToPlayer(player);
    }

    void SendTuJianPageDataToPlayer(Player* player, uint32 chapterId)
    {
        SendTuJianPageToPlayer(player, chapterId);
        SendTuJianPageSetsToPlayer(player, chapterId);
        SendTuJianPageStateToPlayer(player, chapterId);
    }

    void SendTuJianAllDataToPlayer(Player* player)
    {
        SendTuJianBootstrapDataToPlayer(player);

        if (!tuJianMenuEntries.empty())
            SendTuJianPageDataToPlayer(player, tuJianMenuEntries.front().chapterId);
    }

    void SendTuJianActionResult(Player* player, std::string const& action, bool success, uint32 itemEntry, uint32 level, std::string const& message)
    {
        std::ostringstream payload;
        payload << "TJ_RESULT:"
                << SanitizeAddonText(action) << '^'
                << (success ? "OK" : "FAIL") << '^'
                << itemEntry << '^'
                << level << '^'
                << SanitizeAddonText(message);
        SendAddonPayload(player, payload.str());
    }

    uint8 GetAttackSlotForVirtualItem(ItemTemplate const* proto)
    {
        if (!proto)
            return MAX_ATTACK;

        switch (proto->InventoryType)
        {
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_2HWEAPON:
                return BASE_ATTACK;
            case INVTYPE_WEAPONOFFHAND:
                return OFF_ATTACK;
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
                return RANGED_ATTACK;
            default:
                return MAX_ATTACK;
        }
    }

    void GetVirtualWeaponDamageBonus(Player* player, uint8 attackType, uint8 damageIndex, float& minDamage, float& maxDamage)
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;

        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto itr = playerWeaponDamageBonuses.find(playerGuid);
        if (itr == playerWeaponDamageBonuses.end())
            return;

        if (attackType >= MAX_ATTACK || damageIndex >= MAX_ITEM_PROTO_DAMAGES)
            return;

        minDamage = itr->second.minDamage[attackType][damageIndex];
        maxDamage = itr->second.maxDamage[attackType][damageIndex];
    }

    void RefreshPlayerStats(Player* player)
    {
        if (!player)
            return;

        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();
    }

    uint32 LoadTuJianData()
    {
        tuJianEntries.clear();

        bool hasAttributeEffectModeColumn = WorldDatabase.Query(
            "SHOW COLUMNS FROM `_图鉴系统` LIKE '属性生效模式'") != nullptr;
        bool hasFixedAllStatsValueColumn = WorldDatabase.Query(
            "SHOW COLUMNS FROM `_图鉴系统` LIKE '固定全属性值'") != nullptr;
        bool hasAllStatsPercentColumn = WorldDatabase.Query(
            "SHOW COLUMNS FROM `_图鉴系统` LIKE '全属性百分比'") != nullptr;

        std::string query =
            "SELECT tj.`注释`, COALESCE(it.`name`, ''), tj.`id`, tj.`一级菜单名称`, tj.`一级菜单图标`, tj.`二级菜单名称1`, tj.`二级菜单名称2`, tj.`二级菜单图标`, "
            "tj.`第几页`, tj.`等级`, tj.`最大等级`, tj.`物品entry`, tj.`套装ID`, tj.`激活需求`, tj.`激活后执行GM命令`";
        if (hasAttributeEffectModeColumn)
            query += ", tj.`属性生效模式`";
        if (hasFixedAllStatsValueColumn)
            query += ", tj.`固定全属性值`";
        if (hasAllStatsPercentColumn)
            query += ", tj.`全属性百分比`";
        query += " FROM `_图鉴系统` tj LEFT JOIN `item_template` it ON it.`entry` = tj.`物品entry`";

        QueryResult result = WorldDatabase.Query(query);

        if (!result)
        {
            return 0;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            TuJianEntry entry;
            entry.comment = fields[0].Get<std::string>();
            entry.itemName = fields[1].Get<std::string>();
            entry.id = fields[2].Get<uint32>();
            entry.menu1Name = fields[3].Get<std::string>();
            entry.menu1Icon = fields[4].Get<std::string>();
            entry.menu2Name1 = fields[5].Get<std::string>();
            entry.menu2Name2 = fields[6].Get<std::string>();
            entry.menu2Icon = fields[7].Get<std::string>();
            entry.page = fields[8].Get<uint32>();
            entry.level = fields[9].Get<uint32>();
            entry.maxLevel = fields[10].Get<uint32>();
            entry.itemEntry = fields[11].Get<uint32>();
            entry.setId = fields[12].Get<uint32>();
            entry.activationRequirement = fields[13].Get<uint32>();
            entry.activationCommand = fields[14].Get<std::string>();

            uint8 nextFieldIndex = 15;
            entry.attributeEffectMode = hasAttributeEffectModeColumn ? fields[nextFieldIndex++].Get<uint8>() : TUJIAN_ATTR_MODE_EQUIP;
            entry.fixedAllStatsValue = hasFixedAllStatsValueColumn ? fields[nextFieldIndex++].Get<int256>() : 0;
            entry.allStatsPercent = hasAllStatsPercentColumn ? fields[nextFieldIndex++].Get<int256>() : 0;

            if (entry.attributeEffectMode != TUJIAN_ATTR_MODE_FIXED &&
                entry.attributeEffectMode != TUJIAN_ATTR_MODE_EQUIP &&
                entry.attributeEffectMode != TUJIAN_ATTR_MODE_PERCENT)
                entry.attributeEffectMode = TUJIAN_ATTR_MODE_EQUIP;
            if (entry.fixedAllStatsValue < 0)
                entry.fixedAllStatsValue = 0;
            if (entry.allStatsPercent < 0)
                entry.allStatsPercent = 0;

            tuJianEntries[entry.itemEntry] = entry;
            ++count;
        } while (result->NextRow());

        return count;
    }

    uint32 LoadTuJianSetData()
    {
        tuJianSetEntries.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `注释`, `id`, `组`, `需要激活数量`, `激活描述`, `激活模版_物品技能_多个逗号隔开`, `激活后执行GM命令` "
            "FROM `_图鉴系统_套装`");

        if (!result)
        {
            return 0;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            TuJianSetEntry entry;
            entry.comment = fields[0].Get<std::string>();
            entry.id = fields[1].Get<uint32>();
            entry.group = fields[2].Get<uint32>();
            entry.requiredActivationCount = fields[3].Get<uint32>();
            entry.activationDesc = fields[4].Get<std::string>();
            entry.activationTemplates = fields[5].Get<std::string>();
            entry.activationTemplateIds = ParseUint32List(entry.activationTemplates);
            entry.activationCommand = fields[6].Get<std::string>();

            tuJianSetEntries[entry.id] = entry;
            ++count;
        } while (result->NextRow());

        return count;
    }

    void LoadAllTuJianData()
    {
        LoadTuJianData();
        LoadTuJianSetData();
        RebuildTuJianIndexes();
    }

    void ClearPlayerCache(uint32 playerGuid)
    {
        playerActivationCache.erase(playerGuid);
        playerFallbackAppliedItems.erase(playerGuid);
        playerAggregatedItemBonuses.erase(playerGuid);
        playerItemSetContributions.erase(playerGuid);
        playerWeaponDamageBonuses.erase(playerGuid);
        playerFixedAllStatsBonus.erase(playerGuid);
        playerAllStatsPercentBonus.erase(playerGuid);
    }

    void RemovePlayerVirtualItems(Player* player, bool refreshStats)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto itr = playerVirtualItems.find(playerGuid);
        if (itr != playerVirtualItems.end())
        {
            for (auto appliedItr = itr->second.rbegin(); appliedItr != itr->second.rend(); ++appliedItr)
            {
                if (!appliedItr->item)
                    continue;

#ifdef MODULE_ITEM_SKILLS
                if (appliedItr->hasExternalSkills)
                {
                    sItemSkillsEffects->RemoveItemSkillEffectsByGuid(player, appliedItr->item->GetGUID().GetCounter());
                    sItemSkillsManager->RemoveExternalItemSkills(playerGuid, appliedItr->item->GetGUID().GetCounter());
                }
#endif

                if (!appliedItr->allowEquipSpell)
                    blockedVirtualEquipSpellItemGuids.erase(appliedItr->item->GetGUID().GetCounter());

                if (appliedItr->applyEquipSpell)
                    player->ApplyItemEquipSpell(appliedItr->item.get(), false);

                if (appliedItr->applyItemMods)
                {
                    ItemTemplate const* proto = appliedItr->item->GetTemplate();
                    if (proto && proto->ItemSet)
                        RemoveItemsSetItem(player, proto);

                    player->_ApplyItemMods(appliedItr->item.get(), appliedItr->slot, false);
                }
            }

            playerVirtualItems.erase(itr);
        }

        auto setItr = playerItemSetContributions.find(playerGuid);
        if (setItr != playerItemSetContributions.end())
        {
            for (auto appliedItr = setItr->second.rbegin(); appliedItr != setItr->second.rend(); ++appliedItr)
            {
                if (appliedItr->setId == 0 || appliedItr->itemCount == 0)
                    continue;

                RemoveItemSetContribution(player, appliedItr->setId, appliedItr->itemCount);
            }

            playerItemSetContributions.erase(setItr);
        }

        auto aggregatedItr = playerAggregatedItemBonuses.find(playerGuid);
        if (aggregatedItr != playerAggregatedItemBonuses.end())
        {
            ApplyAggregatedItemBonuses(player, aggregatedItr->second, false);
            playerAggregatedItemBonuses.erase(aggregatedItr);
        }

        auto fallbackItr = playerFallbackAppliedItems.find(playerGuid);
        if (fallbackItr != playerFallbackAppliedItems.end())
        {
            for (auto appliedItr = fallbackItr->second.rbegin(); appliedItr != fallbackItr->second.rend(); ++appliedItr)
            {
                if (appliedItr->itemEntry == 0 || appliedItr->applyCount == 0)
                    continue;

                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(appliedItr->itemEntry);
                if (!proto)
                    continue;

                for (uint32 i = 0; i < appliedItr->applyCount; ++i)
                    player->_ApplyItemBonuses(proto, VIRTUAL_TUJIAN_SLOT, false);
            }

            playerFallbackAppliedItems.erase(fallbackItr);
        }

        playerWeaponDamageBonuses.erase(playerGuid);

        auto fixedBonusItr = playerFixedAllStatsBonus.find(playerGuid);
        if (fixedBonusItr != playerFixedAllStatsBonus.end())
        {
            ApplyFixedAllStatsBonus(player, fixedBonusItr->second, false);
            playerFixedAllStatsBonus.erase(fixedBonusItr);
        }

        auto percentBonusItr = playerAllStatsPercentBonus.find(playerGuid);
        if (percentBonusItr != playerAllStatsPercentBonus.end())
        {
            ApplyAllStatsPercentBonus(player, percentBonusItr->second, false);
            playerAllStatsPercentBonus.erase(percentBonusItr);
        }

#ifdef MODULE_ITEM_SKILLS
        sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
#endif

        if (refreshStats)
            RefreshPlayerStats(player);
    }

    size_t LoadPlayerActivationData(Player* player)
    {
        if (!player)
            return 0;

        uint32 playerGuid = player->GetGUID().GetCounter();
        playerActivationCache.erase(playerGuid);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `图鉴ID`, `套装ID`, `当前等级`, `当前物品entry` FROM `_玩家激活图鉴` WHERE `玩家GUID` = {}",
            playerGuid);

        if (!result)
            return 0;

        auto& records = playerActivationCache[playerGuid];
        do
        {
            Field* fields = result->Fetch();

            PlayerActivationRecord record;
            record.tuJianId = fields[0].Get<uint32>();
            record.setId = fields[1].Get<uint32>();
            record.currentLevel = fields[2].Get<uint32>();
            record.currentItemEntry = fields[3].Get<uint32>();

            if (record.tuJianId != 0 && record.currentItemEntry != 0 && record.currentLevel != 0)
                records.push_back(record);
        } while (result->NextRow());

        return records.size();
    }

    void ApplyPlayerActivationData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        bool hadCachedState =
            playerVirtualItems.find(playerGuid) != playerVirtualItems.end() ||
            playerFallbackAppliedItems.find(playerGuid) != playerFallbackAppliedItems.end() ||
            playerAggregatedItemBonuses.find(playerGuid) != playerAggregatedItemBonuses.end() ||
            playerItemSetContributions.find(playerGuid) != playerItemSetContributions.end() ||
            playerWeaponDamageBonuses.find(playerGuid) != playerWeaponDamageBonuses.end() ||
            playerFixedAllStatsBonus.find(playerGuid) != playerFixedAllStatsBonus.end() ||
            playerAllStatsPercentBonus.find(playerGuid) != playerAllStatsPercentBonus.end();

        RemovePlayerVirtualItems(player, false);

        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr == playerActivationCache.end() || activationItr->second.empty())
        {
            if (hadCachedState)
                RefreshPlayerStats(player);

            return;
        }

        size_t activationRecordCount = activationItr->second.size();
        size_t equipModeActivationCount = 0;
        size_t itemSetContributionCount = 0;
        size_t externalSkillCarrierCount = 0;

        bool batchRestoreCanModifyStats = player->CanModifyStats();
        if (batchRestoreCanModifyStats)
            player->SetCanModifyStats(false);

        std::vector<VirtualAppliedItem> appliedItems;
        appliedItems.reserve(activationRecordCount);
        std::vector<DirectAppliedItem> fallbackAppliedItems;
        fallbackAppliedItems.reserve(activationRecordCount);
        AggregatedItemBonusCache aggregatedBonusCache;
        std::unordered_map<uint32, uint32> aggregatedItemSetCounts;
        aggregatedItemSetCounts.reserve(activationRecordCount);
        PlayerWeaponDamageBonusCache weaponDamageBonuses;
        int256 totalFixedAllStatsBonus = 0;
        int256 totalAllStatsPercentBonus = 0;

#ifdef MODULE_ITEM_SKILLS
        std::unordered_map<uint32, std::unordered_set<uint32>> activatedTuJianIdsByGroup;
        std::unordered_map<uint32, uint32> carrierItemEntryByGroup;
        activatedTuJianIdsByGroup.reserve(activationRecordCount);
        carrierItemEntryByGroup.reserve(activationRecordCount);
#endif

        for (PlayerActivationRecord const& record : activationItr->second)
        {
            uint32 groupId = ResolveSetGroupId(record.setId);

#ifdef MODULE_ITEM_SKILLS
            if (groupId != 0)
            {
                if (record.tuJianId != 0)
                    activatedTuJianIdsByGroup[groupId].insert(record.tuJianId);

                if (record.currentItemEntry != 0 && carrierItemEntryByGroup[groupId] == 0)
                    carrierItemEntryByGroup[groupId] = record.currentItemEntry;
            }
#endif

            auto tuJianItr = tuJianEntries.find(record.currentItemEntry);
            if (tuJianItr == tuJianEntries.end())
            {
                LOG_WARN("module", "mod-tujian-system: 玩家 {} 的图鉴物品 {} 未在 world 配置中找到。", player->GetName(), record.currentItemEntry);
                continue;
            }

            TuJianEntry const& tuJian = tuJianItr->second;
            if (tuJian.attributeEffectMode == TUJIAN_ATTR_MODE_FIXED)
            {
                totalFixedAllStatsBonus = AddInt256Saturated(totalFixedAllStatsBonus, tuJian.fixedAllStatsValue);
                continue;
            }
            if (tuJian.attributeEffectMode == TUJIAN_ATTR_MODE_PERCENT)
            {
                totalAllStatsPercentBonus = AddInt256Saturated(totalAllStatsPercentBonus, tuJian.allStatsPercent);
                continue;
            }

            uint32 applyCount = GetTuJianApplyCount(tuJian, record.currentLevel);
            if (applyCount == 0)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(record.currentItemEntry);
            if (!proto)
            {
                LOG_WARN("module", "mod-tujian-system: 玩家 {} 的图鉴物品 {} 缺少 item_template 配置，无法应用装备属性。", player->GetName(), record.currentItemEntry);
                continue;
            }

            ++equipModeActivationCount;
            if (CanUseFastItemBonusPath(proto))
            {
                AccumulateItemBonuses(player, proto, applyCount, aggregatedBonusCache);
            }
            else
            {
                for (uint32 i = 0; i < applyCount; ++i)
                    player->_ApplyItemBonuses(proto, VIRTUAL_TUJIAN_SLOT, true);

                DirectAppliedItem fallbackApplied;
                fallbackApplied.itemEntry = record.currentItemEntry;
                fallbackApplied.applyCount = applyCount;
                fallbackAppliedItems.push_back(fallbackApplied);
            }

            AddWeaponDamageBonus(weaponDamageBonuses, proto, applyCount);

            bool needItemSet = proto->ItemSet != 0 && groupId == 0;
            bool needEquipSpell = ItemTemplateHasEquipSpell(proto);

            if (needItemSet)
                aggregatedItemSetCounts[proto->ItemSet] += applyCount;

            if (!needEquipSpell)
                continue;

            std::unique_ptr<Item> tempItem(Item::CreateItem(record.currentItemEntry, 1, player, true, 0));
            if (!tempItem)
            {
                LOG_WARN("module", "mod-tujian-system: 玩家 {} 的图鉴物品 {} 创建辅助虚拟物品失败，原生套装/装备法术不会生效。", player->GetName(), record.currentItemEntry);
                continue;
            }

            tempItem->SetOwnerGUID(player->GetGUID());
            tempItem->SetSlot(VIRTUAL_TUJIAN_SLOT);

            player->ApplyItemEquipSpell(tempItem.get(), true);

            VirtualAppliedItem applied;
            applied.slot = VIRTUAL_TUJIAN_SLOT;
            applied.tuJianId = record.tuJianId;
            applied.setId = record.setId;
            applied.allowEquipSpell = true;
            applied.applyEquipSpell = true;
            applied.item = std::move(tempItem);
            appliedItems.push_back(std::move(applied));
        }

        bool hasAggregatedBonuses = aggregatedBonusCache.HasAnyValue();

        if (!fallbackAppliedItems.empty())
            playerFallbackAppliedItems[playerGuid] = std::move(fallbackAppliedItems);
        if (equipModeActivationCount > 0)
            playerWeaponDamageBonuses[playerGuid] = weaponDamageBonuses;

        if (hasAggregatedBonuses)
        {
            ApplyAggregatedItemBonuses(player, aggregatedBonusCache, true);
            playerAggregatedItemBonuses[playerGuid] = aggregatedBonusCache;
        }

        if (totalFixedAllStatsBonus > 0)
        {
            ApplyFixedAllStatsBonus(player, totalFixedAllStatsBonus, true);
            playerFixedAllStatsBonus[playerGuid] = totalFixedAllStatsBonus;
        }

        if (totalAllStatsPercentBonus > 0)
        {
            ApplyAllStatsPercentBonus(player, totalAllStatsPercentBonus, true);
            playerAllStatsPercentBonus[playerGuid] = totalAllStatsPercentBonus;
        }

        if (!aggregatedItemSetCounts.empty())
        {
            // 批量施加套装法术时先暂停属性即时重算，最后统一 RefreshPlayerStats。
            bool restoreCanModifyStats = player->CanModifyStats();
            if (restoreCanModifyStats)
                player->SetCanModifyStats(false);

            std::vector<AggregatedItemSetContribution> contributions;
            contributions.reserve(aggregatedItemSetCounts.size());

            for (auto const& pair : aggregatedItemSetCounts)
            {
                if (pair.first == 0 || pair.second == 0)
                    continue;

                if (!ApplyItemSetContribution(player, pair.first, pair.second))
                    continue;

                AggregatedItemSetContribution contribution;
                contribution.setId = pair.first;
                contribution.itemCount = pair.second;
                contributions.push_back(contribution);
                ++itemSetContributionCount;
            }

            if (restoreCanModifyStats)
                player->SetCanModifyStats(true);

            if (!contributions.empty())
                playerItemSetContributions[playerGuid] = std::move(contributions);
        }

#ifdef MODULE_ITEM_SKILLS
        for (auto const& pair : activatedTuJianIdsByGroup)
        {
            uint32 groupId = pair.first;
            uint32 activatedCount = static_cast<uint32>(pair.second.size());
            if (groupId == 0 || activatedCount == 0)
                continue;

            uint32 carrierItemEntry = 0;
            auto carrierItr = carrierItemEntryByGroup.find(groupId);
            if (carrierItr != carrierItemEntryByGroup.end())
                carrierItemEntry = carrierItr->second;

            if (carrierItemEntry == 0)
            {
                auto defaultCarrierItr = tuJianDefaultCarrierItemByGroup.find(groupId);
                if (defaultCarrierItr != tuJianDefaultCarrierItemByGroup.end())
                    carrierItemEntry = defaultCarrierItr->second;
            }

            if (carrierItemEntry == 0)
            {
                LOG_WARN("module", "mod-tujian-system: 套装组 {} 未找到可用的图鉴物品作为技能载体。", groupId);
                continue;
            }

            auto setIdsItr = tuJianSetEntryIdsByGroup.find(groupId);
            if (setIdsItr == tuJianSetEntryIdsByGroup.end())
                continue;

            for (uint32 setEntryId : setIdsItr->second)
            {
                auto setEntryItr = tuJianSetEntries.find(setEntryId);
                if (setEntryItr == tuJianSetEntries.end())
                    continue;

                TuJianSetEntry const& setEntry = setEntryItr->second;
                if (setEntry.effectiveRequiredActivationCount == 0 || activatedCount < setEntry.effectiveRequiredActivationCount)
                    continue;

                if (setEntry.activationTemplateIds.empty())
                    continue;

                std::unique_ptr<Item> tempItem(Item::CreateItem(carrierItemEntry, 1, player, true, 0));
                if (!tempItem)
                {
                    LOG_WARN("module", "mod-tujian-system: 套装组 {} 的档位 {} 创建虚拟技能载体物品失败。", groupId, setEntry.id);
                    continue;
                }

                tempItem->SetOwnerGUID(player->GetGUID());
                tempItem->SetSlot(VIRTUAL_TUJIAN_SLOT);

                sItemSkillsManager->SetExternalItemSkills(
                    playerGuid, tempItem->GetGUID().GetCounter(), carrierItemEntry, setEntry.activationTemplateIds);
                sItemSkillsEffects->ApplyItemSkillEffects(player, tempItem.get());

                VirtualAppliedItem applied;
                applied.slot = VIRTUAL_TUJIAN_SLOT;
                applied.tuJianId = 0;
                applied.setId = groupId;
                applied.allowEquipSpell = true;
                applied.applyItemMods = false;
                applied.hasExternalSkills = true;
                applied.item = std::move(tempItem);
                appliedItems.push_back(std::move(applied));
                ++externalSkillCarrierCount;
            }
        }

        sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
#endif

        if (!appliedItems.empty())
            playerVirtualItems[playerGuid] = std::move(appliedItems);

        if (batchRestoreCanModifyStats)
            player->SetCanModifyStats(true);

        bool needRefreshStats =
            hadCachedState ||
            equipModeActivationCount > 0 ||
            hasAggregatedBonuses ||
            itemSetContributionCount > 0 ||
            totalFixedAllStatsBonus > 0 ||
            totalAllStatsPercentBonus > 0 ||
            externalSkillCarrierCount > 0;

        if (needRefreshStats)
            RefreshPlayerStats(player);
    }

    void UpsertPlayerActivationRecord(uint32 playerGuid, PlayerActivationRecord const& newRecord)
    {
        auto& records = playerActivationCache[playerGuid];
        auto itr = std::find_if(records.begin(), records.end(), [&newRecord](PlayerActivationRecord const& record)
        {
            return record.tuJianId == newRecord.tuJianId;
        });

        if (itr != records.end())
            *itr = newRecord;
        else
            records.push_back(newRecord);
    }

    bool IsTuJianActivatedForPlayer(uint32 playerGuid, uint32 tuJianId)
    {
        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr == playerActivationCache.end())
            return false;

        return std::any_of(
            activationItr->second.begin(),
            activationItr->second.end(),
            [tuJianId](PlayerActivationRecord const& record)
            {
                return record.tuJianId == tuJianId && record.currentLevel > 0;
            });
    }

    bool CheckAndConsumeActivationRequirement(Player* player, TuJianEntry const& tuJian, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        if (tuJian.activationRequirement == 0)
            return true;

#ifdef MODULE_REQUIREMENT_TEMPLATE
        if (!sRequirementSystem)
        {
            LOG_ERROR("module", "自定义UI图鉴: 图鉴ID {} 配置了激活需求 {}，但需求系统未加载",
                tuJian.id, tuJian.activationRequirement);

            if (player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[图鉴]|r 需求模板系统未加载，无法激活当前图鉴");

            if (failureMessage)
                *failureMessage = "需求系统未加载";

            return false;
        }

        bool meetsRequirements = sRequirementSystem->CheckRequirements(player, tuJian.activationRequirement, false);
        if (!meetsRequirements)
        {
            if (player->GetSession())
            {
                ChatHandler handler(player->GetSession());
                handler.SendSysMessage("|cffffcc00[图鉴]|r 不满足激活条件");
                sRequirementSystem->CheckRequirements(player, tuJian.activationRequirement, true);
            }

            if (failureMessage)
                *failureMessage = "不满足激活需求";

            return false;
        }

        if (!sRequirementSystem->ConsumeRequirements(player, tuJian.activationRequirement))
        {
            if (player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[图鉴]|r 激活需求消耗失败");

            if (failureMessage)
                *failureMessage = "需求消耗失败";

            return false;
        }
#else
        LOG_ERROR("module", "自定义UI图鉴: 图鉴ID {} 配置了激活需求 {}，但需求系统模块未编译",
            tuJian.id, tuJian.activationRequirement);

        if (player->GetSession())
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[图鉴]|r 需求模板系统模块未编译，无法激活当前图鉴");

        if (failureMessage)
            *failureMessage = "需求系统未编译";

        return false;
#endif

        return true;
    }

    // trans 非空时写库追加到事务（一键收集批量提交），否则单条异步执行
    bool ActivateTuJianForPlayer(Player* player, uint32 itemEntry, uint32 level, std::string* failureMessage = nullptr, bool applyImmediately = true, CharacterDatabaseTransaction* trans = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        auto itr = tuJianEntries.find(itemEntry);
        if (itr == tuJianEntries.end())
        {
            if (failureMessage)
                *failureMessage = "未找到图鉴配置";
            return false;
        }

        TuJianEntry const& tuJian = itr->second;
        uint32 applyLevel = std::max<uint32>(1, level == 0 ? 1 : level);
        if (tuJian.maxLevel > 0)
            applyLevel = std::min(applyLevel, tuJian.maxLevel);

        if (!CheckAndConsumeActivationRequirement(player, tuJian, failureMessage))
            return false;

        PlayerActivationRecord record;
        record.tuJianId = tuJian.id;
        record.setId = tuJian.setId;
        record.currentLevel = applyLevel;
        record.currentItemEntry = tuJian.itemEntry;

        uint32 playerGuid = player->GetGUID().GetCounter();
        if (trans)
            (*trans)->Append(
                "REPLACE INTO `_玩家激活图鉴` (`玩家GUID`, `图鉴ID`, `套装ID`, `当前等级`, `当前物品entry`) VALUES ({}, {}, {}, {}, {})",
                playerGuid, record.tuJianId, record.setId, record.currentLevel, record.currentItemEntry);
        else
            CharacterDatabase.Execute(
                "REPLACE INTO `_玩家激活图鉴` (`玩家GUID`, `图鉴ID`, `套装ID`, `当前等级`, `当前物品entry`) VALUES ({}, {}, {}, {}, {})",
                playerGuid, record.tuJianId, record.setId, record.currentLevel, record.currentItemEntry);

        UpsertPlayerActivationRecord(playerGuid, record);
        if (applyImmediately)
            ApplyPlayerActivationData(player);
        return true;
    }

    uint32 CollectAvailableTuJianForPlayer(
        Player* player,
        uint32& activeSkipped,
        uint32& missingSkipped,
        uint32& failedCount,
        std::string& lastFailure,
        std::unordered_set<uint32>& touchedChapters)
    {
        activeSkipped = 0;
        missingSkipped = 0;
        failedCount = 0;
        lastFailure.clear();
        touchedChapters.clear();

        if (!player)
            return 0;

        uint32 activatedCount = 0;
        uint32 playerGuid = player->GetGUID().GetCounter();

        // 一键收集的写库聚合为单事务提交（此前每个图鉴一条独立 REPLACE）
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (auto const& pair : tuJianEntries)
        {
            TuJianEntry const& entry = pair.second;
            if (entry.itemEntry == 0 || entry.id == 0)
                continue;

            if (IsTuJianActivatedForPlayer(playerGuid, entry.id))
            {
                ++activeSkipped;
                continue;
            }

            if (!player->HasItemCount(entry.itemEntry, 1, false))
            {
                ++missingSkipped;
                continue;
            }

            std::string failureMessage;
            if (ActivateTuJianForPlayer(player, entry.itemEntry, 1, &failureMessage, false, &trans))
            {
                ++activatedCount;
                uint32 chapterId = GetChapterIdForEntry(entry);
                if (chapterId != 0)
                    touchedChapters.insert(chapterId);
            }
            else
            {
                ++failedCount;
                if (!failureMessage.empty())
                    lastFailure = failureMessage;
            }
        }

        if (activatedCount > 0)
        {
            CharacterDatabase.CommitTransaction(trans);
            ApplyPlayerActivationData(player);
        }

        return activatedCount;
    }

    bool DeactivateTuJianForPlayer(Player* player, uint32 itemEntry)
    {
        if (!player)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto cacheItr = playerActivationCache.find(playerGuid);

        uint32 tuJianId = 0;
        if (cacheItr != playerActivationCache.end())
        {
            auto recordItr = std::find_if(cacheItr->second.begin(), cacheItr->second.end(), [itemEntry](PlayerActivationRecord const& record)
            {
                return record.currentItemEntry == itemEntry;
            });

            if (recordItr != cacheItr->second.end())
            {
                tuJianId = recordItr->tuJianId;
                cacheItr->second.erase(recordItr);
            }
        }

        if (tuJianId == 0)
        {
            auto tuJianItr = tuJianEntries.find(itemEntry);
            if (tuJianItr == tuJianEntries.end())
                return false;

            tuJianId = tuJianItr->second.id;
        }

        CharacterDatabase.Execute(
            "DELETE FROM `_玩家激活图鉴` WHERE `玩家GUID` = {} AND `图鉴ID` = {}",
            playerGuid, tuJianId);

        if (cacheItr != playerActivationCache.end() && cacheItr->second.empty())
            playerActivationCache.erase(cacheItr);

        ApplyPlayerActivationData(player);
        return true;
    }
}

class TujianSystemWorldScript : public WorldScript
{
public:
    TujianSystemWorldScript() : WorldScript("TujianSystemWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        TujianSystem_Enable = sConfigMgr->GetOption<bool>("TujianSystem.Enable", true);
        TujianSystem_Announce = sConfigMgr->GetOption<bool>("TujianSystem.Announce", true);
    }

    void OnStartup() override
    {
        if (!TujianSystem_Enable)
            return;

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!TujianSystem_Enable || _initialized)
            return;

        _loadTime += diff;

        if (_loadTime >= 3000 && !_displayedLog)
        {
            if (TujianSystem_Enable && TujianSystem_Announce)
                LOG_INFO("server.loading", "→自定义UI图鉴系统√");

            _displayedLog = true;
        }

        if (_loadTime >= 3000 && _displayedLog)
        {
            LoadAllTuJianData();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class TujianSystemPlayerScript : public PlayerScript
{
public:
    TujianSystemPlayerScript() : PlayerScript("TujianSystemPlayerScript") { }

    bool OnPlayerCanApplyEquipSpell(Player* /*player*/, SpellInfo const* /*spellInfo*/, Item* item, bool apply, bool /*form_change*/) override
    {
        if (!apply || !item)
            return true;

        return blockedVirtualEquipSpellItemGuids.find(item->GetGUID().GetCounter()) == blockedVirtualEquipSpellItemGuids.end();
    }

    void OnPlayerApplyWeaponDamage(Player* player, uint8 slot, ItemTemplate const* /*proto*/, float& minDamage, float& maxDamage, uint8 damageIndex) override
    {
        if (!TujianSystem_Enable || !player)
            return;

        uint8 attackType = Player::GetAttackBySlot(slot);
        if (attackType == MAX_ATTACK)
            return;

        float bonusMinDamage = 0.0f;
        float bonusMaxDamage = 0.0f;
        GetVirtualWeaponDamageBonus(player, attackType, damageIndex, bonusMinDamage, bonusMaxDamage);

        minDamage += bonusMinDamage;
        maxDamage += bonusMaxDamage;
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!TujianSystem_Enable || !player)
            return;

        LoadPlayerActivationData(player);
        ApplyPlayerActivationData(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        using Clk = std::chrono::high_resolution_clock;
        auto t0 = Clk::now();
        auto step = [&](char const* name)
        {
            auto now = Clk::now();
            int64 ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
            t0 = now;
            if (ms >= 30)
                LOG_WARN("server.loading",
                    "[性能监控-图鉴登出] 阶段={} 角色={} 耗时={}ms", name, player->GetName(), ms);
        };

        // 【性能优化】登出时走快速清理路径，不做属性/aura 回滚：
        //   1) 玩家即将 CleanupsBeforeDelete + delete，aura 与属性都会被析构；
        //   2) _ApplyItemMods / ApplyItemEquipSpell / RemoveItemsSetItem / RefreshPlayerStats
        //      是主线程同步重路径，之前实测占用 400ms，是小退卡顿的主因之一；
        //   3) 所有 unique_ptr<Item> 在 map.erase 时会自动析构，GUID 键的外部缓存也仍会清理。
        uint32 playerGuid = player->GetGUID().GetCounter();

        // 需要按 item GUID 清理的外部缓存，保留
        auto itr = playerVirtualItems.find(playerGuid);
        if (itr != playerVirtualItems.end())
        {
            for (auto const& applied : itr->second)
            {
                if (!applied.item)
                    continue;

                uint32 itemGuidCounter = applied.item->GetGUID().GetCounter();

#ifdef MODULE_ITEM_SKILLS
                if (applied.hasExternalSkills)
                    sItemSkillsManager->RemoveExternalItemSkills(playerGuid, itemGuidCounter);
#endif

                if (!applied.allowEquipSpell)
                    blockedVirtualEquipSpellItemGuids.erase(itemGuidCounter);
            }

            playerVirtualItems.erase(itr); // 触发 unique_ptr<Item> 析构
        }

        // 其余都是玩家 GUID 索引的缓存，直接 erase 即可
        playerItemSetContributions.erase(playerGuid);
        playerAggregatedItemBonuses.erase(playerGuid);
        playerFallbackAppliedItems.erase(playerGuid);
        playerWeaponDamageBonuses.erase(playerGuid);
        playerFixedAllStatsBonus.erase(playerGuid);
        playerAllStatsPercentBonus.erase(playerGuid);
        step("FastCleanupVirtualItems");

        ClearPlayerCache(playerGuid);
        step("ClearPlayerCache");
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        if (trans)
            trans->Append(Acore::StringFormat("DELETE FROM `_玩家激活图鉴` WHERE `玩家GUID` = {}", guid));
        else
            CharacterDatabase.Execute("DELETE FROM `_玩家激活图鉴` WHERE `玩家GUID` = {}", guid);

        playerActivationCache.erase(guid);
        playerVirtualItems.erase(guid);
        playerFallbackAppliedItems.erase(guid);
        playerAggregatedItemBonuses.erase(guid);
        playerItemSetContributions.erase(guid);
        playerWeaponDamageBonuses.erase(guid);
        playerFixedAllStatsBonus.erase(guid);
        playerAllStatsPercentBonus.erase(guid);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!TujianSystem_Enable || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != TUJIAN_SYSTEM_ADDON_PREFIX)
            return;

        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "TUJIAN"))
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendTuJianAllDataToPlayer(player);
            return;
        }

        if (command == "REQ_BOOT")
        {
            SendTuJianBootstrapDataToPlayer(player);
            return;
        }

        if (command == "REQ_LIST" || command == "REQ_INDEX")
        {
            SendTuJianMenuIndexToPlayer(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            SendTuJianSummaryToPlayer(player);
            SendTuJianStateToPlayer(player);
            return;
        }

        if (command.rfind("REQ_PAGE:", 0) == 0)
        {
            uint32 chapterId = 0;

            try
            {
                chapterId = static_cast<uint32>(std::stoul(command.substr(std::strlen("REQ_PAGE:"))));
            }
            catch (...)
            {
                return;
            }

            if (chapterId == 0)
                return;

            SendTuJianPageDataToPlayer(player, chapterId);
            return;
        }

        if (command == "OPEN")
        {
            SendTuJianOpenUI(player);
            return;
        }

        if (command == "COLLECT")
        {
            uint32 activeSkipped = 0;
            uint32 missingSkipped = 0;
            uint32 failedCount = 0;
            std::string lastFailure;
            std::unordered_set<uint32> touchedChapters;
            uint32 activatedCount = CollectAvailableTuJianForPlayer(
                player, activeSkipped, missingSkipped, failedCount, lastFailure, touchedChapters);

            std::ostringstream message;
            message << "一键收集完成：激活 " << activatedCount
                    << " 个，跳过已激活 " << activeSkipped
                    << " 个，背包缺少 " << missingSkipped
                    << " 个";
            if (failedCount > 0)
            {
                message << "，失败 " << failedCount << " 个";
                if (!lastFailure.empty())
                    message << "，最后失败原因：" << lastFailure;
            }
            message << "。";

            SendTuJianActionResult(player, "COLLECT", activatedCount > 0 || failedCount == 0, 0, activatedCount, message.str());
            SendTuJianSummaryToPlayer(player);
            SendTuJianStateToPlayer(player);

            for (uint32 chapterId : touchedChapters)
                SendTuJianPageStateToPlayer(player, chapterId);
            return;
        }

        if (command.rfind("ACT:", 0) == 0)
        {
            std::string args = command.substr(std::strlen("ACT:"));
            size_t sep = args.find(':');
            if (sep == std::string::npos)
            {
                SendTuJianActionResult(player, "ACTIVATE", false, 0, 0, "参数错误");
                return;
            }

            uint32 itemEntry = 0;
            uint32 level = 1;

            try
            {
                itemEntry = static_cast<uint32>(std::stoul(args.substr(0, sep)));
                level = static_cast<uint32>(std::stoul(args.substr(sep + 1)));
            }
            catch (...)
            {
                SendTuJianActionResult(player, "ACTIVATE", false, 0, 0, "参数错误");
                return;
            }

            auto itr = tuJianEntries.find(itemEntry);
            if (itr == tuJianEntries.end())
            {
                SendTuJianActionResult(player, "ACTIVATE", false, itemEntry, level, "未找到图鉴配置");
                return;
            }

            uint32 applyLevel = std::max<uint32>(1, level == 0 ? 1 : level);
            if (itr->second.maxLevel > 0)
                applyLevel = std::min(applyLevel, itr->second.maxLevel);

            std::string failureMessage;
            bool success = ActivateTuJianForPlayer(player, itemEntry, applyLevel, &failureMessage);
            SendTuJianActionResult(player, "ACTIVATE", success, itemEntry, applyLevel,
                success ? "激活成功" : (failureMessage.empty() ? "激活失败" : failureMessage));
            SendTuJianSummaryToPlayer(player);

            uint32 chapterId = GetChapterIdForItemEntry(itemEntry);
            if (chapterId != 0)
                SendTuJianPageStateToPlayer(player, chapterId);
            return;
        }

        if (command.rfind("DEL:", 0) == 0)
        {
            std::string args = command.substr(std::strlen("DEL:"));
            uint32 itemEntry = 0;

            try
            {
                itemEntry = static_cast<uint32>(std::stoul(args));
            }
            catch (...)
            {
                SendTuJianActionResult(player, "REMOVE", false, 0, 0, "参数错误");
                return;
            }

            bool success = DeactivateTuJianForPlayer(player, itemEntry);
            SendTuJianActionResult(player, "REMOVE", success, itemEntry, 0, success ? "移除成功" : "移除失败");
            SendTuJianSummaryToPlayer(player);

            uint32 chapterId = GetChapterIdForItemEntry(itemEntry);
            if (chapterId != 0)
                SendTuJianPageStateToPlayer(player, chapterId);
            return;
        }
    }
};

class TujianSystemCommandScript : public CommandScript
{
public:
    TujianSystemCommandScript() : CommandScript("TujianSystemCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subCommandTable =
        {
            { "帮助", HandleHelpCommand, SEC_GAMEMASTER, Console::No },
            { "查看", HandleListCommand, SEC_GAMEMASTER, Console::No },
            { "列表", HandleListCommand, SEC_GAMEMASTER, Console::No },
            { "激活", HandleActivateCommand, SEC_GAMEMASTER, Console::No },
            { "移除", HandleDeactivateCommand, SEC_GAMEMASTER, Console::No },
            { "取消", HandleDeactivateCommand, SEC_GAMEMASTER, Console::No },
            { "删除", HandleDeactivateCommand, SEC_GAMEMASTER, Console::No },
            { "界面", HandleOpenUICommand, SEC_PLAYER, Console::No },
            { "重载", HandleReloadCommand, SEC_GAMEMASTER, Console::No },
            { "重新加载", HandleReloadCommand, SEC_GAMEMASTER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "图鉴", subCommandTable },
            { "装备图鉴", subCommandTable }
        };

        return commandTable;
    }

    static bool HandleHelpCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SendSysMessage("图鉴命令列表：");
        handler->SendSysMessage(".图鉴 帮助");
        handler->SendSysMessage(".图鉴 列表");
        handler->SendSysMessage(".图鉴 激活 物品ID [等级]");
        handler->SendSysMessage(".图鉴 移除 物品ID");
        handler->SendSysMessage(".图鉴 界面");
        handler->SendSysMessage(".装备图鉴 界面");
        handler->SendSysMessage(".图鉴 重载");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!TujianSystem_Enable)
        {
            handler->SendSysMessage("图鉴系统当前未启用。");
            return true;
        }

        SendTuJianOpenUI(player);
        return true;
    }

    static bool HandleActivateCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!TujianSystem_Enable)
        {
            handler->SendSysMessage("图鉴系统当前未启用。");
            return true;
        }

        std::istringstream iss(args ? args : "");
        uint32 itemEntry = 0;
        uint32 level = 1;
        if (!(iss >> itemEntry))
        {
            handler->SendSysMessage("用法: .图鉴 激活 物品ID [等级]");
            return false;
        }

        if (!(iss >> level))
            level = 1;

        auto itr = tuJianEntries.find(itemEntry);
        uint32 applyLevel = level;
        if (itr != tuJianEntries.end())
        {
            applyLevel = std::max<uint32>(1, level == 0 ? 1 : level);
            if (itr->second.maxLevel > 0)
                applyLevel = std::min(applyLevel, itr->second.maxLevel);
        }

        std::string failureMessage;
        if (!ActivateTuJianForPlayer(player, itemEntry, level, &failureMessage))
        {
            handler->PSendSysMessage("激活失败：{}。", failureMessage.empty() ? "未找到图鉴配置" : failureMessage);
            return false;
        }

        handler->PSendSysMessage("已激活图鉴物品 {}，当前叠加层数 {}。", itemEntry, applyLevel);
        return true;
    }

    static bool HandleDeactivateCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::istringstream iss(args ? args : "");
        uint32 itemEntry = 0;
        if (!(iss >> itemEntry))
        {
            handler->SendSysMessage("用法: .图鉴 移除 物品ID");
            return false;
        }

        if (!DeactivateTuJianForPlayer(player, itemEntry))
        {
            handler->PSendSysMessage("移除失败，未找到已激活图鉴物品 entry={}。", itemEntry);
            return false;
        }

        handler->PSendSysMessage("已移除图鉴物品 {} 的激活效果。", itemEntry);
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto itr = playerActivationCache.find(playerGuid);
        if (itr == playerActivationCache.end() || itr->second.empty())
        {
            handler->SendSysMessage("当前没有已激活的图鉴记录。");
            return true;
        }

        handler->SendSysMessage("当前已激活图鉴：");
        for (PlayerActivationRecord const& record : itr->second)
            handler->PSendSysMessage("- 图鉴ID={} 物品Entry={} 套装ID={} 叠加层数={}", record.tuJianId, record.currentItemEntry, record.setId, record.currentLevel);

        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        LoadAllTuJianData();
        handler->SendSysMessage("图鉴 world 配置已重新加载。");
        return true;
    }
};

void AddSC_mod_tujian_system()
{
    new TujianSystemWorldScript();
    new TujianSystemPlayerScript();
    new TujianSystemCommandScript();
}

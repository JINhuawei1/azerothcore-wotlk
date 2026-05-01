/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "Unit.h"
#include <cmath>
#include <limits>

inline bool _ModifyUInt32(bool apply, uint32& baseValue, int32& amount)
{
    // If amount is negative, change sign and value of apply.
    if (amount < 0)
    {
        apply = !apply;
        amount = -amount;
    }
    if (apply)
        baseValue += amount;
    else
    {
        // Make sure we do not get uint32 overflow.
        if (amount > int32(baseValue))
            amount = baseValue;
        baseValue -= amount;
    }
    return apply;
}

/*#######################################
########                         ########
########    UNIT STAT SYSTEM     ########
########                         ########
#######################################*/

void Unit::UpdateAllResistances()
{
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);
}

void Unit::UpdateDamagePhysical(WeaponAttackType attType)
{
    float totalMin = 0.f;
    float totalMax = 0.f;

    float tmpMin, tmpMax;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        CalculateMinMaxDamage(attType, false, true, tmpMin, tmpMax, i);

        totalMin += tmpMin;
        totalMax += tmpMax;
    }

    // 从数据库获取伤害上限（仅对玩家生效）
    Player* player = ToPlayer();
    if (player)
    {
        QueryResult result;
        switch (attType)
        {
            case BASE_ATTACK:
                result = WorldDatabase.Query("SELECT `主手伤害上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `主手伤害上限` > 0 ORDER BY `class_` DESC LIMIT 1", player->getClass());
                break;
            case OFF_ATTACK:
                result = WorldDatabase.Query("SELECT `副手伤害上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `副手伤害上限` > 0 ORDER BY `class_` DESC LIMIT 1", player->getClass());
                break;
            case RANGED_ATTACK:
                result = WorldDatabase.Query("SELECT `远程伤害上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `远程伤害上限` > 0 ORDER BY `class_` DESC LIMIT 1", player->getClass());
                break;
        }

        if (result)
        {
            Field* fields = result->Fetch();
            float damageLimit = fields[0].Get<float>();

            if (damageLimit > 0.0f)
            {
                // 检查溢出（负数或超过上限说明溢出了）
                if (totalMin < 0.0f || totalMin > damageLimit || std::isnan(totalMin) || std::isinf(totalMin))
                    totalMin = damageLimit;
                if (totalMax < 0.0f || totalMax > damageLimit || std::isnan(totalMax) || std::isinf(totalMax))
                    totalMax = damageLimit;
            }
        }
    }

    // 确保 min <= max
    if (totalMin > totalMax)
        totalMin = totalMax;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MINDAMAGE, totalMin);
            SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, totalMax);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, totalMin);
            SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, totalMax);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, totalMin);
            SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, totalMax);
            break;
    }
}

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);
    double extendedStatValue = 0.0;

    // 先调用脚本钩子，允许模块修改最终属性值（转生模块等会在这里加成）
    sScriptMgr->OnPlayerAfterUpdateStat(this, stat, value);

    constexpr double MAX_EXTENDED_VALUE = static_cast<double>(std::numeric_limits<uint64>::max());

    if (std::isnan(value) || value < 0.0f)
        extendedStatValue = 0.0;
    else if (std::isinf(value) || static_cast<double>(value) > MAX_EXTENDED_VALUE)
        extendedStatValue = MAX_EXTENDED_VALUE;
    else
        extendedStatValue = static_cast<double>(value);

    // 【重要】在钩子之后应用属性上限限制
    constexpr float MAX_SAFE_VALUE = 2000000000.0f;

    // 检查溢出（负数说明溢出了）
    if (value < 0.0f || value > MAX_SAFE_VALUE)
        value = MAX_SAFE_VALUE;

    // Apply attribute limits from database - 使用明确的字段名查询
    {
        const char* limitFieldName = nullptr;
        switch (stat)
        {
            case STAT_STRENGTH:  limitFieldName = "力量上限"; break;
            case STAT_AGILITY:   limitFieldName = "敏捷上限"; break;
            case STAT_STAMINA:   limitFieldName = "耐力上限"; break;
            case STAT_INTELLECT: limitFieldName = "智力上限"; break;
            case STAT_SPIRIT:    limitFieldName = "精神上限"; break;
            default: break;
        }

        if (limitFieldName)
        {
            QueryResult result = WorldDatabase.Query("SELECT `{}` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", limitFieldName, getClass());
            if (result)
            {
                Field* fields = result->Fetch();
                uint64 limitU64 = fields[0].Get<uint64>();
                double valueD = static_cast<double>(value);
                double limitD = static_cast<double>(limitU64);

                if (limitU64 > 0 && valueD > limitD)
                {
                    value = static_cast<float>(limitU64);
                }

                if (limitU64 > 0 && extendedStatValue > limitD)
                    extendedStatValue = limitD;
            }
        }
    }

    _extendedStats[stat] = static_cast<uint64>(extendedStatValue);

    SetStat(stat, static_cast<int32>(value));

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateShieldBlockValue();
            break;
        case STAT_AGILITY:
            UpdateArmor();
            UpdateAllCritPercentages();
            UpdateDodgePercentage();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            UpdateAllSpellCritChances();
            UpdateArmor();                                  //SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only armor currently
            break;
        default:
            break;
    }

    if (stat == STAT_STRENGTH)
    {
        UpdateAttackPowerAndDamage(false);
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(true);
    }
    else if (stat == STAT_AGILITY)
    {
        UpdateAttackPowerAndDamage(false);
        UpdateAttackPowerAndDamage(true);
    }
    else
    {
        // Need update (exist AP from stat auras)
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(false);
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(true);
    }

    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();

    // （调试日志已移除）

    // Update ratings in exist SPELL_AURA_MOD_RATING_FROM_STAT and only depends from stat
    uint32 mask = 0;
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffectList::const_iterator i = modRatingFromStat.begin(); i != modRatingFromStat.end(); ++i)
        if (Stats((*i)->GetMiscValueB()) == stat)
            mask |= (*i)->GetMiscValue();
    if (mask)
    {
        for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
            if (mask & (1 << rating))
                ApplyRatingMod(CombatRating(rating), 0, true);
    }
    return true;
}

void Player::ApplySpellPowerBonus(int64 amount, bool apply)
{
    if (amount < 0)
    {
        apply = !apply;
        amount = -amount;
    }

    if (apply)
    {
        uint64 addAmount = static_cast<uint64>(amount);
        if (addAmount > std::numeric_limits<uint64>::max() - m_baseSpellPower)
            m_baseSpellPower = std::numeric_limits<uint64>::max();
        else
            m_baseSpellPower += addAmount;
    }
    else
    {
        uint64 removeAmount = static_cast<uint64>(amount);
        m_baseSpellPower = removeAmount > m_baseSpellPower ? 0 : m_baseSpellPower - removeAmount;
    }

    constexpr int32 MAX_CLIENT_SPELL_POWER = 2000000000;
    int32 displayAmount = amount > MAX_CLIENT_SPELL_POWER ? MAX_CLIENT_SPELL_POWER : static_cast<int32>(amount);

    auto applyClientUIntMod = [this, MAX_CLIENT_SPELL_POWER](uint16 index, int32 val, bool add)
    {
        int64 current = static_cast<int64>(GetUInt32Value(index));
        current += add ? val : -val;
        if (current < 0)
            current = 0;
        else if (current > MAX_CLIENT_SPELL_POWER)
            current = MAX_CLIENT_SPELL_POWER;

        SetUInt32Value(index, static_cast<uint32>(current));
    };

    auto applyClientIntMod = [this, MAX_CLIENT_SPELL_POWER](uint16 index, int32 val, bool add)
    {
        int64 current = static_cast<int64>(GetInt32Value(index));
        current += add ? val : -val;
        if (current > MAX_CLIENT_SPELL_POWER)
            current = MAX_CLIENT_SPELL_POWER;
        else if (current < -MAX_CLIENT_SPELL_POWER)
            current = -MAX_CLIENT_SPELL_POWER;

        SetInt32Value(index, static_cast<int32>(current));
    };

    // For speed just update for client
    applyClientUIntMod(PLAYER_FIELD_MOD_HEALING_DONE_POS, displayAmount, apply);
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        applyClientIntMod(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, displayAmount, apply);

    UpdateSpellDamageAndHealingBonus();
}

void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    int32 healingBonus = SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL);

    // Get damage bonus for all schools
    int32 spellDamage[MAX_SPELL_SCHOOL];
    spellDamage[SPELL_SCHOOL_NORMAL] = 0;  // 物理学派不计算法术伤害
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        spellDamage[i] = SpellBaseDamageBonusDone(SpellSchoolMask(1 << i));

    auto getServerStatValue = [this](Stats stat) -> double
    {
        uint64 extendedValue = GetExtendedStat(stat);
        if (extendedValue > 0)
            return static_cast<double>(extendedValue);

        int32 displayValue = GetStat(stat);
        return displayValue > 0 ? static_cast<double>(displayValue) : 0.0;
    };

    auto toExtendedValue = [](double value) -> uint64
    {
        if (std::isnan(value) || value <= 0.0)
            return 0;
        if (std::isinf(value) || value >= static_cast<double>(std::numeric_limits<uint64>::max()))
            return std::numeric_limits<uint64>::max();
        return static_cast<uint64>(value);
    };

    double extendedHealingBonus = 0.0;
    std::array<double, MAX_SPELL_SCHOOL> extendedSpellDamage = { };
    double spellPowerMultiplier = 100.0;
    double healingMultiplier = 100.0;

    if (sConfigMgr->GetOption<bool>("ClassAttributes.Enable", false))
    {
        QueryResult result = WorldDatabase.Query("SELECT `法强倍率`, `治疗倍率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (result)
        {
            Field* fields = result->Fetch();
            spellPowerMultiplier = static_cast<double>(fields[0].Get<float>());
            healingMultiplier = static_cast<double>(fields[1].Get<float>());
        }
    }

    AuraEffectList const& mDamageDone = GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (AuraEffectList::const_iterator i = mDamageDone.begin(); i != mDamageDone.end(); ++i)
    {
        if ((*i)->GetSpellInfo()->EquippedItemClass != -1 || (*i)->GetSpellInfo()->EquippedItemInventoryTypeMask != 0)
            continue;

        for (int school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        {
            SpellSchoolMask schoolMask = SpellSchoolMask(1 << school);
            if (((*i)->GetMiscValue() & schoolMask) != 0)
                extendedSpellDamage[school] += static_cast<double>((*i)->GetAmount());
        }
    }

    AuraEffectList const& mHealingDone = GetAuraEffectsByType(SPELL_AURA_MOD_HEALING_DONE);
    for (AuraEffectList::const_iterator i = mHealingDone.begin(); i != mHealingDone.end(); ++i)
        if (!(*i)->GetMiscValue() || ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_ALL) != 0)
            extendedHealingBonus += static_cast<double>((*i)->GetAmount());

    double baseSpellPower = static_cast<double>(GetBaseSpellPowerBonus());
    double attackPowerBonus = GetExtendedTotalAttackPowerValue(BASE_ATTACK);

    extendedHealingBonus += baseSpellPower;
    for (int school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        extendedSpellDamage[school] += baseSpellPower;

    AuraEffectList const& mDamageDoneOfStatPercent = GetAuraEffectsByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mDamageDoneOfStatPercent.begin(); i != mDamageDoneOfStatPercent.end(); ++i)
    {
        Stats usedStat = Stats((*i)->GetMiscValueB());
        if (usedStat < STAT_STRENGTH || usedStat >= MAX_STATS)
            continue;

        double statValue = getServerStatValue(usedStat);
        if (statValue <= 0.0)
            continue;

        double statContribution = statValue * static_cast<double>((*i)->GetAmount()) / 100.0;
        for (int school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        {
            SpellSchoolMask schoolMask = SpellSchoolMask(1 << school);
            if (((*i)->GetMiscValue() & schoolMask) != 0)
                extendedSpellDamage[school] += statContribution;
        }
    }

    AuraEffectList const& mHealingDoneOfStatPercent = GetAuraEffectsByType(SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mHealingDoneOfStatPercent.begin(); i != mHealingDoneOfStatPercent.end(); ++i)
    {
        Stats usedStat = Stats((*i)->GetSpellInfo()->Effects[(*i)->GetEffIndex()].MiscValue);
        if (usedStat < STAT_STRENGTH || usedStat >= MAX_STATS)
            continue;

        double statValue = getServerStatValue(usedStat);
        if (statValue <= 0.0)
            continue;

        extendedHealingBonus += statValue * static_cast<double>((*i)->GetAmount()) / 100.0;
    }

    AuraEffectList const& mDamageDonebyAP = GetAuraEffectsByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER);
    for (AuraEffectList::const_iterator i = mDamageDonebyAP.begin(); i != mDamageDonebyAP.end(); ++i)
    {
        if (attackPowerBonus <= 0.0)
            continue;

        double attackPowerContribution = attackPowerBonus * static_cast<double>((*i)->GetAmount()) / 100.0;
        for (int school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        {
            SpellSchoolMask schoolMask = SpellSchoolMask(1 << school);
            if (((*i)->GetMiscValue() & schoolMask) != 0)
                extendedSpellDamage[school] += attackPowerContribution;
        }
    }

    AuraEffectList const& mHealingDonebyAP = GetAuraEffectsByType(SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER);
    for (AuraEffectList::const_iterator i = mHealingDonebyAP.begin(); i != mHealingDonebyAP.end(); ++i)
    {
        if (attackPowerBonus <= 0.0)
            continue;

        if ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_ALL)
            extendedHealingBonus += attackPowerBonus * static_cast<double>((*i)->GetAmount()) / 100.0;
    }

    if (spellPowerMultiplier > 0.0 && spellPowerMultiplier != 100.0)
        for (int school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
            extendedSpellDamage[school] = extendedSpellDamage[school] * spellPowerMultiplier / 100.0;

    if (healingMultiplier > 0.0 && healingMultiplier != 100.0)
        extendedHealingBonus = extendedHealingBonus * healingMultiplier / 100.0;

    int32 preHookHealingBonus = healingBonus;
    std::array<int32, MAX_SPELL_SCHOOL> preHookSpellDamage = { };
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        preHookSpellDamage[i] = spellDamage[i];

    // 调用钩子允许模块修改法术强度和治疗强度
    sScriptMgr->OnPlayerAfterUpdateSpellDamageAndHealing(this, healingBonus, spellDamage);

    if (extendedHealingBonus > 0.0 && preHookHealingBonus > 0 && healingBonus != preHookHealingBonus)
        extendedHealingBonus = extendedHealingBonus * static_cast<double>(healingBonus) / static_cast<double>(preHookHealingBonus);

    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        if (extendedSpellDamage[i] > 0.0 && preHookSpellDamage[i] > 0 && spellDamage[i] != preHookSpellDamage[i])
            extendedSpellDamage[i] = extendedSpellDamage[i] * static_cast<double>(spellDamage[i]) / static_cast<double>(preHookSpellDamage[i]);

    _extendedHealingBonus = toExtendedValue(extendedHealingBonus);
    _extendedSpellDamageBonuses[SPELL_SCHOOL_NORMAL] = 0;
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        _extendedSpellDamageBonuses[i] = toExtendedValue(extendedSpellDamage[i]);

    // 设置最终值到客户端显示字段
    SetStatInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, healingBonus);
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, spellDamage[i]);
}

bool Player::UpdateAllStats()
{
    if (!CanModifyStats())
        return true;

    // 【重要】使用 UpdateStats 而非直接 SetStat，确保钩子能修改属性值
    // 转身系统等模块通过 OnPlayerAfterUpdateStat 钩子应用加成
    for (int8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        UpdateStats(Stats(i));
    }

    // UpdateStats 已经调用了 UpdateArmor，但这里需要确保完整更新
    UpdateArmor();
    // calls UpdateAttackPowerAndDamage() in UpdateArmor for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
    UpdateAttackPowerAndDamage(true);
    UpdateMaxHealth();

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateDefenseBonusesMod();
    UpdateShieldBlockValue();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
    RecalculateRating(CR_ARMOR_PENETRATION);
    UpdateAllResistances();

    return true;
}

void Player::ApplySpellPenetrationBonus(int32 amount, bool apply)
{
    ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, -amount, apply);
    m_spellPenetrationItemMod += apply ? amount : -amount;
}

void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        // cant use GetTotalAuraModValue because of total pct multiplier :P
        float value = 0.0f;
        UnitMods unitMod = UnitMods(UNIT_MOD_RESISTANCE_START + school);

        value  = GetModifierValue(unitMod, BASE_VALUE);
        value *= GetModifierValue(unitMod, BASE_PCT);
        value += GetModifierValue(unitMod, TOTAL_VALUE);

        AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
        for(AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
        {
            if ((*i)->GetMiscValue() & (1 << (school - 1)))
                value += int32(GetStat(Stats((*i)->GetMiscValueB())) * (*i)->GetAmount() / 100.0f);
        }

        value *= GetModifierValue(unitMod, TOTAL_PCT);

        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Player::UpdateArmor()
{
    UnitMods unitMod = UNIT_MOD_ARMOR;

    double value = static_cast<double>(GetModifierValue(unitMod, BASE_VALUE));   // base armor (from items)
    value *= static_cast<double>(GetModifierValue(unitMod, BASE_PCT));           // armor percent from items
    value += static_cast<double>(GetExtendedStat(STAT_AGILITY) > 0 ? GetExtendedStat(STAT_AGILITY) : GetStat(STAT_AGILITY)) * 2.0; // armor bonus from stats
    value += static_cast<double>(GetModifierValue(unitMod, TOTAL_VALUE));

    //add dynamic flat mods
    AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
    {
        if ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
        {
            uint64 extendedStat = GetExtendedStat(Stats((*i)->GetMiscValueB()));
            double statValue = extendedStat > 0 ? static_cast<double>(extendedStat) : static_cast<double>(GetStat(Stats((*i)->GetMiscValueB())));
            value += statValue * static_cast<double>((*i)->GetAmount()) / 100.0;
        }
    }

    value *= static_cast<double>(GetModifierValue(unitMod, TOTAL_PCT));

    // 调用钩子允许模块修改护甲值
    float hookValue = 0.0f;
    if (value > static_cast<double>(std::numeric_limits<float>::max()))
        hookValue = std::numeric_limits<float>::max();
    else if (value > 0.0)
        hookValue = static_cast<float>(value);
    sScriptMgr->OnPlayerAfterUpdateArmor(this, hookValue);
    if (hookValue != static_cast<float>(value))
        value = static_cast<double>(hookValue);

    if (value < 0.0 || std::isnan(value))
        value = 0.0;
    else if (std::isinf(value) || value > static_cast<double>(std::numeric_limits<uint64>::max()))
        value = static_cast<double>(std::numeric_limits<uint64>::max());

    // Apply armor limit from database
    {
        Player* player = ToPlayer();
        if (player)
        {
            QueryResult result = WorldDatabase.Query("SELECT `护甲上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", player->getClass());
            if (result)
            {
                Field* fields = result->Fetch();
                uint64 armorLimit = fields[0].Get<uint64>();
                if (armorLimit > 0 && value > static_cast<double>(armorLimit))
                {
                    value = static_cast<double>(armorLimit);
                }
            }
        }
    }

    _extendedArmor = static_cast<uint64>(value);

    constexpr double MAX_CLIENT_ARMOR = 2000000000.0;
    double clientArmor = value > MAX_CLIENT_ARMOR ? MAX_CLIENT_ARMOR : value;
    SetArmor(static_cast<int32>(clientArmor));

    UpdateAttackPowerAndDamage();                           // armor dependent auras update for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
}

double Player::GetHealthBonusFromStamina()
{
    double stamina = GetExtendedStat(STAT_STAMINA) > 0 ? static_cast<double>(GetExtendedStat(STAT_STAMINA)) : static_cast<double>(GetStat(STAT_STAMINA));

    double baseStam = stamina < 20.0 ? stamina : 20.0;
    double moreStam = stamina - baseStam;

    // Apply stamina to health conversion rate from database
    float staminaToHealthRate = 1.0f; // Default 100% conversion rate
    QueryResult result = WorldDatabase.Query("SELECT `耐力转生命转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `耐力转生命转换率` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        staminaToHealthRate = fields[0].Get<float>() / 100.0f; // Convert percentage to decimal
    }

    double bonusHealth = baseStam + (moreStam * 5.0 * static_cast<double>(staminaToHealthRate));
    constexpr double MAX_SAFE_BONUS = static_cast<double>(std::numeric_limits<float>::max());

    if (bonusHealth < 0.0 || std::isnan(bonusHealth))
        return 0.0;
    if (std::isinf(bonusHealth) || bonusHealth > MAX_SAFE_BONUS)
        return MAX_SAFE_BONUS;

    return bonusHealth;
}

double Player::GetManaBonusFromIntellect()
{
    double intellect = GetExtendedStat(STAT_INTELLECT) > 0 ? static_cast<double>(GetExtendedStat(STAT_INTELLECT)) : static_cast<double>(GetStat(STAT_INTELLECT));

    double baseInt = intellect < 20.0 ? intellect : 20.0;
    double moreInt = intellect - baseInt;

    // Apply intellect to mana conversion rate from database
    float intellectToManaRate = 1.0f; // Default 100% conversion rate
    QueryResult result = WorldDatabase.Query("SELECT `智力转法力转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `智力转法力转换率` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        intellectToManaRate = fields[0].Get<float>() / 100.0f; // Convert percentage to decimal
    }

    double bonusMana = baseInt + (moreInt * 15.0 * static_cast<double>(intellectToManaRate));
    constexpr double MAX_SAFE_BONUS = static_cast<double>(std::numeric_limits<float>::max());

    if (bonusMana < 0.0 || std::isnan(bonusMana))
        return 0.0;
    if (std::isinf(bonusMana) || bonusMana > MAX_SAFE_BONUS)
        return MAX_SAFE_BONUS;

    return bonusMana;
}

void Player::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    uint64 oldExtendedMaxHealth = GetExtendedMaxHealth();
    uint64 oldExtendedHealth = GetExtendedHealth();
    uint32 oldClientMaxHealth = GetMaxHealth();
    bool wasFullHealth = _extendedHealth ? oldExtendedHealth >= oldExtendedMaxHealth : GetHealth() >= GetMaxHealth();

    float value = GetModifierValue(unitMod, BASE_VALUE) + static_cast<float>(GetCreateHealthForCombat());
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + static_cast<float>(GetHealthBonusFromStamina());
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    // 先调用钩子（转生模块等会在这里加成）
    sScriptMgr->OnPlayerAfterUpdateMaxHealth(this, value);

    double extendedMaxHealth = 0.0;
    constexpr double MAX_EXTENDED_VALUE = static_cast<double>(std::numeric_limits<uint64>::max());

    if (std::isnan(value) || value < 0.0f)
        extendedMaxHealth = 0.0;
    else if (std::isinf(value) || static_cast<double>(value) > MAX_EXTENDED_VALUE)
        extendedMaxHealth = MAX_EXTENDED_VALUE;
    else
        extendedMaxHealth = static_cast<double>(value);

    // 【重要】在钩子之后应用血量上限限制
    constexpr float MAX_SAFE_VALUE = 2000000000.0f;

    // 检查溢出（负数说明溢出了）
    if (value < 0.0f || value > MAX_SAFE_VALUE)
        value = MAX_SAFE_VALUE;

    // Apply health limit from database
    QueryResult result = WorldDatabase.Query("SELECT `血量上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        uint64 healthLimitU64 = fields[0].Get<uint64>();
        double valueD = static_cast<double>(value);
        double limitD = static_cast<double>(healthLimitU64);

        if (healthLimitU64 > 0 && valueD > limitD)
        {
            value = static_cast<float>(healthLimitU64);
        }

        if (healthLimitU64 > 0 && extendedMaxHealth > limitD)
            extendedMaxHealth = limitD;
    }

    uint64 newExtendedMaxHealth = static_cast<uint64>(extendedMaxHealth);
    _extendedMaxHealth = newExtendedMaxHealth;
    SetMaxHealth(static_cast<uint32>(value));
    if (wasFullHealth)
        SetExtendedHealth(GetExtendedMaxHealth());
    else if (oldClientMaxHealth && oldExtendedMaxHealth <= oldClientMaxHealth && oldExtendedHealth <= oldClientMaxHealth && (HasExtendedHealthForCombat() || GetExtendedMaxHealth() > GetMaxHealth()))
        SetExtendedHealthFromClientHealth(static_cast<uint32>(oldExtendedHealth));
    else
        SetExtendedHealth(oldExtendedHealth);

    SyncClientHealthFromExtended();
}

void Player::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + power);
    uint64 oldExtendedMaxPower = GetExtendedMaxPower(power);
    uint64 oldExtendedPower = GetPowerForCombat(power);
    uint32 oldClientMaxPower = GetMaxPower(power);
    bool wasFullPower = oldExtendedMaxPower ? oldExtendedPower >= oldExtendedMaxPower : GetPower(power) >= GetMaxPower(power);

    uint64 createPower = GetCreatePowerForCombat(power);
    float bonusPower = (power == POWER_MANA && createPower > 0) ? static_cast<float>(GetManaBonusFromIntellect()) : 0;

    float value = GetModifierValue(unitMod, BASE_VALUE) + static_cast<float>(createPower);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) +  bonusPower;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    // 先调用钩子（转生模块等会在这里加成）
    sScriptMgr->OnPlayerAfterUpdateMaxPower(this, power, value);

    double extendedMaxPower = 0.0;
    constexpr double MAX_EXTENDED_VALUE = static_cast<double>(std::numeric_limits<uint64>::max());

    if (std::isnan(value) || value < 0.0f)
        extendedMaxPower = 0.0;
    else if (std::isinf(value) || static_cast<double>(value) > MAX_EXTENDED_VALUE)
        extendedMaxPower = MAX_EXTENDED_VALUE;
    else
        extendedMaxPower = static_cast<double>(value);

    // 【重要】在钩子之后应用法力上限限制
    constexpr float MAX_SAFE_VALUE = 2000000000.0f;

    // 检查溢出（负数说明溢出了）
    if (value < 0.0f || value > MAX_SAFE_VALUE)
        value = MAX_SAFE_VALUE;

    // Apply mana limit from database (only for mana power type)
    if (power == POWER_MANA)
    {
        QueryResult result = WorldDatabase.Query("SELECT `法力上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (result)
        {
            Field* fields = result->Fetch();
            uint64 manaLimitU64 = fields[0].Get<uint64>();
            double valueD = static_cast<double>(value);
            double limitD = static_cast<double>(manaLimitU64);

            if (manaLimitU64 > 0 && valueD > limitD)
            {
                value = static_cast<float>(manaLimitU64);
            }

            if (manaLimitU64 > 0 && extendedMaxPower > limitD)
                extendedMaxPower = limitD;
        }
    }

    SetMaxPower(power, static_cast<uint32>(value));
    SetExtendedMaxPower(power, static_cast<uint64>(extendedMaxPower));
    if (wasFullPower)
        SetExtendedPower(power, GetExtendedMaxPower(power));
    else if (oldClientMaxPower && oldExtendedMaxPower <= oldClientMaxPower && oldExtendedPower <= oldClientMaxPower && (HasExtendedPowerForCombat(power) || GetExtendedMaxPower(power) > GetMaxPower(power)))
        SetExtendedPowerFromClientPower(power, static_cast<uint32>(oldExtendedPower));
    else
        SetExtendedPower(power, oldExtendedPower);

    SyncClientPowerFromExtended(power, power == POWER_MANA);
}

void Player::ApplyFeralAPBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseFeralAP, amount);
    UpdateAttackPowerAndDamage();
}

void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(GetLevel());

    sScriptMgr->OnPlayerBeforeUpdateAttackPowerAndDamage(this, level, val2, ranged);

    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    auto getServerStatValue = [this](Stats stat) -> double
    {
        uint64 extendedValue = GetExtendedStat(stat);
        if (extendedValue > 0)
            return static_cast<double>(extendedValue);

        int32 displayValue = GetStat(stat);
        return displayValue > 0 ? static_cast<double>(displayValue) : 0.0;
    };

    double val2D = static_cast<double>(val2);

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;

        // Get agility to attack power conversion rate from database
        float agilityToAPRate = 1.0f; // Default 100% conversion rate
        QueryResult result = WorldDatabase.Query("SELECT `敏捷转攻强转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `敏捷转攻强转换率` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (result)
        {
            Field* fields = result->Fetch();
            agilityToAPRate = fields[0].Get<float>() / 100.0f; // Convert percentage to decimal
        }

        if (IsClass(CLASS_HUNTER, CLASS_CONTEXT_STATS))
        {
            val2D = static_cast<double>(level) * 2.0 + getServerStatValue(STAT_AGILITY) * static_cast<double>(agilityToAPRate) - 10.0;
        }
        else if (IsClass(CLASS_ROGUE, CLASS_CONTEXT_STATS) || IsClass(CLASS_WARRIOR, CLASS_CONTEXT_STATS))
        {
            val2D = static_cast<double>(level) + getServerStatValue(STAT_AGILITY) * static_cast<double>(agilityToAPRate) - 10.0;
        }
        else if (IsClass(CLASS_DRUID, CLASS_CONTEXT_STATS))
        {
            switch (GetShapeshiftForm())
            {
            case FORM_CAT:
            case FORM_BEAR:
            case FORM_DIREBEAR:
                val2D = 0.0;
                break;
            default:
                val2D = getServerStatValue(STAT_AGILITY) * static_cast<double>(agilityToAPRate) - 10.0;
                break;
            }
        }
        else
        {
            val2D = getServerStatValue(STAT_AGILITY) * static_cast<double>(agilityToAPRate) - 10.0;
        }
    }
    else
    {
        // Get strength to attack power conversion rate from database
        float strengthToAPRate = 1.0f; // Default 100% conversion rate
        QueryResult strengthResult = WorldDatabase.Query("SELECT `力量转攻强转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `力量转攻强转换率` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (strengthResult)
        {
            Field* fields = strengthResult->Fetch();
            strengthToAPRate = fields[0].Get<float>() / 100.0f; // Convert percentage to decimal
        }

        // Get agility to attack power conversion rate from database (for melee)
        float agilityToAPRate = 1.0f; // Default 100% conversion rate
        QueryResult agilityResult = WorldDatabase.Query("SELECT `敏捷转攻强转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `敏捷转攻强转换率` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (agilityResult)
        {
            Field* fields = agilityResult->Fetch();
            agilityToAPRate = fields[0].Get<float>() / 100.0f; // Convert percentage to decimal
        }

        if (IsClass(CLASS_PALADIN, CLASS_CONTEXT_STATS) || IsClass(CLASS_DEATH_KNIGHT, CLASS_CONTEXT_STATS) || IsClass(CLASS_WARRIOR, CLASS_CONTEXT_STATS))
        {
            val2D = static_cast<double>(level) * 3.0 + (getServerStatValue(STAT_STRENGTH) * static_cast<double>(strengthToAPRate)) * 2.0 - 20.0;
        }
        else if (IsClass(CLASS_HUNTER, CLASS_CONTEXT_STATS) || IsClass(CLASS_SHAMAN, CLASS_CONTEXT_STATS) || IsClass(CLASS_ROGUE, CLASS_CONTEXT_STATS))
        {
            val2D = static_cast<double>(level) * 2.0 + getServerStatValue(STAT_STRENGTH) * static_cast<double>(strengthToAPRate) + getServerStatValue(STAT_AGILITY) * static_cast<double>(agilityToAPRate) - 20.0;
        }
        else if (IsClass(CLASS_DRUID, CLASS_CONTEXT_STATS))
        {
            // Check if Predatory Strikes is skilled
            float mLevelMult = 0.0f;
            float weapon_bonus = 0.0f;
            if (IsInFeralForm())
            {
                Unit::AuraEffectList const& mDummy = GetAuraEffectsByType(SPELL_AURA_DUMMY);
                for (Unit::AuraEffectList::const_iterator itr = mDummy.begin(); itr != mDummy.end(); ++itr)
                {
                    AuraEffect* aurEff = *itr;
                    if (aurEff->GetSpellInfo()->SpellIconID == 1563)
                    {
                        switch (aurEff->GetEffIndex())
                        {
                        case 0: // Predatory Strikes (effect 0)
                            mLevelMult = CalculatePct(1.0f, aurEff->GetAmount());
                            break;
                        case 1: // Predatory Strikes (effect 1)
                            if (Item* mainHand = m_items[EQUIPMENT_SLOT_MAINHAND])
                            {
                                // also gains % attack power from equipped weapon
                                ItemTemplate const* proto = mainHand->GetTemplate();
                                if (!proto)
                                    continue;

                                uint32 ap = proto->getFeralBonus();
                                // Get AP Bonuses from weapon
                                for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                                {
                                    if (i >= proto->StatsCount)
                                        break;

                                    if (proto->ItemStat[i].ItemStatType == ITEM_MOD_ATTACK_POWER)
                                        ap += proto->ItemStat[i].ItemStatValue;
                                }

                                // Get AP Bonuses from weapon spells
                                for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                                {
                                    // no spell
                                    if (!proto->Spells[i].SpellId || proto->Spells[i].SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                                        continue;

                                    // check if it is valid spell
                                    SpellInfo const* spellproto = sSpellMgr->GetSpellInfo(proto->Spells[i].SpellId);
                                    if (!spellproto)
                                        continue;

                                    for (uint8 j = 0; j < MAX_SPELL_EFFECTS; ++j)
                                        if (spellproto->Effects[j].ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER)
                                            ap += spellproto->Effects[j].CalcValue();
                                }

                                weapon_bonus = CalculatePct(float(ap), aurEff->GetAmount());
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

            switch (GetShapeshiftForm())
            {
            case FORM_CAT:
                val2D = static_cast<double>(GetLevel()) * static_cast<double>(mLevelMult) + getServerStatValue(STAT_STRENGTH) * 2.0 + getServerStatValue(STAT_AGILITY) - 20.0 + static_cast<double>(weapon_bonus) + static_cast<double>(m_baseFeralAP);
                break;
            case FORM_BEAR:
            case FORM_DIREBEAR:
                val2D = static_cast<double>(GetLevel()) * static_cast<double>(mLevelMult) + getServerStatValue(STAT_STRENGTH) * 2.0 - 20.0 + static_cast<double>(weapon_bonus) + static_cast<double>(m_baseFeralAP);
                break;
            case FORM_MOONKIN:
                val2D = static_cast<double>(GetLevel()) * static_cast<double>(mLevelMult) + getServerStatValue(STAT_STRENGTH) * 2.0 - 20.0 + static_cast<double>(m_baseFeralAP);
                break;
            default:
                val2D = getServerStatValue(STAT_STRENGTH) * 2.0 - 20.0;
                break;
            }
        }
        else if (IsClass(CLASS_MAGE, CLASS_CONTEXT_STATS) || IsClass(CLASS_PRIEST, CLASS_CONTEXT_STATS) || IsClass(CLASS_WARLOCK, CLASS_CONTEXT_STATS))
        {
            val2D = getServerStatValue(STAT_STRENGTH) - 10.0;
        }
    }

    constexpr double MAX_SERVER_AP_D = static_cast<double>(std::numeric_limits<float>::max());

    // 服务端内部使用真实攻强，只做数值安全保护
    if (val2D < 0.0 || std::isnan(val2D))
        val2D = 0.0;
    else if (std::isinf(val2D) || val2D > MAX_SERVER_AP_D)
        val2D = MAX_SERVER_AP_D;

    SetModifierValue(unitMod, BASE_VALUE, static_cast<float>(val2D));

    // 使用 double 进行乘法计算，避免 float 精度溢出
    double dBasePct = static_cast<double>(GetModifierValue(unitMod, BASE_PCT));
    double dBaseValue = static_cast<double>(GetModifierValue(unitMod, BASE_VALUE));
    double dBaseAttPower = dBaseValue * dBasePct;

    if (dBaseAttPower < 0.0 || std::isnan(dBaseAttPower))
        dBaseAttPower = 0.0;
    else if (std::isinf(dBaseAttPower) || dBaseAttPower > MAX_SERVER_AP_D)
        dBaseAttPower = MAX_SERVER_AP_D;

    float base_attPower = static_cast<float>(dBaseAttPower);

    double dAttPowerModValue = static_cast<double>(GetModifierValue(unitMod, TOTAL_VALUE));

    //add dynamic flat mods
    if (ranged)
    {
        if ((getClassMask() & CLASSMASK_WAND_USERS) == 0)
        {
            AuraEffectList const& mRAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT);
            for (AuraEffectList::const_iterator i = mRAPbyStat.begin(); i != mRAPbyStat.end(); ++i)
                dAttPowerModValue += CalculatePct(getServerStatValue(Stats((*i)->GetMiscValue())), (*i)->GetAmount());
        }
    }
    else
    {
        AuraEffectList const& mAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT);
        for (AuraEffectList::const_iterator i = mAPbyStat.begin(); i != mAPbyStat.end(); ++i)
            dAttPowerModValue += CalculatePct(getServerStatValue(Stats((*i)->GetMiscValue())), (*i)->GetAmount());

        AuraEffectList const& mAPbyArmor = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR);
        for (AuraEffectList::const_iterator iter = mAPbyArmor.begin(); iter != mAPbyArmor.end(); ++iter)
            // always: ((*i)->GetModifier()->m_miscvalue == 1 == SPELL_SCHOOL_MASK_NORMAL)
            dAttPowerModValue += static_cast<double>(GetExtendedArmor()) / static_cast<double>((*iter)->GetAmount());
    }

    if (dAttPowerModValue < -MAX_SERVER_AP_D || std::isnan(dAttPowerModValue))
        dAttPowerModValue = -MAX_SERVER_AP_D;
    else if (std::isinf(dAttPowerModValue) || dAttPowerModValue > MAX_SERVER_AP_D)
        dAttPowerModValue = MAX_SERVER_AP_D;

    float attPowerMod = static_cast<float>(dAttPowerModValue);

    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    sScriptMgr->OnPlayerAfterUpdateAttackPowerAndDamage(this, level, base_attPower, attPowerMod, attPowerMultiplier, ranged);

    // 钩子后立即检查溢出 - 使用 double 避免精度问题
    double dBaseAP = static_cast<double>(base_attPower);
    double dAttPowerMod = static_cast<double>(attPowerMod);

    if (dBaseAP < 0.0 || std::isnan(base_attPower))
        dBaseAP = 0.0;
    else if (std::isinf(base_attPower) || dBaseAP > MAX_SERVER_AP_D)
        dBaseAP = MAX_SERVER_AP_D;
    if (dAttPowerMod < -MAX_SERVER_AP_D || std::isnan(attPowerMod))
        dAttPowerMod = -MAX_SERVER_AP_D;
    else if (std::isinf(attPowerMod) || dAttPowerMod > MAX_SERVER_AP_D)
        dAttPowerMod = MAX_SERVER_AP_D;

    // Calculate final attack power
    double dFinalAttackPower = dBaseAP;

    // Apply ClassAttributes attack power multiplier first
    {
        QueryResult result;
        if (ranged)
        {
            result = WorldDatabase.Query("SELECT `远程攻强倍率`, `远程攻强上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
        }
        else
        {
            result = WorldDatabase.Query("SELECT `攻强倍率`, `攻强上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
        }

        if (result)
        {
            Field* fields = result->Fetch();
            double apMultiplier = static_cast<double>(fields[0].Get<float>());
            double apLimit = static_cast<double>(fields[1].Get<uint32>());

            // Apply multiplier
            if (apMultiplier != 100.0 && apMultiplier > 0.0)
                dFinalAttackPower = dFinalAttackPower * apMultiplier / 100.0;

            // Apply limit
            if (apLimit > 0.0 && dFinalAttackPower > apLimit)
                dFinalAttackPower = apLimit;
        }
    }

    if (dFinalAttackPower < 0.0 || std::isnan(dFinalAttackPower))
        dFinalAttackPower = 0.0;
    else if (std::isinf(dFinalAttackPower) || dFinalAttackPower > MAX_SERVER_AP_D)
        dFinalAttackPower = MAX_SERVER_AP_D;

    if (dAttPowerMod > MAX_SERVER_AP_D)
        dAttPowerMod = MAX_SERVER_AP_D;
    if (dAttPowerMod < -MAX_SERVER_AP_D)
        dAttPowerMod = -MAX_SERVER_AP_D;

    double serverTotalAP = dFinalAttackPower + dAttPowerMod;
    if (serverTotalAP < 0.0 || std::isnan(serverTotalAP))
        serverTotalAP = 0.0;
    else if (std::isinf(serverTotalAP) || serverTotalAP > MAX_SERVER_AP_D)
        serverTotalAP = MAX_SERVER_AP_D;

    _extendedAttackPower[ranged ? RANGED_ATTACK : BASE_ATTACK] = serverTotalAP;
    if (!ranged)
        _extendedAttackPower[OFF_ATTACK] = serverTotalAP;

    // 【重要】客户端显示攻强 = (AP + Mod) * (1 + Multiplier)
    // 必须确保这个最终显示值不超过 INT32_MAX (约21.47亿)
    // 否则客户端会溢出显示为 0 或负数

    // 先计算合并后的总攻强
    double totalAPForClient = serverTotalAP;

    // 客户端显示值 = totalAP * (1 + multiplier)
    double displayMultiplier = 1.0 + static_cast<double>(attPowerMultiplier);
    double clientDisplayValue = totalAPForClient * displayMultiplier;

    // 客户端使用 int32 显示，最大安全值约 21.47 亿，保守使用 20 亿
    constexpr double MAX_CLIENT_DISPLAY = 2000000000.0;

    if (clientDisplayValue > MAX_CLIENT_DISPLAY || clientDisplayValue < 0.0 || std::isnan(clientDisplayValue) || std::isinf(clientDisplayValue))
    {
        // 客户端显示值会溢出，需要调整
        // 反推：totalAP = MAX_CLIENT_DISPLAY / (1 + multiplier)
        if (displayMultiplier > 0.0)
            totalAPForClient = MAX_CLIENT_DISPLAY / displayMultiplier;
        else
            totalAPForClient = MAX_CLIENT_DISPLAY;
    }

    // 将 totalAP 全部放入 base 字段，mod 设为 0
    dFinalAttackPower = totalAPForClient;
    dAttPowerMod = 0.0;

    int32 finalAP_int32 = static_cast<int32>(dFinalAttackPower);
    int32 finalMod_int32 = static_cast<int32>(dAttPowerMod);

    // 安全转换为 int32
    SetInt32Value(index, finalAP_int32);          //UNIT_FIELD_(RANGED)_ATTACK_POWER field
    SetInt32Value(index_mod, finalMod_int32);     //UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field
    SetFloatValue(index_mult, attPowerMultiplier);                        //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field

    //automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && HasOffhandWeaponForAttack()) //allow update offhand damage only if player knows DualWield Spec and has equipped offhand weapon
            UpdateDamagePhysical(OFF_ATTACK);
        if (IsClass(CLASS_SHAMAN, CLASS_CONTEXT_STATS) || IsClass(CLASS_PALADIN, CLASS_CONTEXT_STATS))                      // mental quickness
            UpdateSpellDamageAndHealingBonus();
    }
}

void Player::UpdateShieldBlockValue()
{
    SetUInt32Value(PLAYER_SHIELD_BLOCK, GetShieldBlockValue());
}

void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage, uint8 damageIndex)
{
    if (attType == OFF_ATTACK && !HasOffhandWeaponForAttack())
    {
        _extendedDamageMin[OFF_ATTACK] = 0.0;
        _extendedDamageMax[OFF_ATTACK] = 0.0;
        minDamage = 0.0f;
        maxDamage = 0.0f;
        return;
    }

    // Only proto damage, not affected by any mods
    if (damageIndex != 0)
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;

        if (!IsInFeralForm() && CanUseAttackType(attType))
        {
            minDamage = GetWeaponDamageRange(attType, MINDAMAGE, damageIndex);
            maxDamage = GetWeaponDamageRange(attType, MAXDAMAGE, damageIndex);
        }

        return;
    }

    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    double attackSpeedMod = static_cast<double>(GetAPMultiplier(attType, normalized));

    // 从数据库获取伤害上限，如果没有配置则使用默认值
    double damageLimit = 0.0;
    {
        QueryResult result;
        switch (attType)
        {
            case BASE_ATTACK:
                result = WorldDatabase.Query("SELECT `主手伤害上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `主手伤害上限` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
                break;
            case OFF_ATTACK:
                result = WorldDatabase.Query("SELECT `副手伤害上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `副手伤害上限` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
                break;
            case RANGED_ATTACK:
                result = WorldDatabase.Query("SELECT `远程伤害上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 AND `远程伤害上限` > 0 ORDER BY `class_` DESC LIMIT 1", getClass());
                break;
        }
        if (result)
        {
            Field* fields = result->Fetch();
            damageLimit = static_cast<double>(fields[0].Get<float>());
        }
    }

    // 获取服务端真实攻击强度，避免客户端 int32/float 显示字段裁剪后影响武器技能伤害
    double attackPower = GetExtendedTotalAttackPowerValue(attType);

    // 如果有配置上限，检查攻击强度是否溢出
    if (damageLimit > 0.0)
    {
        if (attackPower < 0.0 || attackPower > damageLimit || std::isnan(attackPower) || std::isinf(attackPower))
            attackPower = damageLimit;
    }

    double baseModValue = static_cast<double>(GetModifierValue(unitMod, BASE_VALUE));
    double apContribution = attackPower / 14.0 * attackSpeedMod;

    // 如果有配置上限，检查 apContribution 溢出
    if (damageLimit > 0.0)
    {
        if (apContribution < 0.0 || apContribution > damageLimit || std::isnan(apContribution) || std::isinf(apContribution))
            apContribution = damageLimit;
    }

    double baseValue = baseModValue + apContribution;

    // 如果有配置上限，检查 baseValue 溢出
    if (damageLimit > 0.0)
    {
        if (baseValue < 0.0 || baseValue > damageLimit || std::isnan(baseValue) || std::isinf(baseValue))
            baseValue = damageLimit;
    }

    double basePct    = static_cast<double>(GetModifierValue(unitMod, BASE_PCT));
    double totalValue = static_cast<double>(GetModifierValue(unitMod, TOTAL_VALUE));
    double totalPct   = addTotalPct ? static_cast<double>(GetModifierValue(unitMod, TOTAL_PCT)) : 1.0;

    double weaponMinDamage = static_cast<double>(GetWeaponDamageRange(attType, MINDAMAGE));
    double weaponMaxDamage = static_cast<double>(GetWeaponDamageRange(attType, MAXDAMAGE));

    if (IsAttackSpeedOverridenShapeShift()) // forms with no override on attack speed use normal weapon damage
    {
        uint8 lvl = GetLevel();
        if (lvl > 60)
            lvl = 60;

        weaponMinDamage = static_cast<double>(lvl) * 0.85 * attackSpeedMod;
        weaponMaxDamage = static_cast<double>(lvl) * 1.25 * attackSpeedMod;
    }
    else if (!CanUseAttackType(attType)) // check if player not in form but still can't use (disarm case)
    {
        // cannot use ranged/off attack, set values to 0
        if (attType != BASE_ATTACK)
        {
            minDamage = 0.0f;
            maxDamage = 0.0f;
            return;
        }
        weaponMinDamage = BASE_MINDAMAGE;
        weaponMaxDamage = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK) // add ammo DPS to ranged damage
    {
        weaponMinDamage += static_cast<double>(GetAmmoDPS()) * attackSpeedMod;
        weaponMaxDamage += static_cast<double>(GetAmmoDPS()) * attackSpeedMod;
    }

    long double dMinDamage = ((static_cast<long double>(weaponMinDamage) + static_cast<long double>(baseValue)) * static_cast<long double>(basePct) + static_cast<long double>(totalValue)) * static_cast<long double>(totalPct);
    long double dMaxDamage = ((static_cast<long double>(weaponMaxDamage) + static_cast<long double>(baseValue)) * static_cast<long double>(basePct) + static_cast<long double>(totalValue)) * static_cast<long double>(totalPct);

    // 如果有配置上限，限制到数据库配置的范围
    if (damageLimit > 0.0)
    {
        if (dMinDamage < 0.0L || dMinDamage > static_cast<long double>(damageLimit) || !std::isfinite(dMinDamage))
            dMinDamage = static_cast<long double>(damageLimit);
        if (dMaxDamage < 0.0L || dMaxDamage > static_cast<long double>(damageLimit) || !std::isfinite(dMaxDamage))
            dMaxDamage = static_cast<long double>(damageLimit);
    }
    if (dMinDamage > dMaxDamage)
        dMinDamage = dMaxDamage;

    _extendedDamageMin[attType] = dMinDamage > 0.0L && std::isfinite(dMinDamage) ? static_cast<double>(dMinDamage) : 0.0;
    _extendedDamageMax[attType] = dMaxDamage > 0.0L && std::isfinite(dMaxDamage) ? static_cast<double>(dMaxDamage) : 0.0;

    auto toCombatFloat = [](long double value) -> float
    {
        if (value <= 0.0L || !std::isfinite(value))
            return 0.0f;

        constexpr long double MAX_FLOAT_VALUE = static_cast<long double>(std::numeric_limits<float>::max());
        if (value > MAX_FLOAT_VALUE)
            return std::numeric_limits<float>::max();

        return static_cast<float>(value);
    };

    // 兼容旧接口返回 float，但服务端内部不再按 20 亿客户端显示值裁剪
    minDamage = toCombatFloat(dMinDamage);
    maxDamage = toCombatFloat(dMaxDamage);
}

void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

void Player::UpdateBlockPercentage()
{
    // No block
    float value = 0.0f;
    if (CanBlock())
    {
        // Base value
        value = 5.0f;
        // Modify value from defense skill
        value += static_cast<float>((static_cast<double>(GetExtendedDefenseSkillValue()) - static_cast<double>(GetMaxSkillValueForLevel())) * 0.04);
        // Increase from SPELL_AURA_MOD_BLOCK_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
        // Increase from rating
        float blockRating = GetRatingBonusValue(CR_BLOCK);

        // Check for custom block rating conversion rate
        {
            QueryResult result = WorldDatabase.Query("SELECT `格挡等级转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
            if (result)
            {
                Field* fields = result->Fetch();
                float customRate = fields[0].Get<float>();
                if (customRate > 0.0f)
                {
                    // Use custom conversion rate: rating / customRate = percentage
                    uint64 extendedRating = GetExtendedCombatRating(CR_BLOCK);
                    double ratingValue = extendedRating > 0 ? static_cast<double>(extendedRating) : static_cast<double>(GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + CR_BLOCK));
                    double converted = ratingValue / static_cast<double>(customRate);
                    blockRating = converted > static_cast<double>(std::numeric_limits<float>::max()) ? std::numeric_limits<float>::max() : static_cast<float>(converted);

                }
            }
        }

        value += blockRating;
        value = value < 0.0f ? 0.0f : value;
    }

    // Apply block limits with priority: Database > Config file
    bool limitApplied = false;

    // First try database limits
    QueryResult result = WorldDatabase.Query("SELECT `格挡几率上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        float blockLimit = fields[0].Get<float>();
        if (blockLimit > 0.0f && value > blockLimit)
        {
            value = blockLimit;
            limitApplied = true;
        }
    }

    // If no database limit was applied, try config file limits
    if (!limitApplied && sConfigMgr->GetOption<bool>("Stats.Limits.Enable", false))
    {
        float blockLimit = sConfigMgr->GetOption<float>("Stats.Limits.Block", 95.0f);
        if (blockLimit > 0.0f && value > blockLimit)
        {
            value = blockLimit;
        }
    }
    SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, value);
}

void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;
    CombatRating cr;

    switch (attType)
    {
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            index = PLAYER_OFFHAND_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            index = PLAYER_RANGED_CRIT_PERCENTAGE;
            cr = CR_CRIT_RANGED;
            break;
        case BASE_ATTACK:
        default:
            modGroup = CRIT_PERCENTAGE;
            index = PLAYER_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
    }

    float value = GetTotalPercentageModValue(modGroup) + GetRatingBonusValue(cr);
    // Modify crit from weapon skill and maximized defense skill of same level victim difference
    value += (int32(GetWeaponSkillValue(attType)) - int32(GetMaxSkillValueForLevel())) * 0.04f;

    // 调用钩子允许模块修改暴击率
    sScriptMgr->OnPlayerAfterUpdateCritPercentage(this, attType, value);

    // Apply crit limits with priority: Database > Config file
    bool limitApplied = false;
    {
        Player* player = ToPlayer();
        if (player)
        {
            // First try database limits
            QueryResult result = WorldDatabase.Query("SELECT `暴击几率上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", player->getClass());
            if (result)
            {
                Field* fields = result->Fetch();
                float critLimit = fields[0].Get<float>();
                if (critLimit > 0.0f && value > critLimit)
                {
                    value = critLimit;
                    limitApplied = true;
                }
            }

            // If no database limit was applied, try config file limits
            if (!limitApplied && sConfigMgr->GetOption<bool>("Stats.Limits.Enable", false))
            {
                float critLimit = sConfigMgr->GetOption<float>("Stats.Limits.Crit", 95.0f);
                if (critLimit > 0.0f && value > critLimit)
                {
                    value = critLimit;
                }
            }
        }
    }



    value = value < 0.0f ? 0.0f : value;
    SetStatFloatValue(index, value);
}

void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

const float m_diminishing_k[MAX_CLASSES] =
{
    0.9560f,  // Warrior
    0.9560f,  // Paladin
    0.9880f,  // Hunter
    0.9880f,  // Rogue
    0.9830f,  // Priest
    0.9560f,  // DK
    0.9880f,  // Shaman
    0.9830f,  // Mage
    0.9830f,  // Warlock
    0.0f,     // ??
    0.9720f   // Druid
};

float Player::GetMissPercentageFromDefence() const
{
    float const miss_cap[MAX_CLASSES] =
    {
        16.00f,     // Warrior //correct
        16.00f,     // Paladin //correct
        16.00f,     // Hunter  //?
        16.00f,     // Rogue   //?
        16.00f,     // Priest  //?
        16.00f,     // DK      //correct
        16.00f,     // Shaman  //?
        16.00f,     // Mage    //?
        16.00f,     // Warlock //?
        0.0f,       // ??
        16.00f      // Druid   //?
    };

    double diminishing = 0.0, nondiminishing = 0.0;
    // Modify value from defense skill (only bonus from defense rating diminishes)
    nondiminishing += (static_cast<double>(GetSkillValue(SKILL_DEFENSE)) - static_cast<double>(GetMaxSkillValueForLevel())) * 0.04;
    diminishing += GetExtendedRatingBonusValue(CR_DEFENSE_SKILL) * 0.04;

    // apply diminishing formula to diminishing miss chance
    uint32 pclass = getClass() - 1;
    double result = nondiminishing + (diminishing * static_cast<double>(miss_cap[pclass]) / (diminishing + static_cast<double>(miss_cap[pclass]) * static_cast<double>(m_diminishing_k[pclass])));
    if (result <= 0.0 || std::isnan(result))
        return 0.0f;
    if (std::isinf(result) || result > static_cast<double>(std::numeric_limits<float>::max()))
        return std::numeric_limits<float>::max();

    return static_cast<float>(result);
}

void Player::UpdateParryPercentage()
{
    const float parry_cap[MAX_CLASSES] =
    {
        47.003525f,     // Warrior
        47.003525f,     // Paladin
        145.560408f,    // Hunter
        145.560408f,    // Rogue
        0.0f,           // Priest
        47.003525f,     // DK
        145.560408f,    // Shaman
        0.0f,           // Mage
        0.0f,           // Warlock
        0.0f,           // ??
        0.0f            // Druid
    };

    // No parry
    float value = 0.0f;
    m_realParry = 0.0f;
    uint32 pclass = getClass() - 1;
    if (CanParry() && parry_cap[pclass] > 0.0f)
    {
        float nondiminishing  = 5.0f;
        // Parry from rating
        float diminishing = GetRatingBonusValue(CR_PARRY);

        // Check for custom parry rating conversion rate
        {
            QueryResult result = WorldDatabase.Query("SELECT `招架等级转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
            if (result)
            {
                Field* fields = result->Fetch();
                float customRate = fields[0].Get<float>();
                if (customRate > 0.0f)
                {
                    // Use custom conversion rate: rating / customRate = percentage
                    uint64 extendedRating = GetExtendedCombatRating(CR_PARRY);
                    double ratingValue = extendedRating > 0 ? static_cast<double>(extendedRating) : static_cast<double>(GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + CR_PARRY));
                    double converted = ratingValue / static_cast<double>(customRate);
                    diminishing = converted > static_cast<double>(std::numeric_limits<float>::max()) ? std::numeric_limits<float>::max() : static_cast<float>(converted);
                }
            }
        }
        // Modify value from defense skill (only bonus from defense rating diminishes)
        nondiminishing += static_cast<float>((static_cast<double>(GetSkillValue(SKILL_DEFENSE)) - static_cast<double>(GetMaxSkillValueForLevel())) * 0.04);
        double defenseRating = GetExtendedRatingBonusValue(CR_DEFENSE_SKILL) * 0.04;
        diminishing += defenseRating > static_cast<double>(std::numeric_limits<float>::max()) ? std::numeric_limits<float>::max() : static_cast<float>(defenseRating);
        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura
        nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        // apply diminishing formula to diminishing parry chance
        m_realParry = nondiminishing + diminishing * parry_cap[pclass] / (diminishing + parry_cap[pclass] * m_diminishing_k[pclass]);
        m_realParry = m_realParry < 0.0f ? 0.0f : m_realParry;

        // Use the diminished value for limit checking, not the raw sum
        value = m_realParry;

        // Apply parry limits with priority: Database > Config file
        bool limitApplied = false;

        // First try database limits
        QueryResult result = WorldDatabase.Query("SELECT `招架几率上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (result)
        {
            Field* fields = result->Fetch();
            float parryLimit = fields[0].Get<float>();
            if (parryLimit > 0.0f && value > parryLimit)
            {
                value = parryLimit;
                limitApplied = true;
            }
        }

        // If no database limit was applied, try config file limits
        if (!limitApplied && sConfigMgr->GetOption<bool>("Stats.Limits.Enable", false))
        {
            float parryLimit = sConfigMgr->GetOption<float>("Stats.Limits.Parry", 95.0f);
            if (parryLimit > 0.0f && value > parryLimit)
            {
                value = parryLimit;
            }
        }
    }

    SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, value);
}

void Player::UpdateDodgePercentage()
{
    const float dodge_cap[MAX_CLASSES] =
    {
        88.129021f,     // Warrior
        88.129021f,     // Paladin
        145.560408f,    // Hunter
        145.560408f,    // Rogue
        150.375940f,    // Priest
        88.129021f,     // DK
        145.560408f,    // Shaman
        150.375940f,    // Mage
        150.375940f,    // Warlock
        0.0f,           // ??
        116.890707f     // Druid
    };

    float diminishing = 0.0f, nondiminishing = 0.0f;
    GetDodgeFromAgility(diminishing, nondiminishing);
    // Modify value from defense skill (only bonus from defense rating diminishes)
    nondiminishing += static_cast<float>((static_cast<double>(GetSkillValue(SKILL_DEFENSE)) - static_cast<double>(GetMaxSkillValueForLevel())) * 0.04);
    double defenseRating = GetExtendedRatingBonusValue(CR_DEFENSE_SKILL) * 0.04;
    diminishing += defenseRating > static_cast<double>(std::numeric_limits<float>::max()) ? std::numeric_limits<float>::max() : static_cast<float>(defenseRating);
    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura
    nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
    // Dodge from rating
    diminishing += GetRatingBonusValue(CR_DODGE);
    // apply diminishing formula to diminishing dodge chance
    uint32 pclass = getClass() - 1;
    m_realDodge = nondiminishing + (diminishing * dodge_cap[pclass] / (diminishing + dodge_cap[pclass] * m_diminishing_k[pclass]));

    m_realDodge = m_realDodge < 0.0f ? 0.0f : m_realDodge;
    // Use the diminished value for limit checking, not the raw sum
    float value = m_realDodge;

    // Apply dodge limits with priority: Database > Config file
    bool limitApplied = false;
    LOG_DEBUG("entities.player", "UpdateDodgePercentage: Checking dodge limit for class {}", getClass());

    // First try database limits
    QueryResult result = WorldDatabase.Query("SELECT `闪避几率上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        float dodgeLimit = fields[0].Get<float>();
        LOG_DEBUG("entities.player", "UpdateDodgePercentage: Found database dodge limit {} for value {}", dodgeLimit, value);
        if (dodgeLimit > 0.0f && value > dodgeLimit)
        {
            value = dodgeLimit;
            limitApplied = true;
            LOG_DEBUG("entities.player", "UpdateDodgePercentage: Applied database dodge limit, new value {}", value);
        }
    }
    else
    {
        LOG_DEBUG("entities.player", "UpdateDodgePercentage: No database dodge limit found for class {}", getClass());
    }

    // If no database limit was applied, try config file limits
    if (!limitApplied && sConfigMgr->GetOption<bool>("Stats.Limits.Enable", false))
    {
        float dodgeLimit = sConfigMgr->GetOption<float>("Stats.Limits.Dodge", 95.0f);
        LOG_DEBUG("entities.player", "UpdateDodgePercentage: Found config dodge limit {} for value {}", dodgeLimit, value);
        if (dodgeLimit > 0.0f && value > dodgeLimit)
        {
            value = dodgeLimit;
            LOG_DEBUG("entities.player", "UpdateDodgePercentage: Applied config dodge limit, new value {}", value);
        }
    }



    SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, value);
}

void Player::UpdateSpellCritChance(uint32 school)
{
    // For normal school set zero crit chance
    if (school == SPELL_SCHOOL_NORMAL)
    {
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1, 0.0f);
        return;
    }
    // For others recalculate it from:
    float crit = 0.0f;
    // Crit from Intellect
    crit += GetSpellCritFromIntellect();
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifierAreaExclusive(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit from SPELL_AURA_MOD_CRIT_PCT
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    crit += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1 << school);
    // Increase crit from spell crit ratings
    crit += GetRatingBonusValue(CR_CRIT_SPELL);

    // 调用钩子允许模块修改法术暴击率
    sScriptMgr->OnPlayerAfterUpdateSpellCritChance(this, school, crit);

    // Apply spell crit limits with priority: Database > Config file
    bool limitApplied = false;
    {
        // First try database limits
        QueryResult result = WorldDatabase.Query("SELECT `暴击几率上限` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
        if (result)
        {
            Field* fields = result->Fetch();
            float critLimit = fields[0].Get<float>();
            if (critLimit > 0.0f && crit > critLimit)
            {
                crit = critLimit;
                limitApplied = true;
            }
        }

        // If no database limit was applied, try config file limits
        if (!limitApplied && sConfigMgr->GetOption<bool>("Stats.Limits.Enable", false))
        {
            float critLimit = sConfigMgr->GetOption<float>("Stats.Limits.Crit", 95.0f);
            if (critLimit > 0.0f && crit > critLimit)
            {
                crit = critLimit;
            }
        }
    }



    // Store crit value
    SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school, crit);
}

void Player::UpdateArmorPenetration(int32 amount)
{
    // Store Rating Value
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + static_cast<uint16>(CR_ARMOR_PENETRATION), amount);
}

void Player::UpdateMeleeHitChances()
{
    double baseHitChance = static_cast<double>(GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE));

    // Check for custom hit rating conversion rate
    double hitRating = static_cast<double>(GetRatingBonusValue(CR_HIT_MELEE));
    uint64 extendedRatingValue = GetExtendedCombatRating(CR_HIT_MELEE);
    if (extendedRatingValue > 0)
        hitRating = static_cast<double>(extendedRatingValue) * static_cast<double>(GetRatingMultiplier(CR_HIT_MELEE));
    QueryResult result = WorldDatabase.Query("SELECT `命中等级转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        float customRate = fields[0].Get<float>();
        if (customRate > 0.0f)
        {
            // Use custom conversion rate: rating / customRate = percentage
            if (extendedRatingValue > 0)
                hitRating = static_cast<double>(extendedRatingValue) / static_cast<double>(customRate);
            else
            {
                float ratingValue = float(GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + CR_HIT_MELEE));
                hitRating = static_cast<double>(ratingValue) / static_cast<double>(customRate);
            }
        }
    }

    double totalHitChance = baseHitChance + hitRating;
    if (totalHitChance < 0.0 || std::isnan(totalHitChance))
        totalHitChance = 0.0;
    else if (std::isinf(totalHitChance) || totalHitChance > static_cast<double>(std::numeric_limits<float>::max()))
        totalHitChance = static_cast<double>(std::numeric_limits<float>::max());

    m_modMeleeHitChance = static_cast<float>(totalHitChance);
}

void Player::UpdateRangedHitChances()
{
    double baseHitChance = static_cast<double>(GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE));

    // Check for custom hit rating conversion rate
    double hitRating = static_cast<double>(GetRatingBonusValue(CR_HIT_RANGED));
    uint64 extendedRatingValue = GetExtendedCombatRating(CR_HIT_RANGED);
    if (extendedRatingValue > 0)
        hitRating = static_cast<double>(extendedRatingValue) * static_cast<double>(GetRatingMultiplier(CR_HIT_RANGED));

    QueryResult result = WorldDatabase.Query("SELECT `命中等级转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        float customRate = fields[0].Get<float>();

        if (customRate > 0.0f)
        {
            // Use custom conversion rate: rating / customRate = percentage
            if (extendedRatingValue > 0)
                hitRating = static_cast<double>(extendedRatingValue) / static_cast<double>(customRate);
            else
            {
                float ratingValue = float(GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + CR_HIT_RANGED));
                hitRating = static_cast<double>(ratingValue) / static_cast<double>(customRate);
            }
        }
    }

    double totalHitChance = baseHitChance + hitRating;
    if (totalHitChance < 0.0 || std::isnan(totalHitChance))
        totalHitChance = 0.0;
    else if (std::isinf(totalHitChance) || totalHitChance > static_cast<double>(std::numeric_limits<float>::max()))
        totalHitChance = static_cast<double>(std::numeric_limits<float>::max());

    m_modRangedHitChance = static_cast<float>(totalHitChance);
}

void Player::UpdateSpellHitChances()
{
    double baseHitChance = static_cast<double>(GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE));

    // Check for custom hit rating conversion rate
    double hitRating = static_cast<double>(GetRatingBonusValue(CR_HIT_SPELL));
    uint64 extendedRatingValue = GetExtendedCombatRating(CR_HIT_SPELL);
    if (extendedRatingValue > 0)
        hitRating = static_cast<double>(extendedRatingValue) * static_cast<double>(GetRatingMultiplier(CR_HIT_SPELL));
    QueryResult result = WorldDatabase.Query("SELECT `命中等级转换率` FROM `_属性调整_职业` WHERE (`class_` = {} OR `class_` = 0) AND `启用` = 1 ORDER BY `class_` DESC LIMIT 1", getClass());
    if (result)
    {
        Field* fields = result->Fetch();
        float customRate = fields[0].Get<float>();
        if (customRate > 0.0f)
        {
            // Use custom conversion rate: rating / customRate = percentage
            if (extendedRatingValue > 0)
                hitRating = static_cast<double>(extendedRatingValue) / static_cast<double>(customRate);
            else
            {
                float ratingValue = float(GetUInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + CR_HIT_SPELL));
                hitRating = static_cast<double>(ratingValue) / static_cast<double>(customRate);
            }
        }
    }

    double totalHitChance = baseHitChance + hitRating;
    if (totalHitChance < 0.0 || std::isnan(totalHitChance))
        totalHitChance = 0.0;
    else if (std::isinf(totalHitChance) || totalHitChance > static_cast<double>(std::numeric_limits<float>::max()))
        totalHitChance = static_cast<double>(std::numeric_limits<float>::max());

    m_modSpellHitChance = static_cast<float>(totalHitChance);
}

void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateSpellCritChance(i);
}

void Player::UpdateExpertise(WeaponAttackType attack)
{
    if (attack == RANGED_ATTACK)
        return;

    float expertise = GetRatingBonusValue(CR_EXPERTISE);

    Item* weapon = GetWeaponForAttack(attack, true);

    AuraEffectList const& expAuras = GetAuraEffectsByType(SPELL_AURA_MOD_EXPERTISE);
    for (AuraEffectList::const_iterator itr = expAuras.begin(); itr != expAuras.end(); ++itr)
    {
        // item neutral spell
        if ((*itr)->GetSpellInfo()->EquippedItemClass == -1)
            expertise += (*itr)->GetAmount();
        // item dependent spell
        else if (weapon && weapon->IsFitToSpellRequirements((*itr)->GetSpellInfo()))
            expertise += (*itr)->GetAmount();
    }

    if (expertise < 0)
        expertise = 0;

    switch (attack)
    {
        case BASE_ATTACK:
            m_Expertise = expertise;
            SetUInt32Value(PLAYER_EXPERTISE, int32(expertise));
            break;
        case OFF_ATTACK:
            m_OffhandExpertise = expertise;
            SetUInt32Value(PLAYER_OFFHAND_EXPERTISE, int32(expertise));
            break;
        default:
            break;
    }
}

void Player::ApplyManaRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseManaRegen, amount);
    UpdateManaRegen();
}

void Player::ApplyHealthRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseHealthRegen, amount);
}

void Player::UpdateManaRegen()
{
    constexpr uint8 CLIENT_MANA_REGEN_PREDICTION_MASK = 1 << POWER_MANA;
    bool disableClientManaPrediction = HasExtendedPowerForCombat(POWER_MANA) || GetExtendedMaxPower(POWER_MANA) > 2000000000ULL;
    if (disableClientManaPrediction)
        SetByteFlag(PLAYER_FIELD_BYTES2, PLAYER_FIELD_BYTES_2_OFFSET_IGNORE_POWER_REGEN_PREDICTION_MASK, CLIENT_MANA_REGEN_PREDICTION_MASK);
    else
        RemoveByteFlag(PLAYER_FIELD_BYTES2, PLAYER_FIELD_BYTES_2_OFFSET_IGNORE_POWER_REGEN_PREDICTION_MASK, CLIENT_MANA_REGEN_PREDICTION_MASK);

    if (HasAuraTypeWithMiscvalue(SPELL_AURA_PREVENT_REGENERATE_POWER, POWER_MANA + 1))
    {
        SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER, 0);
        SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER, 0);
        if (disableClientManaPrediction && IsInWorld())
            SyncClientPowerFromExtended(POWER_MANA, true);
        return;
    }

    float Intellect = GetStat(STAT_INTELLECT);
    // Mana regen from spirit and intellect
    float power_regen = std::sqrt(Intellect) * OCTRegenMPPerSpirit();
    // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen
    power_regen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

    // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura
    float power_regen_mp5 = (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + m_baseManaRegen) / 5.0f;

    // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
    AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
    for (AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
    {
        power_regen_mp5 += GetStat(Stats((*i)->GetMiscValue())) * (*i)->GetAmount() / 500.0f;
    }

    // Set regen rate in cast state apply only on spirit based regen
    int32 modManaRegenInterrupt = GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    if (modManaRegenInterrupt > 100)
        modManaRegenInterrupt = 100;
    float interruptedRegen = power_regen_mp5 + CalculatePct(power_regen, modManaRegenInterrupt);
    float normalRegen = power_regen_mp5 + power_regen;
    if ((HasExtendedPowerForCombat(POWER_MANA) || GetExtendedMaxPower(POWER_MANA) > GetMaxPower(POWER_MANA)) && (interruptedRegen < 0.0f || normalRegen < 0.0f))
    {
        if (interruptedRegen < 0.0f)
            interruptedRegen = 0.0f;
        if (normalRegen < 0.0f)
            normalRegen = 0.0f;
    }

    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER + AsUnderlyingType(POWER_MANA), interruptedRegen);

    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER + AsUnderlyingType(POWER_MANA), normalRegen);

    if (disableClientManaPrediction && IsInWorld())
        SyncClientPowerFromExtended(POWER_MANA, true);
}

void Player::UpdateEnergyRegen()
{
    float regenPerSecond = 10.f;   // +10 energy per second
    regenPerSecond *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_ENERGY);
    regenPerSecond += static_cast<float>(GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_ENERGY)) / static_cast<float>((5 * IN_MILLISECONDS));

    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER + AsUnderlyingType(POWER_ENERGY), regenPerSecond - 10.f);
    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER + AsUnderlyingType(POWER_ENERGY), regenPerSecond - 10.f);
}

void Player::UpdateRuneRegen(RuneType rune)
{
    if (rune >= NUM_RUNE_TYPES)
        return;

    uint32 cooldown = 0;

    for (uint32 i = 0; i < MAX_RUNES; ++i)
        if (GetBaseRune(i) == rune)
        {
            cooldown = GetRuneBaseCooldown(i, true);
            break;
        }

    if (cooldown <= 0)
        return;

    float regen = float(1 * IN_MILLISECONDS) / float(cooldown);
    SetFloatValue(PLAYER_RUNE_REGEN_1 + uint8(rune), regen);
}

void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraStatMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraStatMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();

    return true;
}

void Creature::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        if (value < 0.0f)
            value = 0.0f;
        else if (value > 2000000000.0f)
            value = 2000000000.0f;
        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Creature::UpdateArmor()
{
    float value = GetTotalAuraModValue(UNIT_MOD_ARMOR);
    if (value < 0.0f)
        value = 0.0f;
    else if (value > 2000000000.0f)
        value = 2000000000.0f;
    SetArmor(int32(value));
}

void Creature::UpdateMaxHealth()
{
    uint64 oldExtendedMaxHealth = GetExtendedMaxHealth();
    uint64 oldExtendedHealth = GetHealthForCombat();
    uint32 oldClientMaxHealth = GetMaxHealth();
    bool wasFullHealth = oldExtendedMaxHealth ? oldExtendedHealth >= oldExtendedMaxHealth : GetHealth() >= GetMaxHealth();

    float value = GetTotalAuraModValue(UNIT_MOD_HEALTH);
    double extendedMaxHealth = 0.0;
    constexpr double MAX_EXTENDED_VALUE = static_cast<double>(std::numeric_limits<uint64>::max());

    if (std::isnan(value) || value < 0.0f)
        extendedMaxHealth = 0.0;
    else if (std::isinf(value) || static_cast<double>(value) > MAX_EXTENDED_VALUE)
        extendedMaxHealth = MAX_EXTENDED_VALUE;
    else
        extendedMaxHealth = static_cast<double>(value);

    if (value < 0.0f)
        value = 0.0f;
    else if (value > 2000000000.0f)
        value = 2000000000.0f;

    SetMaxHealth(uint32(value));
    SetExtendedMaxHealth(static_cast<uint64>(extendedMaxHealth));
    if (wasFullHealth)
        SetExtendedHealth(GetExtendedMaxHealth());
    else if (oldClientMaxHealth && oldExtendedMaxHealth <= oldClientMaxHealth && oldExtendedHealth <= oldClientMaxHealth && GetExtendedMaxHealth() > GetMaxHealth())
        SetExtendedHealth(static_cast<uint32>(oldExtendedHealth));
    else
        SetExtendedHealth(oldExtendedHealth);

    SyncClientHealthFromExtended();
}

void Creature::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + power);
    uint64 oldExtendedMaxPower = GetExtendedMaxPower(power);
    uint64 oldExtendedPower = GetPowerForCombat(power);
    uint32 oldClientMaxPower = GetMaxPower(power);
    bool wasFullPower = oldExtendedMaxPower ? oldExtendedPower >= oldExtendedMaxPower : GetPower(power) >= GetMaxPower(power);

    float value  = GetTotalAuraModValue(unitMod);
    double extendedMaxPower = 0.0;
    constexpr double MAX_EXTENDED_VALUE = static_cast<double>(std::numeric_limits<uint64>::max());

    if (std::isnan(value) || value < 0.0f)
        extendedMaxPower = 0.0;
    else if (std::isinf(value) || static_cast<double>(value) > MAX_EXTENDED_VALUE)
        extendedMaxPower = MAX_EXTENDED_VALUE;
    else
        extendedMaxPower = static_cast<double>(value);

    if (value < 0.0f)
        value = 0.0f;
    else if (value > 2000000000.0f)
        value = 2000000000.0f;

    SetMaxPower(power, uint32(value));
    SetExtendedMaxPower(power, static_cast<uint64>(extendedMaxPower));
    if (wasFullPower)
        SetExtendedPower(power, GetExtendedMaxPower(power));
    else if (oldClientMaxPower && oldExtendedMaxPower <= oldClientMaxPower && oldExtendedPower <= oldClientMaxPower && GetExtendedMaxPower(power) > GetMaxPower(power))
        SetExtendedPower(power, static_cast<uint32>(oldExtendedPower));
    else
        SetExtendedPower(power, oldExtendedPower);

    SyncClientPowerFromExtended(power);
}

void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 indexMod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 indexMulti = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        indexMod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        indexMulti = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;
    }

    float baseAttackPower       = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attackPowerMod        = GetModifierValue(unitMod, TOTAL_VALUE);
    float attackPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    // 攻强字段在 Unit 中以 int32 存储，这里做一次安全截断，避免超过 2,147,483,647 后变成负数
    constexpr float MAX_SAFE_AP = 2000000000.0f;
    if (baseAttackPower < 0.0f)
        baseAttackPower = 0.0f;
    else if (baseAttackPower > MAX_SAFE_AP)
        baseAttackPower = MAX_SAFE_AP;
    if (attackPowerMod < 0.0f)
        attackPowerMod = 0.0f;
    else if (attackPowerMod > MAX_SAFE_AP)
        attackPowerMod = MAX_SAFE_AP;

    SetInt32Value(index, uint32(baseAttackPower));      // UNIT_FIELD_(RANGED)_ATTACK_POWER
    SetInt32Value(indexMod, uint32(attackPowerMod));    // UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS
    SetFloatValue(indexMulti, attackPowerMultiplier);   // UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER

    // automatically update weapon damage after attack power modification
    if (ranged)
        UpdateDamagePhysical(RANGED_ATTACK);
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        UpdateDamagePhysical(OFF_ATTACK);
    }
}

void Creature::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage, uint8 damageIndex /*= 0*/)
{
    // creatures only have one damage
    if (damageIndex != 0)
    {
        minDamage = 0.f;
        maxDamage = 0.f;
        return;
    }

    UnitMods unitMod;
    float variance = 1.0f;
    switch (attType)
    {
        case BASE_ATTACK:
        default:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            variance = GetCreatureTemplate()->RangeVariance;
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    if (attType == OFF_ATTACK && !HasOffhandWeaponForAttack())
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;
        return;
    }

    long double weaponMinDamage = static_cast<long double>(GetWeaponDamageRange(attType, MINDAMAGE));
    long double weaponMaxDamage = static_cast<long double>(GetWeaponDamageRange(attType, MAXDAMAGE));

    // Disarm for creatures
    if (HasWeapon(attType) && !HasWeaponForAttack(attType))
    {
        weaponMinDamage *= 0.5L;
        weaponMaxDamage *= 0.5L;
    }

    long double attackPower      = static_cast<long double>(GetTotalAttackPowerValue(attType));
    long double attackSpeedMulti = static_cast<long double>(GetAPMultiplier(attType, normalized));
    long double baseValue        = static_cast<long double>(GetModifierValue(unitMod, BASE_VALUE)) + (attackPower / 14.0L) * static_cast<long double>(variance);
    long double basePct          = static_cast<long double>(GetModifierValue(unitMod, BASE_PCT)) * attackSpeedMulti;
    long double totalValue       = static_cast<long double>(GetModifierValue(unitMod, TOTAL_VALUE));
    long double totalPct         = addTotalPct ? static_cast<long double>(GetModifierValue(unitMod, TOTAL_PCT)) : 1.0L;
    long double dmgMultiplier    = static_cast<long double>(GetCreatureTemplate()->DamageModifier); // = DamageModifier * _GetDamageMod(rank);

    long double minDamageValue = ((weaponMinDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;
    long double maxDamageValue = ((weaponMaxDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;

    auto clampDamageToFloat = [](long double value) -> float
    {
        constexpr long double MAX_SAFE_DAMAGE = static_cast<long double>(std::numeric_limits<int64>::max());

        if (value <= 0.0L || !std::isfinite(value))
            return 0.0f;

        if (value > MAX_SAFE_DAMAGE)
            value = MAX_SAFE_DAMAGE;

        float floatValue = static_cast<float>(value);
        while (static_cast<long double>(floatValue) > MAX_SAFE_DAMAGE && floatValue > 0.0f)
            floatValue = std::nextafter(floatValue, 0.0f);

        return floatValue;
    };

    minDamage = clampDamageToFloat(minDamageValue);
    maxDamage = clampDamageToFloat(maxDamageValue);
    if (minDamage > maxDamage)
        minDamage = maxDamage;
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Guardian::UpdateStats(Stats stat)
{
    if (stat >= MAX_STATS)
        return false;

    float value = GetTotalStatValue(stat);
    SetStat(stat, int32(value));

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateAttackPowerAndDamage();
            break;
        case STAT_AGILITY:
            UpdateArmor();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            break;
        case STAT_SPIRIT:
            break;
    }

    return true;
}

bool Guardian::UpdateAllStats()
{
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();
    return true;
}

void Guardian::UpdateArmor()
{
    float value = GetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE);
    value *= GetModifierValue(UNIT_MOD_ARMOR, BASE_PCT);
    value += std::max<float>(GetStat(STAT_AGILITY) - GetCreateStat(STAT_AGILITY), 0.0f) * 2.0f;
    value += GetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE);
    value *= GetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT);
    SetArmor(int32(value));
}

void Guardian::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    float stamina = std::max<float>(GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA), 0.0f);

    float multiplicator;
    switch (GetEntry())
    {
        case NPC_IMP:
            multiplicator = 8.4f;
            break;
        case NPC_WATER_ELEMENTAL_TEMP:
            multiplicator = 7.5f;
            break;
        case NPC_WATER_ELEMENTAL_PERM:
            multiplicator = 7.5f;
            break;
        case NPC_VOIDWALKER:
            multiplicator = 11.0f;
            break;
        case NPC_SUCCUBUS:
            multiplicator = 9.1f;
            break;
        case NPC_FELHUNTER:
            multiplicator = 9.5f;
            break;
        case NPC_FELGUARD:
            multiplicator = 11.0f;
            break;
        case NPC_BLOODWORM:
            multiplicator = 1.0f;
            break;
        default:
            multiplicator = 10.0f;
            break;
    }

    float value = GetModifierValue(unitMod, BASE_VALUE);// xinef: Do NOT add base health TWICE + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + stamina * multiplicator;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth((uint32)value);
}

void Guardian::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + power);

    float addValue = (power == POWER_MANA) ? std::max<float>(GetStat(STAT_INTELLECT) - GetCreateStat(STAT_INTELLECT), 0.0f) : 0.0f;
    float multiplicator = 15.0f;

    switch (GetEntry())
    {
        case NPC_IMP:
        case NPC_WATER_ELEMENTAL_TEMP:
        case NPC_WATER_ELEMENTAL_PERM:
            multiplicator = 4.95f;
            break;
        case NPC_VOIDWALKER:
        case NPC_SUCCUBUS:
        case NPC_FELHUNTER:
        case NPC_FELGUARD:
            multiplicator = 11.5f;
            break;
        default:
            multiplicator = 15.0f;
            break;
    }

    // xinef: Do NOT add base mana TWICE
    float value = GetModifierValue(unitMod, BASE_VALUE) + (power != POWER_MANA ? GetCreatePowers(power) : 0);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + addValue * multiplicator;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, uint32(value));
}

void Guardian::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
        return;

    float val = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    if (GetEntry() == NPC_IMP)                                     // imp's attack power
        val = GetStat(STAT_STRENGTH) - 10.0f;
    else if (IsPetGhoul())                                         // DK's ghoul attack power
        val = 589 /*xinef: base ap!*/ + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY);
    else
        val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    SetModifierValue(unitMod, BASE_VALUE, val);

    //in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of meleeattackpower field in DB
    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);
    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    //UNIT_FIELD_(RANGED)_ATTACK_POWER field
    SetInt32Value(UNIT_FIELD_ATTACK_POWER, (int32)base_attPower);
    //UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field
    SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS, (int32)attPowerMod);
    //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, attPowerMultiplier);

    //automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
}

void Guardian::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
        return;

    UnitMods unitMod = UNIT_MOD_DAMAGE_MAINHAND;

    float att_speed = float(GetAttackTime(BASE_ATTACK)) / 1000.0f;

    float base_value  = GetModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType) / 14.0f * att_speed;
    float base_pct    = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

    float mindamage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    if (mindamage < 0.0f || mindamage > 10000000.0f)
        mindamage = BASE_MINDAMAGE;
    if (maxdamage < 0.0f || maxdamage > 10000000.0f)
        maxdamage = BASE_MAXDAMAGE;
    if (mindamage > maxdamage)
        mindamage = maxdamage;

    //  Pet's base damage changes depending on happiness
    if (IsHunterPet() && attType == BASE_ATTACK)
    {
        switch (ToPet()->GetHappinessState())
        {
            case HAPPY:
                // 125% of normal damage
                mindamage = mindamage * 1.25f;
                maxdamage = maxdamage * 1.25f;
                break;
            case CONTENT:
                // 100% of normal damage, nothing to modify
                break;
            case UNHAPPY:
                // 75% of normal damage
                mindamage = mindamage * 0.75f;
                maxdamage = maxdamage * 0.75f;
                break;
        }
    }

    SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
    SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
}

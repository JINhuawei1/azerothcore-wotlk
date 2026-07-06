/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 License
 */

#include "Duration.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"
#include "Unit.h"
#include "Util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#if __has_include("AscensionSystem.h")
    #ifndef MODULE_ASCENSION_SYSTEM
        #define MODULE_ASCENSION_SYSTEM
    #endif
    #include "AscensionSystem.h"
#endif

#if __has_include("XianmenArtifactSlots.h")
    #ifndef MODULE_XIANMEN_ARTIFACT_SLOTS
        #define MODULE_XIANMEN_ARTIFACT_SLOTS
    #endif
    #include "XianmenArtifactSlots.h"
#endif

namespace
{
enum ArtifactEffectMarker
{
    ARTIFACT_MARKER_THUNDER = 9100101,
    ARTIFACT_MARKER_SWORDS = 9100102,
    ARTIFACT_MARKER_TIME_LOCK = 9100103,
    ARTIFACT_MARKER_SOUL_ECHO = 9100104,
    ARTIFACT_MARKER_STAR_BURST = 9100105
};

constexpr uint32 ARTIFACT_SCALE_DENOMINATOR = 10000;
// 注意：SQL 注释把 EffectBasePoints_1 描述为“万分比”，但这里分母是 100，实际按百分比解读
// （100 = 100%，400 = 400%）。已确认保持现有高伤害行为，分母维持 100，不要随意改成 10000，
// 否则伤害会变成原来的 1/100。治疗用的 ARTIFACT_SCALE_DENOMINATOR 才是真正的万分比（10000）。
constexpr uint32 ARTIFACT_PERCENT_DENOMINATOR = 100;
constexpr uint32 ARTIFACT_ITEM_START = 90001;
constexpr uint32 ARTIFACT_ITEM_END = 90016;
constexpr uint32 ARTIFACT_EXTENDED_ITEM_START = 90201;
constexpr uint32 ARTIFACT_EXTENDED_ITEM_END = 90204;
constexpr uint32 ARTIFACT_EXTENDED_RANK_START = 17;
constexpr uint32 ARTIFACT_SPELL_START = 381001;
constexpr uint32 ARTIFACT_SPELL_COUNT_PER_RANK = 5;

constexpr std::array<uint8, 3> ARTIFACT_WEAPON_SLOTS =
{
    EQUIPMENT_SLOT_MAINHAND,
    EQUIPMENT_SLOT_OFFHAND,
    EQUIPMENT_SLOT_RANGED
};

bool IsArtifactEffectMarker(int32 marker)
{
    switch (marker)
    {
        case ARTIFACT_MARKER_THUNDER:
        case ARTIFACT_MARKER_SWORDS:
        case ARTIFACT_MARKER_TIME_LOCK:
        case ARTIFACT_MARKER_SOUL_ECHO:
        case ARTIFACT_MARKER_STAR_BURST:
            return true;
        default:
            return false;
    }
}

bool IsArtifactWeapon(ItemTemplate const* proto)
{
    return proto && ((proto->ItemId >= ARTIFACT_ITEM_START && proto->ItemId <= ARTIFACT_ITEM_END) ||
        (proto->ItemId >= ARTIFACT_EXTENDED_ITEM_START && proto->ItemId <= ARTIFACT_EXTENDED_ITEM_END));
}

bool IsArtifactEffectSpell(SpellInfo const* spellInfo)
{
    return spellInfo && IsArtifactEffectMarker(spellInfo->Effects[EFFECT_2].BasePoints);
}

bool IsValidArtifactEffectTarget(Player* player, Unit* target)
{
    if (!player || !target || !player->IsAlive() || !target->IsAlive() || target == player || !player->IsInMap(target))
        return false;

    if (target->GetCharmerOrOwnerPlayerOrPlayerItself() == player)
        return false;

    if (player->IsFriendlyTo(target))
        return false;

    return player->IsValidAttackTarget(target);
}

uint32 GetArtifactWeaponRank(ItemTemplate const* proto);

void SelectBetterArtifactCandidate(Item*& bestItem, uint32& bestRank, Item* item)
{
    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    if (!IsArtifactWeapon(proto))
        return;

    uint32 rank = GetArtifactWeaponRank(proto);
    if (!bestItem || rank > bestRank)
    {
        bestItem = item;
        bestRank = rank;
    }
}

Item* FindEquippedArtifactWeapon(Player* player)
{
    if (!player)
        return nullptr;

    Item* bestItem = nullptr;
    uint32 bestRank = 0;

    for (uint8 slot : ARTIFACT_WEAPON_SLOTS)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        SelectBetterArtifactCandidate(bestItem, bestRank, item);
    }

#ifdef MODULE_ASCENSION_SYSTEM
    if (sAscensionConfig->IsEnabled())
    {
        if (PlayerAscensionStatus* status = sAscensionManager->GetPlayerStatus(player->GetGUID().GetCounter()))
        {
            for (uint8 slot : ARTIFACT_WEAPON_SLOTS)
            {
                AscensionSlotControl const* control = sAscensionManager->GetSlotControl(slot);
                if (control && !control->enabled)
                    continue;

                auto slotIt = status->slots.find(slot);
                if (slotIt == status->slots.end())
                    continue;

                SelectBetterArtifactCandidate(bestItem, bestRank, slotIt->second.itemPtr);
            }
        }
    }
#endif

#ifdef MODULE_XIANMEN_ARTIFACT_SLOTS
    XianmenArtifactSlots::ForEachEquippedWeaponItem(player, [&](Item* item)
    {
        SelectBetterArtifactCandidate(bestItem, bestRank, item);
    });
#endif

    return bestItem;
}

uint32 GetArtifactWeaponRank(ItemTemplate const* proto)
{
    if (!IsArtifactWeapon(proto))
        return 0;

    if (proto->ItemId >= ARTIFACT_ITEM_START && proto->ItemId <= ARTIFACT_ITEM_END)
        return proto->ItemId - ARTIFACT_ITEM_START + 1;

    return ARTIFACT_EXTENDED_RANK_START + proto->ItemId - ARTIFACT_EXTENDED_ITEM_START;
}

uint256 ToUInt256Positive(int256 const& value)
{
    return value > 0 ? Acore::Number::ToUInt256Saturated(value) : 0;
}

uint256 ToUInt256Positive(double value)
{
    if (value <= 0.0 || std::isnan(value))
        return 0;

    if (std::isinf(value))
        return std::numeric_limits<uint256>::max();

    return Acore::Number::ToUInt256Saturated(static_cast<long double>(value));
}

uint256 GetPlayerStatPower(Player* player, Stats stat)
{
    if (!player)
        return 0;

    int256 const extended = player->GetExtendedStat256(stat);
    if (extended > 0)
        return ToUInt256Positive(extended);

    return player->GetStatUInt32(stat);
}

uint256 GetPlayerCombatRatingPower(Player* player, CombatRating rating)
{
    if (!player)
        return 0;

    int256 const extended = player->GetExtendedCombatRating(rating);
    if (extended > 0)
        return ToUInt256Positive(extended);

    int32 const display = player->GetInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + static_cast<uint16>(rating));
    return display > 0 ? static_cast<uint256>(display) : 0;
}

uint256 GetPlayerArtifactPower(Player* player)
{
    if (!player)
        return 0;

    uint256 power = 0;
    power = AddUInt256Damage(power, GetPlayerStatPower(player, STAT_STRENGTH));
    power = AddUInt256Damage(power, GetPlayerStatPower(player, STAT_AGILITY));
    power = AddUInt256Damage(power, GetPlayerStatPower(player, STAT_STAMINA));
    power = AddUInt256Damage(power, GetPlayerStatPower(player, STAT_INTELLECT));
    power = AddUInt256Damage(power, GetPlayerStatPower(player, STAT_SPIRIT));
    power = AddUInt256Damage(power, player->GetTrueDamageBonus());
    power = AddUInt256Damage(power, player->GetCuttingDamageBonus());
    power = AddUInt256Damage(power, std::max({ GetPlayerCombatRatingPower(player, CR_HIT_MELEE), GetPlayerCombatRatingPower(player, CR_HIT_RANGED), GetPlayerCombatRatingPower(player, CR_HIT_SPELL) }));
    power = AddUInt256Damage(power, ToUInt256Positive(player->GetExtendedTotalAttackPowerValue(BASE_ATTACK)));
    power = AddUInt256Damage(power, ToUInt256Positive(player->GetExtendedSpellPowerBonus256()));

    return power;
}

uint256 ScalePercentValue(uint256 const& value, int32 percent)
{
    if (value == 0 || percent <= 0)
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / static_cast<long double>(ARTIFACT_PERCENT_DENOMINATOR);
    return Acore::Number::ToUInt256Saturated(scaled);
}

uint256 ScaleBasisPointValue(uint256 const& value, int32 scale)
{
    if (value == 0 || scale <= 0)
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(scale) / static_cast<long double>(ARTIFACT_SCALE_DENOMINATOR);
    return Acore::Number::ToUInt256Saturated(scaled);
}

uint256 ApplyArtifactHealthGain(Unit* unit, uint256 const& value)
{
    if (!unit || value == 0)
        return 0;

    uint256 currentHealth = unit->GetHealthForCombat256();
    uint256 maxHealth = unit->GetMaxHealthForCombat256();
    if (currentHealth >= maxHealth)
        return 0;

    uint256 gain = std::min<uint256>(value, maxHealth - currentHealth);
    if (gain)
        unit->SetHealthForCombat256(currentHealth + gain);

    return gain;
}

void ApplyTimeLock(Player* caster, Unit* target, uint32 durationSeconds)
{
    if (durationSeconds == 0 || !IsValidArtifactEffectTarget(caster, target))
        return;

    if (target->HasUnitState(UNIT_STATE_ROOT))
        return;

    ObjectGuid casterGuid = caster->GetGUID();

    target->SetControlled(true, UNIT_STATE_ROOT, caster);

    target->m_Events.AddEventAtOffset([casterGuid, target]()
    {
        Player* caster = ObjectAccessor::FindPlayer(casterGuid);
        target->SetControlled(false, UNIT_STATE_ROOT, caster);
    }, Seconds(durationSeconds));
}

uint32 PickArtifactVisualKit(uint32 first, uint32 second = 0, uint32 third = 0, uint32 fourth = 0, uint32 fifth = 0)
{
    uint32 const kits[] = { first, second, third, fourth, fifth };
    for (uint32 kitId : kits)
        if (kitId && kitId != std::numeric_limits<uint32>::max())
            return kitId;

    return 0;
}

void SendArtifactEffectVisual(Player* player, Unit* victim, SpellInfo const* spellInfo)
{
    if (!player || !victim || !spellInfo || !player->IsInMap(victim))
        return;

    SpellVisualEntry const* visual = nullptr;
    for (uint32 visualId : spellInfo->SpellVisual)
    {
        if (!visualId)
            continue;

        visual = sSpellVisualStore.LookupEntry(visualId);
        if (visual)
            break;
    }

    if (!visual)
        return;

    uint32 const casterKit = PickArtifactVisualKit(visual->CastKit, visual->PrecastKit, visual->StateKit, visual->ChannelKit, visual->CasterImpactKit);
    uint32 const targetKit = PickArtifactVisualKit(visual->TargetImpactKit, visual->ImpactKit, visual->ImpactAreaKit, visual->InstantAreaKit);

    if (casterKit)
        player->SendPlaySpellVisual(casterKit);

    if (targetKit)
        player->SendPlaySpellImpact(victim->GetGUID(), targetKit);
}

uint256 DealArtifactSpellDamage(Player* player, Unit* victim, SpellInfo const* spellInfo, uint256 const& damage)
{
    if (!spellInfo || damage == 0 || !IsValidArtifactEffectTarget(player, victim))
        return 0;

    SpellSchoolMask schoolMask = spellInfo->GetSchoolMask();
    if (!schoolMask)
        schoolMask = SPELL_SCHOOL_MASK_NORMAL;

    SpellNonMeleeDamage damageInfo(player, victim, spellInfo, schoolMask);
    damageInfo.damage = damage;

    Unit::DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    if (player->ShouldSendCustomProcClientFeedback(victim, spellInfo, "artifact-chain"))
    {
        SendArtifactEffectVisual(player, victim, spellInfo);
        player->SendSpellNonMeleeDamageLog(&damageInfo);
    }

    CleanDamage cleanDamage(damageInfo.cleanDamage, damageInfo.absorb, BASE_ATTACK, (damageInfo.HitInfo & SPELL_HIT_TYPE_CRIT) ? MELEE_HIT_CRIT : MELEE_HIT_NORMAL);
    return Unit::DealDamage(player, victim, damageInfo.damage, &cleanDamage, SPELL_DIRECT_DAMAGE, schoolMask, spellInfo, true);
}

bool ApplyArtifactEffect(Player* player, Unit* victim, SpellInfo const* spellInfo, Item* item)
{
    if (!spellInfo || !IsValidArtifactEffectTarget(player, victim))
        return false;

    if (!IsArtifactEffectSpell(spellInfo))
        return false;

    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    if (!IsArtifactWeapon(proto))
        return false;

    int32 const marker = spellInfo->Effects[EFFECT_2].BasePoints;
    int32 const damagePct = spellInfo->Effects[EFFECT_0].BasePoints;
    int32 const miscValue = spellInfo->Effects[EFFECT_1].BasePoints;
    uint256 const playerPower = GetPlayerArtifactPower(player);
    uint256 const damage = ScalePercentValue(playerPower, damagePct);

    if (damage == 0)
        return true;

    uint256 const dealt = DealArtifactSpellDamage(player, victim, spellInfo, damage);

    if (marker == ARTIFACT_MARKER_TIME_LOCK && miscValue > 0)
    {
        ApplyTimeLock(player, victim, static_cast<uint32>(miscValue));
    }
    else if (marker == ARTIFACT_MARKER_SOUL_ECHO && miscValue > 0 && dealt > 0)
    {
        uint256 const heal = ScaleBasisPointValue(dealt, miscValue);
        if (heal > 0)
            ApplyArtifactHealthGain(player, heal);
    }

    return true;
}

bool TriggerEquippedArtifactEffects(Player* player, Unit* victim)
{
    if (!IsValidArtifactEffectTarget(player, victim))
        return false;

    Item* item = FindEquippedArtifactWeapon(player);
    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    uint32 const rank = GetArtifactWeaponRank(proto);
    if (!rank)
        return false;

    bool triggered = false;
    uint32 const firstSpellId = ARTIFACT_SPELL_START + (rank - 1) * ARTIFACT_SPELL_COUNT_PER_RANK;
    for (uint32 offset = 0; offset < ARTIFACT_SPELL_COUNT_PER_RANK; ++offset)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(firstSpellId + offset);
        if (!IsArtifactEffectSpell(spellInfo))
            continue;

        if (ApplyArtifactEffect(player, victim, spellInfo, item))
            triggered = true;
    }

    return triggered;
}

class item_artifact_chain_weapon : public ItemScript
{
public:
    item_artifact_chain_weapon() : ItemScript("item_artifact_chain_weapon") { }

    // 有意失活物品 proc 路径：神器技能返回 false，阻止核心走随机几率的 CHANCE_ON_HIT proc。
    // 神器特效由 artifact_chain_unit_script 的伤害计算钩子触发，避免被幻境血条 OnDamage 吸收逻辑吞掉。
    bool OnCastItemCombatSpell(Player* player, Unit* victim, SpellInfo const* spellInfo, Item* item) override
    {
        ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
        if (IsArtifactWeapon(proto) && IsArtifactEffectSpell(spellInfo))
            return false;

        return true;
    }
};

class ArtifactEffectGuard
{
public:
    explicit ArtifactEffectGuard(bool& active) : _active(active), _entered(!active)
    {
        if (_entered)
            _active = true;
    }

    ~ArtifactEffectGuard()
    {
        if (_entered)
            _active = false;
    }

    [[nodiscard]] bool Entered() const { return _entered; }

private:
    bool& _active;
    bool _entered;
};

bool TriggerEquippedArtifactEffectsGuarded(Player* player, Unit* victim)
{
    if (!IsValidArtifactEffectTarget(player, victim))
        return false;

    static thread_local bool applyingArtifactEffect = false;
    ArtifactEffectGuard guard(applyingArtifactEffect);
    if (!guard.Entered())
        return false;

    return TriggerEquippedArtifactEffects(player, victim);
}
}

class artifact_chain_unit_script : public UnitScript
{
public:
    artifact_chain_unit_script() : UnitScript("artifact_chain_unit_script", true,
        {
            UNITHOOK_MODIFY_MELEE_DAMAGE,
            UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
            UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK
        }) { }

    void ModifyMeleeDamage(Unit* victim, Unit* attacker, uint256& damage) override
    {
        if (damage == 0 || !attacker || !victim)
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        TriggerEquippedArtifactEffectsGuarded(player, victim);
    }

    void ModifySpellDamageTaken(Unit* victim, Unit* attacker, uint256& damage, SpellInfo const* spellInfo) override
    {
        if (damage == 0 || !attacker || !victim || IsArtifactEffectSpell(spellInfo))
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        TriggerEquippedArtifactEffectsGuarded(player, victim);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint256& damage, SpellInfo const* spellInfo) override
    {
        if (damage == 0 || !attacker || !target || IsArtifactEffectSpell(spellInfo))
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        TriggerEquippedArtifactEffectsGuarded(player, target);
    }
};

void AddSC_artifact_quest_chain()
{
    new item_artifact_chain_weapon();
    new artifact_chain_unit_script();
}

/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Released under GNU AGPL v3 License
 */

#include "Duration.h"
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

uint32 GetArtifactWeaponRank(ItemTemplate const* proto);

Item* FindEquippedArtifactWeapon(Player* player)
{
    if (!player)
        return nullptr;

    Item* bestItem = nullptr;
    uint32 bestRank = 0;

    for (uint8 slot : ARTIFACT_WEAPON_SLOTS)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
        if (!IsArtifactWeapon(proto))
            continue;

        uint32 rank = GetArtifactWeaponRank(proto);
        if (!bestItem || rank > bestRank)
        {
            bestItem = item;
            bestRank = rank;
        }
    }

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

uint128 ToUInt128Positive(int128 const& value)
{
    return value > 0 ? Acore::Number::ToUInt128Saturated(value) : 0;
}

uint128 ToUInt128Positive(double value)
{
    if (value <= 0.0 || std::isnan(value))
        return 0;

    if (std::isinf(value))
        return std::numeric_limits<uint128>::max();

    return Acore::Number::ToUInt128Saturated(static_cast<long double>(value));
}

uint128 GetPlayerStatPower(Player* player, Stats stat)
{
    if (!player)
        return 0;

    int128 const extended = player->GetExtendedStat128(stat);
    if (extended > 0)
        return ToUInt128Positive(extended);

    return player->GetStatUInt32(stat);
}

uint128 GetPlayerCombatRatingPower(Player* player, CombatRating rating)
{
    if (!player)
        return 0;

    int128 const extended = player->GetExtendedCombatRating(rating);
    if (extended > 0)
        return ToUInt128Positive(extended);

    int32 const display = player->GetInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + static_cast<uint16>(rating));
    return display > 0 ? static_cast<uint128>(display) : 0;
}

uint128 GetPlayerArtifactPower(Player* player)
{
    if (!player)
        return 0;

    uint128 power = 0;
    power = AddUInt128Damage(power, GetPlayerStatPower(player, STAT_STRENGTH));
    power = AddUInt128Damage(power, GetPlayerStatPower(player, STAT_AGILITY));
    power = AddUInt128Damage(power, GetPlayerStatPower(player, STAT_STAMINA));
    power = AddUInt128Damage(power, GetPlayerStatPower(player, STAT_INTELLECT));
    power = AddUInt128Damage(power, GetPlayerStatPower(player, STAT_SPIRIT));
    power = AddUInt128Damage(power, player->GetTrueDamageBonus());
    power = AddUInt128Damage(power, player->GetCuttingDamageBonus());
    power = AddUInt128Damage(power, std::max({ GetPlayerCombatRatingPower(player, CR_HIT_MELEE), GetPlayerCombatRatingPower(player, CR_HIT_RANGED), GetPlayerCombatRatingPower(player, CR_HIT_SPELL) }));
    power = AddUInt128Damage(power, ToUInt128Positive(player->GetExtendedTotalAttackPowerValue(BASE_ATTACK)));
    power = AddUInt128Damage(power, ToUInt128Positive(player->GetExtendedSpellPowerBonus128()));

    return power;
}

uint128 ScalePercentValue(uint128 const& value, int32 percent)
{
    if (value == 0 || percent <= 0)
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / static_cast<long double>(ARTIFACT_PERCENT_DENOMINATOR);
    return Acore::Number::ToUInt128Saturated(scaled);
}

uint128 ScaleBasisPointValue(uint128 const& value, int32 scale)
{
    if (value == 0 || scale <= 0)
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(scale) / static_cast<long double>(ARTIFACT_SCALE_DENOMINATOR);
    return Acore::Number::ToUInt128Saturated(scaled);
}

int64 ToPositiveHealthChange(uint128 const& value)
{
    return Acore::Number::ToInt64Saturated(Acore::Number::ToInt128Saturated(value));
}

void ApplyTimeLock(Player* caster, Unit* target, uint32 durationSeconds)
{
    if (!caster || !target || durationSeconds == 0 || target->isDead())
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

uint128 DealArtifactSpellDamage(Player* player, Unit* victim, SpellInfo const* spellInfo, uint128 const& damage)
{
    SpellSchoolMask schoolMask = spellInfo->GetSchoolMask();
    if (!schoolMask)
        schoolMask = SPELL_SCHOOL_MASK_NORMAL;

    SpellNonMeleeDamage damageInfo(player, victim, spellInfo, schoolMask);
    damageInfo.damage = damage;

    Unit::DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    player->SendSpellNonMeleeDamageLog(&damageInfo);

    CleanDamage cleanDamage(damageInfo.cleanDamage, damageInfo.absorb, BASE_ATTACK, (damageInfo.HitInfo & SPELL_HIT_TYPE_CRIT) ? MELEE_HIT_CRIT : MELEE_HIT_NORMAL);
    return Unit::DealDamage(player, victim, damageInfo.damage, &cleanDamage, SPELL_DIRECT_DAMAGE, schoolMask, spellInfo, true);
}

bool ApplyArtifactEffect(Player* player, Unit* victim, SpellInfo const* spellInfo, Item* item)
{
    if (!player || !victim || !spellInfo || victim->isDead())
        return false;

    if (!IsArtifactEffectSpell(spellInfo))
        return false;

    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    if (!IsArtifactWeapon(proto))
        return false;

    int32 const marker = spellInfo->Effects[EFFECT_2].BasePoints;
    int32 const damagePct = spellInfo->Effects[EFFECT_0].BasePoints;
    int32 const miscValue = spellInfo->Effects[EFFECT_1].BasePoints;
    uint128 const playerPower = GetPlayerArtifactPower(player);
    uint128 const damage = ScalePercentValue(playerPower, damagePct);

    if (damage == 0)
        return true;

    uint128 const dealt = DealArtifactSpellDamage(player, victim, spellInfo, damage);

    if (marker == ARTIFACT_MARKER_TIME_LOCK && miscValue > 0)
    {
        ApplyTimeLock(player, victim, static_cast<uint32>(miscValue));
    }
    else if (marker == ARTIFACT_MARKER_SOUL_ECHO && miscValue > 0 && dealt > 0)
    {
        uint128 const heal = ScaleBasisPointValue(dealt, miscValue);
        if (heal > 0)
            player->ModifyHealth(ToPositiveHealthChange(heal));
    }

    return true;
}

bool TriggerEquippedArtifactEffects(Player* player, Unit* victim)
{
    if (!player || !victim || victim->isDead())
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
}

class item_artifact_chain_weapon : public ItemScript
{
public:
    item_artifact_chain_weapon() : ItemScript("item_artifact_chain_weapon") { }

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

class artifact_chain_unit_script : public UnitScript
{
public:
    artifact_chain_unit_script() : UnitScript("artifact_chain_unit_script", true, { UNITHOOK_ON_DAMAGE }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint128& damage) override
    {
        if (damage == 0 || !attacker || !victim)
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        static thread_local bool applyingArtifactEffect = false;
        ArtifactEffectGuard guard(applyingArtifactEffect);
        if (!guard.Entered())
            return;

        TriggerEquippedArtifactEffects(player, victim);
    }
};

void AddSC_artifact_quest_chain()
{
    new item_artifact_chain_weapon();
    new artifact_chain_unit_script();
}

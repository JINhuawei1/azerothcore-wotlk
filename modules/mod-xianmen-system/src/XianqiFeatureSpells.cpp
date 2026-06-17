/*
 * Xianqi feature spell runtime.
 *
 * The DBC records only provide passive/equip auras. This script turns the
 * set spells (384001-384016, 384101-384816) and artifact spells (385001-385050)
 * into combat effects while keeping all damage based on "main combat power".
 */

#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Duration.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "UnitScript.h"
#include "Util.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

namespace
{
constexpr uint32 XIANQI_SET_SPELL_FIRST = 384001;
constexpr uint32 XIANQI_SET_SPELL_LAST = 384016;
constexpr uint32 XIANQI_SET_SPELLS_PER_TIER = 16;
constexpr uint32 XIANQI_SET_SPELL_TIER_COUNT = 9;
constexpr uint32 XIANQI_SET_SPELL_TIER_STRIDE = 100;
constexpr uint32 XIANQI_ARTIFACT_SPELL_FIRST = 385001;
constexpr uint32 XIANQI_ARTIFACT_SPELL_LAST = 385050;

constexpr uint32 XIANQI_SET_UNLOCK_ARTIFACT_BASIC = 384014;
constexpr uint32 XIANQI_SET_ARTIFACT_BOUNCE = 384015;
constexpr uint32 XIANQI_SET_UNLOCK_ARTIFACT_ULTIMATE = 384016;

constexpr uint32 XIANQI_ASCEND_DURATION_MS = 12000;
constexpr uint32 XIANQI_ASCEND_COOLDOWN_MS = 90000;
constexpr uint32 XIANQI_TIANJI_COOLDOWN_MS = 25000;
constexpr uint32 XIANQI_LOCK_DOMAIN_COOLDOWN_MS = 30000;
constexpr uint32 XIANQI_LOCK_DOMAIN_DELAY_MS = 5000;
constexpr uint32 XIANQI_SPIRIT_BLADE_PERIOD_MS = 5000;
constexpr uint32 XIANQI_ARTIFACT_ULTIMATE_COOLDOWN_MS = 60000;
constexpr uint32 XIANQI_DEFAULT_TICK_PERIOD_MS = 1000;
constexpr uint32 XIANQI_VISUAL_TARGET_THROTTLE_MS = 250;
constexpr uint32 XIANQI_VISUAL_AREA_THROTTLE_MS = 700;
constexpr uint32 XIANQI_VISUAL_CASTER_THROTTLE_MS = 1000;

enum class FeatureControl
{
    None,
    Root,
    Stun,
    Interrupt,
    Knockback,
    Pull
};

struct ScheduledEffect
{
    uint32 spellId = 0;
    ObjectGuid targetGuid;
    bool area = false;
    uint32 timerMs = 0;
    uint32 intervalMs = 0;
    uint32 ticks = 1;
    int32 percent = 0;
    SpellSchoolMask school = SPELL_SCHOOL_MASK_NORMAL;
    float radius = 0.0f;
    uint32 maxTargets = 1;
    bool fromSet = false;
    bool fromArtifact = false;
    bool ultimate = false;
    FeatureControl control = FeatureControl::None;
    uint32 controlMs = 0;
};

struct PlayerFeatureState
{
    std::map<uint32, uint32> cooldowns;
    std::map<uint32, uint32> visualCooldowns;
    std::map<uint32, uint32> ultimateActiveMs;
    std::vector<ScheduledEffect> scheduled;

    uint32 spiritBladeTimerMs = XIANQI_SPIRIT_BLADE_PERIOD_MS;
    uint8 spiritBlades = 0;
    uint32 lockDomainTimerMs = XIANQI_LOCK_DOMAIN_COOLDOWN_MS;
    uint32 ascendTimerMs = XIANQI_ASCEND_COOLDOWN_MS;
    uint32 ascendRemainingMs = 0;
    uint32 tianjiTimerMs = XIANQI_TIANJI_COOLDOWN_MS;
    bool tianjiReady = false;

    std::map<ObjectGuid, uint32> starMarksMs;
    std::map<ObjectGuid, uint8> tianhuoStacks;
    std::map<ObjectGuid, uint8> judgementStacks;
    std::map<ObjectGuid, uint8> hunterStacks;
    std::map<ObjectGuid, uint8> shadowLotusStacks;
    std::map<ObjectGuid, uint8> condemnationStacks;
    std::map<ObjectGuid, uint8> elementMarks;
    std::map<ObjectGuid, uint32> soulDebtMs;

    ObjectGuid lastHitGuid;
    uint8 consecutiveHits = 0;
    uint8 echoCastCount = 0;
    uint8 elementWheelIndex = 0;
    uint8 triadWheelIndex = 0;
    ObjectGuid recentMeleeAttackerGuid;
    uint32 recentMeleeHitMs = 0;

    uint128 lastDamage = 0;
    uint128 lastNonUltimateDamage = 0;
    uint128 lastElementDamage = 0;
    uint32 lastDamageSpellId = 0;
    SpellSchoolMask lastDamageSchool = SPELL_SCHOOL_MASK_NORMAL;
    SpellSchoolMask lastElementSchool = SPELL_SCHOOL_MASK_NATURE;
};

std::unordered_map<uint32, PlayerFeatureState> s_playerStates;
thread_local bool s_applyingFeatureDamage = false;
thread_local bool s_processingFeatureKill = false;

class FeatureDamageGuard
{
public:
    explicit FeatureDamageGuard(bool& active) : _active(active), _entered(!active)
    {
        if (_entered)
            _active = true;
    }

    ~FeatureDamageGuard()
    {
        if (_entered)
            _active = false;
    }

    [[nodiscard]] bool Entered() const { return _entered; }

private:
    bool& _active;
    bool _entered;
};

PlayerFeatureState& GetState(Player* player)
{
    return s_playerStates[player->GetGUID().GetCounter()];
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

uint128 GetStatPower(Player* player, Stats stat)
{
    if (!player)
        return 0;

    int128 const extended = player->GetExtendedStat128(stat);
    if (extended > 0)
        return ToUInt128Positive(extended);

    return player->GetStatUInt32(stat);
}

uint128 MaxUInt128(uint128 const& left, uint128 const& right)
{
    return left < right ? right : left;
}

uint128 GetMainCombatPower(Player* player)
{
    if (!player)
        return 0;

    uint128 power = 0;
    power = MaxUInt128(power, GetStatPower(player, STAT_STRENGTH));
    power = MaxUInt128(power, GetStatPower(player, STAT_AGILITY));
    power = MaxUInt128(power, GetStatPower(player, STAT_INTELLECT));
    power = MaxUInt128(power, ToUInt128Positive(player->GetExtendedTotalAttackPowerValue(BASE_ATTACK)));
    power = MaxUInt128(power, ToUInt128Positive(player->GetExtendedTotalAttackPowerValue(RANGED_ATTACK)));
    power = MaxUInt128(power, ToUInt128Positive(player->GetExtendedSpellPowerBonus128()));

    return power;
}

uint128 ScalePercent(uint128 const& value, int32 percent)
{
    if (value == 0 || percent <= 0)
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / 100.0L;
    return Acore::Number::ToUInt128Saturated(scaled);
}

bool IsSetSpell(uint32 spellId)
{
    for (uint32 tier = 0; tier < XIANQI_SET_SPELL_TIER_COUNT; ++tier)
    {
        uint32 const first = XIANQI_SET_SPELL_FIRST + tier * XIANQI_SET_SPELL_TIER_STRIDE;
        uint32 const last = first + XIANQI_SET_SPELLS_PER_TIER - 1;
        if (spellId >= first && spellId <= last)
            return true;
    }

    return false;
}

bool IsArtifactSpell(uint32 spellId)
{
    return spellId >= XIANQI_ARTIFACT_SPELL_FIRST && spellId <= XIANQI_ARTIFACT_SPELL_LAST;
}

uint32 GetSetSpellOffset(uint32 spellId)
{
    for (uint32 tier = 0; tier < XIANQI_SET_SPELL_TIER_COUNT; ++tier)
    {
        uint32 const first = XIANQI_SET_SPELL_FIRST + tier * XIANQI_SET_SPELL_TIER_STRIDE;
        uint32 const last = first + XIANQI_SET_SPELLS_PER_TIER - 1;
        if (spellId >= first && spellId <= last)
            return spellId - first;
    }

    return XIANQI_SET_SPELLS_PER_TIER;
}

uint32 MakeSetSpellId(uint32 baseSpellId, uint32 tier)
{
    if (baseSpellId < XIANQI_SET_SPELL_FIRST || baseSpellId > XIANQI_SET_SPELL_LAST || tier >= XIANQI_SET_SPELL_TIER_COUNT)
        return 0;

    return XIANQI_SET_SPELL_FIRST + tier * XIANQI_SET_SPELL_TIER_STRIDE + (baseSpellId - XIANQI_SET_SPELL_FIRST);
}

uint32 NormalizeSetSpellId(uint32 spellId)
{
    uint32 const offset = GetSetSpellOffset(spellId);
    if (offset >= XIANQI_SET_SPELLS_PER_TIER)
        return spellId;

    return XIANQI_SET_SPELL_FIRST + offset;
}

SpellSchoolMask MakeSchoolMask(uint32 mask)
{
    return static_cast<SpellSchoolMask>(mask);
}

SpellInfo const* GetFeatureSpellInfo(uint32 spellId)
{
    if (!IsSetSpell(spellId) && !IsArtifactSpell(spellId))
        return nullptr;

    return sSpellMgr->GetSpellInfo(spellId);
}

uint32 GetFeatureCooldown(uint32 spellId, uint32 fallbackMs)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
    {
        int32 const configured = spellInfo->Effects[EFFECT_1].MiscValueB;
        if (configured > 0)
            return static_cast<uint32>(configured);

        if (spellInfo->RecoveryTime)
            return spellInfo->RecoveryTime;

        if (spellInfo->CategoryRecoveryTime)
            return spellInfo->CategoryRecoveryTime;
    }

    return fallbackMs;
}

float GetFeatureProcChance(uint32 spellId, float fallback)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
    {
        if (spellInfo->ProcChance)
            return static_cast<float>(spellInfo->ProcChance);

        int32 const configured = spellInfo->Effects[EFFECT_0].MiscValueB;
        if (configured > 0)
            return static_cast<float>(configured);
    }

    return fallback;
}

int32 GetFeatureCoeff(uint32 spellId, int32 fallback)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
    {
        int32 const configured = spellInfo->Effects[EFFECT_0].BasePoints;
        if (configured > 0 || fallback == 0)
            return configured;
    }

    return fallback;
}

int32 GetFeatureSecondary(uint32 spellId, int32 fallback)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
    {
        int32 const configured = spellInfo->Effects[EFFECT_1].BasePoints;
        if (configured > 0 || fallback == 0)
            return configured;
    }

    return fallback;
}

uint32 GetFeatureMaxTargets(uint32 spellId, uint32 fallback)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
    {
        if (spellInfo->Effects[EFFECT_0].ChainTarget)
            return spellInfo->Effects[EFFECT_0].ChainTarget;

        int32 const configured = spellInfo->Effects[EFFECT_1].MiscValue;
        if (configured > 0)
            return static_cast<uint32>(configured);

        if (spellInfo->MaxAffectedTargets)
            return spellInfo->MaxAffectedTargets;
    }

    return fallback;
}

uint32 GetFeatureDuration(uint32 spellId, uint32 fallbackMs)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
    {
        if (spellInfo->Effects[EFFECT_1].Amplitude)
            return spellInfo->Effects[EFFECT_1].Amplitude;

        int32 const configured = spellInfo->Effects[EFFECT_2].MiscValue;
        if (configured > 0)
            return static_cast<uint32>(configured);
    }

    return fallbackMs;
}

uint32 GetFeaturePeriod(uint32 spellId, uint32 fallbackMs)
{
    if (SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId))
        if (spellInfo->Effects[EFFECT_0].Amplitude)
            return spellInfo->Effects[EFFECT_0].Amplitude;

    return fallbackMs;
}

uint32 GetFeatureTicks(uint32 spellId, uint32 fallbackTicks)
{
    uint32 const durationMs = GetFeatureDuration(spellId, 0);
    uint32 const periodMs = GetFeaturePeriod(spellId, 0);
    if (durationMs && periodMs)
        return std::max<uint32>(1, durationMs / periodMs);

    return fallbackTicks;
}

bool HasFeatureAura(Player* player, uint32 spellId)
{
    return player && player->HasAura(spellId);
}

uint32 ResolveSetFeatureSpell(Player* player, uint32 baseSpellId)
{
    if (!player || baseSpellId < XIANQI_SET_SPELL_FIRST || baseSpellId > XIANQI_SET_SPELL_LAST)
        return 0;

    for (int32 tier = static_cast<int32>(XIANQI_SET_SPELL_TIER_COUNT) - 1; tier >= 0; --tier)
    {
        uint32 const spellId = MakeSetSpellId(baseSpellId, static_cast<uint32>(tier));
        if (spellId && player->HasAura(spellId))
            return spellId;
    }

    return 0;
}

bool HasSetFeature(Player* player, uint32 baseSpellId, uint32* activeSpellId = nullptr)
{
    uint32 const spellId = ResolveSetFeatureSpell(player, baseSpellId);
    if (activeSpellId)
        *activeSpellId = spellId;

    return spellId != 0;
}

bool HasAnyFeatureAura(Player* player)
{
    if (!player)
        return false;

    for (uint32 tier = 0; tier < XIANQI_SET_SPELL_TIER_COUNT; ++tier)
    {
        uint32 const first = XIANQI_SET_SPELL_FIRST + tier * XIANQI_SET_SPELL_TIER_STRIDE;
        for (uint32 offset = 0; offset < XIANQI_SET_SPELLS_PER_TIER; ++offset)
            if (player->HasAura(first + offset))
                return true;
    }

    for (uint32 spellId = XIANQI_ARTIFACT_SPELL_FIRST; spellId <= XIANQI_ARTIFACT_SPELL_LAST; ++spellId)
        if (player->HasAura(spellId))
            return true;

    return false;
}

bool CanUseArtifactSkill(Player* player, uint32 spellId)
{
    if (!HasFeatureAura(player, spellId))
        return false;

    uint32 const indexInArtifact = (spellId - XIANQI_ARTIFACT_SPELL_FIRST) % 5 + 1;
    if (indexInArtifact <= 2 && !HasSetFeature(player, XIANQI_SET_UNLOCK_ARTIFACT_BASIC))
        return false;

    if (indexInArtifact == 5 && !HasSetFeature(player, XIANQI_SET_UNLOCK_ARTIFACT_ULTIMATE))
        return false;

    return true;
}

bool IsAscended(Player* player, PlayerFeatureState const& state)
{
    return HasSetFeature(player, 384008) && state.ascendRemainingMs > 0;
}

uint32 AdjustTargetCount(Player* player, PlayerFeatureState const& state, uint32 baseCount)
{
    if (!baseCount)
        return 0;

    uint32 ascendSpellId = 0;
    if (baseCount > 1 && state.ascendRemainingMs > 0 && HasSetFeature(player, 384008, &ascendSpellId))
        return baseCount + GetFeatureMaxTargets(ascendSpellId, 5);

    return baseCount;
}

float AdjustProcChance(Player* player, PlayerFeatureState const& state, float chance)
{
    if (chance <= 0.0f)
        return 0.0f;

    if (IsAscended(player, state))
        chance *= 2.0f;

    return std::min(100.0f, chance);
}

bool IsValidFeatureTarget(Player* player, Unit* target)
{
    if (!player || !target || !target->IsAlive() || !player->IsInMap(target) ||
        !player->IsValidAttackTarget(target) || !target->isTargetableForAttack(false, player))
        return false;

    if (Creature* creature = target->ToCreature())
        if (creature->IsTrigger())
            return false;

    return true;
}

bool IsBossOrElite(Unit* target)
{
    if (!target)
        return false;

    Creature* creature = target->ToCreature();
    if (!creature)
        return target->IsPlayer();

    return creature->isWorldBoss() || creature->IsDungeonBoss() ||
        creature->GetCreatureTemplate()->rank >= CREATURE_ELITE_ELITE;
}

bool IsControlledOrImpaired(Unit* target)
{
    if (!target)
        return false;

    return target->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING) ||
        target->HasSilenceAura() || target->HasDecreaseSpeedAura();
}

Unit* GetCurrentEnemyTarget(Player* player)
{
    if (!player)
        return nullptr;

    if (Unit* victim = player->GetVictim())
        if (IsValidFeatureTarget(player, victim))
            return victim;

    if (Unit* selected = player->GetSelectedUnit())
        if (IsValidFeatureTarget(player, selected))
            return selected;

    return nullptr;
}

std::vector<Unit*> SelectNearbyTargets(Player* player, WorldObject* center, Unit* primary, float radius, uint32 maxTargets, bool frontOnly = false, float arc = float(M_PI))
{
    std::vector<Unit*> result;
    if (!player || !center || !maxTargets)
        return result;

    auto alreadyAdded = [&result](Unit* unit)
    {
        return std::find(result.begin(), result.end(), unit) != result.end();
    };

    auto addCandidate = [&](Unit* candidate)
    {
        if (!candidate || alreadyAdded(candidate) || !IsValidFeatureTarget(player, candidate))
            return false;

        if (frontOnly && !player->HasInArc(arc, candidate))
            return false;

        if (!center->IsWithinLOSInMap(candidate))
            return false;

        result.push_back(candidate);
        return result.size() >= maxTargets;
    };

    if (primary && addCandidate(primary))
        return result;

    std::list<Unit*> nearbyTargets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, nearbyTargets, check);
    Cell::VisitAllObjects(center, searcher, radius);

    nearbyTargets.sort([center](Unit* left, Unit* right)
    {
        if (!left)
            return false;
        if (!right)
            return true;
        return left->GetDistance(center) < right->GetDistance(center);
    });

    for (Unit* candidate : nearbyTargets)
        if (addCandidate(candidate))
            break;

    return result;
}

void ApplyTimedControl(Player* player, Unit* target, FeatureControl control, uint32 durationMs)
{
    if (!player || !target || !target->IsAlive() || control == FeatureControl::None)
        return;

    if (control == FeatureControl::Interrupt)
    {
        target->InterruptNonMeleeSpells(false);
        return;
    }

    if (control == FeatureControl::Knockback)
    {
        target->KnockbackFrom(player->GetPositionX(), player->GetPositionY(), 6.0f, 4.0f);
        return;
    }

    if (control == FeatureControl::Pull)
    {
        float const sourceX = target->GetPositionX() + (target->GetPositionX() - player->GetPositionX());
        float const sourceY = target->GetPositionY() + (target->GetPositionY() - player->GetPositionY());
        target->KnockbackFrom(sourceX, sourceY, 3.0f, 1.0f);
        return;
    }

    UnitState state = control == FeatureControl::Stun ? UNIT_STATE_STUNNED : UNIT_STATE_ROOT;
    if (target->HasUnitState(state))
        return;

    ObjectGuid casterGuid = player->GetGUID();
    target->SetControlled(true, state, player);
    target->m_Events.AddEventAtOffset([casterGuid, target, state]()
    {
        Player* caster = ObjectAccessor::FindPlayer(casterGuid);
        target->SetControlled(false, state, caster);
    }, Milliseconds(durationMs ? durationMs : 1000));
}

bool CooldownReady(PlayerFeatureState const& state, uint32 spellId)
{
    auto itr = state.cooldowns.find(spellId);
    return itr == state.cooldowns.end() || itr->second == 0;
}

void StartCooldown(PlayerFeatureState& state, uint32 spellId, uint32 cooldownMs)
{
    cooldownMs = GetFeatureCooldown(spellId, cooldownMs);
    if (cooldownMs)
        state.cooldowns[spellId] = cooldownMs;
}

bool TryUseCooldown(PlayerFeatureState& state, uint32 spellId, uint32 cooldownMs)
{
    if (!CooldownReady(state, spellId))
        return false;

    StartCooldown(state, spellId, cooldownMs);
    return true;
}

uint32 MakeVisualCooldownKey(uint32 spellId, uint32 channel)
{
    return spellId * 8 + channel;
}

bool TryUseVisualCooldown(PlayerFeatureState& state, uint32 spellId, uint32 channel, uint32 cooldownMs)
{
    uint32 const key = MakeVisualCooldownKey(spellId, channel);
    auto itr = state.visualCooldowns.find(key);
    if (itr != state.visualCooldowns.end() && itr->second > 0)
        return false;

    if (cooldownMs)
        state.visualCooldowns[key] = cooldownMs;

    return true;
}

bool IsValidVisualKit(uint32 kitId)
{
    return kitId != 0 && kitId != std::numeric_limits<uint32>::max();
}

uint32 PickVisualKit(uint32 first, uint32 second = 0, uint32 third = 0, uint32 fourth = 0, uint32 fifth = 0)
{
    uint32 const kits[] = { first, second, third, fourth, fifth };
    for (uint32 kitId : kits)
        if (IsValidVisualKit(kitId))
            return kitId;

    return 0;
}

SpellVisualEntry const* GetFeatureVisualEntry(uint32 spellId)
{
    SpellInfo const* spellInfo = GetFeatureSpellInfo(spellId);
    if (!spellInfo)
        return nullptr;

    uint32 visualId = spellInfo->SpellVisual[0] ? spellInfo->SpellVisual[0] : spellId;
    return sSpellVisualStore.LookupEntry(visualId);
}

void PlayFeatureCasterVisual(Player* player, PlayerFeatureState& state, uint32 spellId)
{
    if (!player || !TryUseVisualCooldown(state, spellId, 0, XIANQI_VISUAL_CASTER_THROTTLE_MS))
        return;

    SpellVisualEntry const* visual = GetFeatureVisualEntry(spellId);
    if (!visual)
        return;

    uint32 const kitId = PickVisualKit(visual->CastKit, visual->PrecastKit, visual->StateKit, visual->ChannelKit, visual->CasterImpactKit);
    if (kitId)
        player->SendPlaySpellVisual(kitId);
}

void PlayFeatureTargetVisual(Player* player, Unit* target, PlayerFeatureState& state, uint32 spellId)
{
    if (!player || !target || !player->IsInMap(target) ||
        !TryUseVisualCooldown(state, spellId, 1, XIANQI_VISUAL_TARGET_THROTTLE_MS))
        return;

    SpellVisualEntry const* visual = GetFeatureVisualEntry(spellId);
    if (!visual)
        return;

    uint32 const kitId = PickVisualKit(visual->TargetImpactKit, visual->ImpactKit, visual->ImpactAreaKit, visual->InstantAreaKit);
    if (kitId)
        player->SendPlaySpellImpact(target->GetGUID(), kitId);
}

void PlayFeatureAreaVisual(Player* player, Unit* center, PlayerFeatureState& state, uint32 spellId)
{
    if (!player || !center || !player->IsInMap(center) ||
        !TryUseVisualCooldown(state, spellId, 2, XIANQI_VISUAL_AREA_THROTTLE_MS))
        return;

    SpellVisualEntry const* visual = GetFeatureVisualEntry(spellId);
    if (!visual)
        return;

    uint32 const kitId = PickVisualKit(visual->ImpactAreaKit, visual->PersistentAreaKit, visual->InstantAreaKit, visual->StateKit);
    if (kitId)
        center->SendPlaySpellVisual(kitId);
}

struct ProcDecision
{
    bool triggered = false;
    uint32 tianjiBounces = 0;
};

ProcDecision RollChanceProc(Player* player, PlayerFeatureState& state, uint32 spellId, float chance, uint32 cooldownMs)
{
    ProcDecision decision;
    chance = GetFeatureProcChance(spellId, chance);

    if (!CooldownReady(state, spellId))
        return decision;

    uint32 tianjiSpellId = 0;
    if (HasSetFeature(player, 384012, &tianjiSpellId) && state.tianjiReady)
    {
        state.tianjiReady = false;
        state.tianjiTimerMs = GetFeatureCooldown(tianjiSpellId, XIANQI_TIANJI_COOLDOWN_MS);
        decision.triggered = true;
        decision.tianjiBounces = GetFeatureMaxTargets(tianjiSpellId, 2);
        StartCooldown(state, spellId, cooldownMs);
        return decision;
    }

    if (roll_chance_f(AdjustProcChance(player, state, chance)))
    {
        decision.triggered = true;
        StartCooldown(state, spellId, cooldownMs);
    }

    return decision;
}

uint128 DealRawFeatureDamage(Player* player, Unit* victim, PlayerFeatureState& state, uint32 spellId, uint128 const& damage,
    SpellSchoolMask school, bool fromSet, bool fromArtifact, bool ultimate, bool allowMingxing = true, bool recordRecent = true);

void DealAreaScaled(Player* player, Unit* centerUnit, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, float radius, uint32 maxTargets, bool fromSet, bool fromArtifact, bool ultimate = false,
    FeatureControl control = FeatureControl::None, uint32 controlMs = 0);

void HandleKillTriggers(Player* player, Unit* victim);

void TrySetResonance(Player* player, Unit* victim, PlayerFeatureState& state, uint32 spellId, uint128 const& originalDamage, SpellSchoolMask school)
{
    uint32 resonanceSpellId = 0;
    if (!HasSetFeature(player, 384010, &resonanceSpellId) || NormalizeSetSpellId(spellId) == 384010 || originalDamage == 0)
        return;

    if (!RollChanceProc(player, state, resonanceSpellId, 30.0f, 1000).triggered)
        return;

    DealRawFeatureDamage(player, victim, state, spellId, ScalePercent(originalDamage, GetFeatureCoeff(resonanceSpellId, 50)),
        school, false, false, false, false, false);
}

uint128 DealRawFeatureDamage(Player* player, Unit* victim, PlayerFeatureState& state, uint32 spellId, uint128 const& damage,
    SpellSchoolMask school, bool fromSet, bool /*fromArtifact*/, bool ultimate, bool allowMingxing, bool recordRecent)
{
    if (!player || !victim || damage == 0 || !IsValidFeatureTarget(player, victim))
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    if (!school)
        school = spellInfo->GetSchoolMask() ? spellInfo->GetSchoolMask() : SPELL_SCHOOL_MASK_NORMAL;

    bool const sendClientFeedback = player->ShouldSendCustomProcClientFeedback(victim, spellInfo, "xianqi-feature");
    uint128 dealt = 0;
    {
        FeatureDamageGuard guard(s_applyingFeatureDamage);
        if (!guard.Entered())
            return 0;

        SpellNonMeleeDamage damageInfo(player, victim, spellInfo, school);
        damageInfo.damage = damage;
        Unit::DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
        if (sendClientFeedback)
            player->SendSpellNonMeleeDamageLog(&damageInfo);

        CleanDamage cleanDamage(damageInfo.cleanDamage, damageInfo.absorb, BASE_ATTACK,
            (damageInfo.HitInfo & SPELL_HIT_TYPE_CRIT) ? MELEE_HIT_CRIT : MELEE_HIT_NORMAL);
        dealt = Unit::DealDamage(player, victim, damageInfo.damage, &cleanDamage, SPELL_DIRECT_DAMAGE, school, spellInfo, true);
    }

    if (dealt == 0)
        return 0;

    if (sendClientFeedback)
        PlayFeatureTargetVisual(player, victim, state, spellId);

    if (recordRecent)
    {
        state.lastDamage = dealt;
        state.lastDamageSpellId = spellId;
        state.lastDamageSchool = school;

        if (!ultimate)
            state.lastNonUltimateDamage = dealt;

        if (spellId == 385021 || spellId == 385022 || spellId == 385024 || spellId == 385025)
        {
            state.lastElementDamage = dealt;
            state.lastElementSchool = school;
        }
    }

    if (fromSet)
        TrySetResonance(player, victim, state, spellId, dealt, school);

    uint32 mingxingSpellId = 0;
    if (allowMingxing && NormalizeSetSpellId(spellId) != 384009 && HasSetFeature(player, 384009, &mingxingSpellId))
    {
        auto markItr = state.starMarksMs.find(victim->GetGUID());
        if (markItr != state.starMarksMs.end() && markItr->second > 0)
        {
            DealRawFeatureDamage(player, victim, state, mingxingSpellId, ScalePercent(GetMainCombatPower(player), GetFeatureCoeff(mingxingSpellId, 80)),
                SPELL_SCHOOL_MASK_ARCANE, true, false, false, false, false);
        }
    }

    if (!victim->IsAlive())
    {
        if (!s_processingFeatureKill)
        {
            s_processingFeatureKill = true;
            HandleKillTriggers(player, victim);
            s_processingFeatureKill = false;
        }
    }

    return dealt;
}

uint128 DealScaledDamage(Player* player, Unit* victim, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, bool fromSet, bool fromArtifact, bool ultimate = false, bool allowMingxing = true,
    bool recordRecent = true, uint128 const* baseOverride = nullptr)
{
    uint128 const base = baseOverride ? *baseOverride : GetMainCombatPower(player);
    return DealRawFeatureDamage(player, victim, state, spellId, ScalePercent(base, percent), school, fromSet, fromArtifact, ultimate, allowMingxing, recordRecent);
}

void DealAreaScaled(Player* player, Unit* centerUnit, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, float radius, uint32 maxTargets, bool fromSet, bool fromArtifact, bool ultimate,
    FeatureControl control, uint32 controlMs)
{
    if (!player || !centerUnit)
        return;

    uint32 const adjustedMaxTargets = AdjustTargetCount(player, state, GetFeatureMaxTargets(spellId, maxTargets));
    PlayFeatureAreaVisual(player, centerUnit, state, spellId);

    std::vector<Unit*> targets = SelectNearbyTargets(player, centerUnit, centerUnit, radius, adjustedMaxTargets);
    for (Unit* target : targets)
    {
        DealScaledDamage(player, target, state, spellId, percent, school, fromSet, fromArtifact, ultimate);
        if (control != FeatureControl::None && !IsBossOrElite(target))
            ApplyTimedControl(player, target, control, controlMs);
    }
}

void DealChainScaled(Player* player, Unit* firstTarget, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, uint32 maxJumps, bool fromSet, bool fromArtifact)
{
    if (!player || !firstTarget || !maxJumps)
        return;

    std::vector<Unit*> targets = SelectNearbyTargets(player, firstTarget, firstTarget, 12.0f,
        AdjustTargetCount(player, state, GetFeatureMaxTargets(spellId, maxJumps)));
    for (Unit* target : targets)
        DealScaledDamage(player, target, state, spellId, percent, school, fromSet, fromArtifact);
}

void DealExtraBounces(Player* player, Unit* sourceTarget, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, uint32 count, int32 damageScalePercent)
{
    if (!player || !sourceTarget || !count || damageScalePercent <= 0)
        return;

    std::vector<Unit*> targets = SelectNearbyTargets(player, sourceTarget, nullptr, 12.0f, count + 1);
    uint32 applied = 0;
    uint128 const baseDamage = ScalePercent(GetMainCombatPower(player), percent);
    uint128 const bounceDamage = ScalePercent(baseDamage, damageScalePercent);
    for (Unit* target : targets)
    {
        if (target == sourceTarget)
            continue;

        DealRawFeatureDamage(player, target, state, spellId, bounceDamage, school, false, true, false, true, false);
        if (++applied >= count)
            break;
    }
}

void ApplyArtifactHitBounces(Player* player, Unit* victim, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, uint32 tianjiBounces)
{
    uint32 bounceSpellId = 0;
    if (HasSetFeature(player, XIANQI_SET_ARTIFACT_BOUNCE, &bounceSpellId))
        DealExtraBounces(player, victim, state, spellId, percent, school,
            GetFeatureMaxTargets(bounceSpellId, 1), GetFeatureCoeff(bounceSpellId, 65));

    if (tianjiBounces)
        DealExtraBounces(player, victim, state, spellId, percent, school, tianjiBounces, 100);
}

void ScheduleSingle(PlayerFeatureState& state, uint32 spellId, Unit* target, uint32 delayMs, uint32 intervalMs, uint32 ticks,
    int32 percent, SpellSchoolMask school, bool fromSet, bool fromArtifact, bool ultimate = false,
    FeatureControl control = FeatureControl::None, uint32 controlMs = 0)
{
    if (!target || !ticks)
        return;

    ScheduledEffect effect;
    effect.spellId = spellId;
    effect.targetGuid = target->GetGUID();
    effect.area = false;
    effect.timerMs = delayMs;
    effect.intervalMs = intervalMs;
    effect.ticks = ticks;
    effect.percent = percent;
    effect.school = school;
    effect.fromSet = fromSet;
    effect.fromArtifact = fromArtifact;
    effect.ultimate = ultimate;
    effect.control = control;
    effect.controlMs = controlMs;
    state.scheduled.push_back(effect);
}

void ScheduleArea(PlayerFeatureState& state, uint32 spellId, Unit* center, uint32 delayMs, uint32 intervalMs, uint32 ticks,
    int32 percent, SpellSchoolMask school, float radius, uint32 maxTargets, bool fromSet, bool fromArtifact, bool ultimate = false,
    FeatureControl control = FeatureControl::None, uint32 controlMs = 0)
{
    if (!center || !ticks)
        return;

    ScheduledEffect effect;
    effect.spellId = spellId;
    effect.targetGuid = center->GetGUID();
    effect.area = true;
    effect.timerMs = delayMs;
    effect.intervalMs = intervalMs;
    effect.ticks = ticks;
    effect.percent = percent;
    effect.school = school;
    effect.radius = radius;
    effect.maxTargets = maxTargets;
    effect.fromSet = fromSet;
    effect.fromArtifact = fromArtifact;
    effect.ultimate = ultimate;
    effect.control = control;
    effect.controlMs = controlMs;
    state.scheduled.push_back(effect);
}

void TriggerUltimateFollowups(Player* player, Unit* target, PlayerFeatureState& state)
{
    if (!player || !target)
        return;

    uint32 mingpanSpellId = 0;
    if (HasSetFeature(player, 384013, &mingpanSpellId) && TryUseCooldown(state, mingpanSpellId, 1000))
    {
        uint8 const fragments = static_cast<uint8>(GetFeatureSecondary(mingpanSpellId, 7));
        for (uint8 i = 0; i < fragments; ++i)
            DealScaledDamage(player, target, state, mingpanSpellId, GetFeatureCoeff(mingpanSpellId, 110), SPELL_SCHOOL_MASK_ARCANE, true, false, false);
    }

    uint32 sancaiSpellId = 0;
    if (HasSetFeature(player, 384016, &sancaiSpellId) && TryUseCooldown(state, sancaiSpellId, 3000))
        DealAreaScaled(player, target, state, sancaiSpellId, GetFeatureCoeff(sancaiSpellId, 500), SPELL_SCHOOL_MASK_ALL, 10.0f, 8, true, false, false);
}

void StartUltimate(Player* player, Unit* target, PlayerFeatureState& state, uint32 spellId)
{
    if (!CanUseArtifactSkill(player, spellId) || !TryUseCooldown(state, spellId, XIANQI_ARTIFACT_ULTIMATE_COOLDOWN_MS))
        return;

    PlayFeatureCasterVisual(player, state, spellId);
    TriggerUltimateFollowups(player, target, state);

    switch (spellId)
    {
        case 385005:
            state.ultimateActiveMs[spellId] = GetFeatureDuration(spellId, 10000);
            break;
        case 385010:
            ScheduleArea(state, spellId, target, 0, GetFeaturePeriod(spellId, XIANQI_DEFAULT_TICK_PERIOD_MS),
                GetFeatureTicks(spellId, 8), GetFeatureCoeff(spellId, 180), SPELL_SCHOOL_MASK_HOLY, 10.0f, 10, false, true, true);
            break;
        case 385015:
            ScheduleArea(state, spellId, target, 0, GetFeaturePeriod(spellId, XIANQI_DEFAULT_TICK_PERIOD_MS),
                GetFeatureTicks(spellId, 6), GetFeatureCoeff(spellId, 220), SPELL_SCHOOL_MASK_SHADOW, 10.0f, 8, false, true, true);
            break;
        case 385020:
            ScheduleArea(state, spellId, target, 0, GetFeaturePeriod(spellId, XIANQI_DEFAULT_TICK_PERIOD_MS),
                GetFeatureTicks(spellId, 8), GetFeatureCoeff(spellId, 160), SPELL_SCHOOL_MASK_NORMAL, 12.0f, 12, false, true, true);
            break;
        case 385025:
        {
            uint8 marks = state.elementMarks[target->GetGUID()];
            if (!marks)
                marks = 1;
            marks = std::min<uint8>(marks, static_cast<uint8>(GetFeatureSecondary(spellId, 8)));
            state.elementMarks[target->GetGUID()] = 0;
            for (uint8 i = 0; i < marks; ++i)
                DealScaledDamage(player, target, state, spellId, GetFeatureCoeff(spellId, 180), SPELL_SCHOOL_MASK_NATURE, false, true, true);
            break;
        }
        case 385030:
            ScheduleArea(state, spellId, target, 0, GetFeaturePeriod(spellId, XIANQI_DEFAULT_TICK_PERIOD_MS),
                GetFeatureTicks(spellId, 8), GetFeatureCoeff(spellId, 170), SPELL_SCHOOL_MASK_NATURE, 10.0f, 10, false, true, true);
            break;
        case 385035:
            ScheduleArea(state, spellId, target, 0, XIANQI_DEFAULT_TICK_PERIOD_MS,
                static_cast<uint32>(GetFeatureSecondary(spellId, 5)), GetFeatureCoeff(spellId, 180), SPELL_SCHOOL_MASK_NORMAL, 8.0f, 8, false, true, true);
            break;
        case 385040:
        {
            uint128 copyBase = state.lastNonUltimateDamage ? state.lastNonUltimateDamage : ScalePercent(GetMainCombatPower(player), 600);
            DealRawFeatureDamage(player, target, state, spellId, ScalePercent(copyBase, GetFeatureCoeff(spellId, 80)),
                state.lastDamageSchool ? state.lastDamageSchool : SPELL_SCHOOL_MASK_ARCANE, false, true, true, true, false);
            break;
        }
        case 385045:
        {
            std::vector<Unit*> targets = SelectNearbyTargets(player, target, target, 18.0f,
                AdjustTargetCount(player, state, GetFeatureMaxTargets(spellId, 10)));
            for (Unit* unit : targets)
                DealScaledDamage(player, unit, state, spellId, GetFeatureCoeff(spellId, 200), SPELL_SCHOOL_MASK_SHADOW, false, true, true);
            break;
        }
        case 385050:
            ScheduleArea(state, spellId, target, 0, GetFeaturePeriod(spellId, XIANQI_DEFAULT_TICK_PERIOD_MS),
                GetFeatureTicks(spellId, 8), GetFeatureCoeff(spellId, 190),
                MakeSchoolMask(SPELL_SCHOOL_MASK_HOLY | SPELL_SCHOOL_MASK_SHADOW), 12.0f, 12, false, true, true);
            break;
        default:
            break;
    }
}

bool HasAnySetMarker(PlayerFeatureState const& state, ObjectGuid guid)
{
    auto starItr = state.starMarksMs.find(guid);
    if (starItr != state.starMarksMs.end() && starItr->second > 0)
        return true;

    auto fireItr = state.tianhuoStacks.find(guid);
    if (fireItr != state.tianhuoStacks.end() && fireItr->second > 0)
        return true;

    return state.lastHitGuid == guid && state.consecutiveHits > 0;
}

void HandleSetOutgoingHit(Player* player, Unit* victim, PlayerFeatureState& state)
{
    uint32 mingxingSpellId = 0;
    if (HasSetFeature(player, 384009, &mingxingSpellId))
    {
        auto& markMs = state.starMarksMs[victim->GetGUID()];
        if (!markMs)
            markMs = GetFeatureDuration(mingxingSpellId, 12000);
    }

    uint32 xianmaiSpellId = 0;
    if (HasSetFeature(player, 384001, &xianmaiSpellId))
    {
        ProcDecision proc = RollChanceProc(player, state, xianmaiSpellId, 18.0f, 1000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(xianmaiSpellId, 160);
            DealScaledDamage(player, victim, state, xianmaiSpellId, percent, SPELL_SCHOOL_MASK_ARCANE, true, false);
            if (HasAnySetMarker(state, victim->GetGUID()))
                DealAreaScaled(player, victim, state, xianmaiSpellId, GetFeatureSecondary(xianmaiSpellId, 60), SPELL_SCHOOL_MASK_ARCANE, 6.0f, 5, true, false);
            if (proc.tianjiBounces)
                DealExtraBounces(player, victim, state, xianmaiSpellId, percent, SPELL_SCHOOL_MASK_ARCANE, proc.tianjiBounces, 100);
        }
    }

    uint32 spiritBladeSpellId = 0;
    if (HasSetFeature(player, 384002, &spiritBladeSpellId) && state.spiritBlades)
    {
        uint8 blades = state.spiritBlades;
        state.spiritBlades = 0;
        std::vector<Unit*> targets = SelectNearbyTargets(player, victim, victim, 10.0f,
            std::min<uint32>(blades, GetFeatureMaxTargets(spiritBladeSpellId, blades)));
        for (Unit* target : targets)
            DealScaledDamage(player, target, state, spiritBladeSpellId, GetFeatureCoeff(spiritBladeSpellId, 90), SPELL_SCHOOL_MASK_NORMAL, true, false);
    }

    uint32 tianhuoSpellId = 0;
    if (HasSetFeature(player, 384003, &tianhuoSpellId))
    {
        uint8& stacks = state.tianhuoStacks[victim->GetGUID()];
        stacks = std::min<uint8>(3, stacks + 1);
        if (stacks >= 3)
        {
            if (TryUseCooldown(state, tianhuoSpellId, 1000))
            {
                stacks = 0;
                DealAreaScaled(player, victim, state, tianhuoSpellId, GetFeatureCoeff(tianhuoSpellId, 420), SPELL_SCHOOL_MASK_FIRE, 8.0f, 8, true, false);
            }
        }
    }

    uint32 wuxingSpellId = 0;
    if (HasSetFeature(player, 384005, &wuxingSpellId))
    {
        ProcDecision proc = RollChanceProc(player, state, wuxingSpellId, 20.0f, 3000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(wuxingSpellId, 180);
            uint32 const controlMs = GetFeatureDuration(wuxingSpellId, 1500);
            SpellSchoolMask branchSchool = SPELL_SCHOOL_MASK_FIRE;
            uint32 const branch = urand(0, 4);
            switch (branch)
            {
                case 0:
                    DealAreaScaled(player, victim, state, wuxingSpellId, percent, branchSchool, 6.0f, 5, true, false);
                    break;
                case 1:
                    branchSchool = SPELL_SCHOOL_MASK_FROST;
                    DealScaledDamage(player, victim, state, wuxingSpellId, percent, branchSchool, true, false);
                    ApplyTimedControl(player, victim, FeatureControl::Root, controlMs);
                    break;
                case 2:
                    branchSchool = SPELL_SCHOOL_MASK_NATURE;
                    DealChainScaled(player, victim, state, wuxingSpellId, percent, branchSchool, 4, true, false);
                    break;
                case 3:
                    branchSchool = SPELL_SCHOOL_MASK_SHADOW;
                    DealScaledDamage(player, victim, state, wuxingSpellId, percent, branchSchool, true, false);
                    ApplyTimedControl(player, victim, FeatureControl::Interrupt, 0);
                    break;
                default:
                    branchSchool = SPELL_SCHOOL_MASK_HOLY;
                    DealScaledDamage(player, victim, state, wuxingSpellId, percent, branchSchool, true, false);
                    break;
            }

            if (proc.tianjiBounces)
                DealExtraBounces(player, victim, state, wuxingSpellId, percent, branchSchool, proc.tianjiBounces, 100);
        }
    }

    uint32 tianfaSpellId = 0;
    if (HasSetFeature(player, 384011, &tianfaSpellId))
    {
        ObjectGuid guid = victim->GetGUID();
        if (state.lastHitGuid == guid)
            state.consecutiveHits = std::min<uint8>(6, state.consecutiveHits + 1);
        else
        {
            state.lastHitGuid = guid;
            state.consecutiveHits = 1;
        }

        if (state.consecutiveHits >= 6 && TryUseCooldown(state, tianfaSpellId, 6000))
        {
            state.consecutiveHits = 0;
            ScheduleArea(state, tianfaSpellId, victim, 0, GetFeaturePeriod(tianfaSpellId, XIANQI_DEFAULT_TICK_PERIOD_MS),
                GetFeatureTicks(tianfaSpellId, 3), GetFeatureCoeff(tianfaSpellId, 180), SPELL_SCHOOL_MASK_NATURE, 8.0f, 6, true, false);
        }
    }
}

void HandleArtifactOutgoingHit(Player* player, Unit* victim, PlayerFeatureState& state)
{
    if (CanUseArtifactSkill(player, 385001))
    {
        ProcDecision proc = RollChanceProc(player, state, 385001, 22.0f, 4000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385001, 220);
            std::vector<Unit*> targets = SelectNearbyTargets(player, victim, victim, 12.0f,
                AdjustTargetCount(player, state, GetFeatureMaxTargets(385001, 6)), true, float(2.0 * M_PI / 3.0));
            for (Unit* target : targets)
                DealScaledDamage(player, target, state, 385001, percent, SPELL_SCHOOL_MASK_NORMAL, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385001, percent, SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385003))
    {
        ProcDecision proc = RollChanceProc(player, state, 385003, 20.0f, 6000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385003, 260);
            std::vector<Unit*> targets = SelectNearbyTargets(player, victim, victim, 18.0f,
                AdjustTargetCount(player, state, GetFeatureMaxTargets(385003, 5)), true, float(M_PI / 5.0));
            for (Unit* target : targets)
            {
                DealScaledDamage(player, target, state, 385003, percent, SPELL_SCHOOL_MASK_NORMAL, false, true);
                ApplyTimedControl(player, target, FeatureControl::Knockback, 0);
            }
            ApplyArtifactHitBounces(player, victim, state, 385003, percent, SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385006))
    {
        ProcDecision proc = RollChanceProc(player, state, 385006, 22.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385006, 260);
            ScheduleArea(state, 385006, victim, GetFeatureDuration(385006, 1000), 0, 1, percent, SPELL_SCHOOL_MASK_HOLY, 8.0f, 1, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385006, percent, SPELL_SCHOOL_MASK_HOLY, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385007))
    {
        ProcDecision proc = RollChanceProc(player, state, 385007, 20.0f, 4000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385007, 140);
            SpellSchoolMask const school = MakeSchoolMask(SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_HOLY);
            DealChainScaled(player, victim, state, 385007, percent, school, 5, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385007, percent, school, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385008))
    {
        ProcDecision proc = RollChanceProc(player, state, 385008, 18.0f, 8000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385008, 300);
            DealScaledDamage(player, victim, state, 385008, percent, SPELL_SCHOOL_MASK_HOLY, false, true);
            if (!IsBossOrElite(victim))
                ApplyTimedControl(player, victim, FeatureControl::Stun, GetFeatureDuration(385008, 2000));
            ApplyArtifactHitBounces(player, victim, state, 385008, percent, SPELL_SCHOOL_MASK_HOLY, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385009) && IsBossOrElite(victim))
    {
        uint8& stacks = state.judgementStacks[victim->GetGUID()];
        stacks = std::min<uint8>(5, stacks + 1);
        if (stacks >= 5)
        {
            if (TryUseCooldown(state, 385009, 1000))
            {
                stacks = 0;
                DealScaledDamage(player, victim, state, 385009, GetFeatureCoeff(385009, 600), SPELL_SCHOOL_MASK_HOLY, false, true);
            }
        }
    }

    if (CanUseArtifactSkill(player, 385011))
    {
        ProcDecision proc = RollChanceProc(player, state, 385011, 22.0f, 4000);
        if (proc.triggered)
        {
            int32 const frostPercent = GetFeatureCoeff(385011, 130);
            int32 const shadowPercent = GetFeatureSecondary(385011, 130);
            DealScaledDamage(player, victim, state, 385011, frostPercent, SPELL_SCHOOL_MASK_FROST, false, true);
            DealScaledDamage(player, victim, state, 385011, shadowPercent, SPELL_SCHOOL_MASK_SHADOW, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385011, frostPercent + shadowPercent,
                MakeSchoolMask(SPELL_SCHOOL_MASK_FROST | SPELL_SCHOOL_MASK_SHADOW), proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385013))
    {
        ProcDecision proc = RollChanceProc(player, state, 385013, 20.0f, 8000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385013, 260);
            if (IsBossOrElite(victim))
                DealScaledDamage(player, victim, state, 385013, percent, SPELL_SCHOOL_MASK_FROST, false, true);
            else
                ApplyTimedControl(player, victim, FeatureControl::Root, GetFeatureDuration(385013, 1500));
            ApplyArtifactHitBounces(player, victim, state, 385013, percent, SPELL_SCHOOL_MASK_FROST, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385016))
    {
        uint8& stacks = state.hunterStacks[victim->GetGUID()];
        stacks = std::min<uint8>(4, stacks + 1);
        if (stacks >= 4)
        {
            stacks = 0;
            int32 const percent = GetFeatureCoeff(385016, 520);
            DealScaledDamage(player, victim, state, 385016, percent, SPELL_SCHOOL_MASK_ARCANE, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385016, percent, SPELL_SCHOOL_MASK_ARCANE, 0);
        }
    }

    if (CanUseArtifactSkill(player, 385017))
    {
        ProcDecision proc = RollChanceProc(player, state, 385017, 22.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385017, 240);
            std::vector<Unit*> targets = SelectNearbyTargets(player, victim, victim, 22.0f,
                AdjustTargetCount(player, state, GetFeatureMaxTargets(385017, 8)), true, float(M_PI / 6.0));
            for (Unit* target : targets)
                DealScaledDamage(player, target, state, 385017, percent, SPELL_SCHOOL_MASK_NORMAL, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385017, percent, SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385019))
    {
        ProcDecision proc = RollChanceProc(player, state, 385019, 22.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385019, 120);
            uint8 const strikes = static_cast<uint8>(GetFeatureSecondary(385019, 3));
            for (uint8 i = 0; i < strikes; ++i)
                DealScaledDamage(player, victim, state, 385019, percent, SPELL_SCHOOL_MASK_NORMAL, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385019, percent * strikes, SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385021))
    {
        ProcDecision proc = RollChanceProc(player, state, 385021, 22.0f, 4000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385021, 150);
            DealChainScaled(player, victim, state, 385021, percent, SPELL_SCHOOL_MASK_NATURE, 5, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385021, percent, SPELL_SCHOOL_MASK_NATURE, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385022))
    {
        ProcDecision proc = RollChanceProc(player, state, 385022, 25.0f, 4000);
        if (proc.triggered)
        {
            SpellSchoolMask school = SPELL_SCHOOL_MASK_FIRE;
            switch (state.elementWheelIndex++ % 4)
            {
                case 1: school = SPELL_SCHOOL_MASK_FROST; break;
                case 2: school = SPELL_SCHOOL_MASK_NATURE; break;
                case 3: school = SPELL_SCHOOL_MASK_NORMAL; break;
                default: break;
            }
            int32 const percent = GetFeatureCoeff(385022, 210);
            state.elementMarks[victim->GetGUID()] = std::min<uint8>(
                static_cast<uint8>(GetFeatureSecondary(385025, 8)), state.elementMarks[victim->GetGUID()] + 1);
            DealAreaScaled(player, victim, state, 385022, percent, school, 6.0f, 6, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385022, percent, school, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385026))
    {
        ProcDecision proc = RollChanceProc(player, state, 385026, 20.0f, 8000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385026, 240);
            DealScaledDamage(player, victim, state, 385026, percent, SPELL_SCHOOL_MASK_NATURE, false, true);
            ApplyTimedControl(player, victim, FeatureControl::Root, GetFeatureDuration(385026, 2000));
            ApplyArtifactHitBounces(player, victim, state, 385026, percent, SPELL_SCHOOL_MASK_NATURE, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385028))
    {
        ProcDecision proc = RollChanceProc(player, state, 385028, 22.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385028, 260);
            uint32 const bleedPeriod = XIANQI_DEFAULT_TICK_PERIOD_MS;
            uint32 const bleedTicks = std::max<uint32>(1, GetFeatureDuration(385028, 4000) / bleedPeriod);
            DealScaledDamage(player, victim, state, 385028, percent, SPELL_SCHOOL_MASK_NORMAL, false, true);
            ScheduleSingle(state, 385028, victim, bleedPeriod, bleedPeriod, bleedTicks, 60, SPELL_SCHOOL_MASK_NORMAL, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385028, percent, SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385031))
    {
        ProcDecision proc = RollChanceProc(player, state, 385031, 20.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385031, 300);
            DealScaledDamage(player, victim, state, 385031, percent, SPELL_SCHOOL_MASK_NORMAL, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385031, percent, SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385032))
    {
        uint8& stacks = state.shadowLotusStacks[victim->GetGUID()];
        stacks = std::min<uint8>(5, stacks + 1);
        if (stacks >= 5)
        {
            if (TryUseCooldown(state, 385032, 1000))
            {
                stacks = 0;
                int32 const percent = GetFeatureCoeff(385032, 560);
                SpellSchoolMask const school = MakeSchoolMask(SPELL_SCHOOL_MASK_NATURE | SPELL_SCHOOL_MASK_SHADOW);
                DealAreaScaled(player, victim, state, 385032, percent, school, 8.0f, 6, false, true);
                ApplyArtifactHitBounces(player, victim, state, 385032, percent, school, 0);
            }
        }
    }

    if (CanUseArtifactSkill(player, 385033))
    {
        ProcDecision proc = RollChanceProc(player, state, 385033, 20.0f, 6000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385033, 120);
            SpellSchoolMask const school = MakeSchoolMask(SPELL_SCHOOL_MASK_NATURE | SPELL_SCHOOL_MASK_SHADOW);
            DealChainScaled(player, victim, state, 385033, percent, school, 6, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385033, percent, school, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385036))
    {
        ProcDecision proc = RollChanceProc(player, state, 385036, 22.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385036, 280);
            DealScaledDamage(player, victim, state, 385036, percent, SPELL_SCHOOL_MASK_ARCANE, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385036, percent, SPELL_SCHOOL_MASK_ARCANE, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385037) && TryUseCooldown(state, 385037, 3000))
    {
        SpellSchoolMask school = SPELL_SCHOOL_MASK_FIRE;
        switch (state.triadWheelIndex++ % 3)
        {
            case 1: school = SPELL_SCHOOL_MASK_FROST; break;
            case 2: school = SPELL_SCHOOL_MASK_ARCANE; break;
            default: break;
        }
        int32 const percent = GetFeatureCoeff(385037, 170);
        DealAreaScaled(player, victim, state, 385037, percent, school, 6.0f, 6, false, true);
        ApplyArtifactHitBounces(player, victim, state, 385037, percent, school, 0);
    }

    if (CanUseArtifactSkill(player, 385039))
    {
        ProcDecision proc = RollChanceProc(player, state, 385039, 18.0f, 8000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385039, 160);
            ScheduleArea(state, 385039, victim, 0, GetFeaturePeriod(385039, XIANQI_DEFAULT_TICK_PERIOD_MS),
                static_cast<uint32>(GetFeatureSecondary(385039, 4)), percent, SPELL_SCHOOL_MASK_FIRE, 8.0f, 6, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385039, percent, SPELL_SCHOOL_MASK_FIRE, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385041))
    {
        ProcDecision proc = RollChanceProc(player, state, 385041, 20.0f, 4000);
        if (proc.triggered)
        {
            state.soulDebtMs[victim->GetGUID()] = GetFeatureDuration(385041, 8000);
            if (proc.tianjiBounces)
                DealExtraBounces(player, victim, state, 385041, GetFeatureCoeff(385041, 420), SPELL_SCHOOL_MASK_SHADOW, proc.tianjiBounces, 100);
        }
    }

    if (CanUseArtifactSkill(player, 385043) && IsControlledOrImpaired(victim) && TryUseCooldown(state, 385043, 2500))
    {
        int32 const percent = GetFeatureCoeff(385043, 260);
        DealScaledDamage(player, victim, state, 385043, percent, SPELL_SCHOOL_MASK_SHADOW, false, true);
        ApplyArtifactHitBounces(player, victim, state, 385043, percent, SPELL_SCHOOL_MASK_SHADOW, 0);
    }

    if (CanUseArtifactSkill(player, 385046))
    {
        ProcDecision proc = RollChanceProc(player, state, 385046, 22.0f, 4000);
        if (proc.triggered)
        {
            int32 const holyPercent = GetFeatureCoeff(385046, 150);
            int32 const shadowPercent = GetFeatureSecondary(385046, 150);
            DealScaledDamage(player, victim, state, 385046, holyPercent, SPELL_SCHOOL_MASK_HOLY, false, true);
            DealScaledDamage(player, victim, state, 385046, shadowPercent, SPELL_SCHOOL_MASK_SHADOW, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385046, holyPercent + shadowPercent,
                MakeSchoolMask(SPELL_SCHOOL_MASK_HOLY | SPELL_SCHOOL_MASK_SHADOW), proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385047))
    {
        ProcDecision proc = RollChanceProc(player, state, 385047, 22.0f, 5000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385047, 260);
            std::vector<Unit*> targets = SelectNearbyTargets(player, victim, victim, 18.0f,
                AdjustTargetCount(player, state, GetFeatureMaxTargets(385047, 8)), true, float(M_PI / 6.0));
            for (Unit* target : targets)
                DealScaledDamage(player, target, state, 385047, percent, SPELL_SCHOOL_MASK_SHADOW, false, true);
            ApplyArtifactHitBounces(player, victim, state, 385047, percent, SPELL_SCHOOL_MASK_SHADOW, proc.tianjiBounces);
        }
    }

    if (CanUseArtifactSkill(player, 385049))
    {
        uint8& stacks = state.condemnationStacks[victim->GetGUID()];
        stacks = std::min<uint8>(4, stacks + 1);
        if (stacks >= 4)
        {
            if (TryUseCooldown(state, 385049, 1000))
            {
                stacks = 0;
                int32 const percent = GetFeatureCoeff(385049, 520);
                ScheduleArea(state, 385049, victim, GetFeatureDuration(385049, 2000), 0, 1, percent, SPELL_SCHOOL_MASK_ALL, 8.0f, 8, false, true);
                ApplyArtifactHitBounces(player, victim, state, 385049, percent, SPELL_SCHOOL_MASK_ALL, 0);
            }
        }
    }

    static constexpr uint32 ultimateSpells[] = { 385005, 385010, 385015, 385020, 385025, 385030, 385035, 385040, 385045, 385050 };
    for (uint32 ultimateSpell : ultimateSpells)
        StartUltimate(player, victim, state, ultimateSpell);

    auto activeItr = state.ultimateActiveMs.find(385005);
    if (activeItr != state.ultimateActiveMs.end() && activeItr->second > 0)
        DealAreaScaled(player, victim, state, 385005, GetFeatureCoeff(385005, 200), SPELL_SCHOOL_MASK_ARCANE, 8.0f, 8, false, true, true);
}

void HandleIncomingHit(Player* player, Unit* attacker, PlayerFeatureState& state, uint128 const& incomingDamage)
{
    if (!player || !attacker || incomingDamage == 0 || !attacker->IsAlive())
        return;

    uint32 reboundSpellId = 0;
    if (HasSetFeature(player, 384004, &reboundSpellId))
    {
        ProcDecision proc = RollChanceProc(player, state, reboundSpellId, 25.0f, 2000);
        if (proc.triggered)
            DealScaledDamage(player, attacker, state, reboundSpellId, GetFeatureCoeff(reboundSpellId, 220), SPELL_SCHOOL_MASK_NORMAL, true, false, false);
    }

    if (CanUseArtifactSkill(player, 385002))
    {
        ProcDecision proc = RollChanceProc(player, state, 385002, 20.0f, 6000);
        if (proc.triggered)
        {
            DealScaledDamage(player, attacker, state, 385002, GetFeatureCoeff(385002, 180), SPELL_SCHOOL_MASK_NORMAL, false, true);
            ApplyTimedControl(player, attacker, FeatureControl::Interrupt, 0);
            if (proc.tianjiBounces)
                DealExtraBounces(player, attacker, state, 385002, GetFeatureCoeff(385002, 180), SPELL_SCHOOL_MASK_NORMAL, proc.tianjiBounces, 100);
        }
    }

    if (CanUseArtifactSkill(player, 385008))
    {
        ProcDecision proc = RollChanceProc(player, state, 385008, 18.0f, 8000);
        if (proc.triggered)
        {
            DealScaledDamage(player, attacker, state, 385008, GetFeatureCoeff(385008, 300), SPELL_SCHOOL_MASK_HOLY, false, true);
            if (!IsBossOrElite(attacker))
                ApplyTimedControl(player, attacker, FeatureControl::Stun, GetFeatureDuration(385008, 2000));
            if (proc.tianjiBounces)
                DealExtraBounces(player, attacker, state, 385008, GetFeatureCoeff(385008, 300), SPELL_SCHOOL_MASK_HOLY, proc.tianjiBounces, 100);
        }
    }

    if (CanUseArtifactSkill(player, 385034) && state.recentMeleeHitMs && state.recentMeleeAttackerGuid == attacker->GetGUID() &&
        TryUseCooldown(state, 385034, 3000))
    {
        state.recentMeleeHitMs = 0;
        DealScaledDamage(player, attacker, state, 385034, GetFeatureCoeff(385034, 240), SPELL_SCHOOL_MASK_NORMAL, false, true);
    }
}

void HandleOutgoingDamage(Player* player, Unit* victim, uint128 const& damage)
{
    if (!player || !victim || damage == 0 || s_applyingFeatureDamage || !HasAnyFeatureAura(player) || !IsValidFeatureTarget(player, victim))
        return;

    PlayerFeatureState& state = GetState(player);
    state.lastDamage = damage;
    state.lastNonUltimateDamage = damage;

    HandleSetOutgoingHit(player, victim, state);
    HandleArtifactOutgoingHit(player, victim, state);
}

void TriggerKillArea(Player* player, Unit* victim, PlayerFeatureState& state, uint32 spellId, int32 percent,
    SpellSchoolMask school, float radius, uint32 maxTargets, uint32 delayMs = 0)
{
    if (delayMs)
        ScheduleArea(state, spellId, victim, delayMs, 0, 1, percent, school, radius, maxTargets, false, true);
    else
        DealAreaScaled(player, victim, state, spellId, percent, school, radius, maxTargets, false, true);
}

void HandleKillTriggers(Player* player, Unit* victim)
{
    if (!player || !victim || s_applyingFeatureDamage || !HasAnyFeatureAura(player))
        return;

    PlayerFeatureState& state = GetState(player);

    if (CanUseArtifactSkill(player, 385004) && TryUseCooldown(state, 385004, 1000))
        DealChainScaled(player, victim, state, 385004, GetFeatureCoeff(385004, 300), SPELL_SCHOOL_MASK_ARCANE, 3, false, true);

    if (CanUseArtifactSkill(player, 385014) && TryUseCooldown(state, 385014, 1000))
        TriggerKillArea(player, victim, state, 385014, GetFeatureCoeff(385014, 350), SPELL_SCHOOL_MASK_SHADOW, 10.0f, 8);

    if (CanUseArtifactSkill(player, 385029) && TryUseCooldown(state, 385029, 1000))
        TriggerKillArea(player, victim, state, 385029, GetFeatureCoeff(385029, 420), SPELL_SCHOOL_MASK_NATURE, 10.0f, 8, GetFeatureDuration(385029, 1500));

    auto debtItr = state.soulDebtMs.find(victim->GetGUID());
    if (debtItr != state.soulDebtMs.end())
    {
        DealAreaScaled(player, victim, state, 385041, GetFeatureCoeff(385041, 420), SPELL_SCHOOL_MASK_SHADOW, 8.0f, 6, false, true);
        state.soulDebtMs.erase(debtItr);
    }
}

void ReduceTimers(PlayerFeatureState& state, uint32 diff)
{
    auto reduceCooldownMap = [diff](std::map<uint32, uint32>& timers)
    {
        for (auto itr = timers.begin(); itr != timers.end();)
        {
            if (itr->second <= diff)
                itr = timers.erase(itr);
            else
            {
                itr->second -= diff;
                ++itr;
            }
        }
    };

    reduceCooldownMap(state.cooldowns);
    reduceCooldownMap(state.visualCooldowns);

    for (auto itr = state.ultimateActiveMs.begin(); itr != state.ultimateActiveMs.end();)
    {
        if (itr->second <= diff)
            itr = state.ultimateActiveMs.erase(itr);
        else
        {
            itr->second -= diff;
            ++itr;
        }
    }

    auto reduceMap = [diff](std::map<ObjectGuid, uint32>& timers)
    {
        for (auto itr = timers.begin(); itr != timers.end();)
        {
            if (itr->second <= diff)
                itr = timers.erase(itr);
            else
            {
                itr->second -= diff;
                ++itr;
            }
        }
    };

    reduceMap(state.starMarksMs);

    if (state.recentMeleeHitMs)
        state.recentMeleeHitMs = state.recentMeleeHitMs <= diff ? 0 : state.recentMeleeHitMs - diff;
}

void ProcessScheduledEffects(Player* player, PlayerFeatureState& state, uint32 diff)
{
    for (auto itr = state.scheduled.begin(); itr != state.scheduled.end();)
    {
        if (itr->timerMs > diff)
        {
            itr->timerMs -= diff;
            ++itr;
            continue;
        }

        Unit* target = ObjectAccessor::GetUnit(*player, itr->targetGuid);
        if (target && (itr->area ? player->IsInMap(target) : IsValidFeatureTarget(player, target)))
        {
            if (itr->area)
            {
                DealAreaScaled(player, target, state, itr->spellId, itr->percent, itr->school, itr->radius, itr->maxTargets,
                    itr->fromSet, itr->fromArtifact, itr->ultimate, itr->control, itr->controlMs);
            }
            else
            {
                DealScaledDamage(player, target, state, itr->spellId, itr->percent, itr->school, itr->fromSet, itr->fromArtifact, itr->ultimate);
                if (itr->control != FeatureControl::None && !IsBossOrElite(target))
                    ApplyTimedControl(player, target, itr->control, itr->controlMs);
            }
        }

        if (itr->ticks > 1)
        {
            --itr->ticks;
            itr->timerMs = itr->intervalMs ? itr->intervalMs : XIANQI_DEFAULT_TICK_PERIOD_MS;
            ++itr;
        }
        else
        {
            itr = state.scheduled.erase(itr);
        }
    }
}

void ProcessSoulDebts(Player* player, PlayerFeatureState& state, uint32 diff)
{
    for (auto itr = state.soulDebtMs.begin(); itr != state.soulDebtMs.end();)
    {
        if (itr->second > diff)
        {
            itr->second -= diff;
            ++itr;
            continue;
        }

        Unit* target = ObjectAccessor::GetUnit(*player, itr->first);
        if (target && IsValidFeatureTarget(player, target))
            DealAreaScaled(player, target, state, 385041, GetFeatureCoeff(385041, 420), SPELL_SCHOOL_MASK_SHADOW, 8.0f, 6, false, true);

        itr = state.soulDebtMs.erase(itr);
    }
}

void ProcessSetPassiveTimers(Player* player, PlayerFeatureState& state, uint32 diff)
{
    uint32 spiritBladeSpellId = 0;
    if (HasSetFeature(player, 384002, &spiritBladeSpellId))
    {
        if (state.spiritBladeTimerMs <= diff)
        {
            state.spiritBlades = static_cast<uint8>(GetFeatureMaxTargets(spiritBladeSpellId, 3));
            state.spiritBladeTimerMs = GetFeaturePeriod(spiritBladeSpellId, XIANQI_SPIRIT_BLADE_PERIOD_MS);
        }
        else
            state.spiritBladeTimerMs -= diff;
    }
    else
    {
        state.spiritBlades = 0;
        state.spiritBladeTimerMs = GetFeaturePeriod(384002, XIANQI_SPIRIT_BLADE_PERIOD_MS);
    }

    uint32 lockDomainSpellId = 0;
    if (HasSetFeature(player, 384007, &lockDomainSpellId))
    {
        if (state.lockDomainTimerMs <= diff)
        {
            if (Unit* target = GetCurrentEnemyTarget(player))
            {
                PlayFeatureAreaVisual(player, target, state, lockDomainSpellId);
                ScheduleArea(state, lockDomainSpellId, target, GetFeatureDuration(lockDomainSpellId, XIANQI_LOCK_DOMAIN_DELAY_MS),
                    0, 1, GetFeatureCoeff(lockDomainSpellId, 700), SPELL_SCHOOL_MASK_ALL, 12.0f, 8, true, false);
            }
            state.lockDomainTimerMs = GetFeatureCooldown(lockDomainSpellId, XIANQI_LOCK_DOMAIN_COOLDOWN_MS);
        }
        else
            state.lockDomainTimerMs -= diff;
    }
    else
        state.lockDomainTimerMs = GetFeatureCooldown(384007, XIANQI_LOCK_DOMAIN_COOLDOWN_MS);

    uint32 ascendSpellId = 0;
    if (HasSetFeature(player, 384008, &ascendSpellId))
    {
        if (state.ascendRemainingMs)
        {
            state.ascendRemainingMs = state.ascendRemainingMs <= diff ? 0 : state.ascendRemainingMs - diff;
        }
        else if (state.ascendTimerMs <= diff)
        {
            state.ascendRemainingMs = GetFeatureDuration(ascendSpellId, XIANQI_ASCEND_DURATION_MS);
            state.ascendTimerMs = GetFeatureCooldown(ascendSpellId, XIANQI_ASCEND_COOLDOWN_MS);
            PlayFeatureCasterVisual(player, state, ascendSpellId);
        }
        else
            state.ascendTimerMs -= diff;
    }
    else
    {
        state.ascendRemainingMs = 0;
        state.ascendTimerMs = GetFeatureCooldown(384008, XIANQI_ASCEND_COOLDOWN_MS);
    }

    uint32 tianjiSpellId = 0;
    if (HasSetFeature(player, 384012, &tianjiSpellId))
    {
        if (!state.tianjiReady)
        {
            if (state.tianjiTimerMs <= diff)
            {
                state.tianjiReady = true;
                PlayFeatureCasterVisual(player, state, tianjiSpellId);
            }
            else
                state.tianjiTimerMs -= diff;
        }
    }
    else
    {
        state.tianjiReady = false;
        state.tianjiTimerMs = GetFeatureCooldown(384012, XIANQI_TIANJI_COOLDOWN_MS);
    }
}

void ProcessArtifactPeriodicTimers(Player* player, PlayerFeatureState& state)
{
    Unit* target = GetCurrentEnemyTarget(player);
    if (!target)
        return;

    if (CanUseArtifactSkill(player, 385012) && TryUseCooldown(state, 385012, 15000))
        ScheduleArea(state, 385012, target, 0, GetFeaturePeriod(385012, XIANQI_DEFAULT_TICK_PERIOD_MS),
            GetFeatureTicks(385012, 4), GetFeatureCoeff(385012, 120), SPELL_SCHOOL_MASK_SHADOW, 8.0f, 6, false, true, false, FeatureControl::Pull);

    if (CanUseArtifactSkill(player, 385018) && TryUseCooldown(state, 385018, 18000))
        ScheduleArea(state, 385018, target, GetFeatureDuration(385018, 2000), 0, 1,
            GetFeatureCoeff(385018, 380), SPELL_SCHOOL_MASK_ARCANE, 8.0f, 8, false, true);

    if (CanUseArtifactSkill(player, 385023) && TryUseCooldown(state, 385023, 20000))
    {
        uint128 copyDamage = state.lastElementDamage ? ScalePercent(state.lastElementDamage, GetFeatureCoeff(385023, 60)) :
            ScalePercent(GetMainCombatPower(player), GetFeatureCoeff(385023, 60) * 210 / 100);
        DealRawFeatureDamage(player, target, state, 385023, copyDamage, state.lastElementSchool, false, true, false, true, false);
    }

    if (CanUseArtifactSkill(player, 385024) && TryUseCooldown(state, 385024, 20000))
        ScheduleArea(state, 385024, player, 0, GetFeaturePeriod(385024, XIANQI_DEFAULT_TICK_PERIOD_MS),
            GetFeatureTicks(385024, 6), GetFeatureCoeff(385024, 110), SPELL_SCHOOL_MASK_NATURE, 10.0f, 6, false, true);

    if (CanUseArtifactSkill(player, 385027) && TryUseCooldown(state, 385027, 15000))
        DealScaledDamage(player, target, state, 385027, GetFeatureCoeff(385027, 320), SPELL_SCHOOL_MASK_ARCANE, false, true);

    if (CanUseArtifactSkill(player, 385038) && TryUseCooldown(state, 385038, 18000))
        ScheduleArea(state, 385038, target, 0, 0, 1, GetFeatureCoeff(385038, 220), SPELL_SCHOOL_MASK_FROST,
            8.0f, 8, false, true, false, FeatureControl::Root, GetFeatureDuration(385038, 1500));

    if (CanUseArtifactSkill(player, 385044) && TryUseCooldown(state, 385044, 18000))
        ScheduleArea(state, 385044, target, GetFeatureDuration(385044, 2000), 0, 1,
            GetFeatureCoeff(385044, 400), SPELL_SCHOOL_MASK_SHADOW, 10.0f, 8, false, true);

    if (CanUseArtifactSkill(player, 385048) && TryUseCooldown(state, 385048, 18000))
    {
        SpellSchoolMask const school = MakeSchoolMask(SPELL_SCHOOL_MASK_HOLY | SPELL_SCHOOL_MASK_SHADOW);
        int32 const percent = GetFeatureCoeff(385048, 130);
        uint32 const ringPeriod = GetFeaturePeriod(385048, XIANQI_DEFAULT_TICK_PERIOD_MS);
        ScheduleArea(state, 385048, target, 0, 0, 1, percent, school, 5.0f, 10, false, true);
        ScheduleArea(state, 385048, target, ringPeriod / 2, 0, 1, percent, school, 8.0f, 10, false, true);
        ScheduleArea(state, 385048, target, ringPeriod, 0, 1, percent, school, 12.0f, 10, false, true);
    }
}

bool IsOffensivePlayerSpell(Player* player, Spell* spell)
{
    if (!player || !spell)
        return false;

    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (!spellInfo || spellInfo->IsPositive())
        return false;

    Unit* target = spell->m_targets.GetUnitTarget();
    if (target && IsValidFeatureTarget(player, target))
        return true;

    return GetCurrentEnemyTarget(player) != nullptr;
}

void HandlePlayerSpellCast(Player* player, Spell* spell)
{
    uint32 echoSpellId = 0;
    if (!player || !spell || !HasSetFeature(player, 384006, &echoSpellId) || s_applyingFeatureDamage)
        return;

    if (!IsOffensivePlayerSpell(player, spell))
        return;

    Unit* target = spell->m_targets.GetUnitTarget();
    if (!IsValidFeatureTarget(player, target))
        target = GetCurrentEnemyTarget(player);

    if (!target)
        return;

    PlayerFeatureState& state = GetState(player);
    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (spellInfo)
    {
        state.lastDamageSpellId = spellInfo->Id;
        state.lastDamageSchool = spellInfo->GetSchoolMask() ? spellInfo->GetSchoolMask() : SPELL_SCHOOL_MASK_NORMAL;
    }

    if (++state.echoCastCount < 5)
        return;

    if (!TryUseCooldown(state, echoSpellId, 1000))
        return;

    state.echoCastCount = 0;
    uint128 copyDamage = state.lastDamage ? ScalePercent(state.lastDamage, GetFeatureCoeff(echoSpellId, 45)) : ScalePercent(GetMainCombatPower(player), 180);
    DealRawFeatureDamage(player, target, state, echoSpellId, copyDamage, state.lastDamageSchool, true, false, false, true, false);
}
}

class XianqiFeaturePlayerScript : public PlayerScript
{
public:
    XianqiFeaturePlayerScript() : PlayerScript("XianqiFeaturePlayerScript",
    {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_SPELL_CAST,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player)
            return;

        if (!HasAnyFeatureAura(player))
        {
            s_playerStates.erase(player->GetGUID().GetCounter());
            return;
        }

        PlayerFeatureState& state = GetState(player);
        ReduceTimers(state, diff);
        ProcessSetPassiveTimers(player, state, diff);
        ProcessScheduledEffects(player, state, diff);
        ProcessSoulDebts(player, state, diff);
        ProcessArtifactPeriodicTimers(player, state);
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        HandlePlayerSpellCast(player, spell);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            s_playerStates.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        s_playerStates.erase(guid.GetCounter());
    }
};

class XianqiFeatureUnitScript : public UnitScript
{
public:
    XianqiFeatureUnitScript() : UnitScript("XianqiFeatureUnitScript", true,
    {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_ON_UNIT_DEATH,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_ON_AFTER_ROLL_MELEE_OUTCOME_AGAINST
    }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint128& damage) override
    {
        if (!attacker || !victim || damage == 0 || s_applyingFeatureDamage)
            return;

        if (Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            if (player != victim)
                HandleOutgoingDamage(player, victim, damage);

        if (Player* player = victim->ToPlayer())
        {
            if (attacker != player && HasAnyFeatureAura(player))
            {
                PlayerFeatureState& state = GetState(player);
                HandleIncomingHit(player, attacker, state, damage);
            }
        }
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint128& /*damage*/, SpellInfo const* spellInfo) override
    {
        if (!target || !attacker || !spellInfo || s_applyingFeatureDamage)
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player || !CanUseArtifactSkill(player, 385042) || !IsValidFeatureTarget(player, target))
            return;

        PlayerFeatureState& state = GetState(player);
        ProcDecision proc = RollChanceProc(player, state, 385042, 25.0f, 3000);
        if (proc.triggered)
        {
            int32 const percent = GetFeatureCoeff(385042, 180);
            DealScaledDamage(player, target, state, 385042, percent, SPELL_SCHOOL_MASK_FIRE, false, true);
            if (proc.tianjiBounces)
                DealExtraBounces(player, target, state, 385042, percent, SPELL_SCHOOL_MASK_FIRE, proc.tianjiBounces, 100);
        }
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint128& damage) override
    {
        if (!target || !attacker || damage == 0 || s_applyingFeatureDamage)
            return;

        Player* player = target->ToPlayer();
        if (!player || !CanUseArtifactSkill(player, 385034))
            return;

        PlayerFeatureState& state = GetState(player);
        state.recentMeleeAttackerGuid = attacker->GetGUID();
        state.recentMeleeHitMs = 250;
    }

    void OnAfterRollMeleeOutcomeAgainst(Unit const* attacker, Unit const* victim, WeaponAttackType /*attType*/, uint8 outcome) override
    {
        if (!attacker || !victim || s_applyingFeatureDamage)
            return;

        if (outcome != static_cast<uint8>(MELEE_HIT_DODGE) && outcome != static_cast<uint8>(MELEE_HIT_PARRY))
            return;

        Player* player = const_cast<Player*>(victim->ToPlayer());
        Unit* attackerUnit = const_cast<Unit*>(attacker);
        if (!player || !attackerUnit || !CanUseArtifactSkill(player, 385034) || !IsValidFeatureTarget(player, attackerUnit))
            return;

        PlayerFeatureState& state = GetState(player);
        if (TryUseCooldown(state, 385034, 3000))
            DealScaledDamage(player, attackerUnit, state, 385034, GetFeatureCoeff(385034, 240), SPELL_SCHOOL_MASK_NORMAL, false, true);
    }

    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        if (!unit || !killer || s_applyingFeatureDamage)
            return;

        Player* player = killer->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        HandleKillTriggers(player, unit);
    }
};

void AddSC_xianqi_feature_spells()
{
    new XianqiFeaturePlayerScript();
    new XianqiFeatureUnitScript();
}

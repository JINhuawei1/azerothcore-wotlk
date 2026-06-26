/*
 * Challenge Mirage custom Gundrak combat AI.
 */

#include "ChallengeMirage.h"
#include "CreatureScript.h"
#include "EventMap.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"

namespace
{
constexpr char const* SCRIPT_GUNDRAK_VENOM = "npc_challenge_mirage_gundrak_venom";
constexpr char const* SCRIPT_GUNDRAK_COLOSSUS = "npc_challenge_mirage_gundrak_colossus";
constexpr char const* SCRIPT_GUNDRAK_BEAST = "npc_challenge_mirage_gundrak_beast";
constexpr char const* SCRIPT_GUNDRAK_PROPHET = "npc_challenge_mirage_gundrak_prophet";
constexpr char const* SCRIPT_GUNDRAK_ABYSS = "npc_challenge_mirage_gundrak_abyss";
constexpr char const* SCRIPT_GUNDRAK_MINION = "npc_challenge_mirage_gundrak_minion";

constexpr uint32 SUMMON_DESPAWN_MS = 90000;
constexpr uint32 THREAT_PULSE_MS = 5000;
constexpr uint32 HARD_BERSERK_MS = 360000;
constexpr uint8 MAX_ACTIVE_SUMMONS = 8;

enum CustomEntries : uint32
{
    NPC_SERPENT = 800011,
    NPC_CONSTRICTOR = 800012,
    NPC_SPEARMAN = 800013,
    NPC_HUNTER = 800014,
    NPC_FIREWEAVER = 800015,
    NPC_MEDIC = 800016,
    NPC_MOJO = 800017,
    NPC_RHINO = 800018
};

enum GundrakSpells : uint32
{
    SPELL_POISON_NOVA = 55081,
    SPELL_POWERFUL_BITE = 48287,
    SPELL_VENOM_BOLT = 54970,
    SPELL_SNAKE_WRAP = 55126,

    SPELL_MIGHTY_BLOW = 54719,
    SPELL_MOJO_WAVE = 55626,
    SPELL_MOJO_PUDDLE = 55627,
    SPELL_MOJO_VOLLEY = 54849,
    SPELL_SURGE = 54801,

    SPELL_SUMMON_PHANTOM = 55205,
    SPELL_SUMMON_PHANTOM_TRANSFORM = 55097,
    SPELL_DETERMINED_STAB = 55104,
    SPELL_DETERMINED_GORE = 55102,
    SPELL_GROUND_TREMOR = 55142,
    SPELL_QUAKE = 55101,
    SPELL_NUMBING_SHOUT = 55106,
    SPELL_NUMBING_ROAR = 55100,
    SPELL_MOJO_FRENZY = 55163,

    SPELL_ENRAGE = 55285,
    SPELL_IMPALING_CHARGE = 54956,
    SPELL_STAMPEDE = 55218,
    SPELL_STOMP = 55292,
    SPELL_PUNCTURE = 55276,
    SPELL_WHIRLING_SLASH = 55250,
    SPELL_TRANSFORM_TO_RHINO = 55297,
    SPELL_TRANSFORM_TO_TROLL = 55299,

    SPELL_ECK_BERSERK = 55816,
    SPELL_ECK_BITE = 55813,
    SPELL_ECK_SPIT = 55814,
    SPELL_ECK_SPRING = 55815,

    SPELL_RAIN_OF_FIRE = 49518,
    SPELL_SHADOW_VOLLEY = 49528,
    SPELL_POISON_CLOUD = 49548,
    SPELL_FLASH_HEAL = 2061
};

enum BossKind : uint8
{
    BOSS_VENOM,
    BOSS_COLOSSUS,
    BOSS_BEAST,
    BOSS_PROPHET,
    BOSS_ABYSS
};

enum BossEvents : uint32
{
    EVENT_OPENER = 1,
    EVENT_PRIMARY,
    EVENT_SECONDARY,
    EVENT_AREA,
    EVENT_SUMMON,
    EVENT_SPECIAL,
    EVENT_PHASE,
    EVENT_THREAT_PULSE,
    EVENT_BERSERK
};

bool HasSpell(uint32 spellId)
{
    return spellId && sSpellMgr->GetSpellInfo(spellId);
}

Unit* GetRandomTarget(ScriptedAI* ai, float range = 80.0f, bool skipTank = false)
{
    if (!ai)
        return nullptr;

    if (skipTank)
        if (Unit* target = ai->SelectTarget(SelectTargetMethod::Random, 1, range, true))
            return target;

    if (Unit* target = ai->SelectTarget(SelectTargetMethod::Random, 0, range, true))
        return target;

    return ai->me->GetVictim();
}

bool CastIfKnown(Unit* caster, Unit* target, uint32 spellId, bool triggered = false)
{
    if (!caster || !target || !HasSpell(spellId))
        return false;

    caster->CastSpell(target, spellId, triggered);
    return true;
}

bool CastSelfIfKnown(Unit* caster, uint32 spellId, bool triggered = false)
{
    return CastIfKnown(caster, caster, spellId, triggered);
}

void EngageVisiblePlayers(Creature* me, Unit* engager)
{
    if (!me)
        return;

    Map* map = me->GetMap();
    if (!map)
        return;

    if (map->IsDungeon())
    {
        me->SetInCombatWithZone();
        return;
    }

    if (Player* player = engager ? engager->ToPlayer() : nullptr)
    {
        me->SetInCombatWith(player);
        player->SetInCombatWith(me);
        me->AddThreat(player, 1.0f);
    }

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* player = itr->GetSource();
        if (!player || !player->IsAlive() || me->GetDistance(player) > 100.0f)
            continue;

        if (!sChallengeMirageMgr->IsCreatureVisibleForPlayer(me, player))
            continue;

        me->SetInCombatWith(player);
        player->SetInCombatWith(me);
        me->AddThreat(player, 1.0f);
    }
}

Creature* SummonMirageAdd(Creature* me, uint32 entry, uint8 index)
{
    if (!me || !entry)
        return nullptr;

    static constexpr float offsets[8][2] =
    {
        { 4.0f, 0.0f },
        { -4.0f, 0.0f },
        { 0.0f, 4.0f },
        { 0.0f, -4.0f },
        { 3.0f, 3.0f },
        { -3.0f, -3.0f },
        { 3.0f, -3.0f },
        { -3.0f, 3.0f }
    };

    float const* offset = offsets[index % 8];
    Creature* summon = me->SummonCreature(entry, me->GetPositionX() + offset[0], me->GetPositionY() + offset[1],
        me->GetPositionZ(), me->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN_OOC_ALIVE, SUMMON_DESPAWN_MS);

    if (!summon)
        return nullptr;

    sChallengeMirageMgr->SetCreatureLayer(summon, sChallengeMirageMgr->GetCreatureLayer(me));
    EngageVisiblePlayers(summon, me->GetVictim());

    if (Unit* victim = me->GetVictim())
        summon->AI()->AttackStart(victim);

    return summon;
}
}

class npc_challenge_mirage_gundrak_boss : public CreatureScript
{
public:
    npc_challenge_mirage_gundrak_boss(char const* scriptName, BossKind kind) : CreatureScript(scriptName), _kind(kind) { }

    struct npc_challenge_mirage_gundrak_bossAI : public ScriptedAI
    {
        npc_challenge_mirage_gundrak_bossAI(Creature* creature, BossKind kind)
            : ScriptedAI(creature), _summons(creature), _kind(kind)
        {
        }

        void Reset() override
        {
            _events.Reset();
            _summons.DespawnAll();
            _phase75 = false;
            _phase50 = false;
            _phase25 = false;
            _hardBerserk = false;
            _summonIndex = 0;
        }

        void JustEngagedWith(Unit* who) override
        {
            EngageVisiblePlayers(me, who);
            ScheduleCombat();
        }

        void JustDied(Unit*) override
        {
            _summons.DespawnAll();
        }

        void JustSummoned(Creature* summon) override
        {
            if (summon)
                _summons.Summon(summon);
        }

        void SummonedCreatureDespawn(Creature* summon) override
        {
            if (summon)
                _summons.Despawn(summon);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            _events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
            {
                DoMeleeAttackIfReady();
                return;
            }

            while (uint32 eventId = _events.ExecuteEvent())
            {
                if (!ExecuteCommonEvent(eventId))
                    ExecuteBossEvent(eventId);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    break;
            }

            DoMeleeAttackIfReady();
        }

    private:
        void ScheduleCombat()
        {
            _events.Reset();
            _events.ScheduleEvent(EVENT_OPENER, 1000);
            _events.ScheduleEvent(EVENT_PHASE, 1000);
            _events.ScheduleEvent(EVENT_THREAT_PULSE, THREAT_PULSE_MS);
            _events.ScheduleEvent(EVENT_BERSERK, HARD_BERSERK_MS);

            switch (_kind)
            {
                case BOSS_VENOM:
                    _events.ScheduleEvent(EVENT_PRIMARY, 3500);
                    _events.ScheduleEvent(EVENT_SECONDARY, 8500);
                    _events.ScheduleEvent(EVENT_AREA, 13000);
                    _events.ScheduleEvent(EVENT_SUMMON, 18000);
                    _events.ScheduleEvent(EVENT_SPECIAL, 11500);
                    break;
                case BOSS_COLOSSUS:
                    _events.ScheduleEvent(EVENT_PRIMARY, 5000);
                    _events.ScheduleEvent(EVENT_SECONDARY, 9000);
                    _events.ScheduleEvent(EVENT_AREA, 12000);
                    _events.ScheduleEvent(EVENT_SUMMON, 20000);
                    _events.ScheduleEvent(EVENT_SPECIAL, 15000);
                    break;
                case BOSS_BEAST:
                    _events.ScheduleEvent(EVENT_PRIMARY, 4500);
                    _events.ScheduleEvent(EVENT_SECONDARY, 10500);
                    _events.ScheduleEvent(EVENT_AREA, 13500);
                    _events.ScheduleEvent(EVENT_SUMMON, 22000);
                    _events.ScheduleEvent(EVENT_SPECIAL, 17000);
                    break;
                case BOSS_PROPHET:
                    _events.ScheduleEvent(EVENT_PRIMARY, 3000);
                    _events.ScheduleEvent(EVENT_SECONDARY, 9500);
                    _events.ScheduleEvent(EVENT_AREA, 12500);
                    _events.ScheduleEvent(EVENT_SUMMON, 21000);
                    _events.ScheduleEvent(EVENT_SPECIAL, 16000);
                    break;
                case BOSS_ABYSS:
                    _events.ScheduleEvent(EVENT_PRIMARY, 3500);
                    _events.ScheduleEvent(EVENT_SECONDARY, 7500);
                    _events.ScheduleEvent(EVENT_AREA, 12000);
                    _events.ScheduleEvent(EVENT_SUMMON, 23000);
                    _events.ScheduleEvent(EVENT_SPECIAL, 14000);
                    break;
            }
        }

        bool ExecuteCommonEvent(uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_OPENER:
                    ApplyOpener();
                    return true;
                case EVENT_PHASE:
                    CheckHealthPhases();
                    _events.ScheduleEvent(EVENT_PHASE, 1000);
                    return true;
                case EVENT_THREAT_PULSE:
                    ApplyBossPressure();
                    _events.ScheduleEvent(EVENT_THREAT_PULSE, THREAT_PULSE_MS);
                    return true;
                case EVENT_BERSERK:
                    ApplyHardBerserk();
                    return true;
                default:
                    return false;
            }
        }

        void ApplyOpener()
        {
            switch (_kind)
            {
                case BOSS_VENOM:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_VENOM_BOLT);
                    SummonTracked(NPC_SERPENT);
                    break;
                case BOSS_COLOSSUS:
                    CastSelfIfKnown(me, SPELL_MOJO_VOLLEY);
                    SummonTracked(NPC_MOJO);
                    SummonTracked(NPC_MOJO);
                    break;
                case BOSS_BEAST:
                    CastIfKnown(me, me->GetVictim(), SPELL_STAMPEDE);
                    SummonTracked(NPC_RHINO);
                    break;
                case BOSS_PROPHET:
                    CastSelfIfKnown(me, SPELL_SHADOW_VOLLEY);
                    SummonTracked(NPC_FIREWEAVER);
                    break;
                case BOSS_ABYSS:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_ECK_SPIT);
                    CastIfKnown(me, GetRandomTarget(this), SPELL_POISON_CLOUD);
                    break;
            }
        }

        void ApplyBossPressure()
        {
            EngageVisiblePlayers(me, me->GetVictim());

            if (Unit* victim = me->GetVictim())
                me->AddThreat(victim, 250.0f);
        }

        void ApplyHardBerserk()
        {
            if (_hardBerserk)
                return;

            _hardBerserk = true;

            switch (_kind)
            {
                case BOSS_ABYSS:
                    CastSelfIfKnown(me, SPELL_ECK_BERSERK, true);
                    break;
                case BOSS_COLOSSUS:
                case BOSS_PROPHET:
                    CastSelfIfKnown(me, SPELL_MOJO_FRENZY, true);
                    break;
                case BOSS_VENOM:
                case BOSS_BEAST:
                    CastSelfIfKnown(me, SPELL_ENRAGE, true);
                    break;
            }

            _events.ScheduleEvent(EVENT_AREA, 1000);
            _events.ScheduleEvent(EVENT_SPECIAL, 3000);
            _events.ScheduleEvent(EVENT_SUMMON, 5000);
        }

        void CheckHealthPhases()
        {
            if (!_phase75 && me->HealthBelowPct(75))
            {
                _phase75 = true;
                ExecuteHealthPhase(75);
            }

            if (!_phase50 && me->HealthBelowPct(50))
            {
                _phase50 = true;
                ExecuteHealthPhase(50);
            }

            if (!_phase25 && me->HealthBelowPct(25))
            {
                _phase25 = true;
                ExecuteHealthPhase(25);
            }
        }

        void ExecuteHealthPhase(uint8 pct)
        {
            switch (_kind)
            {
                case BOSS_VENOM:
                    CastSelfIfKnown(me, SPELL_POISON_NOVA);
                    SummonTracked(pct <= 50 ? NPC_CONSTRICTOR : NPC_SERPENT);
                    SummonTracked(NPC_SERPENT);
                    if (pct <= 25)
                        CastSelfIfKnown(me, SPELL_ENRAGE, true);
                    break;
                case BOSS_COLOSSUS:
                    CastSelfIfKnown(me, pct <= 50 ? SPELL_MOJO_FRENZY : SPELL_MOJO_VOLLEY, true);
                    SummonTracked(NPC_MOJO);
                    SummonTracked(NPC_MOJO);
                    if (pct <= 25)
                        CastIfKnown(me, GetRandomTarget(this, 80.0f, true), SPELL_SURGE);
                    break;
                case BOSS_BEAST:
                    CastSelfIfKnown(me, pct <= 50 ? SPELL_TRANSFORM_TO_RHINO : SPELL_WHIRLING_SLASH);
                    SummonTracked(pct <= 50 ? NPC_HUNTER : NPC_RHINO);
                    SummonTracked(NPC_SPEARMAN);
                    if (pct <= 25)
                        CastSelfIfKnown(me, SPELL_ENRAGE, true);
                    break;
                case BOSS_PROPHET:
                    CastSelfIfKnown(me, pct <= 50 ? SPELL_MOJO_FRENZY : SPELL_SHADOW_VOLLEY, true);
                    SummonTracked(NPC_FIREWEAVER);
                    SummonTracked(pct <= 50 ? NPC_MEDIC : NPC_MOJO);
                    if (pct <= 25)
                        CastSelfIfKnown(me, SPELL_ENRAGE, true);
                    break;
                case BOSS_ABYSS:
                    CastSelfIfKnown(me, pct <= 25 ? SPELL_ECK_BERSERK : SPELL_SHADOW_VOLLEY, true);
                    SummonTracked(NPC_MOJO);
                    SummonTracked(pct <= 50 ? NPC_CONSTRICTOR : NPC_SERPENT);
                    break;
            }

            _events.ScheduleEvent(EVENT_AREA, 1500);
            _events.ScheduleEvent(EVENT_SPECIAL, 3500);
        }

        void ExecuteBossEvent(uint32 eventId)
        {
            switch (_kind)
            {
                case BOSS_VENOM:
                    ExecuteVenom(eventId);
                    break;
                case BOSS_COLOSSUS:
                    ExecuteColossus(eventId);
                    break;
                case BOSS_BEAST:
                    ExecuteBeast(eventId);
                    break;
                case BOSS_PROPHET:
                    ExecuteProphet(eventId);
                    break;
                case BOSS_ABYSS:
                    ExecuteAbyss(eventId);
                    break;
            }
        }

        void ExecuteVenom(uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_PRIMARY:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_VENOM_BOLT);
                    _events.ScheduleEvent(EVENT_PRIMARY, _hardBerserk ? 3500 : 6000);
                    break;
                case EVENT_SECONDARY:
                    CastIfKnown(me, me->GetVictim(), SPELL_POWERFUL_BITE);
                    _events.ScheduleEvent(EVENT_SECONDARY, _hardBerserk ? 6500 : 9500);
                    break;
                case EVENT_AREA:
                    CastSelfIfKnown(me, SPELL_POISON_NOVA);
                    _events.ScheduleEvent(EVENT_AREA, _hardBerserk ? 9000 : 15000);
                    break;
                case EVENT_SUMMON:
                    SummonTracked((_summonIndex % 2) ? NPC_CONSTRICTOR : NPC_SERPENT);
                    SummonTracked(NPC_SERPENT);
                    _events.ScheduleEvent(EVENT_SUMMON, _hardBerserk ? 16000 : 24000);
                    break;
                case EVENT_SPECIAL:
                    CastIfKnown(me, GetRandomTarget(this, 80.0f, true), SPELL_SNAKE_WRAP);
                    _events.ScheduleEvent(EVENT_SPECIAL, _hardBerserk ? 8000 : 13000);
                    break;
            }
        }

        void ExecuteColossus(uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_PRIMARY:
                    CastIfKnown(me, me->GetVictim(), SPELL_MIGHTY_BLOW);
                    _events.ScheduleEvent(EVENT_PRIMARY, _hardBerserk ? 6000 : 8500);
                    break;
                case EVENT_SECONDARY:
                    CastIfKnown(me, GetRandomTarget(this, 80.0f, true), SPELL_SURGE);
                    _events.ScheduleEvent(EVENT_SECONDARY, _hardBerserk ? 8000 : 12000);
                    break;
                case EVENT_AREA:
                    CastSelfIfKnown(me, SPELL_MOJO_VOLLEY);
                    CastSelfIfKnown(me, SPELL_MOJO_PUDDLE, true);
                    _events.ScheduleEvent(EVENT_AREA, _hardBerserk ? 9000 : 14000);
                    break;
                case EVENT_SUMMON:
                    SummonTracked(NPC_MOJO);
                    SummonTracked(NPC_MOJO);
                    _events.ScheduleEvent(EVENT_SUMMON, _hardBerserk ? 18000 : 26000);
                    break;
                case EVENT_SPECIAL:
                    CastSelfIfKnown(me, SPELL_GROUND_TREMOR);
                    _events.ScheduleEvent(EVENT_SPECIAL, _hardBerserk ? 9000 : 15000);
                    break;
            }
        }

        void ExecuteBeast(uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_PRIMARY:
                    CastIfKnown(me, me->GetVictim(), SPELL_PUNCTURE);
                    _events.ScheduleEvent(EVENT_PRIMARY, _hardBerserk ? 4500 : 7000);
                    break;
                case EVENT_SECONDARY:
                    if (Unit* target = GetRandomTarget(this, 100.0f, true))
                    {
                        me->AddThreat(target, 500.0f);
                        CastIfKnown(me, target, SPELL_IMPALING_CHARGE);
                    }
                    _events.ScheduleEvent(EVENT_SECONDARY, _hardBerserk ? 9000 : 14000);
                    break;
                case EVENT_AREA:
                    CastSelfIfKnown(me, SPELL_STOMP);
                    _events.ScheduleEvent(EVENT_AREA, _hardBerserk ? 8500 : 13000);
                    break;
                case EVENT_SUMMON:
                    SummonTracked((_summonIndex % 3) == 0 ? NPC_RHINO : NPC_HUNTER);
                    SummonTracked(NPC_SPEARMAN);
                    _events.ScheduleEvent(EVENT_SUMMON, _hardBerserk ? 18000 : 26000);
                    break;
                case EVENT_SPECIAL:
                    CastSelfIfKnown(me, (_summonIndex % 2) ? SPELL_STAMPEDE : SPELL_WHIRLING_SLASH);
                    _events.ScheduleEvent(EVENT_SPECIAL, _hardBerserk ? 10000 : 16000);
                    break;
            }
        }

        void ExecuteProphet(uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_PRIMARY:
                    CastIfKnown(me, me->GetVictim(), SPELL_DETERMINED_STAB);
                    _events.ScheduleEvent(EVENT_PRIMARY, _hardBerserk ? 4500 : 6500);
                    break;
                case EVENT_SECONDARY:
                    CastSelfIfKnown(me, _phase50 ? SPELL_NUMBING_ROAR : SPELL_NUMBING_SHOUT);
                    _events.ScheduleEvent(EVENT_SECONDARY, _hardBerserk ? 8000 : 11500);
                    break;
                case EVENT_AREA:
                    CastSelfIfKnown(me, SPELL_SHADOW_VOLLEY);
                    CastIfKnown(me, GetRandomTarget(this), SPELL_RAIN_OF_FIRE);
                    _events.ScheduleEvent(EVENT_AREA, _hardBerserk ? 8500 : 13000);
                    break;
                case EVENT_SUMMON:
                    SummonTracked(NPC_FIREWEAVER);
                    SummonTracked((_summonIndex % 2) ? NPC_MEDIC : NPC_MOJO);
                    _events.ScheduleEvent(EVENT_SUMMON, _hardBerserk ? 18000 : 26000);
                    break;
                case EVENT_SPECIAL:
                    if (_phase50 && me->HealthBelowPct(70))
                        CastSelfIfKnown(me, SPELL_FLASH_HEAL);
                    else
                        CastSelfIfKnown(me, SPELL_SUMMON_PHANTOM_TRANSFORM, true);
                    _events.ScheduleEvent(EVENT_SPECIAL, _hardBerserk ? 11000 : 17000);
                    break;
            }
        }

        void ExecuteAbyss(uint32 eventId)
        {
            switch (eventId)
            {
                case EVENT_PRIMARY:
                    CastIfKnown(me, me->GetVictim(), SPELL_ECK_BITE);
                    _events.ScheduleEvent(EVENT_PRIMARY, _hardBerserk ? 4500 : 6500);
                    break;
                case EVENT_SECONDARY:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_ECK_SPIT);
                    _events.ScheduleEvent(EVENT_SECONDARY, _hardBerserk ? 6000 : 8500);
                    break;
                case EVENT_AREA:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_POISON_CLOUD);
                    CastSelfIfKnown(me, SPELL_SHADOW_VOLLEY);
                    _events.ScheduleEvent(EVENT_AREA, _hardBerserk ? 8500 : 13000);
                    break;
                case EVENT_SUMMON:
                    SummonTracked((_summonIndex % 2) ? NPC_MOJO : NPC_SERPENT);
                    SummonTracked((_summonIndex % 3) ? NPC_CONSTRICTOR : NPC_MOJO);
                    _events.ScheduleEvent(EVENT_SUMMON, _hardBerserk ? 17000 : 24000);
                    break;
                case EVENT_SPECIAL:
                    if (Unit* target = GetRandomTarget(this, 80.0f, true))
                    {
                        me->AddThreat(target, 500.0f);
                        CastIfKnown(me, target, SPELL_ECK_SPRING);
                    }
                    _events.ScheduleEvent(EVENT_SPECIAL, _hardBerserk ? 9000 : 14000);
                    break;
            }
        }

        void SummonTracked(uint32 entry)
        {
            _summons.RemoveNotExisting();
            if (_summons.size() >= MAX_ACTIVE_SUMMONS)
                return;

            SummonMirageAdd(me, entry, _summonIndex++);
        }

        EventMap _events;
        SummonList _summons;
        BossKind _kind;
        bool _phase75 = false;
        bool _phase50 = false;
        bool _phase25 = false;
        bool _hardBerserk = false;
        uint8 _summonIndex = 0;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_challenge_mirage_gundrak_bossAI(creature, _kind);
    }

private:
    BossKind _kind;
};

class npc_challenge_mirage_gundrak_minion : public CreatureScript
{
public:
    npc_challenge_mirage_gundrak_minion() : CreatureScript(SCRIPT_GUNDRAK_MINION) { }

    struct npc_challenge_mirage_gundrak_minionAI : public ScriptedAI
    {
        npc_challenge_mirage_gundrak_minionAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            _timer = 2000;
        }

        void JustEngagedWith(Unit* who) override
        {
            EngageVisiblePlayers(me, who);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (me->HasUnitState(UNIT_STATE_CASTING))
            {
                DoMeleeAttackIfReady();
                return;
            }

            if (_timer <= diff)
            {
                CastEntrySpell();
                _timer = 5000;
            }
            else
            {
                _timer -= diff;
            }

            DoMeleeAttackIfReady();
        }

    private:
        void CastEntrySpell()
        {
            switch (me->GetEntry())
            {
                case NPC_SERPENT:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_VENOM_BOLT);
                    break;
                case NPC_CONSTRICTOR:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_SNAKE_WRAP);
                    break;
                case NPC_SPEARMAN:
                    CastIfKnown(me, me->GetVictim(), SPELL_DETERMINED_STAB);
                    break;
                case NPC_HUNTER:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_IMPALING_CHARGE);
                    break;
                case NPC_FIREWEAVER:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_RAIN_OF_FIRE);
                    break;
                case NPC_MEDIC:
                    CastIfKnown(me, me, SPELL_FLASH_HEAL);
                    break;
                case NPC_MOJO:
                    CastIfKnown(me, GetRandomTarget(this), SPELL_MOJO_WAVE);
                    break;
                case NPC_RHINO:
                    CastSelfIfKnown(me, SPELL_GROUND_TREMOR);
                    break;
            }
        }

        uint32 _timer = 2000;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_challenge_mirage_gundrak_minionAI(creature);
    }
};

void AddSC_ChallengeMirageDungeonAI()
{
    LOG_INFO("server.loading", "-> Challenge Mirage Gundrak dungeon AI loaded");

    new npc_challenge_mirage_gundrak_boss(SCRIPT_GUNDRAK_VENOM, BOSS_VENOM);
    new npc_challenge_mirage_gundrak_boss(SCRIPT_GUNDRAK_COLOSSUS, BOSS_COLOSSUS);
    new npc_challenge_mirage_gundrak_boss(SCRIPT_GUNDRAK_BEAST, BOSS_BEAST);
    new npc_challenge_mirage_gundrak_boss(SCRIPT_GUNDRAK_PROPHET, BOSS_PROPHET);
    new npc_challenge_mirage_gundrak_boss(SCRIPT_GUNDRAK_ABYSS, BOSS_ABYSS);
    new npc_challenge_mirage_gundrak_minion();
}

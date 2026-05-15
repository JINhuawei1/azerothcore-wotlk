/*
 * Challenge Mirage boss combat AI.
 */

#include "CreatureScript.h"
#include "ChallengeMirage.h"
#include "Map.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"

#include <array>

namespace
{
constexpr char const* MIRAGE_GUARDIAN_SCRIPT = "npc_challenge_mirage_guardian";
constexpr char const* MIRAGE_RUNE_GUARDIAN_SCRIPT = "npc_challenge_mirage_rune_guardian";
constexpr char const* MIRAGE_CLEAVER_GUARDIAN_SCRIPT = "npc_challenge_mirage_cleaver_guardian";
constexpr char const* MIRAGE_MAGIC_GUARDIAN_SCRIPT = "npc_challenge_mirage_magic_guardian";
constexpr char const* MIRAGE_TITLE_GUARDIAN_SCRIPT = "npc_challenge_mirage_title_guardian";
constexpr char const* MIRAGE_REBIRTH_GUARDIAN_SCRIPT = "npc_challenge_mirage_rebirth_guardian";
constexpr uint32 MIRAGE_GUARDIAN_SKILL_INTERVAL_MS = 3000;

enum MirageGuardianKind : uint8
{
    MIRAGE_GUARDIAN_NONE = 0,
    MIRAGE_GUARDIAN_RUNE,
    MIRAGE_GUARDIAN_CLEAVER,
    MIRAGE_GUARDIAN_MAGIC,
    MIRAGE_GUARDIAN_TITLE,
    MIRAGE_GUARDIAN_REBIRTH
};

struct MirageGuardianProfile
{
    MirageGuardianKind Kind;
    uint32 BaseEntry;
    std::array<uint32, 5> Spells;
};

constexpr std::array<MirageGuardianProfile, 5> GuardianProfiles =
{{
    { MIRAGE_GUARDIAN_RUNE,    400000, { 1409507, 1409455, 1409515, 1409431, 1409455 } },
    { MIRAGE_GUARDIAN_CLEAVER, 410000, { 1409497, 1409501, 1409485, 1409491, 1409506 } },
    { MIRAGE_GUARDIAN_MAGIC,   420000, { 1409416, 1409515, 1409490, 1409468, 1409512 } },
    { MIRAGE_GUARDIAN_TITLE,   430000, { 1409465, 1409422, 1409422, 1409437, 1409465 } },
    { MIRAGE_GUARDIAN_REBIRTH, 440000, { 1409501, 1409495, 1409464, 1409476, 1409512 } }
}};

MirageGuardianProfile const* GetProfile(uint32 entry)
{
    for (MirageGuardianProfile const& profile : GuardianProfiles)
        if (entry >= profile.BaseEntry + 1 && entry <= profile.BaseEntry + 1000)
            return &profile;

    return nullptr;
}

MirageGuardianProfile const* GetProfile(MirageGuardianKind kind)
{
    for (MirageGuardianProfile const& profile : GuardianProfiles)
        if (profile.Kind == kind)
            return &profile;

    return nullptr;
}

bool HasSpell(uint32 spellId)
{
    return spellId && sSpellMgr->GetSpellInfo(spellId);
}
}

class npc_challenge_mirage_guardian : public CreatureScript
{
public:
    npc_challenge_mirage_guardian(char const* scriptName, MirageGuardianKind kind)
        : CreatureScript(scriptName), _kind(kind)
    {
    }

    struct npc_challenge_mirage_guardianAI : public ScriptedAI
    {
        npc_challenge_mirage_guardianAI(Creature* creature, MirageGuardianProfile const* profile)
            : ScriptedAI(creature), _profile(profile)
        {
        }

        void Reset() override
        {
            _spellTimer = MIRAGE_GUARDIAN_SKILL_INTERVAL_MS;
            _nextSpellIndex = 0;
        }

        void JustEngagedWith(Unit* who) override
        {
            EngageNearbyPlayers(who);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!_profile || !UpdateVictim())
                return;

            if (me->HasUnitState(UNIT_STATE_CASTING))
            {
                DoMeleeAttackIfReady();
                return;
            }

            if (_spellTimer > diff)
            {
                _spellTimer -= diff;
            }
            else
            {
                CastSingleSpell();
                _spellTimer = MIRAGE_GUARDIAN_SKILL_INTERVAL_MS;
            }

            DoMeleeAttackIfReady();
        }

    private:
        Unit* PickRandomTarget()
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 80.0f, true))
                return target;

            return me->GetVictim();
        }

        void CastSingleSpell()
        {
            uint8 spellCount = static_cast<uint8>(_profile->Spells.size());
            uint8 index = _nextSpellIndex % spellCount;
            _nextSpellIndex = static_cast<uint8>((_nextSpellIndex + 1) % spellCount);

            uint32 spellId = _profile->Spells[index];
            if (!HasSpell(spellId))
                return;

            switch (index)
            {
                case 0:
                case 3:
                    if (Unit* target = PickRandomTarget())
                        DoCast(target, spellId);
                    break;
                case 1:
                    if (Unit* target = me->GetVictim())
                        DoCast(target, spellId);
                    break;
                case 2:
                case 4:
                    DoCastAOE(spellId);
                    break;
                default:
                    break;
            }
        }

        void EngageNearbyPlayers(Unit* engager)
        {
            Map* map = me->GetMap();
            if (!map)
                return;

            if (map->IsDungeon())
            {
                me->SetInCombatWithZone();
                return;
            }

            EngagePlayer(engager ? engager->ToPlayer() : nullptr);

            Map::PlayerList const& players = map->GetPlayers();
            for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                if (Player* player = itr->GetSource())
                    if (player->IsAlive() && me->GetDistance(player) <= 80.0f && sChallengeMirageMgr->IsCreatureVisibleForPlayer(me, player))
                        EngagePlayer(player);
        }

        void EngagePlayer(Player* player)
        {
            if (!player || !player->IsAlive() || !sChallengeMirageMgr->IsCreatureVisibleForPlayer(me, player))
                return;

            me->SetInCombatWith(player);
            player->SetInCombatWith(me);
            me->AddThreat(player, 1.0f);
        }

        MirageGuardianProfile const* _profile;
        uint32 _spellTimer = MIRAGE_GUARDIAN_SKILL_INTERVAL_MS;
        uint8 _nextSpellIndex = 0;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        MirageGuardianProfile const* profile = _kind == MIRAGE_GUARDIAN_NONE ? GetProfile(creature->GetEntry()) : GetProfile(_kind);
        if (!profile)
            return nullptr;

        return new npc_challenge_mirage_guardianAI(creature, profile);
    }

private:
    MirageGuardianKind _kind;
};

void AddSC_ChallengeMirageBossAI()
{
    new npc_challenge_mirage_guardian(MIRAGE_GUARDIAN_SCRIPT, MIRAGE_GUARDIAN_NONE);
    new npc_challenge_mirage_guardian(MIRAGE_RUNE_GUARDIAN_SCRIPT, MIRAGE_GUARDIAN_RUNE);
    new npc_challenge_mirage_guardian(MIRAGE_CLEAVER_GUARDIAN_SCRIPT, MIRAGE_GUARDIAN_CLEAVER);
    new npc_challenge_mirage_guardian(MIRAGE_MAGIC_GUARDIAN_SCRIPT, MIRAGE_GUARDIAN_MAGIC);
    new npc_challenge_mirage_guardian(MIRAGE_TITLE_GUARDIAN_SCRIPT, MIRAGE_GUARDIAN_TITLE);
    new npc_challenge_mirage_guardian(MIRAGE_REBIRTH_GUARDIAN_SCRIPT, MIRAGE_GUARDIAN_REBIRTH);
}

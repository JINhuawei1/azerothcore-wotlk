/*
 * 飞升系统 - AscensionSystem.cpp
 * 允许玩家在原有装备槽位基础上额外装备第二套装备
 */

#include "AscensionSystem.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "Logging/Log.h"
#include "World.h"
#include "Mail.h"
#include "DBCStores.h"
#include "SpellAuras.h"
#include <array>
#include <sstream>
#include <chrono>
#include <algorithm>  // 用于 std::all_of, std::min, std::remove
#include <limits>

// 需求模板系统集成：通过模块管理器访问统一的需求接口
#include "ModuleManager.h"

// 包含幻境系统头文件（用于应用鉴定系统的自定义属性）
#if __has_include("HuanJingSystem.h")
    #ifndef MODULE_HUANJING_SYSTEM
        #define MODULE_HUANJING_SYSTEM
    #endif
    #include "HuanJingSystem.h"
#endif

using namespace Acore::ChatCommands;

// 获取需求模块接口（通过模块管理器）
static RequirementInterface* GetRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    if (!mgr)
        return nullptr;
    return mgr->GetRequirementModule();
}

namespace
{
    constexpr uint32 ASCENSION_CHAIN_BOSS_FIRST = 399801;
    constexpr uint32 ASCENSION_CHAIN_BOSS_LAST  = 399818;
    constexpr uint32 ASCENSION_CHAIN_BOSS_COUNT = ASCENSION_CHAIN_BOSS_LAST - ASCENSION_CHAIN_BOSS_FIRST + 1;
    constexpr uint32 ASCENSION_CHAIN_SUMMON_MS  = 30 * IN_MILLISECONDS;
    constexpr uint32 ASCENSION_CHAIN_BOSS_CORPSE_DELAY_SEC = 5;
    constexpr uint64 ASCENSION_BOSS_HEALTH_BASE = 1000000000;
    constexpr uint64 ASCENSION_BOSS_HEALTH_STEP = 100000000;
    constexpr uint64 ASCENSION_CLIENT_VISIBLE_HEALTH_LIMIT = 2147483520ULL;
    constexpr uint32 ASCENSION_TRUE_STRIKE_MIN_MS = 1800;
    constexpr uint32 ASCENSION_TRUE_STRIKE_MAX_MS = 2400;

    int32 ClampAscensionInt64ToInt32(int64 value)
    {
        if (value > std::numeric_limits<int32>::max())
            return std::numeric_limits<int32>::max();
        if (value < std::numeric_limits<int32>::min())
            return std::numeric_limits<int32>::min();
        return static_cast<int32>(value);
    }

    enum AscensionBossSpellId : uint32
    {
        SPELL_ASCENSION_CLEAVE              = 15284,
        SPELL_ASCENSION_MORTAL_STRIKE       = 16856,
        SPELL_ASCENSION_WHIRLWIND           = 13736,
        SPELL_ASCENSION_WAR_STOMP           = 31480,
        SPELL_ASCENSION_ARCANE_EXPLOSION    = 26192,
        SPELL_ASCENSION_FROSTBOLT           = 62601,
        SPELL_ASCENSION_FROST_NOVA          = 62605,
        SPELL_ASCENSION_CHAIN_LIGHTNING     = 33665,
        SPELL_ASCENSION_SHADOW_BOLT         = 27646,
        SPELL_ASCENSION_SHADOW_BOLT_VOLLEY  = 20741,
        SPELL_ASCENSION_FLAME_BREATH        = 43140,
        SPELL_ASCENSION_FEAR                = 26580,
        SPELL_ASCENSION_SHOCK_BLAST         = 38509,
        SPELL_ASCENSION_FORKED_LIGHTNING    = 38145,
        SPELL_ASCENSION_NEEDLE_SPINE        = 39992,
        SPELL_ASCENSION_TIDAL_BURST         = 39878,
        SPELL_ASCENSION_RAIN_OF_FIRE        = 31340
    };

    enum class AscensionBossSpellCastType : uint8
    {
        Victim,
        Area,
        Random
    };

    struct AscensionBossSpellAction
    {
        uint32 SpellId;
        AscensionBossSpellCastType CastType;
        uint32 FirstCastMinMs;
        uint32 FirstCastMaxMs;
        uint32 RepeatMinMs;
        uint32 RepeatMaxMs;
        float RandomTargetDistance;
    };

    struct AscensionBossSpellProfile
    {
        std::array<AscensionBossSpellAction, 3> Actions;
    };

    struct AscensionBossCombatProfile
    {
        std::array<AscensionBossSpellAction, 5> Actions;
        std::array<uint8, 3> ComboActionOrder;
        uint32 ComboCooldownMinMs;
        uint32 ComboCooldownMaxMs;
    };

    #if 0
    constexpr std::array<AscensionBossSpellProfile, ASCENSION_CHAIN_BOSS_COUNT> ASCENSION_BOSS_SPELL_PROFILES =
    {{
        {{ SPELL_ASCENSION_SHOCK_BLAST,         AscensionBossSpellCastType::Area,   6000,  9000, 12000, 16000,  0.0f },
           { SPELL_ASCENSION_FEAR,               AscensionBossSpellCastType::Area,  12000, 16000, 21000, 26000,  0.0f },
           { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,   4000,  6000, 10000, 13000,  0.0f }}},
        {{ SPELL_ASCENSION_FROSTBOLT,           AscensionBossSpellCastType::Random,  5000,  8000,  8000, 11000, 45.0f },
           { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,   10000, 13000, 17000, 21000,  0.0f },
           { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random,  8000, 12000, 14000, 18000, 45.0f }}},
        {{ SPELL_ASCENSION_CLEAVE,              AscensionBossSpellCastType::Victim,  4000,  6000,  7000, 10000,  0.0f },
           { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,   10000, 14000, 18000, 22000,  0.0f },
           { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,    8000, 12000, 16000, 20000,  0.0f }}},
        {{ SPELL_ASCENSION_SHADOW_BOLT,         AscensionBossSpellCastType::Random,  4000,  7000,  8000, 11000, 45.0f },
           { SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,AscensionBossSpellCastType::Area,   10000, 14000, 17000, 21000,  0.0f },
           { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random, 14000, 18000, 21000, 25000, 40.0f }}},
        {{ SPELL_ASCENSION_MORTAL_STRIKE,       AscensionBossSpellCastType::Victim,  5000,  8000,  9000, 12000,  0.0f },
           { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  8000,  0.0f },
           { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   10000, 13000, 18000, 22000,  0.0f }}},
        {{ SPELL_ASCENSION_CHAIN_LIGHTNING,     AscensionBossSpellCastType::Random,  5000,  8000,  9000, 12000, 45.0f },
           { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,   10000, 14000, 18000, 22000,  0.0f },
           { SPELL_ASCENSION_SHOCK_BLAST,       AscensionBossSpellCastType::Area,   14000, 18000, 22000, 26000,  0.0f }}},
        {{ SPELL_ASCENSION_MORTAL_STRIKE,       AscensionBossSpellCastType::Victim,  4000,  7000,  8000, 11000,  0.0f },
           { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,    9000, 12000, 16000, 20000,  0.0f },
           { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   15000, 18000, 24000, 28000,  0.0f }}},
        {{ SPELL_ASCENSION_FLAME_BREATH,        AscensionBossSpellCastType::Victim,  5000,  8000,  9000, 12000,  0.0f },
           { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   10000, 13000, 16000, 20000,  0.0f },
           { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  9000,  0.0f }}},
        {{ SPELL_ASCENSION_FROSTBOLT,           AscensionBossSpellCastType::Random,  4000,  7000,  7000, 10000, 45.0f },
           { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,    9000, 12000, 15000, 18000,  0.0f },
           { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,   13000, 16000, 20000, 24000,  0.0f }}},
        {{ SPELL_ASCENSION_CLEAVE,              AscensionBossSpellCastType::Victim,  4000,  6000,  7000,  9000,  0.0f },
           { SPELL_ASCENSION_MORTAL_STRIKE,     AscensionBossSpellCastType::Victim,  8000, 11000, 12000, 15000,  0.0f },
           { SPELL_ASCENSION_SHOCK_BLAST,       AscensionBossSpellCastType::Area,   12000, 16000, 20000, 24000,  0.0f }}},
        {{ SPELL_ASCENSION_SHADOW_BOLT,         AscensionBossSpellCastType::Random,  5000,  8000,  8000, 11000, 45.0f },
           { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   12000, 16000, 21000, 25000,  0.0f },
           { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random, 15000, 18000, 24000, 28000, 40.0f }}},
        {{ SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,  AscensionBossSpellCastType::Area,    7000, 10000, 12000, 15000,  0.0f },
           { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random, 10000, 13000, 16000, 19000, 45.0f },
           { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,   14000, 18000, 22000, 26000,  0.0f }}},
        {{ SPELL_ASCENSION_NEEDLE_SPINE,        AscensionBossSpellCastType::Random,  5000,  8000, 10000, 13000, 45.0f },
           { SPELL_ASCENSION_TIDAL_BURST,       AscensionBossSpellCastType::Area,   12000, 15000, 20000, 24000,  0.0f },
           { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,    9000, 12000, 18000, 22000,  0.0f }}},
        {{ SPELL_ASCENSION_RAIN_OF_FIRE,        AscensionBossSpellCastType::Random,  6000,  9000, 11000, 14000, 40.0f },
           { SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,AscensionBossSpellCastType::Area,   11000, 15000, 18000, 22000,  0.0f },
           { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   16000, 20000, 24000, 28000,  0.0f }}},
        {{ SPELL_ASCENSION_ARCANE_EXPLOSION,    AscensionBossSpellCastType::Area,    5000,  8000, 10000, 13000,  0.0f },
           { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random,  8000, 11000, 14000, 17000, 45.0f },
           { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,   13000, 16000, 19000, 23000,  0.0f }}},
        {{ SPELL_ASCENSION_MORTAL_STRIKE,       AscensionBossSpellCastType::Victim,  4000,  6000,  8000, 10000,  0.0f },
           { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,    9000, 12000, 16000, 20000,  0.0f },
           { SPELL_ASCENSION_FLAME_BREATH,      AscensionBossSpellCastType::Victim, 14000, 17000, 22000, 26000,  0.0f }}},
        {{ SPELL_ASCENSION_CLEAVE,              AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  8000,  0.0f },
           { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,    9000, 12000, 16000, 19000,  0.0f },
           { SPELL_ASCENSION_TIDAL_BURST,       AscensionBossSpellCastType::Area,   14000, 18000, 23000, 27000,  0.0f }}},
        {{ SPELL_ASCENSION_NEEDLE_SPINE,        AscensionBossSpellCastType::Random,  4000,  7000,  8000, 11000, 45.0f },
           { SPELL_ASCENSION_SHADOW_BOLT,       AscensionBossSpellCastType::Random,  9000, 12000, 14000, 18000, 45.0f },
           { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random, 13000, 16000, 20000, 24000, 45.0f }}}
    }};

    #endif

    AscensionBossSpellProfile MakeAscensionBossSpellProfile(
        AscensionBossSpellAction const& action1,
        AscensionBossSpellAction const& action2,
        AscensionBossSpellAction const& action3)
    {
        AscensionBossSpellProfile profile = {};
        profile.Actions[0] = action1;
        profile.Actions[1] = action2;
        profile.Actions[2] = action3;
        return profile;
    }

    std::array<AscensionBossSpellProfile, ASCENSION_CHAIN_BOSS_COUNT> const ASCENSION_BOSS_SPELL_PROFILES =
    {{
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_SHOCK_BLAST,        AscensionBossSpellCastType::Area,   6000,  9000, 12000, 16000,  0.0f },
            { SPELL_ASCENSION_FEAR,               AscensionBossSpellCastType::Area,  12000, 16000, 21000, 26000,  0.0f },
            { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,   4000,  6000, 10000, 13000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_FROSTBOLT,         AscensionBossSpellCastType::Random,  5000,  8000,  8000, 11000, 45.0f },
            { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,   10000, 13000, 17000, 21000,  0.0f },
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random,  8000, 12000, 14000, 18000, 45.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  4000,  6000,  7000, 10000,  0.0f },
            { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,   10000, 14000, 18000, 22000,  0.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,    8000, 12000, 16000, 20000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_SHADOW_BOLT,       AscensionBossSpellCastType::Random,  4000,  7000,  8000, 11000, 45.0f },
            { SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,AscensionBossSpellCastType::Area,   10000, 14000, 17000, 21000,  0.0f },
            { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random, 14000, 18000, 21000, 25000, 40.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_MORTAL_STRIKE,     AscensionBossSpellCastType::Victim,  5000,  8000,  9000, 12000,  0.0f },
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  8000,  0.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   10000, 13000, 18000, 22000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random,  5000,  8000,  9000, 12000, 45.0f },
            { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,   10000, 14000, 18000, 22000,  0.0f },
            { SPELL_ASCENSION_SHOCK_BLAST,       AscensionBossSpellCastType::Area,   14000, 18000, 22000, 26000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_MORTAL_STRIKE,     AscensionBossSpellCastType::Victim,  4000,  7000,  8000, 11000,  0.0f },
            { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,    9000, 12000, 16000, 20000,  0.0f },
            { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   15000, 18000, 24000, 28000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_FLAME_BREATH,      AscensionBossSpellCastType::Victim,  5000,  8000,  9000, 12000,  0.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   10000, 13000, 16000, 20000,  0.0f },
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  9000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_FROSTBOLT,         AscensionBossSpellCastType::Random,  4000,  7000,  7000, 10000, 45.0f },
            { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,    9000, 12000, 15000, 18000,  0.0f },
            { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,   13000, 16000, 20000, 24000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  4000,  6000,  7000,  9000,  0.0f },
            { SPELL_ASCENSION_MORTAL_STRIKE,     AscensionBossSpellCastType::Victim,  8000, 11000, 12000, 15000,  0.0f },
            { SPELL_ASCENSION_SHOCK_BLAST,       AscensionBossSpellCastType::Area,   12000, 16000, 20000, 24000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_SHADOW_BOLT,       AscensionBossSpellCastType::Random,  5000,  8000,  8000, 11000, 45.0f },
            { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   12000, 16000, 21000, 25000,  0.0f },
            { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random, 15000, 18000, 24000, 28000, 40.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,AscensionBossSpellCastType::Area,    7000, 10000, 12000, 15000,  0.0f },
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random, 10000, 13000, 16000, 19000, 45.0f },
            { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,   14000, 18000, 22000, 26000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_NEEDLE_SPINE,      AscensionBossSpellCastType::Random,  5000,  8000, 10000, 13000, 45.0f },
            { SPELL_ASCENSION_TIDAL_BURST,       AscensionBossSpellCastType::Area,   12000, 15000, 20000, 24000,  0.0f },
            { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,    9000, 12000, 18000, 22000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random,  6000,  9000, 11000, 14000, 40.0f },
            { SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,AscensionBossSpellCastType::Area,   11000, 15000, 18000, 22000,  0.0f },
            { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   16000, 20000, 24000, 28000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,    5000,  8000, 10000, 13000,  0.0f },
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random,  8000, 11000, 14000, 17000, 45.0f },
            { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,   13000, 16000, 19000, 23000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_MORTAL_STRIKE,     AscensionBossSpellCastType::Victim,  4000,  6000,  8000, 10000,  0.0f },
            { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,    9000, 12000, 16000, 20000,  0.0f },
            { SPELL_ASCENSION_FLAME_BREATH,      AscensionBossSpellCastType::Victim, 14000, 17000, 22000, 26000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  8000,  0.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,    9000, 12000, 16000, 19000,  0.0f },
            { SPELL_ASCENSION_TIDAL_BURST,       AscensionBossSpellCastType::Area,   14000, 18000, 23000, 27000,  0.0f }),
        MakeAscensionBossSpellProfile(
            { SPELL_ASCENSION_NEEDLE_SPINE,      AscensionBossSpellCastType::Random,  4000,  7000,  8000, 11000, 45.0f },
            { SPELL_ASCENSION_SHADOW_BOLT,       AscensionBossSpellCastType::Random,  9000, 12000, 14000, 18000, 45.0f },
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random, 13000, 16000, 20000, 24000, 45.0f })
    }};

    uint32 GetAscensionBossSequenceIndex(uint32 entry)
    {
        if (!entry || entry < ASCENSION_CHAIN_BOSS_FIRST || entry > ASCENSION_CHAIN_BOSS_LAST)
            return 0;

        return entry - ASCENSION_CHAIN_BOSS_FIRST;
    }

    uint32 ToAscensionClientHealth(uint64 value)
    {
        return value > ASCENSION_CLIENT_VISIBLE_HEALTH_LIMIT ? static_cast<uint32>(ASCENSION_CLIENT_VISIBLE_HEALTH_LIMIT) : static_cast<uint32>(value);
    }

    uint64 GetAscensionBossMaxHealth(uint32 entry)
    {
        return ASCENSION_BOSS_HEALTH_BASE + GetAscensionBossSequenceIndex(entry) * ASCENSION_BOSS_HEALTH_STEP;
    }

    uint64 GetAscensionBossTrueStrikeDamage(uint32 entry)
    {
        uint64 maxHealth = GetAscensionBossMaxHealth(entry);
        return std::max<uint64>(25000u, maxHealth / 25000u);
    }

    AscensionBossCombatProfile MakeAscensionBossCombatProfile(
        AscensionBossSpellAction const& action1,
        AscensionBossSpellAction const& action2,
        AscensionBossSpellAction const& action3,
        AscensionBossSpellAction const& action4,
        AscensionBossSpellAction const& action5,
        uint8 comboAction1,
        uint8 comboAction2,
        uint8 comboAction3,
        uint32 comboCooldownMinMs,
        uint32 comboCooldownMaxMs)
    {
        AscensionBossCombatProfile profile = {};
        profile.Actions[0] = action1;
        profile.Actions[1] = action2;
        profile.Actions[2] = action3;
        profile.Actions[3] = action4;
        profile.Actions[4] = action5;
        profile.ComboActionOrder[0] = comboAction1;
        profile.ComboActionOrder[1] = comboAction2;
        profile.ComboActionOrder[2] = comboAction3;
        profile.ComboCooldownMinMs = comboCooldownMinMs;
        profile.ComboCooldownMaxMs = comboCooldownMaxMs;
        return profile;
    }

    constexpr uint32 ASCENSION_COMBAT_ARCHETYPE_COUNT = 6;

    std::array<AscensionBossCombatProfile, ASCENSION_COMBAT_ARCHETYPE_COUNT> const ASCENSION_BOSS_COMBAT_PROFILES =
    {{
        MakeAscensionBossCombatProfile(
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim,  3000,  5000,  6000,  8000,  0.0f },
            { SPELL_ASCENSION_MORTAL_STRIKE,     AscensionBossSpellCastType::Victim,  7000,  9000, 10000, 13000,  0.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   10000, 13000, 17000, 21000,  0.0f },
            { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,   12000, 15000, 18000, 22000,  0.0f },
            { SPELL_ASCENSION_SHOCK_BLAST,       AscensionBossSpellCastType::Area,   15000, 18000, 22000, 26000,  0.0f },
            2, 3, 1, 14000, 18000),
        MakeAscensionBossCombatProfile(
            { SPELL_ASCENSION_FROSTBOLT,         AscensionBossSpellCastType::Random,  4000,  6000,  7000,  9000, 45.0f },
            { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,    8000, 11000, 15000, 19000,  0.0f },
            { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,   10000, 13000, 14000, 17000,  0.0f },
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random, 12000, 15000, 16000, 19000, 45.0f },
            { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   16000, 20000, 24000, 28000,  0.0f },
            1, 0, 2, 15000, 19000),
        MakeAscensionBossCombatProfile(
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random,  4000,  7000,  8000, 11000, 45.0f },
            { SPELL_ASCENSION_FORKED_LIGHTNING,  AscensionBossSpellCastType::Area,    7000, 10000, 13000, 16000,  0.0f },
            { SPELL_ASCENSION_SHOCK_BLAST,       AscensionBossSpellCastType::Area,   10000, 13000, 18000, 22000,  0.0f },
            { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random, 13000, 16000, 18000, 22000, 40.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   15000, 18000, 20000, 24000,  0.0f },
            4, 0, 1, 13000, 17000),
        MakeAscensionBossCombatProfile(
            { SPELL_ASCENSION_SHADOW_BOLT,       AscensionBossSpellCastType::Random,  3000,  5000,  7000,  9000, 45.0f },
            { SPELL_ASCENSION_SHADOW_BOLT_VOLLEY,AscensionBossSpellCastType::Area,    8000, 11000, 14000, 17000,  0.0f },
            { SPELL_ASCENSION_FLAME_BREATH,      AscensionBossSpellCastType::Victim, 10000, 13000, 14000, 17000,  0.0f },
            { SPELL_ASCENSION_FEAR,              AscensionBossSpellCastType::Area,   14000, 18000, 22000, 26000,  0.0f },
            { SPELL_ASCENSION_RAIN_OF_FIRE,      AscensionBossSpellCastType::Random, 16000, 20000, 23000, 27000, 40.0f },
            3, 1, 2, 15000, 19000),
        MakeAscensionBossCombatProfile(
            { SPELL_ASCENSION_NEEDLE_SPINE,      AscensionBossSpellCastType::Random,  4000,  6000,  7000,  9000, 45.0f },
            { SPELL_ASCENSION_TIDAL_BURST,       AscensionBossSpellCastType::Area,    9000, 12000, 15000, 18000,  0.0f },
            { SPELL_ASCENSION_FROST_NOVA,        AscensionBossSpellCastType::Area,   11000, 14000, 18000, 21000,  0.0f },
            { SPELL_ASCENSION_SHADOW_BOLT,       AscensionBossSpellCastType::Random, 13000, 16000, 15000, 19000, 45.0f },
            { SPELL_ASCENSION_CLEAVE,            AscensionBossSpellCastType::Victim, 16000, 19000,  9000, 12000,  0.0f },
            0, 2, 1, 14000, 18000),
        MakeAscensionBossCombatProfile(
            { SPELL_ASCENSION_ARCANE_EXPLOSION,  AscensionBossSpellCastType::Area,    5000,  7000, 10000, 13000,  0.0f },
            { SPELL_ASCENSION_FLAME_BREATH,      AscensionBossSpellCastType::Victim,  8000, 11000, 12000, 16000,  0.0f },
            { SPELL_ASCENSION_CHAIN_LIGHTNING,   AscensionBossSpellCastType::Random, 10000, 13000, 14000, 18000, 45.0f },
            { SPELL_ASCENSION_WHIRLWIND,         AscensionBossSpellCastType::Area,   13000, 16000, 18000, 22000,  0.0f },
            { SPELL_ASCENSION_WAR_STOMP,         AscensionBossSpellCastType::Area,   15000, 19000, 21000, 25000,  0.0f },
            4, 3, 1, 13000, 17000)
    }};

    bool IsAscensionChainBossEntry(uint32 entry)
    {
        return entry >= ASCENSION_CHAIN_BOSS_FIRST && entry <= ASCENSION_CHAIN_BOSS_LAST;
    }

    uint32 GetNextAscensionChainBossEntry(uint32 entry)
    {
        if (!IsAscensionChainBossEntry(entry) || entry >= ASCENSION_CHAIN_BOSS_LAST)
            return 0;

        return entry + 1;
    }

    std::string GetAscensionBossDisplayName(uint32 entry)
    {
        if (CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(entry))
            if (!creatureTemplate->Name.empty())
                return creatureTemplate->Name;

        return "更强大的飞升BOSS";
    }

    AscensionBossSpellProfile const* GetAscensionBossSpellProfile(uint32 entry)
    {
        if (!IsAscensionChainBossEntry(entry))
            return nullptr;

        return &ASCENSION_BOSS_SPELL_PROFILES[entry - ASCENSION_CHAIN_BOSS_FIRST];
    }

    AscensionBossCombatProfile const* GetAscensionBossCombatProfile(uint32 entry)
    {
        if (!IsAscensionChainBossEntry(entry))
            return nullptr;

        return &ASCENSION_BOSS_COMBAT_PROFILES[(entry - ASCENSION_CHAIN_BOSS_FIRST) % ASCENSION_COMBAT_ARCHETYPE_COUNT];
    }

    bool AutoStoreAscensionBossLoot(Creature* creature, Player* fallbackPlayer)
    {
        if (!creature || !fallbackPlayer || creature->loot.items.empty())
            return false;

        Player* rewardPlayer = creature->GetLootRecipient();
        if (!rewardPlayer)
            rewardPlayer = fallbackPlayer;

        bool storedAny = false;
        for (uint8 lootSlot = 0; lootSlot < creature->loot.items.size(); ++lootSlot)
        {
            LootItem& lootItem = creature->loot.items[lootSlot];
            if (lootItem.is_looted || lootItem.count == 0)
                continue;

            ItemPosCountVec dest;
            InventoryResult msg = rewardPlayer->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, lootItem.itemid, lootItem.count);
            if (!lootItem.AllowedForPlayer(rewardPlayer, creature->loot.sourceWorldObjectGUID))
                msg = EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;

            if (msg != EQUIP_ERR_OK)
                return storedAny;

            AllowedLooterSet looters = lootItem.GetAllowedLooters();
            Item* newItem = rewardPlayer->StoreNewItem(dest, lootItem.itemid, true, lootItem.randomPropertyId, looters);
            if (!newItem)
                return storedAny;

            rewardPlayer->SendNewItem(newItem, uint32(lootItem.count), false, false, true);

            lootItem.count = 0;
            lootItem.is_looted = true;
            if (creature->loot.unlootedCount > 0)
                --creature->loot.unlootedCount;

            storedAny = true;
        }

        if (creature->loot.isLooted())
        {
            creature->loot.clear();
            creature->SetLootRecipient(nullptr);
        }

        return storedAny;
    }

    ItemSetEffect* FindAscensionItemSetEffect(Player* player, uint32 setId)
    {
        if (!player || setId == 0)
            return nullptr;

        for (ItemSetEffect* effect : player->ItemSetEff)
            if (effect && effect->setid == setId)
                return effect;

        return nullptr;
    }

    ItemSetEffect* FindOrCreateAscensionItemSetEffect(Player* player, uint32 setId)
    {
        if (!player || setId == 0)
            return nullptr;

        if (ItemSetEffect* effect = FindAscensionItemSetEffect(player, setId))
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

    void MakeAscensionAuraPermanent(Player* player, uint32 spellId)
    {
        if (!player || spellId == 0)
            return;

        if (Aura* aura = player->GetAura(spellId))
        {
            aura->SetMaxDuration(-1);
            aura->SetDuration(-1);
        }
    }

    void MakeAscensionItemSetSpellPermanent(Player* player, uint32 spellId)
    {
        MakeAscensionAuraPermanent(player, spellId);

        if (std::vector<int32> const* linkedSpells = sSpellMgr->GetSpellLinked(spellId + SPELL_LINK_AURA))
        {
            for (int32 linkedSpellId : *linkedSpells)
                if (linkedSpellId > 0)
                    MakeAscensionAuraPermanent(player, uint32(linkedSpellId));
        }
    }

    void CastAscensionEquipSpell(Player* player, Item* item, SpellInfo const* spellInfo)
    {
        (void)item;

        if (!player || !spellInfo)
            return;

        player->CastSpell(player, spellInfo, true);
        MakeAscensionItemSetSpellPermanent(player, spellInfo->Id);
    }

    bool HasAscensionEquipSpell(Player* player, uint32 spellId)
    {
        if (!player || spellId == 0)
            return false;

        PlayerAscensionStatus* status = sAscensionManager->GetPlayerStatus(player->GetGUID().GetCounter());
        if (!status)
            return false;

        for (auto const& slotPair : status->slots)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(slotPair.second.itemId);
            if (!proto)
                continue;

            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (proto->Spells[i].SpellId == int32(spellId) && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
                    return true;
            }
        }

        return false;
    }

    bool ShouldKeepAscensionTriggeredAuraPermanent(Player* player, uint32 spellId)
    {
        switch (spellId)
        {
            case 64413: // 远古王者的庇护，由 64411 远古王者的祝福触发
                return HasAscensionEquipSpell(player, 64411);
            case 33649: // 诺森德神群力量，由 60066 装备效果触发
                return HasAscensionEquipSpell(player, 60066);
            case 89028: // 深渊冲锋毁灭爆发，由深渊装备触发
            {
                if (!player)
                    return false;

                PlayerAscensionStatus* status = sAscensionManager->GetPlayerStatus(player->GetGUID().GetCounter());
                return status && !status->slots.empty();
            }
            default:
                return false;
        }
    }
}

//=============================================================================
// AscensionConfig 实现
//=============================================================================

AscensionConfig* AscensionConfig::instance()
{
    static AscensionConfig instance;
    return &instance;
}

bool AscensionConfig::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("飞升系统.启用", true);
    _debugMode = sConfigMgr->GetOption<bool>("飞升系统.调试模式", false);
    _statMultiplier = sConfigMgr->GetOption<float>("飞升系统.属性倍率", 1.0f);
    _requiredLevel = sConfigMgr->GetOption<uint32>("飞升系统.需要等级", 1);
    _showNotification = sConfigMgr->GetOption<bool>("飞升系统.显示提示", true);
    _autoUnlock = sConfigMgr->GetOption<bool>("飞升系统.自动解锁", false);

    if (_debugMode)
    {
        LOG_INFO("server.loading", "飞升系统: 模块已{}", _enabled ? "启用" : "禁用");
        LOG_INFO("server.loading", "飞升系统: 调试模式已{}", _debugMode ? "启用" : "禁用");
        LOG_INFO("server.loading", "飞升系统: 属性倍率 = {}", _statMultiplier);
        LOG_INFO("server.loading", "飞升系统: 需要等级 = {}", _requiredLevel);
    }

    return true;
}

//=============================================================================
// AscensionManager 实现
//=============================================================================

AscensionManager* AscensionManager::instance()
{
    static AscensionManager instance;
    return &instance;
}

bool AscensionManager::Initialize()
{
    if (!sAscensionConfig->IsEnabled())
        return false;

    LoadSlotControls();
    LoadRestrictedItems();
    return true;
}

void AscensionManager::LoadSlotControls()
{
    _slotControls.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `槽位`, `槽位名称`, `启用`, `解锁需求`, `属性倍率` FROM `_飞升系统_控制` ORDER BY `槽位`");

    if (!result)
    {
        // 创建默认槽位配置
        const char* defaultNames[] = {"头部", "颈部", "肩部", "衬衣", "胸甲", "腰带", "腿部", "脚部",
                                       "手腕", "手套", "戒指1", "戒指2", "饰品1", "饰品2", "披风", "主手", "副手", "远程"};
        for (uint8 i = 0; i < ASCENSION_SLOT_COUNT; ++i)
        {
            AscensionSlotControl ctrl;
            ctrl.slot = i;
            ctrl.slotName = defaultNames[i];
            ctrl.enabled = true;
            ctrl.unlockRequirement = 0;
            ctrl.statMultiplier = 1.0f;
            _slotControls[i] = ctrl;
        }
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        AscensionSlotControl ctrl;
        ctrl.slot = fields[0].Get<uint8>();
        ctrl.slotName = fields[1].Get<std::string>();
        ctrl.enabled = fields[2].Get<bool>();
        ctrl.unlockRequirement = fields[3].Get<uint32>();
        ctrl.statMultiplier = fields[4].Get<float>();

        _slotControls[ctrl.slot] = ctrl;
        count++;
    } while (result->NextRow());
}

void AscensionManager::LoadRestrictedItems()
{
    _restrictedItemIds.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `物品模板ID` FROM `_深渊装备模板` WHERE `是否启用` = 1");

    if (!result)
        return;

    do
    {
        _restrictedItemIds.insert(result->Fetch()[0].Get<uint32>());
    } while (result->NextRow());

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 已加载 {} 个只能穿戴到飞升槽的深渊装备", _restrictedItemIds.size());
    }
}

void AscensionManager::LoadPlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 【修复】清除旧数据前先释放物品内存，防止内存泄漏
    ClearPlayerData(playerGuid);

    PlayerAscensionStatus status;
    status.playerGuid = playerGuid;

    // 从 _飞升系统_数据 表加载已解锁槽位
    // 【新方案】装备数据现在从 character_inventory 表加载，不再从这里解析
    QueryResult result = CharacterDatabase.Query(
        "SELECT `已解锁槽位` FROM `_飞升系统_数据` WHERE `玩家GUID` = {}", playerGuid);

    if (result)
    {
        Field* fields = result->Fetch();

        // 解析已解锁槽位（逗号分隔的字符串）
        std::string unlockedStr = fields[0].Get<std::string>();
        if (!unlockedStr.empty())
        {
            std::stringstream ss(unlockedStr);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                if (!token.empty())
                {
                    try {
                        uint8 slot = static_cast<uint8>(std::stoi(token));
                        if (slot < ASCENSION_SLOT_COUNT)
                        {
                            status.unlockedSlots.insert(slot);
                        }
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR("module", "飞升系统: 解析槽位失败: {}", e.what());
                    }
                }
            }
        }
    }

    // 如果启用自动解锁，解锁所有槽位
    if (sAscensionConfig->IsAutoUnlock())
    {
        for (uint8 i = 0; i < ASCENSION_SLOT_COUNT; ++i)
        {
            status.unlockedSlots.insert(i);
        }
    }

    _playerStatus[playerGuid] = status;

    // 【新方案】从 character_inventory 表加载物品实例（bag=200）
    LoadAscensionItems(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 数据加载完成，已解锁 {} 个槽位，已装备 {} 件",
            player->GetName(), status.unlockedSlots.size(), _playerStatus[playerGuid].slots.size());
    }
}

void AscensionManager::SavePlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 构建已解锁槽位字符串
    std::ostringstream unlockStr;
    bool first = true;
    for (uint8 s : status->unlockedSlots)
    {
        if (!first) unlockStr << ",";
        unlockStr << (int)s;
        first = false;
    }

    // 构建装备数据字符串
    std::ostringstream equipStr;
    first = true;
    for (const auto& pair : status->slots)
    {
        if (!first) equipStr << ",";
        equipStr << (int)pair.first << ":" << pair.second.itemId << ":" << pair.second.itemGuid;
        first = false;
    }

    // 保存到合并表
    CharacterDatabase.Execute(
        "REPLACE INTO `_飞升系统_数据` (`玩家GUID`, `已解锁槽位`, `装备数据`) VALUES ({}, '{}', '{}')",
        playerGuid, unlockStr.str(), equipStr.str());

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 数据已保存", player->GetName());
    }
}

void AscensionManager::ClearPlayerData(uint32 playerGuid)
{
    // 清理内存缓存，释放物品实例（不删除数据库数据）
    auto it = _playerStatus.find(playerGuid);
    if (it != _playerStatus.end())
    {
        // 释放所有物品实例的内存
        for (auto& pair : it->second.slots)
        {
            if (pair.second.itemPtr)
            {
                delete pair.second.itemPtr;
                pair.second.itemPtr = nullptr;
            }
        }
        _playerStatus.erase(it);
    }
}

void AscensionManager::DeletePlayerData(uint32 playerGuid)
{
    // 清理内存并删除数据库数据（仅在角色删除时调用）
    // 先释放物品内存（如果玩家在线/已加载）
    auto it = _playerStatus.find(playerGuid);
    if (it != _playerStatus.end())
    {
        for (auto& pair : it->second.slots)
        {
            if (pair.second.itemPtr)
            {
                delete pair.second.itemPtr;
                pair.second.itemPtr = nullptr;
            }
        }
        _playerStatus.erase(it);
    }

    // 【修复】根据 character_inventory 查询并删除对应的 item_instance 及关联表
    // 这样即使玩家数据未加载（离线角色），也能正确清理
    QueryResult result = CharacterDatabase.Query(
        "SELECT item FROM character_inventory WHERE guid = {} AND bag = {}",
        playerGuid, ASCENSION_VIRTUAL_BAG);

    if (result)
    {
        do
        {
            uint32 itemGuid = result->Fetch()[0].Get<uint32>();
            // 【修复】删除物品关联的所有数据表
            CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
            CharacterDatabase.Execute("DELETE FROM item_text WHERE guid = {}", itemGuid);
            // 如果有其他关联表也需要清理（如物品邮件附件等），可在此添加
        } while (result->NextRow());
    }

    // 删除 character_inventory 表中的飞升物品记录
    CharacterDatabase.Execute(
        "DELETE FROM character_inventory WHERE guid = {} AND bag = {}",
        playerGuid, ASCENSION_VIRTUAL_BAG);

    // 删除飞升系统数据
    CharacterDatabase.Execute("DELETE FROM `_飞升系统_数据` WHERE `玩家GUID` = {}", playerGuid);

    LOG_INFO("module", "飞升系统: 删除玩家 {} 的所有飞升数据", playerGuid);
}

void AscensionManager::SaveAscensionItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;



    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto& pair : status->slots)
    {
        Item* item = pair.second.itemPtr;
        if (item)
        {
            // 【关键】不要设置 SetOwnerGUID，保持为空以防止物品被添加到更新队列
            // 我们直接在 SQL 中使用 player->GetGUID().GetCounter() 作为 owner_guid
            // item->SetOwnerGUID(player->GetGUID());  // 已移除，防止被添加到更新队列

            // 【关键】直接使用 REPLACE 语句保存物品，不调用 SaveToDB
            // 因为 SaveToDB 在 ITEM_REMOVED 状态下会删除物品
            uint8 index = 0;
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_INSTANCE);
            stmt->SetData(  index, item->GetEntry());
            stmt->SetData(++index, player->GetGUID().GetCounter());
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
        else
        {
            LOG_ERROR("module", "飞升系统: 槽位 {} 物品指针为空，无法保存", pair.first);
        }
    }

    CharacterDatabase.CommitTransaction(trans);
}

void AscensionManager::LoadAscensionItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 【新方案】从 character_inventory 表加载 bag=200 的飞升系统物品
    // 这样物品由官方系统管理，不会被当作孤立物品删除
    QueryResult invResult = CharacterDatabase.Query(
        "SELECT ci.slot, ci.item, ii.itemEntry, ii.creatorGuid, ii.giftCreatorGuid, ii.count, "
        "ii.duration, ii.charges, ii.flags, ii.enchantments, ii.randomPropertyId, ii.durability, "
        "ii.playedTime, ii.text "
        "FROM character_inventory ci "
        "JOIN item_instance ii ON ci.item = ii.guid "
        "WHERE ci.guid = {} AND ci.bag = {}", playerGuid, ASCENSION_VIRTUAL_BAG);

    if (!invResult)
        return;

    // 清空旧的槽位数据，从数据库重新加载
    status->slots.clear();

    do
    {
        Field* fields = invResult->Fetch();
        uint8 slot = fields[0].Get<uint8>();
        uint32 itemGuid = fields[1].Get<uint32>();
        uint32 itemEntry = fields[2].Get<uint32>();

        if (slot >= ASCENSION_SLOT_COUNT)
        {
            LOG_ERROR("module", "飞升系统: 无效槽位 {} 物品GUID={}", slot, itemGuid);
            continue;
        }

        if (status->slots.find(slot) != status->slots.end())
        {
            LOG_WARN("module",
                "飞升系统: 玩家 {} LoadAscensionItems 检测到重复槽位 slot={} 旧物品GUID={} 新物品GUID={}",
                player->GetName(),
                slot,
                status->slots[slot].itemGuid,
                itemGuid);
        }

        // 【修复】先校验物品模板是否存在，避免空指针崩溃
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
        if (!proto)
        {
            LOG_ERROR("module", "飞升系统: 物品模板不存在 itemEntry={} itemGuid={} 槽位={}，跳过加载",
                itemEntry, itemGuid, slot);
            // 清理数据库中的无效记录
            CharacterDatabase.Execute(
                "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                playerGuid, ASCENSION_VIRTUAL_BAG, slot);
            continue;
        }

        // 创建物品对象
        Item* item = NewItemOrBag(proto);
        if (!item)
        {
            LOG_ERROR("module", "飞升系统: 无法创建物品实例 itemEntry={}", itemEntry);
            // 【修复】清理数据库中的无效记录
            CharacterDatabase.Execute(
                "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                playerGuid, ASCENSION_VIRTUAL_BAG, slot);
            CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
            continue;
        }

        // 构建 LoadFromDB 需要的字段数组（跳过前3个字段：slot, item, itemEntry）
        // LoadFromDB 期望的字段顺序：creatorGuid, giftCreatorGuid, count, duration, charges,
        //                           flags, enchantments, randomPropertyId, durability, playedTime, text
        if (!item->LoadFromDB(itemGuid, player->GetGUID(), fields + 3, itemEntry))
        {
            LOG_ERROR("module", "飞升系统: 从数据库加载物品失败 GUID={}", itemGuid);
            delete item;
            // 【修复】LoadFromDB 失败时清理数据库中的记录，避免孤儿数据
            CharacterDatabase.Execute(
                "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
                playerGuid, ASCENSION_VIRTUAL_BAG, slot);
            CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
            continue;
        }

        // 【关键】清空 OwnerGUID 并设置状态为 ITEM_UNCHANGED
        // 防止物品被添加到核心的更新队列，飞升系统自己管理物品的保存
        item->SetOwnerGUID(ObjectGuid::Empty);
        item->FSetState(ITEM_UNCHANGED);

        // 保存到槽位数据
        AscensionSlotData slotData;
        slotData.itemId = itemEntry;
        slotData.itemGuid = itemGuid;
        slotData.itemPtr = item;
        status->slots[slot] = slotData;
    } while (invResult->NextRow());
}

void AscensionManager::ValidateEquippedItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    std::vector<uint8> slotsToRemove;
    bool needSave = false;

    for (auto& pair : status->slots)
    {
        uint8 slot = pair.first;
        AscensionSlotData& slotData = pair.second;

        // 检查物品指针是否有效
        if (!slotData.itemPtr)
        {
            // 物品指针无效，可能是加载失败
            slotsToRemove.push_back(slot);
            needSave = true;

            LOG_WARN("module", "飞升系统: 玩家 {} 槽位 {} 的物品指针无效，将移除记录",
                player->GetName(), slot);
            continue;
        }

        // 验证物品实例是否还存在于数据库
        QueryResult result = CharacterDatabase.Query(
            "SELECT guid FROM item_instance WHERE guid = {}", slotData.itemGuid);

        if (!result)
        {
            // 物品已被删除
            slotsToRemove.push_back(slot);
            needSave = true;

            // 释放物品内存
            delete slotData.itemPtr;
            slotData.itemPtr = nullptr;

            LOG_WARN("module", "飞升系统: 玩家 {} 槽位 {} 的物品(GUID:{}) 已被删除，将移除记录",
                player->GetName(), slot, slotData.itemGuid);
        }
    }

    // 【修复】恢复清理逻辑 - 移除无效的装备记录并同步清理数据库
    for (uint8 slot : slotsToRemove)
    {
        auto slotDataIt = status->slots.find(slot);
        if (slotDataIt != status->slots.end())
        {
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(slotDataIt->second.itemId))
                RemoveAscensionItemSet(player, proto->ItemSet, 1);
        }

        // 【关键修复】移除该槽位已应用的属性，防止属性叠加bug
        auto statIt = status->slotStats.find(slot);
        if (statIt != status->slotStats.end())
        {
            for (const AppliedStatEffect& effect : statIt->second)
            {
                RemoveStatEffect(player, effect.statType, effect.statValue);
            }
            status->slotStats.erase(statIt);
            LOG_INFO("module", "飞升系统: 已移除玩家 {} 槽位 {} 的属性效果", player->GetName(), slot);
        }

        // 【关键修复】移除该槽位关联的法术
        auto spellIt = status->slotSpells.find(slot);
        if (spellIt != status->slotSpells.end())
        {
            for (uint32 spellId : spellIt->second)
            {
                // 检查其他槽位是否也有相同法术
                bool spellFromOtherSlot = false;
                for (const auto& otherSlot : status->slotSpells)
                {
                    if (otherSlot.first != slot)
                    {
                        for (uint32 otherId : otherSlot.second)
                        {
                            if (otherId == spellId)
                            {
                                spellFromOtherSlot = true;
                                break;
                            }
                        }
                    }
                    if (spellFromOtherSlot) break;
                }
                if (!spellFromOtherSlot)
                {
                    player->RemoveAurasDueToSpell(spellId);
                }
            }
            status->slotSpells.erase(spellIt);
            LOG_INFO("module", "飞升系统: 已移除玩家 {} 槽位 {} 的法术效果", player->GetName(), slot);
        }

        // 从 character_inventory 表删除该槽位的记录
        CharacterDatabase.Execute(
            "DELETE FROM character_inventory WHERE guid = {} AND bag = {} AND slot = {}",
            playerGuid, ASCENSION_VIRTUAL_BAG, slot);

        // 从内存中移除
        status->slots.erase(slot);

        LOG_INFO("module", "飞升系统: 已清理玩家 {} 槽位 {} 的无效装备记录", player->GetName(), slot);
    }

    // 【关键修复】清理完成后更新玩家属性
    if (!slotsToRemove.empty())
    {
        UpdatePlayerStats(player);
    }

    // 如果有变动，保存数据
    if (needSave)
        SavePlayerData(player);
}

bool AscensionManager::IsSlotUnlocked(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    // 检查槽位是否启用
    auto ctrlIt = _slotControls.find(slot);
    if (ctrlIt == _slotControls.end() || !ctrlIt->second.enabled)
        return false;

    // 自动解锁模式
    if (sAscensionConfig->IsAutoUnlock())
        return true;

    // 检查玩家是否已解锁此槽位（从数据库记录判断）
    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return false;

    return status->unlockedSlots.find(slot) != status->unlockedSlots.end();
}

bool AscensionManager::UnlockSlot(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    if (IsSlotUnlocked(player, slot))
    {
        ChatHandler(player->GetSession()).SendSysMessage("该槽位已经解锁。");
        // 即使槽位已解锁，也要发送数据到客户端，确保UI状态同步
        SendAscensionDataToClient(player);
        return false;
    }

    // 检查解锁需求
    if (!CheckSlotUnlockRequirement(player, slot))
    {
        ChatHandler(player->GetSession()).SendSysMessage("不满足解锁条件。");
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
    {
        LoadPlayerData(player);
        status = GetPlayerStatus(playerGuid);
    }

    status->unlockedSlots.insert(slot);

    // 保存到合并表（使用SavePlayerData统一保存）
    SavePlayerData(player);

    std::string slotName = GetSlotName(slot);
    ChatHandler(player->GetSession()).PSendSysMessage("成功解锁飞升槽位: {}", slotName.c_str());

    // 发送更新数据到客户端
    SendAscensionDataToClient(player);

    return true;
}

bool AscensionManager::CheckSlotUnlockRequirement(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    auto ctrlIt = _slotControls.find(slot);
    if (ctrlIt == _slotControls.end())
        return false;

    uint32 requirementId = ctrlIt->second.unlockRequirement;
    if (requirementId == 0)
        return true; // 无需求

    // 通过模块管理器获取需求模块接口
    RequirementInterface* reqModule = GetRequirementModule();
    if (!reqModule)
    {
        LOG_ERROR("module", "飞升系统: 需求模板系统未初始化，无法检查解锁需求");
        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[飞升系统]|r 需求模板系统未加载，无法解锁槽位");
        return false;
    }

    // 先静默检查是否满足需求
    bool meetsRequirements = reqModule->CheckRequirements(player, requirementId, false);
    if (!meetsRequirements)
    {
        // 不满足条件，显示需求详情
        ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[飞升系统]|r 不满足解锁条件，请查看需求详情：");
        reqModule->CheckRequirements(player, requirementId, true);
        return false;
    }

    // 满足条件，消耗需求资源（扣除物品、金币等）
    if (!reqModule->ConsumeRequirements(player, requirementId))
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[飞升系统]|r 消耗需求资源失败！");
        return false;
    }

    return true;
}

bool AscensionManager::EquipItem(Player* player, uint8 slot, uint32 itemId, uint32 itemGuid)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    // 检查等级要求
    if (player->GetLevel() < sAscensionConfig->GetRequiredLevel())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("需要达到 {} 级才能使用飞升系统。", sAscensionConfig->GetRequiredLevel());
        return false;
    }

    // 检查槽位是否解锁
    if (!IsSlotUnlocked(player, slot))
    {
        ChatHandler(player->GetSession()).SendSysMessage("该槽位尚未解锁。");
        return false;
    }

    // 检查物品是否可以装备到该槽位
    if (!CanEquipItemInSlot(player, slot, itemId))
    {
        ChatHandler(player->GetSession()).SendSysMessage("该物品无法装备到此槽位。");
        return false;
    }

    // 检查物品是否在背包中
    Item* item = FindItemInBags(player, itemGuid);
    if (!item || item->GetEntry() != itemId)
    {
        ChatHandler(player->GetSession()).SendSysMessage("背包中未找到该物品。");
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
    {
        LoadPlayerData(player);
        status = GetPlayerStatus(playerGuid);
    }

    // 检查该槽位是否已有装备
    if (status->slots.find(slot) != status->slots.end())
    {
        // 先卸下旧装备
        UnequipItem(player, slot);
    }

    // 检查该物品是否已在其他槽位
    for (const auto& pair : status->slots)
    {
        if (pair.second.itemGuid == itemGuid)
        {
            ChatHandler(player->GetSession()).SendSysMessage("该物品已装备在其他飞升槽位。");
            return false;
        }
    }

    // 记录物品背包位置（用于从背包移除）
    uint8 bagSlot = item->GetBagSlot();
    uint8 itemSlot = item->GetSlot();

    // 装备物品 - 从背包移除但保留物品实例
    AscensionSlotData slotData;
    slotData.itemId = itemId;
    slotData.itemGuid = itemGuid;
    slotData.itemPtr = item;
    status->slots[slot] = slotData;

    // 【关键】先从更新队列中移除物品，防止 _SaveInventory 将其标记为 ITEM_REMOVED 并删除
    // MoveItemFromInventory 内部会调用 RemoveFromUpdateQueueOf，但我们需要确保状态正确
    item->RemoveFromUpdateQueueOf(player);

    // 从背包移除物品（但不删除物品实例）
    player->MoveItemFromInventory(bagSlot, itemSlot, true);

    // 【新方案】将物品存储到 character_inventory 表，使用 bag=200 作为飞升系统的虚拟背包
    // 这样核心会正确管理物品，不会被当作孤立物品删除
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // 先删除旧的 character_inventory 记录（MoveItemFromInventory 已经删除了，但为了安全再删一次）
    item->DeleteFromInventoryDB(trans);

    // 插入新的 character_inventory 记录，使用 bag=200 标识飞升系统物品
    // 核心的 _LoadInventory 会识别 bag=200 并跳过，让飞升系统自己处理
    CharacterDatabasePreparedStatement* invStmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
    invStmt->SetData(0, playerGuid);                    // characterGuid
    invStmt->SetData(1, ASCENSION_VIRTUAL_BAG);         // bag = 200 (飞升系统虚拟背包)
    invStmt->SetData(2, slot);                          // slot (飞升槽位)
    invStmt->SetData(3, itemGuid);                      // item guid
    trans->Append(invStmt);

    // 保存物品实例到数据库（使用 REPLACE）
    {
        uint8 index = 0;
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_INSTANCE);
        stmt->SetData(  index, item->GetEntry());
        stmt->SetData(++index, player->GetGUID().GetCounter());
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

    LOG_INFO("module", "飞升系统: 装备物品 槽位={} 物品ID={} GUID={} OwnerGUID={}",
        slot, itemId, itemGuid, player->GetGUID().GetCounter());

    // 应用属性与套装贡献
    ApplyItemEffect(player, itemId, slot, true);
    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId))
        ApplyAscensionItemSet(player, proto->ItemSet, 1);

    // 保存数据
    SavePlayerData(player);

    if (sAscensionConfig->ShowNotification())
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        std::string itemName = proto ? proto->Name1 : "未知物品";
        std::string slotName = GetSlotName(slot);
        ChatHandler(player->GetSession()).PSendSysMessage("飞升装备: {} 已装备到 {}", itemName.c_str(), slotName.c_str());
    }

    // 发送数据到客户端
    SendAscensionDataToClient(player);

    return true;
}

bool AscensionManager::UnequipItem(Player* player, uint8 slot)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return false;

    auto it = status->slots.find(slot);
    if (it == status->slots.end())
    {
        ChatHandler(player->GetSession()).SendSysMessage("该槽位没有装备。");
        return false;
    }

    uint32 itemId = it->second.itemId;
    uint32 itemGuid = it->second.itemGuid;
    Item* item = it->second.itemPtr;
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);

    if (ItemTemplate const* equippedProto = sObjectMgr->GetItemTemplate(itemId))
        RemoveAscensionItemSet(player, equippedProto->ItemSet, 1);

    // 【修复】使用 slotStats 记录的值精确移除该槽位的属性，防止属性漂移
    auto statIt = status->slotStats.find(slot);
    if (statIt != status->slotStats.end())
    {
        for (const AppliedStatEffect& effect : statIt->second)
        {
            RemoveStatEffect(player, effect.statType, effect.statValue);
        }
        status->slotStats.erase(statIt);
    }

    // 【修复】移除该槽位关联的法术
    auto spellIt = status->slotSpells.find(slot);
    if (spellIt != status->slotSpells.end())
    {
        for (uint32 spellId : spellIt->second)
        {
            // 检查其他槽位是否也有相同法术
            bool spellFromOtherSlot = false;
            for (const auto& otherSlot : status->slotSpells)
            {
                if (otherSlot.first != slot)
                {
                    for (uint32 otherId : otherSlot.second)
                    {
                        if (otherId == spellId)
                        {
                            spellFromOtherSlot = true;
                            break;
                        }
                    }
                }
                if (spellFromOtherSlot) break;
            }
            if (!spellFromOtherSlot)
            {
                player->RemoveAurasDueToSpell(spellId);
            }
        }
        status->slotSpells.erase(spellIt);
    }

#ifdef MODULE_HUANJING_SYSTEM
    if (item && sHuanJingSystem)
        sHuanJingSystem->RemoveHuanJingEnhancement(player, item);
#endif

    // 更新玩家属性
    UpdatePlayerStats(player);

    // 将物品放回背包
    if (item)
    {
        // 【关键】恢复物品的 OwnerGUID，因为物品要放回背包由核心管理
        item->SetOwnerGUID(player->GetGUID());

        // 查找空闲背包位置
        ItemPosCountVec dest;
        InventoryResult result = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (result == EQUIP_ERR_OK)
        {
            // 存入背包，并使用核心库存保存流程同步 character_inventory 位置
            player->MoveItemToInventory(dest, item, true, true);

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
            delStmt->SetData(0, ASCENSION_VIRTUAL_BAG);
            delStmt->SetData(1, slot);
            delStmt->SetData(2, playerGuid);
            trans->Append(delStmt);
            player->SaveInventoryAndGoldToDB(trans);
            CharacterDatabase.CommitTransaction(trans);
        }
        else
        {
            // 背包已满，发送邮件
            MailDraft draft("飞升系统", "您的背包已满，飞升装备已通过邮件返还。");
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
            delStmt->SetData(0, ASCENSION_VIRTUAL_BAG);
            delStmt->SetData(1, slot);
            delStmt->SetData(2, playerGuid);
            trans->Append(delStmt);
            draft.AddItem(item);
            draft.SendMailTo(trans, MailReceiver(player, playerGuid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
            CharacterDatabase.CommitTransaction(trans);

            ChatHandler(player->GetSession()).SendSysMessage("背包已满，物品已通过邮件返还。");

            LOG_INFO("module", "飞升系统: 背包已满，物品 {} (GUID:{}) 通过邮件返还", itemId, itemGuid);
        }
    }
    else
    {
        // 物品指针无效，也需要清理数据库中的物品实例
        CharacterDatabase.Execute("DELETE FROM item_instance WHERE guid = {}", itemGuid);
        ChatHandler(player->GetSession()).SendSysMessage("物品数据异常，已清除飞升装备记录。");

        LOG_ERROR("module", "飞升系统: 卸下装备时物品指针无效 itemId={} itemGuid={}", itemId, itemGuid);
    }

    // 移除装备记录
    status->slots.erase(it);

    // 保存数据
    SavePlayerData(player);

    if (sAscensionConfig->ShowNotification())
    {
        std::string itemName = proto ? proto->Name1 : "未知物品";
        std::string slotName = GetSlotName(slot);
        ChatHandler(player->GetSession()).PSendSysMessage("飞升装备: {} 已从 {} 卸下", itemName.c_str(), slotName.c_str());
    }

    // 发送数据到客户端
    SendAscensionDataToClient(player);

    return true;
}

void AscensionManager::UnequipAllItems(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 移除所有属性
    RemoveAllEffects(player);

    // 将所有物品放回背包
    std::vector<Item*> itemsToMail;
    CharacterDatabaseTransaction inventoryTrans = CharacterDatabase.BeginTransaction();

    for (auto& pair : status->slots)
    {
        CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
        delStmt->SetData(0, ASCENSION_VIRTUAL_BAG);
        delStmt->SetData(1, pair.first);
        delStmt->SetData(2, playerGuid);
        inventoryTrans->Append(delStmt);

        Item* item = pair.second.itemPtr;
        if (!item)
            continue;

        // 【关键】恢复物品的 OwnerGUID，因为物品要放回背包由核心管理
        item->SetOwnerGUID(player->GetGUID());

        // 查找空闲背包位置
        ItemPosCountVec dest;
        InventoryResult result = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (result == EQUIP_ERR_OK)
        {
            player->MoveItemToInventory(dest, item, true, true);
        }
        else
        {
            // 背包已满，收集待邮寄物品
            itemsToMail.push_back(item);
        }
    }

    player->SaveInventoryAndGoldToDB(inventoryTrans);
    CharacterDatabase.CommitTransaction(inventoryTrans);

    // 【修复】邮寄无法放入背包的物品 - 分批发送，每封邮件最多12件物品
    // 注意：MAX_MAIL_ITEMS 已在 Mail.h 中定义为 12
    if (!itemsToMail.empty())
    {
        size_t totalItems = itemsToMail.size();
        size_t mailCount = 0;

        for (size_t i = 0; i < totalItems; i += MAX_MAIL_ITEMS)
        {
            MailDraft draft("飞升系统", "您的背包已满，飞升装备已通过邮件返还。");
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

            size_t endIndex = std::min(i + static_cast<size_t>(MAX_MAIL_ITEMS), totalItems);
            for (size_t j = i; j < endIndex; ++j)
            {
                draft.AddItem(itemsToMail[j]);
            }

            draft.SendMailTo(trans, MailReceiver(player, playerGuid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED, 0);
            CharacterDatabase.CommitTransaction(trans);
            ++mailCount;
        }

        ChatHandler(player->GetSession()).PSendSysMessage("背包已满，{} 件物品已通过 {} 封邮件返还。", totalItems, mailCount);
    }

    // 清空装备
    status->slots.clear();

    // 保存数据
    SavePlayerData(player);

    if (sAscensionConfig->ShowNotification())
    {
        ChatHandler(player->GetSession()).SendSysMessage("已卸下所有飞升装备。");
    }

    // 发送数据到客户端
    SendAscensionDataToClient(player);
}

void AscensionManager::ApplyAllEffects(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    uint32 appliedCount = 0;

    bool restoreCanModifyStats = player->CanModifyStats();
    if (restoreCanModifyStats)
        player->SetCanModifyStats(false);

    for (const auto& pair : status->slots)
    {
        ApplyItemEffect(player, pair.second.itemId, pair.first, true, false);
        ++appliedCount;
    }

    ApplyAscensionItemSets(player);

    if (restoreCanModifyStats)
        player->SetCanModifyStats(true);

    if (appliedCount > 0 || !status->slotStats.empty() || !status->slotSpells.empty())
        UpdatePlayerStats(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 应用了 {} 件飞升装备的属性", player->GetName(), status->slots.size());
    }
}

void AscensionManager::RemoveAllEffects(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    RemoveAscensionItemSets(player);

    // 【修复】移除所有已应用的法术 - 遍历按槽位记录的法术
    for (const auto& slotPair : status->slotSpells)
    {
        for (uint32 spellId : slotPair.second)
        {
            player->RemoveAurasDueToSpell(spellId);
        }
    }
    status->slotSpells.clear();

    // 【修复】使用 slotStats 记录的值来精确回滚属性，防止倍率变化导致的属性漂移
    for (const auto& slotPair : status->slotStats)
    {
        for (const AppliedStatEffect& effect : slotPair.second)
        {
            // 根据记录的属性类型和值进行移除
            RemoveStatEffect(player, effect.statType, effect.statValue);
        }
    }
    status->slotStats.clear();

#ifdef MODULE_HUANJING_SYSTEM
    for (const auto& slotPair : status->slots)
    {
        if (slotPair.second.itemPtr && sHuanJingSystem)
            sHuanJingSystem->RemoveHuanJingEnhancement(player, slotPair.second.itemPtr);
    }
#endif

    UpdatePlayerStats(player);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 移除了所有飞升装备属性", player->GetName());
    }
}

void AscensionManager::RefreshEffects(Player* player)
{
    if (!player)
        return;

    RemoveAllEffects(player);
    ApplyAllEffects(player);
}

void AscensionManager::ApplyItemEffect(Player* player, uint32 itemId, uint8 slot, bool apply, bool updateStats)
{
    if (!player)
        return;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return;

    // 【修复】检查槽位是否启用，禁用槽位不应用效果
    auto ctrlIt = _slotControls.find(slot);
    if (ctrlIt != _slotControls.end() && !ctrlIt->second.enabled)
    {
        if (sAscensionConfig->IsDebugMode())
        {
            LOG_INFO("module", "飞升系统: 槽位 {} 已禁用，不应用属性", slot);
        }
        return;
    }

    // 获取槽位数据和物品指针（用于应用鉴定系统的自定义属性）
    Item* item = nullptr;
    auto slotIt = status->slots.find(slot);
    if (slotIt != status->slots.end())
    {
        item = slotIt->second.itemPtr;
    }

    // 【修复】检查物品是否破损，破损物品不应用属性
    if (apply && item && item->IsBroken())
    {
        if (sAscensionConfig->IsDebugMode())
        {
            LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 的物品已破损，不应用属性",
                player->GetName(), slot);
        }
        return;
    }

    // 获取槽位倍率
    float slotMultiplier = 1.0f;
    if (ctrlIt != _slotControls.end())
    {
        slotMultiplier = ctrlIt->second.statMultiplier;
    }

    // 全局倍率
    float globalMultiplier = sAscensionConfig->GetStatMultiplier();
    float totalMultiplier = slotMultiplier * globalMultiplier;

    // 应用物品属性 - 使用正确的属性类型映射
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (i >= proto->StatsCount)
            break;

        if (proto->ItemStat[i].ItemStatValue != 0)
        {
            int64 val = static_cast<int64>(static_cast<long double>(proto->ItemStat[i].ItemStatValue) * static_cast<long double>(totalMultiplier));
            uint32 statType = proto->ItemStat[i].ItemStatType;

            // 根据属性类型正确应用
            switch (statType)
            {
                case ITEM_MOD_MANA:
                    player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_HEALTH:
                    player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_AGILITY:
                    player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
                    break;
                case ITEM_MOD_STRENGTH:
                    player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
                    break;
                case ITEM_MOD_INTELLECT:
                    player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
                    break;
                case ITEM_MOD_SPIRIT:
                    player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
                    break;
                case ITEM_MOD_STAMINA:
                    player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                    player->ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
                    break;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                    player->ApplyRatingMod(CR_DEFENSE_SKILL, val, apply);
                    break;
                case ITEM_MOD_DODGE_RATING:
                    player->ApplyRatingMod(CR_DODGE, val, apply);
                    break;
                case ITEM_MOD_PARRY_RATING:
                    player->ApplyRatingMod(CR_PARRY, val, apply);
                    break;
                case ITEM_MOD_BLOCK_RATING:
                    player->ApplyRatingMod(CR_BLOCK, val, apply);
                    break;
                case ITEM_MOD_HIT_MELEE_RATING:
                    player->ApplyRatingMod(CR_HIT_MELEE, val, apply);
                    break;
                case ITEM_MOD_HIT_RANGED_RATING:
                    player->ApplyRatingMod(CR_HIT_RANGED, val, apply);
                    break;
                case ITEM_MOD_HIT_SPELL_RATING:
                    player->ApplyRatingMod(CR_HIT_SPELL, val, apply);
                    break;
                case ITEM_MOD_CRIT_MELEE_RATING:
                    player->ApplyRatingMod(CR_CRIT_MELEE, val, apply);
                    break;
                case ITEM_MOD_CRIT_RANGED_RATING:
                    player->ApplyRatingMod(CR_CRIT_RANGED, val, apply);
                    break;
                case ITEM_MOD_CRIT_SPELL_RATING:
                    player->ApplyRatingMod(CR_CRIT_SPELL, val, apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, val, apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, val, apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, val, apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, val, apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, val, apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, val, apply);
                    break;
                case ITEM_MOD_HASTE_MELEE_RATING:
                    player->ApplyRatingMod(CR_HASTE_MELEE, val, apply);
                    break;
                case ITEM_MOD_HASTE_RANGED_RATING:
                    player->ApplyRatingMod(CR_HASTE_RANGED, val, apply);
                    break;
                case ITEM_MOD_HASTE_SPELL_RATING:
                    player->ApplyRatingMod(CR_HASTE_SPELL, val, apply);
                    break;
                case ITEM_MOD_HIT_RATING:
                    player->ApplyRatingMod(CR_HIT_MELEE, val, apply);
                    player->ApplyRatingMod(CR_HIT_RANGED, val, apply);
                    player->ApplyRatingMod(CR_HIT_SPELL, val, apply);
                    break;
                case ITEM_MOD_CRIT_RATING:
                    player->ApplyRatingMod(CR_CRIT_MELEE, val, apply);
                    player->ApplyRatingMod(CR_CRIT_RANGED, val, apply);
                    player->ApplyRatingMod(CR_CRIT_SPELL, val, apply);
                    break;
                case ITEM_MOD_HIT_TAKEN_RATING:
                    player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, val, apply);
                    player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, val, apply);
                    player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, val, apply);
                    break;
                case ITEM_MOD_CRIT_TAKEN_RATING:
                case ITEM_MOD_RESILIENCE_RATING:
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, val, apply);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, val, apply);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, val, apply);
                    break;
                case ITEM_MOD_HASTE_RATING:
                    player->ApplyRatingMod(CR_HASTE_MELEE, val, apply);
                    player->ApplyRatingMod(CR_HASTE_RANGED, val, apply);
                    player->ApplyRatingMod(CR_HASTE_SPELL, val, apply);
                    break;
                case ITEM_MOD_EXPERTISE_RATING:
                    player->ApplyRatingMod(CR_EXPERTISE, val, apply);
                    break;
                case ITEM_MOD_ATTACK_POWER:
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), apply);
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_RANGED_ATTACK_POWER:
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
                    break;
                case ITEM_MOD_MANA_REGENERATION:
                    player->ApplyManaRegenBonus(ClampAscensionInt64ToInt32(val), apply);
                    break;
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    player->ApplyRatingMod(CR_ARMOR_PENETRATION, val, apply);
                    break;
                case ITEM_MOD_SPELL_POWER:
                    player->ApplySpellPowerBonus(val, apply);
                    break;
                case ITEM_MOD_HEALTH_REGEN:
                    player->ApplyHealthRegenBonus(ClampAscensionInt64ToInt32(val), apply);
                    break;
                case ITEM_MOD_SPELL_PENETRATION:
                    player->ApplySpellPenetrationBonus(ClampAscensionInt64ToInt32(val), apply);
                    break;
                case ITEM_MOD_BLOCK_VALUE:
                    player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), apply);
                    break;
                default:
                    break;
            }

            // 【修复】按槽位记录已应用的属性
            if (apply)
            {
                AppliedStatEffect effect;
                effect.statType = statType;
                effect.statValue = val;
                status->slotStats[slot].push_back(effect);
            }
        }
    }

    // 【修复】应用护甲值
    if (proto->Armor > 0)
    {
        int32 armorVal = int32(proto->Armor * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(armorVal), apply);
        if (apply)
        {
            AppliedStatEffect effect;
            effect.statType = 1000;  // 自定义标识：护甲
            effect.statValue = armorVal;
            status->slotStats[slot].push_back(effect);
        }
    }

    // 【修复】应用格挡值（盾牌）
    if (proto->Block > 0)
    {
        int32 blockVal = int32(proto->Block * totalMultiplier);
        player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(blockVal), apply);
        if (apply)
        {
            AppliedStatEffect effect;
            effect.statType = 1001;  // 自定义标识：格挡值
            effect.statValue = blockVal;
            status->slotStats[slot].push_back(effect);
        }
    }

    // 【修复】应用抗性值
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
    {
        // 注意：抗性在 ItemTemplate 中的存储方式
    }

    // 物品抗性（Holy, Fire, Nature, Frost, Shadow, Arcane）
    if (proto->HolyRes > 0)
    {
        int32 val = int32(proto->HolyRes * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(val), apply);
        if (apply) { AppliedStatEffect e; e.statType = 1002; e.statValue = val; status->slotStats[slot].push_back(e); }
    }
    if (proto->FireRes > 0)
    {
        int32 val = int32(proto->FireRes * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(val), apply);
        if (apply) { AppliedStatEffect e; e.statType = 1003; e.statValue = val; status->slotStats[slot].push_back(e); }
    }
    if (proto->NatureRes > 0)
    {
        int32 val = int32(proto->NatureRes * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(val), apply);
        if (apply) { AppliedStatEffect e; e.statType = 1004; e.statValue = val; status->slotStats[slot].push_back(e); }
    }
    if (proto->FrostRes > 0)
    {
        int32 val = int32(proto->FrostRes * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(val), apply);
        if (apply) { AppliedStatEffect e; e.statType = 1005; e.statValue = val; status->slotStats[slot].push_back(e); }
    }
    if (proto->ShadowRes > 0)
    {
        int32 val = int32(proto->ShadowRes * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(val), apply);
        if (apply) { AppliedStatEffect e; e.statType = 1006; e.statValue = val; status->slotStats[slot].push_back(e); }
    }
    if (proto->ArcaneRes > 0)
    {
        int32 val = int32(proto->ArcaneRes * totalMultiplier);
        player->HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(val), apply);
        if (apply) { AppliedStatEffect e; e.statType = 1007; e.statValue = val; status->slotStats[slot].push_back(e); }
    }

    // 【修复】应用附魔效果（从物品实例读取）
    if (item)
    {
        for (uint8 enchantSlot = 0; enchantSlot < MAX_ENCHANTMENT_SLOT; ++enchantSlot)
        {
            uint32 enchantId = item->GetEnchantmentId(EnchantmentSlot(enchantSlot));
            if (enchantId == 0)
                continue;

            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!enchant)
                continue;

            for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
            {
                uint32 enchType = enchant->type[s];
                int32 amount = enchant->amount[s];

                if (amount == 0)
                    continue;

                // 附魔不应用倍率（保持原始效果）
                switch (enchType)
                {
                    case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:
                    case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                    case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
                        // 这些类型通过法术处理
                        if (enchant->spellid[s])
                        {
                            if (apply)
                            {
                                if (SpellInfo const* enchantSpellInfo = sSpellMgr->GetSpellInfo(enchant->spellid[s]))
                                    CastAscensionEquipSpell(player, item, enchantSpellInfo);
                                status->slotSpells[slot].push_back(enchant->spellid[s]);
                            }
                            else
                            {
                                // 检查其他槽位是否有相同法术
                                bool spellFromOtherSlot = false;
                                for (const auto& slotPair : status->slotSpells)
                                {
                                    if (slotPair.first != slot)
                                    {
                                        for (uint32 spId : slotPair.second)
                                        {
                                            if (spId == enchant->spellid[s])
                                            {
                                                spellFromOtherSlot = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (spellFromOtherSlot) break;
                                }
                                if (!spellFromOtherSlot)
                                {
                                    player->RemoveAurasDueToSpell(enchant->spellid[s]);
                                }
                            }
                        }
                        break;
                    case ITEM_ENCHANTMENT_TYPE_STAT:
                        // 属性附魔
                        if (enchant->spellid[s] < MAX_ITEM_MOD)
                        {
                            // 这里使用与物品属性相同的逻辑
                            uint32 statType = enchant->spellid[s];
                            ApplyEnchantStatMod(player, statType, amount, apply);
                            if (apply)
                            {
                                AppliedStatEffect e;
                                e.statType = statType;
                                e.statValue = amount;
                                status->slotStats[slot].push_back(e);
                            }
                        }
                        break;
                    case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                        // 抗性附魔
                        if (enchant->spellid[s] < MAX_SPELL_SCHOOL)
                        {
                            SpellSchools school = SpellSchools(enchant->spellid[s]);
                            player->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + school), BASE_VALUE, float(amount), apply);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // 应用物品法术效果 - 【修复】按槽位绑定法术来源
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId <= 0)
            continue;

        if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[i].SpellId);
            if (!spellInfo)
                continue;

            uint32 spellId = proto->Spells[i].SpellId;

            if (apply)
            {
                CastAscensionEquipSpell(player, item, spellInfo);
                // 按槽位记录法术，便于单件卸下时精确移除
                status->slotSpells[slot].push_back(spellId);
            }
            else
            {
                // 【修复】只移除属于该槽位的法术，检查其他槽位是否也有相同法术
                bool spellFromOtherSlot = false;
                for (const auto& slotPair : status->slotSpells)
                {
                    if (slotPair.first != slot)
                    {
                        for (uint32 otherSpellId : slotPair.second)
                        {
                            if (otherSpellId == spellId)
                            {
                                spellFromOtherSlot = true;
                                break;
                            }
                        }
                    }
                    if (spellFromOtherSlot)
                        break;
                }

                // 只有当其他槽位没有提供相同法术时才移除
                if (!spellFromOtherSlot)
                {
                    player->RemoveAurasDueToSpell(spellId);
                }

                // 从该槽位的法术列表中移除
                auto& slotSpellList = status->slotSpells[slot];
                slotSpellList.erase(
                    std::remove(slotSpellList.begin(), slotSpellList.end(), spellId),
                    slotSpellList.end());
            }
        }
    }

    // 应用鉴定系统的自定义属性（通过幻境系统）
    // 包括：基础属性、追加属性、成长属性、强化属性等，以及倍率加成
#ifdef MODULE_HUANJING_SYSTEM
    // 【修复】添加 sHuanJingSystem 空指针检查
    if (item && sHuanJingSystem)
    {
        if (apply)
        {
            // 应用鉴定系统的自定义属性（包含倍率计算）
            sHuanJingSystem->ApplyHuanJingEnhancement(player, item, updateStats);

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 应用了鉴定系统自定义属性 (物品GUID: {})",
                    player->GetName(), slot, item->GetGUID().GetCounter());
            }
        }
        else
        {
            // 移除鉴定系统的自定义属性
            sHuanJingSystem->RemoveHuanJingEnhancement(player, item);

            if (sAscensionConfig->IsDebugMode())
            {
                LOG_INFO("module", "飞升系统: 玩家 {} 槽位 {} 移除了鉴定系统自定义属性 (物品GUID: {})",
                    player->GetName(), slot, item->GetGUID().GetCounter());
            }
        }
    }
#endif

    // 更新玩家属性
    if (updateStats)
        UpdatePlayerStats(player);
}

bool AscensionManager::CanEquipItemInSlot(Player* player, uint8 slot, uint32 itemId)
{
    if (!player || slot >= ASCENSION_SLOT_COUNT)
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
    {
        LOG_ERROR("module", "飞升系统: CanEquipItemInSlot 物品模板未找到 itemId={}", itemId);
        return false;
    }

    // 检查物品等级要求
    if (proto->RequiredLevel > player->GetLevel())
    {
        if (sAscensionConfig->IsDebugMode())
            LOG_INFO("module", "飞升系统: 等级不足 需要={} 玩家={}", proto->RequiredLevel, player->GetLevel());
        return false;
    }

    // 检查物品是否是装备类型 (InventoryType = 0 表示不是可装备物品)
    if (proto->InventoryType == 0)
        return false;

    // === 【修复】复用核心装备校验逻辑 ===

    // 检查职业限制
    if (proto->AllowableClass != 0 && proto->AllowableClass != -1)
    {
        if (!(proto->AllowableClass & player->getClassMask()))
        {
            if (sAscensionConfig->IsDebugMode())
                LOG_INFO("module", "飞升系统: 职业不匹配 物品职业限制={} 玩家职业={}", proto->AllowableClass, player->getClassMask());
            return false;
        }
    }

    // 检查种族限制
    if (proto->AllowableRace != 0 && proto->AllowableRace != -1)
    {
        if (!(proto->AllowableRace & player->getRaceMask()))
        {
            if (sAscensionConfig->IsDebugMode())
                LOG_INFO("module", "飞升系统: 种族不匹配 物品种族限制={} 玩家种族={}", proto->AllowableRace, player->getRaceMask());
            return false;
        }
    }

    // 检查技能要求
    if (proto->RequiredSkill != 0)
    {
        if (player->GetSkillValue(proto->RequiredSkill) < proto->RequiredSkillRank)
        {
            if (sAscensionConfig->IsDebugMode())
                LOG_INFO("module", "飞升系统: 技能不足 需要技能={} 等级={} 玩家等级={}",
                    proto->RequiredSkill, proto->RequiredSkillRank, player->GetSkillValue(proto->RequiredSkill));
            return false;
        }
    }

    // 检查声望要求
    if (proto->RequiredReputationFaction != 0)
    {
        if (player->GetReputationRank(proto->RequiredReputationFaction) < proto->RequiredReputationRank)
        {
            if (sAscensionConfig->IsDebugMode())
                LOG_INFO("module", "飞升系统: 声望不足");
            return false;
        }
    }

    // 检查法术要求
    if (proto->RequiredSpell != 0 && !player->HasSpell(proto->RequiredSpell))
    {
        if (sAscensionConfig->IsDebugMode())
            LOG_INFO("module", "飞升系统: 缺少所需法术 spellId={}", proto->RequiredSpell);
        return false;
    }

    // 【修复】检查 Unique-Equipped 限制
    // 检查物品是否标记为唯一装备
    if (proto->Flags & ITEM_FLAG_UNIQUE_EQUIPPABLE)
    {
        // 检查玩家是否已经装备了相同物品（正常装备栏或飞升槽位）
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (equipped && equipped->GetEntry() == itemId)
            {
                if (sAscensionConfig->IsDebugMode())
                    LOG_INFO("module", "飞升系统: Unique-Equipped 物品已在正常装备栏装备 itemId={}", itemId);
                return false;
            }
        }

        // 检查飞升槽位
        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
        if (status)
        {
            for (const auto& slotPair : status->slots)
            {
                if (slotPair.first != slot && slotPair.second.itemId == itemId)
                {
                    if (sAscensionConfig->IsDebugMode())
                        LOG_INFO("module", "飞升系统: Unique-Equipped 物品已在飞升槽位装备 itemId={}", itemId);
                    return false;
                }
            }
        }
    }

    // 【修复】检查 ItemLimitCategory 限制（如宝石槽位限制等）
    if (proto->ItemLimitCategory)
    {
        ItemLimitCategoryEntry const* limitEntry = sItemLimitCategoryStore.LookupEntry(proto->ItemLimitCategory);
        if (limitEntry)
        {
            // 计算玩家已装备的同类物品数量
            uint32 count = 0;

            // 检查正常装备栏
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (equipped)
                {
                    ItemTemplate const* equippedProto = equipped->GetTemplate();
                    if (equippedProto && equippedProto->ItemLimitCategory == proto->ItemLimitCategory)
                        ++count;
                }
            }

            // 检查飞升槽位
            uint32 playerGuid = player->GetGUID().GetCounter();
            PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
            if (status)
            {
                for (const auto& slotPair : status->slots)
                {
                    if (slotPair.first != slot)  // 不计算当前要装备的槽位
                    {
                        ItemTemplate const* slotProto = sObjectMgr->GetItemTemplate(slotPair.second.itemId);
                        if (slotProto && slotProto->ItemLimitCategory == proto->ItemLimitCategory)
                            ++count;
                    }
                }
            }

            // 检查是否超过限制
            if (count >= limitEntry->maxCount)
            {
                if (sAscensionConfig->IsDebugMode())
                    LOG_INFO("module", "飞升系统: ItemLimitCategory 限制超出 category={} count={} max={}",
                        proto->ItemLimitCategory, count, limitEntry->maxCount);
                return false;
            }
        }
    }

    // 检查双手武器互斥（如果要装备双手武器到主手，副手槽位必须为空）
    if (proto->InventoryType == INVTYPE_2HWEAPON && slot == ASCENSION_SLOT_MAINHAND)
    {
        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
        if (status && status->slots.find(ASCENSION_SLOT_OFFHAND) != status->slots.end())
        {
            if (sAscensionConfig->IsDebugMode())
                LOG_INFO("module", "飞升系统: 装备双手武器时副手槽位必须为空");
            return false;
        }
    }

    // 检查副手槽位时主手是否装备了双手武器
    if (slot == ASCENSION_SLOT_OFFHAND)
    {
        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
        if (status)
        {
            auto mainIt = status->slots.find(ASCENSION_SLOT_MAINHAND);
            if (mainIt != status->slots.end())
            {
                ItemTemplate const* mainProto = sObjectMgr->GetItemTemplate(mainIt->second.itemId);
                if (mainProto && mainProto->InventoryType == INVTYPE_2HWEAPON)
                {
                    if (sAscensionConfig->IsDebugMode())
                        LOG_INFO("module", "飞升系统: 主手已装备双手武器，无法装备副手");
                    return false;
                }
            }
        }
    }

    // 获取物品应该装备的槽位
    uint8 expectedSlot = GetSlotForItemClass(proto->Class, proto->SubClass, proto->InventoryType);

    // 无效槽位
    if (expectedSlot == 0xFF)
        return false;

    // 特殊处理：戒指和饰品可以装备到两个槽位
    if (slot == ASCENSION_SLOT_FINGER1 || slot == ASCENSION_SLOT_FINGER2)
    {
        return expectedSlot == ASCENSION_SLOT_FINGER1 || expectedSlot == ASCENSION_SLOT_FINGER2;
    }
    if (slot == ASCENSION_SLOT_TRINKET1 || slot == ASCENSION_SLOT_TRINKET2)
    {
        return expectedSlot == ASCENSION_SLOT_TRINKET1 || expectedSlot == ASCENSION_SLOT_TRINKET2;
    }

    // 【修复】特殊处理：INVTYPE_WEAPON 类型的单手武器可以装备到副手
    if (slot == ASCENSION_SLOT_OFFHAND && proto->InventoryType == INVTYPE_WEAPON)
    {
        // 检查玩家是否有双持技能
        if (player->CanDualWield())
            return true;
        else
        {
            if (sAscensionConfig->IsDebugMode())
                LOG_INFO("module", "飞升系统: 玩家没有双持技能，无法将单手武器装备到副手");
            return false;
        }
    }

    return expectedSlot == slot;
}

uint8 AscensionManager::GetSlotForItemClass(uint32 itemClass, uint32 itemSubClass, uint32 inventoryType)
{
    (void)itemClass;
    (void)itemSubClass;

    switch (inventoryType)
    {
        case INVTYPE_HEAD:          return ASCENSION_SLOT_HEAD;
        case INVTYPE_NECK:          return ASCENSION_SLOT_NECK;
        case INVTYPE_SHOULDERS:     return ASCENSION_SLOT_SHOULDERS;
        case INVTYPE_BODY:          return ASCENSION_SLOT_BODY;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:          return ASCENSION_SLOT_CHEST;
        case INVTYPE_WAIST:         return ASCENSION_SLOT_WAIST;
        case INVTYPE_LEGS:          return ASCENSION_SLOT_LEGS;
        case INVTYPE_FEET:          return ASCENSION_SLOT_FEET;
        case INVTYPE_WRISTS:        return ASCENSION_SLOT_WRISTS;
        case INVTYPE_HANDS:         return ASCENSION_SLOT_HANDS;
        case INVTYPE_FINGER:        return ASCENSION_SLOT_FINGER1;
        case INVTYPE_TRINKET:       return ASCENSION_SLOT_TRINKET1;
        case INVTYPE_CLOAK:         return ASCENSION_SLOT_BACK;
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_2HWEAPON:      return ASCENSION_SLOT_MAINHAND;
        // 【修复】INVTYPE_WEAPON 可以装备到主手或副手，返回主手作为默认
        // 实际是否可装备到副手由 CanEquipItemInSlot 中的特殊处理决定
        case INVTYPE_WEAPON:        return ASCENSION_SLOT_MAINHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:      return ASCENSION_SLOT_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:   return ASCENSION_SLOT_RANGED;
        // 【修复】添加 INVTYPE_RELIC 支持（圣物/图腾/神像/魔印）
        case INVTYPE_RELIC:         return ASCENSION_SLOT_RANGED;
        default:                    return 0xFF; // 无效槽位
    }
}

bool AscensionManager::IsAscensionOnlyItem(uint32 itemId) const
{
    return _restrictedItemIds.find(itemId) != _restrictedItemIds.end();
}

PlayerAscensionStatus* AscensionManager::GetPlayerStatus(uint32 playerGuid)
{
    auto it = _playerStatus.find(playerGuid);
    if (it != _playerStatus.end())
        return &it->second;
    return nullptr;
}

const AscensionSlotControl* AscensionManager::GetSlotControl(uint8 slot) const
{
    auto it = _slotControls.find(slot);
    if (it != _slotControls.end())
        return &it->second;
    return nullptr;
}

std::string AscensionManager::GetSlotName(uint8 slot) const
{
    auto it = _slotControls.find(slot);
    if (it != _slotControls.end())
        return it->second.slotName;

    static const char* defaultNames[] = {"头部", "颈部", "肩部", "衬衣", "胸甲", "腰带", "腿部", "脚部",
                                          "手腕", "手套", "戒指1", "戒指2", "饰品1", "饰品2", "披风", "主手", "副手", "远程"};
    if (slot < ASCENSION_SLOT_COUNT)
        return defaultNames[slot];
    return "未知";
}

void AscensionManager::ApplyAscensionItemSets(Player* player)
{
    if (!player)
        return;

    PlayerAscensionStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
    if (!status)
        return;

    if (!status->itemSetCounts.empty())
        RemoveAscensionItemSets(player);

    std::map<uint32, uint32> setCounts;
    for (auto const& slotPair : status->slots)
    {
        uint8 slot = slotPair.first;
        auto ctrlIt = _slotControls.find(slot);
        if (ctrlIt != _slotControls.end() && !ctrlIt->second.enabled)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(slotPair.second.itemId);
        if (!proto || proto->ItemSet == 0)
            continue;

        ++setCounts[proto->ItemSet];
    }

    for (auto const& setPair : setCounts)
        ApplyAscensionItemSet(player, setPair.first, setPair.second);
}

void AscensionManager::RemoveAscensionItemSets(Player* player)
{
    if (!player)
        return;

    PlayerAscensionStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
    if (!status || status->itemSetCounts.empty())
        return;

    std::map<uint32, uint32> appliedCounts = status->itemSetCounts;
    for (auto const& setPair : appliedCounts)
        RemoveAscensionItemSet(player, setPair.first, setPair.second);
}

bool AscensionManager::ApplyAscensionItemSet(Player* player, uint32 itemSetId, uint32 itemCount)
{
    if (!player || itemSetId == 0 || itemCount == 0)
        return false;

    ItemSetEntry const* set = sItemSetStore.LookupEntry(itemSetId);
    if (!set)
    {
        LOG_ERROR("sql.sql", "飞升系统: ItemSet {} not found, mods not applied.", itemSetId);
        return false;
    }

    if (set->required_skill_id && player->GetSkillValue(set->required_skill_id) < set->required_skill_value)
        return false;

    PlayerAscensionStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
    if (!status)
        return false;

    ItemSetEffect* effect = FindOrCreateAscensionItemSetEffect(player, itemSetId);
    if (!effect)
        return false;

    uint32 oldCount = effect->item_count;
    effect->item_count += itemCount;
    status->itemSetCounts[itemSetId] += itemCount;

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
            LOG_ERROR("entities.item", "飞升系统: unknown spell id {} in aggregated item set {}.", spellId, itemSetId);
            continue;
        }

        for (uint32 slot = 0; slot < MAX_ITEM_SET_SPELLS; ++slot)
        {
            if (!effect->spells[slot])
            {
                effect->spells[slot] = spellInfo;
                if (sScriptMgr->OnPlayerCanApplyEquipSpellsItemSet(player, effect))
                {
                    player->ApplyEquipSpell(spellInfo, nullptr, true);
                    MakeAscensionItemSetSpellPermanent(player, spellId);
                }
                break;
            }
        }
    }

    return true;
}

void AscensionManager::RemoveAscensionItemSet(Player* player, uint32 itemSetId, uint32 itemCount)
{
    if (!player || itemSetId == 0 || itemCount == 0)
        return;

    PlayerAscensionStatus* status = GetPlayerStatus(player->GetGUID().GetCounter());
    if (!status)
        return;

    auto appliedIt = status->itemSetCounts.find(itemSetId);
    if (appliedIt == status->itemSetCounts.end())
        return;

    uint32 removeCount = std::min(itemCount, appliedIt->second);

    ItemSetEntry const* set = sItemSetStore.LookupEntry(itemSetId);
    if (!set)
    {
        LOG_ERROR("sql.sql", "飞升系统: ItemSet {} not found, mods not removed.", itemSetId);
        appliedIt->second -= removeCount;
        if (appliedIt->second == 0)
            status->itemSetCounts.erase(appliedIt);
        return;
    }

    ItemSetEffect* effect = FindAscensionItemSetEffect(player, itemSetId);
    if (!effect)
    {
        appliedIt->second -= removeCount;
        if (appliedIt->second == 0)
            status->itemSetCounts.erase(appliedIt);
        return;
    }

    uint32 oldCount = effect->item_count;
    effect->item_count = oldCount > removeCount ? oldCount - removeCount : 0;

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

    appliedIt->second -= removeCount;
    if (appliedIt->second == 0)
        status->itemSetCounts.erase(appliedIt);

}

void AscensionManager::ApplyEnchantStatMod(Player* player, uint32 statType, int64 amount, bool apply)
{
    if (!player || amount == 0)
        return;

    int64 val = amount;

    switch (statType)
    {
        case ITEM_MOD_MANA:
            player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
            break;
        case ITEM_MOD_HEALTH:
            player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
            break;
        case ITEM_MOD_AGILITY:
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
            player->ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
            break;
        case ITEM_MOD_STRENGTH:
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
            player->ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
            break;
        case ITEM_MOD_INTELLECT:
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
            player->ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
            break;
        case ITEM_MOD_SPIRIT:
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
            player->ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
            break;
        case ITEM_MOD_STAMINA:
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
            player->ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            player->ApplyRatingMod(CR_DEFENSE_SKILL, amount, apply);
            break;
        case ITEM_MOD_DODGE_RATING:
            player->ApplyRatingMod(CR_DODGE, amount, apply);
            break;
        case ITEM_MOD_PARRY_RATING:
            player->ApplyRatingMod(CR_PARRY, amount, apply);
            break;
        case ITEM_MOD_BLOCK_RATING:
            player->ApplyRatingMod(CR_BLOCK, amount, apply);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
            break;
        case ITEM_MOD_HIT_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
            player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
            player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
            break;
        case ITEM_MOD_CRIT_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
            player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
            player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, amount, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, amount, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, amount, apply);
            break;
        case ITEM_MOD_HASTE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, amount, apply);
            player->ApplyRatingMod(CR_HASTE_RANGED, amount, apply);
            player->ApplyRatingMod(CR_HASTE_SPELL, amount, apply);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            player->ApplyRatingMod(CR_EXPERTISE, amount, apply);
            break;
        case ITEM_MOD_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), apply);
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
            break;
        case ITEM_MOD_RANGED_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
            break;
        case ITEM_MOD_MANA_REGENERATION:
            player->ApplyManaRegenBonus(ClampAscensionInt64ToInt32(amount), apply);
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, amount, apply);
            break;
        case ITEM_MOD_SPELL_POWER:
            player->ApplySpellPowerBonus(amount, apply);
            break;
        case ITEM_MOD_HEALTH_REGEN:
            player->ApplyHealthRegenBonus(ClampAscensionInt64ToInt32(amount), apply);
            break;
        case ITEM_MOD_SPELL_PENETRATION:
            player->ApplySpellPenetrationBonus(ClampAscensionInt64ToInt32(amount), apply);
            break;
        case ITEM_MOD_BLOCK_VALUE:
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), apply);
            break;
        default:
            break;
    }
}

void AscensionManager::RemoveStatEffect(Player* player, uint32 statType, int64 statValue)
{
    if (!player || statValue == 0)
        return;

    // 处理标准物品属性类型
    switch (statType)
    {
        case ITEM_MOD_MANA:
            player->HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(statValue), false);
            break;
        case ITEM_MOD_HEALTH:
            player->HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(statValue), false);
            break;
        case ITEM_MOD_AGILITY:
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(statValue), false);
            player->ApplyStatBuffMod(STAT_AGILITY, float(statValue), false);
            break;
        case ITEM_MOD_STRENGTH:
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(statValue), false);
            player->ApplyStatBuffMod(STAT_STRENGTH, float(statValue), false);
            break;
        case ITEM_MOD_INTELLECT:
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(statValue), false);
            player->ApplyStatBuffMod(STAT_INTELLECT, float(statValue), false);
            break;
        case ITEM_MOD_SPIRIT:
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(statValue), false);
            player->ApplyStatBuffMod(STAT_SPIRIT, float(statValue), false);
            break;
        case ITEM_MOD_STAMINA:
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(statValue), false);
            player->ApplyStatBuffMod(STAT_STAMINA, float(statValue), false);
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            player->ApplyRatingMod(CR_DEFENSE_SKILL, statValue, false);
            break;
        case ITEM_MOD_DODGE_RATING:
            player->ApplyRatingMod(CR_DODGE, statValue, false);
            break;
        case ITEM_MOD_PARRY_RATING:
            player->ApplyRatingMod(CR_PARRY, statValue, false);
            break;
        case ITEM_MOD_BLOCK_RATING:
            player->ApplyRatingMod(CR_BLOCK, statValue, false);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, statValue, false);
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            player->ApplyRatingMod(CR_HIT_RANGED, statValue, false);
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            player->ApplyRatingMod(CR_HIT_SPELL, statValue, false);
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, statValue, false);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            player->ApplyRatingMod(CR_CRIT_RANGED, statValue, false);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            player->ApplyRatingMod(CR_CRIT_SPELL, statValue, false);
            break;
        case ITEM_MOD_HIT_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, statValue, false);
            player->ApplyRatingMod(CR_HIT_RANGED, statValue, false);
            player->ApplyRatingMod(CR_HIT_SPELL, statValue, false);
            break;
        case ITEM_MOD_CRIT_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, statValue, false);
            player->ApplyRatingMod(CR_CRIT_RANGED, statValue, false);
            player->ApplyRatingMod(CR_CRIT_SPELL, statValue, false);
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, statValue, false);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, statValue, false);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, statValue, false);
            break;
        case ITEM_MOD_HASTE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, statValue, false);
            player->ApplyRatingMod(CR_HASTE_RANGED, statValue, false);
            player->ApplyRatingMod(CR_HASTE_SPELL, statValue, false);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            player->ApplyRatingMod(CR_EXPERTISE, statValue, false);
            break;
        case ITEM_MOD_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(statValue), false);
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(statValue), false);
            break;
        case ITEM_MOD_RANGED_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(statValue), false);
            break;
        case ITEM_MOD_MANA_REGENERATION:
            player->ApplyManaRegenBonus(ClampAscensionInt64ToInt32(statValue), false);
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, statValue, false);
            break;
        case ITEM_MOD_SPELL_POWER:
            player->ApplySpellPowerBonus(statValue, false);
            break;
        case ITEM_MOD_HEALTH_REGEN:
            player->ApplyHealthRegenBonus(ClampAscensionInt64ToInt32(statValue), false);
            break;
        case ITEM_MOD_SPELL_PENETRATION:
            player->ApplySpellPenetrationBonus(ClampAscensionInt64ToInt32(statValue), false);
            break;
        case ITEM_MOD_BLOCK_VALUE:
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(statValue), false);
            break;
        // 自定义属性类型（1000+）
        case 1000:  // 护甲
            player->HandleStatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(statValue), false);
            break;
        case 1001:  // 格挡值
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(statValue), false);
            break;
        case 1002:  // 神圣抗性
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(statValue), false);
            break;
        case 1003:  // 火焰抗性
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(statValue), false);
            break;
        case 1004:  // 自然抗性
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(statValue), false);
            break;
        case 1005:  // 冰霜抗性
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(statValue), false);
            break;
        case 1006:  // 暗影抗性
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(statValue), false);
            break;
        case 1007:  // 奥术抗性
            player->HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(statValue), false);
            break;
        default:
            // 附魔属性标识（2000+）需要使用 ApplyEnchantStatMod 移除
            if (statType >= 2000)
            {
                // 附魔属性的实际类型需要从记录中解析
                // 这里简化处理，附魔属性已经通过法术系统管理
            }
            break;
    }
}

void AscensionManager::UpdatePlayerStats(Player* player)
{
    if (!player)
        return;

    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);
}

Item* AscensionManager::FindItemInBags(Player* player, uint32 itemGuid)
{
    if (!player)
        return nullptr;

    // 在主背包中查找
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
            return item;
    }

    // 在其他背包中查找
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
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

bool AscensionManager::IsItemEquippedInAscension(Player* player, uint32 itemGuid)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return false;

    for (const auto& pair : status->slots)
    {
        if (pair.second.itemGuid == itemGuid)
            return true;
    }
    return false;
}

int8 AscensionManager::GetAscensionSlotByItemGuid(Player* player, uint32 itemGuid)
{
    if (!player)
        return -1;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
        return -1;

    for (const auto& pair : status->slots)
    {
        if (pair.second.itemGuid == itemGuid)
            return static_cast<int8>(pair.first);
    }
    return -1;
}

void AscensionManager::SendAscensionDataToClient(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    PlayerAscensionStatus* status = GetPlayerStatus(playerGuid);
    if (!status)
    {
        return;
    }

    // 构建压缩的数据字符串
    // 格式: ASC:U=解锁槽位位图;E=槽位:物品ID:GUID:套装ID,槽位:物品ID:GUID:套装ID...

    // 计算解锁槽位位图
    uint32 unlockedBitmap = 0;
    for (uint8 slot = 0; slot < ASCENSION_SLOT_COUNT; ++slot)
    {
        if (IsSlotUnlocked(player, slot))
        {
            unlockedBitmap |= (1 << slot);
        }
    }

    // 构建装备数据（只包含有装备的槽位）
    std::ostringstream equipStr;
    bool first = true;
    for (const auto& pair : status->slots)
    {
        if (!first)
            equipStr << ",";

        uint32 itemSet = 0;
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(pair.second.itemId))
            itemSet = proto->ItemSet;

        equipStr << (int)pair.first << ":" << pair.second.itemId << ":" << pair.second.itemGuid << ":" << itemSet;
        first = false;
    }

    // 组合数据部分 - 格式: U=位图;E=装备数据
    std::ostringstream dataStr;
    dataStr << "U=" << unlockedBitmap << ";E=" << equipStr.str();

    // 【关键】WoTLK 3.3.5 的 Addon 消息格式：PREFIX<TAB>DATA
    // 使用 "ASCENSION" 作为前缀，与客户端注册的前缀一致
    std::string fullMessage = "ASCENSION\t" + dataStr.str();

    // 【修复】检查消息长度，如果超过255字节则拆分发送
    const size_t MAX_ADDON_MSG_LEN = 250;  // 留一些余量

    if (fullMessage.length() <= MAX_ADDON_MSG_LEN)
    {
        // 消息长度正常，直接发送
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
    else
    {
        // 消息过长，分两部分发送：先发解锁数据，再发装备数据
        // 第一条消息：解锁数据
        std::string msg1 = "ASCENSION\tU=" + std::to_string(unlockedBitmap);
        WorldPacket data1;
        ChatHandler::BuildChatPacket(data1, CHAT_MSG_WHISPER, LANG_ADDON, player, player, msg1, 0);
        player->SendDirectMessage(&data1);

        // 第二条消息：装备数据
        std::string equipData = equipStr.str();
        if (!equipData.empty())
        {
            std::string msg2 = "ASCENSION\tE=" + equipData;

            // 如果装备数据仍然太长，继续拆分
            while (msg2.length() > MAX_ADDON_MSG_LEN)
            {
                // 找到最后一个逗号位置作为分割点
                size_t cutPos = msg2.rfind(',', MAX_ADDON_MSG_LEN - 1);
                if (cutPos == std::string::npos || cutPos < 12)  // "ASCENSION\tE=" 长度约12
                {
                    // 无法再拆分，记录警告
                    LOG_WARN("module", "飞升系统: 单条装备数据过长，可能导致客户端解析失败");
                    break;
                }

                std::string partMsg = msg2.substr(0, cutPos);
                WorldPacket partData;
                ChatHandler::BuildChatPacket(partData, CHAT_MSG_WHISPER, LANG_ADDON, player, player, partMsg, 0);
                player->SendDirectMessage(&partData);

                msg2 = "ASCENSION\tE=" + msg2.substr(cutPos + 1);
            }

            // 发送最后一部分
            WorldPacket data2;
            ChatHandler::BuildChatPacket(data2, CHAT_MSG_WHISPER, LANG_ADDON, player, player, msg2, 0);
            player->SendDirectMessage(&data2);
        }
    }

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 发送数据到客户端 (长度:{}) - {}", fullMessage.length(), fullMessage);
    }
}

//=============================================================================
// AscensionWorldScript 实现
//=============================================================================

AscensionWorldScript::AscensionWorldScript() : WorldScript("AscensionWorldScript")
{
    _initialized = false;
    _loadTimer = 0;
}

void AscensionWorldScript::OnAfterConfigLoad(bool reload)
{
    sAscensionConfig->LoadConfig();

    if (reload && _initialized)
    {
        sAscensionManager->LoadSlotControls();
        sAscensionManager->LoadRestrictedItems();
        LOG_INFO("module", "飞升系统: 配置已重新加载");
    }
}

void AscensionWorldScript::OnUpdate(uint32 diff)
{
    if (_initialized)
        return;

    _loadTimer += diff;
    if (_loadTimer >= 1000) // 1秒延迟
    {
        if (sAscensionConfig->IsEnabled())
        {
            sAscensionManager->Initialize();
            LOG_INFO("server.loading", "→飞升系统√");
        }
        _initialized = true;
    }
}

//=============================================================================
// AscensionPlayerScript 实现
//=============================================================================

AscensionPlayerScript::AscensionPlayerScript() : PlayerScript("AscensionPlayerScript", {
    PLAYERHOOK_ON_LOGIN,
    PLAYERHOOK_ON_LOGOUT,
    PLAYERHOOK_ON_DELETE,
    PLAYERHOOK_CAN_EQUIP_ITEM
})
{
}

void AscensionPlayerScript::OnPlayerLogin(Player* player)
{
    if (!sAscensionConfig->IsEnabled() || !player)
        return;

    sAscensionManager->LoadPlayerData(player);

    sAscensionManager->ValidateEquippedItems(player);

    sAscensionManager->ApplyAllEffects(player);

    sAscensionManager->SendAscensionDataToClient(player);

    uint32 restrictedEquippedCount = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (equipped && sAscensionManager->IsAscensionOnlyItem(equipped->GetEntry()))
            ++restrictedEquippedCount;
    }

    if (restrictedEquippedCount > 0 && player->GetSession())
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "检测到 {} 件深渊修仙装备仍在官方装备栏。后续不能再直接穿戴，请手动卸下后放入飞升装备槽。",
            restrictedEquippedCount);
    }

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 登录，已加载飞升数据", player->GetName());
    }
}

void AscensionPlayerScript::OnPlayerLogout(Player* player)
{
    if (!sAscensionConfig->IsEnabled() || !player)
        return;

    // 登出路径分段计时，定位 2s 级卡顿
    using Clk = std::chrono::high_resolution_clock;
    auto t0 = Clk::now();
    auto step = [&](char const* name)
    {
        auto now = Clk::now();
        int64 ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        t0 = now;
        if (ms >= 30)
            LOG_WARN("server.loading",
                "[性能监控-飞升登出] 阶段={} 角色={} 耗时={}ms", name, player->GetName(), ms);
    };

    // 【性能优化】登出时不回滚属性/aura/套装。
    //   原因：玩家对象即将 CleanupsBeforeDelete + delete，aura 列表和属性都会被析构，
    //   无须再走 RemoveAurasDueToSpell / RemoveStatEffect / UpdatePlayerStats 这条同步重路径。
    //   RemoveAllEffects 之前实测耗时 2000ms+，是主线程卡顿的主因。
    //   注意：如果未来在登出路径后还有任何"玩家仍在世界"的逻辑依赖此属性状态，需要重新评估。

    // 保存飞升物品到数据库
    sAscensionManager->SaveAscensionItems(player);
    step("SaveAscensionItems");

    // 保存数据
    sAscensionManager->SavePlayerData(player);
    step("SavePlayerData");

    // 【修复】清理玩家内存状态，防止内存泄漏
    // ClearPlayerData 内部 delete 了 slots 中的 Item*，必须保留。
    sAscensionManager->ClearPlayerData(player->GetGUID().GetCounter());
    step("ClearPlayerData");

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 玩家 {} 登出，已保存飞升数据并清理内存", player->GetName());
    }
}

void AscensionPlayerScript::OnPlayerDelete(ObjectGuid guid, uint32 accountId)
{
    if (!sAscensionConfig->IsEnabled())
        return;

    uint32 playerGuid = guid.GetCounter();
    sAscensionManager->DeletePlayerData(playerGuid);

    if (sAscensionConfig->IsDebugMode())
    {
        LOG_INFO("module", "飞升系统: 角色删除，清理玩家GUID:{} 的飞升数据", playerGuid);
    }
}

bool AscensionPlayerScript::OnPlayerCanEquipItem(Player* player, uint8 slot, uint16& dest, Item* pItem, bool swap, bool not_loading)
{
    (void)slot;
    (void)dest;
    (void)swap;

    if (!sAscensionConfig->IsEnabled() || !player || !pItem || !not_loading)
        return true;

    if (!sAscensionManager->IsAscensionOnlyItem(pItem->GetEntry()))
        return true;

    if (player->GetSession())
    {
        ChatHandler(player->GetSession()).SendSysMessage(
            "深渊修仙装备只能穿戴到飞升装备槽位，不能直接装备到官方装备栏。");
    }

    return false;
}

class AscensionUnitScript : public UnitScript
{
public:
    AscensionUnitScript() : UnitScript("AscensionUnitScript", true, {
        UNITHOOK_ON_AURA_APPLY
    })
    {
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (!sAscensionConfig->IsEnabled() || !unit || !aura)
            return;

        Player* targetPlayer = unit->ToPlayer();
        Player* sourcePlayer = aura->GetCaster() ? aura->GetCaster()->ToPlayer() : nullptr;
        if (!sourcePlayer)
            sourcePlayer = targetPlayer;

        if (!sourcePlayer)
            return;

        if (!ShouldKeepAscensionTriggeredAuraPermanent(sourcePlayer, aura->GetId()))
            return;

        aura->SetMaxDuration(-1);
        aura->SetDuration(-1);
    }
};

//=============================================================================
// AscensionCommandScript 实现
//=============================================================================

AscensionCommandScript::AscensionCommandScript() : CommandScript("AscensionCommandScript")
{
}

Acore::ChatCommands::ChatCommandTable AscensionCommandScript::GetCommands() const
{
    static ChatCommandTable ascensionCommandTable =
    {
        { "查看",   HandleAscensionView,    SEC_PLAYER,        Console::No },
        { "装备",   HandleAscensionEquip,   SEC_PLAYER,        Console::No },
        { "卸下",   HandleAscensionUnequip, SEC_PLAYER,        Console::No },
        { "清空",   HandleAscensionClear,   SEC_PLAYER,        Console::No },
        { "刷新",   HandleAscensionRefresh, SEC_PLAYER,        Console::No },
        { "解锁",   HandleAscensionUnlock,  SEC_PLAYER,        Console::No },
        { "重载",   HandleAscensionReload,  SEC_ADMINISTRATOR, Console::No }
    };

    static ChatCommandTable commandTable =
    {
        { "飞升", ascensionCommandTable }
    };

    return commandTable;
}

bool AscensionCommandScript::HandleAscensionView(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 只发送数据到客户端UI，不在聊天框显示
    sAscensionManager->SendAscensionDataToClient(player);

    return true;
}

bool AscensionCommandScript::HandleAscensionEquip(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!*args)
    {
        handler->SendSysMessage("用法: .飞升 装备 <槽位> <背包ID> <背包槽位> [物品GUID]");
        handler->SendSysMessage("槽位: 0=头部, 1=颈部, 2=肩部, 3=衬衣, 4=胸甲, 5=腰带, 6=腿部, 7=脚部");
        handler->SendSysMessage("      8=手腕, 9=手套, 10=戒指1, 11=戒指2, 12=饰品1, 13=饰品2");
        handler->SendSysMessage("      14=披风, 15=主手, 16=副手, 17=远程");
        return true;
    }

    // 使用int类型读取参数，避免uint8的内存覆盖问题
    int ascensionSlotInt = 0;
    int bagIdInt = 0;
    int bagSlotInt = 0;
    uint32 requestedItemGuid = 0;

    std::istringstream iss(args);
    if (!(iss >> ascensionSlotInt >> bagIdInt >> bagSlotInt))
    {
        handler->SendSysMessage("参数错误。用法: .飞升 装备 <槽位> <背包ID> <背包槽位> [物品GUID]");
        return true;
    }
    iss >> requestedItemGuid;

    uint8 ascensionSlot = static_cast<uint8>(ascensionSlotInt);
    uint8 bagId = static_cast<uint8>(bagIdInt);
    uint8 bagSlot = static_cast<uint8>(bagSlotInt);

    // 【修复】参数范围校验
    if (ascensionSlotInt < 0 || ascensionSlotInt >= ASCENSION_SLOT_COUNT)
    {
        handler->PSendSysMessage("无效槽位，有效范围: 0-{}", ASCENSION_SLOT_COUNT - 1);
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

    // 通过背包位置获取物品
    // 客户端: bag=0 是主背包，bag=1-4 是额外背包
    // 客户端: slot 从1开始
    Item* item = nullptr;
    if (bagId == 0)
    {
        // 主背包 (客户端bag=0 对应 INVENTORY_SLOT_BAG_0)
        // 客户端slot从1开始，所以 slot-1 得到0-based索引
        // 主背包物品从 INVENTORY_SLOT_ITEM_START (23) 开始
        uint8 serverSlot = INVENTORY_SLOT_ITEM_START + bagSlot - 1;
        item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
    }
    else
    {
        // 其他背包 (客户端bag=1-4 对应背包槽位 INVENTORY_SLOT_BAG_START + bagId - 1)
        uint8 bagSlotIndex = INVENTORY_SLOT_BAG_START + bagId - 1;
        Bag* bag = player->GetBagByPos(bagSlotIndex);
        if (bag)
        {
            // 客户端slot从1开始，所以 slot-1 得到0-based索引
            item = bag->GetItemByPos(bagSlot - 1);
        }
    }

    if (!item)
    {
        handler->SendSysMessage("背包中未找到该物品。");
        return true;
    }

    if (requestedItemGuid && item->GetGUID().GetCounter() != requestedItemGuid)
    {
        Item* requestedItem = sAscensionManager->FindItemInBags(player, requestedItemGuid);
        if (!requestedItem)
        {
            handler->PSendSysMessage("背包中未找到指定GUID的物品: {}", requestedItemGuid);
            return true;
        }

        if (sAscensionConfig->IsDebugMode())
        {
            LOG_INFO("module",
                "飞升系统: 客户端传入位置与GUID不一致，按GUID修正装备目标 玩家={} 位置GUID={} 请求GUID={}",
                player->GetName(), item->GetGUID().GetCounter(), requestedItemGuid);
        }

        item = requestedItem;
    }

    uint32 itemId = item->GetEntry();
    uint32 itemGuid = item->GetGUID().GetCounter();

    sAscensionManager->EquipItem(player, ascensionSlot, itemId, itemGuid);
    return true;
}

bool AscensionCommandScript::HandleAscensionUnequip(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!*args)
    {
        handler->SendSysMessage("用法: .飞升 卸下 <槽位>");
        return true;
    }

    // 【修复】参数范围校验
    int slotInt = atoi(args);
    if (slotInt < 0 || slotInt >= ASCENSION_SLOT_COUNT)
    {
        handler->PSendSysMessage("无效槽位，有效范围: 0-{}", ASCENSION_SLOT_COUNT - 1);
        return true;
    }
    uint8 slot = static_cast<uint8>(slotInt);
    sAscensionManager->UnequipItem(player, slot);
    return true;
}

bool AscensionCommandScript::HandleAscensionClear(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    sAscensionManager->UnequipAllItems(player);
    return true;
}

bool AscensionCommandScript::HandleAscensionRefresh(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    sAscensionManager->RefreshEffects(player);
    handler->SendSysMessage("飞升装备属性已刷新。");
    return true;
}

bool AscensionCommandScript::HandleAscensionUnlock(ChatHandler* handler, const char* args)
{
    if (!sAscensionConfig->IsEnabled())
    {
        handler->SendSysMessage("飞升系统已禁用。");
        return true;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!*args)
    {
        handler->SendSysMessage("用法: .飞升 解锁 <槽位>");
        return true;
    }

    // 【修复】参数范围校验
    int slotInt = atoi(args);
    if (slotInt < 0 || slotInt >= ASCENSION_SLOT_COUNT)
    {
        handler->PSendSysMessage("无效槽位，有效范围: 0-{}", ASCENSION_SLOT_COUNT - 1);
        return true;
    }
    uint8 slot = static_cast<uint8>(slotInt);
    sAscensionManager->UnlockSlot(player, slot);
    return true;
}

bool AscensionCommandScript::HandleAscensionReload(ChatHandler* handler, const char* args)
{
    sAscensionManager->LoadSlotControls();
    sAscensionManager->LoadRestrictedItems();
    handler->SendSysMessage("飞升系统配置已重新加载。");
    return true;
}

//=============================================================================
// AscensionItemScript 实现 - 阻止飞升槽位中的物品被删除
//=============================================================================

AscensionItemScript::AscensionItemScript() : AllItemScript("AscensionItemScript")
{
}

bool AscensionItemScript::CanItemRemove(Player* player, Item* item)
{
    if (!sAscensionConfig->IsEnabled() || !player || !item)
        return true;  // 允许删除

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 首先检查内存中的状态
    if (sAscensionManager->IsItemEquippedInAscension(player, itemGuid))
    {
        if (sAscensionConfig->IsDebugMode())
        {
            LOG_INFO("module", "飞升系统: 阻止删除飞升槽位中的物品 GUID={} 玩家={} (内存检查)",
                itemGuid, player->GetName());
        }
        return false;  // 阻止删除
    }

    // 如果内存中没有找到，查询数据库确认
    // 这是为了处理玩家状态已被清理但核心还在保存的情况
    QueryResult result = CharacterDatabase.Query(
        "SELECT `装备数据` FROM `_飞升系统_数据` WHERE `玩家GUID` = {}", playerGuid);

    if (result)
    {
        Field* fields = result->Fetch();
        std::string equipStr = fields[0].Get<std::string>();

        if (!equipStr.empty())
        {
            // 【修复】使用 try/catch 包裹解析逻辑，防止脏数据导致崩溃
            try
            {
                // 解析装备数据，检查物品GUID是否在其中
                std::stringstream ss(equipStr);
                std::string slotInfo;
                while (std::getline(ss, slotInfo, ','))
                {
                    if (!slotInfo.empty())
                    {
                        std::stringstream slotSS(slotInfo);
                        std::string part;
                        std::vector<std::string> parts;
                        while (std::getline(slotSS, part, ':'))
                        {
                            parts.push_back(part);
                        }
                        if (parts.size() >= 3)
                        {
                            // 先验证是否为有效数字
                            bool validNumber = !parts[2].empty() &&
                                std::all_of(parts[2].begin(), parts[2].end(), ::isdigit);

                            if (validNumber)
                            {
                                uint32 storedItemGuid = static_cast<uint32>(std::stoul(parts[2]));
                                if (storedItemGuid == itemGuid)
                                {
                                    if (sAscensionConfig->IsDebugMode())
                                    {
                                        LOG_INFO("module", "飞升系统: 阻止删除飞升槽位中的物品 GUID={} 玩家={} (数据库检查)",
                                            itemGuid, player->GetName());
                                    }
                                    return false;  // 阻止删除
                                }
                            }
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("module", "飞升系统: CanItemRemove 解析装备数据失败: {} 数据: {}",
                    e.what(), equipStr);
                // 解析失败时，为安全起见不阻止删除
            }
        }
    }

    return true;  // 允许删除
}

//=============================================================================
// 飞升首领连环脚本
//=============================================================================

class npc_ascension_chain_boss : public CreatureScript
{
public:
    npc_ascension_chain_boss() : CreatureScript("npc_ascension_chain_boss") { }

    struct npc_ascension_chain_bossAI : public ScriptedAI
    {
        explicit npc_ascension_chain_bossAI(Creature* creature) : ScriptedAI(creature)
        {
            me->SetCorpseDelay(ASCENSION_CHAIN_BOSS_CORPSE_DELAY_SEC);

            scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
        }

        bool _phaseTwoTriggered = false;
        bool _phaseThreeTriggered = false;

        void Reset() override
        {
            scheduler.CancelAll();
            SetAutoAttackAllowed(true);
            me->SetCorpseDelay(ASCENSION_CHAIN_BOSS_CORPSE_DELAY_SEC);
            _phaseTwoTriggered = false;
            _phaseThreeTriggered = false;

            uint64 maxHealth = GetAscensionBossMaxHealth(me->GetEntry());
            if (maxHealth > 0)
            {
                uint32 clientHealth = ToAscensionClientHealth(maxHealth);
                me->SetCreateHealth(clientHealth);
                me->SetMaxHealth(clientHealth);

                if (maxHealth > clientHealth)
                {
                    me->SetExtendedMaxHealth(maxHealth);
                    me->SetExtendedHealth(maxHealth);
                    me->SyncClientHealthFromExtended();
                }
                else
                    me->SetHealth(clientHealth);
            }

            // 扩展血量首领会把核心半血伤害需求抬得过高，这里只解除奖励判定门槛，掉落仍由 creature_template.lootid 正常生成。
            me->LowerPlayerDamageReq(me->GetMaxHealthForCombat(), true);

            if (me->HasWeapon(OFF_ATTACK))
                me->SetCanDualWield(true);
            else
                me->SetCanDualWield(false);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            AscensionBossCombatProfile const* combatProfile = GetAscensionBossCombatProfile(me->GetEntry());
            if (!combatProfile)
                return;

            for (AscensionBossSpellAction const& action : combatProfile->Actions)
                ScheduleSpell(action);

            scheduler.Schedule(std::chrono::milliseconds(combatProfile->ComboCooldownMinMs), std::chrono::milliseconds(combatProfile->ComboCooldownMaxMs), [this, combatProfile](TaskContext context)
            {
                StartAscensionCombo(*combatProfile);
                context.Repeat(std::chrono::milliseconds(combatProfile->ComboCooldownMinMs), std::chrono::milliseconds(combatProfile->ComboCooldownMaxMs));
            });
        }

        void ScheduleSpell(AscensionBossSpellAction const& action)
        {
            scheduler.Schedule(std::chrono::milliseconds(action.FirstCastMinMs), std::chrono::milliseconds(action.FirstCastMaxMs), [this, action](TaskContext context)
            {
                CastSpellByProfile(action);
                context.Repeat(std::chrono::milliseconds(action.RepeatMinMs), std::chrono::milliseconds(action.RepeatMaxMs));
            });
        }

        void CastSpellByProfile(AscensionBossSpellAction const& action)
        {
            switch (action.CastType)
            {
                case AscensionBossSpellCastType::Victim:
                    DoCastVictim(action.SpellId);
                    break;
                case AscensionBossSpellCastType::Area:
                    DoCastAOE(action.SpellId);
                    break;
                case AscensionBossSpellCastType::Random:
                    DoCastRandomTarget(action.SpellId, 0, action.RandomTargetDistance, true, false, true);
                    break;
            }
        }

        void StartAscensionCombo(AscensionBossCombatProfile const& combatProfile)
        {
            scheduler.DelayAll(std::chrono::milliseconds(3200));

            ScheduleComboAction(combatProfile, 0, std::chrono::milliseconds(1));
            ScheduleComboAction(combatProfile, 1, std::chrono::milliseconds(900));
            ScheduleComboAction(combatProfile, 2, std::chrono::milliseconds(1800));

            scheduler.Schedule(std::chrono::milliseconds(2700), [this](TaskContext /*context*/)
            {
                DoAscensionTrueStrike(true);
            });
        }

        void ScheduleComboAction(AscensionBossCombatProfile const& combatProfile, uint8 comboIndex, std::chrono::milliseconds delay)
        {
            uint8 actionIndex = combatProfile.ComboActionOrder[comboIndex];
            AscensionBossSpellAction action = combatProfile.Actions[actionIndex];
            scheduler.Schedule(delay, [this, action](TaskContext /*context*/)
            {
                CastSpellByProfile(action);
            });
        }

        void DoAscensionTrueStrike(bool comboFinisher = false)
        {
            Unit* victim = me->GetVictim();
            if (!victim || !victim->IsAlive())
                return;

            uint64 damage = GetAscensionBossTrueStrikeDamage(me->GetEntry());
            if (comboFinisher)
                damage = std::max<uint64>(damage * 3, victim->GetMaxHealthForCombat() * 75 / 100);

            Unit::DealDamage(me, victim, damage, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW, nullptr, false);
        }

        void JustDied(Unit* killer) override
        {
            scheduler.CancelAll();

            ObjectGuid killerGuid = killer ? killer->GetGUID() : ObjectGuid::Empty;
            Player* killerPlayer = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
            if (killerPlayer)
            {
                AutoStoreAscensionBossLoot(me, killerPlayer);
            }

            uint32 nextBossEntry = GetNextAscensionChainBossEntry(me->GetEntry());
            if (nextBossEntry == 0)
                return;

            Position anchorPos = me->GetHomePosition();
            ObjectGuid killerPlayerGuid = killerPlayer ? killerPlayer->GetGUID() : ObjectGuid::Empty;
            uint32 mapId = me->GetMapId();
            uint32 instanceId = me->GetInstanceId();

            if (killerPlayer)
            {
                std::string nextBossName = GetAscensionBossDisplayName(nextBossEntry);
                ChatHandler handler(killerPlayer->GetSession());
                handler.SendNotification("5秒后将召唤更强大的飞升BOSS：{}。", nextBossName);
                handler.PSendSysMessage("飞升锚点已被击破，5秒后将召唤更强大的飞升BOSS：{}。", nextBossName);
            }
            else
            {
                LOG_ERROR("module", "飞升系统: 无法安排下一阶段首领召唤，没有有效击杀玩家 entry={}", nextBossEntry);
                return;
            }

            killerPlayer->m_Events.AddEventAtOffset([nextBossEntry, anchorPos, killerGuid, killerPlayerGuid, mapId, instanceId]()
            {
                Player* player = ObjectAccessor::FindPlayer(killerPlayerGuid);
                if (!player || !player->IsInWorld() || player->GetMapId() != mapId || player->GetInstanceId() != instanceId)
                    return;

                Creature* nextBoss = player->SummonCreature(nextBossEntry, anchorPos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, ASCENSION_CHAIN_SUMMON_MS);
                if (!nextBoss)
                {
                    LOG_ERROR("module", "飞升系统: 无法召唤下一阶段首领 entry={}", nextBossEntry);
                    return;
                }

                nextBoss->SetHomePosition(anchorPos);

                if (!killerGuid.IsEmpty())
                {
                    if (Unit* killerUnit = ObjectAccessor::GetUnit(*player, killerGuid))
                    {
                        nextBoss->SetInCombatWith(killerUnit);
                        nextBoss->AddThreat(killerUnit, 1000.0f);
                        if (nextBoss->IsAIEnabled)
                            nextBoss->AI()->AttackStart(killerUnit);
                    }
                }
            }, std::chrono::seconds(5));
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            AscensionBossCombatProfile const* combatProfile = GetAscensionBossCombatProfile(me->GetEntry());
            if (combatProfile)
            {
                if (!_phaseTwoTriggered && me->HealthBelowPct(70))
                {
                    _phaseTwoTriggered = true;
                    StartAscensionCombo(*combatProfile);
                }

                if (!_phaseThreeTriggered && me->HealthBelowPct(40))
                {
                    _phaseThreeTriggered = true;
                    StartAscensionCombo(*combatProfile);
                    scheduler.Schedule(std::chrono::milliseconds(3400), [this](TaskContext /*context*/)
                    {
                        DoAscensionTrueStrike(true);
                    });
                }
            }

            scheduler.Update(diff, [this]
            {
                if (IsAutoAttackAllowed())
                    DoMeleeAttackIfReady();
            });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        if (!creature || !IsAscensionChainBossEntry(creature->GetEntry()))
            return nullptr;

        return new npc_ascension_chain_bossAI(creature);
    }
};

//=============================================================================
// 脚本加载函数
//=============================================================================

void AddAscensionSystemScripts()
{
    new AscensionWorldScript();
    new AscensionPlayerScript();
    new AscensionUnitScript();
    new AscensionCommandScript();
    new AscensionItemScript();
    new npc_ascension_chain_boss();
}

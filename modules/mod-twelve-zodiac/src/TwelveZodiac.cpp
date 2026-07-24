/*
 * 十二生肖系统（mod-twelve-zodiac）
 *
 * 设计约束：
 *   1. 只使用两张自定义表：世界库 `_十二生肖控制`、角色库 `_十二生肖玩家`。
 *   2. 冷却和倍率全部从世界库读取；冷却计时只保存在运行内存，不再增加第三张表。
 *   3. 大数计算使用整数比例和 uint256；概率也按整数百分比处理，不经过浮点数。
 */

#include "ScriptMgr.h"
#include "AsyncCallbackProcessor.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "HermesBridgeAddonApi.h"
#include "TwelveZodiacHermesApi.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "Util.h"
#include "AddonThrottle.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

using namespace Acore::ChatCommands;

namespace
{
constexpr uint8 ZODIAC_FIRST_ID = 1;
constexpr uint8 ZODIAC_LAST_ID = 12;
constexpr uint8 ZODIAC_GLOBAL_ID = 0;
constexpr uint8 ZODIAC_INACTIVE_SLOT = 255;
constexpr uint256 ZODIAC_PERCENT_BASE = 100;
constexpr uint32 ZODIAC_DEFAULT_REQUIRED_SKILL = 450;
constexpr uint32 ZODIAC_DEFAULT_TRIGGER_COOLDOWN_MS = 5000;
constexpr uint32 ZODIAC_DEFERRED_LOAD_DELAY_MS = 3000;
constexpr uint32 ZODIAC_PLAYER_REFRESH_BATCH_SIZE = 5;
constexpr uint8 ZODIAC_DEFAULT_MAX_SLOTS = 5;
constexpr char const* ZODIAC_ADDON_PREFIX = "ZODIACUI";
constexpr char const* ZODIAC_OPEN_ADDON_PREFIX = "ZODIACOPEN";
constexpr char const* ZODIAC_DAMAGE_ADDON_PREFIX = "ZODIACDMG";
constexpr size_t ZODIAC_ADDON_CHUNK_SIZE = 180;
constexpr char const* ZODIAC_CONTROL_SELECT_SQL =
    "SELECT `生肖ID`, `生肖名称`, `专业技能ID`, `启用`, `解锁技能等级`, `基础成长加成百分比`, "
    "`最终伤害倍率`, `PVP最终伤害倍率`, `最终治疗倍率`, `PVP最终治疗倍率`, `触发模式`, "
    "`触发概率百分比`, `触发冷却毫秒`, `触发伤害倍率`, `触发治疗倍率`, `斩杀阈值百分比`, "
    "`死亡保护冷却毫秒`, `死亡保护生命百分比`, `三合组`, `三合伤害倍率`, `三合治疗倍率`, "
    "`命宫槽位上限`, `满套成长倍率`, `满套伤害倍率`, `满套治疗倍率`, `描述` "
    "FROM `_十二生肖控制` ORDER BY `生肖ID`";

constexpr std::array<uint32, 14> ZODIAC_TEST_SKILL_IDS =
{
    186, 333, 171, 755, 164, 202, 197, 129, 185, 182, 165, 393, 356, 773
};

constexpr std::array<uint32, 14> ZODIAC_TEST_FIRST_RANK_SPELL_IDS =
{
    2575, 7411, 2259, 25229, 2018, 4036, 3908, 3273, 2550, 2366, 2108, 8613, 7620, 45357
};

// AddOn 协议：
//   客户端 -> GET / DETAIL:<生肖ID> / EQUIP:<生肖ID>:<槽位> / UNEQUIP:<槽位>
//   服务端 -> STATE_BEGIN + Z 行 + STATE_END，或 DETAIL 行；超过单包长度时使用 CHUNK:i:n。

enum ZodiacTriggerMode : uint8
{
    ZODIAC_TRIGGER_NONE = 0,
    ZODIAC_TRIGGER_DAMAGE = 1,
    ZODIAC_TRIGGER_HEAL = 2,
    ZODIAC_TRIGGER_DAMAGE_AND_HEAL = 3
};

uint256 Decimal65Max()
{
    return Acore::Number::GetDecimal65UnsignedMax();
}

uint256 AddSaturated(uint256 const& left, uint256 const& right)
{
    uint256 const maxValue = Decimal65Max();
    if (left >= maxValue || right >= maxValue - left)
        return maxValue;

    return left + right;
}

// 精确执行 value * multiplier / 100，避免 65 位数经过 long double 后丢失有效数字。
uint256 ScaleByMultiplier(uint256 const& value, uint256 const& multiplier)
{
    if (!value || !multiplier)
        return 0;

    boost::multiprecision::cpp_int product = value;
    product *= multiplier;
    product /= 100;

    boost::multiprecision::cpp_int const maxValue = Decimal65Max();
    if (product >= maxValue)
        return Decimal65Max();

    return static_cast<uint256>(product);
}

uint256 AddPercent(uint256 const& value, uint256 const& bonusPercent)
{
    return ScaleByMultiplier(value, AddSaturated(ZODIAC_PERCENT_BASE, bonusPercent));
}

uint256 ToUInt256Positive(int256 const& value)
{
    if (value <= 0)
        return 0;

    return std::min<uint256>(Acore::Number::ToUInt256Saturated(value), Decimal65Max());
}

void ApplyMultiplier(int256& value, uint256 const& multiplier)
{
    if (value <= 0 || multiplier == ZODIAC_PERCENT_BASE)
        return;

    value = Acore::Number::ToInt256Saturated(ScaleByMultiplier(ToUInt256Positive(value), multiplier));
}

bool IsPvpContext(Unit* source, Unit* target)
{
    if (!source || !target)
        return false;

    if (target->ToPlayer() || source->ToPlayer() && source->GetMap() && source->GetMap()->IsBattlegroundOrArena())
        return true;

    return source->GetMap() && source->GetMap()->IsBattlegroundOrArena();
}

bool RollConfiguredChance(uint256 const& chance)
{
    if (!chance)
        return false;

    if (chance >= ZODIAC_PERCENT_BASE)
        return true;

    // 触发概率字段是 DECIMAL(65,0)，因此直接使用整数百分比，避免大数转浮点。
    return roll_chance_i(static_cast<int32>(Acore::Number::ToUInt32Saturated(chance)));
}

bool IsReady(uint64 now, uint64 nextReady)
{
    return !nextReady || now >= nextReady;
}

std::string ToAddonNumber(uint256 const& value)
{
    return value.convert_to<std::string>();
}

uint256 CalculateEffectiveMultiplierPercent(uint256 const& originalDamage, uint256 const& finalDamage)
{
    if (!originalDamage || !finalDamage)
        return 0;

    boost::multiprecision::cpp_int multiplier = finalDamage;
    multiplier *= ZODIAC_PERCENT_BASE;
    multiplier /= originalDamage;

    boost::multiprecision::cpp_int const maxValue = Decimal65Max();
    if (multiplier >= maxValue)
        return Decimal65Max();

    return static_cast<uint256>(multiplier);
}

std::string SanitizeAddonField(std::string value)
{
    for (char& character : value)
    {
        if (character == '|' || character == '\n' || character == '\r' || character == '\t')
            character = ' ';
    }

    return value;
}

void SendZodiacAddonPayload(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession() || payload.empty())
        return;

    auto sendOne = [player](std::string const& message)
    {
        HermesBridge_SendAddonMessage(player, ZODIAC_ADDON_PREFIX, message);
    };

    if (payload.size() <= ZODIAC_ADDON_CHUNK_SIZE)
    {
        sendOne(payload);
        return;
    }

    size_t const totalChunks = (payload.size() + ZODIAC_ADDON_CHUNK_SIZE - 1) / ZODIAC_ADDON_CHUNK_SIZE;
    for (size_t index = 0; index < totalChunks; ++index)
    {
        size_t const start = index * ZODIAC_ADDON_CHUNK_SIZE;
        size_t const length = std::min(ZODIAC_ADDON_CHUNK_SIZE, payload.size() - start);
        std::ostringstream chunk;
        chunk << "CHUNK:" << (index + 1) << ':' << totalChunks << ':' << payload.substr(start, length);
        sendOne(chunk.str());
    }
}

void SendZodiacDamagePayload(Player* player, uint256 const& originalDamage, uint256 const& finalDamage,
    uint256 const& effectiveMultiplierPercent, bool triggerApplied, bool triadApplied, bool fullSet,
    SpellInfo const* spellInfo)
{
    if (!player || !player->GetSession() || !originalDamage || !finalDamage)
        return;

    std::ostringstream payload;
    payload << "HIT|"
            << ToAddonNumber(originalDamage) << '|'
            << ToAddonNumber(finalDamage) << '|'
            << ToAddonNumber(effectiveMultiplierPercent) << '|'
            << (triggerApplied ? 1 : 0) << '|'
            << (triadApplied ? 1 : 0) << '|'
            << (fullSet ? 1 : 0) << '|'
            << (spellInfo ? spellInfo->Id : 0);
    HermesBridge_SendAddonMessage(player, ZODIAC_DAMAGE_ADDON_PREFIX, payload.str());
}

void LearnAllProfessionRecipes(Player* player, uint32 skillId)
{
    if (!player)
        return;

    uint32 const classMask = player->getClassMask();
    for (SkillLineAbilityEntry const* skillLine : GetSkillLineAbilitiesBySkillLine(skillId))
    {
        if (!skillLine || skillLine->SupercededBySpell || skillLine->RaceMask != 0)
            continue;

        if (skillLine->ClassMask && (skillLine->ClassMask & classMask) == 0)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skillLine->Spell);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo))
            continue;

        player->learnSpell(skillLine->Spell);
    }
}

struct ZodiacControl
{
    bool valid = false;
    uint8 id = 0;
    std::string name;
    uint32 professionSkillId = 0;
    bool enabled = false;
    uint32 requiredSkill = ZODIAC_DEFAULT_REQUIRED_SKILL;

    uint256 growthBonusPercent = 0;
    uint256 finalDamageMultiplier = ZODIAC_PERCENT_BASE;
    uint256 pvpFinalDamageMultiplier = ZODIAC_PERCENT_BASE;
    uint256 finalHealMultiplier = ZODIAC_PERCENT_BASE;
    uint256 pvpFinalHealMultiplier = ZODIAC_PERCENT_BASE;

    uint8 triggerMode = ZODIAC_TRIGGER_NONE;
    uint256 triggerChancePercent = 0;
    uint32 triggerCooldownMs = ZODIAC_DEFAULT_TRIGGER_COOLDOWN_MS;
    uint256 triggerDamageMultiplier = ZODIAC_PERCENT_BASE;
    uint256 triggerHealMultiplier = ZODIAC_PERCENT_BASE;

    uint256 executeThresholdPercent = 0;
    uint32 deathSaveCooldownMs = 0;
    uint256 deathSaveHealthPercent = 10;

    uint8 triadGroup = 0;
    uint256 triadDamageMultiplier = ZODIAC_PERCENT_BASE;
    uint256 triadHealMultiplier = ZODIAC_PERCENT_BASE;
    std::string description;
};

struct ZodiacGlobalConfig
{
    bool enabled = true;
    uint8 maxSlots = ZODIAC_DEFAULT_MAX_SLOTS;
    uint256 fullSetGrowthMultiplier = 101;
    uint256 fullSetDamageMultiplier = ZODIAC_PERCENT_BASE;
    uint256 fullSetHealMultiplier = ZODIAC_PERCENT_BASE;
};

struct ZodiacProgress
{
    bool unlocked = false;
    uint8 star = 1;
    uint256 cultivation = 0;
    uint8 slot = ZODIAC_INACTIVE_SLOT;
    uint8 selectedPrimaryStat = 0;
};

struct ZodiacRuntime
{
    std::array<ZodiacProgress, ZODIAC_LAST_ID + 1> progress{};
    std::array<uint64, ZODIAC_LAST_ID + 1> nextDamageReady{};
    std::array<uint64, ZODIAC_LAST_ID + 1> nextHealReady{};
    std::array<uint64, ZODIAC_LAST_ID + 1> nextDeathSaveReady{};
    bool dirty = false;
};

struct ZodiacCombinationRating
{
    std::array<uint8, ZODIAC_DEFAULT_MAX_SLOTS> ids{};
    uint256 damageScore = 0;
    uint256 healScore = 0;
    uint8 deathSaveCount = 0;
    uint256 executeThreshold = 0;
};

std::string JoinZodiacIds(std::array<uint8, ZODIAC_DEFAULT_MAX_SLOTS> const& ids)
{
    std::ostringstream value;
    for (size_t index = 0; index < ids.size(); ++index)
    {
        if (index)
            value << ',';
        if (ids[index])
            value << static_cast<uint32>(ids[index]);
    }
    return value.str();
}

class TwelveZodiacManager
{
public:
    void ApplyControls(QueryResult result);
    void QueueOnlinePlayerRefresh();
    void ProcessPendingPlayerRefresh(uint32 maxPlayers);
    void LoadPlayer(Player* player);
    void SavePlayer(Player* player);
    void DeletePlayer(uint32 guid);
    void ClearPlayer(uint32 guid);

    void SyncProfessionUnlocks(Player* player, bool notify);
    void OnSkillUpdate(Player* player, uint32 skillId, uint32 value, uint32 newValue);

    void ApplyGrowth(Player* player, int256& healingBonus, int256 spellDamage[MAX_SPELL_SCHOOL]);
    void ApplyRating(Player* player, int256& amount);
    void ApplyDamage(Player* player, Unit* victim, uint256& damage,
        DamageEffectType damageType, SpellInfo const* spellInfo);
    void ApplyHeal(Player* player, Unit* receiver, uint256& heal);
    bool TryDeathSave(Unit* victim);

    bool Equip(Player* player, uint8 zodiacId, uint8 slot, bool sendUi = true);
    bool Unequip(Player* player, uint8 slot, bool sendUi = true);
    void NormalizeSlots(Player* player, bool autoFill);
    void SendInfo(ChatHandler* handler, Player* player) const;
    void SendUiState(Player* player) const;
    void SendUiDetail(Player* player, uint8 zodiacId) const;
    void HandleAddonCommand(Player* player, std::string const& command);
    std::string GetHermesState(Player* player) const;
    std::string GetHermesDetail(Player* player, uint32 zodiacId) const;
    std::string ExecuteHermesEquip(Player* player, uint32 zodiacId, uint32 slot);
    std::string ExecuteHermesUnequip(Player* player, uint32 slot);

    ZodiacControl const* GetControl(uint8 id) const
    {
        if (id > ZODIAC_LAST_ID || !_controls[id].valid)
            return nullptr;

        return &_controls[id];
    }

    ZodiacRuntime* GetRuntime(uint32 guid)
    {
        auto itr = _players.find(guid);
        return itr == _players.end() ? nullptr : &itr->second;
    }

    bool IsLoaded() const { return _loaded; }

private:
    ZodiacRuntime* GetRuntime(Player* player)
    {
        return player ? GetRuntime(player->GetGUID().GetCounter()) : nullptr;
    }

    uint8 CountUnlocked(ZodiacRuntime const& runtime) const;
    uint8 GetAllowedSlots(ZodiacRuntime const& runtime) const;
    bool IsFullSet(ZodiacRuntime const& runtime) const;
    uint256 GetGrowthMultiplier(ZodiacRuntime const& runtime) const;
    uint256 GetFullSetMultiplier(bool damage) const;
    bool IsActive(ZodiacRuntime const& runtime, uint8 zodiacId) const;
    uint8 FindSlot(ZodiacRuntime const& runtime, uint8 zodiacId) const;
    bool TryTriggerDamage(Player* player, Unit* victim, uint256& damage, ZodiacRuntime& runtime);
    bool TryTriggerHeal(Player* player, Unit* receiver, uint256& heal, ZodiacRuntime& runtime);
    void ApplyTriadDamage(ZodiacRuntime const& runtime, uint256& damage) const;
    void ApplyTriadHeal(ZodiacRuntime const& runtime, uint256& heal) const;
    bool IsBelowExecuteThreshold(Unit* victim, uint256 const& thresholdPercent) const;
    ZodiacCombinationRating EvaluateCombination(
        std::array<uint8, ZODIAC_DEFAULT_MAX_SLOTS> const& ids) const;
    void FindBestCombinations(
        ZodiacCombinationRating& damageRecommendation,
        ZodiacCombinationRating& healRecommendation,
        ZodiacCombinationRating& survivalRecommendation) const;
    std::string BuildUiState(Player* player) const;
    std::string BuildUiDetail(Player* player, uint8 zodiacId) const;

    std::array<ZodiacControl, ZODIAC_LAST_ID + 1> _controls{};
    ZodiacGlobalConfig _global;
    std::unordered_map<uint32, ZodiacRuntime> _players;
    std::vector<ObjectGuid> _pendingRefreshPlayers;
    size_t _pendingRefreshIndex = 0;
    bool _loaded = false;
};

TwelveZodiacManager s_twelveZodiacManager;

void TwelveZodiacManager::ApplyControls(QueryResult result)
{
    if (!result)
    {
        LOG_ERROR("server.loading", "→十二生肖系统× 无法读取世界库 `_十二生肖控制`，保留当前配置。");
        return;
    }

    std::array<ZodiacControl, ZODIAC_LAST_ID + 1> controls{};
    ZodiacGlobalConfig global;
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint8 const id = fields[0].Get<uint8>();
        if (id > ZODIAC_LAST_ID)
            continue;

        ZodiacControl control;
        control.valid = true;
        control.id = id;
        control.name = fields[1].Get<std::string>();
        control.professionSkillId = fields[2].Get<uint32>();
        control.enabled = fields[3].Get<uint8>() != 0;
        control.requiredSkill = fields[4].Get<uint32>();
        if (!control.requiredSkill)
            control.requiredSkill = ZODIAC_DEFAULT_REQUIRED_SKILL;

        control.growthBonusPercent = fields[5].Get<uint256>();
        control.finalDamageMultiplier = fields[6].Get<uint256>();
        control.pvpFinalDamageMultiplier = fields[7].Get<uint256>();
        control.finalHealMultiplier = fields[8].Get<uint256>();
        control.pvpFinalHealMultiplier = fields[9].Get<uint256>();
        control.triggerMode = fields[10].Get<uint8>();
        control.triggerChancePercent = fields[11].Get<uint256>();
        control.triggerCooldownMs = fields[12].Get<uint32>();
        control.triggerDamageMultiplier = fields[13].Get<uint256>();
        control.triggerHealMultiplier = fields[14].Get<uint256>();
        control.executeThresholdPercent = fields[15].Get<uint256>();
        control.deathSaveCooldownMs = fields[16].Get<uint32>();
        control.deathSaveHealthPercent = fields[17].Get<uint256>();
        control.triadGroup = fields[18].Get<uint8>();
        control.triadDamageMultiplier = fields[19].Get<uint256>();
        control.triadHealMultiplier = fields[20].Get<uint256>();
        control.description = fields[25].Get<std::string>();

        if (id == ZODIAC_GLOBAL_ID)
        {
            global.enabled = control.enabled;
            global.maxSlots = fields[21].Get<uint8>();
            if (!global.maxSlots)
                global.maxSlots = ZODIAC_DEFAULT_MAX_SLOTS;
            global.maxSlots = std::min<uint8>(global.maxSlots, ZODIAC_DEFAULT_MAX_SLOTS);
            global.fullSetGrowthMultiplier = fields[22].Get<uint256>();
            global.fullSetDamageMultiplier = fields[23].Get<uint256>();
            global.fullSetHealMultiplier = fields[24].Get<uint256>();
            if (!global.fullSetGrowthMultiplier)
                global.fullSetGrowthMultiplier = 101;
            if (!global.fullSetDamageMultiplier)
                global.fullSetDamageMultiplier = ZODIAC_PERCENT_BASE;
            if (!global.fullSetHealMultiplier)
                global.fullSetHealMultiplier = ZODIAC_PERCENT_BASE;
        }
        else
        {
            controls[id] = std::move(control);
            ++count;
        }
    } while (result->NextRow());

    _controls = std::move(controls);
    _global = std::move(global);
    _loaded = _global.enabled && count > 0;
    LOG_INFO("server.loading", "→十二生肖系统√ 生肖={} 全局启用={} 命宫槽位={} 满套成长倍率={}",
        count, _global.enabled ? 1 : 0, _global.maxSlots, _global.fullSetGrowthMultiplier);

    QueueOnlinePlayerRefresh();
}

void TwelveZodiacManager::QueueOnlinePlayerRefresh()
{
    _pendingRefreshPlayers.clear();
    _pendingRefreshIndex = 0;

    {
        std::shared_lock<std::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
            if (player)
                _pendingRefreshPlayers.push_back(guid);
    }
}

void TwelveZodiacManager::ProcessPendingPlayerRefresh(uint32 maxPlayers)
{
    uint32 processed = 0;
    while (processed < maxPlayers && _pendingRefreshIndex < _pendingRefreshPlayers.size())
    {
        ObjectGuid const guid = _pendingRefreshPlayers[_pendingRefreshIndex++];
        ++processed;

        Player* player = ObjectAccessor::FindConnectedPlayer(guid);
        if (!player)
            continue;

        ZodiacRuntime* runtime = GetRuntime(player);
        if (!runtime)
            continue;

        // 重载后立即采用新的倍率/冷却配置，不保留旧配置留下的内存冷却。
        runtime->nextDamageReady.fill(0);
        runtime->nextHealReady.fill(0);
        runtime->nextDeathSaveReady.fill(0);

        SyncProfessionUnlocks(player, false);
        NormalizeSlots(player, true);
        SavePlayer(player);
        player->UpdateAllStats();
        SendUiState(player);
    }

    if (_pendingRefreshIndex >= _pendingRefreshPlayers.size())
    {
        _pendingRefreshPlayers.clear();
        _pendingRefreshIndex = 0;
    }
}

void TwelveZodiacManager::LoadPlayer(Player* player)
{
    if (!player)
        return;

    uint32 const guid = player->GetGUID().GetCounter();
    ZodiacRuntime runtime;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `生肖ID`, `已解锁`, `星级`, `修为`, `命宫槽位`, `主属性选择` "
        "FROM `_十二生肖玩家` WHERE `玩家GUID` = {} ORDER BY `生肖ID`", guid);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const zodiacId = fields[0].Get<uint8>();
            if (zodiacId < ZODIAC_FIRST_ID || zodiacId > ZODIAC_LAST_ID)
                continue;

            ZodiacProgress& progress = runtime.progress[zodiacId];
            progress.unlocked = fields[1].Get<uint8>() != 0;
            progress.star = std::max<uint8>(1, fields[2].Get<uint8>());
            progress.cultivation = fields[3].Get<uint256>();
            progress.slot = fields[4].Get<uint8>();
            progress.selectedPrimaryStat = fields[5].Get<uint8>();
        } while (result->NextRow());
    }

    _players[guid] = std::move(runtime);
}

void TwelveZodiacManager::SavePlayer(Player* player)
{
    if (!player)
        return;

    uint32 const guid = player->GetGUID().GetCounter();
    ZodiacRuntime* runtime = GetRuntime(guid);
    if (!runtime)
        return;

    if (!runtime->dirty)
        return;

    for (uint8 zodiacId = ZODIAC_FIRST_ID; zodiacId <= ZODIAC_LAST_ID; ++zodiacId)
    {
        ZodiacProgress const& progress = runtime->progress[zodiacId];
        CharacterDatabase.Execute(
            "REPLACE INTO `_十二生肖玩家` "
            "(`玩家GUID`, `生肖ID`, `已解锁`, `星级`, `修为`, `命宫槽位`, `主属性选择`) "
            "VALUES ({}, {}, {}, {}, '{}', {}, {})",
            guid, zodiacId, progress.unlocked ? 1 : 0, progress.star,
            progress.cultivation.convert_to<std::string>(), progress.slot, progress.selectedPrimaryStat);
    }

    runtime->dirty = false;
}

void TwelveZodiacManager::DeletePlayer(uint32 guid)
{
    CharacterDatabase.Execute("DELETE FROM `_十二生肖玩家` WHERE `玩家GUID` = {}", guid);
    _players.erase(guid);
}

void TwelveZodiacManager::ClearPlayer(uint32 guid)
{
    _players.erase(guid);
}

ZodiacCombinationRating TwelveZodiacManager::EvaluateCombination(
    std::array<uint8, ZODIAC_DEFAULT_MAX_SLOTS> const& ids) const
{
    ZodiacCombinationRating rating;
    rating.ids = ids;
    rating.damageScore = ZODIAC_PERCENT_BASE;
    rating.healScore = ZODIAC_PERCENT_BASE;

    std::array<uint8, 5> triadCounts{};
    std::array<uint256, 5> triadDamageMultipliers{};
    std::array<uint256, 5> triadHealMultipliers{};
    for (uint256& multiplier : triadDamageMultipliers)
        multiplier = ZODIAC_PERCENT_BASE;
    for (uint256& multiplier : triadHealMultipliers)
        multiplier = ZODIAC_PERCENT_BASE;

    uint256 bestTriggerDamage = ZODIAC_PERCENT_BASE;
    uint256 bestTriggerHeal = ZODIAC_PERCENT_BASE;
    for (uint8 id : ids)
    {
        ZodiacControl const* control = GetControl(id);
        if (!control || !control->enabled)
        {
            rating.damageScore = 0;
            rating.healScore = 0;
            rating.deathSaveCount = 0;
            return rating;
        }

        rating.damageScore = ScaleByMultiplier(rating.damageScore, control->finalDamageMultiplier);
        rating.healScore = ScaleByMultiplier(rating.healScore, control->finalHealMultiplier);
        rating.executeThreshold = std::max(rating.executeThreshold, control->executeThresholdPercent);

        if (control->deathSaveCooldownMs && control->deathSaveHealthPercent)
            ++rating.deathSaveCount;

        if (control->triggerChancePercent && (control->triggerMode & ZODIAC_TRIGGER_DAMAGE))
            bestTriggerDamage = std::max(bestTriggerDamage, control->triggerDamageMultiplier);
        if (control->triggerChancePercent && (control->triggerMode & ZODIAC_TRIGGER_HEAL))
            bestTriggerHeal = std::max(bestTriggerHeal, control->triggerHealMultiplier);

        if (control->triadGroup && control->triadGroup < triadCounts.size())
        {
            ++triadCounts[control->triadGroup];
            triadDamageMultipliers[control->triadGroup] =
                std::max(triadDamageMultipliers[control->triadGroup], control->triadDamageMultiplier);
            triadHealMultipliers[control->triadGroup] =
                std::max(triadHealMultipliers[control->triadGroup], control->triadHealMultiplier);
        }
    }

    uint256 bestTriadDamage = ZODIAC_PERCENT_BASE;
    uint256 bestTriadHeal = ZODIAC_PERCENT_BASE;
    for (uint8 group = 1; group < triadCounts.size(); ++group)
    {
        if (triadCounts[group] < 3)
            continue;
        bestTriadDamage = std::max(bestTriadDamage, triadDamageMultipliers[group]);
        bestTriadHeal = std::max(bestTriadHeal, triadHealMultipliers[group]);
    }

    rating.damageScore = ScaleByMultiplier(rating.damageScore, bestTriadDamage);
    rating.damageScore = ScaleByMultiplier(rating.damageScore, bestTriggerDamage);
    rating.healScore = ScaleByMultiplier(rating.healScore, bestTriadHeal);
    rating.healScore = ScaleByMultiplier(rating.healScore, bestTriggerHeal);
    return rating;
}

void TwelveZodiacManager::FindBestCombinations(
    ZodiacCombinationRating& damageRecommendation,
    ZodiacCombinationRating& healRecommendation,
    ZodiacCombinationRating& survivalRecommendation) const
{
    std::vector<uint8> enabledIds;
    enabledIds.reserve(ZODIAC_LAST_ID);
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (control.valid && control.enabled)
            enabledIds.push_back(id);
    }

    if (enabledIds.size() < ZODIAC_DEFAULT_MAX_SLOTS)
        return;

    auto preferLowerIds = [](ZodiacCombinationRating const& candidate, ZodiacCombinationRating const& current)
    {
        return !current.ids[0] || candidate.ids < current.ids;
    };
    auto betterDamage = [&preferLowerIds](ZodiacCombinationRating const& candidate, ZodiacCombinationRating const& current)
    {
        if (candidate.damageScore != current.damageScore)
            return candidate.damageScore > current.damageScore;
        if (candidate.executeThreshold != current.executeThreshold)
            return candidate.executeThreshold > current.executeThreshold;
        if (candidate.deathSaveCount != current.deathSaveCount)
            return candidate.deathSaveCount > current.deathSaveCount;
        if (candidate.healScore != current.healScore)
            return candidate.healScore > current.healScore;
        return preferLowerIds(candidate, current);
    };
    auto betterHeal = [&preferLowerIds](ZodiacCombinationRating const& candidate, ZodiacCombinationRating const& current)
    {
        if (candidate.healScore != current.healScore)
            return candidate.healScore > current.healScore;
        if (candidate.deathSaveCount != current.deathSaveCount)
            return candidate.deathSaveCount > current.deathSaveCount;
        if (candidate.damageScore != current.damageScore)
            return candidate.damageScore > current.damageScore;
        return preferLowerIds(candidate, current);
    };
    auto betterSurvival = [&preferLowerIds](ZodiacCombinationRating const& candidate, ZodiacCombinationRating const& current)
    {
        if (candidate.deathSaveCount != current.deathSaveCount)
            return candidate.deathSaveCount > current.deathSaveCount;
        if (candidate.healScore != current.healScore)
            return candidate.healScore > current.healScore;
        if (candidate.damageScore != current.damageScore)
            return candidate.damageScore > current.damageScore;
        return preferLowerIds(candidate, current);
    };

    size_t const count = enabledIds.size();
    for (size_t first = 0; first + 4 < count; ++first)
    for (size_t second = first + 1; second + 3 < count; ++second)
    for (size_t third = second + 1; third + 2 < count; ++third)
    for (size_t fourth = third + 1; fourth + 1 < count; ++fourth)
    for (size_t fifth = fourth + 1; fifth < count; ++fifth)
    {
        std::array<uint8, ZODIAC_DEFAULT_MAX_SLOTS> const ids =
        {
            enabledIds[first], enabledIds[second], enabledIds[third], enabledIds[fourth], enabledIds[fifth]
        };
        ZodiacCombinationRating const candidate = EvaluateCombination(ids);
        if (betterDamage(candidate, damageRecommendation))
            damageRecommendation = candidate;
        if (betterHeal(candidate, healRecommendation))
            healRecommendation = candidate;
        if (betterSurvival(candidate, survivalRecommendation))
            survivalRecommendation = candidate;
    }
}

std::string TwelveZodiacManager::BuildUiState(Player* player) const
{
    if (!player)
        return "ERROR|NO_PLAYER";

    auto const itr = _players.find(player->GetGUID().GetCounter());
    if (itr == _players.end())
        return "ERROR|玩家数据尚未加载";

    ZodiacRuntime const& runtime = itr->second;
    std::ostringstream payload;
    payload << "STATE_BEGIN|" << (_global.enabled ? 1 : 0)
            << '|' << static_cast<uint32>(_global.maxSlots)
            << '|' << ToAddonNumber(_global.fullSetGrowthMultiplier)
            << '|' << ToAddonNumber(_global.fullSetDamageMultiplier)
            << '|' << ToAddonNumber(_global.fullSetHealMultiplier)
            << '|' << static_cast<uint32>(CountUnlocked(runtime))
            << '|' << static_cast<uint32>(GetAllowedSlots(runtime))
            << '|' << (IsFullSet(runtime) ? 1 : 0)
            << '|' << ToAddonNumber(GetGrowthMultiplier(runtime)) << '\n';

    for (uint8 zodiacId = ZODIAC_FIRST_ID; zodiacId <= ZODIAC_LAST_ID; ++zodiacId)
    {
        ZodiacControl const& control = _controls[zodiacId];
        ZodiacProgress const& progress = runtime.progress[zodiacId];
        uint32 const skillValue = control.professionSkillId ? player->GetSkillValue(control.professionSkillId) : 0;
        int32 const slot = progress.slot == ZODIAC_INACTIVE_SLOT ? -1 : static_cast<int32>(progress.slot);

        payload << "Z|" << static_cast<uint32>(zodiacId)
                << '|' << SanitizeAddonField(control.name)
                << '|' << control.professionSkillId
                << '|' << control.requiredSkill
                << '|' << skillValue
                << '|' << (control.valid && control.enabled ? 1 : 0)
                << '|' << (progress.unlocked ? 1 : 0)
                << '|' << slot
                << '|' << static_cast<uint32>(progress.star)
                << '|' << ToAddonNumber(progress.cultivation)
                << '|' << static_cast<uint32>(control.triadGroup) << '\n';
    }

    ZodiacCombinationRating damageRecommendation;
    ZodiacCombinationRating healRecommendation;
    ZodiacCombinationRating survivalRecommendation;
    FindBestCombinations(damageRecommendation, healRecommendation, survivalRecommendation);
    payload << "GUIDE|" << JoinZodiacIds(damageRecommendation.ids)
            << '|' << ToAddonNumber(damageRecommendation.damageScore)
            << '|' << ToAddonNumber(damageRecommendation.healScore)
            << '|' << static_cast<uint32>(damageRecommendation.deathSaveCount)
            << '|' << JoinZodiacIds(healRecommendation.ids)
            << '|' << ToAddonNumber(healRecommendation.damageScore)
            << '|' << ToAddonNumber(healRecommendation.healScore)
            << '|' << static_cast<uint32>(healRecommendation.deathSaveCount)
            << '|' << JoinZodiacIds(survivalRecommendation.ids)
            << '|' << ToAddonNumber(survivalRecommendation.damageScore)
            << '|' << ToAddonNumber(survivalRecommendation.healScore)
            << '|' << static_cast<uint32>(survivalRecommendation.deathSaveCount) << '\n';

    payload << "STATE_END";
    return payload.str();
}

std::string TwelveZodiacManager::BuildUiDetail(Player* player, uint8 zodiacId) const
{
    if (!player)
        return "ERROR|NO_PLAYER";

    auto const itr = _players.find(player->GetGUID().GetCounter());
    ZodiacControl const* control = GetControl(zodiacId);
    if (itr == _players.end() || !control)
        return "ERROR|生肖配置不存在";

    ZodiacProgress const& progress = itr->second.progress[zodiacId];
    uint32 const skillValue = control->professionSkillId ? player->GetSkillValue(control->professionSkillId) : 0;
    int32 const slot = progress.slot == ZODIAC_INACTIVE_SLOT ? -1 : static_cast<int32>(progress.slot);

    std::ostringstream payload;
    payload << "DETAIL|" << static_cast<uint32>(zodiacId)
            << '|' << SanitizeAddonField(control->name)
            << '|' << control->professionSkillId
            << '|' << control->requiredSkill
            << '|' << skillValue
            << '|' << (control->valid && control->enabled ? 1 : 0)
            << '|' << (progress.unlocked ? 1 : 0)
            << '|' << slot
            << '|' << static_cast<uint32>(progress.star)
            << '|' << ToAddonNumber(progress.cultivation)
            << '|' << ToAddonNumber(control->growthBonusPercent)
            << '|' << ToAddonNumber(control->finalDamageMultiplier)
            << '|' << ToAddonNumber(control->pvpFinalDamageMultiplier)
            << '|' << ToAddonNumber(control->finalHealMultiplier)
            << '|' << ToAddonNumber(control->pvpFinalHealMultiplier)
            << '|' << static_cast<uint32>(control->triggerMode)
            << '|' << ToAddonNumber(control->triggerChancePercent)
            << '|' << control->triggerCooldownMs
            << '|' << ToAddonNumber(control->triggerDamageMultiplier)
            << '|' << ToAddonNumber(control->triggerHealMultiplier)
            << '|' << ToAddonNumber(control->executeThresholdPercent)
            << '|' << control->deathSaveCooldownMs
            << '|' << ToAddonNumber(control->deathSaveHealthPercent)
            << '|' << static_cast<uint32>(control->triadGroup)
            << '|' << ToAddonNumber(control->triadDamageMultiplier)
            << '|' << ToAddonNumber(control->triadHealMultiplier)
            << '|' << SanitizeAddonField(control->description);
    return payload.str();
}

void TwelveZodiacManager::SendUiState(Player* player) const
{
    if (!_loaded || !player)
        return;

    SendZodiacAddonPayload(player, BuildUiState(player));
}

void TwelveZodiacManager::SendUiDetail(Player* player, uint8 zodiacId) const
{
    if (!_loaded || !player)
        return;

    SendZodiacAddonPayload(player, BuildUiDetail(player, zodiacId));
}

std::string TwelveZodiacManager::GetHermesState(Player* player) const
{
    if (!_loaded)
        return "ERROR|十二生肖系统尚未加载";

    return BuildUiState(player);
}

std::string TwelveZodiacManager::GetHermesDetail(Player* player, uint32 zodiacId) const
{
    if (!_loaded)
        return "ERROR|十二生肖系统尚未加载";
    if (zodiacId < ZODIAC_FIRST_ID || zodiacId > ZODIAC_LAST_ID)
        return "ERROR|生肖ID无效";

    return BuildUiDetail(player, static_cast<uint8>(zodiacId));
}

std::string TwelveZodiacManager::ExecuteHermesEquip(Player* player, uint32 zodiacId, uint32 slot)
{
    bool const valid = _loaded && zodiacId >= ZODIAC_FIRST_ID && zodiacId <= ZODIAC_LAST_ID && slot < ZODIAC_INACTIVE_SLOT;
    bool const success = valid && Equip(player, static_cast<uint8>(zodiacId), static_cast<uint8>(slot), false);

    std::ostringstream result;
    result << "RESULT|EQUIP|" << (success ? 1 : 0) << '|'
           << (success ? "命宫已装备" : "装备失败：生肖未解锁或命宫未开放");
    return result.str();
}

std::string TwelveZodiacManager::ExecuteHermesUnequip(Player* player, uint32 slot)
{
    bool const valid = _loaded && slot < ZODIAC_INACTIVE_SLOT;
    bool const success = valid && Unequip(player, static_cast<uint8>(slot), false);

    std::ostringstream result;
    result << "RESULT|UNEQUIP|" << (success ? 1 : 0) << '|'
           << (success ? "命宫已卸下" : "该命宫没有装备生肖");
    return result.str();
}

void TwelveZodiacManager::HandleAddonCommand(Player* player, std::string const& command)
{
    if (!player || command.empty())
        return;

    auto parseNumber = [](std::string const& text, uint32& value) -> bool
    {
        if (text.empty())
            return false;

        char* end = nullptr;
        unsigned long const parsed = std::strtoul(text.c_str(), &end, 10);
        if (!end || *end != '\0' || parsed > std::numeric_limits<uint32>::max())
            return false;

        value = static_cast<uint32>(parsed);
        return true;
    };

    if (command == "GET" || command == "REQ_STATE")
    {
        SendUiState(player);
        return;
    }

    if (command.rfind("DETAIL:", 0) == 0)
    {
        uint32 zodiacId = 0;
        if (parseNumber(command.substr(7), zodiacId) && zodiacId >= ZODIAC_FIRST_ID && zodiacId <= ZODIAC_LAST_ID)
            SendUiDetail(player, static_cast<uint8>(zodiacId));
        else
            SendZodiacAddonPayload(player, "ERROR|生肖ID无效");
        return;
    }

    auto sendResult = [player](char const* action, bool success, char const* message)
    {
        std::ostringstream result;
        result << "RESULT|" << action << '|' << (success ? 1 : 0) << '|' << SanitizeAddonField(message ? message : "");
        SendZodiacAddonPayload(player, result.str());
    };

    if (command.rfind("EQUIP:", 0) == 0)
    {
        std::string const args = command.substr(6);
        size_t const separator = args.find(':');
        uint32 zodiacId = 0;
        uint32 slot = 0;
        bool const parsed = separator != std::string::npos &&
            parseNumber(args.substr(0, separator), zodiacId) &&
            parseNumber(args.substr(separator + 1), slot);
        bool const success = parsed && zodiacId >= ZODIAC_FIRST_ID && zodiacId <= ZODIAC_LAST_ID && slot < 255 &&
            Equip(player, static_cast<uint8>(zodiacId), static_cast<uint8>(slot));
        sendResult("EQUIP", success, success ? "命宫已装备" : "装备失败：生肖未解锁或命宫未开放");
        return;
    }

    if (command.rfind("UNEQUIP:", 0) == 0)
    {
        uint32 slot = 0;
        bool const parsed = parseNumber(command.substr(8), slot);
        bool const success = parsed && slot < 255 && Unequip(player, static_cast<uint8>(slot));
        sendResult("UNEQUIP", success, success ? "命宫已卸下" : "该命宫没有装备生肖");
        return;
    }

    SendZodiacAddonPayload(player, "ERROR|不支持的生肖UI请求");
}

uint8 TwelveZodiacManager::CountUnlocked(ZodiacRuntime const& runtime) const
{
    uint8 count = 0;
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
        if (_controls[id].valid && _controls[id].enabled && runtime.progress[id].unlocked)
            ++count;

    return count;
}

uint8 TwelveZodiacManager::GetAllowedSlots(ZodiacRuntime const& runtime) const
{
    uint8 const unlocked = CountUnlocked(runtime);
    uint8 allowed = 0;
    if (unlocked >= 1)
        allowed = 1;
    if (unlocked >= 3)
        allowed = 2;
    if (unlocked >= 6)
        allowed = 3;
    if (unlocked >= 9)
        allowed = 4;
    if (unlocked >= 12)
        allowed = _global.maxSlots;

    return std::min<uint8>(allowed, _global.maxSlots);
}

bool TwelveZodiacManager::IsFullSet(ZodiacRuntime const& runtime) const
{
    uint8 configured = 0;
    uint8 unlocked = 0;
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (!_controls[id].valid || !_controls[id].enabled)
            continue;

        ++configured;
        if (runtime.progress[id].unlocked)
            ++unlocked;
    }

    return configured == ZODIAC_LAST_ID && unlocked == configured;
}

uint256 TwelveZodiacManager::GetGrowthMultiplier(ZodiacRuntime const& runtime) const
{
    uint256 bonus = 0;
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (control.valid && control.enabled && runtime.progress[id].unlocked)
            bonus = AddSaturated(bonus, control.growthBonusPercent);
    }

    uint256 multiplier = AddSaturated(ZODIAC_PERCENT_BASE, bonus);
    if (IsFullSet(runtime))
        multiplier = ScaleByMultiplier(multiplier, _global.fullSetGrowthMultiplier);

    return multiplier;
}

uint256 TwelveZodiacManager::GetFullSetMultiplier(bool damage) const
{
    return damage ? _global.fullSetDamageMultiplier : _global.fullSetHealMultiplier;
}

bool TwelveZodiacManager::IsActive(ZodiacRuntime const& runtime, uint8 zodiacId) const
{
    if (zodiacId < ZODIAC_FIRST_ID || zodiacId > ZODIAC_LAST_ID)
        return false;

    ZodiacControl const& control = _controls[zodiacId];
    if (!control.valid || !control.enabled)
        return false;

    ZodiacProgress const& progress = runtime.progress[zodiacId];
    return progress.unlocked && progress.slot != ZODIAC_INACTIVE_SLOT && progress.slot < _global.maxSlots;
}

uint8 TwelveZodiacManager::FindSlot(ZodiacRuntime const& runtime, uint8 zodiacId) const
{
    if (zodiacId < ZODIAC_FIRST_ID || zodiacId > ZODIAC_LAST_ID)
        return ZODIAC_INACTIVE_SLOT;

    return runtime.progress[zodiacId].slot;
}

void TwelveZodiacManager::NormalizeSlots(Player* player, bool autoFill)
{
    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime)
        return;

    uint8 const allowedSlots = GetAllowedSlots(*runtime);
    std::array<bool, ZODIAC_DEFAULT_MAX_SLOTS> used{};

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacProgress& progress = runtime->progress[id];
        if (!_controls[id].valid || !_controls[id].enabled || !progress.unlocked ||
            progress.slot >= allowedSlots || progress.slot >= used.size() || used[progress.slot])
        {
            if (progress.slot != ZODIAC_INACTIVE_SLOT)
                runtime->dirty = true;
            progress.slot = ZODIAC_INACTIVE_SLOT;
            continue;
        }

        used[progress.slot] = true;
    }

    if (!autoFill)
        return;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacProgress& progress = runtime->progress[id];
        if (!progress.unlocked || progress.slot != ZODIAC_INACTIVE_SLOT)
            continue;

        auto freeSlot = std::find(used.begin(), used.begin() + allowedSlots, false);
        if (freeSlot == used.begin() + allowedSlots)
            break;

        progress.slot = static_cast<uint8>(std::distance(used.begin(), freeSlot));
        *freeSlot = true;
        runtime->dirty = true;
    }
}

void TwelveZodiacManager::SyncProfessionUnlocks(Player* player, bool notify)
{
    if (!player || !_loaded)
        return;

    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime)
        return;

    bool changed = false;
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (!control.valid || !control.enabled || !control.professionSkillId)
            continue;

        ZodiacProgress& progress = runtime->progress[id];
        bool const shouldUnlock = player->GetSkillValue(control.professionSkillId) >= control.requiredSkill;
        if (progress.unlocked == shouldUnlock)
            continue;

        progress.unlocked = shouldUnlock;
        if (shouldUnlock)
        {
            progress.star = std::max<uint8>(progress.star, 1);
            if (notify && player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00ff00[十二生肖]|r 专业达到{}，已激活 |cffffd700{}|r。",
                    control.requiredSkill, control.name);
        }
        else
        {
            progress.slot = ZODIAC_INACTIVE_SLOT;
            runtime->nextDamageReady[id] = 0;
            runtime->nextHealReady[id] = 0;
            runtime->nextDeathSaveReady[id] = 0;
            if (notify && player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffff4040[十二生肖]|r 专业低于{}，已失去 |cffffd700{}|r 并自动卸下命宫。",
                    control.requiredSkill, control.name);
        }

        runtime->dirty = true;
        changed = true;
    }

    NormalizeSlots(player, true);
    if (changed)
    {
        SavePlayer(player);
        SendUiState(player);
    }
}

void TwelveZodiacManager::OnSkillUpdate(Player* player, uint32 skillId, uint32 /*value*/, uint32 /*newValue*/)
{
    if (!player || !_loaded)
        return;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (control.valid && control.enabled && control.professionSkillId == skillId)
        {
            SyncProfessionUnlocks(player, true);
            player->UpdateAllStats();
            return;
        }
    }
}

bool TwelveZodiacManager::TryTriggerDamage(Player* player, Unit* victim, uint256& damage, ZodiacRuntime& runtime)
{
    if (!player || !victim || !damage)
        return false;

    uint64 const now = static_cast<uint64>(GameTime::GetGameTimeMS().count());
    uint8 selectedId = 0;
    uint256 selectedMultiplier = ZODIAC_PERCENT_BASE;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (!control.valid || !control.enabled || !IsActive(runtime, id))
            continue;
        if (!(control.triggerMode & ZODIAC_TRIGGER_DAMAGE) || control.triggerDamageMultiplier <= ZODIAC_PERCENT_BASE)
            continue;
        if (!IsReady(now, runtime.nextDamageReady[id]))
            continue;
        if (!RollConfiguredChance(control.triggerChancePercent))
            continue;

        if (!selectedId || control.triggerDamageMultiplier > selectedMultiplier)
        {
            selectedId = id;
            selectedMultiplier = control.triggerDamageMultiplier;
        }
    }

    if (!selectedId)
        return false;

    ZodiacControl const& selected = _controls[selectedId];
    damage = ScaleByMultiplier(damage, selectedMultiplier);
    // 0 表示不加冷却；默认配置为 5000，即 5 秒一次。
    runtime.nextDamageReady[selectedId] = now + selected.triggerCooldownMs;
    return true;
}

bool TwelveZodiacManager::TryTriggerHeal(Player* player, Unit* receiver, uint256& heal, ZodiacRuntime& runtime)
{
    if (!player || !receiver || !heal)
        return false;

    uint64 const now = static_cast<uint64>(GameTime::GetGameTimeMS().count());
    uint8 selectedId = 0;
    uint256 selectedMultiplier = ZODIAC_PERCENT_BASE;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (!control.valid || !control.enabled || !IsActive(runtime, id))
            continue;
        if (!(control.triggerMode & ZODIAC_TRIGGER_HEAL) || control.triggerHealMultiplier <= ZODIAC_PERCENT_BASE)
            continue;
        if (!IsReady(now, runtime.nextHealReady[id]))
            continue;
        if (!RollConfiguredChance(control.triggerChancePercent))
            continue;

        if (!selectedId || control.triggerHealMultiplier > selectedMultiplier)
        {
            selectedId = id;
            selectedMultiplier = control.triggerHealMultiplier;
        }
    }

    if (!selectedId)
        return false;

    ZodiacControl const& selected = _controls[selectedId];
    heal = ScaleByMultiplier(heal, selectedMultiplier);
    // 0 表示不加冷却；默认配置为 5000，即 5 秒一次。
    runtime.nextHealReady[selectedId] = now + selected.triggerCooldownMs;
    return true;
}

void TwelveZodiacManager::ApplyTriadDamage(ZodiacRuntime const& runtime, uint256& damage) const
{
    if (!damage)
        return;

    std::array<uint8, 5> groupCounts{};
    std::array<uint256, 5> groupMultipliers{};
    for (uint8& value : groupCounts)
        value = 0;
    for (uint256& value : groupMultipliers)
        value = ZODIAC_PERCENT_BASE;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (!IsActive(runtime, id))
            continue;

        ZodiacControl const& control = _controls[id];
        if (control.triadGroup == 0 || control.triadGroup >= groupCounts.size())
            continue;

        ++groupCounts[control.triadGroup];
        groupMultipliers[control.triadGroup] =
            std::max(groupMultipliers[control.triadGroup], control.triadDamageMultiplier);
    }

    uint256 bestMultiplier = ZODIAC_PERCENT_BASE;
    for (uint8 group = 1; group < groupCounts.size(); ++group)
        if (groupCounts[group] >= 3)
            bestMultiplier = std::max(bestMultiplier, groupMultipliers[group]);

    if (bestMultiplier != ZODIAC_PERCENT_BASE)
        damage = ScaleByMultiplier(damage, bestMultiplier);
}

void TwelveZodiacManager::ApplyTriadHeal(ZodiacRuntime const& runtime, uint256& heal) const
{
    if (!heal)
        return;

    std::array<uint8, 5> groupCounts{};
    std::array<uint256, 5> groupMultipliers{};
    for (uint256& value : groupMultipliers)
        value = ZODIAC_PERCENT_BASE;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (!IsActive(runtime, id))
            continue;

        ZodiacControl const& control = _controls[id];
        if (control.triadGroup == 0 || control.triadGroup >= groupCounts.size())
            continue;

        ++groupCounts[control.triadGroup];
        groupMultipliers[control.triadGroup] =
            std::max(groupMultipliers[control.triadGroup], control.triadHealMultiplier);
    }

    uint256 bestMultiplier = ZODIAC_PERCENT_BASE;
    for (uint8 group = 1; group < groupCounts.size(); ++group)
        if (groupCounts[group] >= 3)
            bestMultiplier = std::max(bestMultiplier, groupMultipliers[group]);

    if (bestMultiplier != ZODIAC_PERCENT_BASE)
        heal = ScaleByMultiplier(heal, bestMultiplier);
}

bool TwelveZodiacManager::IsBelowExecuteThreshold(Unit* victim, uint256 const& thresholdPercent) const
{
    if (!victim || !thresholdPercent || thresholdPercent > ZODIAC_PERCENT_BASE)
        return false;

    uint256 const maxHealth = victim->GetMaxHealthForCombat256();
    uint256 const currentHealth = victim->GetHealthForCombat256();
    if (!maxHealth || !currentHealth)
        return false;

    return currentHealth <= ScaleByMultiplier(maxHealth, thresholdPercent);
}

void TwelveZodiacManager::ApplyGrowth(Player* player, int256& healingBonus, int256 spellDamage[MAX_SPELL_SCHOOL])
{
    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime || !_loaded)
        return;

    uint256 const multiplier = GetGrowthMultiplier(*runtime);
    if (multiplier == ZODIAC_PERCENT_BASE)
        return;

    ApplyMultiplier(healingBonus, multiplier);
    for (uint8 school = 0; school < MAX_SPELL_SCHOOL; ++school)
        ApplyMultiplier(spellDamage[school], multiplier);
}

void TwelveZodiacManager::ApplyRating(Player* player, int256& amount)
{
    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime || !_loaded)
        return;

    ApplyMultiplier(amount, GetGrowthMultiplier(*runtime));
}

void TwelveZodiacManager::ApplyDamage(Player* player, Unit* victim, uint256& damage,
    DamageEffectType /*damageType*/, SpellInfo const* spellInfo)
{
    if (!player)
        return;

    if (!victim)
        return;

    if (!damage)
        return;

    if (!_loaded)
        return;

    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime)
        return;

    static thread_local bool applying = false;
    if (applying)
        return;

    applying = true;

    bool const pvp = IsPvpContext(player, victim);
    uint256 const originalDamage = damage;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (!IsActive(*runtime, id))
            continue;

        ZodiacControl const& control = _controls[id];
        uint256 const& multiplier = pvp ? control.pvpFinalDamageMultiplier : control.finalDamageMultiplier;
        if (multiplier != ZODIAC_PERCENT_BASE)
            damage = ScaleByMultiplier(damage, multiplier);

        if (!pvp && control.executeThresholdPercent && IsBelowExecuteThreshold(victim, control.executeThresholdPercent))
        {
            uint256 const currentHealth = victim->GetHealthForCombat256();
            if (currentHealth > damage)
                damage = currentHealth;
        }
    }

    uint256 const afterIndividual = damage;
    ApplyTriadDamage(*runtime, damage);
    uint256 const afterTriad = damage;
    bool const fullSet = IsFullSet(*runtime);
    if (fullSet)
        damage = ScaleByMultiplier(damage, GetFullSetMultiplier(true));

    // 一个命中最多触发一个生肖额外倍率，避免12个生肖在5秒CD下形成扇出。
    bool const triggerApplied = TryTriggerDamage(player, victim, damage, *runtime);
    bool const triadApplied = afterTriad != afterIndividual;

    if (damage != originalDamage)
    {
        uint256 const effectiveMultiplierPercent = CalculateEffectiveMultiplierPercent(originalDamage, damage);
        SendZodiacDamagePayload(player, originalDamage, damage, effectiveMultiplierPercent,
            triggerApplied, triadApplied, fullSet, spellInfo);
    }

    applying = false;
}

void TwelveZodiacManager::ApplyHeal(Player* player, Unit* receiver, uint256& heal)
{
    if (!player || !receiver || !heal || !_loaded)
        return;

    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime)
        return;

    static thread_local bool applying = false;
    if (applying)
        return;

    applying = true;
    bool const pvp = IsPvpContext(player, receiver);
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (!IsActive(*runtime, id))
            continue;

        ZodiacControl const& control = _controls[id];
        uint256 const& multiplier = pvp ? control.pvpFinalHealMultiplier : control.finalHealMultiplier;
        if (multiplier != ZODIAC_PERCENT_BASE)
            heal = ScaleByMultiplier(heal, multiplier);
    }

    ApplyTriadHeal(*runtime, heal);
    if (IsFullSet(*runtime))
        heal = ScaleByMultiplier(heal, GetFullSetMultiplier(false));

    TryTriggerHeal(player, receiver, heal, *runtime);
    applying = false;
}

bool TwelveZodiacManager::TryDeathSave(Unit* victim)
{
    Player* player = victim ? victim->ToPlayer() : nullptr;
    if (!player || !_loaded || player->InArena())
        return false;

    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime)
        return false;

    uint64 const now = static_cast<uint64>(GameTime::GetGameTimeMS().count());
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (!IsActive(*runtime, id))
            continue;

        ZodiacControl const& control = _controls[id];
        if (!control.deathSaveCooldownMs || !IsReady(now, runtime->nextDeathSaveReady[id]))
            continue;

        uint256 health = ScaleByMultiplier(player->GetMaxHealthForCombat256(), control.deathSaveHealthPercent);
        if (!health)
            health = 1;

        health = std::min(health, player->GetMaxHealthForCombat256());
        player->SetHealthForCombat256(health);
        runtime->nextDeathSaveReady[id] = now + control.deathSaveCooldownMs;

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[十二生肖]|r |cffffd700{}|r 触发护命，剩余生命 {}%。",
                control.name, control.deathSaveHealthPercent);
        return true;
    }

    return false;
}

bool TwelveZodiacManager::Equip(Player* player, uint8 zodiacId, uint8 slot, bool sendUi)
{
    ZodiacRuntime* runtime = GetRuntime(player);
    ZodiacControl const* control = GetControl(zodiacId);
    if (!runtime || !control || !control->enabled || !runtime->progress[zodiacId].unlocked)
        return false;

    uint8 const allowedSlots = GetAllowedSlots(*runtime);
    if (slot >= allowedSlots || slot >= _global.maxSlots)
        return false;

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
        if (id != zodiacId && runtime->progress[id].slot == slot)
            runtime->progress[id].slot = ZODIAC_INACTIVE_SLOT;

    runtime->progress[zodiacId].slot = slot;
    runtime->dirty = true;
    SavePlayer(player);
    player->UpdateAllStats();
    if (sendUi)
        SendUiState(player);
    return true;
}

bool TwelveZodiacManager::Unequip(Player* player, uint8 slot, bool sendUi)
{
    ZodiacRuntime* runtime = GetRuntime(player);
    if (!runtime || slot >= _global.maxSlots)
        return false;

    bool changed = false;
    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        if (runtime->progress[id].slot != slot)
            continue;

        runtime->progress[id].slot = ZODIAC_INACTIVE_SLOT;
        changed = true;
    }

    if (changed)
    {
        runtime->dirty = true;
        SavePlayer(player);
        player->UpdateAllStats();
        if (sendUi)
            SendUiState(player);
    }

    return changed;
}

void TwelveZodiacManager::SendInfo(ChatHandler* handler, Player* player) const
{
    if (!handler || !player)
        return;

    auto itr = _players.find(player->GetGUID().GetCounter());
    if (itr == _players.end())
    {
        handler->SendSysMessage("十二生肖玩家数据尚未加载。");
        return;
    }

    ZodiacRuntime const& runtime = itr->second;
    handler->PSendSysMessage("|cff00ff00[十二生肖]|r 已解锁 {}/12，允许命宫槽位 {} 个。",
        CountUnlocked(runtime), GetAllowedSlots(runtime));

    for (uint8 id = ZODIAC_FIRST_ID; id <= ZODIAC_LAST_ID; ++id)
    {
        ZodiacControl const& control = _controls[id];
        if (!control.valid)
            continue;

        ZodiacProgress const& progress = runtime.progress[id];
        uint32 skillValue = control.professionSkillId ? player->GetSkillValue(control.professionSkillId) : 0;
        std::string status = progress.unlocked ? "已解锁" : "未解锁";
        handler->PSendSysMessage("  {}：{}，专业{}，技能{}，命宫槽位{}，触发冷却{}ms，触发伤害倍率{}。",
            control.name, status, control.professionSkillId, skillValue,
            progress.slot == ZODIAC_INACTIVE_SLOT ? std::string("未装备") : std::to_string(progress.slot),
            control.triggerCooldownMs, control.triggerDamageMultiplier);
    }
}

class TwelveZodiacWorldScript : public WorldScript
{
public:
    TwelveZodiacWorldScript() : WorldScript("TwelveZodiacWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP,
        WORLDHOOK_ON_UPDATE
    }) { }

    void ScheduleReload()
    {
        ScheduleLoad(0);
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            ScheduleLoad(0);
    }

    void OnStartup() override
    {
        ScheduleLoad(ZODIAC_DEFERRED_LOAD_DELAY_MS);
    }

    void OnUpdate(uint32 diff) override
    {
        _queryProcessor.ProcessReadyCallbacks();
        s_twelveZodiacManager.ProcessPendingPlayerRefresh(ZODIAC_PLAYER_REFRESH_BATCH_SIZE);

        if (!_loadScheduled)
            return;

        if (_loadDelayMs > diff)
        {
            _loadDelayMs -= diff;
            return;
        }

        _loadScheduled = false;
        _loadDelayMs = 0;
        uint32 const generation = _loadGeneration;
        _queryProcessor.AddCallback(WorldDatabase.AsyncQuery(ZODIAC_CONTROL_SELECT_SQL).WithCallback(
            [this, generation](QueryResult result)
            {
                if (generation != _loadGeneration)
                    return;

                s_twelveZodiacManager.ApplyControls(std::move(result));
            }));
    }

private:
    void ScheduleLoad(uint32 delayMs)
    {
        ++_loadGeneration;
        _loadScheduled = true;
        _loadDelayMs = delayMs;
    }

    QueryCallbackProcessor _queryProcessor;
    uint32 _loadGeneration = 0;
    uint32 _loadDelayMs = 0;
    bool _loadScheduled = false;
};

TwelveZodiacWorldScript* s_twelveZodiacWorldScript = nullptr;

class TwelveZodiacPlayerScript : public PlayerScript
{
public:
    TwelveZodiacPlayerScript() : PlayerScript("TwelveZodiacPlayerScript", {
        PLAYERHOOK_ON_LOAD_FROM_DB,
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_SAVE,
        PLAYERHOOK_ON_UPDATE_SKILL,
        PLAYERHOOK_ON_AFTER_UPDATE_SPELL_DAMAGE_AND_HEALING,
        PLAYERHOOK_ON_AFTER_UPDATE_RATING,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER
    }) { }

    void OnPlayerLoadFromDB(Player* player) override
    {
        s_twelveZodiacManager.LoadPlayer(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !s_twelveZodiacManager.IsLoaded())
            return;

        s_twelveZodiacManager.SyncProfessionUnlocks(player, true);
        s_twelveZodiacManager.NormalizeSlots(player, true);
        s_twelveZodiacManager.SavePlayer(player);

        // 让数据库配置的成长倍率进入当前角色的扩展法术/评级计算。
        player->UpdateAllStats();
        s_twelveZodiacManager.SendUiState(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        s_twelveZodiacManager.SavePlayer(player);
        s_twelveZodiacManager.ClearPlayer(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        s_twelveZodiacManager.DeletePlayer(guid.GetCounter());
    }

    void OnPlayerSave(Player* player) override
    {
        s_twelveZodiacManager.SavePlayer(player);
    }

    void OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 value, uint32 /*max*/, uint32 /*step*/, uint32 newValue) override
    {
        s_twelveZodiacManager.OnSkillUpdate(player, skillId, value, newValue);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver) override
    {
        if (!player || !receiver || receiver->GetGUID() != player->GetGUID() ||
            type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t const tabPosition = msg.find('\t');
        if (tabPosition == std::string::npos || msg.substr(0, tabPosition) != ZODIAC_ADDON_PREFIX)
            return;

        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "ZODIACUI", 250, 8))
            return;

        s_twelveZodiacManager.HandleAddonCommand(player, msg.substr(tabPosition + 1));
    }

    void OnPlayerAfterUpdateSpellDamageAndHealing(Player* player, int256& healingBonus, int256 spellDamage[MAX_SPELL_SCHOOL]) override
    {
        s_twelveZodiacManager.ApplyGrowth(player, healingBonus, spellDamage);
    }

    void OnPlayerAfterUpdateRating(Player* player, CombatRating /*combatRating*/, int256& amount) override
    {
        s_twelveZodiacManager.ApplyRating(player, amount);
    }
};

class TwelveZodiacUnitScript : public UnitScript
{
public:
    TwelveZodiacUnitScript() : UnitScript("TwelveZodiacUnitScript", true, {
        UNITHOOK_ON_HEAL,
        UNITHOOK_ON_BEFORE_DAMAGE_WITH_CONTEXT,
        UNITHOOK_ON_BEFORE_UNIT_KILL
    }) { }

    void OnHeal(Unit* healer, Unit* receiver, uint256& gain) override
    {
        if (!healer || !receiver || !gain)
            return;

        Player* player = healer->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (player)
            s_twelveZodiacManager.ApplyHeal(player, receiver, gain);
    }

    void OnBeforeDamageWithContext(Unit* attacker, Unit* victim, uint256& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override
    {
        Player* player = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!attacker || !victim || !damage)
            return;

        if (player && player != victim)
            s_twelveZodiacManager.ApplyDamage(player, victim, damage, damageType, spellInfo);
    }

    bool OnBeforeUnitKill(Unit* /*killer*/, Unit* victim, SpellInfo const* /*spellInfo*/, Spell const* /*spell*/) override
    {
        return s_twelveZodiacManager.TryDeathSave(victim);
    }
};

class TwelveZodiacCommandScript : public CommandScript
{
public:
    TwelveZodiacCommandScript() : CommandScript("TwelveZodiacCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subTable =
        {
            { "界面", HandleOpenUi, SEC_PLAYER,     Console::No },
            { "信息", HandleInfo,   SEC_PLAYER,     Console::No },
            { "查看", HandleInfo,   SEC_PLAYER,     Console::No },
            { "装备", HandleEquip,  SEC_PLAYER,     Console::No },
            { "激活", HandleEquip,  SEC_PLAYER,     Console::No },
            { "卸下", HandleUnequip,SEC_PLAYER,     Console::No },
            { "测试学习", HandleTestLearn,  SEC_GAMEMASTER, Console::No },
            { "测试遗忘", HandleTestForget, SEC_GAMEMASTER, Console::No },
            { "重载", HandleReload, SEC_GAMEMASTER, Console::Yes }
        };

        static ChatCommandTable rootTable =
        {
            { "生肖",     subTable },
            { "十二生肖", subTable }
        };

        return rootTable;
    }

private:
    static Player* GetPlayer(ChatHandler* handler)
    {
        return handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    }

    static bool ParseTwoNumbers(char const* args, uint32& first, uint32& second)
    {
        if (!args)
            return false;

        std::istringstream stream(args);
        return static_cast<bool>(stream >> first >> second);
    }

    static bool ParseOneNumber(char const* args, uint32& value)
    {
        if (!args)
            return false;

        std::istringstream stream(args);
        return static_cast<bool>(stream >> value);
    }

    static bool HandleInfo(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        s_twelveZodiacManager.SendInfo(handler, player);
        return true;
    }

    static bool HandleOpenUi(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        if (!HermesBridge_SendAddonMessage(player, ZODIAC_OPEN_ADDON_PREFIX, "OPEN_UI"))
            handler->SendSysMessage("十二生肖UI通信尚未就绪，请稍后重试。");

        return true;
    }

    static bool HandleEquip(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        uint32 zodiacId = 0;
        uint32 slot = 0;
        if (!ParseTwoNumbers(args, zodiacId, slot) || zodiacId < ZODIAC_FIRST_ID || zodiacId > ZODIAC_LAST_ID || slot > 254)
        {
            handler->SendSysMessage("用法：.生肖 装备 <生肖ID> <命宫槽位>");
            return true;
        }

        if (s_twelveZodiacManager.Equip(player, static_cast<uint8>(zodiacId), static_cast<uint8>(slot)))
            handler->PSendSysMessage("|cff00ff00[十二生肖]|r 已将生肖{}装备到命宫槽位{}。", zodiacId, slot);
        else
            handler->SendSysMessage("装备失败：生肖未解锁、槽位未开放或配置无效。");

        return true;
    }

    static bool HandleUnequip(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        uint32 slot = 0;
        if (!ParseOneNumber(args, slot) || slot > 254)
        {
            handler->SendSysMessage("用法：.生肖 卸下 <命宫槽位>");
            return true;
        }

        if (s_twelveZodiacManager.Unequip(player, static_cast<uint8>(slot)))
            handler->PSendSysMessage("|cff00ff00[十二生肖]|r 已卸下命宫槽位{}。", slot);
        else
            handler->SendSysMessage("卸下失败：该命宫槽位没有生肖。");

        return true;
    }

    static bool HandleTestLearn(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        for (uint32 skillId : ZODIAC_TEST_SKILL_IDS)
        {
            LearnAllProfessionRecipes(player, skillId);
            uint16 const skillStep = player->GetSkillStep(skillId);
            player->SetSkill(skillId, skillStep ? skillStep : 1, 450, 450);
        }

        s_twelveZodiacManager.SyncProfessionUnlocks(player, true);
        s_twelveZodiacManager.NormalizeSlots(player, true);
        s_twelveZodiacManager.SavePlayer(player);
        player->UpdateAllStats();
        s_twelveZodiacManager.SendUiState(player);
        handler->SendSysMessage("十二生肖测试：已学习全部专业配方，并将14个专业技能设置为450。");
        return true;
    }

    static bool HandleTestForget(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        for (uint32 firstRankSpellId : ZODIAC_TEST_FIRST_RANK_SPELL_IDS)
            if (player->HasSpell(firstRankSpellId))
                player->removeSpell(firstRankSpellId, SPEC_MASK_ALL, false);

        for (uint32 skillId : ZODIAC_TEST_SKILL_IDS)
            if (player->GetSkillValue(skillId))
                player->SetSkill(skillId, 0, 0, 0);

        s_twelveZodiacManager.SyncProfessionUnlocks(player, false);
        s_twelveZodiacManager.NormalizeSlots(player, true);
        s_twelveZodiacManager.SavePlayer(player);
        player->UpdateAllStats();
        s_twelveZodiacManager.SendUiState(player);
        handler->SendSysMessage("十二生肖测试：已遗忘14个专业；对应生肖已按当前专业等级锁定并自动卸下命宫。");
        return true;
    }

    static bool HandleReload(ChatHandler* handler, char const* /*args*/)
    {
        if (!s_twelveZodiacWorldScript)
        {
            handler->SendSysMessage("十二生肖异步加载器尚未初始化。");
            return true;
        }

        s_twelveZodiacWorldScript->ScheduleReload();
        handler->SendSysMessage("十二生肖控制表已提交异步重载；完成后在线玩家属性会自动刷新。");
        return true;
    }
};

} // anonymous namespace

std::string TwelveZodiacHermesGetState(Player* player)
{
    return s_twelveZodiacManager.GetHermesState(player);
}

std::string TwelveZodiacHermesGetDetail(Player* player, uint32 zodiacId)
{
    return s_twelveZodiacManager.GetHermesDetail(player, zodiacId);
}

std::string TwelveZodiacHermesEquip(Player* player, uint32 zodiacId, uint32 slot)
{
    return s_twelveZodiacManager.ExecuteHermesEquip(player, zodiacId, slot);
}

std::string TwelveZodiacHermesUnequip(Player* player, uint32 slot)
{
    return s_twelveZodiacManager.ExecuteHermesUnequip(player, slot);
}

void AddSC_twelve_zodiac()
{
    s_twelveZodiacWorldScript = new TwelveZodiacWorldScript();
    new TwelveZodiacPlayerScript();
    new TwelveZodiacUnitScript();
    new TwelveZodiacCommandScript();
}

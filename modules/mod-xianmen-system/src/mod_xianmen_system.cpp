/*
 * 仙门系统
 */

#include "Chat.h"
#include "AddonThrottle.h"
#include "Cell.h"
#include "CellImpl.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "DBCStructure.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "HermesBridgeAddonApi.h"
#include "Item.h"
#include "AllItemScript.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ModuleManager.h"
#include "ObjectMgr.h"
#include "StringConvert.h"
#include "Player.h"
#include "PoolMgr.h"
#include "QuestDef.h"
#include "RequirementInterface.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"
#include "Util.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#if __has_include("MagicHitSystem.h")
#ifndef MODULE_MAGIC_HIT_SYSTEM
#define MODULE_MAGIC_HIT_SYSTEM
#endif
#include "MagicHitSystem.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <list>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
static constexpr const char* XIANMEN_ADDON_PREFIX = "XIANMEN";
static constexpr const char* PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX = "PATTRPANEL";
static constexpr size_t XIANMEN_MAX_ADDON_PAYLOAD = 220;
static constexpr int32 XIANMEN_SKILL_MARKER_BASE = 9200000;
static constexpr uint32 XIANMEN_NATIVE_DAILY_NPC = 383000;
static constexpr uint32 XIANMEN_NATIVE_DAILY_QUEST_FIRST = 383101;
static constexpr uint32 XIANMEN_NATIVE_DAILY_QUEST_LAST = 383200;
static constexpr uint8 XIANMEN_NATIVE_DAILY_MENU_LIMIT = 20;
static constexpr uint32 XIANMEN_FROST_SNARE_SPELL = 31589;
static constexpr uint32 XIANMEN_GOLDEN_BODY_COOLDOWN_SECONDS = 120;
static constexpr uint32 XIANMEN_FULU_FACTION_ID = 2;
static constexpr uint32 XIANMEN_BODY_FACTION_ID = 3;
static constexpr uint32 XIANMEN_WUHUN_FACTION_ID = 4;
static constexpr uint32 XIANMEN_LINGDUN_VISUAL_KIT = 990;
static constexpr uint32 XIANMEN_MAGIC_HIT_RESONANCE_SPELL = 382026;
static constexpr uint32 XIANMEN_MAGIC_HIT_RESONANCE_COOLDOWN_MS = 250;
thread_local bool XianmenProcDamageGuard = false;
std::unordered_map<uint32, uint32> XianmenGoldenBodyReadyTimes;
std::unordered_map<uint32, uint256> XianmenLingdunShieldValues;
std::unordered_map<uint32, std::string> XianmenSpellPowerDebugSignatures;
std::unordered_map<uint64, std::string> XianmenRatingDebugSignatures;
std::unordered_map<uint64, uint32> XianmenProcPpmLastEventMs;

std::unordered_map<uint64, uint32> XianmenMagicHitResonanceNextReadyMs;

uint64 MakeXianmenPlayerSpellKey(uint32 playerGuid, uint32 spellId)
{
    return (static_cast<uint64>(playerGuid) << 32) | spellId;
}

bool ConsumeXianmenMagicHitResonanceCooldown(Player* player, uint32 sourceSpellId)
{
    if (!player || sourceSpellId == 0)
        return false;

    uint64 const key = MakeXianmenPlayerSpellKey(player->GetGUID().GetCounter(), sourceSpellId);
    uint32 const nowMs = static_cast<uint32>(GameTime::GetGameTimeMS().count());
    auto const readyItr = XianmenMagicHitResonanceNextReadyMs.find(key);
    if (readyItr != XianmenMagicHitResonanceNextReadyMs.end() && nowMs < readyItr->second)
        return false;

    XianmenMagicHitResonanceNextReadyMs[key] = nowMs + XIANMEN_MAGIC_HIT_RESONANCE_COOLDOWN_MS;
    return true;
}

void ClearXianmenMagicHitResonanceState(uint32 playerGuid)
{
    for (auto itr = XianmenMagicHitResonanceNextReadyMs.begin(); itr != XianmenMagicHitResonanceNextReadyMs.end();)
    {
        if (static_cast<uint32>(itr->first >> 32) == playerGuid)
            itr = XianmenMagicHitResonanceNextReadyMs.erase(itr);
        else
            ++itr;
    }

    for (auto itr = XianmenProcPpmLastEventMs.begin(); itr != XianmenProcPpmLastEventMs.end();)
    {
        if (static_cast<uint32>(itr->first >> 32) == playerGuid)
            itr = XianmenProcPpmLastEventMs.erase(itr);
        else
            ++itr;
    }
}

#if defined(MODULE_MAGIC_HIT_SYSTEM)
bool HasUsableXianmenMagicHitData(MagicHitSystem* magicHitSystem, Player* player, uint32 sourceSpellId)
{
    if (!magicHitSystem || !player || sourceSpellId == 0)
        return false;

    std::vector<ItemMagicHitData> const playerData = magicHitSystem->GetPlayerSpellMagicData(player, sourceSpellId);
    for (ItemMagicHitData const& data : playerData)
    {
        if (data.hitCount == 0)
            continue;

        SpellMagicHitConfig const* config = magicHitSystem->GetConfigById(data.configId);
        if (config && config->IsValid())
            return true;
    }

    return false;
}
#endif

void NotifyPlayerAttributePanelRefresh(Player* player)
{
    if (!player || !player->GetSession())
        return;

    if (HermesBridge_SendAddonMessage(player, PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX, "REFRESH"))
        return;

    WorldPacket data;
    std::string fullMessage = std::string(PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX) + "\tREFRESH";
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
}

struct XianmenFactionConfig
{
    uint32 id = 0;
    std::string name;
    std::string description;
    uint32 maxLevel = 0;
    uint32 joinRequirementId = 0;
};

struct XianmenSkillConfig
{
    uint32 id = 0;
    uint32 factionId = 0;
    uint32 order = 0;
    std::string name;
    uint32 type = 0;
    uint32 spellId = 0;
    float baseValue = 0.0f;
    float growthPer10 = 0.0f;
    std::string triggerParam;
    std::string description;
};

struct XianmenDailyTemplateConfig
{
    uint32 id = 0;
    std::string name;
    uint32 type = 0;
    std::string targetParam;
    uint32 defaultCount = 1;
    float difficulty = 1.0f;
    uint32 defaultRewardTier = 1;
};

struct XianmenRewardConfig
{
    uint32 type = 0;
    std::string name;
    uint32 lowAmount = 0;
    uint32 middleAmount = 0;
    uint32 highAmount = 0;
    float maxChance = 100.0f;
    uint32 itemId = 0;
};

struct XianmenDailyView
{
    uint32 dailyId = 0;
    uint32 templateId = 0;
    std::string templateName;
    uint32 templateType = 0;
    uint32 targetCount = 0;
    uint32 progress = 0;
    bool completed = false;
    uint32 rewardTier = 1;
    uint32 rewardType = 0;
    std::string rewardName;
    uint32 rewardAmount = 0;
};

struct XianmenUpgradeRequirementConfig
{
    uint32 factionId = 0;
    uint32 targetLevel = 0;
    uint64 historyContribution = 0;
    uint32 requirementId = 0;
};

struct XianmenPlayerData
{
    uint32 factionId = 0;
    uint32 level = 0;
    uint64 dailyContribution = 0;
    uint64 historyContribution = 0;
    std::set<uint32> unlockedSkills;
    std::set<uint32> personalActiveSkills;
};

uint32 GetNow()
{
    return static_cast<uint32>(GameTime::GetGameTime().count());
}

std::vector<uint32> ParseUIntList(char const* args)
{
    std::vector<uint32> values;
    std::istringstream stream(args ? args : "");

    uint32 value = 0;
    while (stream >> value)
        values.push_back(value);

    return values;
}

std::vector<uint32> ParseUIntList(std::string const& text)
{
    std::vector<uint32> values;
    std::istringstream stream(text);

    uint32 value = 0;
    while (stream >> value)
        values.push_back(value);

    return values;
}

uint64 ParseUInt64(std::string const& text)
{
    uint64 value = 0;
    std::istringstream stream(text);
    stream >> value;
    return value;
}

bool IsXianmenNativeDailyQuest(uint32 questId)
{
    return questId >= XIANMEN_NATIVE_DAILY_QUEST_FIRST && questId <= XIANMEN_NATIVE_DAILY_QUEST_LAST;
}

std::set<uint32> ParseUIntSet(std::string const& text)
{
    std::set<uint32> values;
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');

    for (uint32 value : ParseUIntList(normalized))
        if (value)
            values.insert(value);

    return values;
}

std::string JoinUIntSet(std::set<uint32> const& values)
{
    std::ostringstream stream;
    bool first = true;

    for (uint32 value : values)
    {
        if (!first)
            stream << ',';

        stream << value;
        first = false;
    }

    return stream.str();
}

std::string JoinXianmenSkillIds(std::vector<XianmenSkillConfig const*> const& skills)
{
    std::set<uint32> values;
    for (XianmenSkillConfig const* skill : skills)
        if (skill)
            values.insert(skill->id);

    return JoinUIntSet(values);
}

int256 GetMaxXianmenSpellDamage(int256 const spellDamage[7])
{
    int256 result = 0;
    for (uint8 school = 0; school < 7; ++school)
        if (spellDamage[school] > result)
            result = spellDamage[school];

    return result;
}

std::string EscapePayload(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '^' || ch == '~' || ch == '|')
            ch = ' ';
    }

    return value;
}

uint64 MakeXianmenUpgradeRequirementKey(uint32 factionId, uint32 targetLevel)
{
    return (static_cast<uint64>(factionId) << 32) | targetLevel;
}

std::string FormatXianmenInt256(int256 const& value)
{
    return value.convert_to<std::string>();
}

int256 AddXianmenInt256Saturated(int256 const& left, int256 const& right)
{
    if (right > 0 && left > std::numeric_limits<int256>::max() - right)
        return std::numeric_limits<int256>::max();
    if (right < 0 && left < std::numeric_limits<int256>::min() - right)
        return std::numeric_limits<int256>::min();

    return left + right;
}

int256 ScaleXianmenInt256Percent(int256 const& value, float percent)
{
    if (value <= 0 || percent <= 0.0f || std::isnan(percent))
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / 100.0L;
    return Acore::Number::ToInt256Saturated(scaled);
}

float XianmenInt256ToFloat(int256 const& value)
{
    if (value <= 0)
        return 0.0f;

    long double raw = Acore::Number::ToLongDouble(value);
    if (!std::isfinite(static_cast<double>(raw)))
        return std::numeric_limits<float>::max();

    return static_cast<float>(std::min<long double>(raw, static_cast<long double>(std::numeric_limits<float>::max())));
}

int32 XianmenInt256ToInt32(int256 const& value)
{
    if (value <= 0)
        return 0;

    if (value > std::numeric_limits<int32>::max())
        return std::numeric_limits<int32>::max();

    return static_cast<int32>(value);
}

int32 XianmenFloatToInt32(float value)
{
    if (value <= 0.0f || std::isnan(value))
        return 0;

    if (value >= static_cast<float>(std::numeric_limits<int32>::max()))
        return std::numeric_limits<int32>::max();

    return static_cast<int32>(value);
}

static RequirementInterface* GetXianmenRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    return mgr ? mgr->GetRequirementModule() : nullptr;
}

uint32 GetXianmenSkillOrder(XianmenSkillConfig const& skill);
float GetXianmenSkillValue(XianmenSkillConfig const& skill, uint32 level);

class XianmenMgr
{
public:
    static XianmenMgr* instance()
    {
        static XianmenMgr instance;
        return &instance;
    }

    void LoadConfig()
    {
        _enabled = sConfigMgr->GetOption<bool>("XianmenSystem.Enable", true);
        _announceOnLogin = sConfigMgr->GetOption<bool>("XianmenSystem.AnnounceOnLogin", true);
        _enableDailyLeaderSettle = sConfigMgr->GetOption<bool>("XianmenSystem.EnableDailyLeaderSettle", true);
        _maxLevel = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.MaxLevel", 100));
        _unlockLevelStep = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.SkillUnlockLevelStep", 10));
        _maxActiveSkills = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.MaxActiveSkills", 10));
        _maxPersonalActiveSkills = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.MaxPersonalActiveSkills", 5));
        _maxDailyPublishes = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.MaxDailyPublishes", 20));
        _upgradeRequirementBase = std::max<uint64>(1ULL, sConfigMgr->GetOption<uint64>("XianmenSystem.UpgradeContributionBase", 100ULL));
        _upgradeRequirementPerLevel = sConfigMgr->GetOption<uint64>("XianmenSystem.UpgradeContributionPerLevel", 10ULL);
        _settleCheckIntervalMs = std::max<uint32>(5, sConfigMgr->GetOption<uint32>("XianmenSystem.LeaderSettleCheckInterval", 60)) * IN_MILLISECONDS;
        _transferKeepLevelPct = std::clamp<float>(sConfigMgr->GetOption<float>("XianmenSystem.TransferKeepLevelPct", 50.0f), 0.0f, 100.0f);
        _debugEffectiveSkillLog = sConfigMgr->GetOption<bool>("XianmenSystem.DebugEffectiveSkillLog", false);
    }

    bool IsEnabled() const
    {
        return _enabled;
    }

    bool IsEffectiveSkillDebugLogEnabled() const
    {
        return _debugEffectiveSkillLog;
    }

    bool ShouldAnnounceOnLogin() const
    {
        return _announceOnLogin;
    }

    uint32 GetSettleCheckIntervalMs() const
    {
        return _settleCheckIntervalMs;
    }

    uint32 GetMaxActiveSkills() const
    {
        return _maxActiveSkills;
    }

    uint32 GetMaxPersonalActiveSkills() const
    {
        return _maxPersonalActiveSkills;
    }

    void LoadAll()
    {
        LoadFactions();
        LoadUpgradeRequirements();
        LoadSkills();
        LoadDailyTemplates();
        LoadRewardConfigs();
        EnsureLeaderRows();
        LoadActiveSkills();
        LoadLeaders();

        LOG_INFO("server.loading", "→仙门系统√ 门派={} 升级需求={} 技能={} 日常模板={} 奖励={} 生效技能={}",
            static_cast<uint32>(_factions.size()),
            static_cast<uint32>(_upgradeRequirements.size()),
            static_cast<uint32>(_skills.size()),
            static_cast<uint32>(_dailyTemplates.size()),
            static_cast<uint32>(_rewardConfigs.size()),
            GetActiveSkillCount());
    }

    void LoadFactions()
    {
        _factions.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `门派ID`, `门派名称`, `门派描述`, `修为上限`, `加入需求ID` "
            "FROM `_仙门_门派` ORDER BY `门派ID`");

        if (!result)
        {
            LOG_WARN("module", "仙门系统: 未找到门派配置，请先导入 sql/world/_仙门系统_配置.sql");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            XianmenFactionConfig faction;
            faction.id = fields[0].Get<uint32>();
            faction.name = fields[1].Get<std::string>();
            faction.description = fields[2].Get<std::string>();
            faction.maxLevel = fields[3].Get<uint32>();
            faction.joinRequirementId = fields[4].Get<uint32>();

            if (faction.id)
                _factions[faction.id] = faction;
        } while (result->NextRow());
    }

    void LoadUpgradeRequirements()
    {
        _upgradeRequirements.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `门派ID`, `目标等级`, `历史贡献需求`, `需求模板ID` "
            "FROM `_仙门_升级需求` ORDER BY `门派ID`, `目标等级`");

        if (!result)
        {
            LOG_WARN("module", "仙门系统: 未找到升级需求配置，升级历史贡献需求将回退到配置公式");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            XianmenUpgradeRequirementConfig requirement;
            requirement.factionId = fields[0].Get<uint32>();
            requirement.targetLevel = fields[1].Get<uint32>();
            requirement.historyContribution = fields[2].Get<uint64>();
            requirement.requirementId = fields[3].Get<uint32>();

            if (!requirement.targetLevel)
                continue;

            _upgradeRequirements[MakeXianmenUpgradeRequirementKey(requirement.factionId, requirement.targetLevel)] = requirement;
        } while (result->NextRow());
    }

    void LoadSkills()
    {
        _skills.clear();
        _skillsByFaction.clear();
        _skillsBySpell.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `技能ID`, `门派ID`, `序号`, `技能名称`, `技能子类`, `法术ID`, "
            "`基础数值`, `每10级成长`, `触发参数`, `技能描述` "
            "FROM `_仙门_技能` ORDER BY `门派ID`, `序号`");

        if (!result)
        {
            LOG_WARN("module", "仙门系统: 未找到技能配置，请先导入 sql/world/_仙门系统_配置.sql");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            XianmenSkillConfig skill;
            skill.id = fields[0].Get<uint32>();
            skill.factionId = fields[1].Get<uint32>();
            skill.order = fields[2].Get<uint32>();
            skill.name = fields[3].Get<std::string>();
            skill.type = fields[4].Get<uint32>();
            skill.spellId = fields[5].Get<uint32>();
            skill.baseValue = fields[6].Get<float>();
            skill.growthPer10 = fields[7].Get<float>();
            skill.triggerParam = fields[8].Get<std::string>();
            skill.description = fields[9].Get<std::string>();

            if (!skill.id || !skill.factionId)
                continue;

            _skills[skill.id] = skill;
            _skillsByFaction[skill.factionId].push_back(skill.id);
            if (skill.spellId)
                _skillsBySpell[skill.spellId] = skill.id;
        } while (result->NextRow());
    }

    void LoadDailyTemplates()
    {
        _dailyTemplates.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `模板ID`, `模板名称`, `模板类型`, `目标参数`, `默认数量`, `难度系数`, `默认奖励档` "
            "FROM `_仙门_日常` ORDER BY `模板ID`");

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            XianmenDailyTemplateConfig daily;
            daily.id = fields[0].Get<uint32>();
            daily.name = fields[1].Get<std::string>();
            daily.type = fields[2].Get<uint32>();
            daily.targetParam = fields[3].Get<std::string>();
            daily.defaultCount = std::max<uint32>(1, fields[4].Get<uint32>());
            daily.difficulty = std::max<float>(0.01f, fields[5].Get<float>());
            daily.defaultRewardTier = NormalizeRewardTier(fields[6].Get<uint32>());

            if (daily.id)
                _dailyTemplates[daily.id] = daily;
        } while (result->NextRow());
    }

    void LoadRewardConfigs()
    {
        _rewardConfigs.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `奖励类型`, `奖励名称`, `低档数量`, `中档数量`, `高档数量`, `最高概率`, `物品ID` "
            "FROM `_仙门_奖励` ORDER BY `奖励类型`");

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            XianmenRewardConfig reward;
            reward.type = fields[0].Get<uint32>();
            reward.name = fields[1].Get<std::string>();
            reward.lowAmount = fields[2].Get<uint32>();
            reward.middleAmount = fields[3].Get<uint32>();
            reward.highAmount = fields[4].Get<uint32>();
            reward.maxChance = fields[5].Get<float>();
            reward.itemId = fields[6].Get<uint32>();

            if (reward.type)
                _rewardConfigs[reward.type] = reward;
        } while (result->NextRow());
    }

    void EnsureLeaderRows()
    {
        for (auto const& pair : _factions)
        {
            CharacterDatabase.Execute(
                "INSERT IGNORE INTO `_仙门_门派状态` (`门派ID`, `当前门主GUID`, `结算日期`, `结算时间`, `激活技能`, `更新时间`) "
                "VALUES ({}, 0, 0, 0, '', 0)",
                pair.first);
        }
    }

    void LoadActiveSkills()
    {
        _activeSkills.clear();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `门派ID`, `激活技能` FROM `_仙门_门派状态` ORDER BY `门派ID`");

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 factionId = fields[0].Get<uint32>();
            std::set<uint32> skillIds = ParseUIntSet(fields[1].Get<std::string>());

            if (!GetFaction(factionId))
                continue;

            for (uint32 skillId : skillIds)
                if (GetSkill(skillId))
                    _activeSkills[factionId].insert(skillId);
        } while (result->NextRow());
    }

    void LoadLeaders()
    {
        _leaders.clear();
        _leaderSettleDates.clear();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `门派ID`, `当前门主GUID`, `结算日期` FROM `_仙门_门派状态` ORDER BY `门派ID`");

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 factionId = fields[0].Get<uint32>();
            _leaders[factionId] = fields[1].Get<uint32>();
            _leaderSettleDates[factionId] = fields[2].Get<uint32>();
        } while (result->NextRow());
    }

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        XianmenPlayerData data;

        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `门派ID`, `修为等级`, `当日贡献`, `历史贡献`, `已解锁技能`, `个人生效技能` FROM `_仙门_玩家` WHERE `角色GUID` = {}", guid))
        {
            Field* fields = result->Fetch();
            data.factionId = fields[0].Get<uint32>();
            data.level = fields[1].Get<uint32>();
            data.dailyContribution = fields[2].Get<uint64>();
            data.historyContribution = ParseUInt64(fields[3].Get<std::string>());
            data.unlockedSkills = ParseUIntSet(fields[4].Get<std::string>());
            data.personalActiveSkills = ParseUIntSet(fields[5].Get<std::string>());
        }

        std::set<uint32> const oldPersonalSkills = data.personalActiveSkills;
        data.personalActiveSkills = FilterPersonalActiveSkills(data.factionId, data.personalActiveSkills, data.unlockedSkills);
        if (data.personalActiveSkills != oldPersonalSkills)
        {
            std::string personalText = JoinUIntSet(data.personalActiveSkills);
            std::string escapedPersonalText = personalText;
            CharacterDatabase.EscapeString(escapedPersonalText);
            CharacterDatabase.Execute(
                "UPDATE `_仙门_玩家` SET `个人生效技能` = '{}', `更新时间` = {} WHERE `角色GUID` = {}",
                escapedPersonalText, GetNow(), guid);
            LOG_INFO("server.loading", "[仙门定位-个人生效清理] GUID={} 门派={} 原个人生效=[{}] 新个人生效=[{}] 门主开放=[{}] 已解锁=[{}] 原因=加载玩家数据",
                guid, data.factionId, JoinUIntSet(oldPersonalSkills), personalText, JoinUIntSet(GetActiveSkills(data.factionId)), JoinUIntSet(data.unlockedSkills));
        }

        _playerData[guid] = data;
    }

    void RefreshSkillDirectBonuses(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        int32 targetSpellPenetration = CalculateSkillSpellPenetrationBonus(player);
        int32 currentSpellPenetration = _appliedSkillSpellPenetration[guid];
        if (targetSpellPenetration == currentSpellPenetration)
            return;

        if (targetSpellPenetration > currentSpellPenetration)
            player->ApplySpellPenetrationBonus(targetSpellPenetration - currentSpellPenetration, true);
        else
            player->ApplySpellPenetrationBonus(currentSpellPenetration - targetSpellPenetration, false);

        _appliedSkillSpellPenetration[guid] = targetSpellPenetration;

        XianmenPlayerData const* data = GetPlayerData(guid);
        LOG_DEBUG("server.loading", "[仙门定位-法术穿透刷新] 玩家={} GUID={} 门派={} 实际生效=[{}] 技能法穿={} 原技能法穿={} 当前总法穿={}",
            player->GetName(), guid, data ? data->factionId : 0, JoinXianmenSkillIds(GetEffectiveSkills(player)),
            targetSpellPenetration, currentSpellPenetration, player->GetSpellPenetrationItemMod());
    }

    void ClearSkillDirectBonuses(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _appliedSkillSpellPenetration.find(guid);
        if (itr != _appliedSkillSpellPenetration.end() && itr->second > 0)
            player->ApplySpellPenetrationBonus(itr->second, false);

        _appliedSkillSpellPenetration.erase(guid);
    }

    void UnloadPlayerData(uint32 guid)
    {
        _playerData.erase(guid);
        _appliedSkillSpellPenetration.erase(guid);
    }

    void DeletePlayerData(uint32 guid)
    {
        _playerData.erase(guid);
        _appliedSkillSpellPenetration.erase(guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_玩家` WHERE `角色GUID` = {}", guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_玩家日常记录` WHERE `角色GUID` = {}", guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_仙器玩家槽位` WHERE `角色GUID` = {}", guid);

        for (auto& pair : _leaders)
        {
            if (pair.second == guid)
            {
                pair.second = 0;
                CharacterDatabase.Execute("UPDATE `_仙门_门派状态` SET `当前门主GUID` = 0, `更新时间` = {} WHERE `门派ID` = {}",
                    GetNow(), pair.first);
            }
        }
    }

    XianmenFactionConfig const* GetFaction(uint32 factionId) const
    {
        auto itr = _factions.find(factionId);
        return itr == _factions.end() ? nullptr : &itr->second;
    }

    XianmenSkillConfig const* GetSkill(uint32 skillId) const
    {
        auto itr = _skills.find(skillId);
        return itr == _skills.end() ? nullptr : &itr->second;
    }

    XianmenSkillConfig const* GetSkillBySpell(uint32 spellId) const
    {
        auto itr = _skillsBySpell.find(spellId);
        return itr == _skillsBySpell.end() ? nullptr : GetSkill(itr->second);
    }

    XianmenSkillConfig const* GetSkillByFactionOrder(uint32 factionId, uint32 order) const
    {
        auto byFaction = _skillsByFaction.find(factionId);
        if (byFaction == _skillsByFaction.end())
            return nullptr;

        for (uint32 skillId : byFaction->second)
        {
            XianmenSkillConfig const* skill = GetSkill(skillId);
            if (skill && skill->order == order)
                return skill;
        }

        return nullptr;
    }

    XianmenDailyTemplateConfig const* GetDailyTemplate(uint32 templateId) const
    {
        auto itr = _dailyTemplates.find(templateId);
        return itr == _dailyTemplates.end() ? nullptr : &itr->second;
    }

    XianmenRewardConfig const* GetRewardConfig(uint32 rewardType) const
    {
        auto itr = _rewardConfigs.find(rewardType);
        return itr == _rewardConfigs.end() ? nullptr : &itr->second;
    }

    XianmenPlayerData const* GetPlayerData(uint32 guid) const
    {
        auto itr = _playerData.find(guid);
        return itr == _playerData.end() ? nullptr : &itr->second;
    }

    std::vector<XianmenFactionConfig> GetFactions() const
    {
        std::vector<XianmenFactionConfig> result;
        result.reserve(_factions.size());

        for (auto const& pair : _factions)
            result.push_back(pair.second);

        std::sort(result.begin(), result.end(), [](XianmenFactionConfig const& left, XianmenFactionConfig const& right)
        {
            return left.id < right.id;
        });

        return result;
    }

    std::vector<XianmenSkillConfig const*> GetSkillsForFaction(uint32 factionId) const
    {
        std::vector<XianmenSkillConfig const*> result;

        auto byFaction = _skillsByFaction.find(factionId);
        if (byFaction == _skillsByFaction.end())
            return result;

        result.reserve(byFaction->second.size());
        for (uint32 skillId : byFaction->second)
            if (XianmenSkillConfig const* skill = GetSkill(skillId))
                result.push_back(skill);

        return result;
    }

    std::vector<XianmenDailyTemplateConfig> GetDailyTemplates() const
    {
        std::vector<XianmenDailyTemplateConfig> result;
        result.reserve(_dailyTemplates.size());

        for (auto const& pair : _dailyTemplates)
            result.push_back(pair.second);

        std::sort(result.begin(), result.end(), [](XianmenDailyTemplateConfig const& left, XianmenDailyTemplateConfig const& right)
        {
            return left.id < right.id;
        });

        return result;
    }

    int32 CalculateSkillSpellPenetrationBonus(Player* player) const
    {
        if (!player)
            return 0;

        int32 bonus = 0;
        uint32 const level = GetPlayerXianmenLevel(player);
        for (XianmenSkillConfig const* skill : GetEffectiveSkills(player))
        {
            if (!skill || skill->factionId != XIANMEN_FULU_FACTION_ID || GetXianmenSkillOrder(*skill) != 2)
                continue;

            bonus = std::min<int32>(std::numeric_limits<int32>::max() - bonus, XianmenFloatToInt32(GetXianmenSkillValue(*skill, level))) + bonus;
        }

        return bonus;
    }

    std::vector<XianmenDailyView> GetTodayDailies(Player* player)
    {
        std::vector<XianmenDailyView> result;
        if (!player)
            return result;

        LoadPlayerData(player);
        XianmenPlayerData const* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data || !data->factionId)
            return result;

        uint32 today = GetTodayDate();
        if (!today)
            return result;

        QueryResult query = CharacterDatabase.Query(
            "SELECT d.`日常ID`, d.`模板ID`, d.`目标数量`, d.`奖励档`, d.`奖励类型`, "
            "IFNULL(r.`进度`, 0), IFNULL(r.`已完成`, 0) "
            "FROM `_仙门_当日日常` d "
            "LEFT JOIN `_仙门_玩家日常记录` r ON r.`角色GUID` = {} AND r.`日常ID` = d.`日常ID` AND r.`日期` = d.`日期` "
            "WHERE d.`日期` = {} AND d.`门派ID` = {} ORDER BY d.`日常ID`",
            player->GetGUID().GetCounter(), today, data->factionId);

        if (!query)
            return result;

        do
        {
            Field* fields = query->Fetch();
            XianmenDailyView daily;
            daily.dailyId = fields[0].Get<uint32>();
            daily.templateId = fields[1].Get<uint32>();
            daily.targetCount = fields[2].Get<uint32>();
            daily.rewardTier = NormalizeRewardTier(fields[3].Get<uint32>());
            daily.rewardType = fields[4].Get<uint32>();
            daily.progress = fields[5].Get<uint32>();
            daily.completed = fields[6].Get<uint8>() != 0;

            if (XianmenDailyTemplateConfig const* tmpl = GetDailyTemplate(daily.templateId))
            {
                daily.templateName = tmpl->name;
                daily.templateType = tmpl->type;
                daily.rewardAmount = CalculateRewardAmount(daily.rewardType, daily.rewardTier, tmpl->difficulty);
            }

            if (XianmenRewardConfig const* reward = GetRewardConfig(daily.rewardType))
                daily.rewardName = reward->name;

            result.push_back(daily);
        } while (query->NextRow());

        return result;
    }

    uint32 GetUnlockedSlotCount(XianmenPlayerData const& data) const
    {
        return std::min<uint32>(10, data.level / _unlockLevelStep);
    }

    uint32 GetMaxLevel() const
    {
        return _maxLevel;
    }

    uint32 GetFactionMaxLevel(uint32 factionId) const
    {
        XianmenFactionConfig const* faction = GetFaction(factionId);
        if (faction && faction->maxLevel > 0)
            return faction->maxLevel;

        return _maxLevel;
    }

    uint64 GetFormulaUpgradeRequirement(uint32 currentLevel, uint32 maxLevel = 0) const
    {
        uint32 effectiveMaxLevel = maxLevel ? maxLevel : _maxLevel;
        if (currentLevel >= effectiveMaxLevel)
            return 0;

        uint64 level = std::max<uint32>(1, currentLevel);
        uint64 linear = SaturatingMul(level, _upgradeRequirementBase);
        uint64 growthSteps = SaturatingMul(level, level - 1) / 2;
        return SaturatingAdd(linear, SaturatingMul(growthSteps, _upgradeRequirementPerLevel));
    }

    XianmenUpgradeRequirementConfig GetNextUpgradeRequirementConfig(uint32 factionId, uint32 currentLevel, uint32 maxLevel = 0) const
    {
        XianmenUpgradeRequirementConfig requirement;
        uint32 effectiveMaxLevel = maxLevel ? maxLevel : GetFactionMaxLevel(factionId);
        if (currentLevel >= effectiveMaxLevel)
            return requirement;

        uint32 targetLevel = currentLevel + 1;
        auto factionItr = _upgradeRequirements.find(MakeXianmenUpgradeRequirementKey(factionId, targetLevel));
        if (factionItr != _upgradeRequirements.end())
            return factionItr->second;

        auto defaultItr = _upgradeRequirements.find(MakeXianmenUpgradeRequirementKey(0, targetLevel));
        if (defaultItr != _upgradeRequirements.end())
            return defaultItr->second;

        requirement.factionId = factionId;
        requirement.targetLevel = targetLevel;
        requirement.historyContribution = GetFormulaUpgradeRequirement(currentLevel, effectiveMaxLevel);
        requirement.requirementId = 0;
        return requirement;
    }

    uint64 GetNextUpgradeRequirement(uint32 factionId, uint32 currentLevel, uint32 maxLevel = 0) const
    {
        return GetNextUpgradeRequirementConfig(factionId, currentLevel, maxLevel).historyContribution;
    }

    bool IsLeader(uint32 factionId, uint32 guid) const
    {
        auto itr = _leaders.find(factionId);
        return itr != _leaders.end() && itr->second != 0 && itr->second == guid;
    }

    uint32 GetLeaderGuid(uint32 factionId) const
    {
        auto itr = _leaders.find(factionId);
        return itr == _leaders.end() ? 0 : itr->second;
    }

    std::set<uint32> const& GetActiveSkills(uint32 factionId) const
    {
        static std::set<uint32> empty;
        auto itr = _activeSkills.find(factionId);
        return itr == _activeSkills.end() ? empty : itr->second;
    }

    std::vector<XianmenSkillConfig const*> GetEffectiveSkills(Player* player) const
    {
        std::vector<XianmenSkillConfig const*> result;
        if (!player)
            return result;

        std::set<uint32> added;
        XianmenPlayerData const* data = GetPlayerData(player->GetGUID().GetCounter());

        // 非仙门玩家短路：本函数挂在所有伤害/属性钩子上，多数玩家不该付遍历的代价
        if (!data || !data->factionId)
            return result;

        // 【冗余段删除】原先这里还有一段玩家全光环遍历（GetAppliedAuras 逐光环反查仙门技能），
        // 其收集条件（门主开放+已解锁+个人选择三重过滤）是下方个人技能循环的真子集，
        // added 去重下产出完全重合——纯重复劳动。仙门玩家单次伤害本函数被调 6-8 次、
        // 一次 UpdateAllStats 调 30-50 次，删除后只剩 O(个人技能数) 的集合查找。

        std::set<uint32> const& activeSkills = GetActiveSkills(data->factionId);
        uint32 personalCount = 0;
        for (uint32 skillId : data->personalActiveSkills)
        {
            if (personalCount >= _maxPersonalActiveSkills)
                break;

            if (!activeSkills.count(skillId))
                continue;

            if (!data->unlockedSkills.count(skillId))
                continue;

            if (XianmenSkillConfig const* skill = GetSkill(skillId))
            {
                if (added.insert(skill->id).second)
                {
                    result.push_back(skill);
                    ++personalCount;
                }
            }
        }

        return result;
    }

    uint32 GetPlayerXianmenLevel(Player* player) const
    {
        if (!player)
            return 0;

        XianmenPlayerData const* data = GetPlayerData(player->GetGUID().GetCounter());
        return data ? data->level : 0;
    }

    uint32 GetPlayerFactionId(Player* player) const
    {
        if (!player)
            return 0;

        XianmenPlayerData const* data = GetPlayerData(player->GetGUID().GetCounter());
        return data ? data->factionId : 0;
    }

    bool CheckAndConsumeRequirement(Player* player, uint32 requirementId, char const* actionName, std::string& error)
    {
        if (!requirementId)
            return true;

        RequirementInterface* reqModule = GetXianmenRequirementModule();
        if (!reqModule)
        {
            error = std::string(actionName ? actionName : "操作") + "需要需求系统，但当前未加载。";
            return false;
        }

        if (!reqModule->CheckRequirements(player, requirementId, false))
        {
            reqModule->CheckRequirements(player, requirementId, true);
            error = std::string(actionName ? actionName : "操作") + "需求未满足。";
            return false;
        }

        if (!reqModule->ConsumeRequirements(player, requirementId))
        {
            error = std::string(actionName ? actionName : "操作") + "消耗需求失败。";
            return false;
        }

        return true;
    }

    bool JoinFaction(Player* player, uint32 factionId, std::string& error)
    {
        if (!player)
            return false;

        XianmenFactionConfig const* faction = GetFaction(factionId);
        if (!faction)
        {
            error = "门派不存在。";
            return false;
        }

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData const* data = GetPlayerData(guid);
        if (data && data->factionId)
        {
            if (data->factionId == factionId)
            {
                error = "你已经属于该仙门。";
                return false;
            }

            XianmenPlayerData oldData = *data;
            if (!CheckAndConsumeRequirement(player, faction->joinRequirementId, "转投仙门", error))
                return false;

            uint32 oldFactionId = oldData.factionId;
            uint32 keptLevel = 1;
            uint32 targetMaxLevel = GetFactionMaxLevel(factionId);
            if (_transferKeepLevelPct > 0.0f)
                keptLevel = std::max<uint32>(1, std::min<uint32>(targetMaxLevel, static_cast<uint32>(std::floor(static_cast<float>(oldData.level) * _transferKeepLevelPct / 100.0f))));

            uint32 now = GetNow();
            CharacterDatabase.Execute(
                "REPLACE INTO `_仙门_玩家` (`角色GUID`, `门派ID`, `修为等级`, `当日贡献`, `历史贡献`, `已解锁技能`, `个人生效技能`, `加入时间`, `更新时间`) "
                "VALUES ({}, {}, {}, 0, {}, '', '', {}, {})",
                guid, factionId, keptLevel, oldData.historyContribution, now, now);

            if (IsLeader(oldFactionId, guid))
            {
                _leaders[oldFactionId] = 0;
                _activeSkills[oldFactionId].clear();
                CharacterDatabase.Execute(
                    "UPDATE `_仙门_门派状态` SET `当前门主GUID` = 0, `激活技能` = '', `更新时间` = {} WHERE `门派ID` = {}",
                    now, oldFactionId);
            }

            LoadPlayerData(player);
            RefreshPassiveAuras(player);

            XianmenFactionConfig const* oldFaction = GetFaction(oldFactionId);
            LOG_INFO("server.loading", "仙门系统: 玩家 {} 从 {} 转投 {}，修为 {} -> {}",
                player->GetName(),
                oldFaction ? oldFaction->name : "未知门派",
                faction->name,
                oldData.level,
                keptLevel);
            return true;
        }

        if (!CheckAndConsumeRequirement(player, faction->joinRequirementId, "加入仙门", error))
            return false;

        uint32 now = GetNow();
        CharacterDatabase.Execute(
            "REPLACE INTO `_仙门_玩家` (`角色GUID`, `门派ID`, `修为等级`, `当日贡献`, `历史贡献`, `已解锁技能`, `个人生效技能`, `加入时间`, `更新时间`) "
            "VALUES ({}, {}, 1, 0, 0, '', '', {}, {})",
            guid, factionId, now, now);

        LoadPlayerData(player);
        RefreshPassiveAuras(player);

        LOG_INFO("server.loading", "仙门系统: 玩家 {} 加入 {}", player->GetName(), faction->name);
        return true;
    }

    bool LeaveFaction(Player* player, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);
        XianmenPlayerData const* data = GetPlayerData(guid);

        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        uint32 factionId = data->factionId;
        CharacterDatabase.Execute("DELETE FROM `_仙门_玩家` WHERE `角色GUID` = {}", guid);

        if (IsLeader(factionId, guid))
        {
            _leaders[factionId] = 0;
            _activeSkills[factionId].clear();
            CharacterDatabase.Execute(
                "UPDATE `_仙门_门派状态` SET `当前门主GUID` = 0, `激活技能` = '', `更新时间` = {} WHERE `门派ID` = {}",
                GetNow(), factionId);
        }

        _playerData[guid] = XianmenPlayerData();
        RefreshPassiveAuras(player);
        return true;
    }

    bool UnlockSkill(Player* player, uint32 skillId, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData* data = GetMutablePlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        XianmenSkillConfig const* skill = GetSkill(skillId);
        if (!skill || skill->factionId != data->factionId)
        {
            error = "该技能不属于你的门派。";
            return false;
        }

        if (data->unlockedSkills.count(skillId))
        {
            error = "你已经解锁该技能。";
            return false;
        }

        uint32 slotCount = GetUnlockedSlotCount(*data);
        if (data->unlockedSkills.size() >= slotCount)
        {
            error = "当前修为等级没有可用的技能解锁名额。";
            return false;
        }

        uint32 now = GetNow();
        data->unlockedSkills.insert(skillId);
        std::string unlocked = JoinUIntSet(data->unlockedSkills);
        CharacterDatabase.EscapeString(unlocked);
        CharacterDatabase.Execute(
            "UPDATE `_仙门_玩家` SET `已解锁技能` = '{}', `更新时间` = {} WHERE `角色GUID` = {}",
            unlocked, now, guid);
        RefreshPassiveAuras(player);
        return true;
    }

    bool SetActiveSkills(Player* player, std::vector<uint32> const& skillIds, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData* data = GetMutablePlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        if (!IsLeader(data->factionId, guid))
        {
            error = "只有当前门主可以设置门派生效技能。";
            return false;
        }

        uint32 const factionId = data->factionId;
        std::set<uint32> uniqueSkills(skillIds.begin(), skillIds.end());
        std::string const requestedSkills = JoinUIntSet(uniqueSkills);
        std::string const oldActiveSkills = JoinUIntSet(GetActiveSkills(factionId));

        LOG_INFO("server.loading", "[仙门定位-门主开放] 玩家={} GUID={} 门派={} 请求开放=[{}] 原门主开放=[{}] 最大开放={}",
            player->GetName(), guid, factionId, requestedSkills, oldActiveSkills, _maxActiveSkills);

        if (uniqueSkills.size() > _maxActiveSkills)
        {
            std::ostringstream stream;
            stream << "门主最多只能开放 " << _maxActiveSkills << " 个门派技能。";
            error = stream.str();
            LOG_INFO("server.loading", "[仙门定位-门主开放失败] 玩家={} GUID={} 门派={} 请求开放=[{}] 原因=超过最大开放数量",
                player->GetName(), guid, factionId, requestedSkills);
            return false;
        }

        for (uint32 skillId : uniqueSkills)
        {
            XianmenSkillConfig const* skill = GetSkill(skillId);
            if (!skill || skill->factionId != factionId)
            {
                error = "存在不属于本门派的技能。";
                LOG_INFO("server.loading", "[仙门定位-门主开放失败] 玩家={} GUID={} 门派={} 技能={} 技能门派={} 原因=技能不存在或不属于本门派",
                    player->GetName(), guid, factionId, skillId, skill ? skill->factionId : 0);
                return false;
            }

        }

        uint32 now = GetNow();
        std::string activeText = JoinUIntSet(uniqueSkills);
        std::string active = activeText;
        CharacterDatabase.EscapeString(active);
        CharacterDatabase.Execute(
            "UPDATE `_仙门_门派状态` SET `激活技能` = '{}', `更新时间` = {} WHERE `门派ID` = {}",
            active, now, factionId);
        _activeSkills[factionId] = uniqueSkills;
        uint32 const cleanedPersonalCount = PrunePersonalActiveSkillsForFaction(factionId, uniqueSkills, now);
        RefreshFactionPassiveAuras(factionId);

        XianmenFactionConfig const* faction = GetFaction(factionId);
        LOG_INFO("server.loading", "仙门系统: 门主 {} 设置 {} 门派开放技能数={} 清理个人选择数={}",
            player->GetName(), faction ? faction->name : "未知门派", static_cast<uint32>(uniqueSkills.size()), cleanedPersonalCount);
        LOG_INFO("server.loading", "[仙门定位-门主开放成功] 玩家={} GUID={} 门派={} 保存门主开放=[{}] 数量={} 清理个人选择数={}",
            player->GetName(), guid, factionId, activeText, static_cast<uint32>(uniqueSkills.size()), cleanedPersonalCount);
        return true;
    }

    bool SetPersonalActiveSkills(Player* player, std::vector<uint32> const& skillIds, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData* data = GetMutablePlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        std::set<uint32> uniqueSkills(skillIds.begin(), skillIds.end());
        if (uniqueSkills.size() > _maxPersonalActiveSkills)
        {
            std::ostringstream stream;
            stream << "成员最多只能选择 " << _maxPersonalActiveSkills << " 个个人生效技能。";
            error = stream.str();
            LOG_INFO("server.loading", "[仙门定位-个人生效失败] 玩家={} GUID={} 门派={} 请求个人生效=[{}] 原因=超过最大个人生效数量",
                player->GetName(), guid, data->factionId, JoinUIntSet(uniqueSkills));
            return false;
        }

        std::set<uint32> const& factionActiveSkills = GetActiveSkills(data->factionId);
        std::string const requestedSkills = JoinUIntSet(uniqueSkills);
        std::string const activeSkillsText = JoinUIntSet(factionActiveSkills);
        std::string const unlockedSkillsText = JoinUIntSet(data->unlockedSkills);
        std::string const oldPersonalSkills = JoinUIntSet(data->personalActiveSkills);
        LOG_INFO("server.loading", "[仙门定位-个人生效] 玩家={} GUID={} 门派={} 请求个人生效=[{}] 原个人生效=[{}] 门主开放=[{}] 已解锁=[{}] 最大个人生效={}",
            player->GetName(), guid, data->factionId, requestedSkills, oldPersonalSkills, activeSkillsText, unlockedSkillsText, _maxPersonalActiveSkills);

        for (uint32 skillId : uniqueSkills)
        {
            XianmenSkillConfig const* skill = GetSkill(skillId);
            if (!skill || skill->factionId != data->factionId)
            {
                error = "存在不属于本门派的技能。";
                LOG_INFO("server.loading", "[仙门定位-个人生效失败] 玩家={} GUID={} 门派={} 技能={} 技能门派={} 原因=技能不存在或不属于本门派",
                    player->GetName(), guid, data->factionId, skillId, skill ? skill->factionId : 0);
                return false;
            }

            if (!factionActiveSkills.count(skillId))
            {
                error = "只能从门主开放的技能中选择。";
                LOG_INFO("server.loading", "[仙门定位-个人生效失败] 玩家={} GUID={} 门派={} 技能={} 门主开放=[{}] 原因=门主未开放",
                    player->GetName(), guid, data->factionId, skillId, activeSkillsText);
                return false;
            }

            if (!data->unlockedSkills.count(skillId))
            {
                error = "只能选择自己已解锁的技能。";
                LOG_INFO("server.loading", "[仙门定位-个人生效失败] 玩家={} GUID={} 门派={} 技能={} 已解锁=[{}] 原因=玩家未解锁",
                    player->GetName(), guid, data->factionId, skillId, unlockedSkillsText);
                return false;
            }
        }

        uint32 now = GetNow();
        std::string personalText = JoinUIntSet(uniqueSkills);
        std::string active = personalText;
        CharacterDatabase.EscapeString(active);
        CharacterDatabase.Execute(
            "UPDATE `_仙门_玩家` SET `个人生效技能` = '{}', `更新时间` = {} WHERE `角色GUID` = {}",
            active, now, guid);
        data->personalActiveSkills = uniqueSkills;
        RefreshPassiveAuras(player);

        LOG_INFO("server.loading", "仙门系统: 玩家 {} 设置个人生效技能数={}",
            player->GetName(), static_cast<uint32>(uniqueSkills.size()));
        LOG_INFO("server.loading", "[仙门定位-个人生效成功] 玩家={} GUID={} 门派={} 保存个人生效=[{}] 数量={}",
            player->GetName(), guid, data->factionId, personalText, static_cast<uint32>(uniqueSkills.size()));
        return true;
    }

    bool SetPlayerLevel(Player* player, uint32 level, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData* data = GetMutablePlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "目标玩家尚未加入仙门。";
            return false;
        }

        uint32 maxLevel = GetFactionMaxLevel(data->factionId);
        level = std::max<uint32>(1, std::min<uint32>(maxLevel, level));
        CharacterDatabase.Execute(
            "UPDATE `_仙门_玩家` SET `修为等级` = {}, `更新时间` = {} WHERE `角色GUID` = {}",
            level, GetNow(), guid);

        data->level = level;
        RefreshPassiveAuras(player);
        return true;
    }

    bool UpgradePlayerLevel(Player* player, uint32 count, uint32& upgraded, std::string& error)
    {
        upgraded = 0;
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData* data = GetMutablePlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        uint32 maxLevel = GetFactionMaxLevel(data->factionId);
        if (data->level >= maxLevel)
        {
            error = "当前门派修为已满级。";
            return false;
        }

        count = std::max<uint32>(1, std::min<uint32>(count, maxLevel));
        while (upgraded < count && data->level < maxLevel)
        {
            XianmenUpgradeRequirementConfig requirement = GetNextUpgradeRequirementConfig(data->factionId, data->level, maxLevel);
            uint64 required = requirement.historyContribution;
            if (data->historyContribution < required)
            {
                if (!upgraded)
                {
                    std::ostringstream stream;
                    stream << "历史贡献不足，下一等级需要累计历史贡献 " << required << "。";
                    error = stream.str();
                    return false;
                }

                break;
            }

            if (requirement.requirementId)
            {
                std::string requirementError;
                if (!CheckAndConsumeRequirement(player, requirement.requirementId, "提升修为", requirementError))
                {
                    if (!upgraded)
                    {
                        error = requirementError;
                        return false;
                    }

                    break;
                }
            }

            ++data->level;
            ++upgraded;
        }

        CharacterDatabase.Execute(
            "UPDATE `_仙门_玩家` SET `修为等级` = {}, `更新时间` = {} WHERE `角色GUID` = {}",
            data->level, GetNow(), guid);

        RefreshPassiveAuras(player);
        return upgraded > 0;
    }

    bool AddContribution(Player* player, uint64 value, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData* data = GetMutablePlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "目标玩家尚未加入仙门。";
            return false;
        }

        CharacterDatabase.Execute(
            "UPDATE `_仙门_玩家` SET `当日贡献` = `当日贡献` + {}, `历史贡献` = `历史贡献` + {}, `更新时间` = {} WHERE `角色GUID` = {}",
            value, value, GetNow(), guid);

        data->dailyContribution += value;
        data->historyContribution = SaturatingAdd(data->historyContribution, value);
        return true;
    }

    bool PublishDaily(Player* player, uint32 templateId, uint32 targetCount, uint32 rewardTier, uint32 rewardType, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData const* data = GetPlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        if (!IsLeader(data->factionId, guid))
        {
            error = "只有当前门主可以发布日常。";
            return false;
        }

        XianmenDailyTemplateConfig const* tmpl = GetDailyTemplate(templateId);
        if (!tmpl)
        {
            error = "日常模板不存在。";
            return false;
        }

        rewardTier = rewardTier ? NormalizeRewardTier(rewardTier) : tmpl->defaultRewardTier;
        rewardType = rewardType ? rewardType : 2;

        XianmenRewardConfig const* reward = GetRewardConfig(rewardType);
        if (!reward)
        {
            error = "奖励类型不存在。";
            return false;
        }

        if (!IsRewardPublishable(*reward))
        {
            error = "该奖励类型没有配置可发放物品，暂不能发布。";
            return false;
        }

        uint32 today = GetTodayDate();
        if (!today)
        {
            error = "无法获取当前日期。";
            return false;
        }

        uint32 published = 0;
        if (QueryResult result = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM `_仙门_当日日常` WHERE `日期` = {} AND `门派ID` = {}",
            today, data->factionId))
        {
            published = (*result)[0].Get<uint32>();
        }

        if (published >= _maxDailyPublishes)
        {
            error = "今日发布日常数量已达到上限。";
            return false;
        }

        targetCount = targetCount ? targetCount : tmpl->defaultCount;
        targetCount = std::max<uint32>(1, std::min<uint32>(targetCount, 1000000));

        CharacterDatabase.Execute(
            "INSERT INTO `_仙门_当日日常` (`日期`, `门派ID`, `模板ID`, `发布者GUID`, `目标数量`, `奖励档`, `奖励类型`, `创建时间`) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {})",
            today, data->factionId, templateId, guid, targetCount, rewardTier, rewardType, GetNow());

        XianmenFactionConfig const* faction = GetFaction(data->factionId);
        LOG_INFO("server.loading", "仙门系统: 门主 {} 发布 {} 日常 {} 奖励={} 档位={} 目标={}",
            player->GetName(), faction ? faction->name : "未知门派", tmpl->name, reward->name, rewardTier, targetCount);
        return true;
    }

    bool CompleteDaily(Player* player, uint32 dailyId, std::string& error)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);

        XianmenPlayerData const* data = GetPlayerData(guid);
        if (!data || !data->factionId)
        {
            error = "你尚未加入仙门。";
            return false;
        }

        uint32 today = GetTodayDate();
        if (!today)
        {
            error = "无法获取当前日期。";
            return false;
        }

        QueryResult dailyResult = CharacterDatabase.Query(
            "SELECT `模板ID`, `目标数量`, `奖励档`, `奖励类型` FROM `_仙门_当日日常` "
            "WHERE `日常ID` = {} AND `日期` = {} AND `门派ID` = {}",
            dailyId, today, data->factionId);

        if (!dailyResult)
        {
            error = "今日没有这个门派日常。";
            return false;
        }

        Field* fields = dailyResult->Fetch();
        uint32 templateId = fields[0].Get<uint32>();
        uint32 targetCount = std::max<uint32>(1, fields[1].Get<uint32>());
        uint32 rewardTier = NormalizeRewardTier(fields[2].Get<uint32>());
        uint32 rewardType = fields[3].Get<uint32>();

        XianmenDailyTemplateConfig const* tmpl = GetDailyTemplate(templateId);
        if (!tmpl)
        {
            error = "日常模板不存在。";
            return false;
        }

        // 【安全修复】完成前校验进度。原实现只查"是否已完成"，任何玩家发一条
        // COMPLETE_DAILY:<id> 即可白拿全部日常奖励（模块内没有任何写进度的代码，
        // 真正可玩的日常走原生任务系统 383101-383200）。在进度追踪实现之前，
        // 模块侧日常必须攒满 `进度 >= 目标数量` 才能领奖。
        uint32 progress = 0;
        if (QueryResult record = CharacterDatabase.Query(
            "SELECT `进度`, `已完成` FROM `_仙门_玩家日常记录` WHERE `角色GUID` = {} AND `日常ID` = {} AND `日期` = {}",
            guid, dailyId, today))
        {
            Field* recordFields = record->Fetch();
            progress = recordFields[0].Get<uint32>();

            if (recordFields[1].Get<uint8>() != 0)
            {
                error = "你今天已经完成过这个日常。";
                return false;
            }
        }

        if (progress < targetCount)
        {
            error = Acore::StringFormat("日常进度不足（{}/{}），尚不能领取奖励。", progress, targetCount);
            return false;
        }

        XianmenRewardConfig const* reward = GetRewardConfig(rewardType);
        if (!reward)
        {
            error = "日常奖励配置不存在。";
            return false;
        }

        uint32 rewardAmount = CalculateRewardAmount(rewardType, rewardTier, tmpl->difficulty);
        if (!GrantConfiguredReward(player, *reward, rewardAmount, error))
            return false;

        if (rewardType != 2)
        {
            if (XianmenRewardConfig const* contributionReward = GetRewardConfig(2))
            {
                uint32 contribution = CalculateRewardAmount(2, rewardTier, tmpl->difficulty);
                if (contribution)
                {
                    std::string contributionError;
                    AddContribution(player, contribution, contributionError);
                }
            }
        }

        CharacterDatabase.Execute(
            "REPLACE INTO `_仙门_玩家日常记录` (`角色GUID`, `日常ID`, `日期`, `进度`, `已完成`, `完成时间`) "
            "VALUES ({}, {}, {}, {}, 1, {})",
            guid, dailyId, today, targetCount, GetNow());

        LoadPlayerData(player);

        LOG_INFO("server.loading", "仙门系统: 玩家 {} 完成日常 {} 奖励={} 数量={}",
            player->GetName(), tmpl->name, reward->name, rewardAmount);
        return true;
    }

    bool SetLeader(Player* player, uint32 factionId, std::string& error)
    {
        if (!player)
            return false;

        if (!GetFaction(factionId))
        {
            error = "门派不存在。";
            return false;
        }

        uint32 guid = player->GetGUID().GetCounter();
        LoadPlayerData(player);
        XianmenPlayerData const* data = GetPlayerData(guid);

        if (!data || data->factionId != factionId)
        {
            error = "玩家必须先加入该门派。";
            return false;
        }

        uint32 today = GetTodayDate();
        uint32 now = GetNow();
        CharacterDatabase.Execute(
            "INSERT INTO `_仙门_门派状态` (`门派ID`, `当前门主GUID`, `结算日期`, `结算时间`, `激活技能`, `更新时间`) "
            "VALUES ({}, {}, {}, {}, '', {}) "
            "ON DUPLICATE KEY UPDATE `当前门主GUID` = {}, `结算日期` = {}, `结算时间` = {}, `更新时间` = {}",
            factionId, guid, today, now, now, guid, today, now, now);

        _leaders[factionId] = guid;
        _leaderSettleDates[factionId] = today;

        XianmenFactionConfig const* faction = GetFaction(factionId);
        LOG_INFO("server.loading", "仙门系统: GM 设置 {} 为 {} 门主", player->GetName(), faction ? faction->name : "未知门派");
        return true;
    }

    void RefreshPassiveAuras(Player* player)
    {
        if (!player)
            return;

        for (auto const& pair : _skills)
        {
            uint32 spellId = pair.second.spellId;
            if (spellId && sSpellMgr->GetSpellInfo(spellId) && player->HasAura(spellId))
                player->RemoveAura(spellId);
        }

        uint32 guid = player->GetGUID().GetCounter();
        XianmenPlayerData const* data = GetPlayerData(guid);
        if (!data || !data->factionId)
        {
            XianmenLingdunShieldValues.erase(guid);
            ClearSkillDirectBonuses(player);
            player->UpdateAllStats();
            NotifyPlayerAttributePanelRefresh(player);
            return;
        }

        std::set<uint32> const& activeSkills = GetActiveSkills(data->factionId);

        uint32 personalCount = 0;
        bool hasLingdun = false;
        for (uint32 skillId : data->personalActiveSkills)
        {
            if (personalCount >= _maxPersonalActiveSkills)
                break;

            if (!activeSkills.count(skillId))
                continue;

            if (!data->unlockedSkills.count(skillId))
                continue;

            XianmenSkillConfig const* skill = GetSkill(skillId);
            if (!skill || !skill->spellId)
                continue;

            if (!sSpellMgr->GetSpellInfo(skill->spellId))
                continue;

            player->AddAura(skill->spellId, player);
            if (skill->factionId == XIANMEN_FULU_FACTION_ID && GetXianmenSkillOrder(*skill) == 9)
                hasLingdun = true;

            ++personalCount;
        }

        RefreshSkillDirectBonuses(player);
        if (!hasLingdun)
            XianmenLingdunShieldValues.erase(guid);
        player->UpdateAllStats();
        NotifyPlayerAttributePanelRefresh(player);
    }

    void RefreshFactionPassiveAuras(uint32 factionId)
    {
        if (!factionId)
            return;

        WorldSessionMgr::SessionMap const& sessionMap = sWorldSessionMgr->GetAllSessions();
        for (auto const& pair : sessionMap)
        {
            WorldSession* session = pair.second;
            Player* onlinePlayer = session ? session->GetPlayer() : nullptr;
            if (!onlinePlayer || !onlinePlayer->IsInWorld())
                continue;

            LoadPlayerData(onlinePlayer);
            XianmenPlayerData const* onlineData = GetPlayerData(onlinePlayer->GetGUID().GetCounter());
            if (!onlineData || onlineData->factionId != factionId)
                continue;

            RefreshPassiveAuras(onlinePlayer);
        }
    }

    uint32 PrunePersonalActiveSkillsForFaction(uint32 factionId, std::set<uint32> const& activeSkills, uint32 now)
    {
        if (!factionId)
            return 0;

        uint32 cleanedCount = 0;
        QueryResult result = CharacterDatabase.Query(
            "SELECT `角色GUID`, `已解锁技能`, `个人生效技能` FROM `_仙门_玩家` WHERE `门派ID` = {}",
            factionId);

        if (!result)
            return 0;

        do
        {
            Field* fields = result->Fetch();
            uint32 memberGuid = fields[0].Get<uint32>();
            std::set<uint32> unlockedSkills = ParseUIntSet(fields[1].Get<std::string>());
            std::set<uint32> oldPersonalSkills = ParseUIntSet(fields[2].Get<std::string>());
            std::set<uint32> newPersonalSkills = FilterPersonalActiveSkills(factionId, oldPersonalSkills, unlockedSkills, &activeSkills);

            if (newPersonalSkills == oldPersonalSkills)
                continue;

            std::string newPersonalText = JoinUIntSet(newPersonalSkills);
            std::string escapedPersonalText = newPersonalText;
            CharacterDatabase.EscapeString(escapedPersonalText);
            CharacterDatabase.Execute(
                "UPDATE `_仙门_玩家` SET `个人生效技能` = '{}', `更新时间` = {} WHERE `角色GUID` = {}",
                escapedPersonalText, now, memberGuid);

            if (XianmenPlayerData* cachedData = GetMutablePlayerData(memberGuid))
                cachedData->personalActiveSkills = newPersonalSkills;

            ++cleanedCount;
            LOG_INFO("server.loading", "[仙门定位-个人生效清理] GUID={} 门派={} 原个人生效=[{}] 新个人生效=[{}] 门主开放=[{}] 已解锁=[{}]",
                memberGuid, factionId, JoinUIntSet(oldPersonalSkills), newPersonalText, JoinUIntSet(activeSkills), JoinUIntSet(unlockedSkills));
        } while (result->NextRow());

        return cleanedCount;
    }

    void SettleDailyLeaders(bool force = false)
    {
        if (!_enableDailyLeaderSettle && !force)
            return;

        uint32 today = GetTodayDate();
        if (!today)
            return;

        for (auto const& pair : _factions)
        {
            uint32 factionId = pair.first;
            uint32 settledDate = _leaderSettleDates[factionId];
            if (!force && settledDate == today)
                continue;

            uint32 newLeaderGuid = 0;
            if (QueryResult result = CharacterDatabase.Query(
                "SELECT `角色GUID` FROM `_仙门_玩家` WHERE `门派ID` = {} AND `当日贡献` > 0 "
                "ORDER BY `当日贡献` DESC, `加入时间` ASC LIMIT 1",
                factionId))
            {
                newLeaderGuid = (*result)[0].Get<uint32>();
            }

            CharacterDatabase.Execute(
                "INSERT INTO `_仙门_门派状态` (`门派ID`, `当前门主GUID`, `结算日期`, `结算时间`, `激活技能`, `更新时间`) "
                "VALUES ({}, {}, {}, {}, '', {}) "
                "ON DUPLICATE KEY UPDATE `当前门主GUID` = {}, `结算日期` = {}, `结算时间` = {}, `更新时间` = {}",
                factionId, newLeaderGuid, today, GetNow(), GetNow(), newLeaderGuid, today, GetNow(), GetNow());
            CharacterDatabase.Execute("UPDATE `_仙门_玩家` SET `当日贡献` = 0 WHERE `门派ID` = {}", factionId);

            _leaders[factionId] = newLeaderGuid;
            _leaderSettleDates[factionId] = today;

            for (auto& playerPair : _playerData)
            {
                if (playerPair.second.factionId == factionId)
                    playerPair.second.dailyContribution = 0;
            }

            LOG_INFO("server.loading", "仙门系统: {} 每日门主结算完成，门主GUID={}，贡献已清零",
                pair.second.name, newLeaderGuid);
        }
    }

private:
    XianmenMgr() = default;

    static uint64 SaturatingAdd(uint64 left, uint64 right)
    {
        uint64 maxValue = std::numeric_limits<uint64>::max();
        if (maxValue - left < right)
            return maxValue;

        return left + right;
    }

    static uint64 SaturatingMul(uint64 left, uint64 right)
    {
        if (!left || !right)
            return 0;

        uint64 maxValue = std::numeric_limits<uint64>::max();
        if (left > maxValue / right)
            return maxValue;

        return left * right;
    }

    uint32 NormalizeRewardTier(uint32 tier) const
    {
        if (tier < 1)
            return 1;

        if (tier > 3)
            return 3;

        return tier;
    }

    uint32 GetRewardBaseAmount(XianmenRewardConfig const& reward, uint32 tier) const
    {
        switch (NormalizeRewardTier(tier))
        {
            case 1:
                return reward.lowAmount;
            case 2:
                return reward.middleAmount;
            case 3:
                return reward.highAmount;
            default:
                return reward.lowAmount;
        }
    }

    uint32 CalculateRewardAmount(uint32 rewardType, uint32 tier, float difficulty) const
    {
        XianmenRewardConfig const* reward = GetRewardConfig(rewardType);
        if (!reward)
            return 0;

        uint32 baseAmount = GetRewardBaseAmount(*reward, tier);
        if (!baseAmount)
            return 0;

        float scaled = static_cast<float>(baseAmount) * std::max<float>(0.01f, difficulty);
        if (scaled < 1.0f)
            return 1;

        return static_cast<uint32>(scaled);
    }

    bool IsRewardPublishable(XianmenRewardConfig const& reward) const
    {
        return reward.type == 2 || reward.itemId != 0;
    }

    bool GrantConfiguredReward(Player* player, XianmenRewardConfig const& reward, uint32 amount, std::string& error)
    {
        if (!player)
            return false;

        if (!amount)
            return true;

        if (reward.type == 2)
            return AddContribution(player, amount, error);

        if (reward.itemId)
        {
            if (!player->AddItem(reward.itemId, amount))
            {
                error = "背包空间不足或奖励物品不存在。";
                return false;
            }

            return true;
        }

        error = "该奖励类型尚未配置可发放物品。";
        return false;
    }

    uint32 GetTodayDate() const
    {
        if (QueryResult result = CharacterDatabase.Query("SELECT DATE_FORMAT(CURDATE(), '%Y%m%d')"))
            return (*result)[0].Get<uint32>();

        return 0;
    }

    XianmenPlayerData* GetMutablePlayerData(uint32 guid)
    {
        auto itr = _playerData.find(guid);
        return itr == _playerData.end() ? nullptr : &itr->second;
    }

    std::set<uint32> FilterPersonalActiveSkills(
        uint32 factionId,
        std::set<uint32> const& personalSkills,
        std::set<uint32> const& unlockedSkills,
        std::set<uint32> const* activeOverride = nullptr) const
    {
        std::set<uint32> result;
        if (!factionId)
            return result;

        std::set<uint32> const& activeSkills = activeOverride ? *activeOverride : GetActiveSkills(factionId);
        for (uint32 skillId : personalSkills)
        {
            if (result.size() >= _maxPersonalActiveSkills)
                break;

            if (activeSkills.count(skillId) && unlockedSkills.count(skillId))
                result.insert(skillId);
        }

        return result;
    }

    uint32 GetActiveSkillCount() const
    {
        uint32 count = 0;
        for (auto const& pair : _activeSkills)
            count += static_cast<uint32>(pair.second.size());
        return count;
    }

    bool _enabled = true;
    bool _announceOnLogin = true;
    bool _debugEffectiveSkillLog = false;
    bool _enableDailyLeaderSettle = true;
    uint32 _maxLevel = 100;
    uint32 _unlockLevelStep = 10;
    uint32 _maxActiveSkills = 10;
    uint32 _maxPersonalActiveSkills = 5;
    uint32 _maxDailyPublishes = 20;
    uint64 _upgradeRequirementBase = 100;
    uint64 _upgradeRequirementPerLevel = 10;
    uint32 _settleCheckIntervalMs = 60 * IN_MILLISECONDS;
    float _transferKeepLevelPct = 50.0f;

    std::unordered_map<uint32, XianmenFactionConfig> _factions;
    std::unordered_map<uint64, XianmenUpgradeRequirementConfig> _upgradeRequirements;
    std::unordered_map<uint32, XianmenSkillConfig> _skills;
    std::unordered_map<uint32, XianmenDailyTemplateConfig> _dailyTemplates;
    std::unordered_map<uint32, XianmenRewardConfig> _rewardConfigs;
    std::unordered_map<uint32, std::vector<uint32>> _skillsByFaction;
    std::unordered_map<uint32, uint32> _skillsBySpell;
    std::unordered_map<uint32, XianmenPlayerData> _playerData;
    std::unordered_map<uint32, int32> _appliedSkillSpellPenetration;
    std::unordered_map<uint32, std::set<uint32>> _activeSkills;
    std::unordered_map<uint32, uint32> _leaders;
    std::unordered_map<uint32, uint32> _leaderSettleDates;
};

#define sXianmenMgr XianmenMgr::instance()

void SendXianmenPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= XIANMEN_MAX_ADDON_PAYLOAD)
    {
        if (HermesBridge_SendAddonMessage(player, XIANMEN_ADDON_PREFIX, payload))
            return;

        std::string fullMessage = std::string(XIANMEN_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
        return;
    }

    size_t totalChunks = (payload.length() + XIANMEN_MAX_ADDON_PAYLOAD - 1) / XIANMEN_MAX_ADDON_PAYLOAD;
    for (size_t i = 0; i < totalChunks; ++i)
    {
        size_t start = i * XIANMEN_MAX_ADDON_PAYLOAD;
        size_t len = std::min(XIANMEN_MAX_ADDON_PAYLOAD, payload.length() - start);
        std::string chunk = payload.substr(start, len);

        std::ostringstream chunkMessage;
        chunkMessage << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << chunk;
        if (HermesBridge_SendAddonMessage(player, XIANMEN_ADDON_PREFIX, chunkMessage.str()))
            continue;

        std::string fullMessage = std::string(XIANMEN_ADDON_PREFIX) + '\t' + chunkMessage.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendXianmenFactions(Player* player)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "XM_FACTIONS:";

    bool first = true;
    for (XianmenFactionConfig const& faction : sXianmenMgr->GetFactions())
    {
        if (!first)
            payload << '~';

        payload << faction.id << '^'
                << EscapePayload(faction.name) << '^'
                << EscapePayload(faction.description);
        first = false;
    }

    SendXianmenPayload(player, payload.str());
}

void SendXianmenState(Player* player)
{
    if (!player)
        return;

    sXianmenMgr->LoadPlayerData(player);

    uint32 guid = player->GetGUID().GetCounter();
    XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(guid);

    uint32 factionId = data ? data->factionId : 0;
    uint32 level = data ? data->level : 0;
    uint64 dailyContribution = data ? data->dailyContribution : 0;
    uint64 historyContribution = data ? data->historyContribution : 0;
    uint32 maxLevel = factionId ? sXianmenMgr->GetFactionMaxLevel(factionId) : sXianmenMgr->GetMaxLevel();
    uint64 nextUpgradeRequirement = data ? sXianmenMgr->GetNextUpgradeRequirement(factionId, level, maxLevel) : 0;
    uint32 unlockSlots = data ? sXianmenMgr->GetUnlockedSlotCount(*data) : 0;
    uint32 leaderGuid = factionId ? sXianmenMgr->GetLeaderGuid(factionId) : 0;
    bool isLeader = factionId && sXianmenMgr->IsLeader(factionId, guid);
    XianmenFactionConfig const* faction = sXianmenMgr->GetFaction(factionId);

    std::set<uint32> unlocked;
    if (data)
        unlocked = data->unlockedSkills;

    std::set<uint32> active;
    if (factionId)
        active = sXianmenMgr->GetActiveSkills(factionId);

    std::set<uint32> personalActive;
    if (data)
        personalActive = data->personalActiveSkills;

    std::ostringstream payload;
    payload << "XM_STATE:"
            << factionId << '|'
            << EscapePayload(faction ? faction->name : "") << '|'
            << level << '|'
            << maxLevel << '|'
            << dailyContribution << '|'
            << historyContribution << '|'
            << nextUpgradeRequirement << '|'
            << unlockSlots << '|'
            << leaderGuid << '|'
            << (isLeader ? 1 : 0) << '|'
            << JoinUIntSet(unlocked) << '|'
            << JoinUIntSet(active) << '|'
            << JoinUIntSet(personalActive) << '|'
            << sXianmenMgr->GetMaxActiveSkills() << '|'
            << sXianmenMgr->GetMaxPersonalActiveSkills();

    SendXianmenPayload(player, payload.str());
    if (factionId)
    {
        LOG_DEBUG("server.loading", "[仙门定位-状态下发] 玩家={} GUID={} 门派={} 修为={} 门主={} 门主开放=[{}] 个人选择=[{}] 已解锁=[{}] 解锁槽={} 最大门主开放={} 最大个人生效={}",
            player->GetName(), guid, factionId, level, isLeader ? 1 : 0, JoinUIntSet(active), JoinUIntSet(personalActive), JoinUIntSet(unlocked),
            unlockSlots, sXianmenMgr->GetMaxActiveSkills(), sXianmenMgr->GetMaxPersonalActiveSkills());
    }
}

void SendXianmenSkills(Player* player)
{
    if (!player)
        return;

    sXianmenMgr->LoadPlayerData(player);

    uint32 guid = player->GetGUID().GetCounter();
    XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(guid);
    uint32 factionId = data ? data->factionId : 0;

    std::ostringstream payload;
    payload << "XM_SKILLS:";

    bool first = true;
    if (factionId)
    {
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetSkillsForFaction(factionId))
        {
            if (!first)
                payload << '~';

            bool unlocked = data && data->unlockedSkills.count(skill->id);
            bool active = sXianmenMgr->GetActiveSkills(factionId).count(skill->id) != 0;
            bool personalActive = data && data->personalActiveSkills.count(skill->id) != 0;

            payload << skill->id << '^'
                    << skill->factionId << '^'
                    << skill->order << '^'
                    << EscapePayload(skill->name) << '^'
                    << skill->type << '^'
                    << skill->spellId << '^'
                    << (unlocked ? 1 : 0) << '^'
                    << (active ? 1 : 0) << '^'
                    << (personalActive ? 1 : 0) << '^'
                    << skill->baseValue << '^'
                    << skill->growthPer10 << '^'
                    << EscapePayload(skill->triggerParam) << '^'
                    << EscapePayload(skill->description);
            LOG_DEBUG("server.loading", "[仙门定位-技能下发] 玩家={} GUID={} 技能={} 名称={} 门派={} 顺序={} 类型={} spellId={} 已解锁={} 门主开放={} 个人选择={}",
                player->GetName(), player->GetGUID().GetCounter(), skill->id, skill->name, skill->factionId, skill->order, skill->type, skill->spellId,
                unlocked ? 1 : 0, active ? 1 : 0, personalActive ? 1 : 0);
            first = false;
        }
    }

    SendXianmenPayload(player, payload.str());
}

void SendXianmenMembers(Player* player)
{
    if (!player)
        return;

    sXianmenMgr->LoadPlayerData(player);

    uint32 guid = player->GetGUID().GetCounter();
    XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(guid);
    uint32 factionId = data ? data->factionId : 0;

    std::ostringstream payload;
    payload << "XM_MEMBERS:";

    bool first = true;
    if (factionId)
    {
        uint32 leaderGuid = sXianmenMgr->GetLeaderGuid(factionId);
        QueryResult result = CharacterDatabase.Query(
            "SELECT p.`角色GUID`, COALESCE(c.`name`, ''), p.`修为等级`, p.`当日贡献`, p.`历史贡献`, "
            "p.`已解锁技能`, p.`个人生效技能`, COALESCE(c.`online`, 0) "
            "FROM `_仙门_玩家` p LEFT JOIN `characters` c ON c.`guid` = p.`角色GUID` "
            "WHERE p.`门派ID` = {} "
            "ORDER BY (p.`角色GUID` = {}) DESC, COALESCE(c.`online`, 0) DESC, p.`当日贡献` DESC, "
            "p.`历史贡献` DESC, p.`修为等级` DESC, c.`name` ASC",
            factionId, leaderGuid);

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 memberGuid = fields[0].Get<uint32>();
                std::string name = fields[1].Get<std::string>();
                uint32 level = fields[2].Get<uint32>();
                uint64 dailyContribution = fields[3].Get<uint64>();
                std::string historyContribution = fields[4].Get<std::string>();
                uint32 unlockedCount = static_cast<uint32>(ParseUIntSet(fields[5].Get<std::string>()).size());
                uint32 personalActiveCount = static_cast<uint32>(ParseUIntSet(fields[6].Get<std::string>()).size());
                XianmenPlayerData memberData;
                memberData.level = level;
                uint32 unlockSlots = sXianmenMgr->GetUnlockedSlotCount(memberData);
                bool online = fields[7].Get<uint32>() != 0;

                if (!first)
                    payload << '~';

                payload << memberGuid << '^'
                        << EscapePayload(name) << '^'
                        << level << '^'
                        << dailyContribution << '^'
                        << EscapePayload(historyContribution) << '^'
                        << unlockedCount << '^'
                        << unlockSlots << '^'
                        << personalActiveCount << '^'
                        << (memberGuid == leaderGuid ? 1 : 0) << '^'
                        << (online ? 1 : 0);
                first = false;
            } while (result->NextRow());
        }
    }

    SendXianmenPayload(player, payload.str());
}

void SendXianmenDailies(Player* player)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "XM_DAILIES:";

    bool first = true;
    for (XianmenDailyView const& daily : sXianmenMgr->GetTodayDailies(player))
    {
        if (!first)
            payload << '~';

        payload << daily.dailyId << '^'
                << daily.templateId << '^'
                << EscapePayload(daily.templateName) << '^'
                << daily.templateType << '^'
                << daily.targetCount << '^'
                << daily.progress << '^'
                << (daily.completed ? 1 : 0) << '^'
                << daily.rewardTier << '^'
                << daily.rewardType << '^'
                << EscapePayload(daily.rewardName) << '^'
                << daily.rewardAmount;
        first = false;
    }

    SendXianmenPayload(player, payload.str());
}

void SendXianmenResult(Player* player, std::string const& action, bool success, std::string const& message)
{
    std::ostringstream payload;
    payload << "XM_RESULT:" << EscapePayload(action) << '^' << (success ? 1 : 0) << '^' << EscapePayload(message);
    SendXianmenPayload(player, payload.str());
}

void SendXianmenAll(Player* player)
{
    SendXianmenFactions(player);
    SendXianmenState(player);
    SendXianmenSkills(player);
    SendXianmenMembers(player);
    SendXianmenDailies(player);
}

void SendXianmenOpenUI(Player* player)
{
    SendXianmenPayload(player, "XM_OPEN");
    SendXianmenAll(player);
}

uint32 GetXianmenSkillOrder(XianmenSkillConfig const& skill)
{
    return skill.order ? skill.order : (skill.id % 100);
}

float GetXianmenSkillValue(XianmenSkillConfig const& skill, uint32 level)
{
    return skill.baseValue + skill.growthPer10 * static_cast<float>(level / 10);
}

bool IsXianmenBossTarget(Unit* target)
{
    Creature* creature = target ? target->ToCreature() : nullptr;
    if (!creature)
        return false;

    CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
    return creature->IsDungeonBoss()
        || creature->isWorldBoss()
        || (creatureTemplate && creatureTemplate->rank == CREATURE_ELITE_WORLDBOSS);
}

uint64 GetXianmenTrialKillContribution(Creature* creature)
{
    if (!creature)
        return 0;

    switch (creature->GetEntry())
    {
        case 800001:
        case 800002:
        case 800003:
        case 800004:
        case 800005:
            return 10;
        case 800011:
        case 800012:
        case 800013:
        case 800014:
        case 800015:
        case 800016:
        case 800017:
        case 800018:
            return 5;
        default:
            return 0;
    }
}

uint256 ScaleXianmenPercent(uint256 const& value, float percent)
{
    if (value == 0 || percent <= 0.0f || std::isnan(percent))
        return 0;

    long double scaled = Acore::Number::ToLongDouble(value) * static_cast<long double>(percent) / 100.0L;
    return Acore::Number::ToUInt256Saturated(scaled);
}

uint256 AddXianmenPercent(uint256 const& value, float percent)
{
    if (value == 0 || percent <= 0.0f)
        return value;

    return AddUInt256Damage(value, ScaleXianmenPercent(value, percent));
}

uint256 ToXianmenUInt256Positive(double value)
{
    if (value <= 0.0 || std::isnan(value))
        return 0;

    if (std::isinf(value))
        return std::numeric_limits<uint256>::max();

    return Acore::Number::ToUInt256Saturated(static_cast<long double>(value));
}

uint256 GetXianmenAttackPower(Player* player)
{
    if (!player)
        return 0;

    return ToXianmenUInt256Positive(player->GetExtendedTotalAttackPowerValue(BASE_ATTACK));
}

uint256 GetXianmenSpellPower(Player* player)
{
    if (!player)
        return 0;

    int256 const spellPower = player->GetExtendedSpellPowerBonus256();
    return spellPower > 0 ? Acore::Number::ToUInt256Saturated(spellPower) : 0;
}

void ApplyXianmenHealthGain(Unit* unit, uint256 const& value)
{
    if (!unit || value == 0)
        return;

    uint256 currentHealth = unit->GetHealthForCombat256();
    uint256 maxHealth = unit->GetMaxHealthForCombat256();
    if (currentHealth >= maxHealth)
        return;

    unit->SetHealthForCombat256(currentHealth + std::min<uint256>(value, maxHealth - currentHealth));
}

void ApplyXianmenPowerGain(Player* player, Powers power, uint256 const& value)
{
    if (!player || value == 0)
        return;

    uint256 currentPower = player->GetPowerForCombat256(power);
    uint256 maxPower = player->GetMaxPowerForCombat256(power);
    if (currentPower >= maxPower)
        return;

    player->SetPowerForCombat256(power, currentPower + std::min<uint256>(value, maxPower - currentPower));
}

bool HasEffectiveXianmenSkill(Player* player, uint32 factionId, uint32 order)
{
    if (!player || !factionId || !order)
        return false;

    for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        if (skill && skill->factionId == factionId && GetXianmenSkillOrder(*skill) == order)
            return true;

    return false;
}

float GetEffectiveXianmenSkillValue(Player* player, uint32 factionId, uint32 order)
{
    if (!player || !factionId || !order)
        return 0.0f;

    float total = 0.0f;
    uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
    for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
    {
        if (!skill || skill->factionId != factionId || GetXianmenSkillOrder(*skill) != order)
            continue;

        total += GetXianmenSkillValue(*skill, level);
    }

    return total;
}

uint32 GetXianmenEffectiveSkillCount(Player* player, uint32 factionId)
{
    if (!player || !factionId)
        return 0;

    uint32 count = 0;
    for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        if (skill && skill->factionId == factionId)
            ++count;

    return count;
}

bool IsXianmenControlledUnit(Player* owner, Unit* unit)
{
    return owner && unit && unit != owner && unit->GetCharmerOrOwnerPlayerOrPlayerItself() == owner;
}

bool IsXianmenHighArmorTarget(Unit* target)
{
    return target && (IsXianmenBossTarget(target) || target->GetArmor() >= 5000);
}

void ApplyXianmenFrostSnare(Player* player, Unit* victim)
{
    if (!player || !victim || victim->isDead())
        return;

    if (sSpellMgr->GetSpellInfo(XIANMEN_FROST_SNARE_SPELL))
        player->CastSpell(victim, XIANMEN_FROST_SNARE_SPELL, true);
}

bool RollXianmenProc(Player* player, XianmenSkillConfig const& skill);

uint256 BuildXianmenLingdunShieldValue(Player* player, float percent)
{
    if (!player || percent <= 0.0f || std::isnan(percent))
        return 0;

    uint256 base = AddUInt256Damage(player->GetMaxHealthForCombat256(), player->GetMaxPowerForCombat256(POWER_MANA));
    base = AddUInt256Damage(base, GetXianmenSpellPower(player));
    base = AddUInt256Damage(base, GetXianmenAttackPower(player));

    return ScaleXianmenPercent(base, percent);
}

void ApplyXianmenLingdunShield(Player* player, uint256& damage, XianmenSkillConfig const& skill, float value)
{
    if (!player || damage == 0)
        return;

    uint32 const guid = player->GetGUID().GetCounter();
    auto shieldItr = XianmenLingdunShieldValues.find(guid);
    if (shieldItr == XianmenLingdunShieldValues.end() || shieldItr->second == 0)
    {
        if (!RollXianmenProc(player, skill))
            return;

        uint256 const shieldValue = BuildXianmenLingdunShieldValue(player, value);
        if (shieldValue == 0)
            return;

        XianmenLingdunShieldValues[guid] = shieldValue;
        shieldItr = XianmenLingdunShieldValues.find(guid);
        ApplyXianmenPowerGain(player, POWER_MANA, ScaleXianmenPercent(player->GetMaxPowerForCombat256(POWER_MANA), value * 0.2f));
        player->SendPlaySpellVisual(XIANMEN_LINGDUN_VISUAL_KIT);
        LOG_DEBUG("server.loading", "[仙门定位-灵盾生成] 玩家={} GUID={} 技能={} 名称={} value={} 护盾值={} 基底=生命+法力+法强+攻强",
            player->GetName(), guid, skill.id, skill.name, value, Acore::Number::ToDecimal65String(shieldValue));
    }

    uint256 const beforeShield = shieldItr->second;
    uint256 const absorb = std::min<uint256>(damage, shieldItr->second);
    damage -= absorb;
    shieldItr->second -= absorb;

    LOG_DEBUG("server.loading", "[仙门定位-灵盾吸收] 玩家={} GUID={} 技能={} 名称={} 入伤={} 吸收={} 护盾前={} 护盾余={}",
        player->GetName(), guid, skill.id, skill.name,
        Acore::Number::ToDecimal65String(AddUInt256Damage(damage, absorb)),
        Acore::Number::ToDecimal65String(absorb),
        Acore::Number::ToDecimal65String(beforeShield),
        Acore::Number::ToDecimal65String(shieldItr->second));

    if (shieldItr->second == 0)
    {
        XianmenLingdunShieldValues.erase(shieldItr);
        LOG_DEBUG("server.loading", "[仙门定位-灵盾破碎] 玩家={} GUID={} 技能={} 名称={}",
            player->GetName(), guid, skill.id, skill.name);
    }
}

void ApplyXianmenReactiveMitigation(Player* player, uint256& damage)
{
    if (!player || damage == 0)
        return;

    uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
    for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
    {
        if (!skill)
            continue;

        uint32 const order = GetXianmenSkillOrder(*skill);
        float const value = GetXianmenSkillValue(*skill, level);

        if (skill->factionId == XIANMEN_FULU_FACTION_ID && order == 9)
        {
            ApplyXianmenLingdunShield(player, damage, *skill, value);
        }
        else if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 8 && RollXianmenProc(player, *skill))
        {
            damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, value));
        }
    }
}

bool TryTriggerXianmenGoldenBody(Player* player, uint256& damage)
{
    if (!player || damage == 0)
        return false;

    float const value = GetEffectiveXianmenSkillValue(player, XIANMEN_BODY_FACTION_ID, 9);
    if (value <= 0.0f)
        return false;

    uint256 const currentHealth = player->GetHealthForCombat256();
    uint256 const maxHealth = player->GetMaxHealthForCombat256();
    if (currentHealth == 0 || maxHealth == 0)
        return false;

    uint256 const lowHealthThreshold = ScaleXianmenPercent(maxHealth, 35.0f);
    uint256 const meaningfulDamage = ScaleXianmenPercent(maxHealth, 10.0f);
    if (damage < currentHealth && (currentHealth > lowHealthThreshold || damage < meaningfulDamage))
        return false;

    uint32 const guid = player->GetGUID().GetCounter();
    uint32 const now = GetNow();
    auto readyItr = XianmenGoldenBodyReadyTimes.find(guid);
    if (readyItr != XianmenGoldenBodyReadyTimes.end() && readyItr->second > now)
        return false;

    XianmenGoldenBodyReadyTimes[guid] = now + XIANMEN_GOLDEN_BODY_COOLDOWN_SECONDS;

    uint256 const healthFloor = std::max<uint256>(uint256(1), ScaleXianmenPercent(maxHealth, value * 0.4f));
    uint256 const allowedDamage = currentHealth > healthFloor ? currentHealth - healthFloor : uint256(0);
    damage = std::min<uint256>(damage, allowedDamage);
    ApplyXianmenHealthGain(player, ScaleXianmenPercent(maxHealth, value));

    ChatHandler(player->GetSession()).SendSysMessage("|cff66ffcc[仙门系统]|r 不灭金身触发，抵住了致命伤害。");
    return true;
}

// 按技能配置的触发参数掷骰（原实现恒 true，"30%"/"PPM 4"等几率配置全部失效=100% 触发）。
// 触发参数格式（见 `_仙门系统_配置` 表数据）：
//   "攻击 30%" / "命中 20%" / "受击 25%" → 按百分比掷骰
//   "攻击 PPM 4" / "命中 PPM 3"          → 每分钟期望 N 次，按同玩家同技能的真实事件间隔计算
//   "常驻" / "攻击触发" / "命中触发" 等   → 必定触发（保持原行为）
bool RollXianmenProc(Player* player, XianmenSkillConfig const& skill)
{
    std::string const& param = skill.triggerParam;
    if (param.empty())
        return true;

    // PPM N
    size_t ppmPos = param.find("PPM");
    if (ppmPos != std::string::npos)
    {
        size_t numStart = param.find_first_of("0123456789", ppmPos);
        if (numStart != std::string::npos)
        {
            size_t numEnd = param.find_first_not_of("0123456789", numStart);
            if (Optional<uint32> ppm = Acore::StringTo<uint32>(param.substr(numStart, numEnd == std::string::npos ? std::string::npos : numEnd - numStart)))
            {
                if (*ppm > 0)
                {
                    uint32 elapsedMs = 2000;
                    if (player)
                    {
                        uint64 const key = MakeXianmenPlayerSpellKey(player->GetGUID().GetCounter(), skill.id);
                        uint32 const nowMs = static_cast<uint32>(GameTime::GetGameTimeMS().count());
                        auto const lastItr = XianmenProcPpmLastEventMs.find(key);
                        if (lastItr != XianmenProcPpmLastEventMs.end())
                            elapsedMs = nowMs >= lastItr->second ? nowMs - lastItr->second : 0;

                        XianmenProcPpmLastEventMs[key] = nowMs;
                    }

                    elapsedMs = std::min<uint32>(elapsedMs, 10000);
                    float const chance = std::min(100.0f, float(*ppm) * static_cast<float>(elapsedMs) / 60000.0f * 100.0f);
                    return chance > 0.0f && roll_chance_f(chance);
                }
            }
        }
        return true;
    }

    // N%
    size_t pctPos = param.find('%');
    if (pctPos != std::string::npos)
    {
        size_t numStart = pctPos;
        while (numStart > 0 && param[numStart - 1] >= '0' && param[numStart - 1] <= '9')
            --numStart;

        if (numStart < pctPos)
        {
            if (Optional<uint32> chance = Acore::StringTo<uint32>(param.substr(numStart, pctPos - numStart)))
                return roll_chance_f(float(std::min<uint32>(*chance, 100)));
        }
        return true;
    }

    return true;
}

void DealXianmenScriptDamage(Player* player, Unit* victim, uint256 const& damage, SpellSchoolMask schoolMask, SpellInfo const* spellInfo)
{
    if (!player || !victim || !spellInfo || damage == 0 || victim->isDead())
        return;

    bool const oldGuard = XianmenProcDamageGuard;
    XianmenProcDamageGuard = true;

    SpellNonMeleeDamage damageInfo(player, victim, spellInfo, schoolMask);
    damageInfo.damage = damage;
    Unit::DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    if (player->ShouldSendCustomProcClientFeedback(victim, spellInfo, "xianmen-script"))
        player->SendSpellNonMeleeDamageLog(&damageInfo);
    CleanDamage cleanDamage(damageInfo.cleanDamage, damageInfo.absorb, BASE_ATTACK, MELEE_HIT_NORMAL);
#if defined(MODULE_MAGIC_HIT_SYSTEM)
    uint32 const magicHitGuardPlayerGuid = player->GetGUID().GetCounter();
    sMagicHitSystem->BeginInternalMagicHitCast(magicHitGuardPlayerGuid);
#endif
    Unit::DealDamage(player, victim, damageInfo.damage, &cleanDamage, SPELL_DIRECT_DAMAGE, schoolMask, spellInfo, true);
#if defined(MODULE_MAGIC_HIT_SYSTEM)
    sMagicHitSystem->EndInternalMagicHitCast(magicHitGuardPlayerGuid);
#endif

    XianmenProcDamageGuard = oldGuard;
}

bool IsXianmenExcludedTarget(Unit* candidate, std::vector<ObjectGuid> const& excluded)
{
    if (!candidate)
        return true;

    ObjectGuid const guid = candidate->GetGUID();
    return std::find(excluded.begin(), excluded.end(), guid) != excluded.end();
}

std::vector<Unit*> SelectNearbyXianmenTargets(Player* player, WorldObject* center, std::vector<ObjectGuid> const& excluded, float radius, uint32 maxTargets)
{
    std::vector<Unit*> result;
    if (!player || !center || radius <= 0.0f || !maxTargets)
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
    {
        if (!candidate || candidate->isDead() || IsXianmenExcludedTarget(candidate, excluded))
            continue;

        result.push_back(candidate);
        if (result.size() >= maxTargets)
            break;
    }

    return result;
}

float GetXianmenAreaRadius(Player* player, float baseRadius)
{
    if (!player || baseRadius <= 0.0f)
        return baseRadius;

    float const bonusPct = GetEffectiveXianmenSkillValue(player, XIANMEN_FULU_FACTION_ID, 4);
    if (bonusPct <= 0.0f || std::isnan(bonusPct))
        return baseRadius;

    return std::min<float>(45.0f, baseRadius * (1.0f + std::min<float>(bonusPct, 200.0f) / 100.0f));
}

bool TriggerXianmenMagicHitResonance(Player* player, Unit* victim, SpellInfo const* sourceSpellInfo, uint256 const& baseDamage)
{
#if defined(MODULE_MAGIC_HIT_SYSTEM)
    if (!player || !victim)
        return false;

    uint32 const sourceSpellId = sourceSpellInfo ? sourceSpellInfo->Id : 0;
    if (!sourceSpellInfo)
        return false;

    if (baseDamage == 0)
        return false;

    MagicHitSystem* magicHitSystem = sMagicHitSystem;
    if (!magicHitSystem || !magicHitSystem->IsEnabled())
        return false;

    if (!magicHitSystem->HasSpellConfig(sourceSpellId))
        return false;

    if (!ConsumeXianmenMagicHitResonanceCooldown(player, sourceSpellId))
        return false;

    if (!HasUsableXianmenMagicHitData(magicHitSystem, player, sourceSpellId))
        return false;

    magicHitSystem->EnqueueMagicHitProcessing(player, victim, sourceSpellId, baseDamage, false, XIANMEN_MAGIC_HIT_RESONANCE_SPELL);
    return true;
#else
    (void)player;
    (void)victim;
    (void)sourceSpellInfo;
    (void)baseDamage;
    return true;
#endif
}

void TriggerXianmenOffensiveProcs(Player* player, Unit* victim, SpellInfo const* sourceSpellInfo, bool spellDamage, uint256 sourceDamage)
{
    if (!player || !victim || victim->isDead())
        return;

    for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
    {
        if (!skill || (skill->type != 2 && skill->type != 3))
            continue;

        uint32 const order = GetXianmenSkillOrder(*skill);

        if (skill->factionId == XIANMEN_FULU_FACTION_ID && order == 9)
            continue;

        if (!RollXianmenProc(player, *skill))
            continue;

        SpellInfo const* spellInfo = skill->spellId ? sSpellMgr->GetSpellInfo(skill->spellId) : sourceSpellInfo;
        if (!spellInfo)
            spellInfo = sourceSpellInfo;
        if (!spellInfo)
            continue;

        float const value = GetXianmenSkillValue(*skill, sXianmenMgr->GetPlayerXianmenLevel(player));

        uint256 damage = 0;
        SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL;

        if (skill->factionId == 1)
        {
            if (order == 5)
                damage = ScaleXianmenPercent(GetXianmenAttackPower(player), value);
            else if (order == 6)
                damage = ScaleXianmenPercent(AddUInt256Damage(GetXianmenAttackPower(player), player->GetCuttingDamageBonus()), value);
            else if (order == 7)
                damage = ScaleXianmenPercent(GetXianmenAttackPower(player), value * 6.0f);
        }
        else if (skill->factionId == XIANMEN_FULU_FACTION_ID && spellDamage)
        {
            schoolMask = order == 7 ? SPELL_SCHOOL_MASK_NATURE : order == 8 ? SPELL_SCHOOL_MASK_FROST : SPELL_SCHOOL_MASK_FIRE;
            if (order == 5)
                damage = ScaleXianmenPercent(GetXianmenSpellPower(player), value);
            else if (order == 6)
            {
                damage = ScaleXianmenPercent(GetXianmenSpellPower(player), value);
                if (!TriggerXianmenMagicHitResonance(player, victim, sourceSpellInfo, sourceDamage != 0 ? sourceDamage : damage))
                    continue;
            }
            else if (order == 7)
                damage = ScaleXianmenPercent(GetXianmenSpellPower(player), value * 3.0f);
            else if (order == 8)
                damage = ScaleXianmenPercent(GetXianmenSpellPower(player), value);
        }
        else if (skill->factionId == XIANMEN_BODY_FACTION_ID)
        {
            if (order == 6)
                damage = ScaleXianmenPercent(player->GetMaxHealthForCombat256(), value);
        }
        else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID)
        {
            if (order == 8)
                ApplyXianmenHealthGain(player, ScaleXianmenPercent(player->GetMaxHealthForCombat256(), value));
            else if (order == 9)
                damage = ScaleXianmenPercent(AddUInt256Damage(GetXianmenAttackPower(player), GetXianmenSpellPower(player)), value);
        }

        if (skill->factionId == XIANMEN_FULU_FACTION_ID)
        {
            uint256 const spellPower = GetXianmenSpellPower(player);
            LOG_DEBUG("server.loading", "[仙门定位-符箓战斗触发] 玩家={} GUID={} 技能={} 名称={} 顺序={} 类型={} spellDamage={} sourceSpell={} skillSpell={} value={} 法强={} 计算伤害={} 目标={} 目标GUID={}",
                player->GetName(), player->GetGUID().GetCounter(), skill->id, skill->name, order, skill->type, spellDamage ? 1 : 0,
                sourceSpellInfo ? sourceSpellInfo->Id : 0, spellInfo ? spellInfo->Id : 0, value,
                Acore::Number::ToDecimal65String(spellPower), Acore::Number::ToDecimal65String(damage),
                victim->GetName(), victim->GetGUID().ToString());
        }

        DealXianmenScriptDamage(player, victim, damage, schoolMask, spellInfo);

        if (damage != 0 && skill->factionId == XIANMEN_FULU_FACTION_ID && order == 5)
        {
            std::vector<ObjectGuid> excluded = { victim->GetGUID() };
            for (Unit* nearbyTarget : SelectNearbyXianmenTargets(player, victim, excluded, GetXianmenAreaRadius(player, 8.0f), 4))
                DealXianmenScriptDamage(player, nearbyTarget, ScaleXianmenPercent(damage, 50.0f), schoolMask, spellInfo);
        }
        else if (damage != 0 && skill->factionId == XIANMEN_FULU_FACTION_ID && order == 7)
        {
            std::vector<ObjectGuid> excluded = { victim->GetGUID() };
            WorldObject* chainCenter = victim;
            uint256 chainDamage = ScaleXianmenPercent(damage, 80.0f);

            for (uint8 bounce = 0; bounce < 4 && chainDamage != 0; ++bounce)
            {
                std::vector<Unit*> targets = SelectNearbyXianmenTargets(player, chainCenter, excluded, GetXianmenAreaRadius(player, 15.0f), 1);
                if (targets.empty())
                    break;

                Unit* chainTarget = targets.front();
                DealXianmenScriptDamage(player, chainTarget, chainDamage, schoolMask, spellInfo);
                excluded.push_back(chainTarget->GetGUID());
                chainCenter = chainTarget;
                chainDamage = ScaleXianmenPercent(chainDamage, 80.0f);
            }
        }

        if (skill->factionId == XIANMEN_FULU_FACTION_ID && order == 8)
            ApplyXianmenFrostSnare(player, victim);
    }
}

void TriggerXianmenDefensiveProcs(Player* player, Unit* attacker, uint256 const& incomingDamage, SpellInfo const* sourceSpellInfo)
{
    if (!player || !attacker || attacker->isDead())
        return;

    for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
    {
        if (!skill || skill->type != 2)
            continue;

        uint32 const order = GetXianmenSkillOrder(*skill);
        if (skill->factionId == XIANMEN_FULU_FACTION_ID && order == 9)
            continue;

        if (!RollXianmenProc(player, *skill))
            continue;

        SpellInfo const* spellInfo = skill->spellId ? sSpellMgr->GetSpellInfo(skill->spellId) : sourceSpellInfo;
        if (!spellInfo)
            spellInfo = sourceSpellInfo;
        if (!spellInfo)
            continue;

        float const value = GetXianmenSkillValue(*skill, sXianmenMgr->GetPlayerXianmenLevel(player));
        uint256 damage = 0;
        SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL;

        if (skill->factionId == 1 && order == 8)
            damage = ScaleXianmenPercent(GetXianmenAttackPower(player), value);
        else if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 5)
            damage = ScaleXianmenPercent(incomingDamage, value);
        else if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 7)
            damage = ScaleXianmenPercent(player->GetMaxHealthForCombat256(), value);

        if (skill->factionId == XIANMEN_FULU_FACTION_ID)
        {
            LOG_DEBUG("server.loading", "[仙门定位-符箓受击触发] 玩家={} GUID={} 技能={} 名称={} 顺序={} 类型={} sourceSpell={} skillSpell={} value={} 入伤={} 计算反击伤害={} 攻击者={} 攻击者GUID={}",
                player->GetName(), player->GetGUID().GetCounter(), skill->id, skill->name, order, skill->type,
                sourceSpellInfo ? sourceSpellInfo->Id : 0, spellInfo ? spellInfo->Id : 0, value,
                Acore::Number::ToDecimal65String(incomingDamage), Acore::Number::ToDecimal65String(damage),
                attacker->GetName(), attacker->GetGUID().ToString());
        }

        DealXianmenScriptDamage(player, attacker, damage, schoolMask, spellInfo);
        if (damage != 0 && skill->factionId == XIANMEN_BODY_FACTION_ID && order == 7)
        {
            std::vector<ObjectGuid> excluded = { attacker->GetGUID() };
            for (Unit* nearbyTarget : SelectNearbyXianmenTargets(player, attacker, excluded, GetXianmenAreaRadius(player, 8.0f), 4))
                DealXianmenScriptDamage(player, nearbyTarget, ScaleXianmenPercent(damage, 50.0f), schoolMask, spellInfo);
        }
    }
}

class XianmenWorldScript : public WorldScript
{
public:
    XianmenWorldScript() : WorldScript("XianmenWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sXianmenMgr->LoadConfig();

        if (reload && sXianmenMgr->IsEnabled())
            sXianmenMgr->LoadAll();
    }

    void OnStartup() override
    {
        if (!sXianmenMgr->IsEnabled())
            return;

        _loaded = false;
        _loadTime = 0;
        _displayedLog = false;
        _settleTimer = sXianmenMgr->GetSettleCheckIntervalMs();
    }

    void OnUpdate(uint32 diff) override
    {
        if (!sXianmenMgr->IsEnabled())
            return;

        if (!_loaded)
        {
            _loadTime += diff;
            if (_loadTime < 3000)
                return;

            if (!_displayedLog)
            {
                LOG_INFO("server.loading", "→仙门系统√");
                _displayedLog = true;
            }

            _loaded = true;
            sXianmenMgr->LoadAll();
            sXianmenMgr->SettleDailyLeaders();
            return;
        }

        if (_settleTimer > diff)
        {
            _settleTimer -= diff;
            return;
        }

        _settleTimer = sXianmenMgr->GetSettleCheckIntervalMs();
        sXianmenMgr->SettleDailyLeaders();
    }

private:
    bool _loaded = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
    uint32 _settleTimer = 60 * IN_MILLISECONDS;
};

bool AddXianmenNativeDailyMenuItem(QuestMenu& menu, uint32 questId, uint8 icon)
{
    if (menu.HasItem(questId))
        return true;

    if (menu.GetMenuItemCount() >= XIANMEN_NATIVE_DAILY_MENU_LIMIT)
        return false;

    menu.AddMenuItem(questId, icon);
    return true;
}

class XianmenNativeDailyCreatureScript : public CreatureScript
{
public:
    XianmenNativeDailyCreatureScript() : CreatureScript("npc_xianmen_native_daily") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature || creature->GetEntry() != XIANMEN_NATIVE_DAILY_NPC)
            return false;

        player->PlayerTalkClass->ClearMenus();
        QuestMenu& menu = player->PlayerTalkClass->GetQuestMenu();
        bool reachedLimit = false;

        for (uint32 questId = XIANMEN_NATIVE_DAILY_QUEST_FIRST; questId <= XIANMEN_NATIVE_DAILY_QUEST_LAST; ++questId)
        {
            QuestStatus const status = player->GetQuestStatus(questId);
            if (status != QUEST_STATUS_COMPLETE && status != QUEST_STATUS_INCOMPLETE)
                continue;

            if (!AddXianmenNativeDailyMenuItem(menu, questId, 4))
            {
                reachedLimit = true;
                break;
            }
        }

        for (uint32 questId = XIANMEN_NATIVE_DAILY_QUEST_FIRST; questId <= XIANMEN_NATIVE_DAILY_QUEST_LAST; ++questId)
        {
            if (menu.GetMenuItemCount() >= XIANMEN_NATIVE_DAILY_MENU_LIMIT)
                break;

            if (!sPoolMgr->IsSpawnedObject<Quest>(questId))
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest || !player->CanTakeQuest(quest, false))
                continue;

            if (quest->IsAutoComplete() && (!quest->IsRepeatable() || quest->IsDaily() || quest->IsWeekly() || quest->IsMonthly()))
                reachedLimit = !AddXianmenNativeDailyMenuItem(menu, questId, 0) || reachedLimit;
            else if (quest->IsAutoComplete())
                reachedLimit = !AddXianmenNativeDailyMenuItem(menu, questId, 4) || reachedLimit;
            else if (player->GetQuestStatus(questId) == QUEST_STATUS_NONE)
                reachedLimit = !AddXianmenNativeDailyMenuItem(menu, questId, 2) || reachedLimit;
        }

        if (menu.Empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙门日常]|r 当前没有可领取或可提交的宗门日常。");
            player->PlayerTalkClass->SendCloseGossip();
            return true;
        }

        if (reachedLimit)
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙门日常]|r 当前最多显示 20 个宗门日常，请先提交或放弃已有日常后再领取新的。");

        player->SendPreparedQuest(creature->GetGUID());
        return true;
    }
};

class XianmenPlayerScript : public PlayerScript
{
public:
    XianmenPlayerScript() : PlayerScript("XianmenPlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER,
        PLAYERHOOK_PASSED_QUEST_KILLED_MONSTER_CREDIT,
        PLAYERHOOK_ON_BEFORE_QUEST_COMPLETE,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_AFTER_UPDATE_MAX_POWER,
        PLAYERHOOK_ON_AFTER_UPDATE_MAX_HEALTH,
        PLAYERHOOK_ON_AFTER_UPDATE_STAT,
        PLAYERHOOK_ON_AFTER_UPDATE_ATTACK_POWER_AND_DAMAGE,
        PLAYERHOOK_ON_AFTER_UPDATE_ARMOR,
        PLAYERHOOK_ON_AFTER_UPDATE_CRIT_PERCENTAGE,
        PLAYERHOOK_ON_AFTER_UPDATE_SPELL_CRIT_CHANCE,
        PLAYERHOOK_ON_AFTER_UPDATE_HIT_CHANCES,
        PLAYERHOOK_ON_AFTER_UPDATE_SPELL_DAMAGE_AND_HEALING,
        PLAYERHOOK_ON_AFTER_UPDATE_RATING
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        sXianmenMgr->LoadPlayerData(player);
        sXianmenMgr->RefreshPassiveAuras(player);

        if (!sXianmenMgr->ShouldAnnounceOnLogin())
            return;

        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (!data || !data->factionId)
            return;

        XianmenFactionConfig const* faction = sXianmenMgr->GetFaction(data->factionId);
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff66ffcc[仙门系统]|r 当前门派: |cffffd700{}|r，修为: |cffffd700{}|r，当日贡献: |cffffd700{}|r",
            faction ? faction->name : "未知", data->level, data->dailyContribution);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        uint32 const guid = player->GetGUID().GetCounter();
        _regenTimers.erase(player->GetGUID().GetCounter());
        XianmenGoldenBodyReadyTimes.erase(guid);
        XianmenLingdunShieldValues.erase(guid);
        ClearXianmenMagicHitResonanceState(guid);
        sXianmenMgr->ClearSkillDirectBonuses(player);
        sXianmenMgr->UnloadPlayerData(guid);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        XianmenGoldenBodyReadyTimes.erase(guid.GetCounter());
        XianmenLingdunShieldValues.erase(guid.GetCounter());
        ClearXianmenMagicHitResonanceState(guid.GetCounter());
        sXianmenMgr->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !sXianmenMgr->IsEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != XIANMEN_ADDON_PREFIX)
            return;

        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "XIANMEN"))
            return;

        std::string command = msg.substr(tabPos + 1);

        if (command == "REQ_ALL")
        {
            SendXianmenAll(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            SendXianmenState(player);
            return;
        }

        if (command == "REQ_SKILLS")
        {
            SendXianmenSkills(player);
            return;
        }

        if (command == "REQ_MEMBERS")
        {
            SendXianmenMembers(player);
            return;
        }

        if (command == "REQ_DAILIES")
        {
            SendXianmenDailies(player);
            return;
        }

        if (command == "OPEN")
        {
            SendXianmenOpenUI(player);
            return;
        }

        if (command == "LEAVE")
        {
            std::string error;
            bool ok = sXianmenMgr->LeaveFaction(player, error);
            SendXianmenResult(player, "LEAVE", ok, ok ? "已退出当前仙门" : error);
            SendXianmenAll(player);
            return;
        }

        if (command == "REFRESH")
        {
            sXianmenMgr->LoadPlayerData(player);
            sXianmenMgr->RefreshPassiveAuras(player);
            SendXianmenResult(player, "REFRESH", true, "仙门被动已刷新");
            SendXianmenAll(player);
            return;
        }

        uint32 joinFaction = 0;
        if (sscanf(command.c_str(), "JOIN:%u", &joinFaction) == 1)
        {
            std::string error;
            bool ok = sXianmenMgr->JoinFaction(player, joinFaction, error);
            SendXianmenResult(player, "JOIN", ok, ok ? "已加入仙门" : error);
            SendXianmenAll(player);
            return;
        }

        uint32 upgradeCount = 0;
        if (command == "UPGRADE" || sscanf(command.c_str(), "UPGRADE:%u", &upgradeCount) == 1)
        {
            std::string error;
            uint32 upgraded = 0;
            bool ok = sXianmenMgr->UpgradePlayerLevel(player, upgradeCount ? upgradeCount : 1, upgraded, error);

            std::ostringstream message;
            if (ok)
                message << "修为已提升 " << upgraded << " 级";

            SendXianmenResult(player, "UPGRADE", ok, ok ? message.str() : error);
            SendXianmenAll(player);
            return;
        }

        uint32 unlockSkill = 0;
        if (sscanf(command.c_str(), "UNLOCK:%u", &unlockSkill) == 1)
        {
            std::string error;
            bool ok = sXianmenMgr->UnlockSkill(player, unlockSkill, error);
            SendXianmenResult(player, "UNLOCK", ok, ok ? "技能已解锁" : error);
            SendXianmenAll(player);
            return;
        }

        uint32 completeDaily = 0;
        if (sscanf(command.c_str(), "COMPLETE_DAILY:%u", &completeDaily) == 1)
        {
            std::string error;
            bool ok = sXianmenMgr->CompleteDaily(player, completeDaily, error);
            SendXianmenResult(player, "COMPLETE_DAILY", ok, ok ? "日常已完成，奖励已发放" : error);
            SendXianmenAll(player);
            return;
        }

        std::string const activePrefix = "SET_ACTIVE:";
        if (command.rfind(activePrefix, 0) == 0)
        {
            std::string args = command.substr(activePrefix.length());
            std::replace(args.begin(), args.end(), ',', ' ');
            std::vector<uint32> skills = ParseUIntList(args);

            std::string error;
            bool ok = sXianmenMgr->SetActiveSkills(player, skills, error);
            std::set<uint32> uniqueSkills(skills.begin(), skills.end());
            std::ostringstream message;
            if (ok)
                message << "门派生效技能已更新：" << uniqueSkills.size() << " 个";
            SendXianmenResult(player, "SET_ACTIVE", ok, ok ? message.str() : error);
            SendXianmenAll(player);
            return;
        }

        std::string const personalActivePrefix = "SET_PERSONAL_ACTIVE:";
        if (command.rfind(personalActivePrefix, 0) == 0)
        {
            std::string args = command.substr(personalActivePrefix.length());
            std::replace(args.begin(), args.end(), ',', ' ');
            std::vector<uint32> skills = ParseUIntList(args.c_str());

            std::string error;
            bool ok = sXianmenMgr->SetPersonalActiveSkills(player, skills, error);
            std::set<uint32> uniqueSkills(skills.begin(), skills.end());
            std::ostringstream message;
            if (ok)
                message << "个人生效技能已更新：" << uniqueSkills.size() << " 个";
            SendXianmenResult(player, "SET_PERSONAL_ACTIVE", ok, ok ? message.str() : error);
            SendXianmenAll(player);
            return;
        }
    }

    bool OnPlayerPassedQuestKilledMonsterCredit(Player* player, Quest const* qinfo, uint32 /*entry*/, uint32 /*real_entry*/, ObjectGuid /*guid*/) override
    {
        if (!player || !qinfo || !sXianmenMgr->IsEnabled())
            return true;

        if (!IsXianmenNativeDailyQuest(qinfo->GetQuestId()))
            return true;

        if (!player->GetGroup())
            return true;

        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙门日常]|r 组队状态下击杀不计入宗门日常，请离开队伍后单人完成。");
        return false;
    }

    bool OnPlayerBeforeQuestComplete(Player* player, uint32 questId) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return true;

        if (!IsXianmenNativeDailyQuest(questId))
            return true;

        if (!player->GetGroup())
            return true;

        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[仙门日常]|r 组队状态下无法完成宗门日常，请离开队伍后单人完成。");
        return false;
    }

    void OnPlayerCreatureKill(Player* player, Creature* killed) override
    {
        RewardTrialKillContribution(player, killed);
        RewardSoulAffinity(player, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* player, Creature* killed) override
    {
        RewardTrialKillContribution(player, killed);
        RewardSoulAffinity(player, killed);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        uint32 const guid = player->GetGUID().GetCounter();

        if (!player->IsInCombat())
            return;

        uint32& timer = _regenTimers[guid];
        timer += diff;
        if (timer < 5 * IN_MILLISECONDS)
            return;

        timer = 0;

        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 2)
                ApplyXianmenHealthGain(player, ScaleXianmenPercent(player->GetMaxHealthForCombat256(), GetXianmenSkillValue(*skill, level) * 0.05f));
        }
    }

    void OnPlayerAfterUpdateStat(Player* player, Stats stat, float& value) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float bonusPct = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        uint32 const soulSkillCount = GetXianmenEffectiveSkillCount(player, XIANMEN_WUHUN_FACTION_ID);
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            float const amount = GetXianmenSkillValue(*skill, level);
            if (skill->factionId == 1 && order == 10)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 4)
                bonusPct += amount + static_cast<float>(soulSkillCount);
            else if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 1 && stat == STAT_STAMINA)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 1)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 2)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 6)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 7 && stat == STAT_STAMINA)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                bonusPct += amount;
        }

        if (bonusPct > 0.0f)
            value += value * bonusPct / 100.0f;
    }

    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float bonusPct = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            float const amount = GetXianmenSkillValue(*skill, level);
            if (skill->factionId == XIANMEN_BODY_FACTION_ID && (order == 2 || order == 10))
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 7)
                bonusPct += amount;
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                bonusPct += amount;
        }

        if (bonusPct > 0.0f)
            value += value * bonusPct / 100.0f;
    }

    void OnPlayerAfterUpdateMaxPower(Player* player, Powers& power, float& value) override
    {
        if (!player || !sXianmenMgr->IsEnabled() || power != POWER_MANA)
            return;

        float const beforeValue = value;

        float bonusPct = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        std::vector<XianmenSkillConfig const*> effectiveSkills = sXianmenMgr->GetEffectiveSkills(player);
        for (XianmenSkillConfig const* skill : effectiveSkills)
            if (skill && skill->factionId == XIANMEN_FULU_FACTION_ID && GetXianmenSkillOrder(*skill) == 3)
                bonusPct += GetXianmenSkillValue(*skill, level);

        if (bonusPct > 0.0f)
            value += value * bonusPct / 100.0f;

        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (sXianmenMgr->IsEffectiveSkillDebugLogEnabled() && data && data->factionId == XIANMEN_FULU_FACTION_ID)
        {
            std::string const effectiveList = JoinXianmenSkillIds(effectiveSkills);
            std::ostringstream signature;
            signature << data->factionId << '|' << effectiveList << '|' << beforeValue << '|' << bonusPct << '|' << value;

            uint32 const guid = player->GetGUID().GetCounter();
            uint64 const cacheKey = (static_cast<uint64>(guid) << 32) | 0xFFFFFF01u;
            std::string const signatureText = signature.str();
            auto cacheItr = XianmenRatingDebugSignatures.find(cacheKey);
            if (cacheItr == XianmenRatingDebugSignatures.end() || cacheItr->second != signatureText)
            {
                XianmenRatingDebugSignatures[cacheKey] = signatureText;
                LOG_DEBUG("server.loading", "[仙门定位-符箓法力计算] 玩家={} GUID={} 修为={} 实际生效=[{}] 法力前={} bonusPct={} 法力后={}",
                    player->GetName(), guid, level, effectiveList, beforeValue, bonusPct, value);
            }
        }
    }

    void OnPlayerAfterUpdateAttackPowerAndDamage(Player* player, float& /*level*/, float& /*base_attPower*/, float& attPowerMod, float& attPowerMultiplier, bool ranged) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float bonusPct = 0.0f;
        uint32 const xianmenLevel = sXianmenMgr->GetPlayerXianmenLevel(player);
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == 1 && order == 1)
                bonusPct += GetXianmenSkillValue(*skill, xianmenLevel);
            else if (!ranged && skill->factionId == XIANMEN_BODY_FACTION_ID && order == 4)
            {
                long double apBonus = Acore::Number::ToLongDouble(ScaleXianmenPercent(player->GetMaxHealthForCombat256(), GetXianmenSkillValue(*skill, xianmenLevel)));
                if (apBonus > 0.0L && std::isfinite(static_cast<double>(apBonus)))
                    attPowerMod += static_cast<float>(std::min<long double>(apBonus, static_cast<long double>(std::numeric_limits<float>::max())));
            }
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 1)
                bonusPct += GetXianmenSkillValue(*skill, xianmenLevel);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 6)
                bonusPct += GetXianmenSkillValue(*skill, xianmenLevel);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                bonusPct += GetXianmenSkillValue(*skill, xianmenLevel);
        }

        if (bonusPct > 0.0f)
            attPowerMultiplier += bonusPct / 100.0f;
    }

    void OnPlayerAfterUpdateArmor(Player* player, float& value) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float bonusPct = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 1)
                bonusPct += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 7)
                bonusPct += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                bonusPct += GetXianmenSkillValue(*skill, level);
        }

        if (bonusPct > 0.0f)
            value += value * bonusPct / 100.0f;
    }

    void OnPlayerAfterUpdateCritPercentage(Player* player, WeaponAttackType /*attType*/, float& value) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float bonus = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == 1 && (order == 1 || order == 10))
                bonus += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && (order == 6 || order == 10))
                bonus += GetXianmenSkillValue(*skill, level);
        }

        value += bonus;
    }

    void OnPlayerAfterUpdateSpellCritChance(Player* player, uint32 /*school*/, float& value) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float const beforeValue = value;
        float bonus = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        std::vector<XianmenSkillConfig const*> effectiveSkills = sXianmenMgr->GetEffectiveSkills(player);
        for (XianmenSkillConfig const* skill : effectiveSkills)
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == XIANMEN_FULU_FACTION_ID && order == 2)
                bonus += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && (order == 6 || order == 10))
                bonus += GetXianmenSkillValue(*skill, level);
        }

        value += bonus;

        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (sXianmenMgr->IsEffectiveSkillDebugLogEnabled() && data && data->factionId == XIANMEN_FULU_FACTION_ID)
        {
            std::string const effectiveList = JoinXianmenSkillIds(effectiveSkills);
            std::ostringstream signature;
            signature << data->factionId << '|' << effectiveList << '|' << beforeValue << '|' << bonus << '|' << value;

            uint32 const guid = player->GetGUID().GetCounter();
            uint64 const cacheKey = (static_cast<uint64>(guid) << 32) | 0xFFFFFF02u;
            std::string const signatureText = signature.str();
            auto cacheItr = XianmenRatingDebugSignatures.find(cacheKey);
            if (cacheItr == XianmenRatingDebugSignatures.end() || cacheItr->second != signatureText)
            {
                XianmenRatingDebugSignatures[cacheKey] = signatureText;
                LOG_DEBUG("server.loading", "[仙门定位-符箓法术暴击计算] 玩家={} GUID={} 修为={} 实际生效=[{}] 暴击前={} 加成={} 暴击后={}",
                    player->GetName(), guid, level, effectiveList, beforeValue, bonus, value);
            }
        }
    }

    void OnPlayerAfterUpdateHitChances(Player* player, float& meleeHit, float& rangedHit, float& spellHit) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        float const beforeSpellHit = spellHit;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        std::vector<XianmenSkillConfig const*> effectiveSkills = sXianmenMgr->GetEffectiveSkills(player);
        for (XianmenSkillConfig const* skill : effectiveSkills)
        {
            if (!skill)
                continue;

            if (skill->factionId == 1 && GetXianmenSkillOrder(*skill) == 2)
            {
                float const bonus = GetXianmenSkillValue(*skill, level);
                meleeHit += bonus;
                rangedHit += bonus;
            }
        }

        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (sXianmenMgr->IsEffectiveSkillDebugLogEnabled() && data && data->factionId == XIANMEN_FULU_FACTION_ID)
        {
            std::string const effectiveList = JoinXianmenSkillIds(effectiveSkills);
            std::ostringstream signature;
            signature << data->factionId << '|' << effectiveList << '|' << beforeSpellHit << '|' << spellHit;

            uint32 const guid = player->GetGUID().GetCounter();
            uint64 const cacheKey = (static_cast<uint64>(guid) << 32) | 0xFFFFFF03u;
            std::string const signatureText = signature.str();
            auto cacheItr = XianmenRatingDebugSignatures.find(cacheKey);
            if (cacheItr == XianmenRatingDebugSignatures.end() || cacheItr->second != signatureText)
            {
                XianmenRatingDebugSignatures[cacheKey] = signatureText;
                LOG_DEBUG("server.loading", "[仙门定位-符箓法术命中计算] 玩家={} GUID={} 修为={} 实际生效=[{}] 命中前={} 命中后={}",
                    player->GetName(), guid, level, effectiveList, beforeSpellHit, spellHit);
            }
        }
    }

    void OnPlayerAfterUpdateSpellDamageAndHealing(Player* player, int256& healingBonus, int256 spellDamage[7]) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        int256 const beforeHealing = healingBonus;
        int256 const beforeSpellPower = GetMaxXianmenSpellDamage(spellDamage);

        float damagePct = 0.0f;
        float healingPct = 0.0f;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
        std::vector<XianmenSkillConfig const*> effectiveSkills = sXianmenMgr->GetEffectiveSkills(player);
        for (XianmenSkillConfig const* skill : effectiveSkills)
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == XIANMEN_FULU_FACTION_ID && (order == 1 || order == 10))
                damagePct += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 1)
                damagePct += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 6)
                damagePct += GetXianmenSkillValue(*skill, level);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                damagePct += GetXianmenSkillValue(*skill, level);
        }

        if (damagePct > 0.0f)
            for (uint8 school = 0; school < 7; ++school)
                spellDamage[school] += Acore::Number::ToInt256Saturated(Acore::Number::ToLongDouble(spellDamage[school]) * damagePct / 100.0L);

        if (healingPct > 0.0f)
            healingBonus += Acore::Number::ToInt256Saturated(Acore::Number::ToLongDouble(healingBonus) * healingPct / 100.0L);

        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (sXianmenMgr->IsEffectiveSkillDebugLogEnabled() && data && data->factionId)
        {
            int256 const afterSpellPower = GetMaxXianmenSpellDamage(spellDamage);
            std::string const effectiveList = JoinXianmenSkillIds(effectiveSkills);
            std::string const beforeSpellText = Acore::Number::ToDecimal65String(beforeSpellPower);
            std::string const afterSpellText = Acore::Number::ToDecimal65String(afterSpellPower);
            std::string const beforeHealingText = Acore::Number::ToDecimal65String(beforeHealing);
            std::string const afterHealingText = Acore::Number::ToDecimal65String(healingBonus);

            std::ostringstream signature;
            signature << data->factionId << '|' << effectiveList << '|' << damagePct << '|' << healingPct << '|'
                      << beforeSpellText << '|' << afterSpellText << '|' << beforeHealingText << '|' << afterHealingText;

            uint32 const guid = player->GetGUID().GetCounter();
            std::string const signatureText = signature.str();
            auto cacheItr = XianmenSpellPowerDebugSignatures.find(guid);
            if (cacheItr == XianmenSpellPowerDebugSignatures.end() || cacheItr->second != signatureText)
            {
                XianmenSpellPowerDebugSignatures[guid] = signatureText;
                LOG_DEBUG("server.loading", "[仙门定位-法强计算] 玩家={} GUID={} 门派={} 修为={} 实际生效=[{}] damagePct={} healingPct={} 法强前={} 法强后={} 治疗前={} 治疗后={}",
                    player->GetName(), guid, data->factionId, level, effectiveList, damagePct, healingPct,
                    beforeSpellText, afterSpellText, beforeHealingText, afterHealingText);
            }
        }
    }

    void OnPlayerAfterUpdateRating(Player* player, CombatRating cr, int256& amount) override
    {
        if (!player || !sXianmenMgr->IsEnabled())
            return;

        int256 const beforeAmount = amount;
        int256 bonus = 0;
        uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);

        std::vector<XianmenSkillConfig const*> effectiveSkills = sXianmenMgr->GetEffectiveSkills(player);
        for (XianmenSkillConfig const* skill : effectiveSkills)
        {
            if (!skill)
                continue;

            uint32 const order = GetXianmenSkillOrder(*skill);
            if (skill->factionId == 1 && order == 3 && (cr == CR_HASTE_MELEE || cr == CR_HASTE_RANGED))
                bonus += static_cast<int32>(GetXianmenSkillValue(*skill, level) * 10.0f);
            else if (skill->factionId == 1 && order == 4 && cr == CR_ARMOR_PENETRATION)
                bonus += static_cast<int32>(GetXianmenSkillValue(*skill, level) * 10.0f);
            else if (skill->factionId == XIANMEN_FULU_FACTION_ID && order == 1 && cr == CR_HASTE_SPELL)
                bonus += static_cast<int32>(GetXianmenSkillValue(*skill, level) * 10.0f);
            else if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 3 && (cr == CR_BLOCK || cr == CR_PARRY))
                bonus += static_cast<int32>(GetXianmenSkillValue(*skill, level) * 10.0f);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 6 && (cr == CR_CRIT_MELEE || cr == CR_CRIT_RANGED || cr == CR_CRIT_SPELL))
                bonus += static_cast<int32>(GetXianmenSkillValue(*skill, level) * 10.0f);
            else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10 && (cr == CR_HASTE_MELEE || cr == CR_HASTE_RANGED || cr == CR_HASTE_SPELL))
                bonus += static_cast<int32>(GetXianmenSkillValue(*skill, level) * 10.0f);
        }

        amount += bonus;

        if (cr == CR_HASTE_SPELL && sXianmenMgr->IsEffectiveSkillDebugLogEnabled())
        {
            XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
            if (data && data->factionId)
            {
                std::string const effectiveList = JoinXianmenSkillIds(effectiveSkills);
                std::string const beforeText = Acore::Number::ToDecimal65String(beforeAmount);
                std::string const bonusText = Acore::Number::ToDecimal65String(bonus);
                std::string const afterText = Acore::Number::ToDecimal65String(amount);
                std::ostringstream signature;
                signature << data->factionId << '|' << effectiveList << '|' << beforeText << '|' << bonusText << '|' << afterText;

                uint32 const guid = player->GetGUID().GetCounter();
                uint64 const cacheKey = (static_cast<uint64>(guid) << 32) | static_cast<uint32>(cr);
                std::string const signatureText = signature.str();
                auto cacheItr = XianmenRatingDebugSignatures.find(cacheKey);
                if (cacheItr == XianmenRatingDebugSignatures.end() || cacheItr->second != signatureText)
                {
                    XianmenRatingDebugSignatures[cacheKey] = signatureText;
                    LOG_DEBUG("server.loading", "[仙门定位-法术急速计算] 玩家={} GUID={} 门派={} 修为={} 实际生效=[{}] 评分前={} 加成={} 评分后={}",
                        player->GetName(), guid, data->factionId, level, effectiveList, beforeText, bonusText, afterText);
                }
            }
        }
    }

private:
    static void RewardTrialKillContribution(Player* player, Creature* killed)
    {
        uint64 const reward = GetXianmenTrialKillContribution(killed);
        if (!player || !reward || !sXianmenMgr->IsEnabled())
            return;

        std::string error;
        if (!sXianmenMgr->AddContribution(player, reward, error))
            return;

        SendXianmenState(player);
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff66ffcc[仙门系统]|r 击杀仙门试炼目标，宗门贡献增加 {}。",
            reward);
    }

    static void RewardSoulAffinity(Player* player, Creature* killed)
    {
        if (!player || !killed || !sXianmenMgr->IsEnabled() || !IsXianmenBossTarget(killed) || !HasEffectiveXianmenSkill(player, XIANMEN_WUHUN_FACTION_ID, 3))
            return;

        float const value = GetEffectiveXianmenSkillValue(player, XIANMEN_WUHUN_FACTION_ID, 3);
        uint64 reward = std::max<uint64>(1, static_cast<uint64>(std::max<float>(1.0f, value / 20.0f)));

        std::string error;
        if (sXianmenMgr->AddContribution(player, reward, error))
        {
            ApplyXianmenHealthGain(player, ScaleXianmenPercent(player->GetMaxHealthForCombat256(), value * 0.06f));
            ApplyXianmenPowerGain(player, POWER_MANA, ScaleXianmenPercent(player->GetMaxPowerForCombat256(POWER_MANA), value * 0.06f));
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff66ffcc[仙门系统]|r 魂力亲和触发，宗门贡献增加 {}。",
                reward);
        }
    }

    std::unordered_map<uint32, uint32> _regenTimers;
};

class XianmenUnitScript : public UnitScript
{
public:
    XianmenUnitScript() : UnitScript("XianmenUnitScript", true, { UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN }) { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint256& damage) override
    {
        if (!sXianmenMgr->IsEnabled() || damage == 0 || XianmenProcDamageGuard)
            return;

        if (Player* player = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
        {
            uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
            bool const controlledAttack = IsXianmenControlledUnit(player, attacker);
            for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(player))
            {
                if (!skill)
                    continue;

                uint32 const order = GetXianmenSkillOrder(*skill);
                if (skill->factionId == 1 && order == 2)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * 0.25f);
                }
                else if (skill->factionId == 1 && order == 4 && IsXianmenHighArmorTarget(target))
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level));
                }
                else if (skill->factionId == 1 && order == 9)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level));
                    if (IsXianmenBossTarget(target))
                        damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * 0.1f);
                }
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 5)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level));
                }
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 1)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * (controlledAttack ? 3.0f : 1.0f));
                }
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 2 && controlledAttack)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level));
                }
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 6)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * (controlledAttack ? 3.0f : 1.0f));
                }
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                {
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * (controlledAttack ? 2.5f : 1.0f));
                }
            }

            TriggerXianmenOffensiveProcs(player, target, nullptr, false, damage);
        }

        if (Player* victimPlayer = target ? target->ToPlayer() : nullptr)
        {
            uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(victimPlayer);
            for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(victimPlayer))
            {
                if (!skill)
                    continue;

                uint32 const order = GetXianmenSkillOrder(*skill);
                if (skill->factionId == XIANMEN_BODY_FACTION_ID && order == 1)
                    damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, GetXianmenSkillValue(*skill, level)));
            }

            ApplyXianmenReactiveMitigation(victimPlayer, damage);
            TryTriggerXianmenGoldenBody(victimPlayer, damage);
            TriggerXianmenDefensiveProcs(victimPlayer, attacker, damage, nullptr);
        }

        if (Player* owner = target ? target->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
        {
            if (IsXianmenControlledUnit(owner, target))
            {
                uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(owner);
                for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(owner))
                {
                    if (!skill || skill->factionId != XIANMEN_WUHUN_FACTION_ID)
                        continue;

                    uint32 const order = GetXianmenSkillOrder(*skill);
                    if (order == 7)
                        damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, GetXianmenSkillValue(*skill, level)));
                    else if (order == 10)
                        damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, GetXianmenSkillValue(*skill, level)));
                }
            }
        }
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, uint256& damage, SpellInfo const* spellInfo) override
    {
        if (!sXianmenMgr->IsEnabled() || damage == 0 || XianmenProcDamageGuard)
            return;

        if (Player* player = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
        {
            uint256 const beforeDamage = damage;
            float fuluDamagePct = 0.0f;
            uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(player);
            bool const controlledAttack = IsXianmenControlledUnit(player, attacker);
            std::vector<XianmenSkillConfig const*> effectiveSkills = sXianmenMgr->GetEffectiveSkills(player);
            for (XianmenSkillConfig const* skill : effectiveSkills)
            {
                if (!skill)
                    continue;

                uint32 const order = GetXianmenSkillOrder(*skill);
                if (skill->factionId == XIANMEN_FULU_FACTION_ID && (order == 4 || order == 10))
                {
                    float const value = GetXianmenSkillValue(*skill, level);
                    fuluDamagePct += value;
                    damage = AddXianmenPercent(damage, value);
                }
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 1)
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * (controlledAttack ? 3.0f : 1.0f));
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 2 && controlledAttack)
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level));
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 6)
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * (controlledAttack ? 3.0f : 1.0f));
                else if (skill->factionId == XIANMEN_WUHUN_FACTION_ID && order == 10)
                    damage = AddXianmenPercent(damage, GetXianmenSkillValue(*skill, level) * (controlledAttack ? 2.5f : 1.0f));
            }

            if (fuluDamagePct > 0.0f)
            {
                LOG_DEBUG("server.loading", "[仙门定位-符箓法术伤害修正] 玩家={} GUID={} 实际生效=[{}] sourceSpell={} 加成Pct={} 伤害前={} 伤害后={} 目标={} 目标GUID={}",
                    player->GetName(), player->GetGUID().GetCounter(), JoinXianmenSkillIds(effectiveSkills), spellInfo ? spellInfo->Id : 0, fuluDamagePct,
                    Acore::Number::ToDecimal65String(beforeDamage), Acore::Number::ToDecimal65String(damage),
                    target ? target->GetName() : "", target ? target->GetGUID().ToString() : "");
            }

            TriggerXianmenOffensiveProcs(player, target, spellInfo, true, damage);
        }

        if (Player* victimPlayer = target ? target->ToPlayer() : nullptr)
        {
            uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(victimPlayer);
            for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(victimPlayer))
            {
                if (skill && skill->factionId == XIANMEN_BODY_FACTION_ID && GetXianmenSkillOrder(*skill) == 1)
                    damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, GetXianmenSkillValue(*skill, level)));
            }

            ApplyXianmenReactiveMitigation(victimPlayer, damage);
            TryTriggerXianmenGoldenBody(victimPlayer, damage);
            TriggerXianmenDefensiveProcs(victimPlayer, attacker, damage, spellInfo);
        }

        if (Player* owner = target ? target->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
        {
            if (IsXianmenControlledUnit(owner, target))
            {
                uint32 const level = sXianmenMgr->GetPlayerXianmenLevel(owner);
                for (XianmenSkillConfig const* skill : sXianmenMgr->GetEffectiveSkills(owner))
                {
                    if (!skill || skill->factionId != XIANMEN_WUHUN_FACTION_ID)
                        continue;

                    uint32 const order = GetXianmenSkillOrder(*skill);
                    if (order == 7)
                        damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, GetXianmenSkillValue(*skill, level)));
                    else if (order == 10)
                        damage -= std::min<uint256>(damage, ScaleXianmenPercent(damage, GetXianmenSkillValue(*skill, level)));
                }
            }
        }
    }
};

class XianmenCommandScript : public CommandScript
{
public:
    XianmenCommandScript() : CommandScript("XianmenCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subTable =
        {
            { "帮助",     HandleHelpCommand,            SEC_PLAYER,        Console::No },
            { "列表",     HandleListCommand,            SEC_PLAYER,        Console::No },
            { "信息",     HandleInfoCommand,            SEC_PLAYER,        Console::No },
            { "加入",     HandleJoinCommand,            SEC_PLAYER,        Console::No },
            { "转投",     HandleJoinCommand,            SEC_PLAYER,        Console::No },
            { "退出",     HandleLeaveCommand,           SEC_PLAYER,        Console::No },
            { "离开",     HandleLeaveCommand,           SEC_PLAYER,        Console::No },
            { "升级",     HandleUpgradeCommand,         SEC_PLAYER,        Console::No },
            { "提升修为", HandleUpgradeCommand,         SEC_PLAYER,        Console::No },
            { "技能",     HandleSkillsCommand,          SEC_PLAYER,        Console::No },
            { "技能列表", HandleSkillsCommand,          SEC_PLAYER,        Console::No },
            { "解锁",     HandleUnlockCommand,          SEC_PLAYER,        Console::No },
            { "解锁技能", HandleUnlockCommand,          SEC_PLAYER,        Console::No },
            { "日常",     HandleDailyListCommand,       SEC_PLAYER,        Console::No },
            { "日常列表", HandleDailyListCommand,       SEC_PLAYER,        Console::No },
            { "完成日常", HandleCompleteDailyCommand,   SEC_PLAYER,        Console::No },
            { "门主技能", HandleLeaderSkillsCommand,    SEC_PLAYER,        Console::No },
            { "设置技能", HandleLeaderSkillsCommand,    SEC_PLAYER,        Console::No },
            { "生效技能", HandlePersonalActiveSkillsCommand, SEC_PLAYER,   Console::No },
            { "个人技能", HandlePersonalActiveSkillsCommand, SEC_PLAYER,   Console::No },
            { "发布日常", HandlePublishDailyCommand,    SEC_PLAYER,        Console::No },
            { "发布",     HandlePublishDailyCommand,    SEC_PLAYER,        Console::No },
            { "刷新",     HandleRefreshCommand,         SEC_PLAYER,        Console::No },
            { "界面",     HandleOpenUICommand,          SEC_PLAYER,        Console::No },
            { "打开",     HandleOpenUICommand,          SEC_PLAYER,        Console::No },
            { "状态",     HandleInfoCommand,            SEC_PLAYER,        Console::No },
            { "门派",     HandleListCommand,            SEC_PLAYER,        Console::No },
            { "设等级",   HandleSetLevelCommand,        SEC_GAMEMASTER,    Console::No },
            { "设置等级", HandleSetLevelCommand,        SEC_GAMEMASTER,    Console::No },
            { "加贡献",   HandleAddContributionCommand, SEC_GAMEMASTER,    Console::No },
            { "增加贡献", HandleAddContributionCommand, SEC_GAMEMASTER,    Console::No },
            { "设门主",   HandleSetLeaderCommand,       SEC_GAMEMASTER,    Console::No },
            { "设置门主", HandleSetLeaderCommand,       SEC_GAMEMASTER,    Console::No },
            { "结算",     HandleSettleCommand,          SEC_GAMEMASTER,    Console::No },
            { "门主结算", HandleSettleCommand,          SEC_GAMEMASTER,    Console::No },
            { "重载",     HandleReloadCommand,          SEC_GAMEMASTER,    Console::Yes },
            { "重新加载", HandleReloadCommand,          SEC_GAMEMASTER,    Console::Yes },
        };

        static ChatCommandTable rootTable =
        {
            { "仙门", subTable },
            { "xianmen", subTable },
        };

        return rootTable;
    }

private:
    static Player* GetPlayer(ChatHandler* handler)
    {
        return handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    }

    static bool CheckEnabled(ChatHandler* handler)
    {
        if (sXianmenMgr->IsEnabled())
            return true;

        handler->SendSysMessage("仙门系统已禁用。");
        return false;
    }

    static bool HandleHelpCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 命令:");
        handler->SendSysMessage(".仙门 列表");
        handler->SendSysMessage(".仙门 加入 <门派ID>");
        handler->SendSysMessage(".仙门 信息");
        handler->SendSysMessage(".仙门 升级 [次数]");
        handler->SendSysMessage(".仙门 技能");
        handler->SendSysMessage(".仙门 解锁 <技能ID>");
        handler->SendSysMessage(".仙门 生效技能 <技能ID1> <技能ID2> ...");
        handler->SendSysMessage(".仙门 日常");
        handler->SendSysMessage(".仙门 完成日常 <日常ID>");
        handler->SendSysMessage(".仙门 门主技能 <技能ID1> <技能ID2> ...");
        handler->SendSysMessage(".仙门 发布日常 <模板ID> [目标数量] [奖励档1-3] [奖励类型]");
        handler->SendSysMessage(".仙门 界面");
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 可加入门派:");
        for (XianmenFactionConfig const& faction : sXianmenMgr->GetFactions())
        {
            handler->PSendSysMessage("  {}. {} 修为上限:{}",
                faction.id,
                faction.name,
                sXianmenMgr->GetFactionMaxLevel(faction.id));
        }

        return true;
    }

    static bool HandleInfoCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sXianmenMgr->LoadPlayerData(player);
        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (!data || !data->factionId)
        {
            handler->SendSysMessage("|cff66ffcc[仙门系统]|r 你尚未加入仙门，使用 .仙门 列表 查看门派。");
            return true;
        }

        XianmenFactionConfig const* faction = sXianmenMgr->GetFaction(data->factionId);
        uint32 maxLevel = sXianmenMgr->GetFactionMaxLevel(data->factionId);
        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 门派: |cffffd700{}|r",
            faction ? faction->name : "未知");
        handler->PSendSysMessage("  修为等级: {}/{}", data->level, maxLevel);
        handler->PSendSysMessage("  当日贡献: {}", data->dailyContribution);
        handler->PSendSysMessage("  历史贡献: {}", data->historyContribution);
        if (data->level < maxLevel)
            handler->PSendSysMessage("  下一修为需要历史贡献: {}", sXianmenMgr->GetNextUpgradeRequirement(data->factionId, data->level, maxLevel));
        else
            handler->SendSysMessage("  下一修为需要历史贡献: 已满级");
        handler->PSendSysMessage("  技能解锁: {}/{}", static_cast<uint32>(data->unlockedSkills.size()), sXianmenMgr->GetUnlockedSlotCount(*data));
        handler->PSendSysMessage("  当前门主GUID: {}", sXianmenMgr->GetLeaderGuid(data->factionId));
        handler->PSendSysMessage("  你是否门主: {}", sXianmenMgr->IsLeader(data->factionId, player->GetGUID().GetCounter()) ? "是" : "否");
        return true;
    }

    static bool HandleJoinCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 加入 <门派ID>");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->JoinFaction(player, values[0], error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        XianmenFactionConfig const* faction = sXianmenMgr->GetFaction(values[0]);
        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 已加入 |cffffd700{}|r。", faction ? faction->name : "仙门");
        return true;
    }

    static bool HandleLeaveCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::string error;
        if (!sXianmenMgr->LeaveFaction(player, error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 已退出当前仙门。");
        return true;
    }

    static bool HandleUpgradeCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        uint32 count = values.empty() ? 1 : values[0];

        std::string error;
        uint32 upgraded = 0;
        if (!sXianmenMgr->UpgradePlayerLevel(player, count, upgraded, error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 修为已提升 {} 级，当前修为 {}/{}。",
            upgraded,
            sXianmenMgr->GetPlayerXianmenLevel(player),
            sXianmenMgr->GetFactionMaxLevel(sXianmenMgr->GetPlayerFactionId(player)));
        return true;
    }

    static bool HandleSkillsCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sXianmenMgr->LoadPlayerData(player);
        XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (!data || !data->factionId)
        {
            handler->SendSysMessage("你尚未加入仙门。");
            return true;
        }

        std::set<uint32> const& activeSkills = sXianmenMgr->GetActiveSkills(data->factionId);
        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 技能池: [已解锁] [门主开放] [个人生效]");

        for (XianmenSkillConfig const* skill : sXianmenMgr->GetSkillsForFaction(data->factionId))
        {
            bool unlocked = data->unlockedSkills.count(skill->id) != 0;
            bool active = activeSkills.count(skill->id) != 0;
            bool personalActive = data->personalActiveSkills.count(skill->id) != 0;
            handler->PSendSysMessage("  {}. {} [{}{}{}] - {}",
                skill->id,
                skill->name,
                unlocked ? "已解锁" : "未解锁",
                active ? "/门主开放" : "",
                personalActive ? "/个人生效" : "",
                skill->description);
        }

        return true;
    }

    static bool HandleUnlockCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 解锁 <技能ID>");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->UnlockSkill(player, values[0], error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        XianmenSkillConfig const* skill = sXianmenMgr->GetSkill(values[0]);
        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 已解锁技能: |cffffd700{}|r。", skill ? skill->name : "未知技能");
        return true;
    }

    static bool HandleDailyListCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<XianmenDailyView> dailies = sXianmenMgr->GetTodayDailies(player);
        if (dailies.empty())
        {
            sXianmenMgr->LoadPlayerData(player);
            XianmenPlayerData const* data = sXianmenMgr->GetPlayerData(player->GetGUID().GetCounter());
            if (!data || !data->factionId)
            {
                handler->SendSysMessage("你尚未加入仙门。");
                return true;
            }

            handler->SendSysMessage("|cff66ffcc[仙门系统]|r 今日暂无已发布的门派日常。");
            return true;
        }

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 今日本门日常:");
        for (XianmenDailyView const& daily : dailies)
        {
            handler->PSendSysMessage("  {}. {} 进度 {}/{} [{}] 奖励: {} x{}",
                daily.dailyId,
                daily.templateName.empty() ? "未知日常" : daily.templateName,
                std::min<uint32>(daily.progress, daily.targetCount),
                daily.targetCount,
                daily.completed ? "已完成" : "未完成",
                daily.rewardName.empty() ? "未知奖励" : daily.rewardName,
                daily.rewardAmount);
        }

        return true;
    }

    static bool HandleCompleteDailyCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 完成日常 <日常ID>");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->CompleteDaily(player, values[0], error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 日常已完成，奖励已发放。");
        return true;
    }

    static bool HandlePublishDailyCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 发布日常 <模板ID> [目标数量] [奖励档1-3] [奖励类型]");
            handler->SendSysMessage("|cff66ffcc[仙门系统]|r 可用日常模板:");
            for (XianmenDailyTemplateConfig const& daily : sXianmenMgr->GetDailyTemplates())
            {
                handler->PSendSysMessage("  {}. {} 类型={} 默认数量={} 默认档={}",
                    daily.id, daily.name, daily.type, daily.defaultCount, daily.defaultRewardTier);
            }

            return true;
        }

        uint32 templateId = values[0];
        uint32 targetCount = values.size() > 1 ? values[1] : 0;
        uint32 rewardTier = values.size() > 2 ? values[2] : 0;
        uint32 rewardType = values.size() > 3 ? values[3] : 0;

        std::string error;
        if (!sXianmenMgr->PublishDaily(player, templateId, targetCount, rewardTier, rewardType, error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        XianmenDailyTemplateConfig const* daily = sXianmenMgr->GetDailyTemplate(templateId);
        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 已发布日常: {}。",
            daily ? daily->name : "未知日常");
        return true;
    }

    static bool HandleLeaderSkillsCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 门主技能 <技能ID1> <技能ID2> ...");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->SetActiveSkills(player, values, error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 门主开放技能已更新，成员需要从开放技能中选择个人生效技能。");
        return true;
    }

    static bool HandlePersonalActiveSkillsCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        std::string error;
        if (!sXianmenMgr->SetPersonalActiveSkills(player, values, error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 个人生效技能已更新，当前选择 {} 个。",
            static_cast<uint32>(std::set<uint32>(values.begin(), values.end()).size()));
        return true;
    }

    static bool HandleRefreshCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sXianmenMgr->LoadPlayerData(player);
        sXianmenMgr->RefreshPassiveAuras(player);
        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 已刷新当前仙门被动。");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        SendXianmenOpenUI(player);
        return true;
    }

    static bool HandleSetLevelCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 设等级 <等级>");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->SetPlayerLevel(player, values[0], error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 修为等级已设置为 {}。", values[0]);
        return true;
    }

    static bool HandleAddContributionCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 加贡献 <数值>");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->AddContribution(player, values[0], error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 已增加当日贡献 {}。", values[0]);
        return true;
    }

    static bool HandleSetLeaderCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return true;

        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<uint32> values = ParseUIntList(args);
        if (values.empty())
        {
            handler->SendSysMessage("用法: .仙门 设门主 <门派ID>");
            return true;
        }

        std::string error;
        if (!sXianmenMgr->SetLeader(player, values[0], error))
        {
            handler->PSendSysMessage("|cffff0000[仙门系统]|r {}", error);
            return true;
        }

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 已设置你为当前门派门主。");
        return true;
    }

    static bool HandleSettleCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return true;

        sXianmenMgr->SettleDailyLeaders(true);
        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 已强制执行每日门主结算。");
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sXianmenMgr->LoadConfig();
        if (!sXianmenMgr->IsEnabled())
        {
            handler->SendSysMessage("仙门系统已禁用。");
            return true;
        }

        sXianmenMgr->LoadAll();
        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 配置已重载。");
        return true;
    }
};
} // namespace

void AddSC_mod_xianmen_system()
{
    new XianmenWorldScript();
    new XianmenNativeDailyCreatureScript();
    new XianmenPlayerScript();
    new XianmenUnitScript();
    new XianmenCommandScript();
}

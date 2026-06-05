/*
 * 仙门系统
 */

#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
static constexpr const char* XIANMEN_ADDON_PREFIX = "XIANMEN";
static constexpr size_t XIANMEN_MAX_ADDON_PAYLOAD = 220;

struct XianmenFactionConfig
{
    uint32 id = 0;
    std::string name;
    std::string description;
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

struct XianmenPlayerData
{
    uint32 factionId = 0;
    uint32 level = 0;
    uint64 dailyContribution = 0;
    std::set<uint32> unlockedSkills;
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

std::string EscapePayload(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '^' || ch == '~' || ch == '|')
            ch = ' ';
    }

    return value;
}

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
        _maxActiveSkills = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.MaxActiveSkills", 5));
        _maxDailyPublishes = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("XianmenSystem.MaxDailyPublishes", 20));
        _settleCheckIntervalMs = std::max<uint32>(5, sConfigMgr->GetOption<uint32>("XianmenSystem.LeaderSettleCheckInterval", 60)) * IN_MILLISECONDS;
    }

    bool IsEnabled() const
    {
        return _enabled;
    }

    bool ShouldAnnounceOnLogin() const
    {
        return _announceOnLogin;
    }

    uint32 GetSettleCheckIntervalMs() const
    {
        return _settleCheckIntervalMs;
    }

    void LoadAll()
    {
        LoadFactions();
        LoadSkills();
        LoadDailyTemplates();
        LoadRewardConfigs();
        EnsureLeaderRows();
        LoadActiveSkills();
        LoadLeaders();

        LOG_INFO("server.loading", "→仙门系统√ 门派={} 技能={} 日常模板={} 奖励={} 生效技能={}",
            static_cast<uint32>(_factions.size()),
            static_cast<uint32>(_skills.size()),
            static_cast<uint32>(_dailyTemplates.size()),
            static_cast<uint32>(_rewardConfigs.size()),
            GetActiveSkillCount());
    }

    void LoadFactions()
    {
        _factions.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `门派ID`, `门派名称`, `门派描述`, `加入需求ID` "
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
            faction.joinRequirementId = fields[3].Get<uint32>();

            if (faction.id)
                _factions[faction.id] = faction;
        } while (result->NextRow());
    }

    void LoadSkills()
    {
        _skills.clear();
        _skillsByFaction.clear();

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
            "SELECT `门派ID`, `修为等级`, `当日贡献`, `已解锁技能` FROM `_仙门_玩家` WHERE `角色GUID` = {}", guid))
        {
            Field* fields = result->Fetch();
            data.factionId = fields[0].Get<uint32>();
            data.level = fields[1].Get<uint32>();
            data.dailyContribution = fields[2].Get<uint64>();
            data.unlockedSkills = ParseUIntSet(fields[3].Get<std::string>());
        }

        _playerData[guid] = data;
    }

    void UnloadPlayerData(uint32 guid)
    {
        _playerData.erase(guid);
    }

    void DeletePlayerData(uint32 guid)
    {
        _playerData.erase(guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_玩家` WHERE `角色GUID` = {}", guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_玩家日常记录` WHERE `角色GUID` = {}", guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_仙器玩家槽位` WHERE `角色GUID` = {}", guid);
        CharacterDatabase.Execute("DELETE FROM `_仙门_玩家丹药属性` WHERE `角色GUID` = {}", guid);

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
            error = "你已经加入仙门，需先退出后才能转投。";
            return false;
        }

        uint32 now = GetNow();
        CharacterDatabase.Execute(
            "REPLACE INTO `_仙门_玩家` (`角色GUID`, `门派ID`, `修为等级`, `当日贡献`, `历史贡献`, `已解锁技能`, `加入时间`, `更新时间`) "
            "VALUES ({}, {}, 1, 0, 0, '', {}, {})",
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

        std::set<uint32> uniqueSkills(skillIds.begin(), skillIds.end());
        if (uniqueSkills.empty())
        {
            error = "至少需要选择 1 个技能。";
            return false;
        }

        if (uniqueSkills.size() > _maxActiveSkills)
        {
            error = "门主最多只能选择 5 个生效技能。";
            return false;
        }

        for (uint32 skillId : uniqueSkills)
        {
            XianmenSkillConfig const* skill = GetSkill(skillId);
            if (!skill || skill->factionId != data->factionId)
            {
                error = "存在不属于本门派的技能。";
                return false;
            }

            if (!data->unlockedSkills.count(skillId))
            {
                error = "门主只能从自己已解锁技能中选择。";
                return false;
            }
        }

        uint32 now = GetNow();
        std::string active = JoinUIntSet(uniqueSkills);
        CharacterDatabase.EscapeString(active);
        CharacterDatabase.Execute(
            "UPDATE `_仙门_门派状态` SET `激活技能` = '{}', `更新时间` = {} WHERE `门派ID` = {}",
            active, now, data->factionId);
        _activeSkills[data->factionId] = uniqueSkills;

        XianmenFactionConfig const* faction = GetFaction(data->factionId);
        LOG_INFO("server.loading", "仙门系统: 门主 {} 设置 {} 生效技能数={}",
            player->GetName(), faction ? faction->name : "未知门派", static_cast<uint32>(uniqueSkills.size()));
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

        level = std::min<uint32>(_maxLevel, level);
        CharacterDatabase.Execute(
            "UPDATE `_仙门_玩家` SET `修为等级` = {}, `更新时间` = {} WHERE `角色GUID` = {}",
            level, GetNow(), guid);

        data->level = level;
        return true;
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

        if (QueryResult done = CharacterDatabase.Query(
            "SELECT `已完成` FROM `_仙门_玩家日常记录` WHERE `角色GUID` = {} AND `日常ID` = {} AND `日期` = {}",
            guid, dailyId, today))
        {
            if ((*done)[0].Get<uint8>() != 0)
            {
                error = "你今天已经完成过这个日常。";
                return false;
            }
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
            return;

        std::set<uint32> const& activeSkills = GetActiveSkills(data->factionId);
        for (uint32 skillId : activeSkills)
        {
            if (!data->unlockedSkills.count(skillId))
                continue;

            XianmenSkillConfig const* skill = GetSkill(skillId);
            if (!skill || !skill->spellId)
                continue;

            if (!sSpellMgr->GetSpellInfo(skill->spellId))
                continue;

            player->AddAura(skill->spellId, player);
        }

        player->UpdateAllStats();
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

    uint32 GetActiveSkillCount() const
    {
        uint32 count = 0;
        for (auto const& pair : _activeSkills)
            count += static_cast<uint32>(pair.second.size());
        return count;
    }

    bool _enabled = true;
    bool _announceOnLogin = true;
    bool _enableDailyLeaderSettle = true;
    uint32 _maxLevel = 100;
    uint32 _unlockLevelStep = 10;
    uint32 _maxActiveSkills = 5;
    uint32 _maxDailyPublishes = 20;
    uint32 _settleCheckIntervalMs = 60 * IN_MILLISECONDS;

    std::unordered_map<uint32, XianmenFactionConfig> _factions;
    std::unordered_map<uint32, XianmenSkillConfig> _skills;
    std::unordered_map<uint32, XianmenDailyTemplateConfig> _dailyTemplates;
    std::unordered_map<uint32, XianmenRewardConfig> _rewardConfigs;
    std::unordered_map<uint32, std::vector<uint32>> _skillsByFaction;
    std::unordered_map<uint32, XianmenPlayerData> _playerData;
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

    std::ostringstream payload;
    payload << "XM_STATE:"
            << factionId << '|'
            << EscapePayload(faction ? faction->name : "") << '|'
            << level << '|'
            << dailyContribution << '|'
            << unlockSlots << '|'
            << leaderGuid << '|'
            << (isLeader ? 1 : 0) << '|'
            << JoinUIntSet(unlocked) << '|'
            << JoinUIntSet(active);

    SendXianmenPayload(player, payload.str());
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

            payload << skill->id << '^'
                    << skill->factionId << '^'
                    << skill->order << '^'
                    << EscapePayload(skill->name) << '^'
                    << skill->type << '^'
                    << skill->spellId << '^'
                    << (unlocked ? 1 : 0) << '^'
                    << (active ? 1 : 0) << '^'
                    << EscapePayload(skill->description);
            first = false;
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
    SendXianmenDailies(player);
}

void SendXianmenOpenUI(Player* player)
{
    SendXianmenPayload(player, "XM_OPEN");
    SendXianmenAll(player);
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

class XianmenPlayerScript : public PlayerScript
{
public:
    XianmenPlayerScript() : PlayerScript("XianmenPlayerScript") { }

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

        sXianmenMgr->UnloadPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
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
            SendXianmenResult(player, "SET_ACTIVE", ok, ok ? "门派生效技能已更新" : error);
            SendXianmenAll(player);
            return;
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
            { "退出",     HandleLeaveCommand,           SEC_PLAYER,        Console::No },
            { "离开",     HandleLeaveCommand,           SEC_PLAYER,        Console::No },
            { "技能",     HandleSkillsCommand,          SEC_PLAYER,        Console::No },
            { "技能列表", HandleSkillsCommand,          SEC_PLAYER,        Console::No },
            { "解锁",     HandleUnlockCommand,          SEC_PLAYER,        Console::No },
            { "解锁技能", HandleUnlockCommand,          SEC_PLAYER,        Console::No },
            { "日常",     HandleDailyListCommand,       SEC_PLAYER,        Console::No },
            { "日常列表", HandleDailyListCommand,       SEC_PLAYER,        Console::No },
            { "完成日常", HandleCompleteDailyCommand,   SEC_PLAYER,        Console::No },
            { "门主技能", HandleLeaderSkillsCommand,    SEC_PLAYER,        Console::No },
            { "设置技能", HandleLeaderSkillsCommand,    SEC_PLAYER,        Console::No },
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
        handler->SendSysMessage(".仙门 技能");
        handler->SendSysMessage(".仙门 解锁 <技能ID>");
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
            handler->PSendSysMessage("  {}. {}", faction.id, faction.name);
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
        handler->PSendSysMessage("|cff66ffcc[仙门系统]|r 门派: |cffffd700{}|r",
            faction ? faction->name : "未知");
        handler->PSendSysMessage("  修为等级: {}/100", data->level);
        handler->PSendSysMessage("  当日贡献: {}", data->dailyContribution);
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
        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 技能池: [已解锁] [门主生效]");

        for (XianmenSkillConfig const* skill : sXianmenMgr->GetSkillsForFaction(data->factionId))
        {
            bool unlocked = data->unlockedSkills.count(skill->id) != 0;
            bool active = activeSkills.count(skill->id) != 0;
            handler->PSendSysMessage("  {}. {} [{}{}] - {}",
                skill->id, skill->name, unlocked ? "已解锁" : "未解锁", active ? "/生效" : "", skill->description);
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

        handler->SendSysMessage("|cff66ffcc[仙门系统]|r 门派生效技能已更新，成员小退后按新技能生效。");
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
    new XianmenPlayerScript();
    new XianmenCommandScript();
}

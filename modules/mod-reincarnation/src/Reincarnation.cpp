/*
 * 转身系统 - 主实现文件
 */

#include "Reincarnation.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "Configuration/Config.h"
#include "ModuleManager.h"
#include "Chat.h"

// 获取需求模块接口
static RequirementInterface* GetRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    if (!mgr)
        return nullptr;
    return mgr->GetRequirementModule();
}

ReincarnationMgr::ReincarnationMgr()
{
}

ReincarnationMgr::~ReincarnationMgr()
{
    _configs.clear();
    _playerData.clear();
}

ReincarnationMgr* ReincarnationMgr::instance()
{
    static ReincarnationMgr instance;
    return &instance;
}

void ReincarnationMgr::LoadReincarnationConfig()
{
    uint32 oldMSTime = getMSTime();

    _configs.clear();

    // 从数据库读取配置
    QueryResult result = WorldDatabase.Query(
        "SELECT `id`, `转身等级`, `模板_需求`, `奖励属性`, `奖励天赋` FROM `_转身系统` ORDER BY `转身等级`");

    if (!result)
    {
        LOG_INFO("server.loading", ">> 转身系统: 未找到配置数据，使用默认值");
        // 创建默认配置
        ReincarnationConfig defaultConfig;
        defaultConfig.id = 0;
        defaultConfig.reincarnationLevel = 0;
        defaultConfig.requirementTemplateId = 0;
        defaultConfig.bonusStats = 5.0f;
        defaultConfig.bonusTalentPoints = 10;
        _configs[0] = defaultConfig;
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ReincarnationConfig config;
        config.id = fields[0].Get<uint32>();
        config.reincarnationLevel = fields[1].Get<uint32>();
        config.requirementTemplateId = fields[2].Get<uint32>();
        config.bonusStats = fields[3].Get<float>();
        config.bonusTalentPoints = fields[4].Get<uint32>();

        _configs[config.reincarnationLevel] = config;
        ++count;

    } while (result->NextRow());

    LOG_INFO("server.loading", ">> 转身系统: 加载 {} 条配置数据，耗时 {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

ReincarnationConfig const* ReincarnationMgr::GetConfigForLevel(uint32 level) const
{
    // 优先查找特定等级的配置
    auto itr = _configs.find(level);
    if (itr != _configs.end())
        return &(itr->second);

    // 没有特定配置，返回默认配置(等级0)
    return GetDefaultConfig();
}

ReincarnationConfig const* ReincarnationMgr::GetDefaultConfig() const
{
    auto itr = _configs.find(0);
    if (itr != _configs.end())
        return &(itr->second);
    return nullptr;
}

void ReincarnationMgr::RecalculatePlayerBonus(PlayerReincarnationData& data, uint32 level)
{
    data.reincarnationLevel = level;
    data.totalBonusStats = 0.0f;
    data.totalBonusTalentPoints = 0;

    // 累加每个等级的奖励
    for (uint32 i = 1; i <= level; ++i)
    {
        ReincarnationConfig const* config = GetConfigForLevel(i);
        if (config)
        {
            data.totalBonusStats += config->bonusStats;
            data.totalBonusTalentPoints += config->bonusTalentPoints;
        }
    }
}

void ReincarnationMgr::LoadPlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 初始化玩家数据
    _playerData[playerGuid] = PlayerReincarnationData();

    QueryResult result = CharacterDatabase.Query(
        "SELECT `转身等级`, `累计属性加成`, `累计天赋点` FROM `_转身系统_玩家数据` WHERE `角色id` = {}", playerGuid);

    if (!result)
        return;

    Field* fields = result->Fetch();

    _playerData[playerGuid].reincarnationLevel = fields[0].Get<uint32>();
    _playerData[playerGuid].totalBonusStats = fields[1].Get<float>();
    _playerData[playerGuid].totalBonusTalentPoints = fields[2].Get<uint32>();

    LOG_DEBUG("module", "转身系统: 为玩家 {} 加载数据，转身等级: {}，属性加成: {:.1f}%，天赋点: {}",
        player->GetName(),
        _playerData[playerGuid].reincarnationLevel,
        _playerData[playerGuid].totalBonusStats,
        _playerData[playerGuid].totalBonusTalentPoints);
}

void ReincarnationMgr::SavePlayerData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return;

    CharacterDatabase.Execute(
        "REPLACE INTO `_转身系统_玩家数据` (`角色id`, `转身等级`, `累计属性加成`, `累计天赋点`) "
        "VALUES ({}, {}, {}, {})",
        playerGuid,
        itr->second.reincarnationLevel,
        itr->second.totalBonusStats,
        itr->second.totalBonusTalentPoints);

    LOG_DEBUG("module", "转身系统: 为玩家 {} 保存数据", player->GetName());
}

void ReincarnationMgr::OnPlayerLogout(uint32 playerGuid)
{
    _playerData.erase(playerGuid);
}

PlayerReincarnationData* ReincarnationMgr::GetPlayerData(uint32 playerGuid)
{
    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return nullptr;
    return &(itr->second);
}

bool ReincarnationMgr::CanReincarnate(Player* player, std::string& errorMsg) const
{
    if (!player)
    {
        errorMsg = "玩家无效";
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 获取玩家当前数据
    auto playerItr = _playerData.find(playerGuid);
    uint32 currentLevel = 0;
    if (playerItr != _playerData.end())
    {
        currentLevel = playerItr->second.reincarnationLevel;
    }

    // 获取下一转的配置
    uint32 nextLevel = currentLevel + 1;
    ReincarnationConfig const* config = GetConfigForLevel(nextLevel);
    if (!config)
    {
        errorMsg = "找不到转身配置数据";
        return false;
    }

    // 检查需求模板
    if (config->requirementTemplateId > 0)
    {
        RequirementInterface* reqModule = GetRequirementModule();
        if (!reqModule)
        {
            LOG_WARN("module", "转身系统: 配置了需求模板ID {}，但需求系统模块未加载", config->requirementTemplateId);
        }
        else
        {
            if (!reqModule->CheckRequirements(player, config->requirementTemplateId, true))
            {
                errorMsg = "不满足转身条件要求";
                return false;
            }
        }
    }

    return true;
}

bool ReincarnationMgr::DoReincarnate(Player* player)
{
    if (!player)
        return false;

    std::string errorMsg;
    if (!CanReincarnate(player, errorMsg))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("{}", errorMsg);
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 确保玩家数据存在
    if (_playerData.find(playerGuid) == _playerData.end())
    {
        _playerData[playerGuid] = PlayerReincarnationData();
    }

    PlayerReincarnationData& playerData = _playerData[playerGuid];
    float oldBonusStats = playerData.totalBonusStats;

    // 获取下一转的配置
    uint32 nextLevel = playerData.reincarnationLevel + 1;
    ReincarnationConfig const* config = GetConfigForLevel(nextLevel);
    if (!config)
        return false;

    // 消耗需求
    if (config->requirementTemplateId > 0)
    {
        RequirementInterface* reqModule = GetRequirementModule();
        if (reqModule)
        {
            if (!reqModule->ConsumeRequirements(player, config->requirementTemplateId))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("消耗转身条件失败");
                return false;
            }
        }
    }

    // 更新玩家数据
    playerData.reincarnationLevel = nextLevel;
    playerData.totalBonusStats += config->bonusStats;
    playerData.totalBonusTalentPoints += config->bonusTalentPoints;

    // 更新属性加成（先移除旧的，再应用新的）
    UpdateReincarnationStats(player, oldBonusStats, playerData.totalBonusStats);

    // 不再内置修改角色等级，转身后直接重算可用天赋点。
    player->InitTalentForLevel();

    // 立即保存到数据库
    SavePlayerData(player);

    // 发送通知
    std::string message = "|cff00ff00恭喜你完成第" + std::to_string(nextLevel) + "次转身！|r\n";
    message += "|cff00ffff全属性加成: +" + std::to_string(static_cast<int>(config->bonusStats)) + "% (累计: " +
               std::to_string(static_cast<int>(playerData.totalBonusStats)) + "%)|r\n";
    message += "|cffff00ff天赋点奖励: +" + std::to_string(config->bonusTalentPoints) + "点 (累计: " +
               std::to_string(playerData.totalBonusTalentPoints) + "点)|r";

    ChatHandler(player->GetSession()).PSendSysMessage("{}", message);

    return true;
}

bool ReincarnationMgr::ModifyReincarnationLevel(Player* player, int32 delta)
{
    if (!player || delta == 0)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 确保玩家数据存在
    if (_playerData.find(playerGuid) == _playerData.end())
    {
        _playerData[playerGuid] = PlayerReincarnationData();
    }

    PlayerReincarnationData& playerData = _playerData[playerGuid];
    uint32 oldLevel = playerData.reincarnationLevel;
    float oldBonusStats = playerData.totalBonusStats;

    // 计算新等级
    int32 newLevel = static_cast<int32>(playerData.reincarnationLevel) + delta;
    if (newLevel < 0)
        newLevel = 0;

    // 重新计算奖励
    RecalculatePlayerBonus(playerData, static_cast<uint32>(newLevel));

    // 更新属性加成
    UpdateReincarnationStats(player, oldBonusStats, playerData.totalBonusStats);

    // 保存到数据库
    SavePlayerData(player);

    return true;
}

bool ReincarnationMgr::SetReincarnationLevel(Player* player, uint32 level)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 确保玩家数据存在
    if (_playerData.find(playerGuid) == _playerData.end())
    {
        _playerData[playerGuid] = PlayerReincarnationData();
    }

    PlayerReincarnationData& playerData = _playerData[playerGuid];
    float oldBonusStats = playerData.totalBonusStats;

    // 重新计算奖励
    RecalculatePlayerBonus(playerData, level);

    // 更新属性加成
    UpdateReincarnationStats(player, oldBonusStats, playerData.totalBonusStats);

    // 保存到数据库
    SavePlayerData(player);

    return true;
}

uint32 ReincarnationMgr::GetPlayerReincarnationLevel(uint32 playerGuid) const
{
    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return 0;
    return itr->second.reincarnationLevel;
}

float ReincarnationMgr::GetPlayerBonusStats(uint32 playerGuid) const
{
    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return 0.0f;
    return itr->second.totalBonusStats;
}

uint32 ReincarnationMgr::GetPlayerBonusTalentPoints(uint32 playerGuid) const
{
    auto itr = _playerData.find(playerGuid);
    if (itr == _playerData.end())
        return 0;
    return itr->second.totalBonusTalentPoints;
}

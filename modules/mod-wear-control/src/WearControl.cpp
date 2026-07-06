/*
 * 穿戴控制模块 - 表驱动的跨系统物品穿戴限制
 */

#include "WearControl.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ModuleManager.h"
#include "Player.h"
#include "RequirementInterface.h"
#include "ScriptMgr.h"
#include "StringFormat.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
using WearRuleList = std::vector<WearControl::WearRule>;

std::unordered_map<uint32, WearRuleList> g_wearRules;
std::unordered_map<uint32, uint32> g_playerWearLevels;
std::mutex g_playerWearLevelsMutex;
int8 g_wearLevelTableState = -1;

constexpr uint32 WEAR_CONTROL_LOAD_DELAY_MS = 3000;
constexpr uint32 DEFAULT_PLAYER_WEAR_LEVEL = 1;

RequirementInterface* GetRequirementModule()
{
    ModuleManager* mgr = sModuleManager;
    return mgr ? mgr->GetRequirementModule() : nullptr;
}

bool WearControlTableExists()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT COUNT(*) FROM `information_schema`.`TABLES` "
        "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '_穿戴控制表'");

    if (!result)
        return false;

    Field* fields = result->Fetch();
    return fields[0].Get<uint64>() > 0;
}

bool WearControlColumnExists(char const* columnName)
{
    QueryResult result = WorldDatabase.Query(
        "SELECT COUNT(*) FROM `information_schema`.`COLUMNS` "
        "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '_穿戴控制表' AND `COLUMN_NAME` = '{}'",
        columnName);

    if (!result)
        return false;

    Field* fields = result->Fetch();
    return fields[0].Get<uint64>() > 0;
}

bool WearLevelPermissionTableExists()
{
    if (g_wearLevelTableState >= 0)
        return g_wearLevelTableState == 1;

    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM `information_schema`.`TABLES` "
        "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '_穿戴等级权限'");

    bool const exists = result && result->Fetch()[0].Get<uint64>() > 0;
    g_wearLevelTableState = exists ? 1 : 0;

    if (!exists)
        LOG_INFO("server.loading", "穿戴控制: characters 表 `_穿戴等级权限` 不存在，玩家默认按 1 级穿戴权限处理；导入 modules/mod-wear-control/sql/characters/_穿戴等级权限.sql 后重启");

    return exists;
}

uint32 LoadPlayerWearLevelFromDatabase(uint32 playerGuid)
{
    if (!WearLevelPermissionTableExists())
        return DEFAULT_PLAYER_WEAR_LEVEL;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `穿戴等级` FROM `_穿戴等级权限` WHERE `玩家GUID` = {}", playerGuid);

    if (!result)
        return DEFAULT_PLAYER_WEAR_LEVEL;

    return result->Fetch()[0].Get<uint32>();
}

void EraseCachedPlayerWearLevel(uint32 playerGuid)
{
    std::lock_guard<std::mutex> guard(g_playerWearLevelsMutex);
    g_playerWearLevels.erase(playerGuid);
}

WearControl::WearRule const* FindRule(uint32 itemId, uint8 limitType, uint8 slotPosition)
{
    auto itr = g_wearRules.find(itemId);
    if (itr == g_wearRules.end())
        return nullptr;

    WearControl::WearRule const* wildcard = nullptr;

    for (WearControl::WearRule const& rule : itr->second)
    {
        if (rule.limitType != limitType)
            continue;

        if (rule.slotPosition == slotPosition)
            return &rule;

        if (rule.slotPosition == 0)
            wildcard = &rule;
    }

    return wildcard;
}

class WearControlWorldScript : public WorldScript
{
public:
    WearControlWorldScript() : WorldScript("WearControlWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload && _loaded)
            WearControl::Reload();
    }

    void OnStartup() override
    {
        _startupComplete = true;
        _loadTimer = 0;
        WearLevelPermissionTableExists();
    }

    void OnUpdate(uint32 diff) override
    {
        if (_loaded || !_startupComplete)
            return;

        _loadTimer += diff;
        if (_loadTimer < WEAR_CONTROL_LOAD_DELAY_MS)
            return;

        WearControl::Reload();
        _loaded = true;
    }

private:
    bool _startupComplete = false;
    bool _loaded = false;
    uint32 _loadTimer = 0;
};

class WearControlPlayerScript : public PlayerScript
{
public:
    WearControlPlayerScript() : PlayerScript("WearControlPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        WearControl::GetPlayerWearLevel(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        EraseCachedPlayerWearLevel(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        uint32 const playerGuid = guid.GetCounter();
        EraseCachedPlayerWearLevel(playerGuid);

        if (WearLevelPermissionTableExists())
            CharacterDatabase.Execute("DELETE FROM `_穿戴等级权限` WHERE `玩家GUID` = {}", playerGuid);
    }
};
}

namespace WearControl
{
char const* GetLimitName(uint8 limitType)
{
    switch (limitType)
    {
        case WEAR_LIMIT_ASCENSION:
            return "飞升";
        case WEAR_LIMIT_XIANQI:
            return "仙器";
        default:
            return "无限制";
    }
}

void Reload()
{
    g_wearRules.clear();

    if (!WearControlTableExists())
    {
        LOG_INFO("server.loading", "穿戴控制: `_穿戴控制表` 不存在，跳过加载；导入 modules/mod-wear-control/sql/world/_穿戴控制表.sql 后重启或 reload config");
        return;
    }

    bool const hasWearLevelColumn = WearControlColumnExists("穿戴等级");
    if (!hasWearLevelColumn)
        LOG_INFO("server.loading", "穿戴控制: `_穿戴控制表`.`穿戴等级` 字段不存在，当前所有规则按 0=不限制处理");

    QueryResult result = hasWearLevelColumn
        ? WorldDatabase.Query(
            "SELECT `物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释` "
            "FROM `_穿戴控制表`")
        : WorldDatabase.Query(
            "SELECT `物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, 0 AS `穿戴等级`, `限制类型`, `注释` "
            "FROM `_穿戴控制表`");

    if (!result)
    {
        LOG_INFO("server.loading", "→穿戴控制表√ 规则=0");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        WearRule rule;
        rule.itemId = fields[0].Get<uint32>();
        rule.wearPosition = fields[1].Get<uint8>();
        rule.slotPosition = fields[2].Get<uint8>();
        rule.requirementId = fields[3].Get<uint32>();
        rule.wearLevel = fields[4].Get<uint32>();
        rule.limitType = fields[5].Get<uint8>();
        rule.comment = fields[6].Get<std::string>();

        if (!rule.itemId || rule.limitType > WEAR_LIMIT_XIANQI)
            continue;

        g_wearRules[rule.itemId].push_back(rule);
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", "→穿戴控制表√ 规则={}", count);
}

uint8 GetExclusiveLimit(uint32 itemId)
{
    auto itr = g_wearRules.find(itemId);
    if (itr == g_wearRules.end())
        return WEAR_LIMIT_NONE;

    for (WearRule const& rule : itr->second)
        if (rule.limitType != WEAR_LIMIT_NONE)
            return rule.limitType;

    return WEAR_LIMIT_NONE;
}

bool HasExclusiveLimit(uint32 itemId)
{
    return GetExclusiveLimit(itemId) != WEAR_LIMIT_NONE;
}

bool IsLimitedTo(uint32 itemId, uint8 limitType)
{
    return GetExclusiveLimit(itemId) == limitType;
}

uint32 GetPlayerWearLevel(Player* player)
{
    if (!player)
        return 0;

    uint32 const playerGuid = player->GetGUID().GetCounter();

    {
        std::lock_guard<std::mutex> guard(g_playerWearLevelsMutex);
        auto itr = g_playerWearLevels.find(playerGuid);
        if (itr != g_playerWearLevels.end())
            return itr->second;
    }

    uint32 const wearLevel = LoadPlayerWearLevelFromDatabase(playerGuid);
    {
        std::lock_guard<std::mutex> guard(g_playerWearLevelsMutex);
        g_playerWearLevels[playerGuid] = wearLevel;
    }

    return wearLevel;
}

bool UnlockPlayerWearLevel(Player* player, uint32 wearLevel, bool notify)
{
    if (wearLevel == 0)
        return true;

    if (!player)
        return false;

    uint32 const currentLevel = GetPlayerWearLevel(player);
    if (currentLevel >= wearLevel)
        return true;

    if (!WearLevelPermissionTableExists())
        return false;

    uint32 const playerGuid = player->GetGUID().GetCounter();
    CharacterDatabase.Execute(
        "INSERT INTO `_穿戴等级权限` (`玩家GUID`, `穿戴等级`) VALUES ({}, {}) "
        "ON DUPLICATE KEY UPDATE `穿戴等级` = GREATEST(`穿戴等级`, VALUES(`穿戴等级`))",
        playerGuid, wearLevel);

    {
        std::lock_guard<std::mutex> guard(g_playerWearLevelsMutex);
        g_playerWearLevels[playerGuid] = std::max(currentLevel, wearLevel);
    }

    if (notify && player->GetSession())
        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[穿戴控制]|r 已解锁穿戴等级 {}。", wearLevel);

    return true;
}

bool CanEquipItem(Player* player, uint32 itemId, uint8 limitType, uint8 slotPosition, std::string* error, bool showRequirementMessages)
{
    uint8 const exclusiveLimit = GetExclusiveLimit(itemId);
    if (exclusiveLimit == WEAR_LIMIT_NONE)
        return true;

    if (exclusiveLimit != limitType)
    {
        if (error)
            *error = std::string("该物品只能穿戴到") + GetLimitName(exclusiveLimit) + "槽位。";
        return false;
    }

    WearRule const* rule = FindRule(itemId, limitType, slotPosition);
    if (!rule)
    {
        if (error)
            *error = std::string("该物品不能穿戴到当前") + GetLimitName(limitType) + "槽位。";
        return false;
    }

    if (rule->requirementId == 0)
    {
        if (rule->wearLevel == 0)
            return true;
    }

    if (rule->wearLevel > 0)
    {
        uint32 const playerWearLevel = GetPlayerWearLevel(player);
        if (playerWearLevel < rule->wearLevel)
        {
            if (error)
                *error = Acore::StringFormat("穿戴等级不足，需要 {} 级，当前 {} 级。", rule->wearLevel, playerWearLevel);
            return false;
        }
    }

    if (rule->requirementId == 0)
        return true;

    RequirementInterface* reqModule = GetRequirementModule();
    if (!reqModule)
    {
        if (error)
            *error = "需求模板系统未加载，无法检查穿戴需求。";
        return false;
    }

    bool const meetsRequirements = reqModule->CheckRequirements(player, rule->requirementId, false);
    if (meetsRequirements)
        return true;

    if (showRequirementMessages)
        reqModule->CheckRequirements(player, rule->requirementId, true);

    if (error)
        *error = "不满足该物品的穿戴需求。";

    return false;
}
}

void AddWearControlScripts()
{
    new WearControlWorldScript();
    new WearControlPlayerScript();
}

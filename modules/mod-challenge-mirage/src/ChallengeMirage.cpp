/*
 * 挑战幻境系统 (mod-challenge-mirage)
 */

#include "ChallengeMirage.h"
#include "AddonThrottle.h"
#include "AllCreatureScript.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"

#include <algorithm>
#include <sstream>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
constexpr char const* CONF_ENABLE = "ChallengeMirage.Enable";
constexpr char const* CONF_DEBUG = "ChallengeMirage.Debug";
constexpr char const* CONF_DEFAULT_LAYER = "ChallengeMirage.DefaultLayer";
constexpr char const* CONF_MAX_LEVEL = "ChallengeMirage.MaxLevel";
constexpr char const* CHALLENGE_MIRAGE_ADDON_PREFIX = "MIRAGEUI";

uint32 GetPlayerGuidLow(Player const* player)
{
    return player ? player->GetGUID().GetCounter() : 0;
}

uint64 BuildCreatureSpawnKey(uint32 mapId, uint32 instanceId, uint32 spawnId, uint32 index)
{
    return (uint64(mapId & 0xFFFF) << 48)
        | (uint64(instanceId & 0xFFFF) << 32)
        | (uint64(spawnId & 0xFFFFFF) << 8)
        | uint64(index & 0xFF);
}

ObjectGuid GetCreatureLinkedPlayerGuid(Creature const* creature)
{
    if (!creature)
        return ObjectGuid::Empty;

    ObjectGuid guid = creature->GetCharmerOrOwnerGUID();
    if (guid.IsPlayer())
        return guid;

    guid = creature->GetCreatorGUID();
    if (guid.IsPlayer())
        return guid;

    if (creature->IsSummon())
    {
        guid = creature->GetSummonerGUID();
        if (guid.IsPlayer())
            return guid;
    }

    return ObjectGuid::Empty;
}

Player const* GetCreatureLinkedPlayer(Creature const* creature)
{
    ObjectGuid guid = GetCreatureLinkedPlayerGuid(creature);
    return guid.IsPlayer() ? ObjectAccessor::FindPlayer(guid) : nullptr;
}

std::string SanitizeAddonField(std::string value)
{
    for (char& ch : value)
    {
        if (ch == ':' || ch == ';' || ch == '\t' || ch == '\n' || ch == '\r')
            ch = ' ';
    }

    return value;
}

bool TryParseUInt(std::string const& value, uint32& out)
{
    try
    {
        size_t parsed = 0;
        unsigned long number = std::stoul(value, &parsed);
        if (parsed != value.length())
            return false;

        out = static_cast<uint32>(number);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void SendChallengeMirageAddonMessage(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    std::string fullMessage = std::string(CHALLENGE_MIRAGE_ADDON_PREFIX) + '\t' + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
}

void SendChallengeMirageStatus(Player* player)
{
    if (!player)
        return;

    std::ostringstream ss;
    ss << "STATUS:"
       << (sChallengeMirageMgr->IsEnabled() ? 1 : 0) << ':'
       << sChallengeMirageMgr->GetPlayerLevel(player) << ':'
       << sChallengeMirageMgr->GetPlayerLayer(player) << ':'
       << sChallengeMirageMgr->GetMaxLevel();

    SendChallengeMirageAddonMessage(player, ss.str());
}

void SendChallengeMirageLevelList(Player* player)
{
    if (!player)
        return;

    SendChallengeMirageAddonMessage(player, "LEVELS_BEGIN");

    // 数据在 LoadTemplates 时已整表缓存（含等级>0 与 <=maxLevel 过滤），直接读内存按等级升序输出
    auto const& templates = sChallengeMirageMgr->GetLevelTemplates();
    std::vector<ChallengeMirageLevelTemplate const*> sorted;
    sorted.reserve(templates.size());
    for (auto const& pair : templates)
        sorted.push_back(&pair.second);

    std::sort(sorted.begin(), sorted.end(),
        [](ChallengeMirageLevelTemplate const* a, ChallengeMirageLevelTemplate const* b) { return a->level < b->level; });

    size_t sent = 0;
    for (ChallengeMirageLevelTemplate const* tpl : sorted)
    {
        if (sent >= 200)
            break;

        std::ostringstream ss;
        ss << "LEVEL:"
           << tpl->level << ':'
           << SanitizeAddonField(tpl->name) << ':'
           << tpl->requiredItem << ':'
           << tpl->requiredCount << ':'
           << tpl->durationSec;

        SendChallengeMirageAddonMessage(player, ss.str());
        ++sent;
    }

    SendChallengeMirageAddonMessage(player, "LEVELS_END");
}
}

ChallengeMirageMgr* ChallengeMirageMgr::instance()
{
    static ChallengeMirageMgr instance;
    return &instance;
}

void ChallengeMirageMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>(CONF_ENABLE, true);
    _debug = sConfigMgr->GetOption<bool>(CONF_DEBUG, false);
    _defaultLayer = sConfigMgr->GetOption<uint32>(CONF_DEFAULT_LAYER, 0);
    _maxLevel = sConfigMgr->GetOption<uint32>(CONF_MAX_LEVEL, 9999999);
}

void ChallengeMirageMgr::LoadTemplates()
{
    _levelTemplates.clear();
    _creatureTemplates.clear();
    _creatureEntryLayers.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `等级`, `名称`, `需求物品`, `需求数量`, `生物组`, `持续秒数` "
        "FROM `_挑战幻境等级`");

    if (!result)
    {
        LOG_WARN("server.loading", "[挑战幻境] 未找到 world.`_挑战幻境等级` 配置，模块入口可用但没有可进入的幻境等级");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ChallengeMirageLevelTemplate tpl;
        tpl.level = fields[0].Get<uint32>();
        tpl.name = fields[1].Get<std::string>();
        tpl.requiredItem = fields[2].Get<uint32>();
        tpl.requiredCount = fields[3].Get<uint32>();
        tpl.creatureSet = fields[4].Get<uint32>();
        tpl.durationSec = fields[5].Get<uint32>();

        if (tpl.level > 0 && tpl.level <= _maxLevel)
            _levelTemplates[tpl.level] = tpl;
    } while (result->NextRow());

    QueryResult creatureResult = WorldDatabase.Query(
        "SELECT `ID`, `生物组`, `生物Entry`, `地图`, `区域`, `坐标X`, `坐标Y`, `坐标Z`, "
        "`朝向O`, `数量`, `刷新秒数`, `游荡距离` "
        "FROM `_挑战幻境生物`");

    if (!creatureResult)
    {
        LOG_WARN("server.loading", "[挑战幻境] 未找到 world.`_挑战幻境生物` 配置，幻境层不会生成额外生物");
        return;
    }

    do
    {
        Field* fields = creatureResult->Fetch();

        ChallengeMirageCreatureTemplate tpl;
        tpl.id = fields[0].Get<uint32>();
        tpl.creatureSet = fields[1].Get<uint32>();
        tpl.entry = fields[2].Get<uint32>();
        tpl.mapId = fields[3].Get<uint32>();
        tpl.areaId = fields[4].Get<uint32>();
        tpl.x = fields[5].Get<float>();
        tpl.y = fields[6].Get<float>();
        tpl.z = fields[7].Get<float>();
        tpl.o = fields[8].Get<float>();
        tpl.count = fields[9].Get<uint32>();
        tpl.respawnSec = fields[10].Get<uint32>();
        tpl.wanderDistance = fields[11].Get<float>();

        if (!tpl.creatureSet || !tpl.entry || !tpl.count)
            continue;

        _creatureTemplates.push_back(tpl);
        _creatureEntryLayers.emplace(tpl.entry, tpl.creatureSet);
    } while (creatureResult->NextRow());
}

void ChallengeMirageMgr::Reload()
{
    LoadConfig();
    LoadTemplates();
}

ChallengeMirageLevelTemplate const* ChallengeMirageMgr::GetLevelTemplate(uint32 level) const
{
    auto itr = _levelTemplates.find(level);
    return itr != _levelTemplates.end() ? &itr->second : nullptr;
}

void ChallengeMirageMgr::LoadPlayer(Player* player)
{
    if (!player)
        return;

    uint32 guid = GetPlayerGuidLow(player);
    uint32 level = _defaultLayer;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `当前幻境等级` FROM `_挑战幻境玩家数据` WHERE `玩家GUID` = {}", guid);

    if (result)
        level = result->Fetch()[0].Get<uint32>();
    else
        CharacterDatabase.Execute(
            "INSERT INTO `_挑战幻境玩家数据` (`玩家GUID`, `当前幻境等级`, `当前生物层`) VALUES ({}, {}, {})",
            guid, _defaultLayer, _defaultLayer);

    if (level > _maxLevel)
        level = _defaultLayer;

    _playerLevels[guid] = level;
}

void ChallengeMirageMgr::SavePlayer(Player* player)
{
    if (!player)
        return;

    uint32 guid = GetPlayerGuidLow(player);
    uint32 level = GetPlayerLevel(player);
    CharacterDatabase.Execute(
        "INSERT INTO `_挑战幻境玩家数据` "
        "(`玩家GUID`, `当前幻境等级`, `当前生物层`, `更新时间`) "
        "VALUES ({}, {}, {}, UNIX_TIMESTAMP()) "
        "ON DUPLICATE KEY UPDATE `当前幻境等级` = VALUES(`当前幻境等级`), "
        "`当前生物层` = VALUES(`当前生物层`), `更新时间` = VALUES(`更新时间`)",
        guid, level, level);
}

void ChallengeMirageMgr::RemovePlayer(Player* player)
{
    if (!player)
        return;

    uint32 guid = GetPlayerGuidLow(player);
    _playerLevels.erase(guid);
    _playerSpawnTimers.erase(guid);
}

void ChallengeMirageMgr::UpdatePlayer(Player* player, uint32 diff)
{
    if (!_enabled || !player || GetPlayerLevel(player) == _defaultLayer)
        return;

    uint32 guid = GetPlayerGuidLow(player);
    uint32& timer = _playerSpawnTimers[guid];
    timer += diff;

    if (timer < 3000)
        return;

    timer = 0;
    SpawnCreaturesForPlayer(player);
}

uint32 ChallengeMirageMgr::GetPlayerLayer(Player const* player) const
{
    return GetPlayerLevel(player);
}

uint32 ChallengeMirageMgr::GetPlayerLevel(Player const* player) const
{
    uint32 guid = GetPlayerGuidLow(player);
    auto itr = _playerLevels.find(guid);
    return itr != _playerLevels.end() ? itr->second : _defaultLayer;
}

bool ChallengeMirageMgr::SetPlayerLevel(Player* player, uint32 level, bool saveNow)
{
    if (!player || level > _maxLevel)
        return false;

    _playerLevels[GetPlayerGuidLow(player)] = level;

    if (saveNow)
        SavePlayer(player);

    SpawnCreaturesForPlayer(player);
    player->UpdateObjectVisibility(true);
    return true;
}

bool ChallengeMirageMgr::Enter(Player* player, uint32 level, std::string& reason)
{
    if (!_enabled)
    {
        reason = "挑战幻境系统已禁用";
        return false;
    }

    if (!player)
    {
        reason = "找不到玩家";
        return false;
    }

    if (level == 0 || level > _maxLevel)
    {
        reason = "幻境等级不合法";
        return false;
    }

    ChallengeMirageLevelTemplate const* tpl = GetLevelTemplate(level);
    if (!tpl)
    {
        reason = "该幻境等级没有配置";
        return false;
    }

    if (tpl->requiredItem && tpl->requiredCount && !player->HasItemCount(tpl->requiredItem, tpl->requiredCount))
    {
        std::ostringstream ss;
        ss << "进入该幻境需要物品 " << tpl->requiredItem << " x" << tpl->requiredCount;
        reason = ss.str();
        return false;
    }

    SetPlayerLevel(player, level, true);

    CharacterDatabase.Execute(
        "UPDATE `_挑战幻境玩家数据` SET `累计进入次数` = `累计进入次数` + 1, "
        "`最后进入时间` = UNIX_TIMESTAMP() WHERE `玩家GUID` = {}",
        GetPlayerGuidLow(player));

    return true;
}

bool ChallengeMirageMgr::Leave(Player* player)
{
    if (!player)
        return false;

    return SetPlayerLevel(player, _defaultLayer, true);
}

void ChallengeMirageMgr::SetCreatureLayer(Creature* creature, uint32 layer)
{
    if (!creature)
        return;

    _creatureLayers[creature->GetGUID()] = layer;
}

uint32 ChallengeMirageMgr::GetCreatureLayer(Creature const* creature) const
{
    if (!creature)
        return _defaultLayer;

    auto itr = _creatureLayers.find(creature->GetGUID());
    if (itr != _creatureLayers.end())
        return itr->second;

    if (Player const* linkedPlayer = GetCreatureLinkedPlayer(creature))
        return GetPlayerLayer(linkedPlayer);

    auto entryItr = _creatureEntryLayers.find(creature->GetEntry());
    return entryItr != _creatureEntryLayers.end() ? entryItr->second : _defaultLayer;
}

bool ChallengeMirageMgr::IsCreatureVisibleForPlayer(Creature const* creature, Player const* player) const
{
    if (!_enabled || !creature || !player)
        return true;

    if (GetCreatureLinkedPlayer(creature))
        return GetCreatureLayer(creature) == GetPlayerLayer(player);

    if (creature->IsPet() || creature->IsControlledByPlayer())
        return true;

    return GetCreatureLayer(creature) == GetPlayerLayer(player);
}

void ChallengeMirageMgr::RemoveCreature(Creature const* creature)
{
    if (!creature)
        return;

    _creatureLayers.erase(creature->GetGUID());

    auto spawnItr = _spawnKeysByCreature.find(creature->GetGUID());
    if (spawnItr != _spawnKeysByCreature.end())
    {
        _activeSpawnKeys.erase(spawnItr->second);
        _spawnKeysByCreature.erase(spawnItr);
    }
}

void ChallengeMirageMgr::SpawnCreaturesForPlayer(Player* player)
{
    if (!_enabled || !player)
        return;

    uint32 level = GetPlayerLevel(player);
    if (level == _defaultLayer)
        return;

    ChallengeMirageLevelTemplate const* levelTemplate = GetLevelTemplate(level);
    if (!levelTemplate)
        return;

    uint32 creatureSet = levelTemplate->creatureSet ? levelTemplate->creatureSet : level;
    uint32 mapId = player->GetMapId();
    uint32 instanceId = player->GetInstanceId();

    for (ChallengeMirageCreatureTemplate const& creatureTemplate : _creatureTemplates)
    {
        if (creatureTemplate.creatureSet != creatureSet || creatureTemplate.mapId != mapId)
            continue;

        for (uint32 i = 0; i < creatureTemplate.count; ++i)
        {
            uint64 spawnKey = BuildCreatureSpawnKey(mapId, instanceId, creatureTemplate.id, i);
            if (!_activeSpawnKeys.insert(spawnKey).second)
                continue;

            float offset = static_cast<float>(i) * 1.5f;
            // 【孤儿怪修复】原 TEMPSUMMON_CORPSE_TIMED_DESPAWN 只对尸体计时，存活召唤怪
            // 永不消失——玩家离开/换层后活怪与 _creatureLayers 等状态无限累积。
            // 改脱战存活计时：战斗中不消失；玩家离开后 respawnSec 秒自动 despawn
            //（OnCreatureRemoveWorld 清理状态并释放 spawnKey，玩家在层内则 3 秒 tick 重新召唤）
            Creature* creature = player->SummonCreature(
                creatureTemplate.entry,
                creatureTemplate.x + offset,
                creatureTemplate.y + offset,
                creatureTemplate.z,
                creatureTemplate.o,
                TEMPSUMMON_TIMED_DESPAWN_OOC_ALIVE,
                creatureTemplate.respawnSec * 1000U);

            if (!creature)
            {
                _activeSpawnKeys.erase(spawnKey);
                continue;
            }

            SetCreatureLayer(creature, level);
            _spawnKeysByCreature[creature->GetGUID()] = spawnKey;
            creature->SetHomePosition(creatureTemplate.x, creatureTemplate.y, creatureTemplate.z, creatureTemplate.o);
            creature->UpdateObjectVisibility(true);
        }
    }
}

class ChallengeMirageWorldScript : public WorldScript
{
public:
    ChallengeMirageWorldScript() : WorldScript("ChallengeMirageWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sChallengeMirageMgr->LoadConfig();

        if (reload)
            sChallengeMirageMgr->LoadTemplates();
    }

    void OnStartup() override
    {
        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (_initialized)
            return;

        _loadTime += diff;

        if (_loadTime < 3000)
            return;

        sChallengeMirageMgr->Reload();

        if (sChallengeMirageMgr->IsEnabled() && !_displayedLog)
        {
            LOG_INFO("server.loading", "→挑战幻境系统√");
            _displayedLog = true;
        }

        _initialized = true;
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class ChallengeMiragePlayerScript : public PlayerScript
{
public:
    ChallengeMiragePlayerScript() : PlayerScript("ChallengeMiragePlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        sChallengeMirageMgr->LoadPlayer(player);
        sChallengeMirageMgr->SpawnCreaturesForPlayer(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        sChallengeMirageMgr->SavePlayer(player);
        sChallengeMirageMgr->RemovePlayer(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sChallengeMirageMgr->UpdatePlayer(player, diff);
    }

    void OnPlayerMapChanged(Player* player) override
    {
        sChallengeMirageMgr->SpawnCreaturesForPlayer(player);
        player->UpdateObjectVisibility(true);
    }

    void OnPlayerUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override
    {
        sChallengeMirageMgr->SpawnCreaturesForPlayer(player);
        player->UpdateObjectVisibility(true);
    }

    bool OnPlayerCanSeeCreature(Player const* player, Creature const* creature) override
    {
        return sChallengeMirageMgr->IsCreatureVisibleForPlayer(creature, player);
    }
};

class ChallengeMirageCreatureScript : public AllCreatureScript
{
public:
    ChallengeMirageCreatureScript() : AllCreatureScript("ChallengeMirageCreatureScript") { }

    void OnCreatureRemoveWorld(Creature* creature) override
    {
        sChallengeMirageMgr->RemoveCreature(creature);
    }
};

class ChallengeMirageAddonScript : public PlayerScript
{
public:
    ChallengeMirageAddonScript() : PlayerScript("ChallengeMirageAddonScript") { }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != CHALLENGE_MIRAGE_ADDON_PREFIX)
            return;

        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "MIRAGE"))
            return;

        std::string command = msg.substr(tabPos + 1);

        if (command == "STATUS")
        {
            SendChallengeMirageStatus(player);
            return;
        }

        if (command == "LIST")
        {
            SendChallengeMirageLevelList(player);
            return;
        }

        if (command == "OPEN")
        {
            SendChallengeMirageAddonMessage(player, "OPEN_UI");
            SendChallengeMirageStatus(player);
            SendChallengeMirageLevelList(player);
            return;
        }

        if (command == "LEAVE")
        {
            sChallengeMirageMgr->Leave(player);
            SendChallengeMirageAddonMessage(player, "LEAVE_OK");
            SendChallengeMirageStatus(player);
            return;
        }

        if (command.compare(0, 6, "ENTER:") == 0)
        {
            uint32 level = 0;
            if (!TryParseUInt(command.substr(6), level))
            {
                SendChallengeMirageAddonMessage(player, "ENTER_FAIL:0:等级参数错误");
                return;
            }

            std::string reason;
            if (!sChallengeMirageMgr->Enter(player, level, reason))
            {
                std::ostringstream fail;
                fail << "ENTER_FAIL:" << level << ':' << SanitizeAddonField(reason);
                SendChallengeMirageAddonMessage(player, fail.str());
                SendChallengeMirageStatus(player);
                return;
            }

            std::ostringstream ok;
            ok << "ENTER_OK:" << level;
            SendChallengeMirageAddonMessage(player, ok.str());
            SendChallengeMirageStatus(player);
            return;
        }
    }
};

class ChallengeMirageCommandScript : public CommandScript
{
public:
    ChallengeMirageCommandScript() : CommandScript("ChallengeMirageCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable challengeMirageCommandTable =
        {
            { "开启",   HandleStartCommand,  SEC_PLAYER,        Console::No  },
            { "start",  HandleStartCommand,  SEC_PLAYER,        Console::No  },
            { "等级",   HandleLevelCommand,  SEC_PLAYER,        Console::No  },
            { "level",  HandleLevelCommand,  SEC_PLAYER,        Console::No  },
            { "离开",   HandleLeaveCommand,  SEC_PLAYER,        Console::No  },
            { "leave",  HandleLeaveCommand,  SEC_PLAYER,        Console::No  },
            { "状态",   HandleStatusCommand, SEC_PLAYER,        Console::No  },
            { "status", HandleStatusCommand, SEC_PLAYER,        Console::No  },
            { "界面",   HandleOpenUICommand, SEC_PLAYER,        Console::No  },
            { "ui",     HandleOpenUICommand, SEC_PLAYER,        Console::No  },
            { "重载",   HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "reload", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "挑战幻境", challengeMirageCommandTable },
            { "mirage",   challengeMirageCommandTable },
        };

        return commandTable;
    }

private:
    static bool HandleStartCommand(ChatHandler* handler, Optional<uint32> level)
    {
        return ChangeLevel(handler, level, ".挑战幻境 开启 <等级>");
    }

    static bool HandleLevelCommand(ChatHandler* handler, Optional<uint32> level)
    {
        return ChangeLevel(handler, level, ".挑战幻境 等级 <等级>");
    }

    static bool ChangeLevel(ChatHandler* handler, Optional<uint32> level, char const* usage)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("该命令只能由玩家使用");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!level)
        {
            handler->PSendSysMessage("用法: {}", usage);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (*level == 0)
        {
            sChallengeMirageMgr->Leave(player);
            handler->SendSysMessage("已切换到原世界。");
            return true;
        }

        std::string reason;
        if (!sChallengeMirageMgr->Enter(player, *level, reason))
        {
            handler->PSendSysMessage("进入挑战幻境失败: {}", reason);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("已进入挑战幻境 {}。玩家仍在原世界，只会看到同层幻境生物。", *level);
        return true;
    }

    static bool HandleLeaveCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("该命令只能由玩家使用");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sChallengeMirageMgr->Leave(player);
        handler->SendSysMessage("已离开挑战幻境，恢复原世界生物层。");
        return true;
    }

    static bool HandleStatusCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("该命令只能由玩家使用");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 level = sChallengeMirageMgr->GetPlayerLevel(player);
        handler->PSendSysMessage("当前挑战幻境等级: {}", level);
        handler->PSendSysMessage("当前生物层: {}", sChallengeMirageMgr->GetPlayerLayer(player));
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("该命令只能由玩家使用");
            handler->SetSentErrorMessage(true);
            return false;
        }

        SendChallengeMirageAddonMessage(player, "OPEN_UI");
        SendChallengeMirageStatus(player);
        SendChallengeMirageLevelList(player);
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler)
    {
        sChallengeMirageMgr->Reload();
        handler->SendSysMessage("挑战幻境系统配置已重载。");
        return true;
    }
};

void AddSC_ChallengeMirage()
{
    new ChallengeMirageWorldScript();
    new ChallengeMiragePlayerScript();
    new ChallengeMirageCreatureScript();
    new ChallengeMirageAddonScript();
    new ChallengeMirageCommandScript();
}

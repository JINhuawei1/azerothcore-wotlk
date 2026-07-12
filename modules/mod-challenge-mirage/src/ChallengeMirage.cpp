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
#include "HermesBridgeAddonApi.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"

#include <algorithm>
#include <mutex>
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

std::string BuildCreatureSpawnKey(uint32 mapId, uint32 instanceId, uint32 spawnId, uint32 index)
{
    return std::to_string(mapId) + ':' + std::to_string(instanceId) + ':' + std::to_string(spawnId) + ':' + std::to_string(index);
}

char const* BoolText(bool value)
{
    return value ? "true" : "false";
}

uint32 GetMapInstanceId(Map const* map)
{
    return map ? map->GetInstanceId() : 0;
}

uint32 GetCreatureInstanceId(Creature const* creature)
{
    if (!creature)
        return 0;

    if (Map const* map = creature->FindMap())
        return map->GetInstanceId();

    return creature->GetInstanceId();
}

std::string BuildCreatureRuntimeKey(Creature const* creature)
{
    if (!creature)
        return "";

    return std::to_string(creature->GetMapId()) + ':' + std::to_string(GetCreatureInstanceId(creature)) + ':' +
        std::to_string(creature->GetGUID().GetRawValue());
}

bool IsChallengeMirageDebugEntry(uint32 entry)
{
    return entry >= 800000 && entry <= 800099;
}

char const* GetCreatureSpawnReleaseReason(Creature const* creature)
{
    if (!creature)
        return "missing_creature";

    TempSummon const* summon = creature->ToTempSummon();
    if (!summon)
        return "non_temp_summon";

    if (summon->GetSummonType() == TEMPSUMMON_DESPAWNED)
        return "despawned";

    Map const* map = creature->FindMap();
    if (!map || (map->IsDungeon() && !map->HavePlayers()))
        return "empty_instance";

    return nullptr;
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

    if (HermesBridge_SendAddonMessage(player, CHALLENGE_MIRAGE_ADDON_PREFIX, payload))
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

    {
        std::unique_lock<std::shared_mutex> lock(_stateMutex);
        _playerLevels[guid] = level;
    }
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
    std::unique_lock<std::shared_mutex> lock(_stateMutex);
    _playerLevels.erase(guid);
    _playerSpawnTimers.erase(guid);
    _locationTeleportStates.erase(guid);
}

void ChallengeMirageMgr::UpdatePlayer(Player* player, uint32 diff)
{
    if (!_enabled || !player || GetPlayerLevel(player) == _defaultLayer)
        return;

    uint32 guid = GetPlayerGuidLow(player);
    bool shouldSpawn = false;
    {
        std::unique_lock<std::shared_mutex> lock(_stateMutex);
        uint32& timer = _playerSpawnTimers[guid];
        timer += diff;

        if (timer < 3000)
            return;

        timer = 0;
        shouldSpawn = true;
    }

    if (shouldSpawn)
        SpawnCreaturesForPlayer(player, "UpdatePlayer");
}

uint32 ChallengeMirageMgr::GetPlayerLayer(Player const* player) const
{
    return GetPlayerLevel(player);
}

uint32 ChallengeMirageMgr::GetPlayerLevel(Player const* player) const
{
    uint32 guid = GetPlayerGuidLow(player);
    std::shared_lock<std::shared_mutex> lock(_stateMutex);
    auto itr = _playerLevels.find(guid);
    return itr != _playerLevels.end() ? itr->second : _defaultLayer;
}

bool ChallengeMirageMgr::SetPlayerLevel(Player* player, uint32 level, bool saveNow)
{
    if (!player || level > _maxLevel)
        return false;

    uint32 oldLevel = GetPlayerLevel(player);
    Map* map = player->GetMap();
    LOG_DEBUG("module.challenge_mirage",
        "[ChallengeMirageDebug] SET_LEVEL player={} guid={} oldLevel={} newLevel={} saveNow={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
        player->GetName(), GetPlayerGuidLow(player), oldLevel, level, BoolText(saveNow), player->GetMapId(), player->GetInstanceId(),
        GetMapInstanceId(map), BoolText(map && map->IsDungeon()), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    {
        std::unique_lock<std::shared_mutex> lock(_stateMutex);
        _playerLevels[GetPlayerGuidLow(player)] = level;
    }

    if (saveNow)
        SavePlayer(player);

    SpawnCreaturesForPlayer(player, "SetPlayerLevel");
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

    Map* map = player->GetMap();
    LOG_DEBUG("module.challenge_mirage",
        "[ChallengeMirageDebug] ENTER_REQUEST player={} guid={} level={} creatureSet={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
        player->GetName(), GetPlayerGuidLow(player), level, tpl->creatureSet, player->GetMapId(), player->GetInstanceId(),
        GetMapInstanceId(map), BoolText(map && map->IsDungeon()), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    SetPlayerLevel(player, level, true);

    CharacterDatabase.Execute(
        "UPDATE `_挑战幻境玩家数据` SET `累计进入次数` = `累计进入次数` + 1, "
        "`最后进入时间` = UNIX_TIMESTAMP() WHERE `玩家GUID` = {}",
        GetPlayerGuidLow(player));

    map = player->GetMap();
    LOG_DEBUG("module.challenge_mirage",
        "[ChallengeMirageDebug] ENTER_OK player={} guid={} level={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
        player->GetName(), GetPlayerGuidLow(player), level, player->GetMapId(), player->GetInstanceId(), GetMapInstanceId(map),
        BoolText(map && map->IsDungeon()), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    return true;
}

bool ChallengeMirageMgr::EnterFromLocationTeleport(Player* player, uint32 level, uint32 targetMapId, std::string& reason)
{
    if (!Enter(player, level, reason))
        return false;

    {
        std::unique_lock<std::shared_mutex> lock(_stateMutex);
        _locationTeleportStates[GetPlayerGuidLow(player)] = { targetMapId, false };
    }
    Map* map = player->GetMap();
    LOG_DEBUG("module.challenge_mirage",
        "[ChallengeMirageDebug] LOCATION_TELEPORT_SET player={} guid={} level={} targetMap={} currentMap={} playerInst={} mapInst={} inDungeon={}",
        player->GetName(), GetPlayerGuidLow(player), level, targetMapId, player->GetMapId(), player->GetInstanceId(),
        GetMapInstanceId(map), BoolText(map && map->IsDungeon()));
    return true;
}

bool ChallengeMirageMgr::Leave(Player* player)
{
    if (!player)
        return false;

    Map* map = player->GetMap();
    LOG_DEBUG("module.challenge_mirage",
        "[ChallengeMirageDebug] LEAVE player={} guid={} oldLevel={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
        player->GetName(), GetPlayerGuidLow(player), GetPlayerLevel(player), player->GetMapId(), player->GetInstanceId(),
        GetMapInstanceId(map), BoolText(map && map->IsDungeon()), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    {
        std::unique_lock<std::shared_mutex> lock(_stateMutex);
        _locationTeleportStates.erase(GetPlayerGuidLow(player));
    }
    return SetPlayerLevel(player, _defaultLayer, true);
}

bool ChallengeMirageMgr::IsLocationTeleportActive(Player const* player) const
{
    if (!player)
        return false;

    std::shared_lock<std::shared_mutex> lock(_stateMutex);
    return _locationTeleportStates.find(GetPlayerGuidLow(player)) != _locationTeleportStates.end();
}

void ChallengeMirageMgr::UpdateLocationTeleportState(Player* player)
{
    if (!player)
        return;

    uint32 guid = GetPlayerGuidLow(player);
    LocationTeleportState* state = nullptr;
    {
        // unordered_map 节点地址稳定；该条目只会被本玩家所在线程读写，
        // 锁只需保护 find 遍历不与其他玩家条目的结构性修改竞争
        std::shared_lock<std::shared_mutex> lock(_stateMutex);
        auto itr = _locationTeleportStates.find(guid);
        if (itr == _locationTeleportStates.end())
            return;
        state = &itr->second;
    }

    Map* map = player->GetMap();
    bool inDungeon = map && map->IsDungeon();

    if (!state->reachedDungeon)
    {
        if (inDungeon && player->GetMapId() == state->targetMapId)
        {
            state->reachedDungeon = true;
            LOG_DEBUG("module.challenge_mirage",
                "[ChallengeMirageDebug] LOCATION_REACHED player={} guid={} targetMap={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
                player->GetName(), guid, state->targetMapId, player->GetMapId(), player->GetInstanceId(), GetMapInstanceId(map),
                BoolText(inDungeon), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
        }

        return;
    }

    if (!inDungeon || player->GetMapId() != state->targetMapId)
    {
        LOG_DEBUG("module.challenge_mirage",
            "[ChallengeMirageDebug] LOCATION_LEAVE_TRIGGER player={} guid={} targetMap={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
            player->GetName(), guid, state->targetMapId, player->GetMapId(), player->GetInstanceId(), GetMapInstanceId(map),
            BoolText(inDungeon), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
        Leave(player);
    }
}

void ChallengeMirageMgr::SetCreatureLayer(Creature* creature, uint32 layer)
{
    if (!creature)
        return;

    std::unique_lock<std::shared_mutex> lock(_stateMutex);
    _creatureLayers[BuildCreatureRuntimeKey(creature)] = layer;
}

uint32 ChallengeMirageMgr::GetCreatureLayer(Creature const* creature) const
{
    if (!creature)
        return _defaultLayer;

    {
        std::shared_lock<std::shared_mutex> lock(_stateMutex);
        auto itr = _creatureLayers.find(BuildCreatureRuntimeKey(creature));
        if (itr != _creatureLayers.end())
            return itr->second;
    }

    if (Player const* linkedPlayer = GetCreatureLinkedPlayer(creature))
        return GetPlayerLayer(linkedPlayer);

    auto entryItr = _creatureEntryLayers.find(creature->GetEntry());
    return entryItr != _creatureEntryLayers.end() ? entryItr->second : _defaultLayer;
}

bool ChallengeMirageMgr::IsCreatureVisibleForPlayer(Creature const* creature, Player const* player) const
{
    if (!_enabled || !creature || !player)
        return true;

    if (creature->IsPet() || creature->IsControlledByPlayer())
        return true;

    if (GetCreatureLinkedPlayer(creature))
        return GetCreatureLayer(creature) == GetPlayerLayer(player);

    return GetCreatureLayer(creature) == GetPlayerLayer(player);
}

void ChallengeMirageMgr::RemoveCreature(Creature const* creature)
{
    if (!creature)
        return;

    std::string runtimeKey = BuildCreatureRuntimeKey(creature);
    std::unique_lock<std::shared_mutex> lock(_stateMutex);
    auto spawnItr = _spawnKeysByCreature.find(runtimeKey);
    bool trackedSpawn = spawnItr != _spawnKeysByCreature.end();
    bool debugEntry = IsChallengeMirageDebugEntry(creature->GetEntry());
    std::string spawnKey = trackedSpawn ? spawnItr->second : "";
    char const* releaseReason = GetCreatureSpawnReleaseReason(creature);

    // RemoveWorld also happens on grid/object unload. Only release dynamic spawn state
    // on real despawn or when an instance has no players left. While players are
    // still in the instance, keep the key so grid unload cannot duplicate spawns.
    if (!releaseReason)
    {
        if (trackedSpawn || debugEntry)
        {
            TempSummon const* summon = creature->ToTempSummon();
            Map const* map = creature->FindMap();
            LOG_DEBUG("module.challenge_mirage",
                "[ChallengeMirageDebug] REMOVE_IGNORE entry={} guid={} rawGuid={} runtimeKey={} key={} summonType={} map={} inst={} mapPlayers={} health={} maxHealth={} activeKeys={}",
                creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), runtimeKey, spawnKey,
                summon ? uint32(summon->GetSummonType()) : 0, creature->GetMapId(), GetCreatureInstanceId(creature),
                map ? map->GetPlayers().getSize() : 0, creature->GetHealth(), creature->GetMaxHealth(), _activeSpawnKeys.size());
        }
        return;
    }

    _creatureLayers.erase(runtimeKey);

    if (spawnItr != _spawnKeysByCreature.end())
    {
        Map const* map = creature->FindMap();
        LOG_DEBUG("module.challenge_mirage",
            "[ChallengeMirageDebug] REMOVE_RELEASE entry={} guid={} rawGuid={} runtimeKey={} key={} reason={} map={} inst={} mapPlayers={} health={} maxHealth={} activeKeysBefore={}",
            creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), runtimeKey, spawnItr->second,
            releaseReason, creature->GetMapId(), GetCreatureInstanceId(creature), map ? map->GetPlayers().getSize() : 0,
            creature->GetHealth(), creature->GetMaxHealth(), _activeSpawnKeys.size());
        _activeSpawnKeys.erase(spawnItr->second);
        _spawnKeysByCreature.erase(spawnItr);
    }
    else if (debugEntry)
    {
        Map const* map = creature->FindMap();
        LOG_DEBUG("module.challenge_mirage",
            "[ChallengeMirageDebug] REMOVE_NO_KEY entry={} guid={} rawGuid={} runtimeKey={} reason={} map={} inst={} mapPlayers={} health={} maxHealth={} activeKeys={}",
            creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), runtimeKey, releaseReason,
            creature->GetMapId(), GetCreatureInstanceId(creature), map ? map->GetPlayers().getSize() : 0,
            creature->GetHealth(), creature->GetMaxHealth(), _activeSpawnKeys.size());
    }
}

void ChallengeMirageMgr::SpawnCreaturesForPlayer(Player* player, char const* reason)
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
    Map* map = player->GetMap();
    uint32 instanceId = map ? map->GetInstanceId() : player->GetInstanceId();
    bool inDungeon = map && map->IsDungeon();
    uint32 candidateCount = 0;
    uint32 skippedExisting = 0;
    uint32 summonAttempts = 0;
    uint32 summonOk = 0;
    uint32 summonFailed = 0;

    for (ChallengeMirageCreatureTemplate const& creatureTemplate : _creatureTemplates)
    {
        if (creatureTemplate.creatureSet != creatureSet || creatureTemplate.mapId != mapId)
            continue;

        for (uint32 i = 0; i < creatureTemplate.count; ++i)
        {
            ++candidateCount;
            std::string spawnKey = BuildCreatureSpawnKey(mapId, instanceId, creatureTemplate.id, i);
            bool insertedKey = false;
            {
                std::unique_lock<std::shared_mutex> lock(_stateMutex);
                insertedKey = _activeSpawnKeys.insert(spawnKey).second;
            }
            if (!insertedKey)
            {
                ++skippedExisting;
                continue;
            }

            float offset = static_cast<float>(i) * 1.5f;
            TempSummonType summonType = inDungeon ? TEMPSUMMON_MANUAL_DESPAWN : TEMPSUMMON_TIMED_DESPAWN_OOC_ALIVE;
            uint32 despawnTime = inDungeon ? 0 : creatureTemplate.respawnSec * 1000U;
            ++summonAttempts;

            LOG_DEBUG("module.challenge_mirage",
                "[ChallengeMirageDebug] SPAWN_ATTEMPT reason={} player={} guid={} level={} set={} key={} templateId={} entry={} index={} map={} playerInst={} mapInst={} inDungeon={} summonType={} despawnMs={} pos={:.2f},{:.2f},{:.2f}",
                reason ? reason : "unknown", player->GetName(), GetPlayerGuidLow(player), level, creatureSet, spawnKey, creatureTemplate.id,
                creatureTemplate.entry, i, mapId, player->GetInstanceId(), instanceId, BoolText(inDungeon), uint32(summonType), despawnTime,
                creatureTemplate.x + offset, creatureTemplate.y + offset, creatureTemplate.z);

            Position spawnPos(creatureTemplate.x + offset, creatureTemplate.y + offset, creatureTemplate.z, creatureTemplate.o);
            TempSummon* creature = map ? map->SummonCreature(creatureTemplate.entry, spawnPos, nullptr, despawnTime) : nullptr;
            if (creature)
            {
                creature->SetTempSummonType(summonType);
                creature->SetRegeneratingHealth(false);
            }

            if (!creature)
            {
                size_t activeKeys = 0;
                {
                    std::unique_lock<std::shared_mutex> lock(_stateMutex);
                    _activeSpawnKeys.erase(spawnKey);
                    activeKeys = _activeSpawnKeys.size();
                }
                ++summonFailed;
                LOG_DEBUG("module.challenge_mirage",
                    "[ChallengeMirageDebug] SUMMON_FAIL reason={} player={} guid={} level={} key={} entry={} map={} playerInst={} mapInst={} activeKeys={}",
                    reason ? reason : "unknown", player->GetName(), GetPlayerGuidLow(player), level, spawnKey, creatureTemplate.entry,
                    mapId, player->GetInstanceId(), instanceId, activeKeys);
                continue;
            }

            SetCreatureLayer(creature, level);
            std::string runtimeKey = BuildCreatureRuntimeKey(creature);
            size_t activeKeysAfterSummon = 0;
            {
                std::unique_lock<std::shared_mutex> lock(_stateMutex);
                _spawnKeysByCreature[runtimeKey] = spawnKey;
                activeKeysAfterSummon = _activeSpawnKeys.size();
            }
            creature->SetHomePosition(creatureTemplate.x, creatureTemplate.y, creatureTemplate.z, creatureTemplate.o);
            creature->UpdateObjectVisibility(true);
            ++summonOk;
            LOG_DEBUG("module.challenge_mirage",
                "[ChallengeMirageDebug] SUMMON_OK reason={} player={} guid={} level={} key={} entry={} creatureGuid={} rawGuid={} runtimeKey={} creatureMap={} creatureInst={} layer={} canThreat={} summonType={} summonerGuid={} health={} maxHealth={} activeKeys={}",
                reason ? reason : "unknown", player->GetName(), GetPlayerGuidLow(player), level, spawnKey, creature->GetEntry(),
                creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), runtimeKey, creature->GetMapId(), GetCreatureInstanceId(creature),
                GetCreatureLayer(creature), BoolText(creature->CanHaveThreatList()), uint32(creature->GetSummonType()),
                creature->GetSummonerGUID().GetCounter(), creature->GetHealth(), creature->GetMaxHealth(), activeKeysAfterSummon);
        }
    }

    size_t activeKeysTotal = 0;
    {
        std::shared_lock<std::shared_mutex> lock(_stateMutex);
        activeKeysTotal = _activeSpawnKeys.size();
    }
    LOG_DEBUG("module.challenge_mirage",
        "[ChallengeMirageDebug] SPAWN_SUMMARY reason={} player={} guid={} level={} set={} map={} playerInst={} mapInst={} inDungeon={} candidates={} attempts={} ok={} skippedExisting={} failed={} activeKeys={} pos={:.2f},{:.2f},{:.2f}",
        reason ? reason : "unknown", player->GetName(), GetPlayerGuidLow(player), level, creatureSet, mapId, player->GetInstanceId(),
        instanceId, BoolText(inDungeon), candidateCount, summonAttempts, summonOk, skippedExisting, summonFailed, activeKeysTotal,
        player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
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
        sChallengeMirageMgr->SpawnCreaturesForPlayer(player, "Login");
    }

    void OnPlayerLogout(Player* player) override
    {
        if (sChallengeMirageMgr->IsLocationTeleportActive(player))
            sChallengeMirageMgr->Leave(player);

        sChallengeMirageMgr->SavePlayer(player);
        sChallengeMirageMgr->RemovePlayer(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sChallengeMirageMgr->UpdatePlayer(player, diff);
        sChallengeMirageMgr->UpdateLocationTeleportState(player);
    }

    void OnPlayerMapChanged(Player* player) override
    {
        Map* map = player ? player->GetMap() : nullptr;
        if (player)
        {
            LOG_DEBUG("module.challenge_mirage",
                "[ChallengeMirageDebug] HOOK_MAP_CHANGED player={} guid={} level={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
                player->GetName(), GetPlayerGuidLow(player), sChallengeMirageMgr->GetPlayerLevel(player), player->GetMapId(),
                player->GetInstanceId(), GetMapInstanceId(map), BoolText(map && map->IsDungeon()), player->GetPositionX(),
                player->GetPositionY(), player->GetPositionZ());
        }

        sChallengeMirageMgr->UpdateLocationTeleportState(player);
        sChallengeMirageMgr->SpawnCreaturesForPlayer(player, "MapChanged");
        player->UpdateObjectVisibility(true);
    }

    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        Map* map = player ? player->GetMap() : nullptr;
        if (player)
        {
            LOG_DEBUG("module.challenge_mirage",
                "[ChallengeMirageDebug] HOOK_UPDATE_ZONE player={} guid={} level={} newZone={} newArea={} map={} playerInst={} mapInst={} inDungeon={} pos={:.2f},{:.2f},{:.2f}",
                player->GetName(), GetPlayerGuidLow(player), sChallengeMirageMgr->GetPlayerLevel(player), newZone, newArea,
                player->GetMapId(), player->GetInstanceId(), GetMapInstanceId(map), BoolText(map && map->IsDungeon()),
                player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
        }

        sChallengeMirageMgr->SpawnCreaturesForPlayer(player, "UpdateZone");
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

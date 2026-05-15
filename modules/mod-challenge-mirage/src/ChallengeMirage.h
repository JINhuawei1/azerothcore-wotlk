/*
 * 挑战幻境系统 (mod-challenge-mirage)
 *
 * 目标：玩家仍在同一世界互相可见，只按玩家当前幻境层隔离生物可见性、攻击与AI。
 * 模块提供数据、命令入口、动态刷怪与生物层管理；核心可见性/攻击判断通过脚本 hook 接入这些 API。
 */

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;
class Player;

struct ChallengeMirageLevelTemplate
{
    uint32 level = 0;
    std::string name;
    uint32 requiredItem = 0;
    uint32 requiredCount = 0;
    uint32 creatureSet = 0;
    uint32 durationSec = 0;
};

struct ChallengeMirageCreatureTemplate
{
    uint32 id = 0;
    uint32 creatureSet = 0;
    uint32 entry = 0;
    uint32 mapId = 0;
    uint32 areaId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    uint32 count = 1;
    uint32 respawnSec = 300;
    float wanderDistance = 0.0f;
};

class ChallengeMirageMgr
{
public:
    static ChallengeMirageMgr* instance();

    void LoadConfig();
    void LoadTemplates();
    void Reload();

    bool IsEnabled() const { return _enabled; }
    bool IsDebugEnabled() const { return _debug; }
    uint32 GetDefaultLayer() const { return _defaultLayer; }
    uint32 GetMaxLevel() const { return _maxLevel; }

    void LoadPlayer(Player* player);
    void SavePlayer(Player* player);
    void RemovePlayer(Player* player);
    void UpdatePlayer(Player* player, uint32 diff);

    uint32 GetPlayerLayer(Player const* player) const;
    uint32 GetPlayerLevel(Player const* player) const;
    bool SetPlayerLevel(Player* player, uint32 level, bool saveNow = true);
    bool Enter(Player* player, uint32 level, std::string& reason);
    bool Leave(Player* player);
    void SpawnCreaturesForPlayer(Player* player);

    void SetCreatureLayer(Creature* creature, uint32 layer);
    uint32 GetCreatureLayer(Creature const* creature) const;
    bool IsCreatureVisibleForPlayer(Creature const* creature, Player const* player) const;
    void RemoveCreature(Creature const* creature);

private:
    ChallengeMirageLevelTemplate const* GetLevelTemplate(uint32 level) const;

    bool _enabled = true;
    bool _debug = false;
    uint32 _defaultLayer = 0;
    uint32 _maxLevel = 9999999;

    std::unordered_map<uint32, ChallengeMirageLevelTemplate> _levelTemplates;
    std::vector<ChallengeMirageCreatureTemplate> _creatureTemplates;
    std::unordered_map<uint32, uint32> _creatureEntryLayers;
    std::unordered_map<uint32, uint32> _playerLevels;
    std::unordered_map<uint32, uint32> _playerSpawnTimers;
    std::unordered_map<ObjectGuid, uint32> _creatureLayers;
    std::unordered_map<ObjectGuid, uint64> _spawnKeysByCreature;
    std::unordered_set<uint64> _activeSpawnKeys;
};

#define sChallengeMirageMgr ChallengeMirageMgr::instance()

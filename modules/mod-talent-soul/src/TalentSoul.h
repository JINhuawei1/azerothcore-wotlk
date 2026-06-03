/*
 * 天赋之魂系统
 *
 * 此文件定义了天赋之魂系统的核心类和数据结构
 * 主要功能：控制玩家技能的公共CD、技能冷却、技能消耗和伤害加成
 * 玩家可以通过点击升级来逐步提升技能效果
 * 支持职业限制和天赋点需求
 * 使用紧凑格式存储玩家数据
 */

#ifndef _TALENT_SOUL_H_
#define _TALENT_SOUL_H_

#include "Common.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "DatabaseEnv.h"
#include <map>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>
#include <shared_mutex>
#include <unordered_set>

/**
 * @enum TalentSoulUpgradeType
 * @brief 升级类型枚举
 */
enum TalentSoulUpgradeType
{
    TALENT_SOUL_UPGRADE_GCD = 0,        // 公共CD
    TALENT_SOUL_UPGRADE_COOLDOWN = 1,   // 技能冷却
    TALENT_SOUL_UPGRADE_COST = 2,       // 技能消耗
    TALENT_SOUL_UPGRADE_DAMAGE = 3,     // 伤害加成
    TALENT_SOUL_UPGRADE_MAX
};

/**
 * @struct TalentSoulData
 * @brief 存储天赋之魂的技能效果配置数据
 */
struct TalentSoulData
{
    uint32 id;                  // 唯一标识
    uint32 classType;           // 职业类型(0=全职业)
    uint32 talentPointCost;     // 需要的天赋点数
    uint32 spellId;             // 技能ID
    float gcdPerLevel;          // 每级公共CD减少百分比
    uint32 gcdMaxLevel;         // 公共CD最大等级
    float cooldownPerLevel;     // 每级冷却减少百分比
    uint32 cooldownMaxLevel;    // 冷却最大等级
    float costPerLevel;         // 每级消耗减少百分比
    uint32 costMaxLevel;        // 消耗最大等级
    float damagePerLevel;       // 每级伤害加成百分比
    uint32 damageMaxLevel;      // 伤害最大等级
    std::string description;    // 效果描述
};

/**
 * @struct PlayerSkillData
 * @brief 存储玩家单个技能的升级数据
 */
struct PlayerSkillData
{
    uint32 spellId;         // 技能ID
    uint32 gcdLevel;        // 公共CD等级
    uint32 cooldownLevel;   // 冷却等级
    uint32 costLevel;       // 消耗等级
    uint32 damageLevel;     // 伤害等级

    PlayerSkillData() : spellId(0), gcdLevel(0), cooldownLevel(0), costLevel(0), damageLevel(0) {}
};

/**
 * @struct PlayerTalentSoulData
 * @brief 存储玩家的天赋之魂数据（包含多个技能）
 */
struct PlayerTalentSoulData
{
    uint32 usedTalentPoints;                        // 已使用的天赋点
    std::map<uint32, PlayerSkillData> skills;       // 技能数据，键为技能ID

    PlayerTalentSoulData() : usedTalentPoints(0) {}
};

/**
 * @class TalentSoulMgr
 * @brief 天赋之魂管理器，单例模式
 */
class TalentSoulMgr
{
public:
    static TalentSoulMgr* instance();

    // 加载配置数据
    void LoadTalentSoulData();

    // 设置技能独立GCD类别（在加载配置后调用）
    void SetupIndependentGCDCategories();

    // 获取技能配置
    TalentSoulData const* GetTalentSoulData(uint32 spellId) const;
    std::map<uint32, TalentSoulData> const& GetAllTalentSoulData() const { return _talentSoulData; }

    // 获取职业可用的技能配置
    std::vector<TalentSoulData const*> GetClassTalentSoulData(uint8 playerClass) const;

    // 玩家数据管理
    void LoadPlayerData(Player* player);
    void SavePlayerData(Player* player);
    void SavePlayerData(uint32 playerGuid);
    void FlushDirtyPlayerData();
    void OnPlayerLogout(uint32 playerGuid);

    // 获取玩家数据
    PlayerTalentSoulData* GetPlayerData(uint32 playerGuid);
    PlayerSkillData* GetPlayerSkillData(uint32 playerGuid, uint32 spellId);

    // 升级技能
    bool UpgradePlayerSpell(Player* player, uint32 spellId, TalentSoulUpgradeType upgradeType);
    bool UpgradePlayerSpellAll(Player* player, uint32 spellId, TalentSoulUpgradeType upgradeType, uint32& upgradedLevels);

    // 检查玩家是否可以升级技能
    bool CanUpgradeSpell(Player* player, uint32 spellId, std::string& errorMsg) const;

    // 获取玩家的天赋点信息
    uint32 GetPlayerUsedTalentPoints(uint32 playerGuid) const;
    uint32 GetPlayerAvailableTalentPoints(Player* player) const;

    // 获取玩家技能的实际效果值
    float GetPlayerGCDReduction(uint32 playerGuid, uint32 spellId) const;
    float GetPlayerCooldownReduction(uint32 playerGuid, uint32 spellId) const;
    float GetPlayerCostReduction(uint32 playerGuid, uint32 spellId) const;
    float GetPlayerDamageBonus(uint32 playerGuid, uint32 spellId) const;

    // 应用效果
    void ApplyGCDReduction(Player* player, uint32 spellId, int32& gcd) const;
    void ApplyCooldownReduction(Player* player, uint32 spellId, int32& cooldown) const;
    void ApplyCostReduction(Player* player, uint32 spellId, int128& cost) const;
    void ApplyDamageBonus(Unit* attacker, uint32 spellId, uint128& damage) const;

    uint32 GetDataCount() const { return static_cast<uint32>(_talentSoulData.size()); }

private:
    TalentSoulMgr();
    ~TalentSoulMgr();

    // 紧凑格式解析和生成
    void ParseCompactData(const std::string& data, PlayerTalentSoulData& playerData);
    std::string GenerateCompactData(const PlayerTalentSoulData& playerData);

    // 配置数据，键为技能ID
    std::map<uint32, TalentSoulData> _talentSoulData;

    // 玩家数据，键为玩家GUID
    std::unordered_map<uint32, PlayerTalentSoulData> _playerData;
    std::unordered_set<uint32> _dirtyPlayers;

    // 线程安全保护
    mutable std::shared_mutex _configMutex;     // 配置数据读写锁
    mutable std::shared_mutex _playerDataMutex; // 玩家数据读写锁
};

#define sTalentSoulMgr TalentSoulMgr::instance()

#endif // _TALENT_SOUL_H_

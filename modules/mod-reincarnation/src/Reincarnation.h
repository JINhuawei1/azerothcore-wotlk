/*
 * 转身系统
 *
 * 此文件定义了转身系统的核心类和数据结构
 * 主要功能：玩家满足需求模板条件后可进行转身，转身后等级重置为1级
 * 每次转身获得全属性百分比加成和额外天赋点
 * 支持无限转身，通过需求模板系统控制转身条件
 */

#ifndef _REINCARNATION_H_
#define _REINCARNATION_H_

#include "Common.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include <map>
#include <unordered_map>
#include <string>

// 前向声明：应用/移除转身属性加成的辅助函数
void ApplyReincarnationStats(Player* player, float bonusPercent);
void RemoveReincarnationStats(Player* player, float bonusPercent);
void UpdateReincarnationStats(Player* player, float oldBonusPercent, float newBonusPercent);

/**
 * @struct ReincarnationConfig
 * @brief 转身配置数据结构
 */
struct ReincarnationConfig
{
    uint32 id;                      // 配置ID
    uint32 reincarnationLevel;      // 转身等级(0表示默认配置)
    uint32 requirementTemplateId;   // 需求模板ID
    float bonusStats;               // 全属性加成百分比
    uint32 bonusTalentPoints;       // 奖励天赋点数

    ReincarnationConfig() : id(0), reincarnationLevel(0), requirementTemplateId(0), bonusStats(5.0f), bonusTalentPoints(10) {}
};

/**
 * @struct PlayerReincarnationData
 * @brief 玩家转身数据结构
 */
struct PlayerReincarnationData
{
    uint32 reincarnationLevel;      // 当前转身等级(转身次数)
    float totalBonusStats;          // 累计全属性加成百分比
    uint32 totalBonusTalentPoints;  // 累计奖励天赋点数

    PlayerReincarnationData() : reincarnationLevel(0), totalBonusStats(0.0f), totalBonusTalentPoints(0) {}
};

/**
 * @class ReincarnationMgr
 * @brief 转身系统管理器，单例模式
 */
class ReincarnationMgr
{
public:
    static ReincarnationMgr* instance();

    // 加载配置数据
    void LoadReincarnationConfig();

    // 获取指定转身等级的配置(如果没有特定配置则返回默认配置)
    ReincarnationConfig const* GetConfigForLevel(uint32 level) const;

    // 获取默认配置
    ReincarnationConfig const* GetDefaultConfig() const;

    // 玩家数据管理
    void LoadPlayerData(Player* player);
    void SavePlayerData(Player* player);
    void OnPlayerLogout(uint32 playerGuid);

    // 获取玩家数据
    PlayerReincarnationData* GetPlayerData(uint32 playerGuid);

    // 检查玩家是否可以转身
    bool CanReincarnate(Player* player, std::string& errorMsg) const;

    // 执行转身（升级）
    bool DoReincarnate(Player* player);

    // 转身升级/降级
    bool ModifyReincarnationLevel(Player* player, int32 delta);

    // 设置转身等级
    bool SetReincarnationLevel(Player* player, uint32 level);

    // 获取玩家转身等级
    uint32 GetPlayerReincarnationLevel(uint32 playerGuid) const;

    // 获取玩家全属性加成百分比
    float GetPlayerBonusStats(uint32 playerGuid) const;

    // 获取玩家额外天赋点
    uint32 GetPlayerBonusTalentPoints(uint32 playerGuid) const;

    uint32 GetConfigCount() const { return static_cast<uint32>(_configs.size()); }

private:
    ReincarnationMgr();
    ~ReincarnationMgr();

    // 重新计算玩家累计奖励
    void RecalculatePlayerBonus(PlayerReincarnationData& data, uint32 level);

    // 转身配置，键为转身等级
    std::map<uint32, ReincarnationConfig> _configs;

    // 玩家数据，键为玩家GUID
    std::unordered_map<uint32, PlayerReincarnationData> _playerData;
};

#define sReincarnationMgr ReincarnationMgr::instance()

#endif // _REINCARNATION_H_

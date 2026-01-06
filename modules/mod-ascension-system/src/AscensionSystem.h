/*
 * 飞升系统 - AscensionSystem.h
 * 允许玩家在原有装备槽位基础上额外装备第二套装备
 */

#ifndef _ASCENSION_SYSTEM_H_
#define _ASCENSION_SYSTEM_H_

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Item.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <set>

// 飞升槽位数量（与官方装备槽位一致）
#define ASCENSION_SLOT_COUNT 18

// 飞升系统使用的特殊背包ID
// 用于在 character_inventory 表中标识飞升系统的物品
// 使用 200 作为虚拟背包标识，核心 PlayerStorage.cpp 会跳过此值
// 注意：需要确保 PlayerStorage.cpp 中的检查值与此一致
#define ASCENSION_VIRTUAL_BAG 200

// 槽位定义
enum AscensionSlots
{
    ASCENSION_SLOT_HEAD         = 0,
    ASCENSION_SLOT_NECK         = 1,
    ASCENSION_SLOT_SHOULDERS    = 2,
    ASCENSION_SLOT_BODY         = 3,
    ASCENSION_SLOT_CHEST        = 4,
    ASCENSION_SLOT_WAIST        = 5,
    ASCENSION_SLOT_LEGS         = 6,
    ASCENSION_SLOT_FEET         = 7,
    ASCENSION_SLOT_WRISTS       = 8,
    ASCENSION_SLOT_HANDS        = 9,
    ASCENSION_SLOT_FINGER1      = 10,
    ASCENSION_SLOT_FINGER2      = 11,
    ASCENSION_SLOT_TRINKET1     = 12,
    ASCENSION_SLOT_TRINKET2     = 13,
    ASCENSION_SLOT_BACK         = 14,
    ASCENSION_SLOT_MAINHAND     = 15,
    ASCENSION_SLOT_OFFHAND      = 16,
    ASCENSION_SLOT_RANGED       = 17
};

// 槽位控制数据
struct AscensionSlotControl
{
    uint8 slot;
    std::string slotName;
    bool enabled;
    uint32 unlockRequirement;
    float statMultiplier;
};

// 前向声明
class Item;

// 玩家飞升装备数据
struct AscensionSlotData
{
    uint32 itemId;
    uint32 itemGuid;
    Item* itemPtr;      // 物品指针（仅在线时有效）
};

// 已应用的属性效果
struct AppliedStatEffect
{
    uint32 statType;
    int32 statValue;
};

// 玩家飞升状态
struct PlayerAscensionStatus
{
    uint32 playerGuid;
    std::map<uint8, AscensionSlotData> slots;           // 槽位 -> 装备数据
    std::set<uint8> unlockedSlots;                       // 已解锁的槽位
    std::map<uint8, std::vector<AppliedStatEffect>> slotStats;   // 【修复】按槽位记录已应用的属性
    std::map<uint8, std::vector<uint32>> slotSpells;     // 【修复】按槽位记录已应用的法术
};

//=============================================================================
// AscensionConfig - 配置管理
//=============================================================================
class AscensionConfig
{
public:
    static AscensionConfig* instance();

    bool LoadConfig();

    bool IsEnabled() const { return _enabled; }
    bool IsDebugMode() const { return _debugMode; }
    float GetStatMultiplier() const { return _statMultiplier; }
    uint32 GetRequiredLevel() const { return _requiredLevel; }
    bool ShowNotification() const { return _showNotification; }
    bool IsAutoUnlock() const { return _autoUnlock; }

private:
    bool _enabled;
    bool _debugMode;
    float _statMultiplier;
    uint32 _requiredLevel;
    bool _showNotification;
    bool _autoUnlock;
};

#define sAscensionConfig AscensionConfig::instance()

//=============================================================================
// AscensionManager - 核心管理器
//=============================================================================
class AscensionManager
{
public:
    static AscensionManager* instance();

    bool Initialize();
    void LoadSlotControls();

    // 玩家数据管理
    void LoadPlayerData(Player* player);
    void SavePlayerData(Player* player);
    void ClearPlayerData(uint32 playerGuid);      // 只清理内存
    void DeletePlayerData(uint32 playerGuid);     // 清理内存并删除数据库
    void ValidateEquippedItems(Player* player);   // 验证装备物品是否有效
    void LoadAscensionItems(Player* player);      // 从数据库加载飞升物品实例
    void SaveAscensionItems(Player* player);      // 保存飞升物品到数据库

    // 槽位解锁
    bool IsSlotUnlocked(Player* player, uint8 slot);
    bool UnlockSlot(Player* player, uint8 slot);
    bool CheckSlotUnlockRequirement(Player* player, uint8 slot);

    // 装备管理
    bool EquipItem(Player* player, uint8 slot, uint32 itemId, uint32 itemGuid);
    bool UnequipItem(Player* player, uint8 slot);
    void UnequipAllItems(Player* player);

    // 属性应用
    void ApplyAllEffects(Player* player);
    void RemoveAllEffects(Player* player);
    void RefreshEffects(Player* player);

    // 槽位验证
    bool CanEquipItemInSlot(Player* player, uint8 slot, uint32 itemId);
    uint8 GetSlotForItemClass(uint32 itemClass, uint32 itemSubClass, uint32 inventoryType);

    // 获取数据
    PlayerAscensionStatus* GetPlayerStatus(uint32 playerGuid);
    const AscensionSlotControl* GetSlotControl(uint8 slot) const;
    std::string GetSlotName(uint8 slot) const;

    // 检查物品是否已装备到飞升槽位
    bool IsItemEquippedInAscension(Player* player, uint32 itemGuid);
    int8 GetAscensionSlotByItemGuid(Player* player, uint32 itemGuid);

    // 发送数据到客户端
    void SendAscensionDataToClient(Player* player);

private:
    void ApplyItemEffect(Player* player, uint32 itemId, uint8 slot, bool apply);
    void ApplyEnchantStatMod(Player* player, uint32 statType, int32 amount, bool apply);
    void RemoveStatEffect(Player* player, uint32 statType, int32 statValue);
    void UpdatePlayerStats(Player* player);
    Item* FindItemInBags(Player* player, uint32 itemGuid);

    std::map<uint8, AscensionSlotControl> _slotControls;
    std::map<uint32, PlayerAscensionStatus> _playerStatus;
};

#define sAscensionManager AscensionManager::instance()

//=============================================================================
// 脚本类声明
//=============================================================================

// 世界脚本
class AscensionWorldScript : public WorldScript
{
public:
    AscensionWorldScript();
    void OnAfterConfigLoad(bool reload) override;
    void OnUpdate(uint32 diff) override;

private:
    bool _initialized;
    uint32 _loadTimer;
};

// 玩家脚本
class AscensionPlayerScript : public PlayerScript
{
public:
    AscensionPlayerScript();
    void OnPlayerLogin(Player* player) override;
    void OnPlayerLogout(Player* player) override;
    void OnPlayerDelete(ObjectGuid guid, uint32 accountId) override;
};

// 命令脚本
class AscensionCommandScript : public CommandScript
{
public:
    AscensionCommandScript();
    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleAscensionView(ChatHandler* handler, const char* args);
    static bool HandleAscensionEquip(ChatHandler* handler, const char* args);
    static bool HandleAscensionUnequip(ChatHandler* handler, const char* args);
    static bool HandleAscensionClear(ChatHandler* handler, const char* args);
    static bool HandleAscensionRefresh(ChatHandler* handler, const char* args);
    static bool HandleAscensionUnlock(ChatHandler* handler, const char* args);
    static bool HandleAscensionReload(ChatHandler* handler, const char* args);
};

// 物品脚本 - 用于阻止飞升槽位中的物品被删除
class AscensionItemScript : public AllItemScript
{
public:
    AscensionItemScript();
    bool CanItemRemove(Player* player, Item* item) override;
};

// 脚本加载函数
void AddAscensionSystemScripts();

#endif // _ASCENSION_SYSTEM_H_

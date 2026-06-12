#ifndef MODULE_ITEM_RECYCLE_H
#define MODULE_ITEM_RECYCLE_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ScriptedGossip.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ModuleManager.h"
#include "RequirementInterface.h"
#include "RewardInterface.h"
#include <unordered_map>
#include <unordered_set>
#include <set>

// 回收类型枚举
enum RecycleType
{
    RECYCLE_TYPE_ALL = -1,          // 所有类型 - 新增：支持一键回收所有类型物品
    RECYCLE_TYPE_EQUIPMENT = 1,     // 装备 - 武器和护甲
    RECYCLE_TYPE_CONSUMABLE = 2,    // 消耗品 - 药水、食物等
    RECYCLE_TYPE_QUEST = 3,         // 任务物品 - 任务相关道具
    RECYCLE_TYPE_JUNK = 4,          // 垃圾 - 灰色品质物品和其他杂物
    RECYCLE_TYPE_GEM = 5,           // 宝石 - 各种宝石
    RECYCLE_TYPE_ENCHANT = 6        // 附魔材料 - 附魔用的材料
};

// 注意：使用核心代码中已定义的ItemQualities枚举，不需要重复定义

// 全局回收配置信息结构体
struct ItemRecycleInfo
{
    uint32 id;                          // 配置ID
    uint32 group;                       // 回收组别
    uint32 priority;                    // 优先级
    uint32 itemId;                      // 物品ID（0表示通用规则）
    uint32 requirementTemplateId;       // 需求模板ID（对应需求_模板表，0=无需求，支持任意模板ID）
    uint32 rewardTemplateId;            // 奖励模板ID（对应奖励_模板表，0=无奖励，支持任意模板ID）
    uint32 recycleType;                 // 回收类型
    uint32 minLevel;                    // 最小等级
    uint32 maxLevel;                    // 最大等级
    int32 qualityRequirement;           // 品质要求（-1=所有品质，0-5=具体品质）
    std::set<uint32> filterItems;       // 过滤物品ID列表（不参与自动/界面回收的物品）
    bool enabled;                       // 是否启用
    std::string comment;                // 注释说明
};

// 玩家回收设置结构体
struct PlayerRecycleSettings
{
    uint32 playerGuid;                  // 玩家GUID
    bool autoRecycleEnabled;            // 是否启用自动回收
    uint32 recycleInterval;             // 自动回收间隔（秒）
    bool recycleTypes[8];               // 回收类型开关数组
                                        // 索引1-6对应具体类型，索引7对应所有类型(-1)
                                        // 新增：支持"所有类型"一键开关
    uint32 minQuality;                  // 最小品质（0=灰色，1=白色，2=绿色，3=蓝色，4=紫色，5=橙色）
    uint32 maxQuality;                  // 最大品质
    uint32 minLevel;                    // 最小物品等级
    uint32 maxLevel;                    // 最大物品等级
    bool protectEquipped;               // 是否保护已装备的物品
    uint32 lastRecycleTime;             // 上次自动回收时间戳
    std::set<uint32> filteredItems;     // 过滤物品ID集合（不回收的物品）

    // 注意：需求模板ID和奖励模板ID在全局配置表（物品_回收）中定义
    // 玩家配置表只存储个人偏好设置
    // 需求模板ID和奖励模板ID现在支持任意数值（不再限制为1-9）
};

// 命令处理函数声明
bool CheckRecycleModuleInit(ChatHandler* handler);
bool HandleRecycleMainCommand(ChatHandler* handler);
bool HandleRecycleGroupCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
bool HandleRecycleUICommand(ChatHandler* handler);
bool HandleRecycleAutoCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
bool HandleRecycleSetCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
bool HandleRecycleStatusCommand(ChatHandler* handler);
bool HandleRecycleTestCommand(ChatHandler* handler);
bool HandleRecycleExecuteCommand(ChatHandler* handler);
bool HandleRecycleFilterCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
bool HandleRecycleUpdateConfigCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);

// 辅助函数声明
std::string GetRequirementDetails(uint32 templateId);

class ItemRecycleScript : public CommandScript
{
public:
    ItemRecycleScript();

    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    // 数据库加载函数
    static void LoadItemRecycleFromDB();
    static void LoadPlayerRecycleSettings();

    // 回收相关函数
    static bool IsProtectedQuestItem(uint32 itemId);
    static bool IsActiveQuestItem(Player* player, uint32 itemId);
    static bool IsReservedByMaterialWarehouse(Player* player, uint32 itemId);
    static bool CanRecycleItem(Player* player, Item* item, const PlayerRecycleSettings& settings);
    // 新增：查找匹配的回收规则
    static const ItemRecycleInfo* FindMatchingRecycleRule(Item* item, const PlayerRecycleSettings& settings);
    // 奖励计算函数（已移除内置奖励，只支持奖励模板）
    static uint32 GetItemRecycleReward(Item* item, Player* player, const PlayerRecycleSettings& settings);
    static void PerformAutoRecycle(Player* player);
    static void SavePlayerRecycleSettings(const PlayerRecycleSettings& settings);
    static PlayerRecycleSettings GetPlayerRecycleSettings(uint32 playerGuid);
    static void DeletePlayerRecycleSettings(uint32 playerGuid);

    // 公有访问函数
    static size_t GetItemRecycleStoreSize() { return m_ItemRecycleStore.size(); }
    static size_t GetPlayerSettingsSize() { return m_PlayerSettings.size(); }
    static const std::vector<ItemRecycleInfo>& GetItemRecycleStore() { return m_ItemRecycleStore; }

private:
    static std::vector<ItemRecycleInfo> m_ItemRecycleStore;
    static std::unordered_map<uint32, PlayerRecycleSettings> m_PlayerSettings;

    // 材料仓库"自动存储"物品集合缓存 <玩家GUID, (物品ID集合, 上次查库时间ms)>
    // 此前自动回收每周期对每个背包物品同步查一次 _材料仓库玩家 表
    static std::unordered_map<uint32, std::pair<std::unordered_set<uint32>, uint32>> m_MaterialReserveCache;

    friend bool CheckRecycleModuleInit(ChatHandler* handler);
    friend bool HandleRecycleMainCommand(ChatHandler* handler);
    friend bool HandleRecycleGroupCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
    friend bool HandleRecycleUICommand(ChatHandler* handler);
    friend bool HandleRecycleAutoCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
    friend bool HandleRecycleSetCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
    friend bool HandleRecycleStatusCommand(ChatHandler* handler);
    friend bool HandleRecycleTestCommand(ChatHandler* handler);
    friend bool HandleRecycleExecuteCommand(ChatHandler* handler);
    friend bool HandleRecycleFilterCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
    friend bool HandleRecycleUpdateConfigCommand(ChatHandler* handler, Acore::ChatCommands::Tail args);
    friend class ItemRecyclePlayerScript;
};

// 玩家脚本类，用于处理自动回收
class ItemRecyclePlayerScript : public PlayerScript
{
public:
    ItemRecyclePlayerScript() : PlayerScript("ItemRecyclePlayerScript") { }

    void OnPlayerLogin(Player* player) override;
    void OnPlayerLogout(Player* player) override;
    void OnPlayerUpdate(Player* player, uint32 diff) override;
    void OnPlayerDelete(ObjectGuid guid, uint32 accountId) override;
};

// 世界脚本类，用于模块初始化和配置加载
class ItemRecycle_Worldscript : public WorldScript
{
public:
    ItemRecycle_Worldscript();

    void OnAfterConfigLoad(bool reload) override;
    void OnUpdate(uint32 diff) override;

private:
    uint32 _initTimer;
    bool _initialized;
};

#endif // MODULE_ITEM_RECYCLE_H

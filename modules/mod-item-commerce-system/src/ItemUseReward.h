#ifndef MODULE_ITEM_USE_REWARD_H
#define MODULE_ITEM_USE_REWARD_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Item.h"
#include "SpellMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"

struct ItemUseRewardInfo
{
    uint32 entry;                // 物品ID
    uint32 rewardId;             // 奖励ID
    std::string gmCommand;       // GM命令
    std::string comment;         // 注释
    bool consumeItem;            // 是否消耗物品
};

class ItemUseReward : public AllItemScript
{
public:
    ItemUseReward();

    // 加载配置
    static bool LoadConfig(bool reload);

    // 从数据库加载物品使用奖励信息
    static void LoadFromDB();

    // 物品使用事件
    bool CanItemUse(Player* player, Item* item, SpellCastTargets const& targets) override;

    // 执行GM命令
    static bool ExecuteGMCommand(Player* player, const std::string& command);

    // 处理奖励
    static void ProcessReward(Player* player, uint32 rewardId);

    // 获取所有配置的物品
    static std::map<uint32, ItemUseRewardInfo> const& GetAllItems() { return itemUseRewardStore; }

    // 检查物品是否在配置中
    static bool HasItem(uint32 itemId) { return itemUseRewardStore.find(itemId) != itemUseRewardStore.end(); }

    // 添加物品到数据库
    static bool AddItemToDatabase(uint32 itemId, uint32 rewardId, const std::string& gmCommand, bool consume, const std::string& comment);

    // 从数据库删除物品
    static bool RemoveItemFromDatabase(uint32 itemId);

private:
    static bool enabled;
    static bool debugMode;
    static bool allowGMCommands;
    static bool consumeItemByDefault;
    static std::map<uint32, ItemUseRewardInfo> itemUseRewardStore;
};

// 使用ModuleLoader代替WorldScript

#endif // MODULE_ITEM_USE_REWARD_H

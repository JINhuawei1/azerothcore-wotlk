#ifndef MODULE_ITEM_SELL_REWARD_H
#define MODULE_ITEM_SELL_REWARD_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "ScriptedGossip.h"
#include "Log.h"

struct ItemSellRewardInfo
{
    uint32 entry;                // 物品ID
    uint32 rewardId;             // 奖励ID
    std::string comment;         // 注释
};

class ItemSellReward : public PlayerScript
{
public:
    ItemSellReward();

    // 加载配置
    static bool LoadConfig(bool reload);

    // 从数据库加载物品售卖奖励信息
    static void LoadFromDB();

    // 检查物品是否在配置中
    static bool HasItem(uint32 itemId) { return itemSellRewardStore.find(itemId) != itemSellRewardStore.end(); }

    // 添加物品到数据库
    static bool AddItemToDatabase(uint32 itemId, uint32 rewardId, const std::string& comment);

    // 从数据库删除物品
    static bool RemoveItemFromDatabase(uint32 itemId);

    // 获取所有配置的物品
    static std::map<uint32, ItemSellRewardInfo> const& GetAllItems() { return itemSellRewardStore; }

    // 处理奖励
    static void ProcessReward(Player* player, uint32 rewardId);
    
    // 玩家售卖物品钩子
    bool OnPlayerCanSellItem(Player* player, Item* item, Creature* creature) override;

private:
    static bool enabled;
    static bool debugMode;
    static bool allowGMCommands;
    static std::map<uint32, ItemSellRewardInfo> itemSellRewardStore;
};

#endif // MODULE_ITEM_SELL_REWARD_H 
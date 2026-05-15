#include "ItemSellReward.h"
#include "Logging/Log.h"
#include "ScriptMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "StringFormat.h"

bool ItemSellReward::enabled = false;
bool ItemSellReward::debugMode = false;
bool ItemSellReward::allowGMCommands = false;
std::map<uint32, ItemSellRewardInfo> ItemSellReward::itemSellRewardStore;

ItemSellReward::ItemSellReward() : PlayerScript("ItemSellReward")
{
}

bool ItemSellReward::LoadConfig(bool reload)
{
    if (reload)
    {
        itemSellRewardStore.clear();
    }

    enabled = sConfigMgr->GetOption<bool>("ItemSellReward.Enable", true);
    if (!enabled)
    {
        return false;
    }

    debugMode = sConfigMgr->GetOption<bool>("ItemSellReward.DebugMode", false);
    allowGMCommands = sConfigMgr->GetOption<bool>("ItemSellReward.AllowGMCommands", true);

    return true;
}

void ItemSellReward::LoadFromDB()
{
    if (!enabled)
        return;

    uint32 oldMSTime = getMSTime();

    // 清除旧数据
    itemSellRewardStore.clear();

    // 加载新数据
    QueryResult result = WorldDatabase.Query("SELECT entry, 物品出售奖励, 注释 FROM `_物品_售卖获得`");
    if (!result)
    {
        LOG_INFO("server.loading", ">> 物品售卖获得系统: 未加载数据。表可能为空。");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 itemId = fields[0].Get<uint32>();
        uint32 rewardId = fields[1].Get<uint32>();
        std::string comment = fields[2].Get<std::string>();

        // 存储物品奖励信息
        ItemSellRewardInfo& itemInfo = itemSellRewardStore[itemId];
        itemInfo.entry = itemId;
        itemInfo.rewardId = rewardId;
        itemInfo.comment = comment;

        if (debugMode)
        {
            LOG_INFO("server.loading", ">> 物品售卖获得系统: 加载物品ID: {} 奖励ID: {}", itemId, rewardId);
        }

        count++;
    } while (result->NextRow());

}

bool ItemSellReward::OnPlayerCanSellItem(Player* player, Item* item, Creature* creature)
{
    if (!enabled || !player || !item)
        return true;

    uint32 itemId = item->GetEntry();
    
    // 检查物品是否在配置中
    if (HasItem(itemId))
    {
        const ItemSellRewardInfo& info = itemSellRewardStore[itemId];

        // 处理奖励
        if (info.rewardId > 0)
        {
            ProcessReward(player, info.rewardId);
            
            if (debugMode)
            {
                LOG_INFO("server.loading", ">> 物品售卖获得系统: 玩家 {} 出售物品 {} 获得奖励 {}", 
                    player->GetName().c_str(), itemId, info.rewardId);
            }
        }
    }

    // 允许正常售卖
    return true;
}

void ItemSellReward::ProcessReward(Player* player, uint32 rewardId)
{
    if (!player || !rewardId)
        return;

    // 这里可以根据rewardId处理不同类型的奖励
    // 例如：给予金币、物品、经验等
    // 当前简单实现为给予奖励金币
    player->ModifyMoney(rewardId * 10000); // 以铜币为单位，10000铜币 = 1金币
    ChatHandler(player->GetSession()).PSendSysMessage("你因售卖物品获得了 {} 金币的额外奖励!", rewardId);
}

bool ItemSellReward::AddItemToDatabase(uint32 itemId, uint32 rewardId, const std::string& comment)
{
    if (!enabled || itemId == 0)
        return false;

    // 检查物品是否存在
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
    {
        return false;
    }

    // 删除现有数据
    WorldDatabase.DirectExecute(Acore::StringFormat("DELETE FROM `_物品_售卖获得` WHERE entry = {}", itemId));

    // 插入新数据
    std::string safeComment = comment;
    WorldDatabase.EscapeString(safeComment);
    
    std::string query = Acore::StringFormat("INSERT INTO `_物品_售卖获得` (entry, 物品出售奖励, 注释) VALUES ({}, {}, '{}')",
        itemId, rewardId, safeComment.c_str());

    WorldDatabase.DirectExecute(query);

    // 更新内存中的数据
    ItemSellRewardInfo& itemInfo = itemSellRewardStore[itemId];
    itemInfo.entry = itemId;
    itemInfo.rewardId = rewardId;
    itemInfo.comment = comment;

    return true;
}

bool ItemSellReward::RemoveItemFromDatabase(uint32 itemId)
{
    if (!enabled || itemId == 0)
        return false;

    // 检查物品是否在配置中
    if (!HasItem(itemId))
    {
        return false;
    }

    // 删除数据库中的记录
    WorldDatabase.DirectExecute(Acore::StringFormat("DELETE FROM `_物品_售卖获得` WHERE entry = {}", itemId));

    // 从内存中移除
    itemSellRewardStore.erase(itemId);

    return true;
}

// 添加脚本
void AddItemSellRewardScripts()
{
    new ItemSellReward();
}

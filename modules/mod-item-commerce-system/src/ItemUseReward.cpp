#include "ItemUseReward.h"
#include "ScriptMgr.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "World.h"
#include "Config.h"
#include "Util.h"
#include "Define.h"
#include "StringFormat.h"
#include "Logging/Log.h"

// 初始化静态变量
bool ItemUseReward::enabled = false;
bool ItemUseReward::debugMode = false;
bool ItemUseReward::allowGMCommands = false;
bool ItemUseReward::consumeItemByDefault = true;
std::map<uint32, ItemUseRewardInfo> ItemUseReward::itemUseRewardStore;

ItemUseReward::ItemUseReward() : AllItemScript("ItemUseReward")
{
}

bool ItemUseReward::LoadConfig(bool reload)
{
    // 使用硬编码值，避免编译问题
    enabled = true;
    debugMode = false;
    allowGMCommands = true;
    consumeItemByDefault = true;

    if (debugMode)
    {
        LOG_INFO("module.itemusereward", "物品使用奖励模块: 已加载配置");
        LOG_INFO("module.itemusereward", "启用状态: {}", enabled ? "启用" : "禁用");
        LOG_INFO("module.itemusereward", "调试模式: {}", debugMode ? "启用" : "禁用");
        LOG_INFO("module.itemusereward", "允许GM命令: {}", allowGMCommands ? "是" : "否");
        LOG_INFO("module.itemusereward", "默认消耗物品: {}", consumeItemByDefault ? "是" : "否");
    }

    return enabled;
}

void ItemUseReward::LoadFromDB()
{
    if (!enabled)
        return;

    // 清空旧数据
    itemUseRewardStore.clear();

    uint32 oldMSTime = getMSTime();
    uint32 count = 0;

    // 从数据库加载物品使用奖励信息
    QueryResult result = WorldDatabase.Query("SELECT entry, 物品使用奖励, GM命令, 注释, 消耗物品 FROM `_物品_使用获得`");
    if (!result)
    {
        LOG_WARN("module.itemusereward", ">> 未加载物品使用奖励数据，数据库表 `_物品_使用获得` 为空");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ItemUseRewardInfo info;
        info.entry = fields[0].Get<uint32>();
        info.rewardId = fields[1].Get<uint32>();

        if (!fields[2].IsNull())
            info.gmCommand = fields[2].Get<std::string>();

        if (!fields[3].IsNull())
            info.comment = fields[3].Get<std::string>();

        info.consumeItem = fields[4].Get<bool>();

        itemUseRewardStore[info.entry] = info;
        count++;

    } while (result->NextRow());

    // 不在这里显示ASCII艺术框，而是在ModuleLoader中显示
}

bool ItemUseReward::CanItemUse(Player* player, Item* item, SpellCastTargets const& targets)
{
    if (!enabled || !player || !item)
        return false;

    uint32 itemId = item->GetEntry();
    auto itr = itemUseRewardStore.find(itemId);
    if (itr == itemUseRewardStore.end())
        return false;

    // 找到了配置的物品
    const ItemUseRewardInfo& info = itr->second;

    if (debugMode)
    {
        LOG_INFO("module.itemusereward", "玩家 {} 使用物品 {} (ID: {})",
            player->GetName(), item->GetTemplate()->Name1, itemId);
    }

    bool hasReward = false;

    // 处理奖励ID
    if (info.rewardId > 0)
    {
        // 处理奖励
        ProcessReward(player, info.rewardId);
        hasReward = true;
    }

    // 处理GM命令
    if (!info.gmCommand.empty() && allowGMCommands)
    {
        if (debugMode)
        {
            LOG_INFO("module.itemusereward", "为玩家 {} 执行GM命令: {}",
                player->GetName(), info.gmCommand);
        }

        if (ExecuteGMCommand(player, info.gmCommand))
        {
            hasReward = true;
        }
    }

    // 如果有奖励或执行了命令，并且需要消耗物品
    if (hasReward && info.consumeItem)
    {
        // 消耗一个物品
        uint32 count = 1;
        player->DestroyItemCount(itemId, count, true);

        if (debugMode)
        {
            LOG_INFO("module.itemusereward", "玩家 {} 消耗了物品 {} (ID: {})",
                player->GetName(), item->GetTemplate()->Name1, itemId);
        }
    }

    // 返回true表示我们已经处理了这个物品的使用事件
    return true;
}

void ItemUseReward::ProcessReward(Player* player, uint32 rewardId)
{
    if (!player || rewardId == 0)
        return;

    if (debugMode)
    {
        LOG_INFO("module.itemusereward", "玩家 {} 获得奖励ID: {}",
            player->GetName(), rewardId);
    }

    // 这里可以实现奖励逻辑，例如调用奖励系统的接口
    // 目前只是简单通知玩家
    ChatHandler handler(player->GetSession());
    handler.PSendSysMessage("您获得了奖励 (ID: {})！", rewardId);

    // 如果有奖励系统，可以在这里调用
    // 例如：sRewardMgr->GiveReward(player, rewardId);
}

bool ItemUseReward::ExecuteGMCommand(Player* player, const std::string& command)
{
    if (!player || command.empty())
        return false;

    // 创建一个ChatHandler来执行命令
    ChatHandler handler(player->GetSession());

    // 移除命令前的点号（如果有）
    std::string actualCommand = command;
    if (!actualCommand.empty() && actualCommand[0] == '.')
        actualCommand = actualCommand.substr(1);

    // 执行命令
    bool result = handler.ParseCommands(actualCommand.c_str());

    // 如果命令执行成功，通知玩家
    if (result)
    {
        handler.PSendSysMessage("命令执行成功！");
    }

    return result;
}

// 添加物品到数据库
bool ItemUseReward::AddItemToDatabase(uint32 itemId, uint32 rewardId, const std::string& gmCommand, bool consume, const std::string& comment)
{
    if (!enabled)
        return false;

    // 检查物品是否已存在
    if (HasItem(itemId))
    {
        // 更新现有记录
        std::string escapedGmCommand = gmCommand;
        std::string escapedComment = comment;
        WorldDatabase.EscapeString(escapedGmCommand);
        WorldDatabase.EscapeString(escapedComment);

        std::string updateQuery = "UPDATE `_物品_使用获得` SET 物品使用奖励 = " + std::to_string(rewardId) +
                                 ", GM命令 = '" + escapedGmCommand +
                                 "', 消耗物品 = " + std::to_string(consume ? 1 : 0);

        if (!comment.empty())
            updateQuery += ", 注释 = '" + escapedComment + "'";

        updateQuery += " WHERE entry = " + std::to_string(itemId);

        WorldDatabase.Execute(updateQuery);
        return true;
    }
    else
    {
        // 插入新记录
        std::string escapedGmCommand = gmCommand;
        std::string escapedComment = comment;
        WorldDatabase.EscapeString(escapedGmCommand);
        WorldDatabase.EscapeString(escapedComment);

        std::string insertQuery = "INSERT INTO `_物品_使用获得` (entry, 物品使用奖励, GM命令, 消耗物品, 注释) VALUES (" +
                                 std::to_string(itemId) + ", " +
                                 std::to_string(rewardId) + ", '" +
                                 escapedGmCommand + "', " +
                                 std::to_string(consume ? 1 : 0) + ", '" +
                                 escapedComment + "')";

        WorldDatabase.Execute(insertQuery);
        return true;
    }
}

// 从数据库删除物品
bool ItemUseReward::RemoveItemFromDatabase(uint32 itemId)
{
    if (!enabled)
        return false;

    // 检查物品是否存在
    if (!HasItem(itemId))
        return false;

    // 从数据库删除
    std::string deleteQuery = "DELETE FROM `_物品_使用获得` WHERE entry = " + std::to_string(itemId);
    WorldDatabase.Execute(deleteQuery);
    return true;
}

// 添加脚本
void AddItemUseRewardScripts()
{
    new ItemUseReward();
}

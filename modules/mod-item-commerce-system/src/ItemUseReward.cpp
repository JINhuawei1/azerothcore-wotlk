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
#include "GameTime.h"
#include "ModuleManager.h"
#include "RewardInterface.h"
#include "StringFormat.h"
#include "Logging/Log.h"
#include <sstream>

// 初始化静态变量
bool ItemUseReward::enabled = false;
bool ItemUseReward::debugMode = false;
bool ItemUseReward::allowGMCommands = false;
bool ItemUseReward::consumeItemByDefault = true;
std::map<uint32, ItemUseRewardInfo> ItemUseReward::itemUseRewardStore;

namespace
{
bool TryExecuteXianmenContributionCommand(Player* player, std::string const& command, bool& result)
{
    std::istringstream stream(command);
    std::string root;
    std::string action;
    uint64 amount = 0;
    stream >> root >> action >> amount;

    if (root != "仙门" && root != "xianmen")
        return false;

    if (action != "加贡献" && action != "增加贡献" && action != "addcontribution" && action != "add_contribution")
        return false;

    ChatHandler handler(player->GetSession());

    if (!amount)
    {
        handler.SendSysMessage("用法: .仙门 加贡献 <数值>");
        result = false;
        return true;
    }

    uint32 const guid = player->GetGUID().GetCounter();
    if (!CharacterDatabase.Query("SELECT 1 FROM `_仙门_玩家` WHERE `角色GUID` = {}", guid))
    {
        handler.SendSysMessage("|cffff0000[仙门系统]|r 你尚未加入仙门，无法获得宗门贡献。");
        result = false;
        return true;
    }

    uint32 const now = static_cast<uint32>(GameTime::GetGameTime().count());
    CharacterDatabase.Execute(
        "UPDATE `_仙门_玩家` SET `当日贡献` = `当日贡献` + {}, `历史贡献` = `历史贡献` + {}, `更新时间` = {} WHERE `角色GUID` = {}",
        amount, amount, now, guid);

    handler.PSendSysMessage("|cff66ffcc[仙门系统]|r 已增加当日贡献 {}。", amount);
    result = true;
    return true;
}
}

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

    // 处理奖励模板ID
    if (info.rewardId > 0)
    {
        hasReward = ProcessReward(player, info.rewardId);
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

bool ItemUseReward::ProcessReward(Player* player, uint32 rewardId)
{
    if (!player || rewardId == 0)
        return false;

    if (debugMode)
    {
        LOG_INFO("module.itemusereward", "玩家 {} 获得奖励模板ID: {}",
            player->GetName(), rewardId);
    }

    RewardInterface* rewardModule = sModuleManager->GetRewardModule();
    if (!rewardModule)
    {
        LOG_INFO("server.loading", "[物品使用奖励] 奖励模板模块未注册，无法发放 _模板_奖励 id={}", rewardId);
        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[物品使用奖励]|r 奖励模板模块未注册，无法发放奖励。");
        return false;
    }

    bool const success = rewardModule->GiveReward(player, rewardId, true, true);
    if (!success)
    {
        LOG_INFO("server.loading", "[物品使用奖励] 玩家 {} 发放 _模板_奖励 id={} 失败", player->GetName(), rewardId);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[物品使用奖励]|r 奖励模板 {} 发放失败。", rewardId);
        return false;
    }

    return true;
}

bool ItemUseReward::ExecuteGMCommand(Player* player, const std::string& command)
{
    if (!player || command.empty())
        return false;

    ChatHandler handler(player->GetSession());

    std::string commandToParse = command;
    if (!commandToParse.empty() && commandToParse[0] != '.' && commandToParse[0] != '!')
        commandToParse.insert(commandToParse.begin(), '.');

    std::string actualCommand = commandToParse.substr(1);
    bool result = false;
    if (TryExecuteXianmenContributionCommand(player, actualCommand, result))
        return result;

    // 执行命令
    result = handler.ParseCommands(commandToParse.c_str());

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

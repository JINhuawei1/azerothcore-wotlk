#include "ItemUseReward.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "Language.h"
#include "Util.h"

using namespace Acore::ChatCommands;

class ItemUseRewardCommand : public CommandScript
{
public:
    ItemUseRewardCommand() : CommandScript("ItemUseRewardCommand") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable itemUseRewardCommandTable =
        {
            { "重载",          HandleReloadCommand,          SEC_ADMINISTRATOR,  Console::No },
            { "列表",          HandleListCommand,            SEC_ADMINISTRATOR,  Console::No },
            { "添加",          HandleAddCommand,             SEC_ADMINISTRATOR,  Console::No },
            { "删除",          HandleRemoveCommand,          SEC_ADMINISTRATOR,  Console::No },
            { "帮助",          HandleHelpCommand,            SEC_PLAYER,         Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "物品使用奖励",  itemUseRewardCommandTable }
        };

        return commandTable;
    }

    // 重载配置和数据库数据
    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!ItemUseReward::LoadConfig(true))
        {
            handler->PSendSysMessage("物品使用奖励模块已禁用，无法重载。");
            return true;
        }

        ItemUseReward::LoadFromDB();
        handler->PSendSysMessage("物品使用奖励模块配置和数据已重载。");
        return true;
    }

    // 列出所有配置的物品
    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        std::map<uint32, ItemUseRewardInfo> items = ItemUseReward::GetAllItems();

        if (items.empty())
        {
            handler->PSendSysMessage("没有配置任何物品使用奖励。");
            return true;
        }

        handler->PSendSysMessage("物品使用奖励列表：");
        handler->PSendSysMessage("----------------------------------------");
        handler->PSendSysMessage("| 物品ID | 奖励ID | 消耗 | 描述");
        handler->PSendSysMessage("----------------------------------------");

        for (auto const& pair : items)
        {
            ItemUseRewardInfo const& info = pair.second;
            std::string itemName = info.comment.empty() ? "未命名" : info.comment;
            std::string gmCommand = info.gmCommand.empty() ? "无" : info.gmCommand;

            handler->PSendSysMessage("| {} | {} | {} | {}",
                info.entry,
                info.rewardId,
                info.consumeItem ? "是" : "否",
                itemName.c_str());

            if (!info.gmCommand.empty())
            {
                handler->PSendSysMessage("| - GM命令: {}", gmCommand.c_str());
            }
        }

        handler->PSendSysMessage("----------------------------------------");
        handler->PSendSysMessage("共 {} 个物品", (uint32)items.size());
        return true;
    }

    // 添加物品使用奖励配置
    static bool HandleAddCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("用法: .物品使用奖励 添加 <物品ID> <奖励ID> <GM命令> <是否消耗物品> <描述>");
            handler->PSendSysMessage("例如: .物品使用奖励 添加 12345 1 \".add 25\" 1 测试物品");
            return true;
        }

        char* itemIdStr = strtok((char*)args, " ");
        char* rewardIdStr = strtok(nullptr, " ");
        char* gmCommandStr = strtok(nullptr, "\"");

        // 跳过引号
        if (gmCommandStr && gmCommandStr[0] == ' ' && gmCommandStr[1] == '"')
            gmCommandStr += 2;

        // 找到下一个引号
        if (gmCommandStr)
        {
            char* endQuote = strchr(gmCommandStr, '"');
            if (endQuote)
                *endQuote = '\0';
        }

        char* consumeStr = strtok(nullptr, " ");

        // 跳过可能的引号和空格
        if (consumeStr && consumeStr[0] == '"')
            consumeStr++;
        if (consumeStr && consumeStr[0] == ' ')
            consumeStr++;

        char* commentStr = strtok(nullptr, "\0");

        if (!itemIdStr || !rewardIdStr)
        {
            handler->PSendSysMessage("用法: .物品使用奖励 添加 <物品ID> <奖励ID> <GM命令> <是否消耗物品> <描述>");
            return true;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 rewardId = atoi(rewardIdStr);
        std::string gmCommand = gmCommandStr ? gmCommandStr : "";
        bool consume = consumeStr ? (atoi(consumeStr) > 0) : true;
        std::string comment = commentStr ? commentStr : "";

        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("错误：物品ID {} 不存在。", itemId);
            return true;
        }

        // 添加到数据库
        bool success = ItemUseReward::AddItemToDatabase(itemId, rewardId, gmCommand, consume, comment);

        if (success)
        {
            handler->PSendSysMessage("成功添加物品使用奖励配置：");
            handler->PSendSysMessage("物品ID: {} ({})", itemId, itemTemplate->Name1.c_str());
            handler->PSendSysMessage("奖励ID: {}", rewardId);

            if (!gmCommand.empty())
                handler->PSendSysMessage("GM命令: {}", gmCommand.c_str());

            handler->PSendSysMessage("消耗物品: {}", consume ? "是" : "否");

            if (!comment.empty())
                handler->PSendSysMessage("描述: {}", comment.c_str());

            // 重新加载数据
            ItemUseReward::LoadFromDB();
        }
        else
        {
            handler->PSendSysMessage("添加物品使用奖励配置失败。");
        }

        return true;
    }

    // 删除物品使用奖励配置
    static bool HandleRemoveCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("用法: .物品使用奖励 删除 <物品ID>");
            return true;
        }

        uint32 itemId = atoi(args);

        // 检查物品是否存在于配置中
        if (!ItemUseReward::HasItem(itemId))
        {
            handler->PSendSysMessage("错误：物品ID {} 不在物品使用奖励配置中。", itemId);
            return true;
        }

        // 从数据库删除
        bool success = ItemUseReward::RemoveItemFromDatabase(itemId);

        if (success)
        {
            handler->PSendSysMessage("成功删除物品ID {} 的使用奖励配置。", itemId);

            // 重新加载数据
            ItemUseReward::LoadFromDB();
        }
        else
        {
            handler->PSendSysMessage("删除物品使用奖励配置失败。");
        }

        return true;
    }

    // 显示帮助信息
    static bool HandleHelpCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->PSendSysMessage("物品使用奖励模块命令：");
        handler->PSendSysMessage(".物品使用奖励 帮助 - 显示此帮助信息");
        handler->PSendSysMessage(".物品使用奖励 列表 - 列出所有配置的物品");
        handler->PSendSysMessage(".物品使用奖励 添加 <物品ID> <奖励ID> <GM命令> <是否消耗物品> <描述> - 添加物品使用奖励配置");
        handler->PSendSysMessage(".物品使用奖励 删除 <物品ID> - 删除物品使用奖励配置");
        handler->PSendSysMessage(".物品使用奖励 重载 - 重新加载配置和数据库数据");
        return true;
    }
};

// 添加脚本
void AddItemUseRewardCommandScripts()
{
    new ItemUseRewardCommand();
}

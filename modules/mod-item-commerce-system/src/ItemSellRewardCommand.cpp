#include "ItemSellReward.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Language.h"
#include "Logging/Log.h"
#include "Player.h"
#include "AccountMgr.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;

class ItemSellRewardCommand : public CommandScript
{
public:
    ItemSellRewardCommand() : CommandScript("ItemSellRewardCommand") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable itemSellRewardCommandTable =
        {
            { "帮助",      HandleHelpCommand,         SEC_GAMEMASTER, Console::No },
            { "列表",      HandleListCommand,         SEC_GAMEMASTER, Console::No },
            { "添加",      HandleAddCommand,          SEC_GAMEMASTER, Console::No },
            { "删除",      HandleRemoveCommand,       SEC_GAMEMASTER, Console::No },
            { "重载",      HandleReloadCommand,       SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "物品售卖奖励", itemSellRewardCommandTable },
        };

        return commandTable;
    }

    // 帮助命令
    static bool HandleHelpCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("物品售卖奖励系统命令:");
        handler->SendSysMessage("  .物品售卖奖励 帮助 - 显示此帮助信息");
        handler->SendSysMessage("  .物品售卖奖励 列表 - 列出所有配置的物品");
        handler->SendSysMessage("  .物品售卖奖励 添加 <物品ID> <奖励ID> <描述> - 添加一个物品售卖奖励配置");
        handler->SendSysMessage("  .物品售卖奖励 删除 <物品ID> - 删除一个物品售卖奖励配置");
        handler->SendSysMessage("  .物品售卖奖励 重载 - 重新加载配置和数据库数据");
        return true;
    }

    // 列表命令
    static bool HandleListCommand(ChatHandler* handler, const char* /*args*/)
    {
        auto const& itemList = ItemSellReward::GetAllItems();
        
        if (itemList.empty())
        {
            handler->SendSysMessage("物品售卖奖励系统: 暂无配置的物品");
            return true;
        }

        handler->SendSysMessage("物品售卖奖励系统配置列表:");
        handler->SendSysMessage("----------------------------------------------");
        handler->SendSysMessage("| 物品ID  | 奖励ID  | 描述                 |");
        handler->SendSysMessage("----------------------------------------------");

        for (const auto& pair : itemList)
        {
            const ItemSellRewardInfo& info = pair.second;
            
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(info.entry);
            std::string itemName = itemTemplate ? itemTemplate->Name1 : "未知物品";
            
            handler->PSendSysMessage("| {:7} | {:7} | {} - {} |", 
                info.entry, info.rewardId, itemName, info.comment);
        }

        handler->SendSysMessage("----------------------------------------------");
        handler->PSendSysMessage("共 {} 个物品配置", itemList.size());
        return true;
    }

    // 添加命令
    static bool HandleAddCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("命令用法: .物品售卖奖励 添加 <物品ID> <奖励ID> <描述>");
            return false;
        }

        // 解析参数
        char* itemIdStr = strtok((char*)args, " ");
        char* rewardIdStr = strtok(NULL, " ");
        char* commentStr = strtok(NULL, "\0");

        if (!itemIdStr || !rewardIdStr)
        {
            handler->SendSysMessage("命令用法: .物品售卖奖励 添加 <物品ID> <奖励ID> <描述>");
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 rewardId = atoi(rewardIdStr);
        std::string comment = commentStr ? commentStr : "";

        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("物品ID {} 不存在", itemId);
            return false;
        }

        // 添加物品到数据库
        if (ItemSellReward::AddItemToDatabase(itemId, rewardId, comment))
        {
            handler->PSendSysMessage("物品 {} [{}] 的售卖奖励配置已添加", itemId, itemTemplate->Name1);
            return true;
        }
        else
        {
            handler->PSendSysMessage("添加物品售卖奖励配置失败");
            return false;
        }
    }

    // 删除命令
    static bool HandleRemoveCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("命令用法: .物品售卖奖励 删除 <物品ID>");
            return false;
        }

        uint32 itemId = atoi(args);
        
        // 检查物品是否在配置中
        if (!ItemSellReward::HasItem(itemId))
        {
            handler->PSendSysMessage("物品ID {} 不在售卖奖励配置中", itemId);
            return false;
        }

        // 从数据库删除
        if (ItemSellReward::RemoveItemFromDatabase(itemId))
        {
            handler->PSendSysMessage("物品ID {} 的售卖奖励配置已删除", itemId);
            return true;
        }
        else
        {
            handler->PSendSysMessage("删除物品售卖奖励配置失败");
            return false;
        }
    }

    // 重载命令
    static bool HandleReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        ItemSellReward::LoadConfig(true);
        ItemSellReward::LoadFromDB();
        handler->SendSysMessage("物品售卖奖励系统配置已重新加载");
        return true;
    }
};

// 添加命令脚本
void AddItemSellRewardCommandScripts()
{
    new ItemSellRewardCommand();
} 
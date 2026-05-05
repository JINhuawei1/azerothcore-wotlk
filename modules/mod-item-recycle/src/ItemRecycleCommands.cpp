#include "ItemRecycle.h"
#include "Log.h"
#include "GameTime.h"
#include "ChatCommandTags.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <sstream>
#include <limits>

namespace
{
    constexpr char const* ITEM_RECYCLE_ADDON_PREFIX = "ITEMRECYCLE";

    void AddCopperReward(Player* player, uint64 amount)
    {
        if (amount <= static_cast<uint64>(std::numeric_limits<int64>::max()))
        {
            player->ModifyMoney(static_cast<int64>(amount));
            return;
        }

        uint64 currentMoney = player->GetMoney();
        player->SetMoney(amount > uint64(MAX_MONEY_AMOUNT) - currentMoney ? uint64(MAX_MONEY_AMOUNT) : currentMoney + amount);
    }

    void SendRecycleAddonMessage(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        std::string fullMessage = std::string(ITEM_RECYCLE_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

// 外部全局变量声明
namespace ItemRecycleGlobals
{
    extern bool bItemRecycleInitialized;
    extern bool bItemRecycleEnabled;
    extern bool bItemRecycleDebugMode;
    // 【审计修复】添加配置变量声明
    extern uint32 defaultRecycleInterval;
    extern uint32 minRecycleInterval;
    extern uint32 maxRecycleInterval;
    extern bool allowCustomSettings;
}

// 调试输出函数
void DebugLog(const std::string& message)
{
    if (ItemRecycleGlobals::bItemRecycleDebugMode)
    {
        LOG_INFO("module.itemrecycle", "[回收系统调试] {}", message);
    }
}

// 检查模块初始化状态的辅助函数
bool CheckRecycleModuleInit(ChatHandler* handler)
{
    if (!ItemRecycleGlobals::bItemRecycleInitialized)
    {
        handler->SendSysMessage("物品回收模块尚未初始化，请稍后再试。");
        handler->SetSentErrorMessage(true);
        return false;
    }
    return true;
}



// 主命令处理器 - 现在只显示帮助信息，子命令由系统自动分发
bool HandleRecycleMainCommand(ChatHandler* handler)
{
    if (!CheckRecycleModuleInit(handler))
        return false;

    handler->SendSysMessage("|cff00ff00=== 物品回收系统命令帮助 ===|r");
    handler->SendSysMessage("用法: .回收 回收 [组ID] - 按组回收物品");
    handler->SendSysMessage("用法: .回收 回收 all - 回收所有启用的组");
    handler->SendSysMessage("用法: .回收 回收 1,2,3 - 回收指定的多个组");
    handler->SendSysMessage("用法: .回收 界面 - 获取界面数据");
    handler->SendSysMessage("用法: .回收 自动 [开启/关闭] - 切换自动回收");
    handler->SendSysMessage("用法: .回收 设置 [选项] - 设置回收参数");
    handler->SendSysMessage("用法: .回收 状态 - 查看回收状态");
    handler->SendSysMessage("用法: .回收 执行 - 执行UI回收（客户端调用）");
    handler->SendSysMessage("用法: .回收 测试 - 测试服务器端功能");
    handler->SendSysMessage("用法: .回收 过滤 [添加/删除/列表] [物品ID] - 管理过滤物品");
    handler->SendSysMessage("用法: .回收 更新配置 [配置数据] - 客户端配置更新（内部命令）");
    handler->SendSysMessage("|cff00ff00客户端UI命令由插件自动发送|r");
    return true;
}

// 按组回收命令 - 支持单组或多组
bool HandleRecycleGroupCommand(ChatHandler* handler, Acore::ChatCommands::Tail args)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    if (args.empty())
    {
        handler->SendSysMessage("用法: .回收 回收 [组ID] - 按组回收物品");
        handler->SendSysMessage("用法: .回收 回收 all - 回收所有启用的组");
        handler->SendSysMessage("用法: .回收 回收 1,2,3 - 回收指定的多个组");
        handler->SetSentErrorMessage(true);
        return false;
    }

    std::string argsStr(args);
    std::vector<uint32> groupIds;

    // 处理特殊命令 "all"
    if (argsStr == "all")
    {
        // 获取玩家设置，回收所有启用的组
        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

        for (int i = 1; i <= 6; ++i)
        {
            if (settings.recycleTypes[i])
            {
                groupIds.push_back(i);
            }
        }

        if (groupIds.empty())
        {
            handler->SendSysMessage("没有启用的回收类型，请先设置回收类型");
            return false;
        }
    }
    else
    {
        // 解析组ID列表（支持逗号分隔）
        std::istringstream iss(argsStr);
        std::string token;

        while (std::getline(iss, token, ','))
        {
            // 去除空格
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            try {
                uint32 groupId = static_cast<uint32>(std::stoi(token));
                if (groupId >= 1 && groupId <= 6)
                {
                    groupIds.push_back(groupId);
                }
                else
                {
                    handler->PSendSysMessage("无效的组ID: {}，组ID必须在1-6之间", groupId);
                    return false;
                }
            }
            catch (std::exception&) {
                handler->PSendSysMessage("无效的组ID格式: {}", token);
                return false;
            }
        }
    }

    if (groupIds.empty())
    {
        handler->SendSysMessage("请指定有效的组ID");
        return false;
    }

    // 收集所有指定组的物品
    std::vector<ItemRecycleInfo> allGroupItems;
    const auto& itemStore = ItemRecycleScript::GetItemRecycleStore();

    for (uint32 groupId : groupIds)
    {
        for (const auto& item : itemStore)
        {
            if (item.group == groupId)
            {
                allGroupItems.push_back(item);
            }
        }
    }

    if (allGroupItems.empty())
    {
        handler->SendSysMessage("找不到指定组的回收物品");
        return false;
    }

    // 检查玩家是否有足够的物品
    std::vector<ItemRecycleInfo> availableItems;
    for (const auto& recycleItem : allGroupItems)
    {
        if (ItemRecycleScript::IsProtectedQuestItem(recycleItem.itemId))
            continue;

        uint32 count = player->GetItemCount(recycleItem.itemId, false);
        if (count >= 1) // 简化：只要有物品就可以回收
        {
            availableItems.push_back(recycleItem);
        }
    }

    if (availableItems.empty())
    {
        handler->SendSysMessage("没有足够的物品可以回收");
        return false;
    }

    // 执行回收
    std::map<uint32, uint32> rewardTemplateCount; // 奖励模板ID -> 数量
    uint64 totalCopperReward = 0; // 总铜币奖励
    uint32 destroyedCount = 0; // 摧毁的物品数量
    uint32 totalCount = 0;
    std::map<uint32, uint32> groupCounts; // 记录每组回收的物品数量

    for (const auto& recycleItem : availableItems)
    {
        uint32 count = player->GetItemCount(recycleItem.itemId, false);
        uint32 recycleCount = count; // 回收所有物品

        if (recycleCount > 0)
        {
            // 获取物品模板
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(recycleItem.itemId);
            if (!itemTemplate)
                continue;

            uint32 totalItemsToDestroy = recycleCount;
            player->DestroyItemCount(recycleItem.itemId, totalItemsToDestroy, true);

            // 处理奖励逻辑
            if (recycleItem.rewardTemplateId > 0)
            {
                // 使用奖励模板
                rewardTemplateCount[recycleItem.rewardTemplateId] += totalItemsToDestroy;
            }
            else
            {
                // 回收奖励为0，检查商店价格
                if (itemTemplate->SellPrice > 0)
                {
                    // 按商店价格给奖励（铜币）
                    totalCopperReward += itemTemplate->SellPrice * totalItemsToDestroy;
                }
                else
                {
                    // 商店不能出售，摧毁
                    destroyedCount += totalItemsToDestroy;
                }
            }

            totalCount += totalItemsToDestroy;
            groupCounts[recycleItem.group] += totalItemsToDestroy;
        }
    }

    // 发放奖励
    if (totalCount > 0)
    {
        // 发放铜币奖励
        if (totalCopperReward > 0)
        {
            AddCopperReward(player, totalCopperReward);
        }

        // 发放奖励模板奖励（禁用单条消息，最后统一发送汇总消息）
        std::map<std::string, uint32> rewardSummary; // 基础描述 -> 总数量
        if (RewardInterface* rewardModule = sModuleManager->GetRewardModule())
        {
            for (const auto& pair : rewardTemplateCount)
            {
                uint32 rewardTemplateId = pair.first;
                uint32 count = pair.second;

                // 获取奖励描述用于汇总
                std::vector<std::string> rewardDesc = rewardModule->GetRewardDescription(player, rewardTemplateId);
                
                // 根据数量发放奖励，禁用单条消息通知
                for (uint32 i = 0; i < count; ++i)
                {
                    rewardModule->GiveReward(player, rewardTemplateId, true, false);
                }
                
                // 汇总奖励信息（解析数量并累加）
                for (const auto& desc : rewardDesc)
                {
                    std::string baseDesc = desc;
                    uint32 singleCount = 1;
                    
                    // 查找 " x" 后面的数字
                    size_t xPos = desc.rfind(" x");
                    if (xPos != std::string::npos && xPos + 2 < desc.length())
                    {
                        std::string numStr = desc.substr(xPos + 2);
                        bool isNumber = !numStr.empty();
                        for (char c : numStr)
                        {
                            if (!std::isdigit(c)) { isNumber = false; break; }
                        }
                        if (isNumber)
                        {
                            baseDesc = desc.substr(0, xPos);
                            singleCount = std::stoul(numStr);
                        }
                    }
                    rewardSummary[baseDesc] += singleCount * count;
                }
            }
        }

        // 显示回收结果
        std::string rewardMessage = "";
        if (totalCopperReward > 0)
        {
            // 将铜币转换为金银铜格式显示
            uint64 gold = totalCopperReward / 10000;
            uint64 silver = (totalCopperReward % 10000) / 100;
            uint64 copper = totalCopperReward % 100;

            std::string moneyStr = "";
            if (gold > 0)
                moneyStr += std::to_string(gold) + "金";
            if (silver > 0)
                moneyStr += std::to_string(silver) + "银";
            if (copper > 0)
                moneyStr += std::to_string(copper) + "铜";

            if (!moneyStr.empty())
                rewardMessage += "，获得" + moneyStr;
        }
        if (destroyedCount > 0)
        {
            rewardMessage += "，摧毁" + std::to_string(destroyedCount) + "个无价值物品";
        }

        if (groupIds.size() == 1)
        {
            handler->PSendSysMessage("|cff00ff00[手动回收]|r 组{} 回收了 |cffff0000{}|r 个物品{}",
                groupIds[0], totalCount, rewardMessage);
        }
        else
        {
            handler->PSendSysMessage("|cff00ff00[多组回收]|r 总共回收了 |cffff0000{}|r 个物品{}",
                totalCount, rewardMessage);

            // 显示每组的详细信息
            for (const auto& pair : groupCounts)
            {
                if (pair.second > 0)
                {
                    const char* groupNames[] = {"", "装备", "消耗品", "任务物品", "垃圾", "宝石", "附魔材料"};
                    handler->PSendSysMessage("  组{} ({}): |cffff0000{}|r 个物品",
                        pair.first, groupNames[pair.first], pair.second);
                }
            }
        }
        
        // 发送奖励汇总消息
        if (!rewardSummary.empty())
        {
            for (const auto& summary : rewardSummary)
            {
                handler->PSendSysMessage("  获得：{} x{}", summary.first, summary.second);
            }
        }
    }

    return true;
}

// 打开回收界面命令
bool HandleRecycleUICommand(ChatHandler* handler)
{
    Player* player = handler->GetPlayer();
    if (!player)
    {
        DebugLog("HandleRecycleUICommand: 玩家对象为空");
        return false;
    }

    // 强制初始化检查和修复
    if (!ItemRecycleGlobals::bItemRecycleInitialized)
    {
        handler->SendSysMessage("|cffff8800[回收系统]|r 模块正在初始化，正在尝试强制初始化...");

        // 强制设置启用状态
        ItemRecycleGlobals::bItemRecycleEnabled = true;

        // 尝试加载数据
        try
        {
            ItemRecycleScript::LoadItemRecycleFromDB();
            ItemRecycleScript::LoadPlayerRecycleSettings();
            ItemRecycleGlobals::bItemRecycleInitialized = true;
            handler->SendSysMessage("|cff00ff00[回收系统]|r 模块初始化成功");
        }
        catch (std::exception& e)
        {
            handler->PSendSysMessage("|cffff0000[回收系统]|r 初始化失败: {}", e.what());
            return false;
        }
    }

    // 发送UI数据到客户端
    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    DebugLog("HandleRecycleUICommand: 开始构建UI数据，玩家GUID=" + std::to_string(playerGuid));

    std::ostringstream ss;
    ss << "RECYCLE_UI_DATA:";
    ss << (settings.autoRecycleEnabled ? 1 : 0) << ":";
    ss << settings.recycleInterval << ":";
    for (int i = 1; i <= 6; ++i)
        ss << (settings.recycleTypes[i] ? 1 : 0) << ":";
    ss << (settings.recycleTypes[7] ? 1 : 0) << ":"; // 添加"所有类型"开关状态
    ss << settings.minQuality << ":";
    ss << settings.maxQuality << ":";
    ss << settings.minLevel << ":";
    ss << settings.maxLevel << ":";
    ss << (settings.protectEquipped ? 1 : 0) << ":";

    // 添加过滤物品列表
    if (!settings.filteredItems.empty())
    {
        bool first = true;
        for (uint32 itemId : settings.filteredItems)
        {
            if (!first) ss << ",";
            ss << itemId;
            first = false;
        }
    }
    else
    {
        ss << "6948"; // 默认炉石
    }

    std::string uiData = ss.str();
    DebugLog("HandleRecycleUICommand: UI数据构建完成，长度=" + std::to_string(uiData.length()));
    DebugLog("HandleRecycleUICommand: UI数据内容=" + uiData);

    // 通过 Addon 通道发送 UI 数据和打开界面请求
    SendRecycleAddonMessage(player, uiData);
    SendRecycleAddonMessage(player, "OPEN_UI");

    // 发送调试信息
    if (ItemRecycleGlobals::bItemRecycleDebugMode)
    {
        handler->SendSysMessage("|cff00ff00[回收系统]|r 当前设置:");
        const char* typeNames[] = {"", "装备", "消耗品", "任务物品", "垃圾", "宝石", "附魔材料"};
        for (int i = 1; i <= 6; ++i)
        {
            handler->PSendSysMessage("  {}: {}", typeNames[i], settings.recycleTypes[i] ? "开启" : "关闭");
        }
        handler->PSendSysMessage("  品质范围: {}-{}", settings.minQuality, settings.maxQuality);
        handler->PSendSysMessage("  等级范围: {}-{}", settings.minLevel, settings.maxLevel);
        handler->PSendSysMessage("  保护装备: {}", settings.protectEquipped ? "开启" : "关闭");
    }

    return true;
}

// 自动回收开关命令
bool HandleRecycleAutoCommand(ChatHandler* handler, Acore::ChatCommands::Tail args)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    bool wasEnabled = settings.autoRecycleEnabled;

    if (args.empty())
    {
        // 切换状态
        settings.autoRecycleEnabled = !settings.autoRecycleEnabled;
    }
    else
    {
        std::string argStr(args);
        if (argStr == "开启" || argStr == "on" || argStr == "1")
            settings.autoRecycleEnabled = true;
        else if (argStr == "关闭" || argStr == "off" || argStr == "0")
            settings.autoRecycleEnabled = false;
        else
        {
            handler->SendSysMessage("用法: .回收 自动 [开启/关闭]");
            return false;
        }
    }

    // 如果从关闭状态切换到开启状态，初始化时间
    if (!wasEnabled && settings.autoRecycleEnabled)
    {
        settings.lastRecycleTime = GameTime::GetGameTime().count();
        handler->PSendSysMessage("|cff00ff00[自动回收]|r 已开启，将在 {}秒 后开始第一次自动回收", settings.recycleInterval);
    }
    else
    {
        handler->PSendSysMessage("|cff00ff00[自动回收]|r 已{}",
            settings.autoRecycleEnabled ? "|cff00ff00开启|r" : "|cffff0000关闭|r");
    }

    ItemRecycleScript::SavePlayerRecycleSettings(settings);
    
    return true;
}

// 回收设置命令
bool HandleRecycleSetCommand(ChatHandler* handler, Acore::ChatCommands::Tail args)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    if (args.empty())
    {
        handler->SendSysMessage("用法: .回收 设置 间隔 [秒数] - 设置自动回收间隔");
        handler->SendSysMessage("用法: .回收 设置 类型 [1-6] [开启/关闭] - 设置回收类型");
        handler->SendSysMessage("用法: .回收 设置 类型 all [开启/关闭] - 新增：设置回收所有类型");
        handler->SendSysMessage("用法: .回收 设置 品质 [最小] [最大] - 设置回收品质范围");
        handler->SendSysMessage("用法: .回收 设置 等级 [最小] [最大] - 设置回收等级范围");
        handler->SendSysMessage("用法: .回收 设置 保护装备 [开启/关闭] - 设置是否保护已装备物品");
        handler->SendSysMessage("注意: 需求模板和奖励模板在全局配置中设置");
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    std::string argsStr(args);
    std::istringstream iss(argsStr);
    std::string subCommand;
    iss >> subCommand;

    if (subCommand == "间隔")
    {
        // 【审计修复】检查是否允许自定义设置
        if (!ItemRecycleGlobals::allowCustomSettings)
        {
            handler->SendSysMessage("服务器已禁止玩家自定义回收设置");
            return false;
        }

        uint32 interval;
        // 【审计修复】使用配置的间隔范围
        if (!(iss >> interval) || interval < ItemRecycleGlobals::minRecycleInterval || interval > ItemRecycleGlobals::maxRecycleInterval)
        {
            handler->PSendSysMessage("间隔时间必须在{}-{}秒之间",
                ItemRecycleGlobals::minRecycleInterval, ItemRecycleGlobals::maxRecycleInterval);
            return false;
        }
        settings.recycleInterval = interval;

        // 如果自动回收已启用，重新初始化时间
        if (settings.autoRecycleEnabled)
        {
            settings.lastRecycleTime = GameTime::GetGameTime().count();
            handler->PSendSysMessage("自动回收间隔设置为: {}秒，将在 {}秒 后开始下一次回收", interval, interval);
        }
        else
        {
            handler->PSendSysMessage("自动回收间隔设置为: {}秒", interval);
        }
    }
    else if (subCommand == "类型")
    {
        std::string typeParam;
        std::string status;
        if (!(iss >> typeParam >> status))
        {
            handler->SendSysMessage("请指定类型和状态");
            return false;
        }

        bool enabled = (status == "开启" || status == "on" || status == "1");

        // 新增：支持"所有类型"一键开关
        if (typeParam == "all" || typeParam == "所有")
        {
            settings.recycleTypes[7] = enabled; // 索引7对应RECYCLE_TYPE_ALL(-1)
            handler->PSendSysMessage("回收所有类型已{}", enabled ? "开启" : "关闭");
        }
        else
        {
            // 处理具体类型设置
            uint32 typeId = std::stoul(typeParam);
            if (typeId < 1 || typeId > 6)
            {
                handler->SendSysMessage("类型ID必须在1-6之间，或使用'all'表示所有类型 (1=装备 2=消耗品 3=任务物品 4=垃圾 5=宝石 6=附魔材料)");
                return false;
            }

            settings.recycleTypes[typeId] = enabled;

            const char* typeNames[] = {"", "装备", "消耗品", "任务物品", "垃圾", "宝石", "附魔材料"};
            handler->PSendSysMessage("{}回收已{}", typeNames[typeId], enabled ? "开启" : "关闭");
        }
    }
    else if (subCommand == "品质")
    {
        uint32 minQuality, maxQuality;
        if (!(iss >> minQuality >> maxQuality) || minQuality > 6 || maxQuality > 6 || minQuality > maxQuality)
        {
            handler->SendSysMessage("品质范围必须在0-6之间，且最小值不能大于最大值");
            handler->SendSysMessage("品质说明: 0=灰色 1=白色 2=绿色 3=蓝色 4=紫色 5=橙色 6=红色");
            return false;
        }
        settings.minQuality = minQuality;
        settings.maxQuality = maxQuality;
        handler->PSendSysMessage("回收品质范围设置为: {}-{}", minQuality, maxQuality);
    }
    else if (subCommand == "等级")
    {
        uint32 minLevel, maxLevel;
        if (!(iss >> minLevel >> maxLevel) || minLevel < 1 || minLevel > maxLevel)
        {
            handler->SendSysMessage("等级范围：最小值不能小于1，且不能大于最大值");
            return false;
        }

        // 支持无限等级范围，无上限限制
        settings.minLevel = minLevel;
        settings.maxLevel = maxLevel;
        handler->PSendSysMessage("回收等级范围设置为: {}-{} (支持无限等级)", minLevel, maxLevel);

        // 调试信息
        if (ItemRecycleGlobals::bItemRecycleDebugMode)
        {
            handler->PSendSysMessage("|cff00ff00[等级设置]|r 等级范围已更新: {}-{}", minLevel, maxLevel);
        }
    }
    else if (subCommand == "保护装备")
    {
        std::string status;
        if (!(iss >> status))
        {
            handler->SendSysMessage("请指定开启或关闭");
            return false;
        }
        
        bool protect = (status == "开启" || status == "on" || status == "1");
        settings.protectEquipped = protect;
        handler->PSendSysMessage("保护已装备物品已{}", protect ? "开启" : "关闭");
    }
    // 注意：需求模板和奖励模板在全局配置中设置，玩家无法直接修改
    else
    {
        handler->SendSysMessage("未知的设置选项");
        return false;
    }

    ItemRecycleScript::SavePlayerRecycleSettings(settings);
    return true;
}

// 查看回收状态命令
bool HandleRecycleStatusCommand(ChatHandler* handler)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    handler->SendSysMessage("|cff00ff00=== 回收系统状态 ===|r");
    handler->PSendSysMessage("自动回收: {}", settings.autoRecycleEnabled ? "|cff00ff00开启|r" : "|cffff0000关闭|r");
    handler->PSendSysMessage("回收间隔: {}秒", settings.recycleInterval);
    
    handler->SendSysMessage("回收类型:");
    // 新增：显示"所有类型"开关状态
    handler->PSendSysMessage("  所有类型: {}", settings.recycleTypes[7] ? "|cff00ff00开启|r" : "|cffff0000关闭|r");
    // 如果没有启用"所有类型"，则显示具体类型状态
    if (!settings.recycleTypes[7])
    {
        const char* typeNames[] = {"", "装备", "消耗品", "任务物品", "垃圾", "宝石", "附魔材料"};
        for (int i = 1; i <= 6; ++i)
        {
            handler->PSendSysMessage("  {}: {}", typeNames[i], settings.recycleTypes[i] ? "|cff00ff00开启|r" : "|cffff0000关闭|r");
        }
    }

    handler->PSendSysMessage("品质范围: {}-{}", settings.minQuality, settings.maxQuality);
    handler->PSendSysMessage("等级范围: {}-{}", settings.minLevel, settings.maxLevel);
    handler->PSendSysMessage("保护装备: {}", settings.protectEquipped ? "|cff00ff00开启|r" : "|cffff0000关闭|r");
    // 注意：需求模板和奖励模板在全局配置中定义
    handler->SendSysMessage("模板系统: 在全局配置中定义");
    
    uint32 currentTime = GameTime::GetGameTime().count();
    uint32 nextRecycle = settings.lastRecycleTime + settings.recycleInterval;
    if (settings.autoRecycleEnabled && currentTime < nextRecycle)
    {
        handler->PSendSysMessage("下次自动回收: {}秒后", nextRecycle - currentTime);
    }

    return true;
}

// 测试命令处理
bool HandleRecycleTestCommand(ChatHandler* handler)
{
    // 测试命令不需要模块完全初始化，用于诊断问题
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    handler->SendSysMessage("|cff00ff00[回收系统测试]|r 开始测试服务器端命令处理...");

    // 测试各种命令格式
    handler->SendSysMessage("测试命令格式:");
    handler->SendSysMessage("  .回收 界面 - 获取界面数据");
    handler->SendSysMessage("  .回收 状态 - 查看当前状态");
    handler->SendSysMessage("  .回收 自动 开启 - 开启自动回收");
    handler->SendSysMessage("  .回收 设置 类型 4 开启 - 开启垃圾回收");

    // 显示当前模块状态
    handler->PSendSysMessage("模块初始化状态: {}",
        ItemRecycleGlobals::bItemRecycleInitialized ? "已初始化" : "未初始化");
    handler->PSendSysMessage("模块启用状态: {}",
        ItemRecycleGlobals::bItemRecycleEnabled ? "已启用" : "未启用");
    handler->PSendSysMessage("调试模式: {}",
        ItemRecycleGlobals::bItemRecycleDebugMode ? "开启" : "关闭");

    // 显示数据库记录数量
    handler->PSendSysMessage("回收配置记录数: {}", ItemRecycleScript::GetItemRecycleStoreSize());
    handler->PSendSysMessage("玩家设置记录数: {}", ItemRecycleScript::GetPlayerSettingsSize());

    // 测试数据库表是否存在
    try
    {
        QueryResult result = CharacterDatabase.Query("SELECT COUNT(*) FROM `物品_回收玩家配置`");
        if (result)
        {
            Field* fields = result->Fetch();
            uint32 count = fields[0].Get<uint32>();
            handler->PSendSysMessage("数据库表状态: 正常，共 {} 条记录", count);
        }
        else
        {
            handler->SendSysMessage("数据库表状态: 查询失败");
        }
    }
    catch (std::exception& e)
    {
        handler->PSendSysMessage("数据库表状态: 错误 - {}", e.what());
    }

    // 测试保存设置
    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);
    handler->SendSysMessage("正在测试保存设置...");
    ItemRecycleScript::SavePlayerRecycleSettings(settings);
    handler->SendSysMessage("保存设置测试完成");

    handler->SendSysMessage("|cff00ff00[回收系统测试]|r 测试完成");

    return true;
}

// 执行回收命令 - 专门用于客户端UI调用
bool HandleRecycleExecuteCommand(ChatHandler* handler)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    // 获取玩家当前设置
    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    // 新增：检查回收类型权限（支持"所有类型"开关）
    bool hasEnabledTypes = settings.recycleTypes[7]; // 检查是否启用"所有类型"开关
    if (!hasEnabledTypes)
    {
        // 如果没有启用"所有类型"，检查是否有具体类型启用
        for (int i = 1; i <= 6; ++i)
        {
            if (settings.recycleTypes[i])
            {
                hasEnabledTypes = true;
                break;
            }
        }
    }

    if (!hasEnabledTypes)
    {
        handler->SendSysMessage("|cffff0000[回收失败]|r 没有启用的回收类型");
        return false;
    }

    // 使用与自动回收相同的逻辑
    std::vector<Item*> itemsToRecycle;
    uint32 totalReward = 0;
    uint32 totalCount = 0;
    std::map<uint32, uint32> groupCounts;
    std::map<uint32, std::string> groupNames = {
        {1, "装备"}, {2, "消耗品"}, {3, "任务物品"},
        {4, "垃圾"}, {5, "宝石"}, {6, "附魔材料"}
    };

    // 遍历背包中的物品
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && ItemRecycleScript::CanRecycleItem(player, item, settings))
        {
            itemsToRecycle.push_back(item);
        }
    }

    // 遍历背包袋中的物品
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && ItemRecycleScript::CanRecycleItem(player, item, settings))
                {
                    itemsToRecycle.push_back(item);
                }
            }
        }
    }

    if (itemsToRecycle.empty())
    {
        handler->SendSysMessage("|cffff0000[回收结果]|r 没有符合条件的物品可以回收");
        return true;
    }

    // 【审计修复】收集需要检查的需求模板ID和对应的物品数量
    std::map<uint32, uint32> requirementTemplateCounts; // 模板ID -> 需要消耗的次数
    std::map<Item*, const ItemRecycleInfo*> itemRecycleRules;

    // 为每个物品查找匹配的回收规则，并按物品数量累加需求次数
    for (Item* item : itemsToRecycle)
    {
        if (!item)
            continue;

        const ItemRecycleInfo* recycleRule = ItemRecycleScript::FindMatchingRecycleRule(item, settings);
        if (recycleRule)
        {
            itemRecycleRules[item] = recycleRule;

            // 如果有需求模板，按物品数量累加需求次数
            if (recycleRule->requirementTemplateId > 0)
            {
                requirementTemplateCounts[recycleRule->requirementTemplateId] += item->GetCount();
            }
        }
    }

    // 检查和消耗需求模板（按物品数量）
    if (RequirementInterface* reqModule = sModuleManager->GetRequirementModule())
    {
        // 先检查所有需求是否满足
        for (const auto& pair : requirementTemplateCounts)
        {
            uint32 requirementTemplateId = pair.first;
            uint32 count = pair.second;

            // 检查是否能满足指定次数的需求
            for (uint32 i = 0; i < count; ++i)
            {
                if (!reqModule->CheckRequirements(player, requirementTemplateId, false))
                {
                    // 获取需求详细信息并显示
                    std::string requirementDetails = GetRequirementDetails(requirementTemplateId);
                    if (!requirementDetails.empty())
                    {
                        handler->PSendSysMessage("|cffff0000[回收失败]|r {}（需要{}次，仅满足{}次）", requirementDetails, count, i);
                    }
                    else
                    {
                        handler->PSendSysMessage("|cffff0000[回收失败]|r 不满足回收需求（需要{}次，仅满足{}次）", count, i);
                    }
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }
        }

        // 消耗所有需求（按次数消耗）
        for (const auto& pair : requirementTemplateCounts)
        {
            uint32 requirementTemplateId = pair.first;
            uint32 count = pair.second;

            for (uint32 i = 0; i < count; ++i)
            {
                if (!reqModule->ConsumeRequirements(player, requirementTemplateId))
                {
                    handler->PSendSysMessage("|cffff0000[回收失败]|r 消耗需求失败（第{}次）", i + 1);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }
        }
    }

    // 执行回收并收集奖励
    std::map<uint32, uint32> rewardTemplateCount; // 奖励模板ID -> 数量
    uint64 totalCopperReward = 0; // 总铜币奖励
    uint32 destroyedCount = 0; // 摧毁的物品数量

    for (Item* item : itemsToRecycle)
    {
        if (!item)
            continue;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            continue;

        uint32 count = item->GetCount();

        // 确定物品类型用于统计
        uint32 recycleType = RECYCLE_TYPE_JUNK; // 默认垃圾类型
        switch (itemTemplate->Class)
        {
            case ITEM_CLASS_WEAPON:
            case ITEM_CLASS_ARMOR:
                recycleType = RECYCLE_TYPE_EQUIPMENT;
                break;
            case ITEM_CLASS_CONSUMABLE:
                recycleType = RECYCLE_TYPE_CONSUMABLE;
                break;
            case ITEM_CLASS_QUEST:
                recycleType = RECYCLE_TYPE_QUEST;
                break;
            case ITEM_CLASS_GEM:
                recycleType = RECYCLE_TYPE_GEM;
                break;
            case ITEM_CLASS_TRADE_GOODS:
                if (itemTemplate->SubClass == ITEM_SUBCLASS_ENCHANTING)
                    recycleType = RECYCLE_TYPE_ENCHANT;
                else
                    recycleType = RECYCLE_TYPE_JUNK;
                break;
            default:
                recycleType = RECYCLE_TYPE_JUNK;
                break;
        }

        // 检查是否有匹配的回收规则并处理奖励
        auto ruleIt = itemRecycleRules.find(item);
        if (ruleIt != itemRecycleRules.end() && ruleIt->second->rewardTemplateId > 0)
        {
            // 使用奖励模板
            rewardTemplateCount[ruleIt->second->rewardTemplateId] += count;
        }
        else
        {
            // 回收奖励为0，检查商店价格
            if (itemTemplate->SellPrice > 0)
            {
                // 按商店价格给奖励（铜币）
                totalCopperReward += itemTemplate->SellPrice * count;
            }
            else
            {
                // 商店不能出售，摧毁
                destroyedCount += count;
            }
        }

        totalCount += count;
        groupCounts[recycleType] += count;

        // 销毁物品
        player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
    }

    // 发放铜币奖励
    if (totalCopperReward > 0)
    {
        AddCopperReward(player, totalCopperReward);
    }

    // 发放奖励模板奖励（禁用单条消息，最后统一发送汇总消息）
    std::map<std::string, uint32> rewardSummary; // 基础描述 -> 总数量
    if (RewardInterface* rewardModule = sModuleManager->GetRewardModule())
    {
        for (const auto& pair : rewardTemplateCount)
        {
            uint32 rewardTemplateId = pair.first;
            uint32 count = pair.second;

            // 获取奖励描述用于汇总
            std::vector<std::string> rewardDesc = rewardModule->GetRewardDescription(player, rewardTemplateId);
            
            // 根据数量发放奖励，禁用单条消息通知
            for (uint32 i = 0; i < count; ++i)
            {
                rewardModule->GiveReward(player, rewardTemplateId, true, false);
            }
            
            // 汇总奖励信息（解析数量并累加）
            for (const auto& desc : rewardDesc)
            {
                std::string baseDesc = desc;
                uint32 singleCount = 1;
                
                // 查找 " x" 后面的数字
                size_t xPos = desc.rfind(" x");
                if (xPos != std::string::npos && xPos + 2 < desc.length())
                {
                    std::string numStr = desc.substr(xPos + 2);
                    bool isNumber = !numStr.empty();
                    for (char c : numStr)
                    {
                        if (!std::isdigit(c)) { isNumber = false; break; }
                    }
                    if (isNumber)
                    {
                        baseDesc = desc.substr(0, xPos);
                        singleCount = std::stoul(numStr);
                    }
                }
                rewardSummary[baseDesc] += singleCount * count;
            }
        }
    }

    // 显示回收结果消息
    std::string rewardMessage = "";
    if (totalCopperReward > 0)
    {
        // 将铜币转换为金银铜格式显示
        uint64 gold = totalCopperReward / 10000;
        uint64 silver = (totalCopperReward % 10000) / 100;
        uint64 copper = totalCopperReward % 100;

        std::string moneyStr = "";
        if (gold > 0)
            moneyStr += std::to_string(gold) + "金";
        if (silver > 0)
            moneyStr += std::to_string(silver) + "银";
        if (copper > 0)
            moneyStr += std::to_string(copper) + "铜";

        if (!moneyStr.empty())
            rewardMessage += "，获得" + moneyStr;
    }
    if (destroyedCount > 0)
    {
        rewardMessage += "，摧毁" + std::to_string(destroyedCount) + "个无价值物品";
    }

    handler->PSendSysMessage("|cff00ff00[UI回收]|r 总共回收了 |cffff0000{}|r 个物品{}",
        totalCount, rewardMessage);

    // 显示每组的详细信息
    for (const auto& pair : groupCounts)
    {
        if (pair.second > 0)
        {
            handler->PSendSysMessage("  {} (类型{}): |cffff0000{}|r 个物品",
                groupNames[pair.first], pair.first, pair.second);
        }
    }
    
    // 发送奖励汇总消息
    if (!rewardSummary.empty())
    {
        for (const auto& summary : rewardSummary)
        {
            handler->PSendSysMessage("  获得：{} x{}", summary.first, summary.second);
        }
    }

    return true;
}

// 过滤物品管理命令
bool HandleRecycleFilterCommand(ChatHandler* handler, Acore::ChatCommands::Tail args)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    if (args.empty())
    {
        handler->SendSysMessage("用法: .回收 过滤 添加 [物品ID] - 添加物品到过滤列表");
        handler->SendSysMessage("用法: .回收 过滤 删除 [物品ID] - 从过滤列表删除物品");
        handler->SendSysMessage("用法: .回收 过滤 列表 - 查看当前过滤列表");
        handler->SendSysMessage("用法: .回收 过滤 清空 - 清空过滤列表（保留炉石）");
        handler->SendSysMessage("注意: 炉石(6948)默认被过滤，无法删除");
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    std::string argsStr(args);
    std::istringstream iss(argsStr);
    std::string subCommand;
    iss >> subCommand;

    if (subCommand == "添加")
    {
        uint32 itemId;
        if (!(iss >> itemId) || itemId == 0)
        {
            handler->SendSysMessage("|cffff0000[过滤管理]|r 请指定有效的物品ID（数字）");
            handler->SendSysMessage("用法: .回收 过滤 添加 [物品ID]");
            handler->SetSentErrorMessage(true); // 防止显示命令用法提示
            return true; // 返回true避免显示错误的命令用法
        }

        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("|cffff0000[过滤管理]|r 物品ID {} 不存在，请检查物品ID是否正确", itemId);
            handler->SetSentErrorMessage(true); // 防止显示命令用法提示
            return true; // 返回true避免显示错误的命令用法
        }

        // 添加到过滤列表
        if (settings.filteredItems.find(itemId) != settings.filteredItems.end())
        {
            handler->PSendSysMessage("|cffff0000[过滤管理]|r 物品 [{}] 已经在过滤列表中，无需重复添加", itemTemplate->Name1);
            handler->SetSentErrorMessage(true); // 防止显示命令用法提示
            return true; // 返回true避免显示错误的命令用法
        }

        settings.filteredItems.insert(itemId);
        ItemRecycleScript::SavePlayerRecycleSettings(settings);

        handler->PSendSysMessage("|cff00ff00[过滤管理]|r 已添加物品 [{}] 到过滤列表", itemTemplate->Name1);
    }
    else if (subCommand == "删除")
    {
        uint32 itemId;
        if (!(iss >> itemId) || itemId == 0)
        {
            handler->SendSysMessage("|cffff0000[过滤管理]|r 请指定有效的物品ID（数字）");
            handler->SendSysMessage("用法: .回收 过滤 删除 [物品ID]");
            handler->SetSentErrorMessage(true); // 防止显示命令用法提示
            return true; // 返回true避免显示错误的命令用法
        }

        // 不允许删除炉石
        if (itemId == 6948)
        {
            handler->SendSysMessage("|cffff0000[过滤管理]|r 炉石是默认过滤物品，无法删除");
            handler->SetSentErrorMessage(true); // 防止显示命令用法提示
            return true; // 返回true避免显示错误的命令用法
        }

        // 从过滤列表删除
        if (settings.filteredItems.find(itemId) == settings.filteredItems.end())
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            std::string itemName = itemTemplate ? itemTemplate->Name1 : std::to_string(itemId);
            handler->PSendSysMessage("|cffff0000[过滤管理]|r 物品 [{}] 不在过滤列表中，无需删除", itemName);
            handler->SetSentErrorMessage(true); // 防止显示命令用法提示
            return true; // 返回true避免显示错误的命令用法
        }

        settings.filteredItems.erase(itemId);
        ItemRecycleScript::SavePlayerRecycleSettings(settings);

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        std::string itemName = itemTemplate ? itemTemplate->Name1 : std::to_string(itemId);
        handler->PSendSysMessage("|cff00ff00[过滤管理]|r 已从过滤列表删除物品 [{}]", itemName);
    }
    else if (subCommand == "列表")
    {
        if (settings.filteredItems.empty())
        {
            handler->SendSysMessage("过滤列表为空");
            return true;
        }

        handler->SendSysMessage("|cff00ff00=== 过滤物品列表 ===|r");
        uint32 count = 0;
        for (uint32 itemId : settings.filteredItems)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            std::string itemName = itemTemplate ? itemTemplate->Name1 : "未知物品";

            if (itemId == 6948)
            {
                handler->PSendSysMessage("{}. [{}] (ID: {}) |cffff0000[默认]|r", ++count, itemName, itemId);
            }
            else
            {
                handler->PSendSysMessage("{}. [{}] (ID: {})", ++count, itemName, itemId);
            }
        }
        handler->PSendSysMessage("总计: {} 个过滤物品", count);
    }
    else if (subCommand == "清空")
    {
        // 清空过滤列表，但保留炉石
        settings.filteredItems.clear();
        settings.filteredItems.insert(6948); // 重新添加炉石
        ItemRecycleScript::SavePlayerRecycleSettings(settings);

        handler->SendSysMessage("|cff00ff00[过滤管理]|r 已清空过滤列表（保留炉石）");
    }
    else
    {
        handler->SendSysMessage("|cffff0000[过滤管理]|r 未知的子命令，请使用: 添加、删除、列表、清空");
        handler->SendSysMessage("用法: .回收 过滤 [添加/删除/列表/清空] [物品ID]");
        handler->SetSentErrorMessage(true); // 防止显示命令用法提示
        return true; // 返回true避免显示错误的命令用法
    }

    return true;
}

// 处理客户端发送的配置更新命令
bool HandleRecycleUpdateConfigCommand(ChatHandler* handler, Acore::ChatCommands::Tail args)
{
    Player* player = handler->GetPlayer();
    if (!player)
    {
        DebugLog("HandleRecycleUpdateConfigCommand: 玩家对象为空");
        return false;
    }

    if (!CheckRecycleModuleInit(handler))
    {
        DebugLog("HandleRecycleUpdateConfigCommand: 模块未初始化");
        return false;
    }

    if (args.empty())
    {
        handler->SendSysMessage("用法: .回收 更新配置 [配置数据] - 客户端配置更新命令");
        handler->SetSentErrorMessage(true);
        return true;
    }

    std::string argsStr(args);
    uint32 playerGuid = player->GetGUID().GetCounter();

    DebugLog("HandleRecycleUpdateConfigCommand: 开始解析配置数据，玩家GUID=" + std::to_string(playerGuid));
    DebugLog("HandleRecycleUpdateConfigCommand: 原始配置数据=" + argsStr);

    // 解析客户端发送的配置数据
    // 格式: autoRecycle:interval:type1:type2:type3:type4:type5:type6:typeAll:minQuality:maxQuality:minLevel:maxLevel:protectEquipped:filteredItems
    std::istringstream iss(argsStr);
    std::string token;
    std::vector<std::string> configParts;

    while (std::getline(iss, token, ':'))
    {
        configParts.push_back(token);
    }

    DebugLog("HandleRecycleUpdateConfigCommand: 解析到 " + std::to_string(configParts.size()) + " 个配置项");

    // 验证配置数据格式
    if (configParts.size() < 14)
    {
        handler->SendSysMessage("|cffff0000[配置更新]|r 配置数据格式错误，期望至少14个参数，实际收到" + std::to_string(configParts.size()) + "个");
        handler->SetSentErrorMessage(true);
        return true;
    }

    try
    {
        // 【审计修复】检查是否允许自定义设置
        if (!ItemRecycleGlobals::allowCustomSettings)
        {
            handler->SendSysMessage("|cffff0000[配置更新]|r 服务器已禁止玩家自定义回收设置");
            handler->SetSentErrorMessage(true);
            return true;
        }

        // 获取当前设置作为基础
        PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

        // 验证配置数据的合理性，防止客户端发送恶意数据
        bool autoRecycle = (configParts[0] == "1");
        uint32 interval = std::stoul(configParts[1]);

        // 【审计修复】使用配置的间隔范围验证
        if (interval < ItemRecycleGlobals::minRecycleInterval || interval > ItemRecycleGlobals::maxRecycleInterval)
        {
            handler->PSendSysMessage("|cffff0000[配置更新]|r 回收间隔必须在{}-{}秒之间",
                ItemRecycleGlobals::minRecycleInterval, ItemRecycleGlobals::maxRecycleInterval);
            handler->SetSentErrorMessage(true);
            return true;
        }

        // 验证品质和等级范围
        uint32 minQuality = std::stoul(configParts[8]);
        uint32 maxQuality = std::stoul(configParts[9]);
        uint32 minLevel = std::stoul(configParts[10]);
        uint32 maxLevel = std::stoul(configParts[11]);

        if (minQuality > 6 || maxQuality > 6 || minQuality > maxQuality)
        {
            handler->SendSysMessage("|cffff0000[配置更新]|r 品质范围设置错误（0-6）");
            handler->SetSentErrorMessage(true);
            return true;
        }

        // 移除等级上限限制，支持无限等级范围
        if (minLevel < 1 || minLevel > maxLevel)
        {
            handler->SendSysMessage("|cffff0000[配置更新]|r 等级范围设置错误（最小等级不能小于1，且不能大于最大等级）");
            handler->SetSentErrorMessage(true);
            return true;
        }

        // 调试信息：显示接收到的等级范围
        if (ItemRecycleGlobals::bItemRecycleDebugMode)
        {
            handler->PSendSysMessage("|cff00ff00[等级验证]|r 接收到等级范围: {}-{}", minLevel, maxLevel);
        }

        // 只有在验证通过后才更新设置
        settings.autoRecycleEnabled = autoRecycle;
        settings.recycleInterval = interval;

        // 更新回收类型设置
        for (int i = 1; i <= 6; ++i)
        {
            settings.recycleTypes[i] = (configParts[i + 1] == "1");
        }
        settings.recycleTypes[7] = (configParts[7] == "1"); // 所有类型开关

        settings.minQuality = minQuality;
        settings.maxQuality = maxQuality;
        settings.minLevel = minLevel;
        settings.maxLevel = maxLevel;
        settings.protectEquipped = (configParts[12] == "1");

        // 解析过滤物品列表（如果有）
        if (configParts.size() > 14 && !configParts[14].empty())
        {
            settings.filteredItems.clear();
            std::istringstream itemsIss(configParts[14]);
            std::string itemIdStr;
            while (std::getline(itemsIss, itemIdStr, ','))
            {
                if (!itemIdStr.empty())
                {
                    try
                    {
                        uint32 itemId = std::stoul(itemIdStr);
                        if (itemId > 0)
                        {
                            settings.filteredItems.insert(itemId);
                        }
                    }
                    catch (const std::exception&)
                    {
                        // 忽略无效的物品ID
                    }
                }
            }
        }

        // 确保炉石在过滤列表中
        settings.filteredItems.insert(6948);

        // 保存设置
        ItemRecycleScript::SavePlayerRecycleSettings(settings);

        handler->SendSysMessage("|cff00ff00[配置更新]|r 配置已成功更新");

        // 调试信息
        if (ItemRecycleGlobals::bItemRecycleDebugMode)
        {
            handler->PSendSysMessage("|cff00ff00[配置更新调试]|r 自动回收: {}, 间隔: {}秒",
                settings.autoRecycleEnabled ? "开启" : "关闭", settings.recycleInterval);
        }
    }
    catch (std::exception& e)
    {
        handler->PSendSysMessage("|cffff0000[配置更新]|r 配置解析失败: {}", e.what());
        handler->SetSentErrorMessage(true);
        return true;
    }

    return true;
}

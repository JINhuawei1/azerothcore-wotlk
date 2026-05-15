#include "ItemPurchaseCondition.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "Logging/Log.h"
#include "ConditionMgr.h"
#include "Common.h"
#include "Configuration/Config.h"
#include <limits>
#include <sstream>

// 包含我们的 placement new 实现
#include "PlacementNew.h"

// 使用已经在Config.h中定义的sConfigMgr宏

// 定义一个函数指针类型，用于动态调用RequirementTemplateManager的CheckRequirements方法
typedef bool (*CheckRequirementsFunc)(void*, Player*, uint32, bool);

// 全局变量，用于存储动态加载的函数指针
CheckRequirementsFunc g_checkRequirementsFunc = nullptr;
void* g_requirementTemplateMgr = nullptr;

// 定义ItemPurchaseConditionMgr的单例实例
ItemPurchaseConditionMgr* ItemPurchaseConditionMgr::instance()
{
    static ItemPurchaseConditionMgr instance;
    return &instance;
}

void ItemPurchaseConditionMgr::Initialize()
{
    // 从配置文件中读取设置
    _enabled = sConfigMgr->GetOption<bool>("ItemPurchaseCondition.Enable", true);

    // 检查需求模板系统是否已安装
    _requirementTemplateEnabled = IsRequirementTemplateModuleInstalled();

    if (_enabled)
    {


        if (_requirementTemplateEnabled)
        {

        }
        else
        {
            LOG_INFO("server.loading", "物品购买条件系统: 未检测到需求模板系统，将使用内置条件系统进行检查");
        }
    }
    else
    {
        LOG_INFO("server.loading", "物品购买条件系统: 已禁用");
    }
}

uint32 ItemPurchaseConditionMgr::LoadData()
{
    if (!_enabled)
        return 0;

    uint32 count = 0;
    _itemPurchaseConditions.clear();

    QueryResult result = WorldDatabase.Query("SELECT `生物id`, `物品id`, `物品购买条件` FROM `_物品_购买条件`");
    if (!result)
        return 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 creatureId = fields[0].Get<uint32>();
        uint32 itemId = fields[1].Get<uint32>();
        uint32 conditionId = fields[2].Get<uint32>();

        _itemPurchaseConditions[creatureId][itemId] = conditionId;
        count++;
    } while (result->NextRow());

    return count;
}

bool ItemPurchaseConditionMgr::IsRequirementTemplateModuleInstalled()
{
    // 检查需求模板模块是否已安装
    QueryResult result = WorldDatabase.Query("SHOW TABLES LIKE '需求_模板'");
    if (!result)
        return false;

    // 尝试获取RequirementTemplateManager实例
    // 这里使用一个简单的方法：检查是否可以查询到需求模板数据
    result = WorldDatabase.Query("SELECT COUNT(*) FROM `需求_模板`");
    if (!result)
        return false;

    return true;
}

bool ItemPurchaseConditionMgr::CheckWithRequirementTemplate(Player* player, uint32 templateId)
{
    // 如果需求模板模块未安装，返回true（默认允许购买）
    if (!_requirementTemplateEnabled)
        return true;

    // 尝试动态获取RequirementTemplateManager实例和CheckRequirements方法
    if (!g_requirementTemplateMgr || !g_checkRequirementsFunc)
    {
        // 这里我们使用一个简单的方法：直接查询数据库
        // 在实际应用中，可能需要更复杂的方法来调用另一个模块的函数
        QueryResult result = WorldDatabase.Query(
            "SELECT `需要人物等级`, `消耗金币`, `消耗物品`, `是否消耗物品` FROM `需求_模板` WHERE `id` = {}", templateId);

        if (!result)
            return true; // 如果找不到模板，默认允许购买

        Field* fields = result->Fetch();
        std::string levelReq = fields[0].Get<std::string>();
        uint64 goldCost = fields[1].Get<uint64>();
        std::string itemsReq = fields[2].Get<std::string>();
        int consumeItems = fields[3].Get<int>();

        // 检查等级要求
        if (!levelReq.empty() && levelReq != "0")
        {
            // 简单处理：只检查大于等于
            if (levelReq[0] == '>')
            {
                int reqLevel = std::stoi(levelReq.substr(1));
                if (player->GetLevel() <= reqLevel)
                    return false;
            }
            else if (levelReq[0] == '<')
            {
                int reqLevel = std::stoi(levelReq.substr(1));
                if (player->GetLevel() >= reqLevel)
                    return false;
            }
            else if (levelReq[0] == '=')
            {
                int reqLevel = std::stoi(levelReq.substr(1));
                if (player->GetLevel() != reqLevel)
                    return false;
            }
            else if (levelReq.substr(0, 2) == "!=")
            {
                int reqLevel = std::stoi(levelReq.substr(2));
                if (player->GetLevel() == reqLevel)
                    return false;
            }
            else
            {
                int reqLevel = std::stoi(levelReq);
                if (player->GetLevel() < reqLevel)
                    return false;
            }
        }

        // 检查金币要求
        if (goldCost > 0)
        {
            uint64 copperCost = goldCost > std::numeric_limits<uint64>::max() / 10000 ? std::numeric_limits<uint64>::max() : goldCost * 10000;
            if (player->GetMoney() < copperCost) // 转换为铜币
                return false;
        }

        // 检查物品要求
        if (!itemsReq.empty() && itemsReq != "0")
        {
            std::istringstream ss(itemsReq);
            std::string token;

            while (std::getline(ss, token, ','))
            {
                std::istringstream itemSS(token);
                uint32 itemId, count;
                itemSS >> itemId >> count;

                if (itemId > 0 && count > 0)
                {
                    if (player->GetItemCount(itemId, false) < count)
                        return false;
                }
            }
        }

        return true;
    }

    // 如果成功获取了函数指针，直接调用
    return g_checkRequirementsFunc(g_requirementTemplateMgr, player, templateId, false);
}

bool ItemPurchaseConditionMgr::CheckWithConditionSystem(Player* player, uint32 conditionId)
{
    // 使用条件系统检查玩家是否满足条件
    ConditionList conditions = sConditionMgr->GetConditionsForNotGroupedEntry(CONDITION_SOURCE_TYPE_SMART_EVENT, conditionId);
    if (!conditions.empty())
    {
        return sConditionMgr->IsObjectMeetToConditions(player, conditions);
    }

    return true;
}

bool ItemPurchaseConditionMgr::CheckItemPurchaseCondition(Player* player, Creature* vendor, uint32 itemId)
{
    if (!_enabled || !player || !vendor)
        return true;

    uint32 creatureId = vendor->GetEntry();
    auto creatureItr = _itemPurchaseConditions.find(creatureId);
    if (creatureItr == _itemPurchaseConditions.end())
        return true;

    auto itemItr = creatureItr->second.find(itemId);
    if (itemItr == creatureItr->second.end())
        return true;

    uint32 conditionId = itemItr->second;
    if (conditionId == 0)
        return true;

    // 首先尝试使用需求模板系统检查条件
    if (_requirementTemplateEnabled)
    {
        if (!CheckWithRequirementTemplate(player, conditionId))
            return false;
    }

    // 然后尝试使用条件系统检查条件
    if (!CheckWithConditionSystem(player, conditionId))
        return false;

    // 如果两种系统都没有找到条件或者都满足条件，允许购买
    return true;
}

void ItemPurchaseConditionPlayerScript::OnPlayerBeforeBuyItemFromVendor(Player* player, ObjectGuid vendorGUID, uint32 /*vendorSlot*/, uint32& item, uint8 /*count*/, uint8 /*bag*/, uint8 /*slot*/)
{
    if (!player)
        return;

    Creature* vendor = ObjectAccessor::GetCreature(*player, vendorGUID);
    if (!vendor)
        return;

    if (!sItemPurchaseConditionMgr->CheckItemPurchaseCondition(player, vendor, item))
    {
        player->SendEquipError(EQUIP_ERR_CANT_DO_RIGHT_NOW, nullptr, nullptr);
        ChatHandler(player->GetSession()).PSendSysMessage("您不满足购买此物品的条件");
        player->PlayerTalkClass->SendCloseGossip();
    }
}

// 创建一个全局函数，用于处理命令
bool HandleItemPurchaseConditionAddCommand(ChatHandler* handler, char const* args)
{
    if (!args || !*args)
    {
        handler->SendSysMessage("命令格式:");
        handler->SendSysMessage(".物品购买条件 添加 <生物ID> <物品ID> <条件ID>");
        return true;
    }

    char* creatureIdStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");
    char* conditionIdStr = strtok(nullptr, " ");

    if (!creatureIdStr || !itemIdStr || !conditionIdStr)
    {
        handler->SendSysMessage("命令格式: .物品购买条件 添加 <生物ID> <物品ID> <条件ID>");
        return true;
    }

    uint32 creatureId = atoi(creatureIdStr);
    uint32 itemId = atoi(itemIdStr);
    uint32 conditionId = atoi(conditionIdStr);

    if (!sObjectMgr->GetCreatureTemplate(creatureId))
    {
        handler->PSendSysMessage("生物ID {} 不存在", creatureId);
        return true;
    }

    if (!sObjectMgr->GetItemTemplate(itemId))
    {
        handler->PSendSysMessage("物品ID {} 不存在", itemId);
        return true;
    }

    WorldDatabase.Execute("REPLACE INTO `_物品_购买条件` (`生物id`, `物品id`, `物品购买条件`) VALUES ({}, {}, {})", creatureId, itemId, conditionId);
    handler->PSendSysMessage("已添加物品购买条件: 生物ID {}, 物品ID {}, 条件ID {}", creatureId, itemId, conditionId);
    sItemPurchaseConditionMgr->LoadData();
    return true;
}

bool HandleItemPurchaseConditionDeleteCommand(ChatHandler* handler, char const* args)
{
    if (!args || !*args)
    {
        handler->SendSysMessage("命令格式:");
        handler->SendSysMessage(".物品购买条件 删除 <生物ID> <物品ID>");
        return true;
    }

    char* creatureIdStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");

    if (!creatureIdStr || !itemIdStr)
    {
        handler->SendSysMessage("命令格式: .物品购买条件 删除 <生物ID> <物品ID>");
        return true;
    }

    uint32 creatureId = atoi(creatureIdStr);
    uint32 itemId = atoi(itemIdStr);

    WorldDatabase.Execute("DELETE FROM `_物品_购买条件` WHERE `生物id` = {} AND `物品id` = {}", creatureId, itemId);
    handler->PSendSysMessage("已删除物品购买条件: 生物ID {}, 物品ID {}", creatureId, itemId);
    sItemPurchaseConditionMgr->LoadData();
    return true;
}

bool HandleItemPurchaseConditionListCommand(ChatHandler* handler, char const* args)
{
    uint32 creatureId = 0;
    if (args && *args)
        creatureId = atoi(args);

    QueryResult result;
    if (creatureId)
    {
        result = WorldDatabase.Query("SELECT `注释`, `生物id`, `物品id`, `物品购买条件` FROM `_物品_购买条件` WHERE `生物id` = {} ORDER BY `物品id`", creatureId);
    }
    else
    {
        result = WorldDatabase.Query("SELECT `注释`, `生物id`, `物品id`, `物品购买条件` FROM `_物品_购买条件` ORDER BY `生物id`, `物品id`");
    }

    if (!result)
    {
        handler->SendSysMessage("没有找到物品购买条件记录");
        return true;
    }

    handler->SendSysMessage("物品购买条件列表:");
    handler->SendSysMessage("注释 | 生物ID | 物品ID | 条件ID");
    handler->SendSysMessage("----------------------------------");

    do
    {
        Field* fields = result->Fetch();
        std::string comment = fields[0].Get<std::string>();
        uint32 creatureId = fields[1].Get<uint32>();
        uint32 itemId = fields[2].Get<uint32>();
        uint32 conditionId = fields[3].Get<uint32>();

        handler->PSendSysMessage("{} | {} | {} | {}", comment.c_str(), creatureId, itemId, conditionId);
    } while (result->NextRow());

    return true;
}

std::vector<Acore::ChatCommands::ChatCommandBuilder> ItemPurchaseConditionCommandScript::GetCommands() const
{
    using namespace Acore::ChatCommands;

    // 使用AzerothCore官方推荐的方式初始化命令表
    static ChatCommandTable itemPurchaseConditionSubCommandTable =
    {
        { "添加", HandleItemPurchaseConditionAddCommand, AccountTypes::SEC_ADMINISTRATOR, Console::No },
        { "删除", HandleItemPurchaseConditionDeleteCommand, AccountTypes::SEC_ADMINISTRATOR, Console::No },
        { "列表", HandleItemPurchaseConditionListCommand, AccountTypes::SEC_ADMINISTRATOR, Console::No }
    };

    static ChatCommandTable commandTable =
    {
        { "物品购买条件", itemPurchaseConditionSubCommandTable }
    };

    return commandTable;
}

bool ItemPurchaseConditionCommandScript::HandleItemPurchaseConditionCommand(ChatHandler* handler, char const* args)
{
    if (!args || !*args)
    {
        handler->SendSysMessage("命令格式:");
        handler->SendSysMessage(".物品购买条件 添加 <生物ID> <物品ID> <条件ID>");
        handler->SendSysMessage(".物品购买条件 删除 <生物ID> <物品ID>");
        handler->SendSysMessage(".物品购买条件 列表 [生物ID]");
        return true;
    }

    char* cmd = strtok((char*)args, " ");
    if (!cmd)
        return false;

    if (strcmp(cmd, "添加") == 0)
    {
        char* creatureIdStr = strtok(nullptr, " ");
        char* itemIdStr = strtok(nullptr, " ");
        char* conditionIdStr = strtok(nullptr, " ");

        if (!creatureIdStr || !itemIdStr || !conditionIdStr)
        {
            handler->SendSysMessage("命令格式: .物品购买条件 添加 <生物ID> <物品ID> <条件ID>");
            return true;
        }

        uint32 creatureId = atoi(creatureIdStr);
        uint32 itemId = atoi(itemIdStr);
        uint32 conditionId = atoi(conditionIdStr);

        if (!sObjectMgr->GetCreatureTemplate(creatureId))
        {
            handler->PSendSysMessage("生物ID {} 不存在", creatureId);
            return true;
        }

        if (!sObjectMgr->GetItemTemplate(itemId))
        {
            handler->PSendSysMessage("物品ID {} 不存在", itemId);
            return true;
        }

        WorldDatabase.Execute("REPLACE INTO `_物品_购买条件` (`生物id`, `物品id`, `物品购买条件`) VALUES ({}, {}, {})", creatureId, itemId, conditionId);
        handler->PSendSysMessage("已添加物品购买条件: 生物ID {}, 物品ID {}, 条件ID {}", creatureId, itemId, conditionId);
        sItemPurchaseConditionMgr->LoadData();
        return true;
    }
    else if (strcmp(cmd, "删除") == 0)
    {
        char* creatureIdStr = strtok(nullptr, " ");
        char* itemIdStr = strtok(nullptr, " ");

        if (!creatureIdStr || !itemIdStr)
        {
            handler->SendSysMessage("命令格式: .物品购买条件 删除 <生物ID> <物品ID>");
            return true;
        }

        uint32 creatureId = atoi(creatureIdStr);
        uint32 itemId = atoi(itemIdStr);

        WorldDatabase.Execute("DELETE FROM `_物品_购买条件` WHERE `生物id` = {} AND `物品id` = {}", creatureId, itemId);
        handler->PSendSysMessage("已删除物品购买条件: 生物ID {}, 物品ID {}", creatureId, itemId);
        sItemPurchaseConditionMgr->LoadData();
        return true;
    }
    else if (strcmp(cmd, "列表") == 0)
    {
        char* creatureIdStr = strtok(nullptr, " ");
        uint32 creatureId = creatureIdStr ? atoi(creatureIdStr) : 0;

        QueryResult result;
        if (creatureId)
        {
            result = WorldDatabase.Query("SELECT `注释`, `生物id`, `物品id`, `物品购买条件` FROM `_物品_购买条件` WHERE `生物id` = {} ORDER BY `物品id`", creatureId);
        }
        else
        {
            result = WorldDatabase.Query("SELECT `注释`, `生物id`, `物品id`, `物品购买条件` FROM `_物品_购买条件` ORDER BY `生物id`, `物品id`");
        }

        if (!result)
        {
            handler->SendSysMessage("没有找到物品购买条件记录");
            return true;
        }

        handler->SendSysMessage("物品购买条件列表:");
        handler->SendSysMessage("注释 | 生物ID | 物品ID | 条件ID");
        handler->SendSysMessage("----------------------------------");

        do
        {
            Field* fields = result->Fetch();
            std::string comment = fields[0].Get<std::string>();
            uint32 creatureId = fields[1].Get<uint32>();
            uint32 itemId = fields[2].Get<uint32>();
            uint32 conditionId = fields[3].Get<uint32>();

            handler->PSendSysMessage("{} | {} | {} | {}", comment.c_str(), creatureId, itemId, conditionId);
        } while (result->NextRow());

        return true;
    }

    return false;
}

// 实现ItemPurchaseConditionModuleLoader的LoadModule方法
uint32 ItemPurchaseConditionModuleLoader::LoadModule()
{
    // 加载模块数据
    uint32 count = sItemPurchaseConditionMgr->LoadData();

    // 加载完成后显示结果
    LOG_INFO("server.loading", "→物品购买条件系统√");

    return count;
}

// 实现ItemPurchaseConditionModuleLoader的OnUpdate方法
void ItemPurchaseConditionModuleLoader::OnUpdate(uint32 diff)
{
    if (_isLoaded)
        return;

    _loadTimer += diff;
    if (_loadTimer >= _loadDelay)
    {
        // 加载模块
        LoadModule();
        _isLoaded = true;
    }
}

// 添加所有脚本到核心
void AddItemPurchaseConditionScripts()
{
    // 注册脚本
    new ItemPurchaseConditionWorldScript();
    new ItemPurchaseConditionPlayerScript();
    new ItemPurchaseConditionCommandScript();
    new ItemPurchaseConditionModuleLoader();
}

/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "ScriptedGossip.h"
#include "DatabaseEnv.h"
#include "Logging/Log.h"
#include "Configuration/Config.h"
#include "World.h"
#include <map>
#include <vector>
#include <string>
#include <sstream>

// 声明外部函数，用于检查需求模板
extern bool CheckRequirementTemplate(Player* player, uint32 templateId, bool showMessages = true);

using namespace Acore::ChatCommands;

enum ItemUseConditionAcoreString
{
    ITEM_USE_CONDITION_LOADED = 50000,
    ITEM_USE_CONDITION_RELOAD,
    ITEM_USE_CONDITION_NOT_FOUND,
    ITEM_USE_CONDITION_MEETS_REQUIREMENTS,
    ITEM_USE_CONDITION_NOT_MEETS_REQUIREMENTS,
    ITEM_USE_CONDITION_RELOADED,
    ITEM_USE_CONDITION_LOADED_COUNT
};

struct ItemUseConditionEntry
{
    std::string 注释;
    uint32 entry;
    uint32 物品使用需求;
};

class ItemUseConditionManager
{
public:
    static ItemUseConditionManager* instance()
    {
        static ItemUseConditionManager instance;
        return &instance;
    }

    void Initialize()
    {
        LoadItemUseConditions();
    }

    void LoadItemUseConditions()
    {
        _itemUseConditions.clear();

        QueryResult result = WorldDatabase.Query("SELECT `注释`, `entry`, `物品使用需求` FROM `_物品_使用条件`");
        if (!result)
        {
            LOG_INFO("server.loading", ">> _物品_使用条件表不存在或为空，请确保已手动导入SQL文件");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            ItemUseConditionEntry entry;

            entry.注释 = fields[0].Get<std::string>();
            entry.entry = fields[1].Get<uint32>();
            entry.物品使用需求 = fields[2].Get<uint32>();

            _itemUseConditions[entry.entry] = entry;
            count++;
        } while (result->NextRow());


    }

    ItemUseConditionEntry const* GetItemUseCondition(uint32 itemId) const
    {
        auto itr = _itemUseConditions.find(itemId);
        if (itr != _itemUseConditions.end())
            return &itr->second;

        return nullptr;
    }

private:
    std::map<uint32, ItemUseConditionEntry> _itemUseConditions;
};

#define sItemUseConditionMgr ItemUseConditionManager::instance()

class ItemUseCondition_PlayerScript : public PlayerScript
{
public:
    ItemUseCondition_PlayerScript() : PlayerScript("ItemUseCondition_PlayerScript") { }

    // 使用正确的物品使用事件回调
    bool OnPlayerCanCastItemUseSpell(Player* player, Item* item, SpellCastTargets const& targets, uint8 cast_count, uint32 glyphIndex) override
    {
        if (!player || !item)
            return true;

        uint32 itemId = item->GetEntry();

        ItemUseConditionEntry const* condition = sItemUseConditionMgr->GetItemUseCondition(itemId);
        if (!condition)
            return true;

        // 需求系统的ID
        uint32 requirementId = condition->物品使用需求;
        if (requirementId == 0)
            return true;

        // 使用需求模板系统检查玩家是否满足条件
        bool result = CheckRequirementTemplate(player, requirementId);

        if (!result)
        {
            // 如果不满足条件，阻止使用物品
            player->GetSession()->SendAreaTriggerMessage("您不满足使用此物品的条件");
            return false;
        }

        return true;
    }
};

class ItemUseCondition_CommandScript : public CommandScript
{
public:
    ItemUseCondition_CommandScript() : CommandScript("ItemUseCondition_CommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable itemUseConditionCommandTable =
        {
            { "重载", HandleItemUseConditionReloadCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "物品使用条件", itemUseConditionCommandTable }
        };
        return commandTable;
    }

    static bool HandleItemUseConditionReloadCommand(ChatHandler* handler)
    {
        sItemUseConditionMgr->LoadItemUseConditions();
        handler->SendSysMessage("物品使用条件已重新加载。");
        return true;
    }
};

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemUseCondition_ModuleLoader : public WorldScript
{
public:
    ItemUseCondition_ModuleLoader() : WorldScript("ItemUseCondition_ModuleLoader")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟
        _enabled = sConfigMgr->GetOption<bool>("ItemUseCondition.Enable", true);

        if (!_enabled)
        {
            LOG_INFO("server.loading", "物品使用条件系统已关闭");
        }
        else
        {
            // 不显示准备中的日志
        }
    }

    void OnUpdate(uint32 diff) override
    {
        if (_isLoaded || !_enabled)
            return;

        _loadTimer += diff;
        if (_loadTimer >= _loadDelay)
        {
            // 加载模块
            LoadModule();
            _isLoaded = true;
        }
    }

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;
    bool _enabled;

    uint32 LoadModule()
    {
        if (!_enabled)
            return 0;

        sItemUseConditionMgr->Initialize();

        // 获取加载的条件数量
        uint32 count = 0;
        try
        {
            QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `_物品_使用条件`");
            if (result)
            {
                Field* fields = result->Fetch();
                count = fields[0].Get<uint32>();
            }

            // 加载完成后显示结果
			LOG_INFO("server.loading", "→物品使用条件系统√");
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("server.loading", "查询物品使用条件数据时发生错误: {}", e.what());
        }

        return count;
    }
};

// 添加所有脚本
void AddItemUseConditionScripts()
{
    new ItemUseCondition_PlayerScript();
    new ItemUseCondition_CommandScript();
    new ItemUseCondition_ModuleLoader();
}

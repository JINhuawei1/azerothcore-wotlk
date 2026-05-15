#ifndef MODULE_ITEM_PURCHASE_CONDITION_H
#define MODULE_ITEM_PURCHASE_CONDITION_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

#include "Creature.h"
#include "ObjectMgr.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "Logging/Log.h"
#include <map>

// 包含我们的 placement new 实现
#include "PlacementNew.h"

// 前向声明RequirementTemplateManager类，避免直接包含头文件
class RequirementTemplateManager;

class ItemPurchaseConditionMgr;
#define sItemPurchaseConditionMgr ItemPurchaseConditionMgr::instance()

struct ItemPurchaseCondition
{
    uint32 creatureId;
    uint32 itemId;
    uint32 conditionId;
};

class ItemPurchaseConditionMgr
{
public:
    static ItemPurchaseConditionMgr* instance();

    ItemPurchaseConditionMgr()
    {
        // 初始化将在Initialize方法中完成
    }

    void Initialize();
    uint32 LoadData();
    bool CheckItemPurchaseCondition(Player* player, Creature* vendor, uint32 itemId);

private:
    bool _enabled;
    bool _requirementTemplateEnabled;
    std::map<uint32, std::map<uint32, uint32>> _itemPurchaseConditions;

    bool IsRequirementTemplateModuleInstalled();
    bool CheckWithRequirementTemplate(Player* player, uint32 templateId);
    bool CheckWithConditionSystem(Player* player, uint32 conditionId);
};

class ItemPurchaseConditionWorldScript : public WorldScript
{
public:
    ItemPurchaseConditionWorldScript() : WorldScript("ItemPurchaseConditionWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        sItemPurchaseConditionMgr->Initialize();
    }
};

class ItemPurchaseConditionPlayerScript : public PlayerScript
{
public:
    ItemPurchaseConditionPlayerScript() : PlayerScript("ItemPurchaseConditionPlayerScript") { }

    void OnPlayerBeforeBuyItemFromVendor(Player* player, ObjectGuid vendorGUID, uint32 vendorSlot, uint32& item, uint8 count, uint8 bag, uint8 slot) override;
};

class ItemPurchaseConditionCommandScript : public CommandScript
{
public:
    ItemPurchaseConditionCommandScript() : CommandScript("ItemPurchaseConditionCommandScript") { }

    std::vector<Acore::ChatCommands::ChatCommandBuilder> GetCommands() const override;
    bool HandleItemPurchaseConditionCommand(ChatHandler* handler, char const* args);
};

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemPurchaseConditionModuleLoader : public WorldScript
{
public:
    ItemPurchaseConditionModuleLoader() : WorldScript("ItemPurchaseConditionModuleLoader")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟

        // 不显示准备中的日志
    }

    void OnUpdate(uint32 diff) override;
    uint32 LoadModule();

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;
};

#endif // MODULE_ITEM_PURCHASE_CONDITION_H

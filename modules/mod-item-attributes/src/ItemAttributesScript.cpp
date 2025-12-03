#include "ItemAttributesLoader.h"
#include "ItemAttributesEffects.h"
#include "ItemAttributesDisplay.h"
#include "ItemAttributesGenerator.h"
#include "ItemAttributesEvents.h"
#include "ItemAttributesDBHelper.h"
#include "ItemAttributesGlobalScript.h"
#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "Player.h"
#include "Item.h"
#include "World.h"
#include <sstream>
#include <vector>

using namespace Acore::ChatCommands;

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemAttributesWorldScript : public WorldScript
{
public:
    ItemAttributesWorldScript() :
        WorldScript("ItemAttributesWorldScript"),
        _initialized(false),
        _loadTimer(0),
        _delayTime(500) // 0.5秒延迟，确保在其他依赖模块（如mod-item-enhancement 1秒延迟）之前初始化
    {
    }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        // 在AzerothCore中，ConfigMgr没有LoadMoreConfig方法
        // 配置文件已经在模块初始化时加载
    }

    void OnUpdate(uint32 diff) override
    {
        // 如果已经初始化，不再执行
        if (_initialized)
            return;

        // 累加时间
        _loadTimer += diff;

        // 当达到延迟时间时，加载模块
        if (_loadTimer >= _delayTime)
        {
            if (!sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
            {
                _initialized = true;
                return;
            }

            // 在服务器完全启动后初始化物品属性系统

            // 加载物品属性模板
            sItemAttributesLoader->LoadItemAttributeTemplates();
            uint32 templateCount = sItemAttributesLoader->GetAllItemAttributeTemplates().size();

            // 初始化物品属性效果系统
            sItemAttributesEffects->Initialize();

            // 初始化物品属性显示系统
            sItemAttributesDisplay->Initialize();

            // 初始化物品属性生成系统
            sItemAttributesGenerator->Initialize();

            LOG_INFO("server.loading", "→物品属性系统加载√ [模板数: {}]", templateCount);

            _initialized = true;
        }
    }

    void OnShutdown() override
    {
        // 服务器关闭时的清理工作
        LOG_INFO("module.itemattributes", "物品属性系统已关闭");
    }

private:
    bool _initialized;     // 是否已初始化
    uint32 _loadTimer;     // 计时器
    uint32 _delayTime;     // 延迟时间
};

// ============================================================
// 物品查找辅助函数 (参考 mod-item-growth)
// ============================================================

// 根据物品ID和GUID查找玩家的物品
static Item* FindPlayerItemByIdAndGuid(Player* player, uint32 itemId, uint64 clientGuid)
{
    if (!player || itemId == 0)
        return nullptr;

    // 检查主背包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId)
        {
            uint64 actualGuid = item->GetGUID().GetCounter();
            if (actualGuid == clientGuid)
                return item;
        }
    }

    // 检查其他背包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetEntry() == itemId)
                {
                    uint64 actualGuid = item->GetGUID().GetCounter();
                    if (actualGuid == clientGuid)
                        return item;
                }
            }
        }
    }

    // 检查装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId)
        {
            uint64 actualGuid = item->GetGUID().GetCounter();
            if (actualGuid == clientGuid)
                return item;
        }
    }

    return nullptr;
}

// 根据位置查找玩家的物品 (bag/slot/equipFlag)
static Item* FindPlayerItemByPosition(Player* player, uint32 itemId, int32 bag, int32 slot, bool isEquip)
{
    if (!player)
        return nullptr;

    // 装备位
    if (isEquip)
    {
        // 客户端装备槽是1-based，服务器是0-based
        int32 clientSlot = slot;
        uint8 serverSlot = (clientSlot > 0) ? static_cast<uint8>(clientSlot - 1) : static_cast<uint8>(clientSlot);
        if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (it && it->GetEntry() == itemId)
                return it;
        }
        return nullptr;
    }

    // 背包位
    if (bag >= 0 && slot >= 0)
    {
        if (bag == 0)
        {
            // 主背包，客户端槽位是1-based
            uint8 serverSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : static_cast<uint8>(slot);
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (it && it->GetEntry() == itemId)
                return it;
        }
        else
        {
            // 其他背包
            uint8 bagPos = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            Bag* bagObj = player->GetBagByPos(bagPos);
            if (bagObj)
            {
                uint8 serverSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : static_cast<uint8>(slot);
                if (serverSlot < bagObj->GetBagSize())
                {
                    Item* it = bagObj->GetItemByPos(serverSlot);
                    if (it && it->GetEntry() == itemId)
                        return it;
                }
            }
        }
    }

    return nullptr;
}

// 格式化属性字符串 (属性ID列表 + 属性值列表 -> "属性名+值,属性名+值")
static std::string FormatAttributesForClient(const std::string& attrIds, const std::string& attrVals)
{
    std::vector<uint32> ids;
    std::vector<int32> vals;
    
    // 解析ID列表
    std::istringstream idsStream(attrIds);
    std::string id;
    while (std::getline(idsStream, id, ','))
    {
        if (!id.empty())
            ids.push_back(atoi(id.c_str()));
    }
    
    // 解析值列表
    std::istringstream valsStream(attrVals);
    std::string val;
    while (std::getline(valsStream, val, ','))
    {
        if (!val.empty())
            vals.push_back(atoi(val.c_str()));
    }
    
    // 构造 "属性名+值" 格式
    std::string result;
    for (size_t i = 0; i < ids.size() && i < vals.size(); ++i)
    {
        if (i > 0)
            result += ",";
        
        // 【重要】ids中存的是属性类型，不是模板ID
        ItemAttributeTemplate const* tpl = sItemAttributesLoader->GetItemAttributeTemplateByType(ids[i]);
        if (tpl)
            result += tpl->clientDisplay + "+" + std::to_string(vals[i]);
        else
            result += "未知属性+" + std::to_string(vals[i]);
    }
    
    return result;
}

// ============================================================
// 命令处理器
// ============================================================

// 添加命令处理器
class ItemAttributes_CommandScript : public CommandScript
{
public:
    ItemAttributes_CommandScript() : CommandScript("ItemAttributes_CommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable attributesCommandTable =
        {
            { "查询",       HandleAttributesQueryCommand,      SEC_PLAYER,        Console::No },
            { "列表",       HandleAttributesListCommand,       SEC_GAMEMASTER,    Console::No },
            { "添加",       HandleAttributesAddCommand,        SEC_ADMINISTRATOR, Console::No },
            { "删除",       HandleAttributesRemoveCommand,     SEC_ADMINISTRATOR, Console::No },
            { "清空",       HandleAttributesClearCommand,      SEC_ADMINISTRATOR, Console::No },
            { "查看",       HandleAttributesShowCommand,       SEC_PLAYER,        Console::No },
            { "生成",       HandleAttributesGenerateCommand,   SEC_ADMINISTRATOR, Console::No },
            { "刷新",       HandleAttributesRefreshCommand,    SEC_PLAYER,        Console::No },
            { "重载",       HandleAttributesReloadCommand,     SEC_ADMINISTRATOR, Console::No },
            { "清理",       HandleAttributesCleanupCommand,    SEC_ADMINISTRATOR, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "物品属性",   attributesCommandTable }
        };

        return commandTable;
    }

    // 查询命令 - 供客户端插件调用 (参考 mod-item-growth 实现)
    static bool HandleAttributesQueryCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 静默处理：无参数时直接返回
        if (!args || !*args)
            return true;

        // 解析参数: itemID clientGuid [bag] [slot] [equipFlag] [rpId]
        char* itemIdStr = strtok((char*)args, " ");
        char* clientGuidStr = strtok(nullptr, " ");
        char* bagStr = strtok(nullptr, " ");
        char* slotStr = strtok(nullptr, " ");
        char* equipStr = strtok(nullptr, " ");
        char* rpIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !clientGuidStr)
            return true; // 静默失败

        uint32 itemId = atoi(itemIdStr);
        uint64 clientGuid = atoi(clientGuidStr);
        int32 bag = bagStr ? atoi(bagStr) : -1;
        int32 slot = slotStr ? atoi(slotStr) : -1;
        bool isEquip = equipStr ? (atoi(equipStr) != 0) : false;

        if (itemId == 0)
        {
            // 空响应
            std::string response = "ITEMATTRIBUTES:" + std::to_string(itemId) + ":" + std::to_string(clientGuid) + ":::0:-1:-1:0";
            handler->PSendSysMessage(response.c_str());
            return true;
        }

        // 多种方式查找物品
        Item* item = FindPlayerItemByIdAndGuid(player, itemId, clientGuid);
        if (!item)
            item = FindPlayerItemByPosition(player, itemId, bag, slot, isEquip);

        // 获取真实GUID（如果找到物品则用物品GUID，否则用clientGuid）
        uint64 realGuid = item ? item->GetGUID().GetCounter() : clientGuid;

        std::string attributesStr;
        ItemAttributesDBHelper::ItemAttributeData* data = ItemAttributesDBHelper::LoadItemAttributes(realGuid);
        if (data)
        {
            std::string baseAttrs = ItemAttributesDBHelper::ItemAttributeData::ToCompactString(data->baseAttributeIds, data->baseAttributeValues);
            std::string additionalAttrs = ItemAttributesDBHelper::ItemAttributeData::ToCompactString(data->additionalAttributeIds, data->additionalAttributeValues);

            if (!baseAttrs.empty())
                attributesStr = baseAttrs;
            if (!additionalAttrs.empty())
            {
                if (!attributesStr.empty())
                    attributesStr += ",";
                attributesStr += additionalAttrs;
            }

            delete data;
        }

        // 计算物品位置信息
        int32 outBag = -1;
        int32 outSlot = -1;
        int32 outEquip = 0;
        
        if (item)
        {
            if (item->IsEquipped())
            {
                outBag = 255;  // 装备栏标识
                outSlot = item->GetSlot() + 1;  // 客户端1-based
                outEquip = 1;
            }
            else if (item->IsInBag())
            {
                outBag = item->GetBagSlot() - INVENTORY_SLOT_BAG_START + 1;  // 客户端1-based
                outSlot = item->GetSlot() + 1;  // 客户端1-based
                outEquip = 0;
            }
            else
            {
                outBag = 0;  // 主背包
                outSlot = item->GetSlot() + 1;  // 客户端1-based
                outEquip = 0;
            }
        }

        // 构造响应: ITEMATTRIBUTES:itemID:clientGuid:attributes:realGuid:bag:slot:equip
        std::string response = "ITEMATTRIBUTES:" +
                              std::to_string(itemId) + ":" +
                              std::to_string(clientGuid) + ":" +
                              attributesStr + ":" +
                              std::to_string(realGuid) + ":" +
                              std::to_string(outBag) + ":" +
                              std::to_string(outSlot) + ":" +
                              std::to_string(outEquip);

        handler->PSendSysMessage(response.c_str());
        
        LOG_DEBUG("module.itemattributes", "Query: Player {} itemID {} clientGuid {} realGuid {} - attributes: '{}'",
                  player->GetName(), itemId, clientGuid, realGuid, attributesStr);
        
        return true;
    }

    static bool HandleAttributesListCommand(ChatHandler* handler, const char* args)
    {
        uint32 group = 0;

        if (*args)
            group = atoi(args);

        std::vector<ItemAttributeTemplate const*> attributes;

        if (group > 0)
            attributes = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(group);
        else
            attributes = sItemAttributesLoader->GetAllItemAttributeTemplates();

        if (attributes.empty())
        {
            if (group > 0)
                handler->PSendSysMessage("组 {} 中没有物品属性", group);
            else
                handler->SendSysMessage("没有找到物品属性");

            handler->SetSentErrorMessage(true);
            return false;
        }

        if (group > 0)
            handler->PSendSysMessage("组 {} 中的物品属性列表:", group);
        else
            handler->SendSysMessage("所有物品属性列表:");

        for (ItemAttributeTemplate const* attribute : attributes)
        {
            handler->PSendSysMessage("{} - {} ({})",
                attribute->id,
                attribute->clientDisplay,
                attribute->comment);
        }

        handler->PSendSysMessage("共找到 {} 个物品属性", static_cast<uint32>(attributes.size()));
        return true;
    }

    static bool HandleAttributesAddCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("用法: .物品属性 添加 <属性ID> <物品ID> [属性值]");
            handler->SendSysMessage("说明: 为指定物品添加属性");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* attributeIdStr = strtok((char*)args, " ");
        char* itemIdStr = strtok(nullptr, " ");
        char* attributeValueStr = strtok(nullptr, " ");

        if (!attributeIdStr || !itemIdStr)
        {
            handler->SendSysMessage("用法: .物品属性 添加 <属性ID> <物品ID> [属性值]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 attributeId = atoi(attributeIdStr);
        uint32 itemId = atoi(itemIdStr);
        
        // 验证属性是否存在
        ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader->GetItemAttributeTemplate(attributeId);
        if (!attrTemplate)
        {
            handler->PSendSysMessage("属性ID {} 无效", attributeId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 在玩家背包中查找指定ID的物品
        Item* item = nullptr;
        
        // 检查装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (tempItem && tempItem->GetEntry() == itemId)
            {
                item = tempItem;
                break;
            }
        }
        
        // 检查主背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (tempItem && tempItem->GetEntry() == itemId)
                {
                    item = tempItem;
                    break;
                }
            }
        }
        
        // 检查其他背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                Bag* bag = player->GetBagByPos(i);
                if (bag)
                {
                    for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        Item* tempItem = bag->GetItemByPos(j);
                        if (tempItem && tempItem->GetEntry() == itemId)
                        {
                            item = tempItem;
                            break;
                        }
                    }
                    if (item)
                        break;
                }
            }
        }
        
        if (!item)
        {
            handler->PSendSysMessage("未在背包中找到物品ID: {}", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 计算属性值（如果未指定）
        int32 attributeValue = 0;
        if (attributeValueStr)
        {
            attributeValue = atoi(attributeValueStr);
        }
        else
        {
            // 使用模板中的随机范围生成属性值
            uint32 itemLevel = item->GetTemplate()->ItemLevel;
            int32 minPercent = attrTemplate->minPercent;
            int32 maxPercent = attrTemplate->maxPercent;
            
            if (attrTemplate->calcType == 1) // 乘以物品等级
            {
                int32 baseValue = (minPercent + maxPercent) / 2;
                attributeValue = (baseValue * itemLevel) / 100;
            }
            else if (attrTemplate->calcType == 2) // 固定值
            {
                attributeValue = (minPercent + maxPercent) / 2;
            }
            else // 常规值
            {
                attributeValue = (minPercent + maxPercent) / 2;
            }
        }

        // 添加属性到数据库
        uint64 itemGuid = item->GetGUID().GetCounter();
        uint32 itemEntry = item->GetEntry();
        if (ItemAttributesDBHelper::AddAttributeToItem(itemGuid, itemEntry, attributeId, attributeValue))
        {
            // 将真实GUID写入randomPropertyId字段（客户端识别用）
            int32 randomPropertyId = static_cast<int32>(itemGuid);
            
            // 直接设置字段值并标记为已修改
            item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, randomPropertyId);
            item->SetState(ITEM_CHANGED, player);
            item->SaveToDB(nullptr); // 保存到数据库
            
            handler->PSendSysMessage("成功为物品 [{}] (ID: {}) 添加属性: {} +{}", 
                item->GetTemplate()->Name1, itemId, attrTemplate->clientDisplay, attributeValue);
            
            // 更新物品显示和属性效果（如果已装备）
            if (item->IsEquipped())
            {
                // 【关键修复】添加空指针检查
                if (sItemAttributesEffects)
                {
                    // 先移除旧效果
                    sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                }
                // 重新应用原生属性
                player->_ApplyItemMods(item, item->GetSlot(), false);
                player->_ApplyItemMods(item, item->GetSlot(), true);
                // 应用新的自定义属性效果
                if (sItemAttributesEffects)
                {
                    sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
                }
                // 刷新玩家属性
                player->UpdateStats(STAT_STRENGTH);
                player->UpdateStats(STAT_AGILITY);
                player->UpdateStats(STAT_STAMINA);
                player->UpdateStats(STAT_INTELLECT);
                player->UpdateStats(STAT_SPIRIT);
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateSpellDamageAndHealingBonus();
            }
            
            return true;
        }
        else
        {
            handler->SendSysMessage("添加属性失败");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    static bool HandleAttributesRemoveCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("用法: .物品属性 删除 <属性ID> <物品ID>");
            handler->SendSysMessage("说明: 从指定物品移除指定属性");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* attributeIdStr = strtok((char*)args, " ");
        char* itemIdStr = strtok(nullptr, " ");

        if (!attributeIdStr || !itemIdStr)
        {
            handler->SendSysMessage("用法: .物品属性 删除 <属性ID> <物品ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 attributeId = atoi(attributeIdStr);
        uint32 itemId = atoi(itemIdStr);
        
        // 验证属性是否存在
        ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader->GetItemAttributeTemplate(attributeId);
        if (!attrTemplate)
        {
            handler->PSendSysMessage("属性ID {} 无效", attributeId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 在玩家背包中查找指定ID的物品
        Item* item = nullptr;
        
        // 检查装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (tempItem && tempItem->GetEntry() == itemId)
            {
                item = tempItem;
                break;
            }
        }
        
        // 检查主背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (tempItem && tempItem->GetEntry() == itemId)
                {
                    item = tempItem;
                    break;
                }
            }
        }
        
        // 检查其他背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                Bag* bag = player->GetBagByPos(i);
                if (bag)
                {
                    for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        Item* tempItem = bag->GetItemByPos(j);
                        if (tempItem && tempItem->GetEntry() == itemId)
                        {
                            item = tempItem;
                            break;
                        }
                    }
                    if (item)
                        break;
                }
            }
        }
        
        if (!item)
        {
            handler->PSendSysMessage("未在背包中找到物品ID: {}", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 从数据库移除属性
        uint64 itemGuid = item->GetGUID().GetCounter();
        if (ItemAttributesDBHelper::RemoveAttributeFromItem(itemGuid, attributeId))
        {
            // 检查是否还有剩余属性
            if (!ItemAttributesDBHelper::HasAttributes(itemGuid))
            {
                // 没有属性了，清除randomPropertyId
                item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, 0);
                item->SetState(ITEM_CHANGED, player);
                item->SaveToDB(nullptr);
            }
            
            handler->PSendSysMessage("成功从物品 [{}] (ID: {}) 移除属性: {}", 
                item->GetTemplate()->Name1, itemId, attrTemplate->clientDisplay);
            
            // 更新物品显示和属性效果（如果已装备）
            if (item->IsEquipped())
            {
                // 【关键修复】添加空指针检查
                if (sItemAttributesEffects)
                {
                    // 先移除所有自定义属性效果
                    sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                }
                // 重新应用原生属性
                player->_ApplyItemMods(item, item->GetSlot(), false);
                player->_ApplyItemMods(item, item->GetSlot(), true);
                // 重新应用剩余的自定义属性效果
                if (sItemAttributesEffects)
                {
                    sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
                }
                // 刷新玩家属性
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateSpellDamageAndHealingBonus();
            }
            
            return true;
        }
        else
        {
            handler->PSendSysMessage("物品 [{}] (ID: {}) 没有属性: {}", 
                item->GetTemplate()->Name1, itemId, attrTemplate->clientDisplay);
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    static bool HandleAttributesClearCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("用法: .物品属性 清空 <物品ID>");
            handler->SendSysMessage("说明: 清空指定物品的所有属性");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(args);
        
        // 在玩家背包中查找指定ID的物品
        Item* item = nullptr;
        
        // 检查装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (tempItem && tempItem->GetEntry() == itemId)
            {
                item = tempItem;
                break;
            }
        }
        
        // 检查主背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (tempItem && tempItem->GetEntry() == itemId)
                {
                    item = tempItem;
                    break;
                }
            }
        }
        
        // 检查其他背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                Bag* bag = player->GetBagByPos(i);
                if (bag)
                {
                    for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        Item* tempItem = bag->GetItemByPos(j);
                        if (tempItem && tempItem->GetEntry() == itemId)
                        {
                            item = tempItem;
                            break;
                        }
                    }
                    if (item)
                        break;
                }
            }
        }
        
        if (!item)
        {
            handler->PSendSysMessage("未在背包中找到物品ID: {}", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint64 itemGuid = item->GetGUID().GetCounter();
        
        // 检查是否有属性
        if (!ItemAttributesDBHelper::HasAttributes(itemGuid))
        {
            handler->PSendSysMessage("物品 [{}] (ID: {}) 没有自定义属性", item->GetTemplate()->Name1, itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 清空所有属性
        if (ItemAttributesDBHelper::ClearItemAttributes(itemGuid))
        {
            // 清除randomPropertyId
            item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, 0);
            item->SetState(ITEM_CHANGED, player);
            item->SaveToDB(nullptr);
            
            handler->PSendSysMessage("已清空物品 [{}] (ID: {}) 的所有自定义属性", item->GetTemplate()->Name1, itemId);
            
            // 更新物品显示和属性效果（如果已装备）
            if (item->IsEquipped())
            {
                // 【关键修复】添加空指针检查
                if (sItemAttributesEffects)
                {
                    // 移除所有自定义属性效果
                    sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                }
                // 重新应用原生属性
                player->_ApplyItemMods(item, item->GetSlot(), false);
                player->_ApplyItemMods(item, item->GetSlot(), true);
                // 刷新玩家属性
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateSpellDamageAndHealingBonus();
            }
            
            return true;
        }
        else
        {
            handler->SendSysMessage("清空属性失败");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    static bool HandleAttributesShowCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 获取手持物品
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        if (!item)
        {
            handler->SendSysMessage("请手持一件物品");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint64 itemGuid = item->GetGUID().GetCounter();
        
        // 读取物品属性
        ItemAttributesDBHelper::ItemAttributeData* data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
        
        if (!data || (data->baseAttributeIds.empty() && data->additionalAttributeIds.empty()))
        {
            handler->PSendSysMessage("物品 [{}] (GUID: {}) 没有自定义属性", 
                item->GetTemplate()->Name1, itemGuid);
            if (data)
                delete data;
            return true;
        }

        handler->PSendSysMessage("===== 物品属性信息 =====");
        handler->PSendSysMessage("物品: [{}] (ID: {}, GUID: {})", 
            item->GetTemplate()->Name1, item->GetEntry(), itemGuid);
        handler->PSendSysMessage("基础属性数量: {}, 追加属性数量: {}", 
            data->baseAttributeIds.size(), data->additionalAttributeIds.size());
        handler->PSendSysMessage("------------------------");

        // 显示基础属性
        if (!data->baseAttributeIds.empty())
        {
            handler->PSendSysMessage("【基础属性】（替换官方属性）");
            for (size_t i = 0; i < data->baseAttributeIds.size(); ++i)
            {
                // 【重要】baseAttributeIds中存的是属性类型，不是模板ID
                ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(data->baseAttributeIds[i]);
                if (attrTemplate)
                {
                    handler->PSendSysMessage("  {}: {} +{} (ID: {})", 
                        i + 1, attrTemplate->clientDisplay, data->baseAttributeValues[i], data->baseAttributeIds[i]);
                }
                else
                {
                    handler->PSendSysMessage("  {}: [未知] +{} (ID: {})", 
                        i + 1, data->baseAttributeValues[i], data->baseAttributeIds[i]);
                }
            }
        }

        // 显示追加属性
        if (!data->additionalAttributeIds.empty())
        {
            handler->PSendSysMessage("【追加属性】（额外添加）");
            for (size_t i = 0; i < data->additionalAttributeIds.size(); ++i)
            {
                // 【重要】additionalAttributeIds中存的是属性类型，不是模板ID
                ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(data->additionalAttributeIds[i]);
                if (attrTemplate)
                {
                    handler->PSendSysMessage("  {}: {} +{} (ID: {})", 
                        i + 1, attrTemplate->clientDisplay, data->additionalAttributeValues[i], data->additionalAttributeIds[i]);
                }
                else
                {
                    handler->PSendSysMessage("  {}: [未知] +{} (ID: {})", 
                        i + 1, data->additionalAttributeValues[i], data->additionalAttributeIds[i]);
                }
            }
        }

        handler->PSendSysMessage("========================");
        
        delete data;
        return true;
    }

    static bool HandleAttributesGenerateCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 获取手持物品
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        if (!item)
        {
            handler->SendSysMessage("请手持一件物品");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 解析参数：random [数量] 或 group [组ID] [数量]
        char* typeStr = strtok((char*)args, " ");
        char* param1Str = strtok(nullptr, " ");
        char* param2Str = strtok(nullptr, " ");

        if (!typeStr)
        {
            handler->SendSysMessage("用法: .物品属性 生成 random <数量>");
            handler->SendSysMessage("     .物品属性 生成 group <组ID> <数量>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string type(typeStr);
        uint32 count = 1;
        uint32 groupId = 0;

        if (type == "random")
        {
            if (param1Str)
                count = atoi(param1Str);
        }
        else if (type == "group")
        {
            if (!param1Str)
            {
                handler->SendSysMessage("用法: .物品属性 生成 group <组ID> <数量>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            groupId = atoi(param1Str);
            if (param2Str)
                count = atoi(param2Str);
        }
        else
        {
            handler->SendSysMessage("用法: .物品属性 生成 random <数量>");
            handler->SendSysMessage("     .物品属性 生成 group <组ID> <数量>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 限制数量
        if (count < 1 || count > 10)
        {
            handler->SendSysMessage("属性数量必须在1-10之间");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 获取属性模板列表
        std::vector<ItemAttributeTemplate const*> templates;
        if (groupId > 0)
        {
            templates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(groupId);
        }
        else
        {
            templates = sItemAttributesLoader->GetAllItemAttributeTemplates();
        }

        if (templates.empty())
        {
            handler->PSendSysMessage("找不到可用的属性模板 (组ID: {})", groupId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint64 itemGuid = item->GetGUID().GetCounter();
        uint32 itemId = item->GetEntry();
        uint32 itemLevel = item->GetTemplate()->ItemLevel;
        uint32 successCount = 0;

        // 随机生成属性
        for (uint32 i = 0; i < count && !templates.empty(); ++i)
        {
            // 随机选择一个属性
            uint32 index = urand(0, templates.size() - 1);
            ItemAttributeTemplate const* attrTemplate = templates[index];
            
            // 计算属性值
            int32 minValue = attrTemplate->minPercent;
            int32 maxValue = attrTemplate->maxPercent;
            int32 attributeValue = urand(minValue, maxValue);

            if (attrTemplate->calcType == 1) // 乘以物品等级
            {
                attributeValue = (attributeValue * itemLevel) / 100;
            }
            else if (attrTemplate->calcType == 2) // 固定值
            {
                // 已经是最终值
            }

            // 添加属性
            if (ItemAttributesDBHelper::AddAttributeToItem(itemGuid, itemId, attrTemplate->id, attributeValue))
            {
                handler->PSendSysMessage("生成属性: {} +{}", attrTemplate->clientDisplay, attributeValue);
                successCount++;
                
                // 从列表中移除已使用的属性（避免重复）
                templates.erase(templates.begin() + index);
            }
        }

        if (successCount > 0)
        {
            // 将真实GUID写入randomPropertyId字段
            int32 randomPropertyId = static_cast<int32>(itemGuid);
            
            // 直接设置字段值并标记为已修改
            item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, randomPropertyId);
            item->SetState(ITEM_CHANGED, player);
            item->SaveToDB(nullptr);
            
            handler->PSendSysMessage("成功为物品 [{}] 生成 {} 个属性", 
                item->GetTemplate()->Name1, successCount);
            
            // 更新物品显示（如果已装备）
            if (item->IsEquipped())
            {
                player->_ApplyItemMods(item, item->GetSlot(), false);
                player->_ApplyItemMods(item, item->GetSlot(), true);
            }
            
            return true;
        }
        else
        {
            handler->SendSysMessage("生成属性失败");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    static bool HandleAttributesRefreshCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 移除所有已装备物品的属性效果
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                // 【关键修复】添加空指针检查
                if (sItemAttributesEffects)
                {
                    sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                }
            }
        }

        // 重新应用所有已装备物品的属性效果
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                // 【关键修复】添加空指针检查
                if (sItemAttributesEffects)
                {
                    sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
                }
            }
        }

        // 刷新玩家属性
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();

        handler->SendSysMessage("已刷新所有装备的追加属性");
        return true;
    }

    static bool HandleAttributesReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        sItemAttributesLoader->LoadItemAttributeTemplates();
        handler->SendSysMessage("物品属性模板已重新加载");
        return true;
    }

    static bool HandleAttributesCleanupCommand(ChatHandler* handler, const char* /*args*/)
    {
        // 清理孤立的属性数据（物品已删除但数据仍存在）
        sItemAttributesLoader->CleanupOrphanedAttributeData();
        handler->SendSysMessage("已清理孤立的物品属性数据");
        return true;
    }
};

// 添加更多命令处理器
class ItemAttributes_GenerateCommandScript : public CommandScript
{
public:
    ItemAttributes_GenerateCommandScript() : CommandScript("ItemAttributes_GenerateCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable generateCommandTable =
        {
            { "random",     HandleGenerateRandomCommand,     SEC_ADMINISTRATOR, Console::No },
            { "bylevel",    HandleGenerateByLevelCommand,    SEC_ADMINISTRATOR, Console::No },
            { "byquality",  HandleGenerateByQualityCommand,  SEC_ADMINISTRATOR, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "物品属性生成",   generateCommandTable }
        };

        return commandTable;
    }

    static bool HandleGenerateRandomCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .物品属性生成 random [物品ID] [最小属性数] [最大属性数] [属性组ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* itemIdStr = strtok((char*)args, " ");
        char* minAttrStr = strtok(nullptr, " ");
        char* maxAttrStr = strtok(nullptr, " ");
        char* groupIdStr = strtok(nullptr, " ");

        if (!itemIdStr)
        {
            handler->SendSysMessage("用法: .物品属性生成 random [物品ID] [最小属性数] [最大属性数] [属性组ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 minAttr = minAttrStr ? atoi(minAttrStr) : 1;
        uint32 maxAttr = maxAttrStr ? atoi(maxAttrStr) : minAttr;
        uint32 groupId = groupIdStr ? atoi(groupIdStr) : 0;

        Player* player = handler->GetSession()->GetPlayer();
        Item* item = nullptr;

        // 查找物品
        for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == itemId)
                {
                    item = pItem;
                    break;
                }
            }
        }

        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                if (Bag* pBag = player->GetBagByPos(i))
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        if (Item* pItem = player->GetItemByPos(i, j))
                        {
                            if (pItem->GetEntry() == itemId)
                            {
                                item = pItem;
                                break;
                            }
                        }
                    }
                }
                if (item)
                    break;
            }
        }

        if (!item)
        {
            handler->PSendSysMessage("找不到物品ID为 {} 的物品", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 设置生成选项
        ItemAttributeGenerateOptions options;
        options.minAttributes = minAttr;
        options.maxAttributes = maxAttr;
        options.attributeGroup = groupId;
        options.minItemLevel = 0;
        options.maxItemLevel = 0;
        options.respectChance = true;
        options.allowDuplicateTypes = false;

        // 生成属性
        bool result = sItemAttributesGenerator->GenerateRandomAttributes(item, options);

        if (result)
        {
            handler->PSendSysMessage("已成功为物品 {} 生成随机属性", item->GetTemplate()->Name1);
            return true;
        }
        else
        {
            handler->PSendSysMessage("为物品 {} 生成随机属性失败", item->GetTemplate()->Name1);
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    static bool HandleGenerateByLevelCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .物品属性生成 bylevel [物品ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(args);
        Player* player = handler->GetSession()->GetPlayer();
        Item* item = nullptr;

        // 查找物品
        for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == itemId)
                {
                    item = pItem;
                    break;
                }
            }
        }

        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                if (Bag* pBag = player->GetBagByPos(i))
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        if (Item* pItem = player->GetItemByPos(i, j))
                        {
                            if (pItem->GetEntry() == itemId)
                            {
                                item = pItem;
                                break;
                            }
                        }
                    }
                }
                if (item)
                    break;
            }
        }

        if (!item)
        {
            handler->PSendSysMessage("找不到物品ID为 {} 的物品", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 根据物品等级生成属性
        bool result = sItemAttributesGenerator->GenerateAttributesByItemLevel(item);

        if (result)
        {
            handler->PSendSysMessage("已成功根据物品等级为物品 {} 生成属性", item->GetTemplate()->Name1);
            return true;
        }
        else
        {
            handler->PSendSysMessage("根据物品等级为物品 {} 生成属性失败", item->GetTemplate()->Name1);
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    static bool HandleGenerateByQualityCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .物品属性生成 byquality [物品ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(args);
        Player* player = handler->GetSession()->GetPlayer();
        Item* item = nullptr;

        // 查找物品
        for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == itemId)
                {
                    item = pItem;
                    break;
                }
            }
        }

        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                if (Bag* pBag = player->GetBagByPos(i))
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        if (Item* pItem = player->GetItemByPos(i, j))
                        {
                            if (pItem->GetEntry() == itemId)
                            {
                                item = pItem;
                                break;
                            }
                        }
                    }
                }
                if (item)
                    break;
            }
        }

        if (!item)
        {
            handler->PSendSysMessage("找不到物品ID为 {} 的物品", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 根据物品品质生成属性
        bool result = sItemAttributesGenerator->GenerateAttributesByItemQuality(item);

        if (result)
        {
            handler->PSendSysMessage("已成功根据物品品质为物品 {} 生成属性", item->GetTemplate()->Name1);
            return true;
        }
        else
        {
            handler->PSendSysMessage("根据物品品质为物品 {} 生成属性失败", item->GetTemplate()->Name1);
            handler->SetSentErrorMessage(true);
            return false;
        }
    }
};

// 添加所有脚本
void Addmod_item_attributesScripts()
{
    new ItemAttributesWorldScript();
    new ItemAttributes_CommandScript();
    new ItemAttributes_GenerateCommandScript();
    new ItemAttributesEvents();
    new ItemAttributesGlobalScript();  // 处理物品删除事件
}

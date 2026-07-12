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
#include "StringConvert.h"
#include "Util.h"
#include "World.h"
#include <algorithm>
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

            // 【关键修复】使用不安全的版本进行初始化，因为此时系统尚未标记为已初始化
            // 加载物品属性模板
            sItemAttributesLoaderUnsafe->LoadItemAttributeTemplates();
            uint32 templateCount = sItemAttributesLoaderUnsafe->GetAllItemAttributeTemplates().size();

            // 初始化物品属性效果系统
            sItemAttributesEffects->Initialize();

            // 初始化物品属性显示系统
            sItemAttributesDisplay->Initialize();

            // 初始化物品属性生成系统
            sItemAttributesGenerator->Initialize();

            LOG_INFO("server.loading", "→物品属性系统√");

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
    std::vector<int256> vals;
    
    // 解析ID列表
    std::istringstream idsStream(attrIds);
    std::string id;
    while (std::getline(idsStream, id, ','))
    {
        if (!id.empty())
        {
            if (Optional<uint32> parsedId = Acore::StringTo<uint32>(id))
                ids.push_back(*parsedId);
        }
    }
    
    // 解析值列表
    std::istringstream valsStream(attrVals);
    std::string val;
    while (std::getline(valsStream, val, ','))
    {
        if (!val.empty())
        {
            if (Optional<int256> parsedValue = Acore::StringTo<int256>(val))
                vals.push_back(*parsedValue);
        }
    }
    
    // 构造 "属性名+值" 格式
    std::string result;
    for (size_t i = 0; i < ids.size() && i < vals.size(); ++i)
    {
        if (i > 0)
            result += ",";
        
        // 【重要】ids中存的是属性类型，不是模板ID
        // 【审计修复】SafeInstance 在模板表缺失时返回 nullptr，判空后走"未知属性"分支
        ItemAttributeTemplate const* tpl = sItemAttributesLoader ? sItemAttributesLoader->GetItemAttributeTemplateByType(ids[i]) : nullptr;
        if (tpl)
            result += tpl->clientDisplay + "+" + Acore::ToString(vals[i]);
        else
            result += "未知属性+" + Acore::ToString(vals[i]);
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

        // 【审计修复】使用 istringstream 替代 strtok，线程安全
        // 解析参数: itemID clientGuid [bag] [slot] [equipFlag] [rpId]
        std::istringstream iss(args);
        std::string itemIdStr, clientGuidStr, bagStr, slotStr, equipStr, rpIdStr;

        iss >> itemIdStr >> clientGuidStr >> bagStr >> slotStr >> equipStr >> rpIdStr;

        if (itemIdStr.empty() || clientGuidStr.empty())
            return true; // 静默失败

        // 【审计修复】使用 stoul/stoll 替代 atoi，避免截断和更好的错误处理
        uint32 itemId = 0;
        uint64 clientGuid = 0;
        int32 bag = -1;
        int32 slot = -1;
        bool isEquip = false;

        try {
            itemId = static_cast<uint32>(std::stoul(itemIdStr));
            clientGuid = std::stoull(clientGuidStr);
            if (!bagStr.empty()) bag = std::stoi(bagStr);
            if (!slotStr.empty()) slot = std::stoi(slotStr);
            if (!equipStr.empty()) isEquip = (std::stoi(equipStr) != 0);
        } catch (const std::exception&) {
            return true; // 静默失败
        }

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
        auto data = ItemAttributesDBHelper::LoadItemAttributes(realGuid);  // 【智能指针修复】
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

            // 【智能指针修复】移除 delete，自动清理
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
        {
            // 【审计修复】使用 strtoul 替代 atoi，带错误检查
            char* endPtr = nullptr;
            unsigned long val = strtoul(args, &endPtr, 10);
            if (endPtr != args && val <= UINT32_MAX)
                group = static_cast<uint32>(val);
        }

        // 【审计修复】添加空指针保护
        if (!sItemAttributesLoader)
        {
            handler->SendSysMessage("物品属性系统尚未初始化");
            handler->SetSentErrorMessage(true);
            return false;
        }

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

        // 【安全修复】使用 std::istringstream 替代 strtok/atoi
        std::istringstream iss(args);
        std::string attributeIdStr, itemIdStr, attributeValueStr;
        iss >> attributeIdStr >> itemIdStr >> attributeValueStr;

        if (attributeIdStr.empty() || itemIdStr.empty())
        {
            handler->SendSysMessage("用法: .物品属性 添加 <属性ID> <物品ID> [属性值]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 安全解析参数
        char* endPtr = nullptr;
        unsigned long attrLong = strtoul(attributeIdStr.c_str(), &endPtr, 10);
        uint32 attributeId = (endPtr != attributeIdStr.c_str() && attrLong <= UINT32_MAX) ? static_cast<uint32>(attrLong) : 0;

        endPtr = nullptr;
        unsigned long itemLong = strtoul(itemIdStr.c_str(), &endPtr, 10);
        uint32 itemId = (endPtr != itemIdStr.c_str() && itemLong <= UINT32_MAX) ? static_cast<uint32>(itemLong) : 0;

        if (attributeId == 0 || itemId == 0)
        {
            handler->SendSysMessage("无效的属性ID或物品ID");
            handler->SetSentErrorMessage(true);
            return false;
        }
        
        // 验证属性是否存在
        // 【审计修复】SafeInstance 可为空，判空后走下方"属性ID无效"分支
        ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader ? sItemAttributesLoader->GetItemAttributeTemplate(attributeId) : nullptr;
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
        int256 attributeValue = 0;
        if (!attributeValueStr.empty())
        {
            if (Optional<int256> parsedValue = Acore::StringTo<int256>(attributeValueStr))
                attributeValue = *parsedValue;
        }
        else
        {
            // 使用模板中的随机范围生成属性值
            uint32 itemLevel = item->GetTemplate()->ItemLevel;
            int256 minPercent = attrTemplate->minPercent;
            int256 maxPercent = attrTemplate->maxPercent;
            if (minPercent > maxPercent)
                std::swap(minPercent, maxPercent);
            
            if (attrTemplate->calcType == 1) // 乘以物品等级
            {
                int256 baseValue = minPercent + ((maxPercent - minPercent) / 2);
                attributeValue = Acore::Number::CalculatePct(baseValue, itemLevel);
            }
            else if (attrTemplate->calcType == 2) // 固定值
            {
                attributeValue = minPercent + ((maxPercent - minPercent) / 2);
            }
            else // 常规值
            {
                attributeValue = minPercent + ((maxPercent - minPercent) / 2);
            }
        }

        // 添加属性到数据库
        uint64 itemGuid = item->GetGUID().GetCounter();
        uint32 itemEntry = item->GetEntry();
        // 【审计修复】保存属性类型而不是模板ID，确保与效果系统一致
        uint32 attrTypeToSave = attrTemplate->attributeType;
        if (ItemAttributesDBHelper::AddAttributeToItem(itemGuid, itemEntry, attrTypeToSave, attributeValue))
        {
            // 【审计修复】使用特殊标记值而不是GUID，避免64位GUID转32位溢出
            // 使用负值 -1 作为"有自定义属性"的标记，不会与正常的随机属性ID冲突
            constexpr int32 CUSTOM_ATTR_MARKER = -1;

            // 直接设置字段值并标记为已修改
            item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, CUSTOM_ATTR_MARKER);
            item->SetState(ITEM_CHANGED, player);
            item->SaveToDB(nullptr); // 保存到数据库
            
            handler->PSendSysMessage("成功为物品 [{}] (ID: {}) 添加属性: {} +{}", 
                item->GetTemplate()->Name1, itemId, attrTemplate->clientDisplay, Acore::ToString(attributeValue));
            
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

        // 【审计修复】使用 std::istringstream 替代 strtok/atoi
        std::istringstream iss(args);
        std::string attributeIdStr, itemIdStr;
        iss >> attributeIdStr >> itemIdStr;

        if (attributeIdStr.empty() || itemIdStr.empty())
        {
            handler->SendSysMessage("用法: .物品属性 删除 <属性ID> <物品ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 【审计修复】使用 strtoul 替代 atoi，带错误检查
        char* endPtr = nullptr;
        unsigned long attrLong = strtoul(attributeIdStr.c_str(), &endPtr, 10);
        uint32 attributeId = (endPtr != attributeIdStr.c_str() && attrLong <= UINT32_MAX) ? static_cast<uint32>(attrLong) : 0;

        endPtr = nullptr;
        unsigned long itemLong = strtoul(itemIdStr.c_str(), &endPtr, 10);
        uint32 itemId = (endPtr != itemIdStr.c_str() && itemLong <= UINT32_MAX) ? static_cast<uint32>(itemLong) : 0;

        if (attributeId == 0 || itemId == 0)
        {
            handler->SendSysMessage("无效的属性ID或物品ID");
            handler->SetSentErrorMessage(true);
            return false;
        }
        
        // 验证属性是否存在
        // 【审计修复】SafeInstance 可为空，判空后走下方"属性ID无效"分支
        ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader ? sItemAttributesLoader->GetItemAttributeTemplate(attributeId) : nullptr;
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

        // 【审计修复】使用 strtoul 替代 atoi，带错误检查
        char* endPtr = nullptr;
        unsigned long val = strtoul(args, &endPtr, 10);
        uint32 itemId = (endPtr != args && val <= UINT32_MAX) ? static_cast<uint32>(val) : 0;

        if (itemId == 0)
        {
            handler->SendSysMessage("无效的物品ID");
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
        auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);  // 【智能指针修复】

        if (!data || (data->baseAttributeIds.empty() && data->additionalAttributeIds.empty()))
        {
            handler->PSendSysMessage("物品 [{}] (GUID: {}) 没有自定义属性",
                item->GetTemplate()->Name1, itemGuid);
            // 【智能指针修复】移除 delete，自动清理
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
                ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader ? sItemAttributesLoader->GetItemAttributeTemplateByType(data->baseAttributeIds[i]) : nullptr; // 【审计修复】判空
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
                ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader ? sItemAttributesLoader->GetItemAttributeTemplateByType(data->additionalAttributeIds[i]) : nullptr; // 【审计修复】判空
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

        // 【智能指针修复】移除 delete，自动清理
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
        // 【审计修复】使用 std::istringstream 替代 strtok/atoi
        std::istringstream iss(args);
        std::string typeStr, param1Str, param2Str;
        iss >> typeStr >> param1Str >> param2Str;

        if (typeStr.empty())
        {
            handler->SendSysMessage("用法: .物品属性 生成 random <数量>");
            handler->SendSysMessage("     .物品属性 生成 group <组ID> <数量>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 count = 1;
        uint32 groupId = 0;

        if (typeStr == "random")
        {
            if (!param1Str.empty())
            {
                char* endPtr = nullptr;
                unsigned long val = strtoul(param1Str.c_str(), &endPtr, 10);
                if (endPtr != param1Str.c_str() && val <= UINT32_MAX)
                    count = static_cast<uint32>(val);
            }
        }
        else if (typeStr == "group")
        {
            if (param1Str.empty())
            {
                handler->SendSysMessage("用法: .物品属性 生成 group <组ID> <数量>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            char* endPtr = nullptr;
            unsigned long val = strtoul(param1Str.c_str(), &endPtr, 10);
            if (endPtr != param1Str.c_str() && val <= UINT32_MAX)
                groupId = static_cast<uint32>(val);

            if (!param2Str.empty())
            {
                endPtr = nullptr;
                val = strtoul(param2Str.c_str(), &endPtr, 10);
                if (endPtr != param2Str.c_str() && val <= UINT32_MAX)
                    count = static_cast<uint32>(val);
            }
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
        // 【审计修复】SafeInstance 可为空，判空后走下方空列表提示分支（后面的 CalculateAttributeValue 仅在列表非空时可达）
        if (sItemAttributesLoader)
        {
            if (groupId > 0)
            {
                templates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(groupId);
            }
            else
            {
                templates = sItemAttributesLoader->GetAllItemAttributeTemplates();
            }
        }

        if (templates.empty())
        {
            handler->PSendSysMessage("找不到可用的属性模板 (组ID: {})", groupId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint64 itemGuid = item->GetGUID().GetCounter();
        uint32 itemId = item->GetEntry();
        uint32 successCount = 0;

        // 随机生成属性
        for (uint32 i = 0; i < count && !templates.empty(); ++i)
        {
            // 随机选择一个属性
            uint32 index = urand(0, templates.size() - 1);
            ItemAttributeTemplate const* attrTemplate = templates[index];
            
            // 计算属性值
            int256 attributeValue = sItemAttributesLoader->CalculateAttributeValue(item, attrTemplate);

            // 添加属性
            // 【审计修复】保存属性类型而不是模板ID，确保与效果系统一致
            if (ItemAttributesDBHelper::AddAttributeToItem(itemGuid, itemId, attrTemplate->attributeType, attributeValue))
            {
                handler->PSendSysMessage("生成属性: {} +{}", attrTemplate->clientDisplay, Acore::ToString(attributeValue));
                successCount++;
                
                // 从列表中移除已使用的属性（避免重复）
                templates.erase(templates.begin() + index);
            }
        }

        if (successCount > 0)
        {
            // 【审计修复】使用特殊标记值而不是GUID，避免64位GUID转32位溢出
            constexpr int32 CUSTOM_ATTR_MARKER = -1;

            // 直接设置字段值并标记为已修改
            item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, CUSTOM_ATTR_MARKER);
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
        // 【关键修复】使用不安全的版本进行重新加载
        sItemAttributesLoaderUnsafe->LoadItemAttributeTemplates();
        handler->SendSysMessage("物品属性模板已重新加载");
        return true;
    }

    static bool HandleAttributesCleanupCommand(ChatHandler* handler, const char* /*args*/)
    {
        // 【审计修复】添加空指针保护
        if (!sItemAttributesLoader)
        {
            handler->SendSysMessage("物品属性系统尚未初始化");
            handler->SetSentErrorMessage(true);
            return false;
        }
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

        // 【审计修复】使用 std::istringstream 替代 strtok/atoi
        std::istringstream iss(args);
        std::string itemIdStr, minAttrStr, maxAttrStr, groupIdStr;
        iss >> itemIdStr >> minAttrStr >> maxAttrStr >> groupIdStr;

        if (itemIdStr.empty())
        {
            handler->SendSysMessage("用法: .物品属性生成 random [物品ID] [最小属性数] [最大属性数] [属性组ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 【审计修复】使用 strtoul 替代 atoi，带错误检查
        char* endPtr = nullptr;
        unsigned long val = strtoul(itemIdStr.c_str(), &endPtr, 10);
        uint32 itemId = (endPtr != itemIdStr.c_str() && val <= UINT32_MAX) ? static_cast<uint32>(val) : 0;

        uint32 minAttr = 1;
        if (!minAttrStr.empty())
        {
            endPtr = nullptr;
            val = strtoul(minAttrStr.c_str(), &endPtr, 10);
            if (endPtr != minAttrStr.c_str() && val <= UINT32_MAX)
                minAttr = static_cast<uint32>(val);
        }

        uint32 maxAttr = minAttr;
        if (!maxAttrStr.empty())
        {
            endPtr = nullptr;
            val = strtoul(maxAttrStr.c_str(), &endPtr, 10);
            if (endPtr != maxAttrStr.c_str() && val <= UINT32_MAX)
                maxAttr = static_cast<uint32>(val);
        }

        uint32 groupId = 0;
        if (!groupIdStr.empty())
        {
            endPtr = nullptr;
            val = strtoul(groupIdStr.c_str(), &endPtr, 10);
            if (endPtr != groupIdStr.c_str() && val <= UINT32_MAX)
                groupId = static_cast<uint32>(val);
        }

        if (itemId == 0)
        {
            handler->SendSysMessage("无效的物品ID");
            handler->SetSentErrorMessage(true);
            return false;
        }

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

        // 【审计修复】使用 strtoul 替代 atoi，带错误检查
        char* endPtr = nullptr;
        unsigned long val = strtoul(args, &endPtr, 10);
        uint32 itemId = (endPtr != args && val <= UINT32_MAX) ? static_cast<uint32>(val) : 0;

        if (itemId == 0)
        {
            handler->SendSysMessage("无效的物品ID");
            handler->SetSentErrorMessage(true);
            return false;
        }

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

        // 【审计修复】使用 strtoul 替代 atoi，带错误检查
        char* endPtr = nullptr;
        unsigned long val = strtoul(args, &endPtr, 10);
        uint32 itemId = (endPtr != args && val <= UINT32_MAX) ? static_cast<uint32>(val) : 0;

        if (itemId == 0)
        {
            handler->SendSysMessage("无效的物品ID");
            handler->SetSentErrorMessage(true);
            return false;
        }

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

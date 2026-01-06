#include "ItemAttributesEvents.h"
#include "ItemAttributesLoader.h"
#include "ItemAttributesEffects.h"
#include "ItemAttributesDisplay.h"
#include "ItemAttributesGenerator.h"
#include "ItemAttributesDBHelper.h"
#include "Configuration/Config.h"
#include "Logging/Log.h"
#include <chrono>

ItemAttributesEvents::ItemAttributesEvents() :
    PlayerScript("ItemAttributesEvents"),
    ItemScript("ItemAttributesEvents")
{
}

void ItemAttributesEvents::OnPlayerLogin(Player* player)
{
    auto loginStart = std::chrono::high_resolution_clock::now();

    if (!player || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 【性能优化】登录时不再刷新玩家属性
    // 原因：
    // 1. OnPlayerEquip 已经为每个装备应用了属性（已优化为跳过Update）
    // 2. 幻境系统的 OnPlayerLogin 会在最后统一刷新所有属性
    // 3. 避免多个系统重复调用 UpdateAllStats，节省约 39ms
    //
    // 如果需要单独刷新，可以在配置中启用 ItemAttributes.ForceRefreshOnLogin

    auto step1Start = std::chrono::high_resolution_clock::now();
    bool forceRefresh = sConfigMgr->GetOption<bool>("ItemAttributes.ForceRefreshOnLogin", false);
    if (forceRefresh)
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();
    }
    auto step1End = std::chrono::high_resolution_clock::now();
    auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

    // 可以在这里添加登录消息
    if (sConfigMgr->GetOption<bool>("ItemAttributes.LoginMessage", false))
    {
        player->GetSession()->SendAreaTriggerMessage("物品追加属性系统已加载");
    }

    auto loginEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loginEnd - loginStart).count();

    // 【性能诊断】记录登录处理耗时
    if (totalDuration > 100)
    {
        LOG_WARN("module.itemattributes.perf", "OnPlayerLogin 耗时过长: {}ms (玩家={}, 强制刷新={})",
            totalDuration, player->GetName(), forceRefresh);
    }
    else if (sConfigMgr->GetOption<bool>("ItemAttributes.Debug.Performance", false))
    {
        LOG_DEBUG("module.itemattributes.perf", "OnPlayerLogin 完成: {}ms (强制刷新={}ms)",
            totalDuration, step1Duration);
    }
}

void ItemAttributesEvents::OnPlayerLogout(Player* player)
{
    if (!player)
        return;

    // 【根本性修复】线程安全地清理该玩家的装备跟踪数据
    uint64 playerGuid = player->GetGUID().GetCounter();
    {
        std::lock_guard<std::mutex> lock(_equippedItemsMutex);
        _equippedItems.erase(playerGuid);
    }

    // 玩家登出时清理孤立的属性数据（频率限制）
    // 说明：该清理会触发对 item_instance 的子查询/删除，容易与物品保存事务产生锁竞争。
    // 为减少 [1213] deadlock，改为按时间间隔执行。
    static std::chrono::steady_clock::time_point lastCleanup;
    auto now = std::chrono::steady_clock::now();
    if (lastCleanup.time_since_epoch().count() == 0 || (now - lastCleanup) > std::chrono::minutes(30))
    {
        lastCleanup = now;
        // 【审计修复】添加空指针保护和模块启用检查
        if (sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true) && sItemAttributesLoader)
        {
            sItemAttributesLoader->CleanupOrphanedAttributeData();
        }
    }
}

void ItemAttributesEvents::OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid)
{
    if (!sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 当角色被删除时，清理该角色所有物品的属性数据
    // 【审计修复】使用传入的事务来确保数据一致性
    if (trans)
    {
        trans->Append(Acore::StringFormat(
            "DELETE FROM `物品属性_数据` WHERE `物品GUID` IN "
            "(SELECT `guid` FROM `item_instance` WHERE `owner_guid` = {})", guid));
    }
    else
    {
        // 如果没有事务，使用异步执行
        CharacterDatabase.Execute(
            "DELETE FROM `物品属性_数据` WHERE `物品GUID` IN "
            "(SELECT `guid` FROM `item_instance` WHERE `owner_guid` = {})", guid);
    }
    ItemAttributesDBHelper::FlushCache();
}

void ItemAttributesEvents::OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!player || !item || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 只处理真正的装备槽位（背包装备、银行等忽略），避免不必要的属性应用
    // 【性能优化】提前检查，避免不必要的GUID获取
    if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
        return;

    // 【性能诊断】记录开始时间
    using namespace std::chrono;
    auto perfStart = high_resolution_clock::now();

    // 【关键修复】检查物品是否已完全初始化
    // 在玩家登录时加载装备的过程中，Item对象的m_uint32Values可能还未初始化
    // 此时调用GetGUID()会触发"GetGuidValue called on uninitialized object"错误
    ObjectGuid itemGuidObj = item->GetGUID();
    if (itemGuidObj.IsEmpty())
    {
        // 物品尚未初始化，跳过此次处理
        // 注意：这不是错误，只是初始化顺序问题，后续会再次触发OnPlayerEquip
        return;
    }

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint32 itemEntry = item->GetEntry();
    uint64 itemGuid = itemGuidObj.GetCounter();

    // 【性能诊断】是否在加载状态
    bool isLoading = player->isBeingLoaded();
    if (sConfigMgr->GetOption<bool>("ItemAttributes.Debug.Performance", false))
    {
        LOG_DEBUG("module.itemattributes.perf", "OnPlayerEquip: 玩家={} 物品={} 槽位={} 加载中={}",
            player->GetName(), itemEntry, slot, isLoading);
    }

    // 【根本性修复】线程安全地检查和更新装备映射
    uint64 oldItemGuid = 0;
    bool hasOldItem = false;
    {
        std::lock_guard<std::mutex> lock(_equippedItemsMutex);
        auto& playerEquipMap = _equippedItems[playerGuid];
        auto it = playerEquipMap.find(slot);
        if (it != playerEquipMap.end())
        {
            oldItemGuid = it->second;
            hasOldItem = true;
        }
        // 记录新装备
        playerEquipMap[slot] = itemGuid;
    }

    // 如果有旧装备，移除其属性效果
    if (hasOldItem)
    {
        // 通过 GUID 查找旧物品
        // 注意：此时旧物品可能已经不在装备槽了，可能在背包中
        // 我们需要遍历玩家的所有物品来查找
        Item* oldItem = nullptr;
        // 先检查背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* bagItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                // 【关键修复】检查物品是否已完全初始化
                ObjectGuid bagItemGuid = bagItem->GetGUID();
                if (!bagItemGuid.IsEmpty() && bagItemGuid.GetCounter() == oldItemGuid)
                {
                    oldItem = bagItem;
                    break;
                }
            }
        }
        
        if (sItemAttributesEffects)
        {
            if (oldItem)
                sItemAttributesEffects->RemoveItemAttributeEffects(player, oldItem);
            else
                sItemAttributesEffects->RemoveItemAttributeEffectsByGuid(player, oldItemGuid);
        }
    }

    // 【修复】移除重复代码：playerEquipMap[slot] = itemGuid 已在上面的lock作用域中执行

    // 【关键修复】添加空指针检查和安全性验证
    // 应用新装备的属性
    if (sItemAttributesEffects)
    {
        sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
    }

    // 刷新玩家属性交由 ItemAttributesEffects 内部统一合并处理，
    // 避免每次换装都对全身装备做“移除→UpdateAllStats→重加”的 O(装备数) 操作。

    auto perfEnd = high_resolution_clock::now();
    auto perfMs = duration_cast<milliseconds>(perfEnd - perfStart).count();

    // 【性能诊断】如果耗时超过阈值，记录警告
    if (perfMs > 50)  // 超过50ms记录警告
    {
        LOG_WARN("module.itemattributes.perf", "OnPlayerEquip 耗时过长: {}ms (玩家={}, 物品={}, 槽位={}, 加载中={})",
            perfMs, player->GetName(), itemEntry, slot, isLoading);
    }
    else if (sConfigMgr->GetOption<bool>("ItemAttributes.Debug.Performance", false))
    {
        LOG_DEBUG("module.itemattributes.perf", "OnPlayerEquip 完成: {}ms", perfMs);
    }
}

void ItemAttributesEvents::OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item)
{
    if (!player || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 这个钩子在装备槽的可见物品改变时触发
    // 穿戴装备时：item != nullptr
    // 卸下装备时：item == nullptr
    
    if (item == nullptr)
    {
        uint64 playerGuid = player->GetGUID().GetCounter();
        uint64 oldItemGuid = 0;
        bool found = false;

        // 【根本性修复】线程安全地从映射表中获取该槽位之前的物品GUID
        {
            std::lock_guard<std::mutex> lock(_equippedItemsMutex);
            auto playerIt = _equippedItems.find(playerGuid);
            if (playerIt != _equippedItems.end())
            {
                auto& playerEquipMap = playerIt->second;
                auto slotIt = playerEquipMap.find(slot);
                if (slotIt != playerEquipMap.end())
                {
                    oldItemGuid = slotIt->second;
                    found = true;
                    // 从映射表中移除
                    playerEquipMap.erase(slotIt);
                }
            }
        }

        if (found)
        {
            // 【关键修复】添加空指针检查
            // 直接根据GUID移除属性，不需要查找Item对象
            if (sItemAttributesEffects)
            {
                sItemAttributesEffects->RemoveItemAttributeEffectsByGuid(player, oldItemGuid);
            }
        }
    }
}

// OnDummyEffect 不是 ItemScript 的虚函数，已移除

bool ItemAttributesEvents::OnQuestAccept(Player* player, Item* item, Quest const* quest)
{
    // 这里可以处理物品接受任务时的逻辑
    return false; // 返回true表示处理了事件，返回false表示继续处理
}

bool ItemAttributesEvents::OnUse(Player* player, Item* item, SpellCastTargets const& targets)
{
    // 这里可以处理物品使用时的逻辑
    return false; // 返回true表示处理了事件，返回false表示继续处理
}

bool ItemAttributesEvents::OnExpire(Player* player, ItemTemplate const* proto)
{
    // 这里可以处理物品过期时的逻辑
    return false; // 返回true表示处理了事件，返回false表示继续处理
}

bool ItemAttributesEvents::OnRemove(Player* player, Item* item)
{
    // 注意：这个事件不会在物品删除时触发
    // 物品删除时的清理由 ItemAttributesGlobalScript::OnItemDelFromDB 处理
    return false;
}

void ItemAttributesEvents::OnGossipSelect(Player* player, Item* item, uint32 sender, uint32 action)
{
    // 这里可以处理物品对话选择时的逻辑
}


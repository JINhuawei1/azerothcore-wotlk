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

    // 玩家登出时清理孤立的属性数据
    // 这会清理所有已删除物品但属性数据仍存在的记录
    // 
    // 清理时机说明：
    // 1. 玩家出售装备到商店 → 属性数据保留（可以买回）
    // 2. 玩家小退 → 清理所有孤立数据（未买回的装备属性被删除）
    // 3. 玩家买回装备 → 属性依然存在 ✅
    //
    // 频率控制：每次登出都清理（确保及时清理）
    static uint32 cleanupCounter = 0;
    if (++cleanupCounter % 1 == 0)  // 每次玩家登出都清理
    {
        sItemAttributesLoader->CleanupOrphanedAttributeData();
    }
}

void ItemAttributesEvents::OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid)
{
    if (!sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 当角色被删除时，清理该角色所有物品的属性数据
    // 使用事务来确保数据一致性
    CharacterDatabase.Execute(
        "DELETE FROM `物品属性_数据` WHERE `物品GUID` IN "
        "(SELECT `guid` FROM `item_instance` WHERE `owner_guid` = {})", guid);
    ItemAttributesDBHelper::FlushCache();
}

void ItemAttributesEvents::OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!player || !item || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    using namespace std::chrono;
    auto perfStart = high_resolution_clock::now();

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint32 itemEntry = item->GetEntry();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 只处理真正的装备槽位（背包装备、银行等忽略），避免不必要的属性应用
    if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
        return;

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
                if (bagItem->GetGUID().GetCounter() == oldItemGuid)
                {
                    oldItem = bagItem;
                    break;
                }
            }
        }
        
        if (oldItem)
        {
            // 【关键修复】添加空指针检查
            if (sItemAttributesEffects)
            {
                sItemAttributesEffects->RemoveItemAttributeEffects(player, oldItem);
            }
        }
    }

    // 【修复】移除重复代码：playerEquipMap[slot] = itemGuid 已在上面的lock作用域中执行

    // 【关键修复】添加空指针检查和安全性验证
    // 应用新装备的属性
    if (sItemAttributesEffects)
    {
        sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
    }

    // 刷新玩家属性：
    // 【性能优化】登录加载阶段不做全量刷新，交给 OnPlayerLogin 统一刷新一次
    // 原因：登录时每件装备都会触发 OnPlayerEquip，如果每次都刷新属性，
    //       9件装备 × 80ms = 720ms 浪费在重复刷新上
    // 优化后：登录阶段跳过刷新，OnPlayerLogin 最后统一刷新一次，节省 ~640ms
    // 正常在线换装时才做即时刷新
    if (!player->isBeingLoaded())
    {
        // 【关键修复】UpdateAllStats会清除自定义属性,所以需要先移除再重新应用
        // 步骤1: 移除所有装备的自定义属性
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (sItemAttributesEffects)
                {
                    sItemAttributesEffects->RemoveItemAttributeEffects(player, equippedItem);
                }
            }
        }

        // 步骤2: 刷新基础属性
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();

        // 步骤3: 重新应用所有装备的自定义属性
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (sItemAttributesEffects)
                {
                    sItemAttributesEffects->ApplyItemAttributeEffects(player, equippedItem);
                }
            }
        }
    }

    auto perfEnd = high_resolution_clock::now();
    auto perfMs = duration_cast<milliseconds>(perfEnd - perfStart).count();
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

            // 【关键修复】UpdateAllStats会清除自定义属性,需要先移除所有装备属性再重新应用
            // 步骤1: 移除所有装备的自定义属性
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                if (Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (sItemAttributesEffects)
                    {
                        sItemAttributesEffects->RemoveItemAttributeEffects(player, equippedItem);
                    }
                }
            }

            // 步骤2: 刷新玩家属性面板
            player->UpdateAllStats();
            player->UpdateAttackPowerAndDamage();
            player->UpdateAttackPowerAndDamage(true);
            player->UpdateSpellDamageAndHealingBonus();

            // 步骤3: 重新应用所有剩余装备的自定义属性
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                if (Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                {
                    if (sItemAttributesEffects)
                    {
                        sItemAttributesEffects->ApplyItemAttributeEffects(player, equippedItem);
                    }
                }
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


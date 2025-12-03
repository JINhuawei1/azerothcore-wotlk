#include "ItemEnhancement.h"
#include "ItemEnhancementMgr.h"
#include "Chat.h"
#include "Player.h"
#include "WorldPacket.h"
#include "GameTime.h"
#include "ScriptMgr.h"
#include "CommandScript.h"
#include "AllItemScript.h"
#include "ObjectAccessor.h"
#include <unordered_map>
#include <chrono>

// 可选集成：物品属性系统（用于在启用时让其统一处理强化带来的属性）
#if __has_include("ItemAttributesLoader.h")
#ifndef MODULE_ITEM_ATTRIBUTES
#define MODULE_ITEM_ATTRIBUTES
#endif
#endif

// 外部函数声明
extern void DeleteEnhancementRecord(uint32 itemGuid);

// ============================================================================
// 【架构重构】参考 mod-item-attributes 的稳定实现
// ============================================================================
//
// 问题根源：
// - ItemEnhancementMgr::PreloadPlayerEnhancementRecords 在登录时崩溃
// - CheckEquipmentChanges 周期性访问 Manager 崩溃
// - 崩溃地址始终在 0xC46XXX 区域，RCX=0, RDX=0 空指针
//
// 新架构（完全参考 ItemAttributesEvents）：
// 1. OnPlayerLogin 几乎不做任何操作（只发消息）
// 2. OnPlayerEquip 使用本地 map 跟踪，使用 isBeingLoaded() 检查
// 3. 完全移除 OnPlayerUpdate 周期性检查（事件驱动）
// 4. 不依赖复杂的 Manager 加载方法
//
// ============================================================================

// 装备跟踪数据（使用本地 map，不依赖全局数据结构）
static std::unordered_map<uint64, std::unordered_map<uint8, uint64>> _equippedEnhancementItems;

// 物品强化处理脚本
class ItemEnhancement_PlayerScript : public PlayerScript
{
public:
    ItemEnhancement_PlayerScript() : PlayerScript("ItemEnhancement_PlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        // 【参考 ItemAttributesEvents::OnPlayerLogin】
        // 登录时不做任何加载操作，只发送可选消息

        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

        // 可选：发送登录消息
        bool loginMessage = sConfigMgr->GetOption<bool>("ItemEnhancement.LoginMessage", false);
        if (loginMessage && player->GetSession())
        {
            std::string msg = "ITEMENHANCE\tLOGIN_COMPLETED";
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, player, player, msg);
            player->GetSession()->SendPacket(&data);
        }

        LOG_DEBUG("module.itemenhancement", "OnPlayerLogin: Player {} - Enhancement system ready (no preloading)",
            player->GetGUID().ToString());
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

        // 清理玩家的装备跟踪数据
        uint64 playerGuid = player->GetGUID().GetCounter();
        _equippedEnhancementItems.erase(playerGuid);

        // 清理内存中的应用记录
        sItemEnhancementMgr->ClearPlayerAppliedEnhancements(player);

        LOG_DEBUG("module.itemenhancement", "OnPlayerLogout: Player {} - Cleaned up tracking data",
            player->GetGUID().ToString());
    }

    // 【完全移除 OnPlayerUpdate】
    // 不再使用周期性检查，完全事件驱动
};

// 装备事件处理脚本
class ItemEnhancement_EquipScript : public PlayerScript
{
public:
    ItemEnhancement_EquipScript() : PlayerScript("ItemEnhancement_EquipScript") { }

    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        // 【完全参考 ItemAttributesEvents::OnPlayerEquip 的实现】

        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return;

        // 只处理装备槽位
        if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
            return;

#ifndef MODULE_ITEM_ATTRIBUTES
        try
        {
            // 【安全检查】确保 Session 存在
            WorldSession* session = player->GetSession();
            if (!session)
            {
                LOG_WARN("module.itemenhancement", "OnPlayerEquip: Player {} has no session",
                    player->GetGUID().ToString());
                return;
            }

            uint64 playerGuid = player->GetGUID().GetCounter();
            uint64 itemGuid = item->GetGUID().GetCounter();

            // 检查该槽位是否已有装备（需要先移除旧装备的强化）
            auto& playerEquipMap = _equippedEnhancementItems[playerGuid];
            auto it = playerEquipMap.find(slot);
            if (it != playerEquipMap.end())
            {
                uint64 oldItemGuid = it->second;

                // 移除旧装备的强化效果
                // 由于旧物品可能已经不在这个槽位了，我们直接清理该槽位的强化效果
                sItemEnhancementMgr->ClearSlotEnhancements(player, slot);

                LOG_DEBUG("module.itemenhancement", "OnPlayerEquip: Cleared old item {} enhancement from slot {} for player {}",
                    oldItemGuid, slot, player->GetGUID().ToString());
            }

            // 记录新装备
            playerEquipMap[slot] = itemGuid;

            // 应用新装备的强化效果
            EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
            if (record && record->level > 0)
            {
                // 设置可见槽位
                if (item->IsEquipped())
                    player->SetVisibleItemSlot(item->GetSlot(), item);

                sItemEnhancementMgr->ApplyOfficialItemEnhancement(player, item, record->level);

                LOG_DEBUG("module.itemenhancement", "OnPlayerEquip: Applied enhancement level {} for item {} in slot {} for player {}",
                    record->level, itemGuid, slot, player->GetGUID().ToString());
            }

            // 【关键优化】登录加载阶段不做全量刷新
            // 参考 ItemAttributesEvents 的实现：
            // 使用 isBeingLoaded() 检查，避免登录时每件装备都刷新属性
            if (!player->isBeingLoaded())
            {
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateMaxHealth();
                player->UpdateMaxPower(POWER_MANA);
            }
        }
        catch (...)
        {
            LOG_ERROR("module.itemenhancement", "OnPlayerEquip: Exception for player {} item {} slot {}",
                player->GetGUID().ToString(), item->GetGUID().GetCounter(), slot);
        }
#endif
    }

    void OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return;

#ifndef MODULE_ITEM_ATTRIBUTES
        try
        {
            // 如果物品从装备槽移出，移除强化效果
            if (bag == INVENTORY_SLOT_BAG_0 && slot < EQUIPMENT_SLOT_END)
            {
                // 【安全检查】
                if (!player->GetSession() || !player->IsInWorld())
                {
                    LOG_WARN("module.itemenhancement", "OnPlayerAfterMoveItemFromInventory: Player {} not valid",
                        player->GetGUID().ToString());
                    return;
                }

                uint64 itemGuid = item->GetGUID().GetCounter();
                uint64 playerGuid = player->GetGUID().GetCounter();

                // 移除强化效果
                sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);

                // 从跟踪map中移除
                auto& playerEquipMap = _equippedEnhancementItems[playerGuid];
                playerEquipMap.erase(slot);

                // 刷新玩家属性
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateMaxHealth();
                player->UpdateMaxPower(POWER_MANA);

                LOG_DEBUG("module.itemenhancement", "OnPlayerAfterMoveItemFromInventory: Removed enhancement for item {} from slot {}",
                    itemGuid, slot);
            }
        }
        catch (...)
        {
            LOG_ERROR("module.itemenhancement", "OnPlayerAfterMoveItemFromInventory: Exception for player {} item {}",
                player->GetGUID().ToString(), item->GetGUID().GetCounter());
        }
#endif
    }
};

// 物品移除事件处理脚本
class ItemEnhancement_AllItemScript : public AllItemScript
{
public:
    ItemEnhancement_AllItemScript() : AllItemScript("ItemEnhancement_AllItemScript") { }

    bool CanItemRemove(Player* player, Item* item) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return true; // 允许移除

#ifdef MODULE_ITEM_ATTRIBUTES
        // 物品属性系统启用时，由物品属性模块统一处理属性移除
        return true;
#else
        try
        {
            // 【安全检查】
            if (!player->GetSession() || !player->IsInWorld())
                return true; // 玩家状态异常，允许移除但不处理强化

            // 检查物品是否从装备槽移除
            uint8 slot = item->GetSlot();
            if (slot >= EQUIPMENT_SLOT_END)
                return true; // 不是装备槽，允许移除

            uint64 itemGuid = item->GetGUID().GetCounter();
            EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);

            // 移除强化效果
            sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);

            if (record && record->level > 0)
            {
                LOG_DEBUG("module.itemenhancement", "CanItemRemove: Removed enhancement for item {} from slot {}",
                    itemGuid, slot);
            }

            return true; // 允许移除物品
        }
        catch (...)
        {
            LOG_ERROR("module.itemenhancement", "CanItemRemove: Exception for player {} item {}, allowing removal",
                player->GetGUID().ToString(), item->GetGUID().GetCounter());
            return true; // 异常情况下仍然允许移除
        }
#endif
    }
};

// 物品脚本（处理物品删除）
class ItemEnhancement_ItemScript : public ItemScript
{
public:
    ItemEnhancement_ItemScript() : ItemScript("ItemEnhancement_ItemScript") { }

    bool OnRemove(Player* player, Item* item) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return false;

        // 当物品被移除时，删除对应的强化记录
        uint32 itemGuid = item->GetGUID().GetCounter();
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
        if (record)
        {
            DeleteEnhancementRecord(itemGuid);
            LOG_DEBUG("module.itemenhancement", "ItemEnhancement: Deleted enhancement record for item GUID: {}", itemGuid);
        }

        return false;
    }
};

// 注册脚本
void AddSC_ItemEnhancementScript()
{
    new ItemEnhancement_PlayerScript();
    new ItemEnhancement_EquipScript();
    new ItemEnhancement_AllItemScript();
    new ItemEnhancement_ItemScript();
}

#include "ItemEnhancement.h"
#include "ItemEnhancementMgr.h"
#include "Chat.h"
#include "Player.h"
#include "WorldPacket.h"
#include "GameTime.h"
#include "ScriptMgr.h"
#include "CommandScript.h"
#include "AllItemScript.h"
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

// 装备跟踪数据
static std::unordered_map<uint32, std::unordered_map<uint8, uint32>> playerEquipmentTracker; // playerGuid -> slot -> itemGuid

// 物品删除事件处理
class ItemEnhancement_PlayerScript : public PlayerScript
{
public:
    ItemEnhancement_PlayerScript() : PlayerScript("ItemEnhancement_PlayerScript") { }

    // 注意：PlayerScript 类中没有 OnItemRemove 方法，所以不能使用 override
    // 此方法目前未被使用，物品移除逻辑已在 ItemEnhancement_ItemScript::OnRemove 中实现
    // 保留此方法以备将来可能需要
    void HandleItemRemove(Player* player, Item* item)
    {
        if (!ItemEnhancementEnabled || !player || !item)
            return;

        // 当物品被移除时，删除对应的强化记录
        uint32 itemGuid = item->GetGUID().GetCounter();
        EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
        if (record)
        {
            DeleteEnhancementRecord(itemGuid);
            LOG_DEBUG("module", "物品强化记录已删除，物品GUID: {}", itemGuid);
        }
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

        // 【紧急修复】禁用延迟加载，改为同步执行
        // 原因：延迟任务捕获裸指针存在严重安全隐患（悬空指针崩溃）
        // 2秒内玩家断线 → Player对象被删除 → 延迟任务访问已释放内存 → 崩溃
        // 修复：立即执行，确保 player 指针有效
        auto loginFunc = [](Player* player)
        {
            auto loginStart = std::chrono::high_resolution_clock::now();

            if (!player || !player->IsInWorld() || !sItemEnhancementMgr->IsEnabled())
                return;

            // 预加载玩家的强化记录到缓存
            auto step1Start = std::chrono::high_resolution_clock::now();
            sItemEnhancementMgr->PreloadPlayerEnhancementRecords(player);
            auto step1End = std::chrono::high_resolution_clock::now();
            auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

#ifndef MODULE_ITEM_ATTRIBUTES
            // 【旧模式】在未启用物品属性系统时，由强化模块自己批量应用属性

            // 【性能优化】移除登录时的清理步骤
            // 原因：玩家刚登录，内存中不可能有残留的强化效果，清理是多余的
            // sItemEnhancementMgr->ClearPlayerAppliedEnhancements(player);  // 已禁用

            // 初始化装备跟踪
            if (!player)
                return;

            auto step3Start = std::chrono::high_resolution_clock::now();
            uint32 playerGuid = player->GetGUID().GetCounter();
            playerEquipmentTracker[playerGuid].clear();

            // 记录当前装备状态
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (item)
                {
                    playerEquipmentTracker[playerGuid][slot] = item->GetGUID().GetCounter();
                }
            }
            auto step3End = std::chrono::high_resolution_clock::now();
            auto step3Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step3End - step3Start).count();

            // 登录时批量应用强化效果
            auto step4Start = std::chrono::high_resolution_clock::now();
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (!item)
                    continue;

                uint32 itemGuid = item->GetGUID().GetCounter();
                EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
                if (record && record->level > 0 && !record->statValues.empty())
                {
                    std::map<uint32, int32> stats = sItemEnhancementMgr->ParseStatValues(record->statValues);
                    for (const auto& stat : stats)
                    {
                        if (stat.second > 0)
                        {
                            sItemEnhancementMgr->ApplyStatModifierBatch(player, stat.first, stat.second, true);
                        }
                    }
                    sItemEnhancementMgr->TrackAppliedEnhancement(player, item, record->statValues);
                }
            }

            // 统一刷新一次属性
            player->UpdateAllStats();
            player->UpdateAttackPowerAndDamage();
            player->UpdateAttackPowerAndDamage(true);
            player->UpdateMaxHealth();
            player->UpdateMaxPower(POWER_MANA);

            auto step4End = std::chrono::high_resolution_clock::now();
            auto step4Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step4End - step4Start).count();
#endif

            // 发送登录完成消息给客户端UI插件
            // 注意：需要确保SendLoginCompleteMessage函数可以访问player
            auto step5Start = std::chrono::high_resolution_clock::now();
            std::string msg = "ITEMENHANCE\tLOGIN_COMPLETED";
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, player, player, msg);
            player->GetSession()->SendPacket(&data);
            auto step5End = std::chrono::high_resolution_clock::now();
            auto step5Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step5End - step5Start).count();

            auto loginEnd = std::chrono::high_resolution_clock::now();
            auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loginEnd - loginStart).count();
        };

        // 立即执行，不再延迟
        loginFunc(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

        // 玩家登出时清理跟踪数据
        sItemEnhancementMgr->ClearPlayerAppliedEnhancements(player);

        // 清理装备跟踪数据
        uint32 playerGuid = player->GetGUID().GetCounter();
        playerEquipmentTracker.erase(playerGuid);
    }

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

#ifndef MODULE_ITEM_ATTRIBUTES
        // 每1秒检查一次装备变化，确保能捕获到装备卸载
        static std::unordered_map<uint32, uint32> lastCheckTime;
        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 currentTime = GameTime::GetGameTime().count();

        if (currentTime - lastCheckTime[playerGuid] >= 1)
        {
            CheckEquipmentChanges(player);
            lastCheckTime[playerGuid] = currentTime;
        }
#endif
    }

private:
    void InitializeEquipmentTracking(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        playerEquipmentTracker[playerGuid].clear();

        // 记录当前装备状态
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item)
            {
                playerEquipmentTracker[playerGuid][slot] = item->GetGUID().GetCounter();
            }
        }

        // 装备跟踪已初始化
    }

    void CheckEquipmentChanges(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto& trackedEquipment = playerEquipmentTracker[playerGuid];

        ChatHandler handler(player->GetSession());

        // 检查每个装备槽
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* currentItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            uint32 currentItemGuid = currentItem ? currentItem->GetGUID().GetCounter() : 0;
            uint32 trackedItemGuid = trackedEquipment[slot];

            // 如果装备发生变化
            if (currentItemGuid != trackedItemGuid)
            {
                // 如果有旧装备被移除（从有装备变为无装备）
                if (trackedItemGuid != 0 && currentItemGuid == 0)
                {
                    // 尝试移除强化效果
                    EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(trackedItemGuid);
                    if (record && record->level > 0)
                    {
                        // 直接调用移除函数，使用GUID
                        RemoveEnhancementByGuid(player, trackedItemGuid);
                    }
                }
                // 如果装备发生更换（从一个装备换成另一个装备）
                else if (trackedItemGuid != 0 && currentItemGuid != 0 && trackedItemGuid != currentItemGuid)
                {
                    // 先移除旧装备的强化效果
                    EnhancementRecord const* oldRecord = sItemEnhancementMgr->GetEnhancementRecord(trackedItemGuid);
                    if (oldRecord && oldRecord->level > 0)
                    {
                        RemoveEnhancementByGuid(player, trackedItemGuid);
                    }

                    // 再应用新装备的强化效果
                    EnhancementRecord const* newRecord = sItemEnhancementMgr->GetEnhancementRecord(currentItemGuid);
                    if (newRecord && newRecord->level > 0 && currentItem)
                    {
                        sItemEnhancementMgr->ApplyOfficialItemEnhancement(player, currentItem, newRecord->level);
                    }
                }
                // 如果有新装备被穿戴（从无装备变为有装备）
                // 不在这里应用强化效果，避免重复应用
                // OnPlayerEquip已经处理了装备穿戴时的强化效果应用

                // 更新跟踪数据
                trackedEquipment[slot] = currentItemGuid;
            }
        }
    }

    void RemoveEnhancementByGuid(Player* player, uint32 itemGuid)
    {
        if (!player || itemGuid == 0)
            return;

        // 开始处理强化效果移除

        // 关键修复：首先检查跟踪的已应用效果
        uint32 playerGuid = player->GetGUID().GetCounter();

        // 检查跟踪系统中是否有这个物品的已应用效果
        std::string appliedStats = "";
        bool foundTrackedData = false;

        auto playerItr = sItemEnhancementMgr->_appliedEnhancements.find(playerGuid);
        if (playerItr != sItemEnhancementMgr->_appliedEnhancements.end())
        {
            auto itemItr = playerItr->second.find(itemGuid);
            if (itemItr != playerItr->second.end())
            {
                appliedStats = itemItr->second;
                foundTrackedData = true;
            }
        }

        if (foundTrackedData && !appliedStats.empty())
        {
            // 使用跟踪的数据移除属性
            std::map<uint32, int32> stats = sItemEnhancementMgr->ParseStatValues(appliedStats);

            for (const auto& stat : stats)
            {
                if (stat.second > 0)
                {
                    sItemEnhancementMgr->ApplyStatModifier(player, stat.first, stat.second, false);
                }
            }

            // 移除跟踪记录
            playerItr->second.erase(itemGuid);
            if (playerItr->second.empty())
            {
                sItemEnhancementMgr->_appliedEnhancements.erase(playerItr);
            }
        }
        else
        {
            // 如果没有跟踪数据，尝试从数据库读取（备用方案）
            EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
            if (record && record->level > 0 && !record->statValues.empty())
            {
                std::map<uint32, int32> stats = sItemEnhancementMgr->ParseStatValues(record->statValues);
                for (const auto& stat : stats)
                {
                    if (stat.second > 0)
                    {
                        sItemEnhancementMgr->ApplyStatModifier(player, stat.first, stat.second, false);
                    }
                }
            }
        }

        // 更新玩家属性
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }

    void SendLoginCompleteMessage(Player* player)
    {
        if (!player)
            return;

        // 发送登录完成消息，告诉客户端可以开始处理强化数据
        std::string message = "ITEMENHANCE|LOGIN_COMPLETE|" + std::to_string(player->GetGUID().GetCounter());

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_GUILD, LANG_ADDON, player, player, message);
        player->GetSession()->SendPacket(&data);
    }
};

// 物品属性修改
class ItemEnhancement_ItemScript : public ItemScript
{
public:
    ItemEnhancement_ItemScript() : ItemScript("ItemEnhancement_ItemScript") { }

    bool OnExpire(Player* player, ItemTemplate const* /*itemTemplate*/) override
    {
        if (!ItemEnhancementEnabled || !player)
            return false;

        // 物品过期时的处理
        return false;
    }

    // 自定义方法，不是重写
    bool OnDummyEffect(Player* player, Item* item, SpellInfo const* /*spellInfo*/, uint8 /*effIndex*/)
    {
        if (!ItemEnhancementEnabled || !player || !item)
            return false;

        // 物品特效触发时的处理
        return false;
    }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!ItemEnhancementEnabled || !player || !item)
            return false;

        // 物品使用时的处理
        return false;
    }

    // 自定义方法，不是重写
    bool OnEquip(Player* player, Item* item, uint8 /*slot*/)
    {
        if (!ItemEnhancementEnabled || !player || !item)
            return false;

        // 物品装备时的处理
        // 简单起见，我们直接调用检查函数

        return false;
    }

    bool OnRemove(Player* player, Item* item) override
    {
        if (!ItemEnhancementEnabled || !player || !item)
            return false;

        // 物品卸下时的处理
        // 简单起见，我们直接调用检查函数

        // 直接处理物品移除逻辑，而不是尝试调用 PlayerScript 中的方法
        // 当物品被移除时，删除对应的强化记录
        uint32 itemGuid = item->GetGUID().GetCounter();
        EnhancementRecord const* record = GetEnhancementRecord(itemGuid);
        if (record)
        {
            DeleteEnhancementRecord(itemGuid);
            LOG_DEBUG("module", "物品强化记录已删除，物品GUID: {}", itemGuid);
        }

        return false;
    }
};

// 装备事件处理脚本
class ItemEnhancement_EquipScript : public PlayerScript
{
public:
    ItemEnhancement_EquipScript() : PlayerScript("ItemEnhancement_EquipScript") { }

    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return;

        using namespace std::chrono;
        auto perfStart = high_resolution_clock::now();

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 itemEntry = item->GetEntry();
        uint32 itemGuid = item->GetGUID().GetCounter();

        // 只处理装备槽位
        if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
            return;

        // 检查是否有强化记录
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
#ifndef MODULE_ITEM_ATTRIBUTES
        if (player->GetSession()->PlayerLoading())
            return;

        if (record && record->level > 0)
        {
            // 先清理该槽位的所有强化效果，再应用新装备的强化效果
            sItemEnhancementMgr->ClearSlotEnhancements(player, slot);

            if (item->IsEquipped())
                player->SetVisibleItemSlot(item->GetSlot(), item);

            sItemEnhancementMgr->ApplyOfficialItemEnhancement(player, item, record->level);
        }
        else
        {
            // 即使没有强化效果，也要清理槽位，防止旧装备的效果残留
            sItemEnhancementMgr->ClearSlotEnhancements(player, slot);
        }
#endif

        auto perfEnd = high_resolution_clock::now();
        auto perfMs = duration_cast<milliseconds>(perfEnd - perfStart).count();
    }

    void OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return;

#ifndef MODULE_ITEM_ATTRIBUTES
        // 如果物品从装备槽移出，移除强化效果
        if (bag == INVENTORY_SLOT_BAG_0 && slot < EQUIPMENT_SLOT_END)
        {
            // 检查是否有强化记录
            uint32 itemGuid = item->GetGUID().GetCounter();
            EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
            if (record && record->level > 0)
            {
                // 关键修复：使用统一的移除函数，确保属性完全移除
                sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);
            }
            else
            {
                // 即使没有强化记录，也要尝试清理可能残留的跟踪数据
                sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);
            }
        }
#endif
    }

    // 移除不存在的钩子方法

    // 移除不必要的定期检查逻辑
};

// 物品移除事件处理脚本 - 使用AllItemScript来处理所有物品的移除事件
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
        // 检查物品是否从装备槽移除
        uint8 slot = item->GetSlot();
        if (slot >= EQUIPMENT_SLOT_END)
            return true; // 不是装备槽，允许移除

        // 检查是否有强化记录
        uint32 itemGuid = item->GetGUID().GetCounter();
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
        if (record && record->level > 0)
        {
            // 关键修复：使用统一的移除函数，确保属性完全移除
            sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);
        }
        else
        {
            // 即使没有强化记录，也要尝试清理可能残留的跟踪数据
            sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);
        }

        return true; // 允许移除物品
#endif
    }
};



// 注册脚本
void AddSC_ItemEnhancementScript()
{
    new ItemEnhancement_PlayerScript();
    new ItemEnhancement_ItemScript();
    new ItemEnhancement_EquipScript();
    new ItemEnhancement_AllItemScript();
}

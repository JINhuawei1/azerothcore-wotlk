/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
char constexpr MATERIAL_WAREHOUSE_ADDON_PREFIX[] = "MATWH";
size_t constexpr MATERIAL_WAREHOUSE_MAX_ADDON_PAYLOAD = 230;
uint32 constexpr MATERIAL_WAREHOUSE_AUTO_STORE_BATCH_SIZE = 500;

struct MaterialWarehouseType
{
    int32 itemClass = -1;
    int32 itemSubclass = -1;
    uint8 minQuality = 0;
    uint8 maxQuality = 7;
    bool requireStackable = true;
};

struct MaterialWarehouseStoredItem
{
    uint64 count = 0;
    bool autoStore = true;
};

std::string SanitizeAddonField(std::string value)
{
    for (char& ch : value)
        if (ch == '^' || ch == '|' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = '/';

    return value;
}

std::string BuildAddonIconPath(char const* inventoryIcon)
{
    if (!inventoryIcon || !*inventoryIcon)
        return "Interface\\Icons\\INV_Misc_QuestionMark";

    std::string icon(inventoryIcon);
    if (icon.find("Interface\\") == 0 || icon.find("interface\\") == 0)
        return icon;

    return "Interface\\Icons\\" + icon;
}

std::string GetItemIconPath(uint32 itemId)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate || !itemTemplate->DisplayInfoID)
        return "Interface\\Icons\\INV_Misc_QuestionMark";

    if (ItemDisplayInfoEntry const* displayInfo = sItemDisplayInfoStore.LookupEntry(itemTemplate->DisplayInfoID))
        return BuildAddonIconPath(displayInfo->inventoryIcon);

    return "Interface\\Icons\\INV_Misc_QuestionMark";
}

bool TryParseUInt32(std::string const& text, uint32& value)
{
    try
    {
        size_t pos = 0;
        unsigned long parsed = std::stoul(text, &pos, 10);
        if (pos != text.length() || parsed > std::numeric_limits<uint32>::max())
            return false;

        value = static_cast<uint32>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool TryParseUInt64(std::string const& text, uint64& value)
{
    try
    {
        size_t pos = 0;
        unsigned long long parsed = std::stoull(text, &pos, 10);
        if (pos != text.length())
            return false;

        value = static_cast<uint64>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::string> Split(std::string const& text, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, delimiter))
        parts.push_back(part);

    return parts;
}

class MaterialWarehouseMgr
{
public:
    static MaterialWarehouseMgr* Instance()
    {
        static MaterialWarehouseMgr instance;
        return &instance;
    }

    void LoadConfig()
    {
        _enabled = sConfigMgr->GetOption<bool>("MaterialWarehouse.Enable", true);
        _depositExistingOnAdd = sConfigMgr->GetOption<bool>("MaterialWarehouse.DepositExistingOnAdd", true);
        _maxWithdrawPerRequest = sConfigMgr->GetOption<uint32>("MaterialWarehouse.MaxWithdrawPerRequest", 100000);
    }

    void LoadAllowedTypes()
    {
        _allowedTypes.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT "
            "CAST(CAST(`物品大类` AS CHAR) AS SIGNED), "
            "CAST(CAST(`物品子类` AS CHAR) AS SIGNED), "
            "`最低品质`, `最高品质`, `必须可堆叠` "
            "FROM `_材料仓库` WHERE `启用` = 1 ORDER BY `id` ASC");

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                MaterialWarehouseType type;
                type.itemClass = fields[0].Get<int32>();
                type.itemSubclass = fields[1].Get<int32>();
                type.minQuality = fields[2].Get<uint8>();
                type.maxQuality = fields[3].Get<uint8>();
                type.requireStackable = fields[4].Get<uint8>() != 0;
                _allowedTypes.push_back(type);
            }
            while (result->NextRow());
        }

        LOG_INFO("server.loading", "材料仓库: 已加载 {} 条可存储类型配置", static_cast<uint32>(_allowedTypes.size()));
    }

    bool IsEnabled() const { return _enabled; }

    bool IsItemAllowed(uint32 itemId, std::string* reason = nullptr) const
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            if (reason)
                *reason = "物品模板不存在";
            return false;
        }

        for (MaterialWarehouseType const& type : _allowedTypes)
        {
            if (type.itemClass != -1 && static_cast<uint32>(type.itemClass) != itemTemplate->Class)
                continue;

            if (type.itemSubclass != -1 && static_cast<uint32>(type.itemSubclass) != itemTemplate->SubClass)
                continue;

            if (itemTemplate->Quality < type.minQuality || itemTemplate->Quality > type.maxQuality)
                continue;

            if (type.requireStackable && itemTemplate->Stackable <= 1)
                continue;

            return true;
        }

        if (reason)
            *reason = "该物品类型未在 world._材料仓库 中启用";
        return false;
    }

    void LoadPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto& items = _players[playerGuid];
        items.clear();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `物品ID`, `数量`, `自动存储` FROM `_材料仓库玩家` WHERE `玩家GUID` = {}",
            playerGuid);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            MaterialWarehouseStoredItem state;
            uint32 itemId = fields[0].Get<uint32>();
            state.count = fields[1].Get<uint64>();
            state.autoStore = fields[2].Get<uint8>() != 0;
            items[itemId] = state;
        }
        while (result->NextRow());
    }

    void UnloadPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        _players.erase(playerGuid);
        _pendingAutoStore.erase(playerGuid);
    }

    void DeletePlayer(ObjectGuid guid)
    {
        uint32 playerGuid = guid.GetCounter();
        _players.erase(playerGuid);
        _pendingAutoStore.erase(playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_材料仓库玩家` WHERE `玩家GUID` = {}", playerGuid);
    }

    bool AddTrackedItem(Player* player, uint32 itemId)
    {
        if (!player || !_enabled)
            return false;

        std::string reason;
        if (!IsItemAllowed(itemId, &reason))
        {
            SendResult(player, "ADD", false, reason);
            return false;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();
        MaterialWarehouseStoredItem& state = _players[playerGuid][itemId];
        state.autoStore = true;

        CharacterDatabase.Execute(
            "INSERT INTO `_材料仓库玩家` (`玩家GUID`, `物品ID`, `数量`, `自动存储`) "
            "VALUES ({}, {}, {}, 1) "
            "ON DUPLICATE KEY UPDATE `自动存储` = 1",
            playerGuid, itemId, state.count);

        uint64 deposited = 0;
        if (_depositExistingOnAdd)
            deposited = DepositFromInventory(player, itemId, 0, false);

        if (deposited)
            SendResult(player, "ADD", true, "已加入仓库，并存入背包中已有数量");
        else
            SendResult(player, "ADD", true, "已加入仓库");

        SendWarehouse(player);
        return true;
    }

    bool RemoveTrackedItem(Player* player, uint32 itemId)
    {
        if (!player || !_enabled)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto playerItr = _players.find(playerGuid);
        if (playerItr == _players.end())
        {
            SendResult(player, "REMOVE", false, "仓库中没有该物品");
            return false;
        }

        auto itemItr = playerItr->second.find(itemId);
        if (itemItr == playerItr->second.end())
        {
            SendResult(player, "REMOVE", false, "仓库中没有该物品");
            return false;
        }

        if (itemItr->second.count > 0)
        {
            itemItr->second.autoStore = false;
            CharacterDatabase.Execute(
                "UPDATE `_材料仓库玩家` SET `自动存储` = 0 WHERE `玩家GUID` = {} AND `物品ID` = {}",
                playerGuid, itemId);
            SendResult(player, "REMOVE", false, "仓库仍有数量，已关闭自动存储；请先取出后再移除");
            SendWarehouse(player);
            return false;
        }

        playerItr->second.erase(itemItr);
        CharacterDatabase.Execute("DELETE FROM `_材料仓库玩家` WHERE `玩家GUID` = {} AND `物品ID` = {}", playerGuid, itemId);

        SendResult(player, "REMOVE", true, "已移出仓库列表");
        SendWarehouse(player);
        return true;
    }

    bool SetAutoStore(Player* player, uint32 itemId, bool enabled)
    {
        if (!player || !_enabled)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
        if (!state)
        {
            SendResult(player, "AUTO", false, "仓库中没有该物品");
            return false;
        }

        state->autoStore = enabled;
        CharacterDatabase.Execute(
            "UPDATE `_材料仓库玩家` SET `自动存储` = {} WHERE `玩家GUID` = {} AND `物品ID` = {}",
            enabled ? 1 : 0, playerGuid, itemId);

        SendResult(player, "AUTO", true, enabled ? "已开启自动存储" : "已关闭自动存储");
        SendWarehouse(player);
        return true;
    }

    uint64 DepositFromInventory(Player* player, uint32 itemId, uint64 requestedCount, bool notify = true)
    {
        if (!player || !_enabled)
            return 0;

        std::string reason;
        if (!IsItemAllowed(itemId, &reason))
        {
            if (notify)
                SendResult(player, "DEPOSIT", false, reason);
            return 0;
        }

        uint32 available = player->GetItemCount(itemId, false);
        if (!available)
        {
            if (notify)
                SendResult(player, "DEPOSIT", false, "背包中没有该物品");
            return 0;
        }

        uint64 amount = requestedCount ? std::min<uint64>(requestedCount, available) : available;
        if (!amount)
            return 0;

        uint32 destroyCount = static_cast<uint32>(std::min<uint64>(amount, std::numeric_limits<uint32>::max()));
        EnsureTracked(player, itemId);
        player->DestroyItemCount(itemId, destroyCount, true, false);
        AddStoredCount(player->GetGUID().GetCounter(), itemId, destroyCount);

        if (notify)
        {
            SendResult(player, "DEPOSIT", true, "已存入仓库");
            SendWarehouse(player);
        }

        return destroyCount;
    }

    uint64 DepositAllTracked(Player* player)
    {
        if (!player || !_enabled)
            return 0;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto playerItr = _players.find(playerGuid);
        if (playerItr == _players.end())
        {
            SendResult(player, "DEPOSIT_ALL", false, "还没有加入任何物品");
            return 0;
        }

        std::vector<uint32> itemIds;
        itemIds.reserve(playerItr->second.size());
        for (auto const& [itemId, state] : playerItr->second)
        {
            if (state.autoStore)
                itemIds.push_back(itemId);
        }

        uint64 total = 0;
        for (uint32 itemId : itemIds)
            total += DepositFromInventory(player, itemId, 0, false);

        SendResult(player, "DEPOSIT_ALL", total > 0, total > 0 ? "已存入所有已开启自动存储的物品" : "没有可存入的物品");
        SendWarehouse(player);
        return total;
    }

    bool Withdraw(Player* player, uint32 itemId, uint64 requestedCount)
    {
        if (!player || !_enabled)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
        if (!state || state->count == 0)
        {
            SendResult(player, "WITHDRAW", false, "仓库中没有该物品");
            return false;
        }

        uint64 amount = requestedCount ? std::min<uint64>(requestedCount, state->count) : state->count;
        amount = std::min<uint64>(amount, _maxWithdrawPerRequest);
        amount = std::min<uint64>(amount, std::numeric_limits<uint32>::max());
        if (!amount)
        {
            SendResult(player, "WITHDRAW", false, "取出数量无效");
            return false;
        }

        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, static_cast<uint32>(amount));
        if (msg != EQUIP_ERR_OK)
        {
            player->SendEquipError(msg, nullptr, nullptr, itemId);
            SendResult(player, "WITHDRAW", false, "背包空间不足");
            return false;
        }

        Item* newItem = Item::CreateItem(itemId, static_cast<uint32>(amount), player);
        if (!newItem)
        {
            SendResult(player, "WITHDRAW", false, "创建物品失败");
            return false;
        }

        _withdrawGuard.insert(playerGuid);
        Item* item = player->StoreItem(dest, newItem, true);
        _withdrawGuard.erase(playerGuid);

        if (!item)
        {
            delete newItem;
            SendResult(player, "WITHDRAW", false, "创建物品失败");
            return false;
        }

        SubtractStoredCount(playerGuid, itemId, amount);
        SendResult(player, "WITHDRAW", true, "已取出物品");
        SendWarehouse(player);
        return true;
    }

    void HandleAutoStore(Player* player, Item* item, uint32 count)
    {
        if (!player || !item || !_enabled || !count)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        if (_withdrawGuard.find(playerGuid) != _withdrawGuard.end())
            return;

        uint32 itemId = item->GetEntry();
        MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
        if (!state || !state->autoStore)
            return;

        if (!IsItemAllowed(itemId))
            return;

        _pendingAutoStore[playerGuid][itemId] += count;
    }

    void Update(Player* player, uint32 /*diff*/)
    {
        if (!player || !_enabled)
            return;

        ProcessPendingAutoStore(player, MATERIAL_WAREHOUSE_AUTO_STORE_BATCH_SIZE);
    }

    void SendOpen(Player* player)
    {
        SendPayload(player, "OPEN");
        SendWarehouse(player);
    }

    void SendWarehouse(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        SendPayload(player, "BEGIN");

        auto playerItr = _players.find(playerGuid);
        if (playerItr != _players.end())
        {
            std::vector<uint32> itemIds;
            itemIds.reserve(playerItr->second.size());
            for (auto const& [itemId, state] : playerItr->second)
            {
                if (state.count > 0)
                    itemIds.push_back(itemId);
            }

            std::sort(itemIds.begin(), itemIds.end());

            for (uint32 itemId : itemIds)
            {
                MaterialWarehouseStoredItem const& state = playerItr->second[itemId];
                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
                if (!itemTemplate)
                    continue;

                std::ostringstream payload;
                payload << "ITEM:" << itemId << '^'
                        << state.count << '^'
                        << (state.autoStore ? 1 : 0) << '^'
                        << SanitizeAddonField(itemTemplate->Name1) << '^'
                        << static_cast<uint32>(itemTemplate->Quality) << '^'
                        << SanitizeAddonField(GetItemIconPath(itemId)) << '^'
                        << itemTemplate->Stackable;

                SendPayload(player, payload.str());
            }
        }

        SendPayload(player, "END");
    }

    void SendResult(Player* player, std::string const& action, bool success, std::string const& message)
    {
        std::ostringstream payload;
        payload << "RESULT:" << action << '^' << (success ? 1 : 0) << '^' << SanitizeAddonField(message);
        SendPayload(player, payload.str());

        if (player && player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("材料仓库：{}", message);
    }

    void HandleAddonCommand(Player* player, std::string const& command)
    {
        if (!player || command.empty())
            return;

        LOG_INFO("server.loading", "材料仓库: 收到Addon指令 player={} command={}", player->GetName(), command);

        if (command == "OPEN" || command == "REQ")
        {
            SendOpen(player);
            return;
        }

        if (command == "STORE_ALL" || command == "DEPOSIT_ALL" || command == "存储全部" || command == "全部存储" || command == "全部存入")
        {
            DepositAllTracked(player);
            return;
        }

        if (command == "RELOAD" || command == "重载")
        {
            if (player->GetSession() && player->GetSession()->GetSecurity() >= SEC_ADMINISTRATOR)
            {
                LoadConfig();
                LoadAllowedTypes();
                SendResult(player, "RELOAD", true, "配置已重载");
            }
            else
                SendResult(player, "RELOAD", false, "权限不足");
            return;
        }

        std::vector<std::string> parts = Split(command, ':');
        if (parts.empty())
            return;

        uint32 itemId = 0;
        uint64 count = 0;

        if ((parts[0] == "ADD" || parts[0] == "添加" || parts[0] == "提交" || parts[0] == "提交存储") && parts.size() >= 2 && TryParseUInt32(parts[1], itemId))
        {
            AddTrackedItem(player, itemId);
            return;
        }

        if ((parts[0] == "REMOVE" || parts[0] == "删除" || parts[0] == "移除") && parts.size() >= 2 && TryParseUInt32(parts[1], itemId))
        {
            RemoveTrackedItem(player, itemId);
            return;
        }

        if ((parts[0] == "STORE" || parts[0] == "DEPOSIT" || parts[0] == "存储" || parts[0] == "存入") && parts.size() >= 2 && TryParseUInt32(parts[1], itemId))
        {
            if (parts.size() >= 3)
                TryParseUInt64(parts[2], count);
            DepositFromInventory(player, itemId, count);
            return;
        }

        if ((parts[0] == "WITHDRAW" || parts[0] == "EXTRACT" || parts[0] == "取出" || parts[0] == "提取") && parts.size() >= 2 && TryParseUInt32(parts[1], itemId))
        {
            if (parts.size() >= 3)
                TryParseUInt64(parts[2], count);
            Withdraw(player, itemId, count);
            return;
        }

        if ((parts[0] == "AUTO" || parts[0] == "自动") && parts.size() >= 3 && TryParseUInt32(parts[1], itemId))
        {
            uint32 enabled = 0;
            if (TryParseUInt32(parts[2], enabled))
                SetAutoStore(player, itemId, enabled != 0);
            return;
        }

        SendResult(player, "UNKNOWN", false, "未知指令");
    }

private:
    MaterialWarehouseStoredItem* GetStoredState(uint32 playerGuid, uint32 itemId)
    {
        auto playerItr = _players.find(playerGuid);
        if (playerItr == _players.end())
            return nullptr;

        auto itemItr = playerItr->second.find(itemId);
        if (itemItr == playerItr->second.end())
            return nullptr;

        return &itemItr->second;
    }

    void EnsureTracked(Player* player, uint32 itemId)
    {
        uint32 playerGuid = player->GetGUID().GetCounter();
        MaterialWarehouseStoredItem& state = _players[playerGuid][itemId];
        state.autoStore = true;

        CharacterDatabase.Execute(
            "INSERT INTO `_材料仓库玩家` (`玩家GUID`, `物品ID`, `数量`, `自动存储`) "
            "VALUES ({}, {}, {}, 1) ON DUPLICATE KEY UPDATE `自动存储` = `自动存储`",
            playerGuid, itemId, state.count);
    }

    void AddStoredCount(uint32 playerGuid, uint32 itemId, uint64 count)
    {
        MaterialWarehouseStoredItem& state = _players[playerGuid][itemId];
        state.count += count;

        CharacterDatabase.Execute(
            "INSERT INTO `_材料仓库玩家` (`玩家GUID`, `物品ID`, `数量`, `自动存储`) "
            "VALUES ({}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `数量` = `数量` + VALUES(`数量`)",
            playerGuid, itemId, count, state.autoStore ? 1 : 0);
    }

    void ProcessPendingAutoStore(Player* player, uint32 budget)
    {
        if (!player || !budget)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto pendingPlayerItr = _pendingAutoStore.find(playerGuid);
        if (pendingPlayerItr == _pendingAutoStore.end())
            return;

        uint32 remainingBudget = budget;
        uint64 storedTotal = 0;
        auto& pendingItems = pendingPlayerItr->second;

        for (auto itr = pendingItems.begin(); itr != pendingItems.end() && remainingBudget > 0; )
        {
            uint32 itemId = itr->first;
            uint64 pendingCount = itr->second;

            MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
            if (!state || !state->autoStore || !IsItemAllowed(itemId))
            {
                itr = pendingItems.erase(itr);
                continue;
            }

            uint32 available = player->GetItemCount(itemId, false);
            if (!available)
            {
                itr = pendingItems.erase(itr);
                continue;
            }

            uint64 storeCount64 = std::min<uint64>(pendingCount, std::min<uint64>(available, remainingBudget));
            uint32 storeCount = static_cast<uint32>(storeCount64);
            if (!storeCount)
            {
                ++itr;
                continue;
            }

            player->DestroyItemCount(itemId, storeCount, true, false);
            AddStoredCount(playerGuid, itemId, storeCount);
            storedTotal += storeCount;
            remainingBudget -= storeCount;

            if (available <= storeCount || pendingCount <= storeCount)
                itr = pendingItems.erase(itr);
            else
            {
                itr->second = pendingCount - storeCount;
                ++itr;
            }
        }

        if (pendingItems.empty())
            _pendingAutoStore.erase(pendingPlayerItr);

        if (storedTotal > 0)
            SendWarehouse(player);
    }

    void SubtractStoredCount(uint32 playerGuid, uint32 itemId, uint64 count)
    {
        MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
        if (!state)
            return;

        state->count = state->count > count ? state->count - count : 0;

        CharacterDatabase.Execute(
            "UPDATE `_材料仓库玩家` SET `数量` = IF(`数量` > {}, `数量` - {}, 0) "
            "WHERE `玩家GUID` = {} AND `物品ID` = {}",
            count, count, playerGuid, itemId);
    }

    void SendPayload(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        if (payload.length() <= MATERIAL_WAREHOUSE_MAX_ADDON_PAYLOAD)
        {
            SendRawPayload(player, payload);
            return;
        }

        size_t total = (payload.length() + MATERIAL_WAREHOUSE_MAX_ADDON_PAYLOAD - 1) / MATERIAL_WAREHOUSE_MAX_ADDON_PAYLOAD;
        for (size_t i = 0; i < total; ++i)
        {
            std::ostringstream chunk;
            chunk << "CHUNK:" << (i + 1) << '^' << total << '^'
                  << payload.substr(i * MATERIAL_WAREHOUSE_MAX_ADDON_PAYLOAD, MATERIAL_WAREHOUSE_MAX_ADDON_PAYLOAD);
            SendRawPayload(player, chunk.str());
        }
    }

    void SendRawPayload(Player* player, std::string const& payload)
    {
        std::string fullMessage = std::string(MATERIAL_WAREHOUSE_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }

    bool _enabled = true;
    bool _depositExistingOnAdd = true;
    uint32 _maxWithdrawPerRequest = 100000;
    std::vector<MaterialWarehouseType> _allowedTypes;
    std::unordered_map<uint32, std::unordered_map<uint32, MaterialWarehouseStoredItem>> _players;
    std::unordered_map<uint32, std::unordered_map<uint32, uint64>> _pendingAutoStore;
    std::unordered_set<uint32> _withdrawGuard;
};

#define sMaterialWarehouseMgr MaterialWarehouseMgr::Instance()

class MaterialWarehouseWorldScript : public WorldScript
{
public:
    MaterialWarehouseWorldScript() : WorldScript("MaterialWarehouseWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sMaterialWarehouseMgr->LoadConfig();
        sMaterialWarehouseMgr->LoadAllowedTypes();
    }
};

class MaterialWarehousePlayerScript : public PlayerScript
{
public:
    MaterialWarehousePlayerScript() : PlayerScript("MaterialWarehousePlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_STORE_NEW_ITEM,
        PLAYERHOOK_ON_CHAT
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sMaterialWarehouseMgr->IsEnabled())
            return;

        sMaterialWarehouseMgr->LoadPlayer(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        sMaterialWarehouseMgr->UnloadPlayer(player);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        sMaterialWarehouseMgr->DeletePlayer(guid);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sMaterialWarehouseMgr->Update(player, diff);
    }

    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override
    {
        sMaterialWarehouseMgr->HandleAutoStore(player, item, count);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        if (msg.substr(0, tabPos) != MATERIAL_WAREHOUSE_ADDON_PREFIX)
            return;

        sMaterialWarehouseMgr->HandleAddonCommand(player, msg.substr(tabPos + 1));
    }
};

using namespace Acore::ChatCommands;

class MaterialWarehouseCommandScript : public CommandScript
{
public:
    MaterialWarehouseCommandScript() : CommandScript("MaterialWarehouseCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subTable =
        {
            { "界面", HandleOpenCommand, SEC_PLAYER, Console::No },
            { "ui", HandleOpenCommand, SEC_PLAYER, Console::No },
            { "添加", HandleAddCommand, SEC_PLAYER, Console::No },
            { "删除", HandleRemoveCommand, SEC_PLAYER, Console::No },
            { "自动", HandleAutoCommand, SEC_PLAYER, Console::No },
            { "auto", HandleAutoCommand, SEC_PLAYER, Console::No },
            { "存储", HandleDepositCommand, SEC_PLAYER, Console::No },
            { "存入", HandleDepositCommand, SEC_PLAYER, Console::No },
            { "取出", HandleWithdrawCommand, SEC_PLAYER, Console::No },
            { "提取", HandleWithdrawCommand, SEC_PLAYER, Console::No },
            { "列表", HandleListCommand, SEC_PLAYER, Console::No },
            { "刷新", HandleListCommand, SEC_PLAYER, Console::No },
            { "重载", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "材料仓库", subTable },
            { "仓库", subTable }
        };

        return commandTable;
    }

private:
    static Player* GetPlayer(ChatHandler* handler)
    {
        return handler ? handler->GetPlayer() : nullptr;
    }

    static bool HandleOpenCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sMaterialWarehouseMgr->SendOpen(player);
        return true;
    }

    static bool HandleAddCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        uint32 itemId = 0;
        std::istringstream stream(args ? args : "");
        stream >> itemId;

        if (!player || !itemId)
        {
            handler->PSendSysMessage("用法: .材料仓库 添加 <物品ID>");
            return false;
        }

        sMaterialWarehouseMgr->AddTrackedItem(player, itemId);
        return true;
    }

    static bool HandleRemoveCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        uint32 itemId = 0;
        std::istringstream stream(args ? args : "");
        stream >> itemId;

        if (!player || !itemId)
        {
            handler->PSendSysMessage("用法: .材料仓库 删除 <物品ID>");
            return false;
        }

        sMaterialWarehouseMgr->RemoveTrackedItem(player, itemId);
        return true;
    }

    static bool HandleDepositCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        std::string first;
        uint64 count = 0;
        std::istringstream stream(args ? args : "");
        stream >> first;
        stream >> count;

        if (!player || first.empty())
        {
            handler->PSendSysMessage("用法: .材料仓库 存储 <物品ID|all> [数量]");
            return false;
        }

        if (first == "all" || first == "全部")
        {
            sMaterialWarehouseMgr->DepositAllTracked(player);
            return true;
        }

        uint32 itemId = 0;
        if (!TryParseUInt32(first, itemId) || !itemId)
        {
            handler->PSendSysMessage("物品ID无效");
            return false;
        }

        sMaterialWarehouseMgr->DepositFromInventory(player, itemId, count);
        return true;
    }

    static bool HandleAutoCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        uint32 itemId = 0;
        uint32 enabled = 0;
        std::istringstream stream(args ? args : "");
        stream >> itemId;
        stream >> enabled;

        if (!player || !itemId)
        {
            handler->PSendSysMessage("用法: .材料仓库 自动 <物品ID> <1|0>");
            return false;
        }

        sMaterialWarehouseMgr->SetAutoStore(player, itemId, enabled != 0);
        return true;
    }

    static bool HandleWithdrawCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        uint32 itemId = 0;
        std::string countText;
        uint64 count = 0;
        std::istringstream stream(args ? args : "");
        stream >> itemId;
        stream >> countText;

        if (!player || !itemId)
        {
            handler->PSendSysMessage("用法: .材料仓库 提取 <物品ID> [数量|all]");
            return false;
        }

        if (!countText.empty() && countText != "all" && countText != "全部")
            TryParseUInt64(countText, count);

        sMaterialWarehouseMgr->Withdraw(player, itemId, count);
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        sMaterialWarehouseMgr->SendWarehouse(player);
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sMaterialWarehouseMgr->LoadConfig();
        sMaterialWarehouseMgr->LoadAllowedTypes();
        handler->PSendSysMessage("材料仓库：配置已重载。");
        return true;
    }
};
}

void AddSC_mod_material_warehouse()
{
    new MaterialWarehouseWorldScript();
    new MaterialWarehousePlayerScript();
    new MaterialWarehouseCommandScript();
}

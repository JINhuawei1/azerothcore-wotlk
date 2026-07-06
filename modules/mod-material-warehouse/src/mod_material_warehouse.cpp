/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "AddonThrottle.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "HermesBridgeAddonApi.h"
#include "Log.h"
#include "MaterialWarehouseSystem.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "StringConvert.h"
#include "Util.h"
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
// 每次 OnPlayerUpdate 最多处理的“待自动存储物品种类数”。
// 旧实现这里是按“数量”限速(每 tick 最多存 500 个)，玩家一次获得千万级材料时
// 需要几万个 tick 才能存完，且每 tick 都 SendWarehouse 刷屏客户端造成严重卡顿。
// 现改为按“物品种类数”限速：每个种类一次性全量存入，正常一个 tick 即可存完所有材料。
uint32 constexpr MATERIAL_WAREHOUSE_AUTO_STORE_MAX_ITEMS_PER_TICK = 256;
uint32 constexpr MATERIAL_WAREHOUSE_PAGE_SIZE = 200;

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
    uint256 count = 0;
    bool autoStore = true;
};

uint256 SaturatingAddUInt256(uint256 const& left, uint256 const& right)
{
    if (right > std::numeric_limits<uint256>::max() - left)
        return std::numeric_limits<uint256>::max();

    return left + right;
}

std::string SanitizeAddonField(std::string value)
{
    for (char& ch : value)
        if (ch == '^' || ch == '|' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = '/';

    return value;
}

std::string BuildAddonIconPath(char const* inventoryIcon)
{
    // 图标路径统一使用正斜杠：该字段经 Hermes 桥的 JSON 通道下发，客户端 DLL 的
    // 轻量 JSON 解析不做反转义，反斜杠会以 "\\" 字面形式到达插件导致 SetTexture
    // 失效(图标空白)。WoW 的纹理路径接受正斜杠，且 '/' 在 JSON 中无需转义。
    if (!inventoryIcon || !*inventoryIcon)
        return "Interface/Icons/INV_Misc_QuestionMark";

    std::string icon(inventoryIcon);
    std::replace(icon.begin(), icon.end(), '\\', '/');
    if (icon.find("Interface/") == 0 || icon.find("interface/") == 0)
        return icon;

    return "Interface/Icons/" + icon;
}

std::string GetItemIconPath(uint32 itemId)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate || !itemTemplate->DisplayInfoID)
        return "Interface/Icons/INV_Misc_QuestionMark";

    if (ItemDisplayInfoEntry const* displayInfo = sItemDisplayInfoStore.LookupEntry(itemTemplate->DisplayInfoID))
        return BuildAddonIconPath(displayInfo->inventoryIcon);

    return "Interface/Icons/INV_Misc_QuestionMark";
}

std::string BuildPlainItemText(ItemTemplate const* itemTemplate, uint32 itemId)
{
    if (!itemTemplate)
        return "[" + std::to_string(itemId) + "]";

    return "[" + itemTemplate->Name1 + "]";
}

std::string BuildItemChatLink(ItemTemplate const* itemTemplate, uint32 itemId)
{
    if (!itemTemplate)
        return BuildPlainItemText(itemTemplate, itemId);

    std::ostringstream link;
    uint32 quality = std::min<uint32>(itemTemplate->Quality, MAX_ITEM_QUALITY - 1);
    link << "|c" << std::hex << ItemQualityColors[quality] << std::dec
         << "|Hitem:" << itemTemplate->ItemId << ":0:0:0:0:0:0:0:0:0|h[" << itemTemplate->Name1 << "]|h|r";

    return link.str();
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

bool TryParseUInt256(std::string const& text, uint256& value)
{
    if (Optional<uint256> parsed = Acore::StringTo<uint256>(text))
    {
        value = *parsed;
        return true;
    }

    return false;
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

    uint32 GetStoredCount(Player* player, uint32 itemId)
    {
        if (!player || !_enabled || !itemId)
            return 0;

        MaterialWarehouseStoredItem* state = GetStoredState(player->GetGUID().GetCounter(), itemId);
        if (!state)
            return 0;

        return Acore::Number::ToUInt32Saturated(state->count);
    }

    uint32 ConsumeStoredCount(Player* player, uint32 itemId, uint32 count)
    {
        if (!player || !_enabled || !itemId || !count)
            return 0;

        uint32 playerGuid = player->GetGUID().GetCounter();
        MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
        if (!state || state->count == 0)
            return 0;

        uint256 amount = std::min<uint256>(uint256(count), state->count);
        uint32 amount32 = Acore::Number::ToUInt32Saturated(amount);
        if (!amount32)
            return 0;

        SubtractStoredCount(playerGuid, itemId, amount);
        SendWarehouse(player);
        return amount32;
    }

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
            *reason = "无法存储该类型的装备或者材料";
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
        {
            RefreshAutoStoreSnapshot(player);
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            MaterialWarehouseStoredItem state;
            uint32 itemId = fields[0].Get<uint32>();
            state.count = fields[1].Get<uint256>();
            state.autoStore = fields[2].Get<uint8>() != 0;
            items[itemId] = state;
        }
        while (result->NextRow());

        QueueCurrentAutoStoreInventory(player);
    }

    void UnloadPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        _players.erase(playerGuid);
        _pendingAutoStore.erase(playerGuid);
        _recentStoreNewAutoStore.erase(playerGuid);
        _inventoryAutoStoreCounts.erase(playerGuid);
        _playerPages.erase(playerGuid);
    }

    void DeletePlayer(ObjectGuid guid)
    {
        uint32 playerGuid = guid.GetCounter();
        _players.erase(playerGuid);
        _pendingAutoStore.erase(playerGuid);
        _recentStoreNewAutoStore.erase(playerGuid);
        _inventoryAutoStoreCounts.erase(playerGuid);
        _playerPages.erase(playerGuid);
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
            playerGuid, itemId, Acore::Number::ToDecimal65String(state.count));

        uint256 deposited = 0;
        if (_depositExistingOnAdd)
            deposited = DepositFromInventory(player, itemId, 0, false);

        if (deposited)
            SendResult(player, "ADD", true, "已加入仓库，并存入背包中已有数量");
        else
            SendResult(player, "ADD", true, "已加入仓库");

        RefreshAutoStoreSnapshot(player);
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
            _inventoryAutoStoreCounts[playerGuid].erase(itemId);
            CharacterDatabase.Execute(
                "UPDATE `_材料仓库玩家` SET `自动存储` = 0 WHERE `玩家GUID` = {} AND `物品ID` = {}",
                playerGuid, itemId);
            SendResult(player, "REMOVE", false, "仓库仍有数量，已关闭自动存储；请先取出后再移除");
            SendWarehouse(player);
            return false;
        }

        playerItr->second.erase(itemItr);
        _inventoryAutoStoreCounts[playerGuid].erase(itemId);
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

        if (enabled)
            QueueCurrentAutoStoreInventory(player, itemId);
        else
            RefreshAutoStoreSnapshot(player, itemId);
        SendResult(player, "AUTO", true, enabled ? "已开启自动存储" : "已关闭自动存储");
        SendWarehouse(player);
        return true;
    }

    uint256 DepositFromInventory(Player* player, uint32 itemId, uint256 const& requestedCount, bool notify = true)
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

        uint256 amount = requestedCount != 0 ? std::min<uint256>(requestedCount, uint256(available)) : uint256(available);
        if (!amount)
            return 0;

        uint32 destroyCount = Acore::Number::ToUInt32Saturated(amount);
        EnsureTracked(player, itemId);
        player->DestroyItemCount(itemId, destroyCount, true, false);
        AddStoredCount(player->GetGUID().GetCounter(), itemId, destroyCount);

        if (notify)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            std::ostringstream addonMessage;
            addonMessage << BuildPlainItemText(itemTemplate, itemId) << "x" << destroyCount << " 存储成功";
            std::ostringstream chatMessage;
            chatMessage << BuildItemChatLink(itemTemplate, itemId) << "x" << destroyCount << " 存储成功";
            SendResult(player, "DEPOSIT", true, addonMessage.str(), chatMessage.str());
            SendWarehouse(player);
        }

        RefreshAutoStoreSnapshot(player, itemId);
        return destroyCount;
    }

    uint256 DepositAllTracked(Player* player)
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

        uint256 total = 0;
        for (uint32 itemId : itemIds)
            total = SaturatingAddUInt256(total, DepositFromInventory(player, itemId, 0, false));

        SendResult(player, "DEPOSIT_ALL", total > 0, total > 0 ? "已存入所有已开启自动存储的物品" : "没有可存入的物品");
        SendWarehouse(player);
        return total;
    }

    bool Withdraw(Player* player, uint32 itemId, uint256 const& requestedCount)
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

        uint256 amount = requestedCount != 0 ? std::min<uint256>(requestedCount, state->count) : state->count;
        amount = std::min<uint256>(amount, uint256(_maxWithdrawPerRequest));
        amount = std::min<uint256>(amount, uint256(std::numeric_limits<uint32>::max()));
        uint32 amount32 = Acore::Number::ToUInt32Saturated(amount);
        if (!amount32)
        {
            SendResult(player, "WITHDRAW", false, "取出数量无效");
            return false;
        }

        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, amount32);
        if (msg != EQUIP_ERR_OK)
        {
            player->SendEquipError(msg, nullptr, nullptr, itemId);
            SendResult(player, "WITHDRAW", false, "背包空间不足");
            return false;
        }

        Item* newItem = Item::CreateItem(itemId, amount32, player);
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
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        std::ostringstream addonMessage;
        addonMessage << BuildPlainItemText(itemTemplate, itemId) << "x" << amount32 << " 取出成功";
        std::ostringstream chatMessage;
        chatMessage << BuildItemChatLink(itemTemplate, itemId) << "x" << amount32 << " 取出成功";
        SendResult(player, "WITHDRAW", true, addonMessage.str(), chatMessage.str());
        SendWarehouse(player);
        RefreshAutoStoreSnapshot(player, itemId);
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

        uint256& pendingCount = _pendingAutoStore[playerGuid][itemId];
        pendingCount = SaturatingAddUInt256(pendingCount, count);
        RememberStoreNewEvent(playerGuid, item, count);
        RefreshAutoStoreSnapshot(player, itemId);
    }

    void HandleConfirmedAutoStore(Player* player, Item* item, uint32 count)
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

        uint256 uncoveredCount = ConsumeRecentStoreNewEvent(playerGuid, item, count);
        if (!uncoveredCount)
            return;

        uint256& pendingCount = _pendingAutoStore[playerGuid][itemId];
        pendingCount = SaturatingAddUInt256(pendingCount, uncoveredCount);
        RefreshAutoStoreSnapshot(player, itemId);
    }

    void Update(Player* player, uint32 /*diff*/)
    {
        if (!player || !_enabled)
            return;

        ReconcileAutoStoreInventory(player);
        ProcessPendingAutoStore(player, MATERIAL_WAREHOUSE_AUTO_STORE_MAX_ITEMS_PER_TICK);
        RefreshAutoStoreSnapshot(player);
        _recentStoreNewAutoStore.erase(player->GetGUID().GetCounter());
    }

    void SendOpen(Player* player)
    {
        SendPayload(player, "OPEN");
    }

    void SendWarehouse(Player* player, uint32 requestedPage = 0)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        std::vector<uint32> itemIds;
        auto playerItr = _players.find(playerGuid);
        if (playerItr != _players.end())
        {
            itemIds.reserve(playerItr->second.size());
            for (auto const& [itemId, state] : playerItr->second)
            {
                if (state.count > 0)
                    itemIds.push_back(itemId);
            }

            std::sort(itemIds.begin(), itemIds.end());
        }

        uint32 totalItems = static_cast<uint32>(itemIds.size());
        uint32 totalPages = std::max<uint32>(1, (totalItems + MATERIAL_WAREHOUSE_PAGE_SIZE - 1) / MATERIAL_WAREHOUSE_PAGE_SIZE);
        uint32 page = requestedPage;
        if (!page)
        {
            auto pageItr = _playerPages.find(playerGuid);
            page = pageItr != _playerPages.end() ? pageItr->second : 1;
        }

        page = std::max<uint32>(1, std::min<uint32>(page, totalPages));
        _playerPages[playerGuid] = page;

        std::ostringstream begin;
        begin << "BEGIN:" << page << '^' << totalPages << '^' << totalItems << '^' << MATERIAL_WAREHOUSE_PAGE_SIZE;
        SendPayload(player, begin.str());

        if (playerItr != _players.end() && totalItems > 0)
        {
            uint32 offset = (page - 1) * MATERIAL_WAREHOUSE_PAGE_SIZE;
            uint32 end = std::min<uint32>(offset + MATERIAL_WAREHOUSE_PAGE_SIZE, totalItems);
            for (uint32 i = offset; i < end; ++i)
            {
                uint32 itemId = itemIds[i];
                MaterialWarehouseStoredItem const& state = playerItr->second[itemId];
                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
                if (!itemTemplate)
                    continue;

                std::ostringstream payload;
                payload << "ITEM:" << itemId << '^'
                        << Acore::ToString(state.count) << '^'
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
        SendResult(player, action, success, message, message);
    }

    void SendResult(Player* player, std::string const& action, bool success, std::string const& addonMessage, std::string const& chatMessage)
    {
        std::ostringstream payload;
        payload << "RESULT:" << action << '^' << (success ? 1 : 0) << '^' << SanitizeAddonField(addonMessage);
        SendPayload(player, payload.str());

        if (player && player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("材料仓库：{}", chatMessage);
    }

    void HandleAddonCommand(Player* player, std::string const& command)
    {
        if (!player || command.empty())
            return;

        LOG_INFO("server.loading", "材料仓库: 收到Addon指令 player={} command={}", player->GetName(), command);

        if (command == "OPEN")
        {
            SendOpen(player);
            return;
        }

        if (command == "REQ")
        {
            SendWarehouse(player);
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

        if ((parts[0] == "REQ" || parts[0] == "PAGE" || parts[0] == "列表" || parts[0] == "刷新") && parts.size() >= 2)
        {
            uint32 page = 0;
            if (TryParseUInt32(parts[1], page))
                SendWarehouse(player, page);
            return;
        }

        uint32 itemId = 0;
        uint256 count = 0;

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
                TryParseUInt256(parts[2], count);
            DepositFromInventory(player, itemId, count);
            return;
        }

        if ((parts[0] == "WITHDRAW" || parts[0] == "EXTRACT" || parts[0] == "取出" || parts[0] == "提取") && parts.size() >= 2 && TryParseUInt32(parts[1], itemId))
        {
            if (parts.size() >= 3)
                TryParseUInt256(parts[2], count);
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
            playerGuid, itemId, Acore::Number::ToDecimal65String(state.count));
    }

    void AddStoredCount(uint32 playerGuid, uint32 itemId, uint256 const& count)
    {
        MaterialWarehouseStoredItem& state = _players[playerGuid][itemId];
        state.count = SaturatingAddUInt256(state.count, count);

        CharacterDatabase.Execute(
            "INSERT INTO `_材料仓库玩家` (`玩家GUID`, `物品ID`, `数量`, `自动存储`) "
            "VALUES ({}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `数量` = VALUES(`数量`), `自动存储` = VALUES(`自动存储`)",
            playerGuid, itemId, Acore::Number::ToDecimal65String(state.count), state.autoStore ? 1 : 0);
    }

    void ProcessPendingAutoStore(Player* player, uint32 maxItems)
    {
        if (!player || !maxItems)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto pendingPlayerItr = _pendingAutoStore.find(playerGuid);
        if (pendingPlayerItr == _pendingAutoStore.end())
            return;

        uint32 remainingItems = maxItems;
        uint256 storedTotal = 0;
        auto& pendingItems = pendingPlayerItr->second;

        for (auto itr = pendingItems.begin(); itr != pendingItems.end() && remainingItems > 0; )
        {
            uint32 itemId = itr->first;
            uint256 pendingCount = itr->second;

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

            // 一次性存入该物品当前背包中的全部可存数量，不再按 500/tick 分批。
            // available 为 uint32，storeCount = min(pending, available) 不会因饱和而丢失精度。
            uint256 storeCountWide = std::min<uint256>(pendingCount, uint256(available));
            uint32 storeCount = Acore::Number::ToUInt32Saturated(storeCountWide);
            if (!storeCount)
            {
                itr = pendingItems.erase(itr);
                continue;
            }

            player->DestroyItemCount(itemId, storeCount, true, false);
            AddStoredCount(playerGuid, itemId, storeCount);
            storedTotal = SaturatingAddUInt256(storedTotal, storeCount);
            --remainingItems;

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

    void RememberStoreNewEvent(uint32 playerGuid, Item* item, uint256 const& count)
    {
        if (!item || !count)
            return;

        _recentStoreNewAutoStore[playerGuid][item->GetGUID().GetCounter()] =
            SaturatingAddUInt256(_recentStoreNewAutoStore[playerGuid][item->GetGUID().GetCounter()], count);
    }

    uint256 ConsumeRecentStoreNewEvent(uint32 playerGuid, Item* item, uint256 const& count)
    {
        if (!item || !count)
            return 0;

        auto playerItr = _recentStoreNewAutoStore.find(playerGuid);
        if (playerItr == _recentStoreNewAutoStore.end())
            return count;

        auto itemItr = playerItr->second.find(item->GetGUID().GetCounter());
        if (itemItr == playerItr->second.end())
            return count;

        uint256 coveredCount = std::min<uint256>(itemItr->second, count);
        if (itemItr->second <= coveredCount)
            playerItr->second.erase(itemItr);
        else
            itemItr->second -= coveredCount;

        if (playerItr->second.empty())
            _recentStoreNewAutoStore.erase(playerItr);

        return count - coveredCount;
    }

    void ReconcileAutoStoreInventory(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto playerItr = _players.find(playerGuid);
        if (playerItr == _players.end())
            return;

        auto& lastCounts = _inventoryAutoStoreCounts[playerGuid];
        for (auto const& [itemId, state] : playerItr->second)
        {
            if (!state.autoStore || !IsItemAllowed(itemId))
            {
                lastCounts.erase(itemId);
                continue;
            }

            uint32 currentCount = player->GetItemCount(itemId, false);
            uint32 previousCount = 0;
            auto countItr = lastCounts.find(itemId);
            if (countItr != lastCounts.end())
                previousCount = countItr->second;

            if (currentCount > previousCount)
            {
                uint256& pendingCount = _pendingAutoStore[playerGuid][itemId];
                pendingCount = SaturatingAddUInt256(pendingCount, uint256(currentCount - previousCount));
            }

            lastCounts[itemId] = currentCount;
        }
    }

    void RefreshAutoStoreSnapshot(Player* player, uint32 onlyItemId = 0)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto playerItr = _players.find(playerGuid);
        if (playerItr == _players.end())
        {
            _inventoryAutoStoreCounts.erase(playerGuid);
            return;
        }

        auto& lastCounts = _inventoryAutoStoreCounts[playerGuid];
        if (!onlyItemId)
            lastCounts.clear();

        for (auto const& [itemId, state] : playerItr->second)
        {
            if (onlyItemId && itemId != onlyItemId)
                continue;

            if (state.autoStore)
                lastCounts[itemId] = player->GetItemCount(itemId, false);
            else
                lastCounts.erase(itemId);
        }
    }

    void QueueCurrentAutoStoreInventory(Player* player, uint32 onlyItemId = 0)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto playerItr = _players.find(playerGuid);
        if (playerItr == _players.end())
            return;

        for (auto const& [itemId, state] : playerItr->second)
        {
            if (onlyItemId && itemId != onlyItemId)
                continue;

            if (!state.autoStore || !IsItemAllowed(itemId))
                continue;

            uint32 currentCount = player->GetItemCount(itemId, false);
            if (!currentCount)
                continue;

            uint256& pendingCount = _pendingAutoStore[playerGuid][itemId];
            pendingCount = SaturatingAddUInt256(pendingCount, currentCount);
        }

        RefreshAutoStoreSnapshot(player, onlyItemId);
    }

    void SubtractStoredCount(uint32 playerGuid, uint32 itemId, uint256 const& count)
    {
        MaterialWarehouseStoredItem* state = GetStoredState(playerGuid, itemId);
        if (!state)
            return;

        state->count = state->count > count ? state->count - count : 0;

        CharacterDatabase.Execute(
            "UPDATE `_材料仓库玩家` SET `数量` = {} "
            "WHERE `玩家GUID` = {} AND `物品ID` = {}",
            Acore::Number::ToDecimal65String(state->count), playerGuid, itemId);
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
        if (HermesBridge_SendAddonMessage(player, MATERIAL_WAREHOUSE_ADDON_PREFIX, payload))
            return;

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
    std::unordered_map<uint32, std::unordered_map<uint32, uint256>> _pendingAutoStore;
    std::unordered_map<uint32, std::unordered_map<uint64, uint256>> _recentStoreNewAutoStore;
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _inventoryAutoStoreCounts;
    std::unordered_map<uint32, uint32> _playerPages;
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
        PLAYERHOOK_ON_LOOT_ITEM,
        PLAYERHOOK_ON_STORE_NEW_ITEM,
        PLAYERHOOK_ON_CREATE_ITEM,
        PLAYERHOOK_ON_QUEST_REWARD_ITEM,
        PLAYERHOOK_ON_GROUP_ROLL_REWARD_ITEM,
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

    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid /*lootguid*/) override
    {
        sMaterialWarehouseMgr->HandleConfirmedAutoStore(player, item, count);
    }

    void OnPlayerCreateItem(Player* player, Item* item, uint32 count) override
    {
        sMaterialWarehouseMgr->HandleConfirmedAutoStore(player, item, count);
    }

    void OnPlayerQuestRewardItem(Player* player, Item* item, uint32 count) override
    {
        sMaterialWarehouseMgr->HandleConfirmedAutoStore(player, item, count);
    }

    void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, RollVote /*voteType*/, Roll* /*roll*/) override
    {
        sMaterialWarehouseMgr->HandleConfirmedAutoStore(player, item, count);
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

        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "MATWAREHOUSE"))
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
        std::string countText;
        uint256 count = 0;
        std::istringstream stream(args ? args : "");
        stream >> first;
        stream >> countText;

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

        if (!countText.empty())
            TryParseUInt256(countText, count);

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
        uint256 count = 0;
        std::istringstream stream(args ? args : "");
        stream >> itemId;
        stream >> countText;

        if (!player || !itemId)
        {
            handler->PSendSysMessage("用法: .材料仓库 提取 <物品ID> [数量|all]");
            return false;
        }

        if (!countText.empty() && countText != "all" && countText != "全部")
            TryParseUInt256(countText, count);

        sMaterialWarehouseMgr->Withdraw(player, itemId, count);
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        uint32 page = 0;
        std::istringstream stream(args ? args : "");
        stream >> page;

        sMaterialWarehouseMgr->SendWarehouse(player, page);
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

uint32 MaterialWarehouseGetItemCount(Player* player, uint32 itemId)
{
    return sMaterialWarehouseMgr->GetStoredCount(player, itemId);
}

uint32 MaterialWarehouseConsumeItemCount(Player* player, uint32 itemId, uint32 count)
{
    return sMaterialWarehouseMgr->ConsumeStoredCount(player, itemId, count);
}

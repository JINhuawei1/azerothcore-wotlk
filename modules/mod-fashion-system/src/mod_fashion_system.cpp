/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Bag.h"
#if defined(__INTELLISENSE__)
#include "Common.h"
#include "SharedDefines.h"
#include <string>
#include <string_view>
#include <vector>

class WorldObject;
class WorldPacket;
class WorldSession;

namespace Acore::ChatCommands
{
    enum class Console : bool
    {
        No = false,
        Yes = true
    };

    struct ChatCommandBuilder
    {
        template<typename... Args>
        ChatCommandBuilder(Args&&...);
    };

    using ChatCommandTable = std::vector<ChatCommandBuilder>;
}

class ChatHandler
{
public:
    explicit ChatHandler(WorldSession* session);

    template<typename... Args>
    void SendSysMessage(Args&&...);

    template<typename... Args>
    void PSendSysMessage(Args&&...);

    WorldSession* GetSession() const;

    static std::size_t BuildChatPacket(WorldPacket& data, ChatMsg chatType, Language language,
        WorldObject const* sender, WorldObject const* receiver, std::string_view message,
        uint32 achievementId = 0, std::string const& channelName = "", LocaleConstant locale = DEFAULT_LOCALE);
};
#else
#include "Chat.h"
#include "ChatCommand.h"
#endif
#include "Config.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#if defined(__INTELLISENSE__)
#ifndef sConfigMgr
class ConfigMgr
{
public:
    static ConfigMgr* instance();

    template<class T>
    T GetOption(std::string const& /*name*/, T const& def, bool /*showLogs*/ = true) const
    {
        return def;
    }
};
#define sConfigMgr ConfigMgr::instance()
#endif

#ifndef LOG_INFO
#define LOG_INFO(...)
#endif

#ifndef LOG_WARN
#define LOG_WARN(...)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(...)
#endif
#endif
#include "ObjectMgr.h"
#include "Player.h"
#if defined(__INTELLISENSE__)
class ScriptObject
{
protected:
    explicit ScriptObject(char const* /*name*/) { }

public:
    virtual ~ScriptObject() = default;
};

class WorldScript : public ScriptObject
{
protected:
    explicit WorldScript(char const* name) : ScriptObject(name) { }

public:
    virtual void OnAfterConfigLoad(bool /*reload*/) { }
    virtual void OnStartup() { }
};

class PlayerScript : public ScriptObject
{
protected:
    explicit PlayerScript(char const* name) : ScriptObject(name) { }

public:
    virtual void OnPlayerLogin(Player* /*player*/) { }
    virtual void OnPlayerLogout(Player* /*player*/) { }
    virtual void OnPlayerDelete(ObjectGuid /*guid*/, uint32 /*accountId*/) { }
    virtual void OnPlayerAfterSetVisibleItemSlot(Player* /*player*/, uint8 /*slot*/, Item* /*item*/) { }
    virtual void OnPlayerChat(Player* /*player*/, uint32 /*type*/, uint32 /*lang*/, std::string& /*msg*/, Player* /*receiver*/) { }
};

class CommandScript : public ScriptObject
{
protected:
    explicit CommandScript(char const* name) : ScriptObject(name) { }

public:
    virtual Acore::ChatCommands::ChatCommandTable GetCommands() const = 0;
};
#else
#include "ScriptMgr.h"
#endif
#ifdef ACORE_WITH_REQUIREMENT_SYSTEM
#include "RequirementSystem.h"
#endif
#include "ItemSets.h"
#include "UpdateFields.h"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr char const* CONF_ENABLE = "FashionSystem.Enable";
constexpr char FASHION_SYSTEM_ADDON_PREFIX[] = "FASHION_SYS";
constexpr size_t MAX_ADDON_PAYLOAD = 220;

enum FashionSetAssignMode : uint8
{
    FASHION_SET_ASSIGN_NONE = 0,
    FASHION_SET_ASSIGN_ID = 1,
    FASHION_SET_ASSIGN_GROUP = 2
};

// 玩家时装状态：开关 + 每个槽位的视觉覆盖物品id + 每个槽位最终解析出的实际套装id
struct PlayerFashionState
{
    bool enabled = true;
    std::unordered_map<uint8, uint32> visualOverrides; // equipSlot -> itemEntry
    std::unordered_map<uint8, uint32> resolvedSetIds;  // equipSlot -> resolved item set id
};

// 世界库时装配置：每个槽位的需求、套装分配方式等
struct FashionSlotConfig
{
    uint32 id = 0;
    uint32 requirementId = 0;    // 需求系统id
    uint8 setAssignMode = FASHION_SET_ASSIGN_NONE; // 套装分配模式
    uint32 setId = 0;            // 套装id（固定分配）
    uint32 setGroup = 0;         // 套装组（随机分配）
    uint8 slot = 0;              // 时装位置 0-18
    std::string slotDesc;        // 时装描述
};

std::string SanitizeAddonText(std::string text)
{
    for (char& ch : text)
    {
        if (ch == '^' || ch == '~' || ch == '|' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }

    return text;
}

bool IsSupportedFashionSlot(uint8 slot)
{
    return slot < EQUIPMENT_SLOT_END; // 0 ~ 18
}

char const* GetFashionSlotName(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD:      return "头部";
        case EQUIPMENT_SLOT_NECK:      return "项链";
        case EQUIPMENT_SLOT_SHOULDERS: return "肩部";
        case EQUIPMENT_SLOT_BODY:      return "衬衣";
        case EQUIPMENT_SLOT_CHEST:     return "胸甲";
        case EQUIPMENT_SLOT_WAIST:     return "腰带";
        case EQUIPMENT_SLOT_LEGS:      return "腿部";
        case EQUIPMENT_SLOT_FEET:      return "脚部";
        case EQUIPMENT_SLOT_WRISTS:    return "护腕";
        case EQUIPMENT_SLOT_HANDS:     return "手套";
        case EQUIPMENT_SLOT_FINGER1:   return "戒指1";
        case EQUIPMENT_SLOT_FINGER2:   return "戒指2";
        case EQUIPMENT_SLOT_TRINKET1:  return "饰品1";
        case EQUIPMENT_SLOT_TRINKET2:  return "饰品2";
        case EQUIPMENT_SLOT_BACK:      return "背部";
        case EQUIPMENT_SLOT_MAINHAND:  return "主手";
        case EQUIPMENT_SLOT_OFFHAND:   return "副手";
        case EQUIPMENT_SLOT_RANGED:    return "远程";
        case EQUIPMENT_SLOT_TABARD:    return "战袍";
        default: return "未知";
    }
}

// 判断物品的 InventoryType 是否可以装备到目标槽位（支持双槽位如戒指、饰品、武器）
bool CanItemEquipInSlot(uint8 inventoryType, uint8 slot)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:            return slot == EQUIPMENT_SLOT_HEAD;
        case INVTYPE_NECK:            return slot == EQUIPMENT_SLOT_NECK;
        case INVTYPE_SHOULDERS:       return slot == EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_BODY:            return slot == EQUIPMENT_SLOT_BODY;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:            return slot == EQUIPMENT_SLOT_CHEST;
        case INVTYPE_WAIST:           return slot == EQUIPMENT_SLOT_WAIST;
        case INVTYPE_LEGS:            return slot == EQUIPMENT_SLOT_LEGS;
        case INVTYPE_FEET:            return slot == EQUIPMENT_SLOT_FEET;
        case INVTYPE_WRISTS:          return slot == EQUIPMENT_SLOT_WRISTS;
        case INVTYPE_HANDS:           return slot == EQUIPMENT_SLOT_HANDS;
        case INVTYPE_FINGER:          return slot == EQUIPMENT_SLOT_FINGER1 || slot == EQUIPMENT_SLOT_FINGER2;
        case INVTYPE_TRINKET:         return slot == EQUIPMENT_SLOT_TRINKET1 || slot == EQUIPMENT_SLOT_TRINKET2;
        case INVTYPE_CLOAK:           return slot == EQUIPMENT_SLOT_BACK;
        case INVTYPE_WEAPON:          return slot == EQUIPMENT_SLOT_MAINHAND || slot == EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:  return slot == EQUIPMENT_SLOT_MAINHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:        return slot == EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_RELIC:           return slot == EQUIPMENT_SLOT_RANGED;
        case INVTYPE_TABARD:          return slot == EQUIPMENT_SLOT_TABARD;
        default:                      return false;
    }
}

void SetPlayerVisibleItemDirect(Player* player, uint8 visibleSlot, uint32 itemEntry, uint16 permEnchant, uint16 tempEnchant)
{
    if (!player || visibleSlot >= EQUIPMENT_SLOT_END)
        return;

    player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (visibleSlot * 2), itemEntry);
    player->SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (visibleSlot * 2), 0, permEnchant);
    player->SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (visibleSlot * 2), 1, tempEnchant);
}

std::vector<std::string> SplitText(std::string const& text, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string token;

    while (std::getline(stream, token, delimiter))
        result.push_back(token);

    return result;
}

std::string BuildCompactSlotState(PlayerFashionState const& state)
{
    std::ostringstream stream;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot > EQUIPMENT_SLOT_START)
            stream << ',';

        auto itr = state.visualOverrides.find(slot);
        uint32 enabledFlag = (itr != state.visualOverrides.end() && itr->second > 0) ? 1 : 0;
        stream << static_cast<uint32>(slot) << ' ' << enabledFlag;
    }

    return stream.str();
}

std::string BuildCompactItemEntryList(PlayerFashionState const& state)
{
    std::ostringstream stream;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot > EQUIPMENT_SLOT_START)
            stream << ',';

        auto itr = state.visualOverrides.find(slot);
        uint32 itemEntry = itr != state.visualOverrides.end() ? itr->second : 0;
        stream << itemEntry;
    }

    return stream.str();
}

std::string BuildCompactResolvedSetIdList(PlayerFashionState const& state)
{
    std::ostringstream stream;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot > EQUIPMENT_SLOT_START)
            stream << ',';

        auto itr = state.resolvedSetIds.find(slot);
        uint32 setId = itr != state.resolvedSetIds.end() ? itr->second : 0;
        stream << setId;
    }

    return stream.str();
}

void ParseCompactFashionData(std::string const& compactSlots, std::string const& compactItems, std::string const& compactSetIds, PlayerFashionState& state)
{
    std::vector<std::string> slotParts = SplitText(compactSlots, ',');
    std::vector<std::string> itemParts = SplitText(compactItems, ',');
    std::vector<std::string> setParts = SplitText(compactSetIds, ',');

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        uint32 activeFlag = 0;
        uint32 parsedSlot = slot;
        uint32 itemEntry = 0;
        uint32 resolvedSetId = 0;

        if (slot < slotParts.size())
        {
            std::stringstream slotStream(slotParts[slot]);
            slotStream >> parsedSlot >> activeFlag;
        }

        if (slot < itemParts.size())
        {
            try
            {
                itemEntry = static_cast<uint32>(std::stoul(itemParts[slot]));
            }
            catch (...)
            {
                itemEntry = 0;
            }
        }

        if (slot < setParts.size())
        {
            try
            {
                resolvedSetId = static_cast<uint32>(std::stoul(setParts[slot]));
            }
            catch (...)
            {
                resolvedSetId = 0;
            }
        }

        if (parsedSlot != slot || activeFlag == 0 || itemEntry == 0)
            continue;

        if (sObjectMgr->GetItemTemplate(itemEntry))
        {
            state.visualOverrides[slot] = itemEntry;
            if (resolvedSetId > 0)
                state.resolvedSetIds[slot] = resolvedSetId;
        }
    }
}

uint32 GetResolvedSetIdForSlot(PlayerFashionState const& state, FashionSlotConfig const* slotConfig, uint8 slot)
{
    auto resolvedItr = state.resolvedSetIds.find(slot);
    if (resolvedItr != state.resolvedSetIds.end() && resolvedItr->second > 0)
        return resolvedItr->second;

    if (slotConfig && slotConfig->setAssignMode == FASHION_SET_ASSIGN_ID && slotConfig->setId > 0)
        return slotConfig->setId;

    return 0;
}

std::string BuildSetEffectDescFromItemSet(uint32 setId)
{
    if (setId == 0)
        return "";

    std::ostringstream stream;
    auto const& setDataList = sItemSetsManager->GetItemSetData(setId);

    bool hasEffect = false;
    for (ItemSetData const& setData : setDataList)
    {
        if (setData.EffectDesc.empty())
            continue;

        if (hasEffect)
            stream << ' ';

        stream << '[' << static_cast<uint32>(setData.ItemsCount) << "件] " << setData.EffectDesc;
        hasEffect = true;
    }

    if (!hasEffect)
        return "";

    return stream.str();
}

// 检查玩家是否持有指定物品（背包+身上+银行）
bool PlayerHasItemEntry(Player* player, uint32 itemEntry)
{
    if (!player)
        return false;

    // 身上装备 + 背包
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetEntry() == itemEntry)
            return true;
    }

    // 额外背包
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                Item* item = bag->GetItemByPos(slot);
                if (item && item->GetEntry() == itemEntry)
                    return true;
            }
        }
    }

    // 银行
    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_BAG_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetEntry() == itemEntry)
            return true;
    }

    for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                Item* item = bag->GetItemByPos(slot);
                if (item && item->GetEntry() == itemEntry)
                    return true;
            }
        }
    }

    return false;
}

bool PlayerHasEquippedItemInSlot(Player* player, uint8 slot)
{
    if (!player || !IsSupportedFashionSlot(slot))
        return false;

    return player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot) != nullptr;
}

// 仅销毁玩家背包中的一个物品（主背包 + 背包容器），不影响身上已装备物品和银行
bool DestroyOneInventoryItemEntry(Player* player, uint32 itemEntry)
{
    if (!player || itemEntry == 0)
        return false;

    uint32 count = 1;

    // AzerothCore 的 DestroyItemCount 这里只会处理主背包和背包容器，不会清理银行和已装备槽位
    player->DestroyItemCount(itemEntry, count, true);
    return true;
}

class FashionSystemMgr
{
public:
    static FashionSystemMgr* Instance()
    {
        static FashionSystemMgr instance;
        return &instance;
    }

    bool IsEnabled() const { return sConfigMgr->GetOption<bool>(CONF_ENABLE, true); }

    // 加载世界库时装配置
    void LoadWorldConfig()
    {
        _slotConfigs.clear();
        QueryResult result = WorldDatabase.Query("SELECT `id`, `需求系统id`, `套装分配模式`, `套装id`, `套装组`, `时装位置`, `时装描述` FROM `_时装系统`");
        if (result)
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                FashionSlotConfig config;
                config.id = fields[0].Get<uint32>();
                config.requirementId = fields[1].Get<uint32>();
                config.setAssignMode = fields[2].Get<uint8>();
                config.setId = fields[3].Get<uint32>();
                config.setGroup = fields[4].Get<uint32>();
                config.slot = fields[5].Get<uint8>();
                config.slotDesc = fields[6].Get<std::string>();

                if (config.slot < EQUIPMENT_SLOT_END)
                {
                    _slotConfigs[config.slot] = config;
                    ++count;
                }
            }
            while (result->NextRow());
        }
    }

    FashionSlotConfig const* GetSlotConfig(uint8 slot) const
    {
        auto itr = _slotConfigs.find(slot);
        return itr != _slotConfigs.end() ? &itr->second : nullptr;
    }

    std::unordered_map<uint8, FashionSlotConfig> const& GetAllSlotConfigs() const
    {
        return _slotConfigs;
    }

    PlayerFashionState* GetPlayerState(uint32 playerGuid)
    {
        auto itr = _playerStates.find(playerGuid);
        return itr != _playerStates.end() ? &itr->second : nullptr;
    }

    PlayerFashionState const* GetPlayerState(uint32 playerGuid) const
    {
        auto itr = _playerStates.find(playerGuid);
        return itr != _playerStates.end() ? &itr->second : nullptr;
    }

    uint32 ResolveSetIdForActivation(Player* player, FashionSlotConfig const* slotConfig, std::string* failureMessage = nullptr) const
    {
        if (!slotConfig)
            return 0;

        if (slotConfig->setAssignMode == FASHION_SET_ASSIGN_NONE)
            return 0;

        if (slotConfig->setAssignMode == FASHION_SET_ASSIGN_ID)
        {
            if (slotConfig->setId == 0)
            {
                if (failureMessage)
                    *failureMessage = "未配置固定套装ID";
                return 0;
            }

            if (sItemSetsManager->GetItemSetData(slotConfig->setId).empty())
            {
                if (failureMessage)
                    *failureMessage = "配置的套装ID不存在";
                return 0;
            }

            return slotConfig->setId;
        }

        if (slotConfig->setAssignMode == FASHION_SET_ASSIGN_GROUP)
        {
            if (slotConfig->setGroup == 0)
            {
                if (failureMessage)
                    *failureMessage = "未配置套装组";
                return 0;
            }

            uint32 resolvedSetId = sItemSetsManager->GetRandomSetForGroup(slotConfig->setGroup, player);
            if (resolvedSetId == 0)
            {
                if (failureMessage)
                    *failureMessage = "该套装组没有可用套装";
                return 0;
            }

            return resolvedSetId;
        }

        if (failureMessage)
            *failureMessage = "未知的套装分配模式";
        return 0;
    }

    bool NormalizePlayerResolvedSetIds(Player* player, PlayerFashionState& state) const
    {
        bool changed = false;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            auto visualItr = state.visualOverrides.find(slot);
            if (visualItr == state.visualOverrides.end() || visualItr->second == 0)
            {
                if (state.resolvedSetIds.erase(slot) > 0)
                    changed = true;
                continue;
            }

            FashionSlotConfig const* slotConfig = GetSlotConfig(slot);
            if (!slotConfig || slotConfig->setAssignMode == FASHION_SET_ASSIGN_NONE)
            {
                if (state.resolvedSetIds.erase(slot) > 0)
                    changed = true;
                continue;
            }

            if (slotConfig->setAssignMode == FASHION_SET_ASSIGN_ID)
            {
                if (state.resolvedSetIds[slot] != slotConfig->setId)
                {
                    state.resolvedSetIds[slot] = slotConfig->setId;
                    changed = true;
                }
                continue;
            }

            auto resolvedItr = state.resolvedSetIds.find(slot);
            bool validResolved = resolvedItr != state.resolvedSetIds.end() && resolvedItr->second > 0 &&
                !sItemSetsManager->GetItemSetData(resolvedItr->second).empty();

            if (!validResolved)
            {
                uint32 resolvedSetId = ResolveSetIdForActivation(player, slotConfig);
                if (resolvedSetId > 0)
                {
                    state.resolvedSetIds[slot] = resolvedSetId;
                    changed = true;
                }
                else if (state.resolvedSetIds.erase(slot) > 0)
                {
                    changed = true;
                }
            }
        }

        return changed;
    }

    uint32 GetResolvedSetId(PlayerFashionState const& state, uint8 slot) const
    {
        return GetResolvedSetIdForSlot(state, GetSlotConfig(slot), slot);
    }

    std::string GetEffectiveSetEffectDesc(PlayerFashionState const* state, uint8 slot) const
    {
        FashionSlotConfig const* slotConfig = GetSlotConfig(slot);
        if (state)
        {
            uint32 resolvedSetId = GetResolvedSetId(*state, slot);
            if (resolvedSetId > 0)
                return BuildSetEffectDescFromItemSet(resolvedSetId);
        }

        if (slotConfig && slotConfig->setAssignMode == FASHION_SET_ASSIGN_ID && slotConfig->setId > 0)
            return BuildSetEffectDescFromItemSet(slotConfig->setId);

        return "";
    }

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerFashionState state;

        QueryResult slotsResult = CharacterDatabase.Query("SELECT `是否启用`, `槽位`, `物品id`, `套装id` FROM `_玩家时装系统_穿戴` WHERE `玩家GUID` = {}", playerGuid);
        if (slotsResult)
        {
            Field* fields = slotsResult->Fetch();
            state.enabled = fields[0].Get<uint8>() != 0;
            ParseCompactFashionData(fields[1].Get<std::string>(), fields[2].Get<std::string>(), fields[3].Get<std::string>(), state);
        }

        bool changed = NormalizePlayerResolvedSetIds(player, state);
        _playerStates[playerGuid] = state;
        if (changed)
            SavePlayerData(playerGuid);
    }

    void SavePlayerData(uint32 playerGuid)
    {
        PlayerFashionState* state = GetPlayerState(playerGuid);
        if (!state)
            return;

        std::string compactSlots = BuildCompactSlotState(*state);
        std::string compactItems = BuildCompactItemEntryList(*state);
        std::string compactSetIds = BuildCompactResolvedSetIdList(*state);

        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家时装系统_穿戴` (`玩家GUID`, `是否启用`, `槽位`, `物品id`, `套装id`) VALUES ({}, {}, '{}', '{}', '{}')",
            playerGuid, state->enabled ? 1 : 0, compactSlots, compactItems, compactSetIds);
    }

    void UnloadPlayerData(Player* player)
    {
        if (player)
            _playerStates.erase(player->GetGUID().GetCounter());
    }

    void DeletePlayerData(ObjectGuid::LowType playerGuid)
    {
        _playerStates.erase(playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家时装系统_穿戴` WHERE `玩家GUID` = {}", playerGuid);
    }

    // 设置某个槽位的视觉覆盖，itemEntry=0 表示清除
    bool SetVisualOverride(Player* player, uint8 slot, uint32 itemEntry, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        if (!IsSupportedFashionSlot(slot))
        {
            if (failureMessage)
                *failureMessage = "装备位置无效";
            return false;
        }

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
        {
            LoadPlayerData(player);
            state = GetPlayerState(player->GetGUID().GetCounter());
        }
        if (!state)
        {
            if (failureMessage)
                *failureMessage = "玩家状态未加载";
            return false;
        }

        // 清除覆盖
        if (itemEntry == 0)
        {
            // 取消该槽位的套装效果
            uint32 oldSetId = GetResolvedSetId(*state, slot);
            auto oldItr = state->visualOverrides.find(slot);
            if (oldSetId > 0 && oldItr != state->visualOverrides.end() && oldItr->second > 0)
                sItemSetsManager->DeactivateItemSet(player, oldSetId);

            state->visualOverrides.erase(slot);
            state->resolvedSetIds.erase(slot);
            SavePlayerData(player->GetGUID().GetCounter());
            RefreshPlayerVisibleSlot(player, slot);
            return true;
        }

        // 验证物品模板存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry);
        if (!itemTemplate)
        {
            if (failureMessage)
                *failureMessage = "物品不存在";
            return false;
        }

        // 验证物品类型匹配槽位
        if (!CanItemEquipInSlot(itemTemplate->InventoryType, slot))
        {
            if (failureMessage)
                *failureMessage = "该物品不能装备在此位置";
            return false;
        }

        // 验证玩家持有该物品
        if (!PlayerHasItemEntry(player, itemEntry))
        {
            if (failureMessage)
                *failureMessage = "你没有该物品";
            return false;
        }

        // 必须先在对应槽位穿戴真实装备，才能进行时装幻化
        if (!PlayerHasEquippedItemInSlot(player, slot))
        {
            if (failureMessage)
                *failureMessage = std::string("请先穿戴") + GetFashionSlotName(slot) + "装备后再进行时装幻化";
            return false;
        }

        // 检查世界配置中该槽位的需求
        FashionSlotConfig const* slotConfig = GetSlotConfig(slot);
        if (slotConfig && slotConfig->requirementId > 0)
        {
#ifdef ACORE_WITH_REQUIREMENT_SYSTEM
            if (!sRequirementSystem->CheckRequirements(player, slotConfig->requirementId))
            {
                if (failureMessage)
                    *failureMessage = "不满足激活需求";
                return false;
            }
            if (!sRequirementSystem->ConsumeRequirements(player, slotConfig->requirementId))
            {
                if (failureMessage)
                    *failureMessage = "消耗需求失败";
                return false;
            }
#endif
        }

        uint32 newResolvedSetId = ResolveSetIdForActivation(player, slotConfig, failureMessage);
        if ((slotConfig && slotConfig->setAssignMode != FASHION_SET_ASSIGN_NONE) && newResolvedSetId == 0)
            return false;

        uint32 oldResolvedSetId = GetResolvedSetId(*state, slot);
        auto oldItr = state->visualOverrides.find(slot);
        if (oldResolvedSetId > 0 && oldItr != state->visualOverrides.end() && oldItr->second > 0)
            sItemSetsManager->DeactivateItemSet(player, oldResolvedSetId);

        state->visualOverrides[slot] = itemEntry;
        if (newResolvedSetId > 0)
            state->resolvedSetIds[slot] = newResolvedSetId;
        else
            state->resolvedSetIds.erase(slot);
        SavePlayerData(player->GetGUID().GetCounter());
        RefreshPlayerVisibleSlot(player, slot);

        // 激活成功后消耗一件背包中的时装模型装备，避免残留占用背包空间
        DestroyOneInventoryItemEntry(player, itemEntry);

        // 激活该槽位的套装效果
        if (newResolvedSetId > 0)
            sItemSetsManager->ApplySingleSetEffect(player, newResolvedSetId, 1);

        return true;
    }

    void ClearAllOverrides(Player* player)
    {
        if (!player)
            return;

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return;

        // 取消所有有套装效果的槽位
        for (auto const& [slot, itemEntry] : state->visualOverrides)
        {
            if (itemEntry == 0)
                continue;
            uint32 resolvedSetId = GetResolvedSetId(*state, slot);
            if (resolvedSetId > 0)
                sItemSetsManager->DeactivateItemSet(player, resolvedSetId);
        }

        state->visualOverrides.clear();
        state->resolvedSetIds.clear();
        SavePlayerData(player->GetGUID().GetCounter());
        RefreshAllVisibleSlots(player);
    }

    void SetPlayerEnabled(Player* player, bool enabled)
    {
        if (!player)
            return;

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
        {
            LoadPlayerData(player);
            state = GetPlayerState(player->GetGUID().GetCounter());
        }
        if (!state)
            return;

        bool wasEnabled = state->enabled;
        state->enabled = enabled;
        SavePlayerData(player->GetGUID().GetCounter());
        RefreshAllVisibleSlots(player);

        // 启用/关闭时同步套装效果
        if (enabled && !wasEnabled)
            ApplyAllFashionSetEffects(player);
        else if (!enabled && wasEnabled)
            RemoveAllFashionSetEffects(player);
    }

    void RefreshPlayerVisibleSlot(Player* player, uint8 slot) const
    {
        if (!player || !IsSupportedFashionSlot(slot))
            return;

        PlayerFashionState const* state = GetPlayerState(player->GetGUID().GetCounter());
        Item* actualItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        uint32 overrideEntry = 0;

        if (state && state->enabled)
        {
            auto itr = state->visualOverrides.find(slot);
            if (itr != state->visualOverrides.end())
                overrideEntry = itr->second;
        }

        if (overrideEntry > 0 && actualItem)
        {
            SetPlayerVisibleItemDirect(player, slot, overrideEntry, 0, 0);
            return;
        }

        if (actualItem)
            SetPlayerVisibleItemDirect(player, slot, actualItem->GetEntry(), actualItem->GetEnchantmentId(PERM_ENCHANTMENT_SLOT), actualItem->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT));
        else
            SetPlayerVisibleItemDirect(player, slot, 0, 0, 0);
    }

    void RefreshAllVisibleSlots(Player* player) const
    {
        if (!player)
            return;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            RefreshPlayerVisibleSlot(player, slot);
    }

    // 登录时恢复所有已激活槽位的套装效果
    void ApplyAllFashionSetEffects(Player* player) const
    {
        if (!player)
            return;

        PlayerFashionState const* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state || !state->enabled)
            return;

        for (auto const& [slot, itemEntry] : state->visualOverrides)
        {
            if (itemEntry == 0 || !PlayerHasEquippedItemInSlot(player, slot))
                continue;
            uint32 resolvedSetId = GetResolvedSetId(*state, slot);
            if (resolvedSetId > 0)
                sItemSetsManager->ApplySingleSetEffect(player, resolvedSetId, 1);
        }
    }

    // 登出时移除所有时装套装效果
    void RemoveAllFashionSetEffects(Player* player) const
    {
        if (!player)
            return;

        PlayerFashionState const* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return;

        for (auto const& [slot, itemEntry] : state->visualOverrides)
        {
            if (itemEntry == 0)
                continue;
            uint32 resolvedSetId = GetResolvedSetId(*state, slot);
            if (resolvedSetId > 0)
                sItemSetsManager->DeactivateItemSet(player, resolvedSetId);
        }
    }

private:
    std::unordered_map<uint32, PlayerFashionState> _playerStates;
    std::unordered_map<uint8, FashionSlotConfig> _slotConfigs;
};

// ============== 通信层 ==============

void SendFashionSystemPayload(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession() || payload.empty())
        return;

    if (payload.length() <= MAX_ADDON_PAYLOAD)
    {
        std::string fullMessage = std::string(FASHION_SYSTEM_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
        return;
    }

    size_t totalChunks = (payload.length() + MAX_ADDON_PAYLOAD - 1) / MAX_ADDON_PAYLOAD;
    for (size_t i = 0; i < totalChunks; ++i)
    {
        size_t start = i * MAX_ADDON_PAYLOAD;
        size_t len = std::min(MAX_ADDON_PAYLOAD, payload.length() - start);
        std::ostringstream chunk;
        chunk << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << payload.substr(start, len);

        std::string fullMessage = std::string(FASHION_SYSTEM_ADDON_PREFIX) + '\t' + chunk.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

// 发送当前状态给客户端：FS_STATE:enabled|slot^itemEntry^quality^name^setEffectDesc^slotDesc^resolvedSetId~...
void SendFashionStateToPlayer(Player* player)
{
    if (!player)
        return;

    PlayerFashionState const* state = FashionSystemMgr::Instance()->GetPlayerState(player->GetGUID().GetCounter());
    if (!state)
        return;

    std::ostringstream payload;
    payload << "FS_STATE:" << (state->enabled ? 1 : 0) << '|';

    bool first = true;
    for (auto const& [slot, itemEntry] : state->visualOverrides)
    {
        if (itemEntry == 0)
            continue;

        if (!first)
            payload << '~';
        first = false;

        ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(itemEntry);
        std::string itemName = tmpl ? tmpl->Name1 : "";
        uint32 quality = tmpl ? tmpl->Quality : 0;

        FashionSlotConfig const* slotConfig = FashionSystemMgr::Instance()->GetSlotConfig(slot);
        uint32 resolvedSetId = slotConfig ? FashionSystemMgr::Instance()->GetResolvedSetId(*state, slot) : 0;
        std::string setEffectDesc = FashionSystemMgr::Instance()->GetEffectiveSetEffectDesc(state, slot);
        std::string slotDesc = slotConfig ? slotConfig->slotDesc : "";

        payload << static_cast<uint32>(slot) << '^'
                << itemEntry << '^'
                << quality << '^'
                << SanitizeAddonText(itemName) << '^'
                << SanitizeAddonText(setEffectDesc) << '^'
                << SanitizeAddonText(slotDesc) << '^'
                << resolvedSetId;
    }

    SendFashionSystemPayload(player, payload.str());
}

void SendFashionActionResult(Player* player, char const* action, bool success, uint32 slot, uint32 itemEntry, std::string const& message)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "FS_RESULT:"
            << (action ? action : "") << '^'
            << (success ? "OK" : "FAIL") << '^'
            << slot << '^'
            << itemEntry << '^'
            << SanitizeAddonText(message);
    SendFashionSystemPayload(player, payload.str());
}

// 发送世界配置给客户端：FS_CONFIG:slot^requirementId^setAssignMode^setId^setGroup^setEffectDesc^slotDesc~...
void SendFashionConfigToPlayer(Player* player)
{
    if (!player)
        return;

    auto const& configs = FashionSystemMgr::Instance()->GetAllSlotConfigs();
    if (configs.empty())
        return;

    std::ostringstream payload;
    payload << "FS_CONFIG:";

    bool first = true;
    for (auto const& [slot, config] : configs)
    {
        if (!first)
            payload << '~';
        first = false;

        std::string setEffectDesc = FashionSystemMgr::Instance()->GetEffectiveSetEffectDesc(nullptr, slot);

        payload << static_cast<uint32>(config.slot) << '^'
                << config.requirementId << '^'
                << static_cast<uint32>(config.setAssignMode) << '^'
                << config.setId << '^'
                << config.setGroup << '^'
                << SanitizeAddonText(setEffectDesc) << '^'
                << SanitizeAddonText(config.slotDesc);
    }

    SendFashionSystemPayload(player, payload.str());
}

// ============== 脚本钩子 ==============

class FashionSystemWorldScript : public WorldScript
{
public:
    FashionSystemWorldScript() : WorldScript("FashionSystemWorldScript") { }

    void OnStartup() override
    {
        if (FashionSystemMgr::Instance()->IsEnabled())
        {
            FashionSystemMgr::Instance()->LoadWorldConfig();
            LOG_INFO("server.loading", "→时装系统√");
        }
        else
            LOG_INFO("module", "时装系统已禁用。");
    }
};

class FashionSystemPlayerScript : public PlayerScript
{
public:
    FashionSystemPlayerScript() : PlayerScript("FashionSystemPlayerScript") { }

    class FashionSystemLoginRefreshEvent : public BasicEvent
    {
    public:
        explicit FashionSystemLoginRefreshEvent(Player* player) : _player(player) { }

        bool Execute(uint64 /*time*/, uint32 /*diff*/) override
        {
            if (!_player || !_player->IsInWorld() || !FashionSystemMgr::Instance()->IsEnabled())
                return true;

            FashionSystemMgr::Instance()->RefreshAllVisibleSlots(_player);
            FashionSystemMgr::Instance()->ApplyAllFashionSetEffects(_player);
            return true;
        }

    private:
        Player* _player;
    };

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !FashionSystemMgr::Instance()->IsEnabled())
            return;

        FashionSystemMgr::Instance()->LoadPlayerData(player);
        FashionSystemMgr::Instance()->RefreshAllVisibleSlots(player);
        FashionSystemMgr::Instance()->ApplyAllFashionSetEffects(player);
        player->m_Events.AddEvent(new FashionSystemLoginRefreshEvent(player), player->m_Events.CalculateTime(1000));
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
        {
            FashionSystemMgr::Instance()->RemoveAllFashionSetEffects(player);
            FashionSystemMgr::Instance()->UnloadPlayerData(player);
        }
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        FashionSystemMgr::Instance()->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* /*item*/) override
    {
        if (!player || !FashionSystemMgr::Instance()->IsEnabled())
            return;

        if (IsSupportedFashionSlot(slot))
        {
            FashionSystemMgr::Instance()->RefreshPlayerVisibleSlot(player, slot);
            FashionSystemMgr::Instance()->RemoveAllFashionSetEffects(player);
            FashionSystemMgr::Instance()->ApplyAllFashionSetEffects(player);
        }
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !FashionSystemMgr::Instance()->IsEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos || msg.substr(0, tabPos) != FASHION_SYSTEM_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);

        if (command == "REQ_STATE")
        {
            if (!FashionSystemMgr::Instance()->GetPlayerState(player->GetGUID().GetCounter()))
            {
                FashionSystemMgr::Instance()->LoadPlayerData(player);
                FashionSystemMgr::Instance()->RefreshAllVisibleSlots(player);
            }
            SendFashionConfigToPlayer(player);
            SendFashionStateToPlayer(player);
            return;
        }

        if (command == "CLEAR_ALL")
        {
            FashionSystemMgr::Instance()->ClearAllOverrides(player);
            SendFashionActionResult(player, "CLEAR_ALL", true, 0, 0, "已清空所有时装外观");
            SendFashionStateToPlayer(player);
            return;
        }

        if (command.rfind("EN:", 0) == 0)
        {
            bool enabled = command.substr(3) != "0";
            FashionSystemMgr::Instance()->SetPlayerEnabled(player, enabled);
            SendFashionActionResult(player, "ENABLE", true, 0, 0, enabled ? "已启用时装系统" : "已关闭时装系统");
            SendFashionStateToPlayer(player);
            return;
        }

        // SET:slot:itemEntry — 设置视觉覆盖
        if (command.rfind("SET:", 0) == 0)
        {
            std::string payload = command.substr(4);
            size_t sepPos = payload.find(':');
            if (sepPos == std::string::npos)
            {
                SendFashionActionResult(player, "SET", false, 0, 0, "参数错误");
                return;
            }

            uint32 slot = 0;
            uint32 itemEntry = 0;
            try
            {
                slot = static_cast<uint32>(std::stoul(payload.substr(0, sepPos)));
                itemEntry = static_cast<uint32>(std::stoul(payload.substr(sepPos + 1)));
            }
            catch (...)
            {
                SendFashionActionResult(player, "SET", false, 0, 0, "参数错误");
                return;
            }

            std::string failureMessage;
            bool success = FashionSystemMgr::Instance()->SetVisualOverride(player, static_cast<uint8>(slot), itemEntry, &failureMessage);
            SendFashionActionResult(player, "SET", success, slot, itemEntry,
                success ? (itemEntry == 0 ? "已清除该位置外观" : "外观已替换") : (failureMessage.empty() ? "替换失败" : failureMessage));
            SendFashionStateToPlayer(player);
        }
    }
};

class FashionSystemCommandScript : public CommandScript
{
public:
    FashionSystemCommandScript() : CommandScript("FashionSystemCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable subTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("界面", HandleOpenUICommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("清空", HandleClearCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("启用", HandleEnableCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("关闭", HandleDisableCommand, SEC_PLAYER, Acore::ChatCommands::Console::No)
        };

        static Acore::ChatCommands::ChatCommandTable rootTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("时装", subTable),
            Acore::ChatCommands::ChatCommandBuilder("时装系统", subTable)
        };

        return rootTable;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        SendFashionSystemPayload(player, "FS_OPEN");
        return true;
    }

    static bool HandleClearCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        FashionSystemMgr::Instance()->ClearAllOverrides(player);
        handler->SendSysMessage("时装系统：所有外观覆盖已清空。");
        return true;
    }

    static bool HandleEnableCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        FashionSystemMgr::Instance()->SetPlayerEnabled(player, true);
        handler->SendSysMessage("时装系统：已启用。");
        return true;
    }

    static bool HandleDisableCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        FashionSystemMgr::Instance()->SetPlayerEnabled(player, false);
        handler->SendSysMessage("时装系统：已关闭。");
        return true;
    }
};
}

void AddSC_mod_fashion_system()
{
    new FashionSystemWorldScript();
    new FashionSystemPlayerScript();
    new FashionSystemCommandScript();
}

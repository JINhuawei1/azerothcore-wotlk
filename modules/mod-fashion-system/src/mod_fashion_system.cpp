/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#if __has_include("RequirementSystem.h")
#include "RequirementSystem.h"
#define FASHION_SYSTEM_HAS_REQUIREMENT_SYSTEM 1
#else
#define FASHION_SYSTEM_HAS_REQUIREMENT_SYSTEM 0
#endif

#if __has_include("ItemSets.h")
#if defined(__INTELLISENSE__)
#include <cstdint>
#include <string>
#include <vector>

class Player;

struct ItemSetData
{
    std::uint8_t ItemsCount = 0;
    std::string SetAttributes;
    std::uint32_t EffectId = 0;
    float EffectValue = 0.0f;
};

class ItemSetsManager
{
public:
    static ItemSetsManager* instance();
    void ApplyStatEffects(Player* player, std::string const& attributes, bool apply);
    void RemoveSpellEnhancement(Player* player, std::uint32_t spellId);
    std::vector<ItemSetData> const& GetItemSetData(std::uint32_t setId) const;
    void AddSpellEnhancement(Player* player, std::uint32_t spellId, float enhancePercent);
};

#define sItemSetsManager ItemSetsManager::instance()
#else
#include "ItemSets.h"
#endif
#define FASHION_SYSTEM_HAS_ITEM_SETS 1
#else
#define FASHION_SYSTEM_HAS_ITEM_SETS 0
#endif

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
    virtual void OnUpdate(uint32 /*diff*/) { }
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
    virtual void OnPlayerStoreNewItem(Player* /*player*/, Item* /*item*/, uint32 /*count*/) { }
    virtual void OnPlayerCreateItem(Player* /*player*/, Item* /*item*/, uint32 /*count*/) { }
    virtual void OnPlayerQuestRewardItem(Player* /*player*/, Item* /*item*/, uint32 /*count*/) { }
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
#include "SpellDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UpdateFields.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr char const* CONF_ENABLE = "FashionSystem.Enable";
constexpr char const* CONF_DEBUG = "FashionSystem.Debug";
constexpr char FASHION_SYSTEM_ADDON_PREFIX[] = "FASHION_SYS";
constexpr size_t MAX_ADDON_PAYLOAD = 220;

enum FashionSlot : uint8
{
    FASHION_SLOT_HANDS = 1,
    FASHION_SLOT_WAIST = 2,
    FASHION_SLOT_LEGS = 3,
    FASHION_SLOT_FEET = 4,
    FASHION_SLOT_BACK = 5,
    FASHION_SLOT_SHOULDERS = 6,
    FASHION_SLOT_HEAD = 7,
    FASHION_SLOT_WRISTS = 8
};

struct FashionEntry
{
    uint32 id = 0;
    uint32 itemId = 0;
    uint32 requirementTemplateId = 0;
    uint8 fashionSlot = 0;
    uint32 groupId = 0;
    uint32 setId = 0;
    std::string itemName;
    uint32 quality = 0;
};

struct PlayerFashionState
{
    bool enabled = true;
    std::unordered_set<uint32> unlockedFashionIds;
    std::unordered_map<uint8, uint32> selectedFashionBySlot;
    std::unordered_map<uint32, std::string> appliedSetAttributes;
    std::unordered_map<uint32, std::vector<uint32>> appliedSetAuraIds;
    std::unordered_map<uint32, std::vector<uint32>> appliedSetSkillIds;
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
    return slot >= FASHION_SLOT_HANDS && slot <= FASHION_SLOT_WRISTS;
}

char const* GetFashionSlotName(uint8 slot)
{
    switch (slot)
    {
        case FASHION_SLOT_HANDS: return "手";
        case FASHION_SLOT_WAIST: return "腰带";
        case FASHION_SLOT_LEGS: return "裤子";
        case FASHION_SLOT_FEET: return "脚";
        case FASHION_SLOT_BACK: return "背部";
        case FASHION_SLOT_SHOULDERS: return "肩部";
        case FASHION_SLOT_HEAD: return "头";
        case FASHION_SLOT_WRISTS: return "护腕";
        default: return "未知";
    }
}

uint8 GetVisibleEquipmentSlot(uint8 fashionSlot)
{
    switch (fashionSlot)
    {
        case FASHION_SLOT_HANDS: return EQUIPMENT_SLOT_HANDS;
        case FASHION_SLOT_WAIST: return EQUIPMENT_SLOT_WAIST;
        case FASHION_SLOT_LEGS: return EQUIPMENT_SLOT_LEGS;
        case FASHION_SLOT_FEET: return EQUIPMENT_SLOT_FEET;
        case FASHION_SLOT_BACK: return EQUIPMENT_SLOT_BACK;
        case FASHION_SLOT_SHOULDERS: return EQUIPMENT_SLOT_SHOULDERS;
        case FASHION_SLOT_HEAD: return EQUIPMENT_SLOT_HEAD;
        case FASHION_SLOT_WRISTS: return EQUIPMENT_SLOT_WRISTS;
        default: return EQUIPMENT_SLOT_END;
    }
}

uint8 GetFashionSlotByVisibleSlot(uint8 visibleSlot)
{
    switch (visibleSlot)
    {
        case EQUIPMENT_SLOT_HANDS: return FASHION_SLOT_HANDS;
        case EQUIPMENT_SLOT_WAIST: return FASHION_SLOT_WAIST;
        case EQUIPMENT_SLOT_LEGS: return FASHION_SLOT_LEGS;
        case EQUIPMENT_SLOT_FEET: return FASHION_SLOT_FEET;
        case EQUIPMENT_SLOT_BACK: return FASHION_SLOT_BACK;
        case EQUIPMENT_SLOT_SHOULDERS: return FASHION_SLOT_SHOULDERS;
        case EQUIPMENT_SLOT_HEAD: return FASHION_SLOT_HEAD;
        case EQUIPMENT_SLOT_WRISTS: return FASHION_SLOT_WRISTS;
        default: return 0;
    }
}

template <typename Func>
void ForEachPlayerItem(Player* player, bool includeBank, Func&& func)
{
    if (!player)
        return;

    auto handleItem = [&](Item* item)
    {
        if (item)
            func(item);
    };

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        handleItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 slot = KEYRING_SLOT_START; slot < CURRENCYTOKEN_SLOT_END; ++slot)
        handleItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                handleItem(bag->GetItemByPos(slot));
        }
    }

    if (!includeBank)
        return;

    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_BAG_END; ++slot)
        handleItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                handleItem(bag->GetItemByPos(slot));
        }
    }
}

bool IsPositiveSelfAuraEffect(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->IsPassive() && spellInfo->HasAnyAura() && spellInfo->IsSelfCast())
        return true;

    if (spellInfo->IsPositive() && spellInfo->IsSelfCast() && spellInfo->HasAnyAura() && !spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
        return true;

    return false;
}

void SetPlayerVisibleItemDirect(Player* player, uint8 visibleSlot, uint32 itemEntry, uint16 permEnchant, uint16 tempEnchant)
{
    if (!player || visibleSlot >= EQUIPMENT_SLOT_END)
        return;

    player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (visibleSlot * 2), itemEntry);
    player->SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (visibleSlot * 2), 0, permEnchant);
    player->SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (visibleSlot * 2), 1, tempEnchant);
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
    bool IsDebugEnabled() const { return sConfigMgr->GetOption<bool>(CONF_DEBUG, false); }

    void LoadEntries()
    {
        _entries.clear();
        _entriesById.clear();
        _entryIdsByItemId.clear();

        QueryResult result = WorldDatabase.Query("SELECT `id`, `物品id`, `需求系统id`, `时装穿戴位置`, `组`, `套装id` FROM `_时装系统` ORDER BY `组`, `时装穿戴位置`, `id`");
        if (!result)
        {
            LOG_WARN("server.loading", "时装系统未加载到任何配置，请检查表 `_时装系统`。");
            return;
        }

        do
        {
            Field* fields = result->Fetch();

            FashionEntry entry;
            entry.id = fields[0].Get<uint32>();
            entry.itemId = fields[1].Get<uint32>();
            entry.requirementTemplateId = fields[2].Get<uint32>();
            entry.fashionSlot = fields[3].Get<uint8>();
            entry.groupId = fields[4].Get<uint32>();
            entry.setId = fields[5].Get<uint32>();

            if (!entry.id || !entry.itemId || !IsSupportedFashionSlot(entry.fashionSlot))
            {
                LOG_ERROR("sql.sql", "时装系统存在非法配置: id={}, item={}, slot={}", entry.id, entry.itemId, static_cast<uint32>(entry.fashionSlot));
                continue;
            }

            if (entry.groupId == 0)
            {
                LOG_ERROR("sql.sql", "时装系统存在非法分组配置: id={}, item={}, group=0", entry.id, entry.itemId);
                continue;
            }

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
            if (!itemTemplate)
            {
                LOG_ERROR("sql.sql", "时装系统配置 id={} 的物品模板 {} 不存在。", entry.id, entry.itemId);
                continue;
            }

            entry.itemName = itemTemplate->Name1;
            entry.quality = itemTemplate->Quality;

            _entries.push_back(entry);
            _entriesById[entry.id] = entry;
            _entryIdsByItemId[entry.itemId].push_back(entry.id);
        }
        while (result->NextRow());

        LOG_INFO("server.loading", ">> 时装系统已加载 {} 条配置。", static_cast<uint32>(_entries.size()));
    }

    FashionEntry const* GetEntryById(uint32 fashionId) const
    {
        auto itr = _entriesById.find(fashionId);
        return itr != _entriesById.end() ? &itr->second : nullptr;
    }

    std::vector<FashionEntry> const& GetEntries() const
    {
        return _entries;
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

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerFashionState state;

        QueryResult unlockResult = CharacterDatabase.Query("SELECT `时装id` FROM `_玩家时装系统_收集` WHERE `玩家GUID` = {}", playerGuid);
        if (unlockResult)
        {
            do
            {
                uint32 fashionId = unlockResult->Fetch()[0].Get<uint32>();
                if (_entriesById.find(fashionId) != _entriesById.end())
                    state.unlockedFashionIds.insert(fashionId);
            }
            while (unlockResult->NextRow());
        }

        QueryResult schemeResult = CharacterDatabase.Query("SELECT `是否启用`, `手`, `腰带`, `裤子`, `脚`, `背部`, `肩部`, `头`, `护腕` FROM `_玩家时装系统_方案` WHERE `玩家GUID` = {}", playerGuid);
        if (schemeResult)
        {
            Field* fields = schemeResult->Fetch();
            state.enabled = fields[0].Get<uint8>() != 0;

            static uint8 const slotMap[] =
            {
                FASHION_SLOT_HANDS,
                FASHION_SLOT_WAIST,
                FASHION_SLOT_LEGS,
                FASHION_SLOT_FEET,
                FASHION_SLOT_BACK,
                FASHION_SLOT_SHOULDERS,
                FASHION_SLOT_HEAD,
                FASHION_SLOT_WRISTS
            };

            for (uint32 index = 0; index < 8; ++index)
            {
                uint32 fashionId = fields[index + 1].Get<uint32>();
                FashionEntry const* entry = GetEntryById(fashionId);
                if (fashionId && entry && entry->fashionSlot == slotMap[index])
                    state.selectedFashionBySlot[slotMap[index]] = fashionId;
            }
        }

        _playerStates[playerGuid] = state;
    }

    void SavePlayerScheme(uint32 playerGuid)
    {
        PlayerFashionState* state = GetPlayerState(playerGuid);
        if (!state)
            return;

        auto getSelected = [&](uint8 slot) -> uint32
        {
            auto itr = state->selectedFashionBySlot.find(slot);
            return itr != state->selectedFashionBySlot.end() ? itr->second : 0;
        };

        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家时装系统_方案` (`玩家GUID`, `是否启用`, `手`, `腰带`, `裤子`, `脚`, `背部`, `肩部`, `头`, `护腕`) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
            playerGuid,
            state->enabled ? 1 : 0,
            getSelected(FASHION_SLOT_HANDS),
            getSelected(FASHION_SLOT_WAIST),
            getSelected(FASHION_SLOT_LEGS),
            getSelected(FASHION_SLOT_FEET),
            getSelected(FASHION_SLOT_BACK),
            getSelected(FASHION_SLOT_SHOULDERS),
            getSelected(FASHION_SLOT_HEAD),
            getSelected(FASHION_SLOT_WRISTS));
    }

    void ClearAppliedSetEffects(Player* player)
    {
        if (!player)
            return;

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return;

#if FASHION_SYSTEM_HAS_ITEM_SETS
        if (sItemSetsManager)
        {
            for (auto const& [setId, attributes] : state->appliedSetAttributes)
            {
                if (!attributes.empty())
                    sItemSetsManager->ApplyStatEffects(player, attributes, false);
            }

            for (auto const& [setId, skillIds] : state->appliedSetSkillIds)
            {
                for (uint32 spellId : skillIds)
                    sItemSetsManager->RemoveSpellEnhancement(player, spellId);
            }
        }
#endif

        for (auto const& [setId, auraIds] : state->appliedSetAuraIds)
        {
            for (uint32 spellId : auraIds)
                player->RemoveAurasDueToSpell(spellId);
        }

        state->appliedSetAttributes.clear();
        state->appliedSetAuraIds.clear();
        state->appliedSetSkillIds.clear();
    }

    void UnloadPlayerData(Player* player)
    {
        if (!player)
            return;

        ClearAppliedSetEffects(player);
        _playerStates.erase(player->GetGUID().GetCounter());
    }

    void DeletePlayerData(ObjectGuid::LowType playerGuid)
    {
        _playerStates.erase(playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家时装系统_收集` WHERE `玩家GUID` = {}", playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家时装系统_方案` WHERE `玩家GUID` = {}", playerGuid);
    }

    bool MeetsRequirements(Player* player, FashionEntry const& entry, bool showMessages) const
    {
        if (!player)
            return false;

        if (entry.requirementTemplateId == 0)
            return true;

#if FASHION_SYSTEM_HAS_REQUIREMENT_SYSTEM
        if (!sRequirementSystem)
        {
            if (showMessages && player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[时装]|r 需求模板系统未加载，暂时无法使用该时装。");
            return false;
        }

        bool meets = sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, false);
        if (!meets && showMessages && player->GetSession())
        {
            ChatHandler handler(player->GetSession());
            handler.SendSysMessage("|cffffcc00[时装]|r 当前不满足该时装的使用条件");
            sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, true);
        }

        return meets;
#else
        if (showMessages && player->GetSession())
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[时装]|r 需求模板系统模块未编译。");
        return false;
#endif
    }

    uint32 CollectPlayerUnlocks(Player* player)
    {
        if (!player)
            return 0;

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return 0;

        uint32 unlockedCount = 0;
        uint32 playerGuid = player->GetGUID().GetCounter();

        ForEachPlayerItem(player, true, [&](Item* item)
        {
            auto itr = _entryIdsByItemId.find(item->GetEntry());
            if (itr == _entryIdsByItemId.end())
                return;

            for (uint32 fashionId : itr->second)
            {
                if (state->unlockedFashionIds.insert(fashionId).second)
                {
                    CharacterDatabase.Execute("INSERT IGNORE INTO `_玩家时装系统_收集` (`玩家GUID`, `时装id`) VALUES ({}, {})", playerGuid, fashionId);
                    ++unlockedCount;
                }
            }
        });

        return unlockedCount;
    }

    bool UnlockFromItem(Player* player, Item* item, uint32* unlockedCount = nullptr)
    {
        if (!player || !item)
            return false;

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return false;

        auto itr = _entryIdsByItemId.find(item->GetEntry());
        if (itr == _entryIdsByItemId.end())
            return false;

        uint32 localCount = 0;
        uint32 playerGuid = player->GetGUID().GetCounter();
        for (uint32 fashionId : itr->second)
        {
            if (state->unlockedFashionIds.insert(fashionId).second)
            {
                CharacterDatabase.Execute("INSERT IGNORE INTO `_玩家时装系统_收集` (`玩家GUID`, `时装id`) VALUES ({}, {})", playerGuid, fashionId);
                ++localCount;
            }
        }

        if (unlockedCount)
            *unlockedCount = localCount;

        return localCount > 0;
    }

    FashionEntry const* GetSelectedFashionEntry(Player* player, uint8 slot, bool requireUsable) const
    {
        if (!player)
            return nullptr;

        PlayerFashionState const* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return nullptr;

        auto itr = state->selectedFashionBySlot.find(slot);
        if (itr == state->selectedFashionBySlot.end())
            return nullptr;

        FashionEntry const* entry = GetEntryById(itr->second);
        if (!entry || entry->fashionSlot != slot)
            return nullptr;

        if (state->unlockedFashionIds.find(entry->id) == state->unlockedFashionIds.end())
            return nullptr;

        if (requireUsable && !MeetsRequirements(player, *entry, false))
            return nullptr;

        return entry;
    }

    void RefreshPlayerVisibleSlot(Player* player, uint8 fashionSlot) const
    {
        if (!player || !IsSupportedFashionSlot(fashionSlot))
            return;

        uint8 visibleSlot = GetVisibleEquipmentSlot(fashionSlot);
        PlayerFashionState const* state = GetPlayerState(player->GetGUID().GetCounter());
        FashionEntry const* entry = (state && state->enabled) ? GetSelectedFashionEntry(player, fashionSlot, true) : nullptr;

        if (entry)
        {
            SetPlayerVisibleItemDirect(player, visibleSlot, entry->itemId, 0, 0);
            return;
        }

        Item* actualItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, visibleSlot);
        if (actualItem)
            SetPlayerVisibleItemDirect(player, visibleSlot, actualItem->GetEntry(), actualItem->GetEnchantmentId(PERM_ENCHANTMENT_SLOT), actualItem->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT));
        else
            SetPlayerVisibleItemDirect(player, visibleSlot, 0, 0, 0);
    }

    void RefreshAllVisibleSlots(Player* player) const
    {
        if (!player)
            return;

        for (uint8 slot = FASHION_SLOT_HANDS; slot <= FASHION_SLOT_WRISTS; ++slot)
            RefreshPlayerVisibleSlot(player, slot);
    }

    void RefreshPlayerFashionEffects(Player* player)
    {
        if (!player)
            return;

        PlayerFashionState* state = GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
            return;

        ClearAppliedSetEffects(player);
        if (!state->enabled)
            return;

#if FASHION_SYSTEM_HAS_ITEM_SETS
        if (!sItemSetsManager)
            return;

        std::unordered_map<uint32, uint8> setCounts;
        for (auto const& [slot, fashionId] : state->selectedFashionBySlot)
        {
            FashionEntry const* entry = GetSelectedFashionEntry(player, slot, true);
            if (entry && entry->setId > 0)
                ++setCounts[entry->setId];
        }

        for (auto const& [setId, count] : setCounts)
        {
            std::vector<ItemSetData> const& setDataList = sItemSetsManager->GetItemSetData(setId);
            if (setDataList.empty())
                continue;

            std::string combinedAttributes;
            std::unordered_set<uint32> auraIds;
            std::unordered_map<uint32, float> skillIds;

            for (ItemSetData const& setData : setDataList)
            {
                if (setData.ItemsCount > count)
                    continue;

                if (!setData.SetAttributes.empty())
                {
                    if (!combinedAttributes.empty())
                        combinedAttributes += ",";
                    combinedAttributes += setData.SetAttributes;
                }

                if (!setData.EffectId)
                    continue;

                if (IsPositiveSelfAuraEffect(setData.EffectId))
                    auraIds.insert(setData.EffectId);
                else
                    skillIds[setData.EffectId] = setData.EffectValue;
            }

            if (!combinedAttributes.empty())
            {
                sItemSetsManager->ApplyStatEffects(player, combinedAttributes, true);
                state->appliedSetAttributes[setId] = combinedAttributes;
            }

            for (uint32 spellId : auraIds)
            {
                player->RemoveAurasDueToSpell(spellId);
                if (Aura* aura = player->AddAura(spellId, player))
                {
                    aura->SetDuration(-1);
                    aura->SetMaxDuration(-1);
                    state->appliedSetAuraIds[setId].push_back(spellId);
                }
            }

            for (auto const& [spellId, effectValue] : skillIds)
            {
                sItemSetsManager->AddSpellEnhancement(player, spellId, effectValue);
                state->appliedSetSkillIds[setId].push_back(spellId);
            }
        }
#endif
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

        state->enabled = enabled;
        SavePlayerScheme(player->GetGUID().GetCounter());
        RefreshPlayerFashionEffects(player);
        RefreshAllVisibleSlots(player);
    }

    bool SetPlayerFashion(Player* player, uint8 slot, uint32 fashionId, std::string* failureMessage = nullptr)
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
                *failureMessage = "时装位置无效";
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

        if (fashionId == 0)
        {
            state->selectedFashionBySlot.erase(slot);
            SavePlayerScheme(player->GetGUID().GetCounter());
            RefreshPlayerFashionEffects(player);
            RefreshPlayerVisibleSlot(player, slot);
            return true;
        }

        FashionEntry const* entry = GetEntryById(fashionId);
        if (!entry)
        {
            if (failureMessage)
                *failureMessage = "未找到该时装";
            return false;
        }

        if (entry->fashionSlot != slot)
        {
            if (failureMessage)
                *failureMessage = "该时装与目标位置不匹配";
            return false;
        }

        if (state->unlockedFashionIds.find(fashionId) == state->unlockedFashionIds.end())
        {
            if (failureMessage)
                *failureMessage = "该时装尚未解锁";
            return false;
        }

        if (!MeetsRequirements(player, *entry, true))
        {
            if (failureMessage)
                *failureMessage = "不满足该时装的使用条件";
            return false;
        }

        state->selectedFashionBySlot[slot] = fashionId;
        SavePlayerScheme(player->GetGUID().GetCounter());
        RefreshPlayerFashionEffects(player);
        RefreshPlayerVisibleSlot(player, slot);
        return true;
    }

    void ClearPlayerSelections(Player* player)
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

        state->selectedFashionBySlot.clear();
        SavePlayerScheme(player->GetGUID().GetCounter());
        RefreshPlayerFashionEffects(player);
        RefreshAllVisibleSlots(player);
    }

private:
    std::vector<FashionEntry> _entries;
    std::unordered_map<uint32, FashionEntry> _entriesById;
    std::unordered_map<uint32, std::vector<uint32>> _entryIdsByItemId;
    std::unordered_map<uint32, PlayerFashionState> _playerStates;
};

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

void SendFashionSystemOpenUI(Player* player)
{
    SendFashionSystemPayload(player, "FS_OPEN");
}

void SendFashionCatalogToPlayer(Player* player)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "FS_LIST:";

    bool first = true;
    for (FashionEntry const& entry : FashionSystemMgr::Instance()->GetEntries())
    {
        if (!first)
            payload << '~';
        first = false;

        payload << entry.id << '^'
                << static_cast<uint32>(entry.fashionSlot) << '^'
                << entry.itemId << '^'
                << entry.setId << '^'
                << entry.requirementTemplateId << '^'
                << entry.quality << '^'
                << entry.groupId << '^'
                << SanitizeAddonText(entry.itemName);
    }

    SendFashionSystemPayload(player, payload.str());
}

void SendFashionStateToPlayer(Player* player)
{
    if (!player)
        return;

    PlayerFashionState const* state = FashionSystemMgr::Instance()->GetPlayerState(player->GetGUID().GetCounter());
    if (!state)
        return;

    std::unordered_map<uint32, uint8> setCounts;
    for (uint8 slot = FASHION_SLOT_HANDS; slot <= FASHION_SLOT_WRISTS; ++slot)
    {
        FashionEntry const* entry = FashionSystemMgr::Instance()->GetSelectedFashionEntry(player, slot, true);
        if (entry && entry->setId > 0)
            ++setCounts[entry->setId];
    }

    std::ostringstream payload;
    payload << "FS_STATE:" << (state->enabled ? 1 : 0) << '|';

    bool first = true;
    for (uint8 slot = FASHION_SLOT_HANDS; slot <= FASHION_SLOT_WRISTS; ++slot)
    {
        if (!first)
            payload << '~';
        first = false;

        FashionEntry const* entry = FashionSystemMgr::Instance()->GetSelectedFashionEntry(player, slot, false);
        payload << static_cast<uint32>(slot) << '^' << (entry ? entry->id : 0);
    }

    payload << '|';
    first = true;
    for (FashionEntry const& entry : FashionSystemMgr::Instance()->GetEntries())
    {
        if (!first)
            payload << '~';
        first = false;

        bool unlocked = state->unlockedFashionIds.find(entry.id) != state->unlockedFashionIds.end();
        bool eligible = unlocked && FashionSystemMgr::Instance()->MeetsRequirements(player, entry, false);
        bool selected = false;

        auto itr = state->selectedFashionBySlot.find(entry.fashionSlot);
        if (itr != state->selectedFashionBySlot.end() && itr->second == entry.id)
            selected = true;

        payload << entry.id << '^'
                << (unlocked ? 1 : 0) << '^'
                << (eligible ? 1 : 0) << '^'
                << (selected ? 1 : 0);
    }

    payload << '|';
    first = true;
    for (auto const& [setId, count] : setCounts)
    {
        if (!first)
            payload << '~';
        first = false;
        payload << setId << '^' << static_cast<uint32>(count);
    }

    SendFashionSystemPayload(player, payload.str());
}

void SendFashionAllDataToPlayer(Player* player)
{
    if (!player)
        return;

    if (!FashionSystemMgr::Instance()->GetPlayerState(player->GetGUID().GetCounter()))
    {
        FashionSystemMgr::Instance()->LoadPlayerData(player);
        FashionSystemMgr::Instance()->CollectPlayerUnlocks(player);
        FashionSystemMgr::Instance()->RefreshPlayerFashionEffects(player);
        FashionSystemMgr::Instance()->RefreshAllVisibleSlots(player);
    }
    SendFashionCatalogToPlayer(player);
    SendFashionStateToPlayer(player);
}

void SendFashionActionResult(Player* player, char const* action, bool success, uint32 slot, uint32 fashionId, std::string const& message)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "FS_RESULT:"
            << (action ? action : "") << '^'
            << (success ? "OK" : "FAIL") << '^'
            << slot << '^'
            << fashionId << '^'
            << SanitizeAddonText(message);
    SendFashionSystemPayload(player, payload.str());
}

class FashionSystemWorldScript : public WorldScript
{
public:
    FashionSystemWorldScript() : WorldScript("FashionSystemWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload && FashionSystemMgr::Instance()->IsEnabled())
            FashionSystemMgr::Instance()->LoadEntries();
    }

    void OnStartup() override
    {
        if (!FashionSystemMgr::Instance()->IsEnabled())
        {
            LOG_INFO("module", "时装系统已禁用。");
            return;
        }

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!FashionSystemMgr::Instance()->IsEnabled() || _initialized)
            return;

        _loadTime += diff;

        if (_loadTime >= 3000 && !_displayedLog)
        {
            LOG_INFO("server.loading", "→时装系统√");
            _displayedLog = true;
        }

        if (_loadTime >= 3000 && _displayedLog)
        {
            FashionSystemMgr::Instance()->LoadEntries();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class FashionSystemPlayerScript : public PlayerScript
{
public:
    FashionSystemPlayerScript() : PlayerScript("FashionSystemPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !FashionSystemMgr::Instance()->IsEnabled())
            return;

        FashionSystemMgr::Instance()->LoadPlayerData(player);
        FashionSystemMgr::Instance()->CollectPlayerUnlocks(player);
        FashionSystemMgr::Instance()->RefreshPlayerFashionEffects(player);
        FashionSystemMgr::Instance()->RefreshAllVisibleSlots(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            FashionSystemMgr::Instance()->UnloadPlayerData(player);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        FashionSystemMgr::Instance()->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* /*item*/) override
    {
        if (!player || !FashionSystemMgr::Instance()->IsEnabled())
            return;

        uint8 fashionSlot = GetFashionSlotByVisibleSlot(slot);
        if (fashionSlot)
            FashionSystemMgr::Instance()->RefreshPlayerVisibleSlot(player, fashionSlot);
    }

    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (!player || !item || !FashionSystemMgr::Instance()->IsEnabled())
            return;

        uint32 unlockedCount = 0;
        if (FashionSystemMgr::Instance()->UnlockFromItem(player, item, &unlockedCount) && unlockedCount > 0)
        {
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("时装系统：你新解锁了 {} 个时装外观。", unlockedCount);
            SendFashionStateToPlayer(player);
        }
    }

    void OnPlayerCreateItem(Player* player, Item* item, uint32 count) override
    {
        OnPlayerStoreNewItem(player, item, count);
    }

    void OnPlayerQuestRewardItem(Player* player, Item* item, uint32 count) override
    {
        OnPlayerStoreNewItem(player, item, count);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !FashionSystemMgr::Instance()->IsEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos || msg.substr(0, tabPos) != FASHION_SYSTEM_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendFashionAllDataToPlayer(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            SendFashionStateToPlayer(player);
            return;
        }

        if (command == "OPEN")
        {
            SendFashionSystemOpenUI(player);
            return;
        }

        if (command == "CLEAR_ALL")
        {
            FashionSystemMgr::Instance()->ClearPlayerSelections(player);
            SendFashionActionResult(player, "CLEAR_ALL", true, 0, 0, "已清空当前时装搭配");
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
            uint32 fashionId = 0;
            try
            {
                slot = static_cast<uint32>(std::stoul(payload.substr(0, sepPos)));
                fashionId = static_cast<uint32>(std::stoul(payload.substr(sepPos + 1)));
            }
            catch (...)
            {
                SendFashionActionResult(player, "SET", false, 0, 0, "参数错误");
                return;
            }

            std::string failureMessage;
            bool success = FashionSystemMgr::Instance()->SetPlayerFashion(player, static_cast<uint8>(slot), fashionId, &failureMessage);
            SendFashionActionResult(player, "SET", success, slot, fashionId, success ? (fashionId == 0 ? "已清除该位置时装" : "时装已切换") : (failureMessage.empty() ? "切换失败" : failureMessage));
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
            Acore::ChatCommands::ChatCommandBuilder("重载", HandleReloadCommand, SEC_ADMINISTRATOR, Acore::ChatCommands::Console::Yes),
            Acore::ChatCommands::ChatCommandBuilder("界面", HandleOpenUICommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("清空", HandleClearCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("启用", HandleEnableCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("关闭", HandleDisableCommand, SEC_PLAYER, Acore::ChatCommands::Console::No)
        };

        static Acore::ChatCommands::ChatCommandTable rootTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("时装", subTable)
        };

        return rootTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        FashionSystemMgr::Instance()->LoadEntries();
        handler->SendSysMessage("时装系统配置已重新加载。");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        SendFashionSystemOpenUI(player);
        handler->SendSysMessage("已向客户端发送时装系统界面打开请求。");
        return true;
    }

    static bool HandleClearCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        FashionSystemMgr::Instance()->ClearPlayerSelections(player);
        handler->SendSysMessage("时装系统：当前搭配已清空。");
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

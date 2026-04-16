#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Item.h"

#if __has_include("RequirementSystem.h")
    #ifndef MODULE_REQUIREMENT_TEMPLATE
        #define MODULE_REQUIREMENT_TEMPLATE
    #endif
    #include "RequirementSystem.h"
#endif

#if __has_include("ItemSkillsManager.h")
    #ifndef MODULE_ITEM_SKILLS
        #define MODULE_ITEM_SKILLS
    #endif
    #include "ItemSkillsManager.h"
    #include "ItemSkillsEffects.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <sstream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace Acore::ChatCommands;

bool TujianSystem_Enable = true;
bool TujianSystem_Announce = true;

namespace
{
    constexpr uint8 VIRTUAL_TUJIAN_SLOT = EQUIPMENT_SLOT_BODY;
    constexpr char TUJIAN_SYSTEM_ADDON_PREFIX[] = "ZDYUI_TJ";
    constexpr size_t MAX_ADDON_PAYLOAD = 200;
    constexpr uint8 TUJIAN_ATTR_MODE_FIXED = 0;
    constexpr uint8 TUJIAN_ATTR_MODE_EQUIP = 1;

    struct TuJianEntry
    {
        uint32 id = 0;
        std::string comment;
        std::string itemName;
        std::string menu1Name;
        std::string menu1Icon;
        std::string menu2Name1;
        std::string menu2Name2;
        std::string menu2Icon;
        uint32 page = 0;
        uint32 level = 0;
        uint32 maxLevel = 0;
        uint32 itemEntry = 0;
        uint32 setId = 0;
        uint32 activationRequirement = 0;
        std::string activationCommand;
        uint8 attributeEffectMode = TUJIAN_ATTR_MODE_EQUIP;
        int32 fixedAllStatsValue = 0;
    };

    struct TuJianSetEntry
    {
        uint32 id = 0;
        uint32 group = 0;
        uint32 requiredActivationCount = 0;
        std::string comment;
        std::string activationDesc;
        std::string activationTemplates;
        std::string activationCommand;
    };

    struct PlayerActivationRecord
    {
        uint32 tuJianId = 0;
        uint32 setId = 0;
        uint32 currentLevel = 0;
        uint32 currentItemEntry = 0;
    };

    struct TuJianMenuEntry
    {
        uint32 chapterId = 0;
        std::string menu1Name;
        std::string menu2Name1;
        std::string menu2Name2;
        uint32 itemCount = 0;
    };

    struct VirtualAppliedItem
    {
        std::unique_ptr<Item> item;
        uint8 slot = VIRTUAL_TUJIAN_SLOT;
        uint32 tuJianId = 0;
        uint32 setId = 0;
        bool allowEquipSpell = false;
        bool applyItemMods = false;
        bool applyEquipSpell = false;
        bool hasExternalSkills = false;
    };

    struct DirectAppliedItem
    {
        uint32 itemEntry = 0;
        uint32 applyCount = 0;
        bool applyItemSet = false;
    };

    struct PlayerWeaponDamageBonusCache
    {
        float minDamage[MAX_ATTACK][MAX_ITEM_PROTO_DAMAGES] = {};
        float maxDamage[MAX_ATTACK][MAX_ITEM_PROTO_DAMAGES] = {};
    };

    std::unordered_map<uint32, TuJianEntry> tuJianEntries;
    std::unordered_map<uint32, TuJianSetEntry> tuJianSetEntries;
    std::vector<TuJianMenuEntry> tuJianMenuEntries;
    std::unordered_map<uint32, std::vector<uint32>> tuJianItemEntriesByChapter;
    std::unordered_map<uint32, std::set<uint32>> tuJianSetGroupsByChapter;
    std::unordered_map<uint32, std::vector<PlayerActivationRecord>> playerActivationCache;
    std::unordered_map<uint32, std::vector<VirtualAppliedItem>> playerVirtualItems;
    std::unordered_map<uint32, std::vector<DirectAppliedItem>> playerDirectAppliedItems;
    std::unordered_map<uint32, PlayerWeaponDamageBonusCache> playerWeaponDamageBonuses;
    std::unordered_map<uint32, int32> playerFixedAllStatsBonus;
    std::unordered_set<uint32> blockedVirtualEquipSpellItemGuids;

    uint8 GetAttackSlotForVirtualItem(ItemTemplate const* proto);

    std::string SanitizeAddonText(std::string text)
    {
        for (char& ch : text)
        {
            if (ch == '^' || ch == '~' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
                ch = ' ';
        }

        return text;
    }

    std::vector<uint32> ParseUint32List(std::string const& value)
    {
        std::vector<uint32> result;
        std::unordered_set<uint32> seenValues;
        std::istringstream iss(value);
        std::string token;

        while (std::getline(iss, token, ','))
        {
            token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch)
            {
                return std::isspace(ch) != 0;
            }), token.end());

            if (token.empty())
                continue;

            try
            {
                uint32 parsedValue = static_cast<uint32>(std::stoul(token));
                if (parsedValue != 0 && seenValues.insert(parsedValue).second)
                    result.push_back(parsedValue);
            }
            catch (...)
            {
            }
        }

        return result;
    }

    std::string FormatActivationTemplatesForClient(std::string const& value)
    {
        std::vector<uint32> templateIds = ParseUint32List(value);
        if (templateIds.empty())
            return "";

        std::ostringstream displayText;
        bool first = true;

        for (uint32 templateId : templateIds)
        {
            std::string part = std::to_string(templateId);

#ifdef MODULE_ITEM_SKILLS
            if (sItemSkillsManager)
            {
                if (ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(templateId))
                {
                    if (!skillTemplate->clientDisplay.empty())
                        part = skillTemplate->clientDisplay;
                }
            }
#endif

            if (!first)
                displayText << ", ";
            first = false;

            displayText << SanitizeAddonText(part);
        }

        return displayText.str();
    }

    uint32 GetTuJianApplyCount(TuJianEntry const& entry, uint32 currentLevel)
    {
        if (entry.attributeEffectMode == TUJIAN_ATTR_MODE_FIXED)
            return 0;

        uint32 applyCount = std::max<uint32>(1, currentLevel);
        if (entry.maxLevel > 0)
            applyCount = std::min(applyCount, entry.maxLevel);

        return applyCount;
    }

    void ApplyFixedAllStatsBonus(Player* player, int32 amount, bool apply)
    {
        if (!player || amount == 0)
            return;

        for (uint8 stat = STAT_STRENGTH; stat < MAX_STATS; ++stat)
        {
            player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + stat), BASE_VALUE, float(amount), apply);
            player->ApplyStatBuffMod(Stats(stat), float(amount), apply);
        }
    }

    bool ItemTemplateHasEquipSpell(ItemTemplate const* proto)
    {
        if (!proto)
            return false;

        for (uint8 spellIndex = 0; spellIndex < MAX_ITEM_SPELLS; ++spellIndex)
        {
            if (proto->Spells[spellIndex].SpellId != 0 &&
                proto->Spells[spellIndex].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
                return true;
        }

        return false;
    }

    void AddWeaponDamageBonus(PlayerWeaponDamageBonusCache& cache, ItemTemplate const* proto, uint32 applyCount)
    {
        if (!proto || applyCount == 0)
            return;

        uint8 attackType = GetAttackSlotForVirtualItem(proto);
        if (attackType == MAX_ATTACK)
            return;

        for (uint8 damageIndex = 0; damageIndex < MAX_ITEM_PROTO_DAMAGES; ++damageIndex)
        {
            cache.minDamage[attackType][damageIndex] += proto->Damage[damageIndex].DamageMin * applyCount;
            cache.maxDamage[attackType][damageIndex] += proto->Damage[damageIndex].DamageMax * applyCount;
        }
    }

    uint32 GetChapterIdFromPageValue(uint32 page)
    {
        if (page >= 10)
            return page / 10;

        return page;
    }

    uint32 GetChapterIdForEntry(TuJianEntry const& entry)
    {
        return GetChapterIdFromPageValue(entry.page);
    }

    uint32 GetChapterIdForItemEntry(uint32 itemEntry)
    {
        auto itr = tuJianEntries.find(itemEntry);
        if (itr == tuJianEntries.end())
            return 0;

        return GetChapterIdForEntry(itr->second);
    }

    void RebuildTuJianIndexes()
    {
        tuJianMenuEntries.clear();
        tuJianItemEntriesByChapter.clear();
        tuJianSetGroupsByChapter.clear();

        std::unordered_map<uint32, TuJianMenuEntry> menuEntryByChapter;

        for (auto const& pair : tuJianEntries)
        {
            TuJianEntry const& entry = pair.second;
            uint32 chapterId = GetChapterIdForEntry(entry);
            if (chapterId == 0)
                continue;

            auto& menuEntry = menuEntryByChapter[chapterId];
            if (menuEntry.chapterId == 0)
            {
                menuEntry.chapterId = chapterId;
                menuEntry.menu1Name = entry.menu1Name;
                menuEntry.menu2Name1 = entry.menu2Name1;
                menuEntry.menu2Name2 = entry.menu2Name2;
            }

            ++menuEntry.itemCount;
            tuJianItemEntriesByChapter[chapterId].push_back(entry.itemEntry);

            if (entry.setId != 0)
                tuJianSetGroupsByChapter[chapterId].insert(entry.setId);
        }

        for (auto& pair : menuEntryByChapter)
            tuJianMenuEntries.push_back(pair.second);

        std::sort(tuJianMenuEntries.begin(), tuJianMenuEntries.end(), [](TuJianMenuEntry const& left, TuJianMenuEntry const& right)
        {
            if (left.chapterId != right.chapterId)
                return left.chapterId < right.chapterId;
            if (left.menu1Name != right.menu1Name)
                return left.menu1Name < right.menu1Name;
            return left.menu2Name2 < right.menu2Name2;
        });

        for (auto& pair : tuJianItemEntriesByChapter)
        {
            std::vector<uint32>& itemEntries = pair.second;
            std::sort(itemEntries.begin(), itemEntries.end(), [](uint32 leftItemEntry, uint32 rightItemEntry)
            {
                auto leftItr = tuJianEntries.find(leftItemEntry);
                auto rightItr = tuJianEntries.find(rightItemEntry);

                if (leftItr == tuJianEntries.end() || rightItr == tuJianEntries.end())
                    return leftItemEntry < rightItemEntry;

                TuJianEntry const& left = leftItr->second;
                TuJianEntry const& right = rightItr->second;

                if (left.page != right.page)
                    return left.page < right.page;
                if (left.id != right.id)
                    return left.id < right.id;
                return left.itemEntry < right.itemEntry;
            });
        }
    }

    uint32 ResolveSetGroupId(uint32 assignedSetId)
    {
        if (assignedSetId == 0)
            return 0;

        auto setItr = tuJianSetEntries.find(assignedSetId);
        if (setItr != tuJianSetEntries.end() && setItr->second.group != 0)
            return setItr->second.group;

        return assignedSetId;
    }

    std::set<uint32> GetTuJianIdsForGroup(uint32 groupId)
    {
        std::set<uint32> tuJianIds;
        if (groupId == 0)
            return tuJianIds;

        for (auto const& pair : tuJianEntries)
        {
            TuJianEntry const& entry = pair.second;
            if (ResolveSetGroupId(entry.setId) == groupId && entry.id != 0)
                tuJianIds.insert(entry.id);
        }

        return tuJianIds;
    }

    std::set<uint32> GetActivatedTuJianIdsForGroup(uint32 playerGuid, uint32 groupId)
    {
        std::set<uint32> activatedIds;
        if (groupId == 0)
            return activatedIds;

        auto itr = playerActivationCache.find(playerGuid);
        if (itr == playerActivationCache.end())
            return activatedIds;

        for (PlayerActivationRecord const& record : itr->second)
        {
            if (ResolveSetGroupId(record.setId) == groupId && record.tuJianId != 0 && record.currentLevel > 0)
                activatedIds.insert(record.tuJianId);
        }

        return activatedIds;
    }

    uint32 GetRequiredActivationCountForSetEntry(TuJianSetEntry const& setEntry)
    {
        uint32 groupId = setEntry.group;
        if (groupId == 0)
            return 0;

        std::set<uint32> groupIds = GetTuJianIdsForGroup(groupId);
        uint32 totalCount = static_cast<uint32>(groupIds.size());
        if (totalCount == 0)
            return 0;

        if (setEntry.requiredActivationCount == 0)
            return totalCount;

        return std::min(setEntry.requiredActivationCount, totalCount);
    }

    bool IsSetTierActivated(uint32 playerGuid, TuJianSetEntry const& setEntry)
    {
        uint32 requiredCount = GetRequiredActivationCountForSetEntry(setEntry);
        if (requiredCount == 0)
            return false;

        std::set<uint32> activatedIds = GetActivatedTuJianIdsForGroup(playerGuid, setEntry.group);
        return activatedIds.size() >= requiredCount;
    }

    uint32 GetCarrierItemEntryForGroup(uint32 playerGuid, uint32 groupId)
    {
        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr != playerActivationCache.end())
        {
            for (PlayerActivationRecord const& record : activationItr->second)
            {
                if (ResolveSetGroupId(record.setId) == groupId && record.currentItemEntry != 0)
                    return record.currentItemEntry;
            }
        }

        for (auto const& pair : tuJianEntries)
        {
            if (ResolveSetGroupId(pair.second.setId) == groupId && pair.second.itemEntry != 0)
                return pair.second.itemEntry;
        }

        return 0;
    }

    void SendAddonPayload(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        if (payload.length() <= MAX_ADDON_PAYLOAD)
        {
            std::string fullMessage = std::string(TUJIAN_SYSTEM_ADDON_PREFIX) + '\t' + payload;
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
            std::string chunk = payload.substr(start, len);

            std::ostringstream chunkMessage;
            chunkMessage << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << chunk;

            std::string fullMessage = std::string(TUJIAN_SYSTEM_ADDON_PREFIX) + '\t' + chunkMessage.str();
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
        }
    }

    void SendTuJianOpenUI(Player* player)
    {
        SendAddonPayload(player, "TJ_OPEN");
    }

    void SendTuJianMenuIndexToPlayer(Player* player)
    {
        std::ostringstream payload;
        payload << "TJ_INDEX:";

        bool first = true;
        for (TuJianMenuEntry const& entry : tuJianMenuEntries)
        {
            if (entry.chapterId == 0)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry.chapterId << '^'
                    << SanitizeAddonText(entry.menu1Name) << '^'
                    << SanitizeAddonText(entry.menu2Name1) << '^'
                    << SanitizeAddonText(entry.menu2Name2) << '^'
                    << entry.itemCount;
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianListToPlayer(Player* player)
    {
        std::vector<TuJianEntry const*> entries;
        entries.reserve(tuJianEntries.size());
        for (auto const& pair : tuJianEntries)
            entries.push_back(&pair.second);

        std::sort(entries.begin(), entries.end(), [](TuJianEntry const* left, TuJianEntry const* right)
        {
            if (left->page != right->page)
                return left->page < right->page;
            if (left->menu1Name != right->menu1Name)
                return left->menu1Name < right->menu1Name;
            if (left->menu2Name1 != right->menu2Name1)
                return left->menu2Name1 < right->menu2Name1;
            if (left->id != right->id)
                return left->id < right->id;
            return left->itemEntry < right->itemEntry;
        });

        std::ostringstream payload;
        payload << "TJ_LIST:";

        bool first = true;
        for (TuJianEntry const* entry : entries)
        {
            if (!entry)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry->itemEntry << '^'
                    << entry->id << '^'
                    << entry->page << '^'
                    << entry->level << '^'
                    << entry->maxLevel << '^'
                    << entry->setId << '^'
                    << entry->activationRequirement << '^'
                    << SanitizeAddonText(entry->menu1Name) << '^'
                    << SanitizeAddonText(entry->menu2Name1) << '^'
                    << SanitizeAddonText(entry->menu2Name2) << '^'
                    << SanitizeAddonText(entry->comment) << '^'
                    << SanitizeAddonText(entry->itemName);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianPageToPlayer(Player* player, uint32 chapterId)
    {
        std::ostringstream payload;
        payload << "TJ_PAGE:" << chapterId << ':';

        auto itemItr = tuJianItemEntriesByChapter.find(chapterId);
        if (itemItr == tuJianItemEntriesByChapter.end())
        {
            SendAddonPayload(player, payload.str());
            return;
        }

        bool first = true;
        for (uint32 itemEntry : itemItr->second)
        {
            auto entryItr = tuJianEntries.find(itemEntry);
            if (entryItr == tuJianEntries.end())
                continue;

            TuJianEntry const& entry = entryItr->second;

            if (!first)
                payload << '~';
            first = false;

            payload << entry.itemEntry << '^'
                    << entry.id << '^'
                    << entry.page << '^'
                    << entry.level << '^'
                    << entry.maxLevel << '^'
                    << entry.setId << '^'
                    << entry.activationRequirement << '^'
                    << SanitizeAddonText(entry.menu1Name) << '^'
                    << SanitizeAddonText(entry.menu2Name1) << '^'
                    << SanitizeAddonText(entry.menu2Name2) << '^'
                    << SanitizeAddonText(entry.comment) << '^'
                    << SanitizeAddonText(entry.itemName);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianSetListToPlayer(Player* player)
    {
        std::vector<TuJianSetEntry const*> entries;
        entries.reserve(tuJianSetEntries.size());
        for (auto const& pair : tuJianSetEntries)
            entries.push_back(&pair.second);

        std::sort(entries.begin(), entries.end(), [](TuJianSetEntry const* left, TuJianSetEntry const* right)
        {
            if (left->group != right->group)
                return left->group < right->group;
            if (left->requiredActivationCount != right->requiredActivationCount)
            {
                if (left->requiredActivationCount == 0)
                    return false;
                if (right->requiredActivationCount == 0)
                    return true;
                return left->requiredActivationCount < right->requiredActivationCount;
            }
            return left->id < right->id;
        });

        std::ostringstream payload;
        payload << "TJ_SET:";

        bool first = true;
        for (TuJianSetEntry const* entry : entries)
        {
            if (!entry)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry->id << '^'
                    << entry->group << '^'
                    << entry->requiredActivationCount << '^'
                    << SanitizeAddonText(entry->activationDesc) << '^'
                    << FormatActivationTemplatesForClient(entry->activationTemplates) << '^'
                    << SanitizeAddonText(entry->comment);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianPageSetsToPlayer(Player* player, uint32 chapterId)
    {
        std::ostringstream payload;
        payload << "TJ_PAGESET:" << chapterId << ':';

        auto setGroupItr = tuJianSetGroupsByChapter.find(chapterId);
        if (setGroupItr == tuJianSetGroupsByChapter.end() || setGroupItr->second.empty())
        {
            SendAddonPayload(player, payload.str());
            return;
        }

        std::vector<TuJianSetEntry const*> entries;
        entries.reserve(tuJianSetEntries.size());
        for (auto const& pair : tuJianSetEntries)
        {
            if (setGroupItr->second.find(pair.second.group) != setGroupItr->second.end())
                entries.push_back(&pair.second);
        }

        std::sort(entries.begin(), entries.end(), [](TuJianSetEntry const* left, TuJianSetEntry const* right)
        {
            if (left->group != right->group)
                return left->group < right->group;
            if (left->requiredActivationCount != right->requiredActivationCount)
            {
                if (left->requiredActivationCount == 0)
                    return false;
                if (right->requiredActivationCount == 0)
                    return true;
                return left->requiredActivationCount < right->requiredActivationCount;
            }
            return left->id < right->id;
        });

        bool first = true;
        for (TuJianSetEntry const* entry : entries)
        {
            if (!entry)
                continue;

            if (!first)
                payload << '~';
            first = false;

            payload << entry->id << '^'
                    << entry->group << '^'
                    << entry->requiredActivationCount << '^'
                    << SanitizeAddonText(entry->activationDesc) << '^'
                    << FormatActivationTemplatesForClient(entry->activationTemplates) << '^'
                    << SanitizeAddonText(entry->comment);
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianStateToPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        std::vector<PlayerActivationRecord> records;

        auto itr = playerActivationCache.find(playerGuid);
        if (itr != playerActivationCache.end())
            records = itr->second;

        std::sort(records.begin(), records.end(), [](PlayerActivationRecord const& left, PlayerActivationRecord const& right)
        {
            if (left.currentItemEntry != right.currentItemEntry)
                return left.currentItemEntry < right.currentItemEntry;
            return left.tuJianId < right.tuJianId;
        });

        std::ostringstream payload;
        payload << "TJ_STATE:";

        bool first = true;
        for (PlayerActivationRecord const& record : records)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << record.currentItemEntry << '^'
                    << record.tuJianId << '^'
                    << record.setId << '^'
                    << record.currentLevel;
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianPageStateToPlayer(Player* player, uint32 chapterId)
    {
        if (!player)
            return;

        std::ostringstream payload;
        payload << "TJ_PAGESTATE:" << chapterId << ':';

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr == playerActivationCache.end())
        {
            SendAddonPayload(player, payload.str());
            return;
        }

        std::vector<PlayerActivationRecord> records;
        records.reserve(activationItr->second.size());

        for (PlayerActivationRecord const& record : activationItr->second)
        {
            if (record.currentItemEntry == 0 || record.currentLevel == 0)
                continue;

            if (GetChapterIdForItemEntry(record.currentItemEntry) == chapterId)
                records.push_back(record);
        }

        std::sort(records.begin(), records.end(), [](PlayerActivationRecord const& left, PlayerActivationRecord const& right)
        {
            if (left.currentItemEntry != right.currentItemEntry)
                return left.currentItemEntry < right.currentItemEntry;
            return left.tuJianId < right.tuJianId;
        });

        bool first = true;
        for (PlayerActivationRecord const& record : records)
        {
            if (!first)
                payload << '~';
            first = false;

            payload << record.currentItemEntry << '^'
                    << record.tuJianId << '^'
                    << record.setId << '^'
                    << record.currentLevel;
        }

        SendAddonPayload(player, payload.str());
    }

    void SendTuJianSummaryToPlayer(Player* player)
    {
        if (!player)
            return;

        uint32 activeCount = 0;
        uint32 totalLevel = 0;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr != playerActivationCache.end())
        {
            for (PlayerActivationRecord const& record : activationItr->second)
            {
                if (record.currentItemEntry == 0 || record.currentLevel == 0)
                    continue;

                ++activeCount;
                totalLevel += record.currentLevel;
            }
        }

        std::ostringstream payload;
        payload << "TJ_SUMMARY:"
                << tuJianEntries.size() << '^'
                << activeCount << '^'
                << totalLevel;
        SendAddonPayload(player, payload.str());
    }

    void SendTuJianBootstrapDataToPlayer(Player* player)
    {
        SendTuJianMenuIndexToPlayer(player);
        SendTuJianSummaryToPlayer(player);
    }

    void SendTuJianPageDataToPlayer(Player* player, uint32 chapterId)
    {
        SendTuJianPageToPlayer(player, chapterId);
        SendTuJianPageSetsToPlayer(player, chapterId);
        SendTuJianPageStateToPlayer(player, chapterId);
    }

    void SendTuJianAllDataToPlayer(Player* player)
    {
        SendTuJianBootstrapDataToPlayer(player);

        if (!tuJianMenuEntries.empty())
            SendTuJianPageDataToPlayer(player, tuJianMenuEntries.front().chapterId);
    }

    void SendTuJianActionResult(Player* player, std::string const& action, bool success, uint32 itemEntry, uint32 level, std::string const& message)
    {
        std::ostringstream payload;
        payload << "TJ_RESULT:"
                << SanitizeAddonText(action) << '^'
                << (success ? "OK" : "FAIL") << '^'
                << itemEntry << '^'
                << level << '^'
                << SanitizeAddonText(message);
        SendAddonPayload(player, payload.str());
    }

    uint8 GetAttackSlotForVirtualItem(ItemTemplate const* proto)
    {
        if (!proto)
            return MAX_ATTACK;

        switch (proto->InventoryType)
        {
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_2HWEAPON:
                return BASE_ATTACK;
            case INVTYPE_WEAPONOFFHAND:
                return OFF_ATTACK;
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
                return RANGED_ATTACK;
            default:
                return MAX_ATTACK;
        }
    }

    void GetVirtualWeaponDamageBonus(Player* player, uint8 attackType, uint8 damageIndex, float& minDamage, float& maxDamage)
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;

        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto itr = playerWeaponDamageBonuses.find(playerGuid);
        if (itr == playerWeaponDamageBonuses.end())
            return;

        if (attackType >= MAX_ATTACK || damageIndex >= MAX_ITEM_PROTO_DAMAGES)
            return;

        minDamage = itr->second.minDamage[attackType][damageIndex];
        maxDamage = itr->second.maxDamage[attackType][damageIndex];
    }

    void RefreshPlayerStats(Player* player)
    {
        if (!player)
            return;

        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();
    }

    uint32 LoadTuJianData()
    {
        tuJianEntries.clear();

        bool hasAttributeEffectModeColumn = WorldDatabase.Query(
            "SHOW COLUMNS FROM `_图鉴系统` LIKE '属性生效模式'") != nullptr;
        bool hasFixedAllStatsValueColumn = WorldDatabase.Query(
            "SHOW COLUMNS FROM `_图鉴系统` LIKE '固定全属性值'") != nullptr;

        if (!hasAttributeEffectModeColumn)
            LOG_INFO("server.loading", "→_图鉴系统：未检测到属性生效模式字段，当前默认按装备属性模式加载");
        if (!hasFixedAllStatsValueColumn)
            LOG_INFO("server.loading", "→_图鉴系统：未检测到固定全属性值字段，固定属性模式将不会生效");

        std::string query =
            "SELECT tj.`注释`, COALESCE(it.`name`, ''), tj.`id`, tj.`一级菜单名称`, tj.`一级菜单图标`, tj.`二级菜单名称1`, tj.`二级菜单名称2`, tj.`二级菜单图标`, "
            "tj.`第几页`, tj.`等级`, tj.`最大等级`, tj.`物品entry`, tj.`套装ID`, tj.`激活需求`, tj.`激活后执行GM命令`";
        if (hasAttributeEffectModeColumn)
            query += ", tj.`属性生效模式`";
        if (hasFixedAllStatsValueColumn)
            query += ", tj.`固定全属性值`";
        query += " FROM `_图鉴系统` tj LEFT JOIN `item_template` it ON it.`entry` = tj.`物品entry`";

        QueryResult result = WorldDatabase.Query(query);

        if (!result)
        {
            return 0;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            TuJianEntry entry;
            entry.comment = fields[0].Get<std::string>();
            entry.itemName = fields[1].Get<std::string>();
            entry.id = fields[2].Get<uint32>();
            entry.menu1Name = fields[3].Get<std::string>();
            entry.menu1Icon = fields[4].Get<std::string>();
            entry.menu2Name1 = fields[5].Get<std::string>();
            entry.menu2Name2 = fields[6].Get<std::string>();
            entry.menu2Icon = fields[7].Get<std::string>();
            entry.page = fields[8].Get<uint32>();
            entry.level = fields[9].Get<uint32>();
            entry.maxLevel = fields[10].Get<uint32>();
            entry.itemEntry = fields[11].Get<uint32>();
            entry.setId = fields[12].Get<uint32>();
            entry.activationRequirement = fields[13].Get<uint32>();
            entry.activationCommand = fields[14].Get<std::string>();

            uint8 nextFieldIndex = 15;
            entry.attributeEffectMode = hasAttributeEffectModeColumn ? fields[nextFieldIndex++].Get<uint8>() : TUJIAN_ATTR_MODE_EQUIP;
            entry.fixedAllStatsValue = hasFixedAllStatsValueColumn ? fields[nextFieldIndex++].Get<int32>() : 0;

            if (entry.attributeEffectMode != TUJIAN_ATTR_MODE_FIXED)
                entry.attributeEffectMode = TUJIAN_ATTR_MODE_EQUIP;
            if (entry.fixedAllStatsValue < 0)
                entry.fixedAllStatsValue = 0;

            tuJianEntries[entry.itemEntry] = entry;
            ++count;
        } while (result->NextRow());

        return count;
    }

    uint32 LoadTuJianSetData()
    {
        tuJianSetEntries.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT `注释`, `id`, `组`, `需要激活数量`, `激活描述`, `激活模版_物品技能_多个逗号隔开`, `激活后执行GM命令` "
            "FROM `_图鉴系统_套装`");

        if (!result)
        {
            return 0;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            TuJianSetEntry entry;
            entry.comment = fields[0].Get<std::string>();
            entry.id = fields[1].Get<uint32>();
            entry.group = fields[2].Get<uint32>();
            entry.requiredActivationCount = fields[3].Get<uint32>();
            entry.activationDesc = fields[4].Get<std::string>();
            entry.activationTemplates = fields[5].Get<std::string>();
            entry.activationCommand = fields[6].Get<std::string>();

            tuJianSetEntries[entry.id] = entry;
            ++count;
        } while (result->NextRow());

        return count;
    }

    void LoadAllTuJianData()
    {
        uint32 tuJianCount = LoadTuJianData();
        uint32 tuJianSetCount = LoadTuJianSetData();
        RebuildTuJianIndexes();

        if (tuJianCount > 0)
            LOG_INFO("server.loading", "→_图鉴系统：已加载{}条数据", tuJianCount);
        else
            LOG_INFO("server.loading", "→_图鉴系统：已加载，无数据");

        if (tuJianSetCount > 0)
            LOG_INFO("server.loading", "→_图鉴系统_套装：已加载{}条数据", tuJianSetCount);
        else
            LOG_INFO("server.loading", "→_图鉴系统_套装：已加载，无数据");
    }

    void ClearPlayerCache(uint32 playerGuid)
    {
        playerActivationCache.erase(playerGuid);
        playerDirectAppliedItems.erase(playerGuid);
        playerWeaponDamageBonuses.erase(playerGuid);
        playerFixedAllStatsBonus.erase(playerGuid);
    }

    void RemovePlayerVirtualItems(Player* player, bool refreshStats)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto itr = playerVirtualItems.find(playerGuid);
        if (itr != playerVirtualItems.end())
        {
            for (auto appliedItr = itr->second.rbegin(); appliedItr != itr->second.rend(); ++appliedItr)
            {
                if (!appliedItr->item)
                    continue;

#ifdef MODULE_ITEM_SKILLS
                if (appliedItr->hasExternalSkills)
                {
                    sItemSkillsEffects->RemoveItemSkillEffectsByGuid(player, appliedItr->item->GetGUID().GetCounter());
                    sItemSkillsManager->RemoveExternalItemSkills(playerGuid, appliedItr->item->GetGUID().GetCounter());
                }
#endif

                if (!appliedItr->allowEquipSpell)
                    blockedVirtualEquipSpellItemGuids.erase(appliedItr->item->GetGUID().GetCounter());

                if (appliedItr->applyEquipSpell)
                    player->ApplyItemEquipSpell(appliedItr->item.get(), false);

                if (appliedItr->applyItemMods)
                {
                    ItemTemplate const* proto = appliedItr->item->GetTemplate();
                    if (proto && proto->ItemSet)
                        RemoveItemsSetItem(player, proto);

                    player->_ApplyItemMods(appliedItr->item.get(), appliedItr->slot, false);
                }
            }

            playerVirtualItems.erase(itr);
        }

        auto directItr = playerDirectAppliedItems.find(playerGuid);
        if (directItr != playerDirectAppliedItems.end())
        {
            for (auto appliedItr = directItr->second.rbegin(); appliedItr != directItr->second.rend(); ++appliedItr)
            {
                if (appliedItr->itemEntry == 0 || appliedItr->applyCount == 0)
                    continue;

                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(appliedItr->itemEntry);
                if (!proto)
                    continue;

                for (uint32 i = 0; i < appliedItr->applyCount; ++i)
                    player->_ApplyItemBonuses(proto, VIRTUAL_TUJIAN_SLOT, false);

                if (appliedItr->applyItemSet && proto->ItemSet)
                {
                    for (uint32 i = 0; i < appliedItr->applyCount; ++i)
                        RemoveItemsSetItem(player, proto);
                }
            }

            playerDirectAppliedItems.erase(directItr);
        }

        playerWeaponDamageBonuses.erase(playerGuid);

        auto fixedBonusItr = playerFixedAllStatsBonus.find(playerGuid);
        if (fixedBonusItr != playerFixedAllStatsBonus.end())
        {
            ApplyFixedAllStatsBonus(player, fixedBonusItr->second, false);
            playerFixedAllStatsBonus.erase(fixedBonusItr);
        }

#ifdef MODULE_ITEM_SKILLS
        sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
#endif

        if (refreshStats)
            RefreshPlayerStats(player);
    }

    void LoadPlayerActivationData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        playerActivationCache.erase(playerGuid);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `图鉴ID`, `套装ID`, `当前等级`, `当前物品entry` FROM `_玩家激活图鉴` WHERE `玩家GUID` = {}",
            playerGuid);

        if (!result)
            return;

        auto& records = playerActivationCache[playerGuid];
        do
        {
            Field* fields = result->Fetch();

            PlayerActivationRecord record;
            record.tuJianId = fields[0].Get<uint32>();
            record.setId = fields[1].Get<uint32>();
            record.currentLevel = fields[2].Get<uint32>();
            record.currentItemEntry = fields[3].Get<uint32>();

            if (record.tuJianId != 0 && record.currentItemEntry != 0 && record.currentLevel != 0)
                records.push_back(record);
        } while (result->NextRow());
    }

    void ApplyPlayerActivationData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        RemovePlayerVirtualItems(player, false);

        auto activationItr = playerActivationCache.find(playerGuid);
        if (activationItr == playerActivationCache.end() || activationItr->second.empty())
        {
            RefreshPlayerStats(player);
            return;
        }

        auto& appliedItems = playerVirtualItems[playerGuid];
        std::vector<DirectAppliedItem> directAppliedItems;
        directAppliedItems.reserve(activationItr->second.size());
        PlayerWeaponDamageBonusCache weaponDamageBonuses;
        int32 totalFixedAllStatsBonus = 0;
        uint32 equipModeActivationCount = 0;

        for (PlayerActivationRecord const& record : activationItr->second)
        {
            auto tuJianItr = tuJianEntries.find(record.currentItemEntry);
            if (tuJianItr == tuJianEntries.end())
            {
                LOG_WARN("module", "mod-tujian-system: 玩家 {} 的图鉴物品 {} 未在 world 配置中找到。", player->GetName(), record.currentItemEntry);
                continue;
            }

            TuJianEntry const& tuJian = tuJianItr->second;
            if (tuJian.attributeEffectMode == TUJIAN_ATTR_MODE_FIXED)
            {
                totalFixedAllStatsBonus += tuJian.fixedAllStatsValue;
                continue;
            }

            ++equipModeActivationCount;
            uint32 applyCount = GetTuJianApplyCount(tuJian, record.currentLevel);
            if (applyCount == 0)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(record.currentItemEntry);
            if (!proto)
            {
                LOG_WARN("module", "mod-tujian-system: 玩家 {} 的图鉴物品 {} 缺少 item_template 配置，无法应用装备属性。", player->GetName(), record.currentItemEntry);
                continue;
            }

            for (uint32 i = 0; i < applyCount; ++i)
                player->_ApplyItemBonuses(proto, VIRTUAL_TUJIAN_SLOT, true);

            AddWeaponDamageBonus(weaponDamageBonuses, proto, applyCount);

            bool needItemSet = proto->ItemSet != 0;
            bool needEquipSpell = ItemTemplateHasEquipSpell(proto);

            DirectAppliedItem directApplied;
            directApplied.itemEntry = record.currentItemEntry;
            directApplied.applyCount = applyCount;
            directAppliedItems.push_back(directApplied);

            if (!needItemSet && !needEquipSpell)
                continue;

            std::unique_ptr<Item> tempItem(Item::CreateItem(record.currentItemEntry, 1, player, true, 0));
            if (!tempItem)
            {
                LOG_WARN("module", "mod-tujian-system: 玩家 {} 的图鉴物品 {} 创建辅助虚拟物品失败，原生套装/装备法术不会生效。", player->GetName(), record.currentItemEntry);
                continue;
            }

            tempItem->SetOwnerGUID(player->GetGUID());
            tempItem->SetSlot(VIRTUAL_TUJIAN_SLOT);

            if (needItemSet)
            {
                for (uint32 i = 0; i < applyCount; ++i)
                    AddItemsSetItem(player, tempItem.get());

                directAppliedItems.back().applyItemSet = true;
            }

            if (needEquipSpell)
            {
                player->ApplyItemEquipSpell(tempItem.get(), true);

                VirtualAppliedItem applied;
                applied.slot = VIRTUAL_TUJIAN_SLOT;
                applied.tuJianId = record.tuJianId;
                applied.setId = record.setId;
                applied.allowEquipSpell = true;
                applied.applyEquipSpell = true;
                applied.item = std::move(tempItem);
                appliedItems.push_back(std::move(applied));
            }
        }

        if (!directAppliedItems.empty())
            playerDirectAppliedItems[playerGuid] = std::move(directAppliedItems);
        if (equipModeActivationCount > 0)
            playerWeaponDamageBonuses[playerGuid] = weaponDamageBonuses;

        if (totalFixedAllStatsBonus > 0)
        {
            ApplyFixedAllStatsBonus(player, totalFixedAllStatsBonus, true);
            playerFixedAllStatsBonus[playerGuid] = totalFixedAllStatsBonus;
        }

        if (equipModeActivationCount > 500)
            LOG_WARN("module", "mod-tujian-system: 玩家 {} 当前启用了 {} 个装备属性模式图鉴，虽然已启用轻量化应用，但仍可能带来较高的属性刷新和战斗计算开销。", player->GetName(), equipModeActivationCount);

#ifdef MODULE_ITEM_SKILLS
        std::unordered_set<uint32> processedGroups;
        for (PlayerActivationRecord const& record : activationItr->second)
        {
            uint32 groupId = ResolveSetGroupId(record.setId);
            if (groupId == 0 || !processedGroups.insert(groupId).second)
                continue;

            uint32 carrierItemEntry = GetCarrierItemEntryForGroup(playerGuid, groupId);
            if (carrierItemEntry == 0)
            {
                LOG_WARN("module", "mod-tujian-system: 套装组 {} 未找到可用的图鉴物品作为技能载体。", groupId);
                continue;
            }

            for (auto const& pair : tuJianSetEntries)
            {
                TuJianSetEntry const& setEntry = pair.second;
                if (setEntry.group != groupId || !IsSetTierActivated(playerGuid, setEntry))
                    continue;

                std::vector<uint32> skillTemplateIds = ParseUint32List(setEntry.activationTemplates);
                if (skillTemplateIds.empty())
                    continue;

                std::unique_ptr<Item> tempItem(Item::CreateItem(carrierItemEntry, 1, player, true, 0));
                if (!tempItem)
                {
                    LOG_WARN("module", "mod-tujian-system: 套装组 {} 的档位 {} 创建虚拟技能载体物品失败。", groupId, setEntry.id);
                    continue;
                }

                tempItem->SetOwnerGUID(player->GetGUID());
                tempItem->SetSlot(VIRTUAL_TUJIAN_SLOT);

                sItemSkillsManager->SetExternalItemSkills(
                    playerGuid, tempItem->GetGUID().GetCounter(), carrierItemEntry, skillTemplateIds);
                sItemSkillsEffects->ApplyItemSkillEffects(player, tempItem.get());

                VirtualAppliedItem applied;
                applied.slot = VIRTUAL_TUJIAN_SLOT;
                applied.tuJianId = 0;
                applied.setId = groupId;
                applied.allowEquipSpell = true;
                applied.applyItemMods = false;
                applied.hasExternalSkills = true;
                applied.item = std::move(tempItem);
                appliedItems.push_back(std::move(applied));
            }
        }

        sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
#endif

        RefreshPlayerStats(player);
    }

    void UpsertPlayerActivationRecord(uint32 playerGuid, PlayerActivationRecord const& newRecord)
    {
        auto& records = playerActivationCache[playerGuid];
        auto itr = std::find_if(records.begin(), records.end(), [&newRecord](PlayerActivationRecord const& record)
        {
            return record.tuJianId == newRecord.tuJianId;
        });

        if (itr != records.end())
            *itr = newRecord;
        else
            records.push_back(newRecord);
    }

    bool CheckAndConsumeActivationRequirement(Player* player, TuJianEntry const& tuJian, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        if (tuJian.activationRequirement == 0)
            return true;

#ifdef MODULE_REQUIREMENT_TEMPLATE
        if (!sRequirementSystem)
        {
            LOG_ERROR("module", "自定义UI图鉴: 图鉴ID {} 配置了激活需求 {}，但需求系统未加载",
                tuJian.id, tuJian.activationRequirement);

            if (player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[图鉴]|r 需求模板系统未加载，无法激活当前图鉴");

            if (failureMessage)
                *failureMessage = "需求系统未加载";

            return false;
        }

        bool meetsRequirements = sRequirementSystem->CheckRequirements(player, tuJian.activationRequirement, false);
        if (!meetsRequirements)
        {
            if (player->GetSession())
            {
                ChatHandler handler(player->GetSession());
                handler.SendSysMessage("|cffffcc00[图鉴]|r 不满足激活条件");
                sRequirementSystem->CheckRequirements(player, tuJian.activationRequirement, true);
            }

            if (failureMessage)
                *failureMessage = "不满足激活需求";

            return false;
        }

        if (!sRequirementSystem->ConsumeRequirements(player, tuJian.activationRequirement))
        {
            if (player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[图鉴]|r 激活需求消耗失败");

            if (failureMessage)
                *failureMessage = "需求消耗失败";

            return false;
        }
#else
        LOG_ERROR("module", "自定义UI图鉴: 图鉴ID {} 配置了激活需求 {}，但需求系统模块未编译",
            tuJian.id, tuJian.activationRequirement);

        if (player->GetSession())
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[图鉴]|r 需求模板系统模块未编译，无法激活当前图鉴");

        if (failureMessage)
            *failureMessage = "需求系统未编译";

        return false;
#endif

        return true;
    }

    bool ActivateTuJianForPlayer(Player* player, uint32 itemEntry, uint32 level, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        auto itr = tuJianEntries.find(itemEntry);
        if (itr == tuJianEntries.end())
        {
            if (failureMessage)
                *failureMessage = "未找到图鉴配置";
            return false;
        }

        TuJianEntry const& tuJian = itr->second;
        uint32 applyLevel = std::max<uint32>(1, level == 0 ? 1 : level);
        if (tuJian.maxLevel > 0)
            applyLevel = std::min(applyLevel, tuJian.maxLevel);

        if (!CheckAndConsumeActivationRequirement(player, tuJian, failureMessage))
            return false;

        PlayerActivationRecord record;
        record.tuJianId = tuJian.id;
        record.setId = tuJian.setId;
        record.currentLevel = applyLevel;
        record.currentItemEntry = tuJian.itemEntry;

        uint32 playerGuid = player->GetGUID().GetCounter();
        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家激活图鉴` (`玩家GUID`, `图鉴ID`, `套装ID`, `当前等级`, `当前物品entry`) VALUES ({}, {}, {}, {}, {})",
            playerGuid, record.tuJianId, record.setId, record.currentLevel, record.currentItemEntry);

        UpsertPlayerActivationRecord(playerGuid, record);
        ApplyPlayerActivationData(player);
        return true;
    }

    bool DeactivateTuJianForPlayer(Player* player, uint32 itemEntry)
    {
        if (!player)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto cacheItr = playerActivationCache.find(playerGuid);

        uint32 tuJianId = 0;
        if (cacheItr != playerActivationCache.end())
        {
            auto recordItr = std::find_if(cacheItr->second.begin(), cacheItr->second.end(), [itemEntry](PlayerActivationRecord const& record)
            {
                return record.currentItemEntry == itemEntry;
            });

            if (recordItr != cacheItr->second.end())
            {
                tuJianId = recordItr->tuJianId;
                cacheItr->second.erase(recordItr);
            }
        }

        if (tuJianId == 0)
        {
            auto tuJianItr = tuJianEntries.find(itemEntry);
            if (tuJianItr == tuJianEntries.end())
                return false;

            tuJianId = tuJianItr->second.id;
        }

        CharacterDatabase.Execute(
            "DELETE FROM `_玩家激活图鉴` WHERE `玩家GUID` = {} AND `图鉴ID` = {}",
            playerGuid, tuJianId);

        if (cacheItr != playerActivationCache.end() && cacheItr->second.empty())
            playerActivationCache.erase(cacheItr);

        ApplyPlayerActivationData(player);
        return true;
    }
}

class TujianSystemWorldScript : public WorldScript
{
public:
    TujianSystemWorldScript() : WorldScript("TujianSystemWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        TujianSystem_Enable = sConfigMgr->GetOption<bool>("TujianSystem.Enable", true);
        TujianSystem_Announce = sConfigMgr->GetOption<bool>("TujianSystem.Announce", true);
    }

    void OnStartup() override
    {
        if (!TujianSystem_Enable)
            return;

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!TujianSystem_Enable || _initialized)
            return;

        _loadTime += diff;

        if (_loadTime >= 3000 && !_displayedLog)
        {
            if (TujianSystem_Enable && TujianSystem_Announce)
                LOG_INFO("server.loading", "→自定义UI图鉴系统√");

            _displayedLog = true;
        }

        if (_loadTime >= 3000 && _displayedLog)
        {
            LoadAllTuJianData();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class TujianSystemPlayerScript : public PlayerScript
{
public:
    TujianSystemPlayerScript() : PlayerScript("TujianSystemPlayerScript") { }

    bool OnPlayerCanApplyEquipSpell(Player* /*player*/, SpellInfo const* /*spellInfo*/, Item* item, bool apply, bool /*form_change*/) override
    {
        if (!apply || !item)
            return true;

        return blockedVirtualEquipSpellItemGuids.find(item->GetGUID().GetCounter()) == blockedVirtualEquipSpellItemGuids.end();
    }

    void OnPlayerApplyWeaponDamage(Player* player, uint8 slot, ItemTemplate const* /*proto*/, float& minDamage, float& maxDamage, uint8 damageIndex) override
    {
        if (!TujianSystem_Enable || !player)
            return;

        uint8 attackType = Player::GetAttackBySlot(slot);
        if (attackType == MAX_ATTACK)
            return;

        float bonusMinDamage = 0.0f;
        float bonusMaxDamage = 0.0f;
        GetVirtualWeaponDamageBonus(player, attackType, damageIndex, bonusMinDamage, bonusMaxDamage);

        minDamage += bonusMinDamage;
        maxDamage += bonusMaxDamage;
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!TujianSystem_Enable || !player)
            return;

        LoadPlayerActivationData(player);
        ApplyPlayerActivationData(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        RemovePlayerVirtualItems(player, true);
        ClearPlayerCache(player->GetGUID().GetCounter());
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        if (trans)
            trans->Append(Acore::StringFormat("DELETE FROM `_玩家激活图鉴` WHERE `玩家GUID` = {}", guid));
        else
            CharacterDatabase.Execute("DELETE FROM `_玩家激活图鉴` WHERE `玩家GUID` = {}", guid);

        playerActivationCache.erase(guid);
        playerVirtualItems.erase(guid);
        playerDirectAppliedItems.erase(guid);
        playerWeaponDamageBonuses.erase(guid);
        playerFixedAllStatsBonus.erase(guid);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!TujianSystem_Enable || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != TUJIAN_SYSTEM_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendTuJianAllDataToPlayer(player);
            return;
        }

        if (command == "REQ_BOOT")
        {
            SendTuJianBootstrapDataToPlayer(player);
            return;
        }

        if (command == "REQ_LIST" || command == "REQ_INDEX")
        {
            SendTuJianMenuIndexToPlayer(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            SendTuJianSummaryToPlayer(player);
            return;
        }

        if (command.rfind("REQ_PAGE:", 0) == 0)
        {
            uint32 chapterId = 0;

            try
            {
                chapterId = static_cast<uint32>(std::stoul(command.substr(std::strlen("REQ_PAGE:"))));
            }
            catch (...)
            {
                return;
            }

            if (chapterId == 0)
                return;

            SendTuJianPageDataToPlayer(player, chapterId);
            return;
        }

        if (command == "OPEN")
        {
            SendTuJianOpenUI(player);
            return;
        }

        if (command.rfind("ACT:", 0) == 0)
        {
            std::string args = command.substr(std::strlen("ACT:"));
            size_t sep = args.find(':');
            if (sep == std::string::npos)
            {
                SendTuJianActionResult(player, "ACTIVATE", false, 0, 0, "参数错误");
                return;
            }

            uint32 itemEntry = 0;
            uint32 level = 1;

            try
            {
                itemEntry = static_cast<uint32>(std::stoul(args.substr(0, sep)));
                level = static_cast<uint32>(std::stoul(args.substr(sep + 1)));
            }
            catch (...)
            {
                SendTuJianActionResult(player, "ACTIVATE", false, 0, 0, "参数错误");
                return;
            }

            auto itr = tuJianEntries.find(itemEntry);
            if (itr == tuJianEntries.end())
            {
                SendTuJianActionResult(player, "ACTIVATE", false, itemEntry, level, "未找到图鉴配置");
                return;
            }

            uint32 applyLevel = std::max<uint32>(1, level == 0 ? 1 : level);
            if (itr->second.maxLevel > 0)
                applyLevel = std::min(applyLevel, itr->second.maxLevel);

            std::string failureMessage;
            bool success = ActivateTuJianForPlayer(player, itemEntry, applyLevel, &failureMessage);
            SendTuJianActionResult(player, "ACTIVATE", success, itemEntry, applyLevel,
                success ? "激活成功" : (failureMessage.empty() ? "激活失败" : failureMessage));
            SendTuJianSummaryToPlayer(player);

            uint32 chapterId = GetChapterIdForItemEntry(itemEntry);
            if (chapterId != 0)
                SendTuJianPageStateToPlayer(player, chapterId);
            return;
        }

        if (command.rfind("DEL:", 0) == 0)
        {
            std::string args = command.substr(std::strlen("DEL:"));
            uint32 itemEntry = 0;

            try
            {
                itemEntry = static_cast<uint32>(std::stoul(args));
            }
            catch (...)
            {
                SendTuJianActionResult(player, "REMOVE", false, 0, 0, "参数错误");
                return;
            }

            bool success = DeactivateTuJianForPlayer(player, itemEntry);
            SendTuJianActionResult(player, "REMOVE", success, itemEntry, 0, success ? "移除成功" : "移除失败");
            SendTuJianSummaryToPlayer(player);

            uint32 chapterId = GetChapterIdForItemEntry(itemEntry);
            if (chapterId != 0)
                SendTuJianPageStateToPlayer(player, chapterId);
            return;
        }
    }
};

class TujianSystemCommandScript : public CommandScript
{
public:
    TujianSystemCommandScript() : CommandScript("TujianSystemCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subCommandTable =
        {
            { "帮助", HandleHelpCommand, SEC_GAMEMASTER, Console::No },
            { "查看", HandleListCommand, SEC_GAMEMASTER, Console::No },
            { "列表", HandleListCommand, SEC_GAMEMASTER, Console::No },
            { "激活", HandleActivateCommand, SEC_GAMEMASTER, Console::No },
            { "移除", HandleDeactivateCommand, SEC_GAMEMASTER, Console::No },
            { "取消", HandleDeactivateCommand, SEC_GAMEMASTER, Console::No },
            { "删除", HandleDeactivateCommand, SEC_GAMEMASTER, Console::No },
            { "界面", HandleOpenUICommand, SEC_PLAYER, Console::No },
            { "重载", HandleReloadCommand, SEC_GAMEMASTER, Console::No },
            { "重新加载", HandleReloadCommand, SEC_GAMEMASTER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "图鉴", subCommandTable }
        };

        return commandTable;
    }

    static bool HandleHelpCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SendSysMessage("图鉴命令列表：");
        handler->SendSysMessage(".图鉴 帮助");
        handler->SendSysMessage(".图鉴 列表");
        handler->SendSysMessage(".图鉴 激活 物品ID [等级]");
        handler->SendSysMessage(".图鉴 移除 物品ID");
        handler->SendSysMessage(".图鉴 界面");
        handler->SendSysMessage(".图鉴 重载");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!TujianSystem_Enable)
        {
            handler->SendSysMessage("图鉴系统当前未启用。");
            return true;
        }

        SendTuJianOpenUI(player);
        handler->SendSysMessage("已向客户端发送图鉴界面打开请求。");
        return true;
    }

    static bool HandleActivateCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!TujianSystem_Enable)
        {
            handler->SendSysMessage("图鉴系统当前未启用。");
            return true;
        }

        std::istringstream iss(args ? args : "");
        uint32 itemEntry = 0;
        uint32 level = 1;
        if (!(iss >> itemEntry))
        {
            handler->SendSysMessage("用法: .图鉴 激活 物品ID [等级]");
            return false;
        }

        if (!(iss >> level))
            level = 1;

        auto itr = tuJianEntries.find(itemEntry);
        uint32 applyLevel = level;
        if (itr != tuJianEntries.end())
        {
            applyLevel = std::max<uint32>(1, level == 0 ? 1 : level);
            if (itr->second.maxLevel > 0)
                applyLevel = std::min(applyLevel, itr->second.maxLevel);
        }

        std::string failureMessage;
        if (!ActivateTuJianForPlayer(player, itemEntry, level, &failureMessage))
        {
            handler->PSendSysMessage("激活失败：{}。", failureMessage.empty() ? "未找到图鉴配置" : failureMessage);
            return false;
        }

        handler->PSendSysMessage("已激活图鉴物品 {}，当前叠加层数 {}。", itemEntry, applyLevel);
        return true;
    }

    static bool HandleDeactivateCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::istringstream iss(args ? args : "");
        uint32 itemEntry = 0;
        if (!(iss >> itemEntry))
        {
            handler->SendSysMessage("用法: .图鉴 移除 物品ID");
            return false;
        }

        if (!DeactivateTuJianForPlayer(player, itemEntry))
        {
            handler->PSendSysMessage("移除失败，未找到已激活图鉴物品 entry={}。", itemEntry);
            return false;
        }

        handler->PSendSysMessage("已移除图鉴物品 {} 的激活效果。", itemEntry);
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto itr = playerActivationCache.find(playerGuid);
        if (itr == playerActivationCache.end() || itr->second.empty())
        {
            handler->SendSysMessage("当前没有已激活的图鉴记录。");
            return true;
        }

        handler->SendSysMessage("当前已激活图鉴：");
        for (PlayerActivationRecord const& record : itr->second)
            handler->PSendSysMessage("- 图鉴ID={} 物品Entry={} 套装ID={} 叠加层数={}", record.tuJianId, record.currentItemEntry, record.setId, record.currentLevel);

        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        LoadAllTuJianData();
        handler->SendSysMessage("图鉴 world 配置已重新加载。");
        return true;
    }
};

void AddSC_mod_tujian_system()
{
    new TujianSystemWorldScript();
    new TujianSystemPlayerScript();
    new TujianSystemCommandScript();
}

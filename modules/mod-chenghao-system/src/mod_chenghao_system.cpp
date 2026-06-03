/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#if __has_include("RequirementSystem.h")
#include "RequirementSystem.h"
#define CHENGHAO_SYSTEM_HAS_REQUIREMENT_SYSTEM 1
#else
#define CHENGHAO_SYSTEM_HAS_REQUIREMENT_SYSTEM 0
#endif

#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr char const* CONF_ENABLE = "ChenghaoSystem.Enable";
constexpr char const* CONF_NOTIFY_PLAYER = "ChenghaoSystem.NotifyPlayer";
constexpr char CHENGHAO_SYSTEM_ADDON_PREFIX[] = "ZDYUI_CH";
constexpr size_t MAX_ADDON_PAYLOAD = 220;

std::string SanitizeAddonText(std::string text)
{
    for (char& ch : text)
    {
        if (ch == '^' || ch == '~' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }

    return text;
}

struct ChenghaoSystemEntry
{
    uint32 id = 0;
    uint32 titleLevel = 0;
    uint32 requirementTemplateId = 0;
    uint64 attributeValue1 = 0;
    uint32 attributeValue2 = 0;
    std::string description;
};

struct ChenghaoSystemPlayerState
{
    uint32 titleLevel = 0;
    uint64 attributeValue1 = 0;
    uint32 attributeValue2 = 0;
};

uint64 ParseUnsigned64Value(std::string const& text)
{
    uint64 value = 0;
    bool hasDigit = false;
    bool saturated = false;
    uint64 const maxValue = std::numeric_limits<uint64>::max();

    for (char ch : text)
    {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch))
            continue;

        if (ch == '-')
            return 0;

        if (ch == '.')
            break;

        if (!std::isdigit(uch))
            break;

        hasDigit = true;
        uint8 digit = static_cast<uint8>(ch - '0');
        if (!saturated)
        {
            if (value > (maxValue - digit) / 10)
            {
                value = maxValue;
                saturated = true;
            }
            else
                value = value * 10 + digit;
        }
    }

    return hasDigit ? value : 0;
}

uint32 ParseUnsigned32Value(std::string const& text)
{
    uint64 value = ParseUnsigned64Value(text);
    return value > std::numeric_limits<uint32>::max() ? std::numeric_limits<uint32>::max() : static_cast<uint32>(value);
}

class ChenghaoSystemMgr
{
public:
    static ChenghaoSystemMgr* Instance()
    {
        static ChenghaoSystemMgr instance;
        return &instance;
    }

    bool IsEnabled() const
    {
        return sConfigMgr->GetOption<bool>(CONF_ENABLE, true);
    }

    bool ShouldNotifyPlayer() const
    {
        return sConfigMgr->GetOption<bool>(CONF_NOTIFY_PLAYER, true);
    }

    void Initialize()
    {
        _entries.clear();

        QueryResult result = WorldDatabase.Query("SELECT id, 称号等级, 需求系统id, 属性值1, 属性值2, 称号描述 FROM _称号系统");
        if (!result)
        {
            LOG_WARN("server.loading", "自定义UI称号系统未加载到任何数据，请检查表 _称号系统。");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            ChenghaoSystemEntry entry;
            entry.id = fields[0].Get<uint32>();
            entry.titleLevel = fields[1].Get<uint32>();
            entry.requirementTemplateId = fields[2].Get<uint32>();
            entry.attributeValue1 = ParseUnsigned64Value(fields[3].Get<std::string>());
            entry.attributeValue2 = ParseUnsigned32Value(fields[4].Get<std::string>());
            entry.description = fields[5].Get<std::string>();

            if (!entry.id || !entry.titleLevel)
            {
                LOG_ERROR("sql.sql", "自定义UI称号系统存在非法数据: id={}, 称号等级={}, 需求系统id={}, 属性值1={}, 属性值2={}",
                    entry.id, entry.titleLevel, entry.requirementTemplateId, entry.attributeValue1, entry.attributeValue2);
                continue;
            }

            _entries.push_back(entry);
        }
        while (result->NextRow());

        std::sort(_entries.begin(), _entries.end(), [](ChenghaoSystemEntry const& left, ChenghaoSystemEntry const& right)
        {
            if (left.titleLevel != right.titleLevel)
                return left.titleLevel < right.titleLevel;

            return left.id < right.id;
        });
    }

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        _playerTitles[playerGuid].clear();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `称号ID`, `称号等级`, `属性值1`, `属性值2` FROM `_玩家称号系统` WHERE `玩家GUID` = {} "
            "ORDER BY `称号等级` DESC, `称号ID` ASC LIMIT 1",
            playerGuid);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 titleId = fields[0].Get<uint32>();
            ChenghaoSystemPlayerState state;
            state.titleLevel = fields[1].Get<uint32>();
            state.attributeValue1 = ParseUnsigned64Value(fields[2].Get<std::string>());
            state.attributeValue2 = ParseUnsigned32Value(fields[3].Get<std::string>());

            if (!titleId || !state.titleLevel)
                continue;

            _playerTitles[playerGuid][titleId] = state;
        }
        while (result->NextRow());
    }

    std::vector<ChenghaoSystemEntry> const& GetEntries() const
    {
        return _entries;
    }

    std::vector<ChenghaoSystemEntry> GetCurrentAndNextEntries(Player* player) const
    {
        std::vector<ChenghaoSystemEntry> result;
        if (!player)
            return result;

        uint32 currentTitleId = GetHighestUnlockedTitleId(player);
        uint32 currentLevel = 0;

        if (ChenghaoSystemEntry const* currentEntry = currentTitleId ? GetEntryById(currentTitleId) : nullptr)
        {
            result.push_back(*currentEntry);
            currentLevel = currentEntry->titleLevel;
        }
        else if (ChenghaoSystemPlayerState const* highestState = GetHighestUnlockedState(player))
        {
            currentLevel = highestState->titleLevel;
        }

        uint32 const nextLevel = currentLevel + 1;
        for (ChenghaoSystemEntry const& entry : _entries)
        {
            if (entry.titleLevel < nextLevel)
                continue;

            if (entry.titleLevel > nextLevel)
                break;

            if (entry.id != currentTitleId)
                result.push_back(entry);

            break;
        }

        return result;
    }

    uint32 GetHighestUnlockedTitleId(Player* player) const
    {
        if (!player)
            return 0;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end() || playerIt->second.empty())
            return 0;

        uint32 bestTitleId = 0;
        ChenghaoSystemPlayerState const* bestState = nullptr;

        for (auto const& [titleId, state] : playerIt->second)
        {
            if (!bestState || state.titleLevel > bestState->titleLevel ||
                (state.titleLevel == bestState->titleLevel && titleId < bestTitleId))
            {
                bestState = &state;
                bestTitleId = titleId;
            }
        }

        return bestTitleId;
    }

    ChenghaoSystemEntry const* GetHighestUnlockedEntry(Player* player) const
    {
        uint32 titleId = GetHighestUnlockedTitleId(player);
        return titleId ? GetEntryById(titleId) : nullptr;
    }

    uint32 GetHighestUnlockedTitleLevel(Player* player) const
    {
        if (ChenghaoSystemPlayerState const* state = GetHighestUnlockedState(player))
            return state->titleLevel;

        return 0;
    }

    uint64 GetHealthBonus(Player* player) const
    {
        if (ChenghaoSystemPlayerState const* state = GetHighestUnlockedState(player))
            return state->attributeValue1;

        return 0;
    }

    uint32 GetDamageReductionPct(Player* player) const
    {
        if (ChenghaoSystemPlayerState const* state = GetHighestUnlockedState(player))
            return state->attributeValue2;

        return 0;
    }

    void RefreshPlayerBonuses(Player* player) const
    {
        if (!player)
            return;

        player->UpdateAllStats();
    }

    bool IsPlayerUnlocked(Player* player, uint32 titleId) const
    {
        if (!player)
            return false;

        ChenghaoSystemEntry const* entry = GetEntryById(titleId);
        if (!entry)
            return false;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end())
            return false;

        for (auto const& [storedTitleId, state] : playerIt->second)
        {
            (void)storedTitleId;
            if (state.titleLevel >= entry->titleLevel)
                return true;
        }

        return false;
    }

    bool HasUnlockedTitleLevel(Player* player, uint32 titleLevel) const
    {
        if (!player || !titleLevel)
            return false;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end())
            return false;

        for (auto const& [titleId, state] : playerIt->second)
        {
            (void)titleId;
            if (state.titleLevel >= titleLevel)
                return true;
        }

        return false;
    }

    bool CanActivateEntry(Player* player, ChenghaoSystemEntry const& entry, bool showMessages, bool consumeRequirements,
        std::string* failureMessage = nullptr) const
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        if (entry.titleLevel > 1 && !HasUnlockedTitleLevel(player, entry.titleLevel - 1))
        {
            if (showMessages)
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "称号系统：请先激活 {} 级称号后，再激活 {} 级称号。",
                    entry.titleLevel - 1,
                    entry.titleLevel);
            }

            if (failureMessage)
                *failureMessage = "请先激活上一等级称号";

            return false;
        }

        if (entry.requirementTemplateId == 0)
            return true;

#if CHENGHAO_SYSTEM_HAS_REQUIREMENT_SYSTEM
        if (!sRequirementSystem)
        {
            if (showMessages)
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[称号]|r 需求模板系统未加载，无法激活当前称号");

            if (failureMessage)
                *failureMessage = "需求系统未加载";

            return false;
        }

        bool meetsRequirements = sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, false);
        if (!meetsRequirements)
        {
            if (showMessages)
            {
                ChatHandler handler(player->GetSession());
                handler.SendSysMessage("|cffffcc00[称号]|r 不满足激活条件");
                sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, true);
            }

            if (failureMessage)
                *failureMessage = "不满足激活条件";

            return false;
        }

        if (consumeRequirements && !sRequirementSystem->ConsumeRequirements(player, entry.requirementTemplateId))
        {
            if (showMessages)
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[称号]|r 激活需求消耗失败");

            if (failureMessage)
                *failureMessage = "需求消耗失败";

            return false;
        }

        return true;
#else
        if (showMessages)
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[称号]|r 需求模板系统模块未编译，无法激活当前称号");

        if (failureMessage)
            *failureMessage = "需求系统未编译";

        return false;
#endif
    }

    bool UnlockTitle(Player* player, uint32 titleId, bool showMessages, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        ChenghaoSystemEntry const* entry = GetEntryById(titleId);
        if (!entry)
        {
            if (failureMessage)
                *failureMessage = "未找到称号配置";
            return false;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto& playerTitles = _playerTitles[playerGuid];
        if (IsPlayerUnlocked(player, titleId))
        {
            if (failureMessage)
                *failureMessage = "该称号已经激活";
            return false;
        }

        if (!CanActivateEntry(player, *entry, showMessages, true, failureMessage))
            return false;

        ChenghaoSystemPlayerState state;
        state.titleLevel = entry->titleLevel;
        state.attributeValue1 = entry->attributeValue1;
        state.attributeValue2 = entry->attributeValue2;

        CharacterDatabase.DirectExecute(
            "INSERT INTO `_玩家称号系统` (`玩家GUID`, `称号ID`, `称号等级`, `属性值1`, `属性值2`) "
            "VALUES ({}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `称号ID` = VALUES(`称号ID`), `称号等级` = VALUES(`称号等级`), "
            "`属性值1` = VALUES(`属性值1`), `属性值2` = VALUES(`属性值2`)",
            playerGuid,
            titleId,
            state.titleLevel,
            state.attributeValue1,
            state.attributeValue2);

        playerTitles.clear();
        playerTitles[titleId] = state;

        RefreshPlayerBonuses(player);

        if (showMessages && ShouldNotifyPlayer())
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "称号系统：已激活称号 [id:{}] 等级:{} 生命加成:{} 减伤:{}% 描述:{}",
                titleId,
                state.titleLevel,
                GetHealthBonus(player),
                GetDamageReductionPct(player),
                entry->description.empty() ? "无" : entry->description.c_str());
        }

        return true;
    }

    uint32 UnlockAllAvailableTitles(Player* player, bool showMessages)
    {
        if (!player)
            return 0;

        uint32 count = 0;
        for (ChenghaoSystemEntry const& entry : _entries)
        {
            if (IsPlayerUnlocked(player, entry.id))
                continue;

            if (!CanActivateEntry(player, entry, false, false))
                continue;

            if (UnlockTitle(player, entry.id, false))
                ++count;
        }

        if (showMessages)
        {
            ChatHandler handler(player->GetSession());
            handler.PSendSysMessage("称号系统：本次共激活 {} 个称号。", count);

            if (ChenghaoSystemEntry const* highestEntry = GetHighestUnlockedEntry(player))
            {
                handler.PSendSysMessage("称号系统：当前生效称号 [id:{}] 等级:{} 生命加成:{} 减伤:{}% 描述:{}",
                    highestEntry->id,
                    highestEntry->titleLevel,
                    GetHealthBonus(player),
                    GetDamageReductionPct(player),
                    highestEntry->description.empty() ? "无" : highestEntry->description.c_str());
            }
            else
            {
                handler.SendSysMessage("称号系统：当前没有任何已生效称号。");
            }
        }

        return count;
    }

    void UnloadPlayerData(uint32 playerGuid)
    {
        _playerTitles.erase(playerGuid);
    }

    void DeletePlayerData(uint32 playerGuid)
    {
        _playerTitles.erase(playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家称号系统` WHERE `玩家GUID` = {}", playerGuid);
    }

private:
    ChenghaoSystemPlayerState const* GetHighestUnlockedState(Player* player) const
    {
        if (!player)
            return nullptr;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end() || playerIt->second.empty())
            return nullptr;

        uint32 bestTitleId = GetHighestUnlockedTitleId(player);
        return bestTitleId ? &playerIt->second.at(bestTitleId) : nullptr;
    }

    ChenghaoSystemEntry const* GetEntryById(uint32 titleId) const
    {
        for (ChenghaoSystemEntry const& entry : _entries)
        {
            if (entry.id == titleId)
                return &entry;
        }

        return nullptr;
    }

    std::vector<ChenghaoSystemEntry> _entries;
    std::unordered_map<uint32, std::unordered_map<uint32, ChenghaoSystemPlayerState>> _playerTitles;
};

void SendAddonPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= MAX_ADDON_PAYLOAD)
    {
        std::string fullMessage = std::string(CHENGHAO_SYSTEM_ADDON_PREFIX) + '\t' + payload;
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

        std::string fullMessage = std::string(CHENGHAO_SYSTEM_ADDON_PREFIX) + '\t' + chunkMessage.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendChengHaoOpenUI(Player* player)
{
    SendAddonPayload(player, "CH_OPEN");
}

void SendChengHaoListToPlayer(Player* player)
{
    if (!player)
        return;

    std::vector<ChenghaoSystemEntry> entries = ChenghaoSystemMgr::Instance()->GetCurrentAndNextEntries(player);

    std::ostringstream payload;
    payload << "CH_LIST:";

    bool first = true;
    for (ChenghaoSystemEntry const& entry : entries)
    {
        if (!first)
            payload << '~';
        first = false;

        payload << entry.id << '^'
                << entry.titleLevel << '^'
                << entry.requirementTemplateId << '^'
                << entry.attributeValue1 << '^'
                << entry.attributeValue2 << '^'
                << SanitizeAddonText(entry.description);
    }

    SendAddonPayload(player, payload.str());
}

void SendChengHaoStateToPlayer(Player* player)
{
    if (!player)
        return;

    std::vector<ChenghaoSystemEntry> entries = ChenghaoSystemMgr::Instance()->GetCurrentAndNextEntries(player);
    uint32 currentTitleId = ChenghaoSystemMgr::Instance()->GetHighestUnlockedTitleId(player);

    std::ostringstream payload;
    payload << "CH_STATE:";

    bool first = true;
    for (ChenghaoSystemEntry const& entry : entries)
    {
        if (!first)
            payload << '~';
        first = false;

        bool unlocked = ChenghaoSystemMgr::Instance()->IsPlayerUnlocked(player, entry.id);
        bool eligible = false;
        if (!unlocked)
            eligible = ChenghaoSystemMgr::Instance()->CanActivateEntry(player, entry, false, false);

        payload << entry.id << '^'
                << (unlocked ? 1 : 0) << '^'
                << (currentTitleId == entry.id ? 1 : 0) << '^'
                << (eligible ? 1 : 0);
    }

    SendAddonPayload(player, payload.str());
}

void SendChengHaoAllDataToPlayer(Player* player)
{
    if (!player)
        return;

    ChenghaoSystemMgr::Instance()->LoadPlayerData(player);
    SendChengHaoListToPlayer(player);
    SendChengHaoStateToPlayer(player);
}

void SendChengHaoActionResult(Player* player, char const* action, bool success, uint32 titleId, std::string const& message)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "CH_RESULT:"
            << (action ? action : "") << '^'
            << (success ? "OK" : "FAIL") << '^'
            << titleId << '^'
            << SanitizeAddonText(message);

    SendAddonPayload(player, payload.str());
}

class ChenghaoSystemWorldScript : public WorldScript
{
public:
    ChenghaoSystemWorldScript() : WorldScript("ChenghaoSystemWorldScript") { }

    void OnStartup() override
    {
        if (!ChenghaoSystemMgr::Instance()->IsEnabled())
            return;

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!ChenghaoSystemMgr::Instance()->IsEnabled() || _initialized)
            return;

        _loadTime += diff;

        if (_loadTime >= 3000 && !_displayedLog)
        {
            LOG_INFO("server.loading", "→自定义UI称号系统√");
            _displayedLog = true;
        }

        if (_loadTime >= 3000 && _displayedLog)
        {
            ChenghaoSystemMgr::Instance()->Initialize();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class ChenghaoSystemPlayerScript : public PlayerScript
{
public:
    ChenghaoSystemPlayerScript() : PlayerScript("ChenghaoSystemPlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER,
        PLAYERHOOK_ON_AFTER_UPDATE_MAX_HEALTH
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        if (!ChenghaoSystemMgr::Instance()->IsEnabled())
        {
            ChenghaoSystemMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
            return;
        }

        ChenghaoSystemMgr::Instance()->LoadPlayerData(player);
        ChenghaoSystemMgr::Instance()->RefreshPlayerBonuses(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        ChenghaoSystemMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        ChenghaoSystemMgr::Instance()->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!ChenghaoSystemMgr::Instance()->IsEnabled() || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != CHENGHAO_SYSTEM_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendChengHaoAllDataToPlayer(player);
            return;
        }

        if (command == "REQ_LIST")
        {
            SendChengHaoListToPlayer(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            ChenghaoSystemMgr::Instance()->LoadPlayerData(player);
            SendChengHaoStateToPlayer(player);
            return;
        }

        if (command == "OPEN")
        {
            SendChengHaoOpenUI(player);
            return;
        }

        if (command == "ACT_ALL")
        {
            uint32 count = ChenghaoSystemMgr::Instance()->UnlockAllAvailableTitles(player, false);
            std::ostringstream message;
            message << "本次激活 " << count << " 个称号";
            SendChengHaoActionResult(player, "ACT_ALL", true, 0, message.str());
            SendChengHaoAllDataToPlayer(player);
            return;
        }

        if (command.rfind("ACT:", 0) == 0)
        {
            std::string args = command.substr(std::strlen("ACT:"));
            uint32 titleId = 0;

            try
            {
                titleId = static_cast<uint32>(std::stoul(args));
            }
            catch (...)
            {
                SendChengHaoActionResult(player, "ACTIVATE", false, 0, "参数错误");
                return;
            }

            if (!titleId)
            {
                SendChengHaoActionResult(player, "ACTIVATE", false, 0, "称号ID无效");
                return;
            }

            if (ChenghaoSystemMgr::Instance()->IsPlayerUnlocked(player, titleId))
            {
                SendChengHaoActionResult(player, "ACTIVATE", false, titleId, "该称号已经激活");
                SendChengHaoAllDataToPlayer(player);
                return;
            }

            std::string failureMessage;
            bool success = ChenghaoSystemMgr::Instance()->UnlockTitle(player, titleId, true, &failureMessage);
            SendChengHaoActionResult(player, "ACTIVATE", success, titleId, success ? "激活成功" : (failureMessage.empty() ? "激活失败" : failureMessage));
            SendChengHaoAllDataToPlayer(player);
            return;
        }
    }

    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        if (!player || !ChenghaoSystemMgr::Instance()->IsEnabled())
            return;

        uint64 const bonus = ChenghaoSystemMgr::Instance()->GetHealthBonus(player);
        if (bonus)
            value += static_cast<float>(bonus);
    }
};

class ChenghaoSystemUnitScript : public UnitScript
{
public:
    ChenghaoSystemUnitScript() : UnitScript("ChenghaoSystemUnitScript", true, { UNITHOOK_ON_DAMAGE }) { }

    void OnDamage(Unit* /*attacker*/, Unit* victim, uint128& damage) override
    {
        if (!victim || damage == 0 || !ChenghaoSystemMgr::Instance()->IsEnabled())
            return;

        Player* player = victim->ToPlayer();
        if (!player)
            return;

        uint32 const reductionPct = ChenghaoSystemMgr::Instance()->GetDamageReductionPct(player);
        if (!reductionPct)
            return;

        if (reductionPct >= 100)
        {
            damage = 0;
            return;
        }

        damage = damage * static_cast<uint128>(100 - reductionPct) / 100;
    }
};

class ChenghaoSystemCommandScript : public CommandScript
{
public:
    ChenghaoSystemCommandScript() : CommandScript("ChenghaoSystemCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable chenghaoCommandTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("重载", HandleReloadCommand, SEC_ADMINISTRATOR, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("界面", HandleOpenUICommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("激活", HandleUnlockCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("刷新", HandleRefreshCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("列表", HandleListCommand, SEC_PLAYER, Acore::ChatCommands::Console::No)
        };

        static Acore::ChatCommands::ChatCommandTable commandTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("称号", chenghaoCommandTable),
            Acore::ChatCommands::ChatCommandBuilder("称号圣册", chenghaoCommandTable)
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        ChenghaoSystemMgr::Instance()->Initialize();

        if (Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
        {
            ChenghaoSystemMgr::Instance()->LoadPlayerData(player);
            ChenghaoSystemMgr::Instance()->RefreshPlayerBonuses(player);
        }

        handler->SendSysMessage("称号系统数据已重新加载。");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        SendChengHaoOpenUI(player);
        return true;
    }

    static bool HandleUnlockCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::string argText = args ? args : "";
        if (argText.empty())
        {
            handler->SendSysMessage("语法: .称号 激活 <称号ID|全部>");
            return false;
        }

        if (argText == "全部" || argText == "all" || argText == "ALL")
        {
            ChenghaoSystemMgr::Instance()->UnlockAllAvailableTitles(player, true);
            return true;
        }

        uint32 titleId = static_cast<uint32>(std::strtoul(argText.c_str(), nullptr, 10));
        if (!titleId)
        {
            handler->SendSysMessage("称号系统：称号ID无效。");
            return false;
        }

        if (ChenghaoSystemMgr::Instance()->IsPlayerUnlocked(player, titleId))
        {
            handler->SendSysMessage("称号系统：该称号已经激活过了。");
            return true;
        }

        std::string failureMessage;
        if (!ChenghaoSystemMgr::Instance()->UnlockTitle(player, titleId, true, &failureMessage))
        {
            handler->PSendSysMessage("称号系统：{}。", failureMessage.empty() ? "激活失败，请检查需求或称号ID" : failureMessage);
            return false;
        }

        return true;
    }

    static bool HandleRefreshCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        ChenghaoSystemMgr::Instance()->LoadPlayerData(player);
        ChenghaoSystemMgr::Instance()->RefreshPlayerBonuses(player);

        if (ChenghaoSystemEntry const* highestEntry = ChenghaoSystemMgr::Instance()->GetHighestUnlockedEntry(player))
        {
            handler->PSendSysMessage("称号系统已刷新。当前生效称号 [id:{}] 等级:{} 生命加成:{} 减伤:{}% 描述:{}",
                highestEntry->id,
                highestEntry->titleLevel,
                ChenghaoSystemMgr::Instance()->GetHealthBonus(player),
                ChenghaoSystemMgr::Instance()->GetDamageReductionPct(player),
                highestEntry->description.empty() ? "无" : highestEntry->description.c_str());
        }
        else
        {
            handler->SendSysMessage("称号系统已刷新，当前没有任何已生效称号。");
        }

        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<ChenghaoSystemEntry> const& entries = ChenghaoSystemMgr::Instance()->GetEntries();
        if (entries.empty())
        {
            handler->SendSysMessage("称号系统当前没有可用配置。");
            return true;
        }

        handler->PSendSysMessage("称号系统共有 {} 条配置：", static_cast<uint32>(entries.size()));
        uint32 currentTitleId = ChenghaoSystemMgr::Instance()->GetHighestUnlockedTitleId(player);

        for (ChenghaoSystemEntry const& entry : entries)
        {
            char const* status = "未激活";
            if (currentTitleId == entry.id)
                status = "当前生效";
            else if (ChenghaoSystemMgr::Instance()->IsPlayerUnlocked(player, entry.id))
                status = "已激活";

            handler->PSendSysMessage("[id:{}] 等级:{} 需求系统:{} 生命加成:{} 减伤:{}% 状态:{} 描述:{}",
                entry.id,
                entry.titleLevel,
                entry.requirementTemplateId,
                entry.attributeValue1,
                entry.attributeValue2,
                status,
                entry.description.empty() ? "无" : entry.description.c_str());
        }

        return true;
    }
};
}

void AddSC_mod_chenghao_system()
{
    new ChenghaoSystemWorldScript();
    new ChenghaoSystemPlayerScript();
    new ChenghaoSystemUnitScript();
    new ChenghaoSystemCommandScript();
}

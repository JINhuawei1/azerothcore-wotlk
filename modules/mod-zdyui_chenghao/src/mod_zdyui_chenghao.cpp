/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#if __has_include("RequirementSystem.h")
#include "RequirementSystem.h"
#define ZDYUI_CHENGHAO_HAS_REQUIREMENT_SYSTEM 1
#else
#define ZDYUI_CHENGHAO_HAS_REQUIREMENT_SYSTEM 0
#endif

#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr char const* CONF_ENABLE = "ZDYUI.ChengHao.Enable";
constexpr char const* CONF_NOTIFY_PLAYER = "ZDYUI.ChengHao.NotifyPlayer";
constexpr char ZDYUI_CHENGHAO_ADDON_PREFIX[] = "ZDYUI_CH";
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

struct ZDYUIChengHaoEntry
{
    uint32 id = 0;
    uint32 titleLevel = 0;
    uint32 requirementTemplateId = 0;
    uint32 auraSpellId = 0;
    std::string description;
};

struct ZDYUIPlayerChengHaoState
{
    uint32 titleLevel = 0;
    uint32 auraSpellId = 0;
};

class ZDYUIChengHaoMgr
{
public:
    static ZDYUIChengHaoMgr* Instance()
    {
        static ZDYUIChengHaoMgr instance;
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
        _managedAuraIds.clear();

        QueryResult result = WorldDatabase.Query("SELECT id, 称号等级, 需求系统id, 激活光环技能id, 称号描述 FROM _自定义ui_称号系统");
        if (!result)
        {
            LOG_WARN("server.loading", "自定义UI称号系统未加载到任何数据，请检查表 _自定义ui_称号系统。");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            ZDYUIChengHaoEntry entry;
            entry.id = fields[0].Get<uint32>();
            entry.titleLevel = fields[1].Get<uint32>();
            entry.requirementTemplateId = fields[2].Get<uint32>();
            entry.auraSpellId = fields[3].Get<uint32>();
            entry.description = fields[4].Get<std::string>();

            if (!entry.id || !entry.titleLevel || !entry.auraSpellId)
            {
                LOG_ERROR("sql.sql", "自定义UI称号系统存在非法数据: id={}, 称号等级={}, 需求系统id={}, 激活光环技能id={}",
                    entry.id, entry.titleLevel, entry.requirementTemplateId, entry.auraSpellId);
                continue;
            }

            if (!sSpellMgr->GetSpellInfo(entry.auraSpellId))
            {
                LOG_ERROR("sql.sql", "自定义UI称号系统 id={} 的光环技能 {} 不存在。", entry.id, entry.auraSpellId);
                continue;
            }

            _entries.push_back(entry);
            _managedAuraIds.insert(entry.auraSpellId);
        }
        while (result->NextRow());

        std::sort(_entries.begin(), _entries.end(), [](ZDYUIChengHaoEntry const& left, ZDYUIChengHaoEntry const& right)
        {
            if (left.titleLevel != right.titleLevel)
                return left.titleLevel < right.titleLevel;

            return left.id < right.id;
        });

        LOG_INFO("server.loading", ">> 自定义UI称号系统已加载 {} 条称号配置。", _entries.size());
    }

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        _playerTitles[playerGuid].clear();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `称号ID`, `称号等级`, `光环技能id` FROM `_玩家称号系统` WHERE `玩家GUID` = {}",
            playerGuid);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 titleId = fields[0].Get<uint32>();
            ZDYUIPlayerChengHaoState state;
            state.titleLevel = fields[1].Get<uint32>();
            state.auraSpellId = fields[2].Get<uint32>();

            if (!titleId || !state.auraSpellId || !sSpellMgr->GetSpellInfo(state.auraSpellId))
                continue;

            _playerTitles[playerGuid][titleId] = state;
        }
        while (result->NextRow());
    }

    void RemoveConfiguredAuras(Player* player) const
    {
        if (!player)
            return;

        for (uint32 auraId : _managedAuraIds)
        {
            player->RemoveAura(auraId);
        }
    }

    void ReapplyPlayerAuras(Player* player) const
    {
        if (!player)
            return;

        RemoveConfiguredAuras(player);

        ZDYUIPlayerChengHaoState const* highestState = GetHighestUnlockedState(player);
        if (!highestState)
            return;

        if (!highestState->auraSpellId || !sSpellMgr->GetSpellInfo(highestState->auraSpellId))
            return;

        if (!player->HasAura(highestState->auraSpellId))
            player->AddAura(highestState->auraSpellId, player);
    }

    std::vector<ZDYUIChengHaoEntry> const& GetEntries() const
    {
        return _entries;
    }

    uint32 GetHighestUnlockedTitleId(Player* player) const
    {
        if (!player)
            return 0;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end() || playerIt->second.empty())
            return 0;

        uint32 bestTitleId = 0;
        ZDYUIPlayerChengHaoState const* bestState = nullptr;

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

    ZDYUIChengHaoEntry const* GetHighestUnlockedEntry(Player* player) const
    {
        uint32 titleId = GetHighestUnlockedTitleId(player);
        return titleId ? GetEntryById(titleId) : nullptr;
    }

    bool IsPlayerUnlocked(Player* player, uint32 titleId) const
    {
        if (!player)
            return false;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end())
            return false;

        return playerIt->second.find(titleId) != playerIt->second.end();
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
            if (state.titleLevel == titleLevel)
                return true;
        }

        return false;
    }

    bool CanActivateEntry(Player* player, ZDYUIChengHaoEntry const& entry, bool showMessages, bool consumeRequirements,
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

#if ZDYUI_CHENGHAO_HAS_REQUIREMENT_SYSTEM
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

        ZDYUIChengHaoEntry const* entry = GetEntryById(titleId);
        if (!entry)
        {
            if (failureMessage)
                *failureMessage = "未找到称号配置";
            return false;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto& playerTitles = _playerTitles[playerGuid];
        if (playerTitles.find(titleId) != playerTitles.end())
        {
            if (failureMessage)
                *failureMessage = "该称号已经激活";
            return false;
        }

        if (!CanActivateEntry(player, *entry, showMessages, true, failureMessage))
            return false;

        ZDYUIPlayerChengHaoState state;
        state.titleLevel = entry->titleLevel;
        state.auraSpellId = entry->auraSpellId;

        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家称号系统` (`玩家GUID`, `称号ID`, `称号等级`, `光环技能id`) "
            "VALUES ({}, {}, {}, {})",
            playerGuid,
            titleId,
            state.titleLevel,
            state.auraSpellId);

        playerTitles[titleId] = state;

        ReapplyPlayerAuras(player);

        if (showMessages && ShouldNotifyPlayer())
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "称号系统：已激活称号 [id:{}] 等级:{} 光环:{} 描述:{}",
                titleId,
                state.titleLevel,
                state.auraSpellId,
                entry->description.empty() ? "无" : entry->description.c_str());
        }

        return true;
    }

    uint32 UnlockAllAvailableTitles(Player* player, bool showMessages)
    {
        if (!player)
            return 0;

        uint32 count = 0;
        for (ZDYUIChengHaoEntry const& entry : _entries)
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

            if (ZDYUIChengHaoEntry const* highestEntry = GetHighestUnlockedEntry(player))
            {
                handler.PSendSysMessage("称号系统：当前生效称号 [id:{}] 等级:{} 光环:{} 描述:{}",
                    highestEntry->id,
                    highestEntry->titleLevel,
                    highestEntry->auraSpellId,
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
    ZDYUIPlayerChengHaoState const* GetHighestUnlockedState(Player* player) const
    {
        if (!player)
            return nullptr;

        auto playerIt = _playerTitles.find(player->GetGUID().GetCounter());
        if (playerIt == _playerTitles.end() || playerIt->second.empty())
            return nullptr;

        uint32 bestTitleId = GetHighestUnlockedTitleId(player);
        return bestTitleId ? &playerIt->second.at(bestTitleId) : nullptr;
    }

    ZDYUIChengHaoEntry const* GetEntryById(uint32 titleId) const
    {
        for (ZDYUIChengHaoEntry const& entry : _entries)
        {
            if (entry.id == titleId)
                return &entry;
        }

        return nullptr;
    }

    std::vector<ZDYUIChengHaoEntry> _entries;
    std::unordered_set<uint32> _managedAuraIds;
    std::unordered_map<uint32, std::unordered_map<uint32, ZDYUIPlayerChengHaoState>> _playerTitles;
};

void SendAddonPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= MAX_ADDON_PAYLOAD)
    {
        std::string fullMessage = std::string(ZDYUI_CHENGHAO_ADDON_PREFIX) + '\t' + payload;
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

        std::string fullMessage = std::string(ZDYUI_CHENGHAO_ADDON_PREFIX) + '\t' + chunkMessage.str();
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

    std::vector<ZDYUIChengHaoEntry> entries = ZDYUIChengHaoMgr::Instance()->GetEntries();
    std::sort(entries.begin(), entries.end(), [](ZDYUIChengHaoEntry const& left, ZDYUIChengHaoEntry const& right)
    {
        if (left.titleLevel != right.titleLevel)
            return left.titleLevel < right.titleLevel;
        return left.id < right.id;
    });

    std::ostringstream payload;
    payload << "CH_LIST:";

    bool first = true;
    for (ZDYUIChengHaoEntry const& entry : entries)
    {
        if (!first)
            payload << '~';
        first = false;

        payload << entry.id << '^'
                << entry.titleLevel << '^'
                << entry.requirementTemplateId << '^'
                << entry.auraSpellId << '^'
                << SanitizeAddonText(entry.description);
    }

    SendAddonPayload(player, payload.str());
}

void SendChengHaoStateToPlayer(Player* player)
{
    if (!player)
        return;

    std::vector<ZDYUIChengHaoEntry> const& entries = ZDYUIChengHaoMgr::Instance()->GetEntries();
    uint32 currentTitleId = ZDYUIChengHaoMgr::Instance()->GetHighestUnlockedTitleId(player);

    std::ostringstream payload;
    payload << "CH_STATE:";

    bool first = true;
    for (ZDYUIChengHaoEntry const& entry : entries)
    {
        if (!first)
            payload << '~';
        first = false;

        bool unlocked = ZDYUIChengHaoMgr::Instance()->IsPlayerUnlocked(player, entry.id);
        bool eligible = false;
        if (!unlocked)
            eligible = ZDYUIChengHaoMgr::Instance()->CanActivateEntry(player, entry, false, false);

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

    ZDYUIChengHaoMgr::Instance()->LoadPlayerData(player);
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

class ZDYUIChengHaoWorldScript : public WorldScript
{
public:
    ZDYUIChengHaoWorldScript() : WorldScript("ZDYUIChengHaoWorldScript") { }

    void OnStartup() override
    {
        if (!ZDYUIChengHaoMgr::Instance()->IsEnabled())
            return;

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!ZDYUIChengHaoMgr::Instance()->IsEnabled() || _initialized)
            return;

        _loadTime += diff;

        if (_loadTime >= 3000 && !_displayedLog)
        {
            LOG_INFO("server.loading", "→自定义UI称号系统√");
            _displayedLog = true;
        }

        if (_loadTime >= 3000 && _displayedLog)
        {
            ZDYUIChengHaoMgr::Instance()->Initialize();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class ZDYUIChengHaoPlayerScript : public PlayerScript
{
public:
    ZDYUIChengHaoPlayerScript() : PlayerScript("ZDYUIChengHaoPlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        if (!ZDYUIChengHaoMgr::Instance()->IsEnabled())
        {
            ZDYUIChengHaoMgr::Instance()->RemoveConfiguredAuras(player);
            ZDYUIChengHaoMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
            return;
        }

        ZDYUIChengHaoMgr::Instance()->LoadPlayerData(player);
        ZDYUIChengHaoMgr::Instance()->ReapplyPlayerAuras(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        ZDYUIChengHaoMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        ZDYUIChengHaoMgr::Instance()->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!ZDYUIChengHaoMgr::Instance()->IsEnabled() || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != ZDYUI_CHENGHAO_ADDON_PREFIX)
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
            ZDYUIChengHaoMgr::Instance()->LoadPlayerData(player);
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
            uint32 count = ZDYUIChengHaoMgr::Instance()->UnlockAllAvailableTitles(player, false);
            std::ostringstream message;
            message << "本次激活 " << count << " 个称号";
            SendChengHaoActionResult(player, "ACT_ALL", true, 0, message.str());
            SendChengHaoStateToPlayer(player);
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

            if (ZDYUIChengHaoMgr::Instance()->IsPlayerUnlocked(player, titleId))
            {
                SendChengHaoActionResult(player, "ACTIVATE", false, titleId, "该称号已经激活");
                SendChengHaoStateToPlayer(player);
                return;
            }

            std::string failureMessage;
            bool success = ZDYUIChengHaoMgr::Instance()->UnlockTitle(player, titleId, true, &failureMessage);
            SendChengHaoActionResult(player, "ACTIVATE", success, titleId, success ? "激活成功" : (failureMessage.empty() ? "激活失败" : failureMessage));
            SendChengHaoStateToPlayer(player);
            return;
        }
    }
};

class ZDYUIChengHaoCommandScript : public CommandScript
{
public:
    ZDYUIChengHaoCommandScript() : CommandScript("ZDYUIChengHaoCommandScript") { }

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
            Acore::ChatCommands::ChatCommandBuilder("称号", chenghaoCommandTable)
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        ZDYUIChengHaoMgr::Instance()->Initialize();

        if (Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
        {
            ZDYUIChengHaoMgr::Instance()->LoadPlayerData(player);
            ZDYUIChengHaoMgr::Instance()->ReapplyPlayerAuras(player);
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
        handler->SendSysMessage("已向客户端发送称号界面打开请求。");
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
            ZDYUIChengHaoMgr::Instance()->UnlockAllAvailableTitles(player, true);
            return true;
        }

        uint32 titleId = static_cast<uint32>(std::strtoul(argText.c_str(), nullptr, 10));
        if (!titleId)
        {
            handler->SendSysMessage("称号系统：称号ID无效。");
            return false;
        }

        if (ZDYUIChengHaoMgr::Instance()->IsPlayerUnlocked(player, titleId))
        {
            handler->SendSysMessage("称号系统：该称号已经激活过了。");
            return true;
        }

        std::string failureMessage;
        if (!ZDYUIChengHaoMgr::Instance()->UnlockTitle(player, titleId, true, &failureMessage))
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

        ZDYUIChengHaoMgr::Instance()->LoadPlayerData(player);
        ZDYUIChengHaoMgr::Instance()->ReapplyPlayerAuras(player);

        if (ZDYUIChengHaoEntry const* highestEntry = ZDYUIChengHaoMgr::Instance()->GetHighestUnlockedEntry(player))
        {
            handler->PSendSysMessage("称号系统已刷新。当前生效称号 [id:{}] 等级:{} 光环:{} 描述:{}",
                highestEntry->id,
                highestEntry->titleLevel,
                highestEntry->auraSpellId,
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

        std::vector<ZDYUIChengHaoEntry> const& entries = ZDYUIChengHaoMgr::Instance()->GetEntries();
        if (entries.empty())
        {
            handler->SendSysMessage("称号系统当前没有可用配置。");
            return true;
        }

        handler->PSendSysMessage("称号系统共有 {} 条配置：", static_cast<uint32>(entries.size()));
        uint32 currentTitleId = ZDYUIChengHaoMgr::Instance()->GetHighestUnlockedTitleId(player);

        for (ZDYUIChengHaoEntry const& entry : entries)
        {
            char const* status = "未激活";
            if (currentTitleId == entry.id)
                status = "当前生效";
            else if (ZDYUIChengHaoMgr::Instance()->IsPlayerUnlocked(player, entry.id))
                status = "已激活";

            handler->PSendSysMessage("[id:{}] 等级:{} 需求系统:{} 光环:{} 状态:{} 描述:{}",
                entry.id,
                entry.titleLevel,
                entry.requirementTemplateId,
                entry.auraSpellId,
                status,
                entry.description.empty() ? "无" : entry.description.c_str());
        }

        return true;
    }
};
}

void AddSC_mod_zdyui_chenghao()
{
    new ZDYUIChengHaoWorldScript();
    new ZDYUIChengHaoPlayerScript();
    new ZDYUIChengHaoCommandScript();
}

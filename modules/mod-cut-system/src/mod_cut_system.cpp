/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#if __has_include("RequirementSystem.h")
#include "RequirementSystem.h"
#define CUT_SYSTEM_HAS_REQUIREMENT_SYSTEM 1
#else
#define CUT_SYSTEM_HAS_REQUIREMENT_SYSTEM 0
#endif

#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Unit.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr char const* CONF_ENABLE = "CutSystem.Enable";
constexpr char const* CONF_DEBUG = "CutSystem.Debug";
constexpr char CUT_SYSTEM_ADDON_PREFIX[] = "CUT_SYS";
constexpr size_t MAX_ADDON_PAYLOAD = 220;

std::string SanitizeAddonText(std::string text)
{
    for (char& ch : text)
    {
        if (ch == '^' || ch == '~' || ch == '|' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }

    return text;
}

enum CutDamageType : uint8
{
    CUT_DAMAGE_FIXED = 1,
    CUT_DAMAGE_PERCENT = 2
};

struct CutEntry
{
    uint32 id = 0;
    uint32 cutLevel = 0;
    uint32 requirementTemplateId = 0;
    std::string requirementText;
    uint8 damageType = CUT_DAMAGE_FIXED;
    float cutDamage = 0.0f;
    float chance = 0.0f;
};

void SendCutSystemHitNotification(Player* player, std::vector<uint32> const& damages);

class CutSystemMgr
{
public:
    static CutSystemMgr* Instance()
    {
        static CutSystemMgr instance;
        return &instance;
    }

    bool IsEnabled() const
    {
        return sConfigMgr->GetOption<bool>(CONF_ENABLE, true);
    }

    bool IsDebugEnabled() const
    {
        return sConfigMgr->GetOption<bool>(CONF_DEBUG, false);
    }

    void LoadEntries()
    {
        _entries.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT c.`id`, c.`切割等级`, c.`需求系统id`, c.`伤害类型`, c.`切割伤害`, c.`触发几率`, "
            "COALESCE(q.`客户端显示`, '') "
            "FROM `_切割系统` c "
            "LEFT JOIN `_模板_需求` q ON q.`id` = c.`需求系统id`");
        if (!result)
        {
            LOG_WARN("server.loading", "切割系统未加载到任何配置，请检查表 `_切割系统`。");
            return;
        }

        do
        {
            Field* fields = result->Fetch();

            CutEntry entry;
            entry.id = fields[0].Get<uint32>();
            entry.cutLevel = fields[1].Get<uint32>();
            entry.requirementTemplateId = fields[2].Get<uint32>();
            entry.damageType = fields[3].Get<uint8>();
            entry.cutDamage = fields[4].Get<float>();
            entry.chance = fields[5].Get<float>();
            entry.requirementText = fields[6].Get<std::string>();

            if (!entry.id || !entry.cutLevel)
            {
                LOG_ERROR("sql.sql", "切割系统存在非法数据: id={}, 切割等级={}", entry.id, entry.cutLevel);
                continue;
            }

            if (entry.damageType != CUT_DAMAGE_FIXED && entry.damageType != CUT_DAMAGE_PERCENT)
            {
                LOG_ERROR("sql.sql", "切割系统 id={} 的伤害类型 {} 非法，只允许 1 或 2。", entry.id, entry.damageType);
                continue;
            }

            if (entry.cutDamage <= 0.0f)
            {
                LOG_ERROR("sql.sql", "切割系统 id={} 的切割伤害 {} 非法，必须大于 0。", entry.id, entry.cutDamage);
                continue;
            }

            if (entry.damageType == CUT_DAMAGE_PERCENT && entry.cutDamage > 100.0f)
            {
                LOG_WARN("sql.sql", "切割系统 id={} 的百分比伤害 {} 超过 100，已按 100 处理。", entry.id, entry.cutDamage);
                entry.cutDamage = 100.0f;
            }

            if (entry.chance <= 0.0f || entry.chance > 100.0f)
            {
                LOG_ERROR("sql.sql", "切割系统 id={} 的触发几率 {} 非法，必须在 0-100 之间。", entry.id, entry.chance);
                continue;
            }

            _entries.push_back(entry);
        }
        while (result->NextRow());

        std::sort(_entries.begin(), _entries.end(), [](CutEntry const& left, CutEntry const& right)
        {
            if (left.cutLevel != right.cutLevel)
                return left.cutLevel < right.cutLevel;

            return left.id < right.id;
        });

        LOG_INFO("server.loading", ">> 切割系统已加载 {} 条配置。", static_cast<uint32>(_entries.size()));
    }

    void LoadPlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `切割等级` FROM `_玩家切割系统` WHERE `玩家GUID` = {}",
            playerGuid);

        if (!result)
        {
            _playerCutLevels.erase(playerGuid);
            return;
        }

        Field* fields = result->Fetch();
        uint32 cutLevel = fields[0].Get<uint32>();

        if (cutLevel == 0)
            _playerCutLevels.erase(playerGuid);
        else
            _playerCutLevels[playerGuid] = cutLevel;
    }

    void UnloadPlayerData(ObjectGuid::LowType playerGuid)
    {
        _playerCutLevels.erase(playerGuid);
    }

    void DeletePlayerData(ObjectGuid::LowType playerGuid)
    {
        _playerCutLevels.erase(playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家切割系统` WHERE `玩家GUID` = {}", playerGuid);
    }

    uint32 GetPlayerCutLevel(ObjectGuid::LowType playerGuid) const
    {
        auto itr = _playerCutLevels.find(playerGuid);
        if (itr == _playerCutLevels.end())
            return 0;

        return itr->second;
    }

    void SetPlayerCutLevel(ObjectGuid::LowType playerGuid, uint32 cutLevel)
    {
        if (cutLevel == 0)
        {
            _playerCutLevels.erase(playerGuid);
            CharacterDatabase.Execute("DELETE FROM `_玩家切割系统` WHERE `玩家GUID` = {}", playerGuid);
            return;
        }

        _playerCutLevels[playerGuid] = cutLevel;
        CharacterDatabase.Execute(
            "REPLACE INTO `_玩家切割系统` (`玩家GUID`, `切割等级`) VALUES ({}, {})",
            playerGuid,
            cutLevel);
    }

    CutEntry const* GetMatchedEntry(uint32 playerCutLevel) const
    {
        if (playerCutLevel == 0 || _entries.empty())
            return nullptr;

        auto itr = std::upper_bound(_entries.begin(), _entries.end(), playerCutLevel,
            [](uint32 level, CutEntry const& entry)
            {
                return level < entry.cutLevel;
            });

        if (itr == _entries.begin())
            return nullptr;

        --itr;
        return &(*itr);
    }

    CutEntry const* GetEntryByCutLevel(uint32 cutLevel) const
    {
        auto itr = std::find_if(_entries.begin(), _entries.end(), [cutLevel](CutEntry const& entry)
        {
            return entry.cutLevel == cutLevel;
        });

        return itr != _entries.end() ? &(*itr) : nullptr;
    }

    bool CanUpgradeEntry(Player* player, CutEntry const& entry, bool showMessages, bool consumeRequirements,
        std::string* failureMessage = nullptr) const
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        uint32 currentLevel = GetPlayerCutLevel(player->GetGUID().GetCounter());
        if (entry.cutLevel <= currentLevel)
        {
            if (showMessages)
                ChatHandler(player->GetSession()).PSendSysMessage("切割系统：当前切割等级 {} 已不低于目标等级 {}。", currentLevel, entry.cutLevel);

            if (failureMessage)
                *failureMessage = "当前等级已不低于目标等级";

            return false;
        }

        if (entry.requirementTemplateId == 0)
            return true;

#if CUT_SYSTEM_HAS_REQUIREMENT_SYSTEM
        if (!sRequirementSystem)
        {
            if (showMessages)
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[切割]|r 需求模板系统未加载，无法升级当前切割等级");

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
                handler.SendSysMessage("|cffffcc00[切割]|r 不满足升级条件");
                sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, true);
            }

            if (failureMessage)
                *failureMessage = "不满足升级条件";

            return false;
        }

        if (consumeRequirements && !sRequirementSystem->ConsumeRequirements(player, entry.requirementTemplateId))
        {
            if (showMessages)
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[切割]|r 升级需求消耗失败");

            if (failureMessage)
                *failureMessage = "需求消耗失败";

            return false;
        }

        return true;
#else
        if (showMessages)
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[切割]|r 需求模板系统模块未编译，无法升级当前切割等级");

        if (failureMessage)
            *failureMessage = "需求系统未编译";

        return false;
#endif
    }

    bool UpgradePlayerCutLevel(Player* player, uint32 targetLevel, bool showMessages, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        CutEntry const* entry = GetEntryByCutLevel(targetLevel);
        if (!entry)
        {
            if (showMessages)
                ChatHandler(player->GetSession()).PSendSysMessage("切割系统：未找到切割等级 {} 的配置。", targetLevel);

            if (failureMessage)
                *failureMessage = "未找到对应切割配置";

            return false;
        }

        if (!CanUpgradeEntry(player, *entry, showMessages, true, failureMessage))
            return false;

        SetPlayerCutLevel(player->GetGUID().GetCounter(), targetLevel);

        if (showMessages)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "切割系统：切割等级已升级到 {}。当前配置 [id:{}] 需求:{} 类型:{} 数值:{} 几率:{}%",
                entry->cutLevel,
                entry->id,
                entry->requirementTemplateId,
                static_cast<uint32>(entry->damageType),
                entry->cutDamage,
                entry->chance);
        }

        return true;
    }

    std::vector<CutEntry> const& GetEntries() const
    {
        return _entries;
    }

    void QueueHitNotification(ObjectGuid::LowType playerGuid, uint32 damage)
    {
        if (!playerGuid || damage == 0)
            return;

        _pendingHitNotifications[playerGuid].push_back(damage);
    }

    void FlushHitNotifications()
    {
        if (_pendingHitNotifications.empty())
            return;

        auto pendingNotifications = std::move(_pendingHitNotifications);
        _pendingHitNotifications.clear();

        for (auto const& [playerGuid, damages] : pendingNotifications)
        {
            if (damages.empty())
                continue;

            Player* player = ObjectAccessor::FindPlayerByLowGUID(playerGuid);
            if (!player)
                continue;

            SendCutSystemHitNotification(player, damages);
        }
    }

    uint32 CalculateCutDisplayDamage(Unit* attacker, Unit* victim, CutEntry const& entry) const
    {
        if (!attacker || !victim)
            return 0;

        if (entry.damageType == CUT_DAMAGE_FIXED)
        {
            return std::max<uint32>(1, static_cast<uint32>(std::ceil(entry.cutDamage)));
        }

        float percent = std::min(entry.cutDamage, 100.0f);
        uint32 finalDamage = static_cast<uint32>(std::ceil(static_cast<double>(victim->GetHealth()) * static_cast<double>(percent) / 100.0));
        return std::max<uint32>(1, finalDamage);
    }

    bool TryPrepareCutDamage(Unit* attacker, Unit* victim, uint32& displayDamage) const
    {
        displayDamage = 0;

        if (!IsEnabled() || !attacker || !victim)
            return false;

        Player* player = attacker->ToPlayer();
        if (!player)
            return false;

        if (!victim->IsCreature() || victim->IsPet() || victim->IsTotem())
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 playerCutLevel = GetPlayerCutLevel(playerGuid);
        CutEntry const* entry = GetMatchedEntry(playerCutLevel);
        if (!entry)
            return false;

        if (!roll_chance_f(entry->chance))
            return false;

        displayDamage = CalculateCutDisplayDamage(attacker, victim, *entry);
        if (displayDamage == 0)
            return false;

        if (IsDebugEnabled())
        {
            LOG_DEBUG("module", "切割系统：玩家 {} 切割等级 {} 命中 {}，切割伤害 {}，配置等级 {}，类型 {}，数值 {}，几率 {}%",
                player->GetName(),
                playerCutLevel,
                victim->GetName(),
                displayDamage,
                entry->cutLevel,
                static_cast<uint32>(entry->damageType),
                entry->cutDamage,
                entry->chance);
        }

        return true;
    }

private:
    std::vector<CutEntry> _entries;
    std::unordered_map<uint32, uint32> _playerCutLevels;
    std::unordered_map<uint32, std::vector<uint32>> _pendingHitNotifications;
};

void SendCutSystemPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= MAX_ADDON_PAYLOAD)
    {
        std::string fullMessage = std::string(CUT_SYSTEM_ADDON_PREFIX) + '\t' + payload;
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

        std::string fullMessage = std::string(CUT_SYSTEM_ADDON_PREFIX) + '\t' + chunkMessage.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendCutSystemOpenUI(Player* player)
{
    SendCutSystemPayload(player, "CT_OPEN");
}

void SendCutSystemListToPlayer(Player* player)
{
    if (!player)
        return;

    std::vector<CutEntry> entries = CutSystemMgr::Instance()->GetEntries();
    std::sort(entries.begin(), entries.end(), [](CutEntry const& left, CutEntry const& right)
    {
        if (left.cutLevel != right.cutLevel)
            return left.cutLevel < right.cutLevel;

        return left.id < right.id;
    });

    std::ostringstream payload;
    payload << "CT_LIST:";

    bool first = true;
    for (CutEntry const& entry : entries)
    {
        if (!first)
            payload << '~';
        first = false;

        payload << entry.id << '^'
                << entry.cutLevel << '^'
                << entry.requirementTemplateId << '^'
                << static_cast<uint32>(entry.damageType) << '^'
                << entry.cutDamage << '^'
                << entry.chance << '^'
                << SanitizeAddonText(entry.requirementText);
    }

    SendCutSystemPayload(player, payload.str());
}

void SendCutSystemStateToPlayer(Player* player)
{
    if (!player)
        return;

    uint32 playerCutLevel = CutSystemMgr::Instance()->GetPlayerCutLevel(player->GetGUID().GetCounter());
    CutEntry const* currentEntry = CutSystemMgr::Instance()->GetMatchedEntry(playerCutLevel);
    std::vector<CutEntry> const& entries = CutSystemMgr::Instance()->GetEntries();

    std::ostringstream payload;
    payload << "CT_STATE:" << playerCutLevel << '|' << (currentEntry ? currentEntry->id : 0) << '|';

    bool first = true;
    for (CutEntry const& entry : entries)
    {
        if (!first)
            payload << '~';
        first = false;

        bool reached = playerCutLevel >= entry.cutLevel;
        bool current = currentEntry && currentEntry->id == entry.id;
        bool eligible = CutSystemMgr::Instance()->CanUpgradeEntry(player, entry, false, false);

        payload << entry.id << '^'
                << (reached ? 1 : 0) << '^'
                << (current ? 1 : 0) << '^'
                << (eligible ? 1 : 0);
    }

    SendCutSystemPayload(player, payload.str());
}

void SendCutSystemAllDataToPlayer(Player* player)
{
    if (!player)
        return;

    CutSystemMgr::Instance()->LoadPlayerData(player);
    SendCutSystemListToPlayer(player);
    SendCutSystemStateToPlayer(player);
}

void SendCutSystemActionResult(Player* player, char const* action, bool success, uint32 cutLevel, std::string const& message)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "CT_RESULT:"
            << (action ? action : "") << '^'
            << (success ? "OK" : "FAIL") << '^'
            << cutLevel << '^'
            << SanitizeAddonText(message);

    SendCutSystemPayload(player, payload.str());
}

void SendCutSystemHitNotification(Player* player, std::vector<uint32> const& damages)
{
    if (!player || damages.empty())
        return;

    std::ostringstream payload;
    payload << "CT_HITS:";

    bool first = true;
    for (uint32 damage : damages)
    {
        if (damage == 0)
            continue;

        if (!first)
            payload << '~';

        first = false;
        payload << damage;
    }

    SendCutSystemPayload(player, payload.str());
}

class CutSystemWorldScript : public WorldScript
{
public:
    CutSystemWorldScript() : WorldScript("CutSystemWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (!CutSystemMgr::Instance()->IsEnabled())
            return;

        if (reload)
        {
            CutSystemMgr::Instance()->LoadEntries();
            LOG_INFO("module", "切割系统配置已重载。");
        }
    }

    void OnStartup() override
    {
        if (!CutSystemMgr::Instance()->IsEnabled())
        {
            LOG_INFO("module", "切割系统已禁用。");
            return;
        }

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
        _hitFlushTime = 0;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!CutSystemMgr::Instance()->IsEnabled())
            return;

        if (!_initialized)
        {
            _loadTime += diff;

            if (_loadTime >= 3000 && !_displayedLog)
            {
                LOG_INFO("server.loading", "→切割系统√");
                _displayedLog = true;
            }

            if (_loadTime >= 3000 && _displayedLog)
            {
                CutSystemMgr::Instance()->LoadEntries();
                _initialized = true;
            }
        }

        _hitFlushTime += diff;
        if (_initialized && _hitFlushTime >= 50)
        {
            CutSystemMgr::Instance()->FlushHitNotifications();
            _hitFlushTime = 0;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
    uint32 _hitFlushTime = 0;
};

class CutSystemPlayerScript : public PlayerScript
{
public:
    CutSystemPlayerScript() : PlayerScript("CutSystemPlayerScript",
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

        if (!CutSystemMgr::Instance()->IsEnabled())
        {
            CutSystemMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
            return;
        }

        CutSystemMgr::Instance()->LoadPlayerData(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        CutSystemMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CutSystemMgr::Instance()->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!CutSystemMgr::Instance()->IsEnabled() || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != CUT_SYSTEM_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendCutSystemAllDataToPlayer(player);
            return;
        }

        if (command == "REQ_LIST")
        {
            SendCutSystemListToPlayer(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            CutSystemMgr::Instance()->LoadPlayerData(player);
            SendCutSystemStateToPlayer(player);
            return;
        }

        if (command == "OPEN")
        {
            SendCutSystemOpenUI(player);
            return;
        }

        if (command.rfind("UPG:", 0) == 0)
        {
            std::string args = command.substr(4);
            uint32 cutLevel = 0;

            try
            {
                cutLevel = static_cast<uint32>(std::stoul(args));
            }
            catch (...)
            {
                SendCutSystemActionResult(player, "UPGRADE", false, 0, "参数错误");
                return;
            }

            if (!cutLevel)
            {
                SendCutSystemActionResult(player, "UPGRADE", false, 0, "切割等级无效");
                return;
            }

            std::string failureMessage;
            bool success = CutSystemMgr::Instance()->UpgradePlayerCutLevel(player, cutLevel, true, &failureMessage);
            SendCutSystemActionResult(player, "UPGRADE", success, cutLevel, success ? "升级成功" : (failureMessage.empty() ? "升级失败" : failureMessage));
            SendCutSystemStateToPlayer(player);
            return;
        }
    }
};

class CutSystemUnitScript : public UnitScript
{
public:
    CutSystemUnitScript() : UnitScript("CutSystemUnitScript") { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !victim->IsAlive())
            return;

        uint32 displayDamage = 0;
        if (!CutSystemMgr::Instance()->TryPrepareCutDamage(attacker, victim, displayDamage))
            return;

        uint32 appliedDamage = displayDamage;
        if (appliedDamage > std::numeric_limits<uint32>::max() - damage)
            appliedDamage = std::numeric_limits<uint32>::max() - damage;

        if (appliedDamage == 0)
            return;

        damage += appliedDamage;

        if (Player* player = attacker->ToPlayer())
            CutSystemMgr::Instance()->QueueHitNotification(player->GetGUID().GetCounter(), appliedDamage);
    }
};

class CutSystemCommandScript : public CommandScript
{
public:
    CutSystemCommandScript() : CommandScript("CutSystemCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable cutCommandTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("重载", HandleReloadCommand, SEC_ADMINISTRATOR, Acore::ChatCommands::Console::Yes),
            Acore::ChatCommands::ChatCommandBuilder("界面", HandleOpenUICommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("查看", HandleInfoCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("升级", HandleUpgradeCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("设置", HandleSetCommand, SEC_GAMEMASTER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("列表", HandleListCommand, SEC_GAMEMASTER, Acore::ChatCommands::Console::No)
        };

        static Acore::ChatCommands::ChatCommandTable commandTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("切割", cutCommandTable)
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        CutSystemMgr::Instance()->LoadEntries();
        handler->SendSysMessage("切割系统配置已重新加载。");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        SendCutSystemOpenUI(player);
        handler->SendSysMessage("已向客户端发送切割系统界面打开请求。");
        return true;
    }

    static bool HandleInfoCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;

        uint32 playerCutLevel = CutSystemMgr::Instance()->GetPlayerCutLevel(target->GetGUID().GetCounter());
        CutEntry const* entry = CutSystemMgr::Instance()->GetMatchedEntry(playerCutLevel);

        handler->PSendSysMessage("切割系统：角色 {} 当前切割等级为 {}。", target->GetName(), playerCutLevel);

        if (!entry)
        {
            if (playerCutLevel == 0)
                handler->SendSysMessage("切割系统：当前角色未设置切割等级。");
            else
                handler->SendSysMessage("切割系统：当前切割等级没有匹配到任何生效配置。");

            return true;
        }

        handler->PSendSysMessage("切割系统：当前生效配置 [id:{}] 等级门槛:{} 需求:{} 类型:{} 数值:{} 触发几率:{}%",
            entry->id,
            entry->cutLevel,
            entry->requirementTemplateId,
            static_cast<uint32>(entry->damageType),
            entry->cutDamage,
            entry->chance);

        return true;
    }

    static bool HandleUpgradeCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::string argText = args ? args : "";
        if (argText.empty())
        {
            handler->SendSysMessage("语法: .切割 升级 <切割等级>");
            return false;
        }

        char* endPtr = nullptr;
        unsigned long parsedValue = std::strtoul(argText.c_str(), &endPtr, 10);

        while (endPtr && *endPtr == ' ')
            ++endPtr;

        if (argText.empty() || !endPtr || *endPtr != '\0')
        {
            handler->SendSysMessage("切割系统：请输入有效的数字等级。");
            return false;
        }

        uint32 cutLevel = static_cast<uint32>(parsedValue);
        if (cutLevel == 0 || cutLevel > 99999)
        {
            handler->SendSysMessage("切割系统：升级等级必须在 1-99999 之间。");
            return false;
        }

        std::string failureMessage;
        if (!CutSystemMgr::Instance()->UpgradePlayerCutLevel(player, cutLevel, true, &failureMessage))
        {
            handler->PSendSysMessage("切割系统：{}。", failureMessage.empty() ? "升级失败" : failureMessage);
            return false;
        }

        return true;
    }

    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;

        std::string argText = args ? args : "";
        if (argText.empty())
        {
            handler->SendSysMessage("语法: .切割 设置 <等级>");
            return false;
        }

        char* endPtr = nullptr;
        unsigned long parsedValue = std::strtoul(argText.c_str(), &endPtr, 10);

        while (endPtr && *endPtr == ' ')
            ++endPtr;

        if (argText.empty() || !endPtr || *endPtr != '\0')
        {
            handler->SendSysMessage("切割系统：请输入有效的数字等级。");
            return false;
        }

        uint32 cutLevel = static_cast<uint32>(parsedValue);
        if (cutLevel > 99999)
        {
            handler->SendSysMessage("切割系统：切割等级必须在 0-99999 之间。");
            return false;
        }

        CutSystemMgr::Instance()->SetPlayerCutLevel(target->GetGUID().GetCounter(), cutLevel);

        if (cutLevel == 0)
        {
            handler->PSendSysMessage("切割系统：已清除角色 {} 的切割等级。", target->GetName());
            return true;
        }

        CutEntry const* entry = CutSystemMgr::Instance()->GetMatchedEntry(cutLevel);
        if (entry)
        {
            handler->PSendSysMessage("切割系统：已将角色 {} 的切割等级设置为 {}，当前匹配配置等级 {}。",
                target->GetName(),
                cutLevel,
                entry->cutLevel);
        }
        else
        {
            handler->PSendSysMessage("切割系统：已将角色 {} 的切割等级设置为 {}，但暂时没有匹配到生效配置。",
                target->GetName(),
                cutLevel);
        }

        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        std::vector<CutEntry> const& entries = CutSystemMgr::Instance()->GetEntries();
        if (entries.empty())
        {
            handler->SendSysMessage("切割系统：当前没有任何配置。");
            return true;
        }

        handler->PSendSysMessage("切割系统共有 {} 条配置：", static_cast<uint32>(entries.size()));

        for (CutEntry const& entry : entries)
        {
            handler->PSendSysMessage("[id:{}] 等级门槛:{} 需求:{} 类型:{} 数值:{} 触发几率:{}%",
                entry.id,
                entry.cutLevel,
                entry.requirementTemplateId,
                static_cast<uint32>(entry.damageType),
                entry.cutDamage,
                entry.chance);
        }

        return true;
    }
};
}

void AddSC_mod_cut_system()
{
    new CutSystemWorldScript();
    new CutSystemPlayerScript();
    new CutSystemUnitScript();
    new CutSystemCommandScript();
}

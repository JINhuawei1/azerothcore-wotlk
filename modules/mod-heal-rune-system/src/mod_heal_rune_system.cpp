/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license:
 * https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#if __has_include("RequirementSystem.h")
#include "RequirementSystem.h"
#include "AddonThrottle.h"
#define HEAL_RUNE_HAS_REQUIREMENT_SYSTEM 1
#else
#define HEAL_RUNE_HAS_REQUIREMENT_SYSTEM 0
#endif

#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "HermesBridgeAddonApi.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Util.h"
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
constexpr char const* CONF_ENABLE = "HealRune.Enable";
constexpr char const* CONF_NOTIFY_PLAYER = "HealRune.NotifyPlayer";
constexpr char const* CONF_TICK_INTERVAL_MS = "HealRune.TickIntervalMs";
constexpr char const* CONF_DEBUG = "HealRune.Debug";
constexpr char HEAL_RUNE_ADDON_PREFIX[] = "HEAL_RUNE";
constexpr size_t MAX_ADDON_PAYLOAD = 220;

enum HealRuneMode : uint8
{
    HEAL_RUNE_MODE_VALUE = 1,
    HEAL_RUNE_MODE_PERCENT = 2
};

struct HealRuneEntry
{
    uint32 id = 0;
    std::string name;
    std::string description;
    std::string requirementText;
    uint32 healLevel = 0;
    uint32 requirementTemplateId = 0;
    uint8 healMode = HEAL_RUNE_MODE_VALUE;
    uint256 healValue = 0;
};

struct PlayerHealRuneState
{
    uint32 healLevel = 0;
};

std::string SanitizeAddonText(std::string text)
{
    for (char& ch : text)
    {
        if (ch == '^' || ch == '~' || ch == ':' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }

    return text;
}

std::string FormatHealValue(uint256 const& value)
{
    return value.convert_to<std::string>();
}

uint256 GetSecondaryDisplayValue(uint256 const& value)
{
    return value / 10;
}

uint32 GetPercentValue(uint256 const& value)
{
    return value > 100 ? 100 : static_cast<uint32>(value);
}

uint256 SaturatingMultiplyUInt256(uint256 const& value, uint32 multiplier)
{
    if (multiplier == 0 || value == 0)
        return 0;

    uint256 const maxValue = std::numeric_limits<uint256>::max();
    if (value > maxValue / multiplier)
        return maxValue;

    return value * multiplier;
}

std::string BuildHealRuneValueText(HealRuneEntry const& entry)
{
    std::ostringstream ss;
    if (entry.healMode == HEAL_RUNE_MODE_PERCENT)
        ss << "每秒恢复 " << FormatHealValue(entry.healValue) << "% 血量与法力，并恢复 "
           << FormatHealValue(GetSecondaryDisplayValue(entry.healValue)) << "% 怒气/能量/符文能量";
    else
        ss << "每秒恢复 " << FormatHealValue(entry.healValue) << " 点血量与法力，并恢复 "
           << FormatHealValue(GetSecondaryDisplayValue(entry.healValue)) << " 点怒气/能量/符文能量";

    return ss.str();
}

std::string GetHealRuneDisplayName(HealRuneEntry const& entry)
{
    if (!entry.name.empty())
        return entry.name;

    std::ostringstream ss;
    ss << "神符 #" << entry.id << "  ·  等级 " << entry.healLevel;
    return ss.str();
}

std::string BuildHealRuneModeText(uint8 mode)
{
    switch (mode)
    {
        case HEAL_RUNE_MODE_VALUE:
            return "自定义值";
        case HEAL_RUNE_MODE_PERCENT:
            return "百分比";
        default:
            return "未知模式";
    }
}

class HealRuneMgr
{
public:
    static HealRuneMgr* Instance()
    {
        static HealRuneMgr instance;
        return &instance;
    }

    // 配置缓存：这些函数每 tick/每次治疗都会被调用，
    // 不能每次调用都做 GetOption 的字符串查找；LoadEntries（启动/重载）时刷新
    bool IsEnabled() const
    {
        return _configEnabled;
    }

    bool ShouldNotifyPlayer() const
    {
        return _configNotifyPlayer;
    }

    bool IsDebugEnabled() const
    {
        return _configDebug;
    }

    uint32 GetTickIntervalMs() const
    {
        return _configTickIntervalMs;
    }

private:
    bool _configEnabled = true;
    bool _configNotifyPlayer = true;
    bool _configDebug = false;
    uint32 _configTickIntervalMs = 1000;

public:
    void LoadEntries()
    {
        _configEnabled = sConfigMgr->GetOption<bool>(CONF_ENABLE, true);
        _configNotifyPlayer = sConfigMgr->GetOption<bool>(CONF_NOTIFY_PLAYER, true);
        _configDebug = sConfigMgr->GetOption<bool>(CONF_DEBUG, false);
        _configTickIntervalMs = std::max<uint32>(100, sConfigMgr->GetOption<uint32>(CONF_TICK_INTERVAL_MS, 1000));

        _entries.clear();

        QueryResult result = WorldDatabase.Query(
            "SELECT r.`id`, r.`回血神符名称`, r.`回血神符描述`, r.`回血等级`, r.`需求系统id`, r.`血蓝设置`, r.`血蓝值`, "
            "COALESCE(q.`客户端显示`, '') "
            "FROM `_回血神符` r "
            "LEFT JOIN `_模板_需求` q ON q.`id` = r.`需求系统id`");
        if (!result)
        {
            LOG_WARN("server.loading", "回血神符系统未加载到任何配置，请检查表 `_回血神符`。");
            return;
        }

        do
        {
            Field* fields = result->Fetch();

            HealRuneEntry entry;
            entry.id = fields[0].Get<uint32>();
            entry.name = fields[1].Get<std::string>();
            entry.description = fields[2].Get<std::string>();
            entry.healLevel = fields[3].Get<uint32>();
            entry.requirementTemplateId = fields[4].Get<uint32>();
            entry.healMode = fields[5].Get<uint8>();
            entry.healValue = fields[6].GetUInt256();
            entry.requirementText = fields[7].Get<std::string>();

            if (!entry.id || !entry.healLevel || entry.healLevel > 999999)
            {
                LOG_ERROR("sql.sql", "回血神符存在非法数据: id={}, 回血等级={}", entry.id, entry.healLevel);
                continue;
            }

            if (entry.healMode != HEAL_RUNE_MODE_VALUE && entry.healMode != HEAL_RUNE_MODE_PERCENT)
            {
                LOG_ERROR("sql.sql", "回血神符 id={} 的血蓝设置 {} 非法，只允许 1 或 2。", entry.id, static_cast<uint32>(entry.healMode));
                continue;
            }

            if (entry.healValue == 0)
            {
                LOG_ERROR("sql.sql", "回血神符 id={} 的血蓝值 {} 非法，必须大于 0。", entry.id, FormatHealValue(entry.healValue));
                continue;
            }

            if (entry.healMode == HEAL_RUNE_MODE_PERCENT && entry.healValue > 100)
            {
                LOG_WARN("sql.sql", "回血神符 id={} 的百分比数值 {} 超过 100，已按 100 处理。", entry.id, FormatHealValue(entry.healValue));
                entry.healValue = 100;
            }

            _entries.push_back(entry);
        }
        while (result->NextRow());

        std::sort(_entries.begin(), _entries.end(), [](HealRuneEntry const& left, HealRuneEntry const& right)
        {
            if (left.healLevel != right.healLevel)
                return left.healLevel < right.healLevel;

            return left.id < right.id;
        });
    }

    std::vector<HealRuneEntry> const& GetEntries() const
    {
        return _entries;
    }

    HealRuneEntry const* GetEntryById(uint32 runeId) const
    {
        auto itr = std::find_if(_entries.begin(), _entries.end(), [runeId](HealRuneEntry const& entry)
        {
            return entry.id == runeId;
        });

        return itr != _entries.end() ? &(*itr) : nullptr;
    }

    bool IsPlayerUnlocked(Player* player, uint32 runeId) const
    {
        if (!player || !runeId)
            return false;

        HealRuneEntry const* entry = GetEntryById(runeId);
        if (!entry)
            return false;

        auto playerItr = _playerRunes.find(player->GetGUID().GetCounter());
        if (playerItr == _playerRunes.end())
            return false;

        for (auto const& [storedRuneId, state] : playerItr->second)
        {
            (void)storedRuneId;
            if (state.healLevel >= entry->healLevel)
                return true;
        }

        return false;
    }

    bool HasUnlockedHealLevel(Player* player, uint32 healLevel) const
    {
        if (!player || !healLevel)
            return false;

        auto playerItr = _playerRunes.find(player->GetGUID().GetCounter());
        if (playerItr == _playerRunes.end())
            return false;

        for (auto const& [runeId, state] : playerItr->second)
        {
            (void)runeId;
            if (state.healLevel >= healLevel)
                return true;
        }

        return false;
    }

    HealRuneEntry const* GetHighestUnlockedEntry(Player* player) const
    {
        if (!player)
            return nullptr;

        auto playerItr = _playerRunes.find(player->GetGUID().GetCounter());
        if (playerItr == _playerRunes.end() || playerItr->second.empty())
            return nullptr;

        HealRuneEntry const* bestEntry = nullptr;
        uint32 bestId = 0;

        for (auto const& [runeId, state] : playerItr->second)
        {
            HealRuneEntry const* entry = GetEntryById(runeId);
            if (!entry)
            {
                (void)state;
                continue;
            }

            if (!bestEntry || entry->healLevel > bestEntry->healLevel ||
                (entry->healLevel == bestEntry->healLevel && runeId < bestId))
            {
                bestEntry = entry;
                bestId = runeId;
            }
        }

        return bestEntry;
    }

    uint32 GetHighestUnlockedRuneId(Player* player) const
    {
        HealRuneEntry const* entry = GetHighestUnlockedEntry(player);
        return entry ? entry->id : 0;
    }

    std::vector<HealRuneEntry const*> GetUiEntries(Player* player) const
    {
        std::vector<HealRuneEntry const*> result;

        HealRuneEntry const* currentEntry = GetHighestUnlockedEntry(player);
        if (currentEntry)
            result.push_back(currentEntry);

        for (HealRuneEntry const& entry : _entries)
        {
            if (currentEntry && entry.id == currentEntry->id)
                continue;

            if (!IsPlayerUnlocked(player, entry.id))
            {
                result.push_back(&entry);
                break;
            }
        }

        return result;
    }

    void LoadPlayerData(Player* player, bool keepOnlineCacheIfDbEmpty = false)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 onlineHealLevel = 0;
        auto cachedRunesItr = _playerRunes.find(playerGuid);
        if (cachedRunesItr != _playerRunes.end())
        {
            for (auto const& [runeId, state] : cachedRunesItr->second)
            {
                (void)runeId;
                onlineHealLevel = std::max(onlineHealLevel, state.healLevel);
            }
        }

        bool hasOnlineCache = onlineHealLevel > 0;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `神符ID`, `回血等级` FROM `_玩家回血神符` WHERE `玩家GUID` = {} "
            "ORDER BY `回血等级` DESC, `神符ID` DESC LIMIT 1",
            playerGuid);

        if (!result)
        {
            if (!keepOnlineCacheIfDbEmpty || !hasOnlineCache)
                _playerRunes[playerGuid].clear();

            _playerTickTimers[playerGuid] = 0;
            return;
        }

        uint32 loadedHealLevel = 0;
        std::unordered_map<uint32, PlayerHealRuneState> loadedRunes;
        do
        {
            Field* fields = result->Fetch();
            uint32 runeId = fields[0].Get<uint32>();
            PlayerHealRuneState state;
            state.healLevel = fields[1].Get<uint32>();

            if (!runeId)
                continue;

            loadedRunes[runeId] = state;
            loadedHealLevel = std::max(loadedHealLevel, state.healLevel);
        }
        while (result->NextRow());

        if (keepOnlineCacheIfDbEmpty && onlineHealLevel > loadedHealLevel)
        {
            _playerTickTimers[playerGuid] = 0;
            return;
        }

        _playerRunes[playerGuid] = loadedRunes;
        _playerTickTimers[playerGuid] = 0;

    }

    void UnloadPlayerData(uint32 playerGuid)
    {
        _playerRunes.erase(playerGuid);
        _playerTickTimers.erase(playerGuid);
    }

    void DeletePlayerData(uint32 playerGuid)
    {
        _playerRunes.erase(playerGuid);
        _playerTickTimers.erase(playerGuid);
        CharacterDatabase.Execute("DELETE FROM `_玩家回血神符` WHERE `玩家GUID` = {}", playerGuid);
    }

    bool CanActivateEntry(Player* player, HealRuneEntry const& entry, bool showMessages, bool consumeRequirements,
        std::string* failureMessage = nullptr) const
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        if (IsPlayerUnlocked(player, entry.id))
        {
            if (failureMessage)
                *failureMessage = "该神符已经激活";
            return false;
        }

        if (entry.healLevel > 1 && !HasUnlockedHealLevel(player, entry.healLevel - 1))
        {
            if (showMessages && player->GetSession())
            {
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "回血神符：请先激活 {} 级神符后，再激活 {} 级神符。",
                    entry.healLevel - 1,
                    entry.healLevel);
            }

            if (failureMessage)
                *failureMessage = "请先激活上一等级神符";

            return false;
        }

        if (entry.requirementTemplateId == 0)
            return true;

#if HEAL_RUNE_HAS_REQUIREMENT_SYSTEM
        if (!sRequirementSystem)
        {
            if (showMessages && player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[回血神符]|r 需求模板系统未加载，无法激活当前神符");

            if (failureMessage)
                *failureMessage = "需求系统未加载";

            return false;
        }

        bool meetsRequirements = sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, false);
        if (!meetsRequirements)
        {
            if (showMessages && player->GetSession())
            {
                ChatHandler handler(player->GetSession());
                handler.SendSysMessage("|cffffcc00[回血神符]|r 不满足激活条件");
                sRequirementSystem->CheckRequirements(player, entry.requirementTemplateId, true);
            }

            if (failureMessage)
                *failureMessage = "不满足激活条件";

            return false;
        }

        if (consumeRequirements && !sRequirementSystem->ConsumeRequirements(player, entry.requirementTemplateId))
        {
            if (showMessages && player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[回血神符]|r 激活需求消耗失败");

            if (failureMessage)
                *failureMessage = "需求消耗失败";

            return false;
        }

        return true;
#else
        if (showMessages && player->GetSession())
            ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[回血神符]|r 需求模板系统模块未编译，无法激活当前神符");

        if (failureMessage)
            *failureMessage = "需求系统未编译";

        return false;
#endif
    }

    bool UnlockRune(Player* player, uint32 runeId, bool showMessages, std::string* failureMessage = nullptr)
    {
        if (!player)
        {
            if (failureMessage)
                *failureMessage = "玩家无效";
            return false;
        }

        HealRuneEntry const* entry = GetEntryById(runeId);
        if (!entry)
        {
            if (failureMessage)
                *failureMessage = "未找到神符配置";
            return false;
        }

        if (!CanActivateEntry(player, *entry, showMessages, true, failureMessage))
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        auto& playerRunes = _playerRunes[playerGuid];
        playerRunes.clear();
        playerRunes[runeId] = { entry->healLevel };

        // 内存即权威（上面已先更新 _playerRunes），upsert 无后续回读，异步写即可
        CharacterDatabase.Execute(
            "INSERT INTO `_玩家回血神符` (`玩家GUID`, `神符ID`, `回血等级`) VALUES ({}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `神符ID` = VALUES(`神符ID`), `回血等级` = VALUES(`回血等级`)",
            playerGuid,
            runeId,
            entry->healLevel);

        if (showMessages && ShouldNotifyPlayer() && player->GetSession())
        {
            std::ostringstream ss;
            ss << "回血神符：已激活 " << GetHealRuneDisplayName(*entry)
               << " [id:" << entry->id
               << "] 等级:" << entry->healLevel
               << " 模式:" << BuildHealRuneModeText(entry->healMode)
               << " 效果:" << BuildHealRuneValueText(*entry);
            ChatHandler(player->GetSession()).SendSysMessage(ss.str().c_str());
        }

        return true;
    }

    uint32 UnlockAllAvailableRunes(Player* player, bool showMessages)
    {
        if (!player)
            return 0;

        uint32 count = 0;
        for (HealRuneEntry const& entry : _entries)
        {
            if (IsPlayerUnlocked(player, entry.id))
                continue;

            if (!CanActivateEntry(player, entry, false, false))
                continue;

            if (UnlockRune(player, entry.id, false))
                ++count;
        }

        if (showMessages && player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("回血神符：本次共激活 {} 个神符。", count);

        return count;
    }

    void ResetPlayerTick(uint32 playerGuid)
    {
        _playerTickTimers[playerGuid] = 0;
    }

    void UpdatePlayer(Player* player, uint32 diff)
    {
        if (!IsEnabled() || !player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 tickInterval = GetTickIntervalMs();
        uint32& elapsed = _playerTickTimers[playerGuid];
        elapsed += diff;

        if (elapsed < tickInterval)
            return;

        elapsed %= tickInterval;

        if (!player->IsAlive())
            return;

        HealRuneEntry const* entry = GetHighestUnlockedEntry(player);
        if (!entry)
            return;

        ApplyRuneTick(player, *entry);
    }

private:
    uint256 CalculateTickAmount(HealRuneEntry const& entry, uint256 const& maxValue) const
    {
        if (maxValue == 0)
            return 0;

        if (entry.healMode == HEAL_RUNE_MODE_PERCENT)
        {
            uint32 percent = GetPercentValue(entry.healValue);
            uint256 amount = Acore::Number::ToUInt256Saturated(std::ceil(Acore::Number::ToLongDouble(maxValue) * static_cast<long double>(percent) / 100.0L));

            return amount > 0 ? amount : 1;
        }

        return entry.healValue > 0 ? entry.healValue : 1;
    }

    uint256 CalculateSecondaryTickAmount(HealRuneEntry const& entry, uint256 const& maxValue, Powers powerType) const
    {
        if (maxValue == 0)
            return 0;

        if (entry.healMode == HEAL_RUNE_MODE_PERCENT)
        {
            uint32 percent = GetPercentValue(entry.healValue) / 10;
            if (percent == 0)
                return 0;

            uint256 amount = Acore::Number::ToUInt256Saturated(std::ceil(Acore::Number::ToLongDouble(maxValue) * static_cast<long double>(percent) / 100.0L));

            return amount > 0 ? amount : 1;
        }

        uint256 displayValue = GetSecondaryDisplayValue(entry.healValue);
        if (displayValue == 0)
            return 0;

        if (powerType == POWER_RAGE || powerType == POWER_RUNIC_POWER)
            return SaturatingMultiplyUInt256(displayValue, 10);

        return displayValue;
    }

    uint256 ClampRestoreAmount(uint256 const& amount, uint256 const& currentValue, uint256 const& maxValue) const
    {
        if (amount == 0 || currentValue >= maxValue)
            return 0;

        uint256 missingValue = maxValue - currentValue;
        return amount > missingValue ? missingValue : amount;
    }

    void TryRestoreHealth(Player* player, uint256 const& amount, bool& applied) const
    {
        if (!player || amount == 0)
            return;

        uint256 maxHealth = player->GetMaxHealthForCombat256();
        uint256 currentHealth = player->GetHealthForCombat256();
        uint256 restoreAmount = ClampRestoreAmount(amount, currentHealth, maxHealth);
        if (restoreAmount == 0)
            return;

        player->SetHealthForCombat256(currentHealth + restoreAmount);
        applied = true;
    }

    void TryRestorePower(Player* player, Powers powerType, uint256 const& amount, bool& applied) const
    {
        if (!player || amount == 0)
            return;

        if (powerType != POWER_MANA && !player->HasActivePowerType(powerType))
            return;

        uint256 maxPower = player->GetMaxPowerForCombat256(powerType);
        uint256 currentPower = player->GetPowerForCombat256(powerType);
        uint256 restoreAmount = ClampRestoreAmount(amount, currentPower, maxPower);
        if (restoreAmount == 0)
            return;

        player->SetPowerForCombat256(powerType, currentPower + restoreAmount);

        applied = true;
    }

    void ApplyRuneTick(Player* player, HealRuneEntry const& entry) const
    {
        bool applied = false;

        TryRestoreHealth(player, CalculateTickAmount(entry, player->GetMaxHealthForCombat256()), applied);

        TryRestorePower(player, POWER_MANA, CalculateTickAmount(entry, player->GetMaxPowerForCombat256(POWER_MANA)), applied);

        TryRestorePower(player, POWER_ENERGY, CalculateSecondaryTickAmount(entry, player->GetMaxPowerForCombat256(POWER_ENERGY), POWER_ENERGY), applied);
        TryRestorePower(player, POWER_RAGE, CalculateSecondaryTickAmount(entry, player->GetMaxPowerForCombat256(POWER_RAGE), POWER_RAGE), applied);
        TryRestorePower(player, POWER_RUNIC_POWER, CalculateSecondaryTickAmount(entry, player->GetMaxPowerForCombat256(POWER_RUNIC_POWER), POWER_RUNIC_POWER), applied);

        if (applied && IsDebugEnabled())
        {
            LOG_DEBUG("module", "回血神符：玩家 {} 触发 [id:{}] 等级:{}，模式:{}，效果:{}。",
                player->GetName(),
                entry.id,
                entry.healLevel,
                static_cast<uint32>(entry.healMode),
                BuildHealRuneValueText(entry));
        }
    }

    std::vector<HealRuneEntry> _entries;
    std::unordered_map<uint32, std::unordered_map<uint32, PlayerHealRuneState>> _playerRunes;
    std::unordered_map<uint32, uint32> _playerTickTimers;
};

void SendHealRunePayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= MAX_ADDON_PAYLOAD)
    {
        if (HermesBridge_SendAddonMessage(player, HEAL_RUNE_ADDON_PREFIX, payload))
            return;

        std::string fullMessage = std::string(HEAL_RUNE_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
        return;
    }

    size_t totalChunks = (payload.length() + MAX_ADDON_PAYLOAD - 1) / MAX_ADDON_PAYLOAD;
    for (size_t index = 0; index < totalChunks; ++index)
    {
        size_t start = index * MAX_ADDON_PAYLOAD;
        size_t length = std::min(MAX_ADDON_PAYLOAD, payload.length() - start);
        std::string chunk = payload.substr(start, length);

        std::ostringstream chunkMessage;
        chunkMessage << "CHUNK:" << (index + 1) << ":" << totalChunks << ":" << chunk;
        if (HermesBridge_SendAddonMessage(player, HEAL_RUNE_ADDON_PREFIX, chunkMessage.str()))
            continue;

        std::string fullMessage = std::string(HEAL_RUNE_ADDON_PREFIX) + '\t' + chunkMessage.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendHealRuneOpenUI(Player* player)
{
    SendHealRunePayload(player, "HR_OPEN");
}

void SendHealRuneListToPlayer(Player* player)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "HR_LIST:";

    HealRuneMgr* mgr = HealRuneMgr::Instance();
    std::vector<HealRuneEntry const*> entries = mgr->GetUiEntries(player);
    uint32 currentRuneId = mgr->GetHighestUnlockedRuneId(player);
    if (mgr->IsDebugEnabled())
    {
        LOG_INFO("server.loading", "[回血神符UI] 下发列表: 玩家={}, 当前神符ID={}, 下发条数={}, 总配置={}",
            player->GetName(), currentRuneId, static_cast<uint32>(entries.size()), static_cast<uint32>(mgr->GetEntries().size()));
    }

    bool first = true;
    for (HealRuneEntry const* entryPtr : entries)
    {
        if (!entryPtr)
            continue;

        HealRuneEntry const& entry = *entryPtr;
        if (!first)
            payload << '~';
        first = false;

        bool unlocked = mgr->IsPlayerUnlocked(player, entry.id);
        bool current = currentRuneId == entry.id;
        bool eligible = !unlocked && mgr->CanActivateEntry(player, entry, false, false);

        payload << entry.id << '^'
                << SanitizeAddonText(entry.name) << '^'
                << SanitizeAddonText(entry.description) << '^'
                << entry.healLevel << '^'
                << entry.requirementTemplateId << '^'
                << static_cast<uint32>(entry.healMode) << '^'
                << FormatHealValue(entry.healValue) << '^'
                << SanitizeAddonText(entry.requirementText) << '^'
                << (unlocked ? 1 : 0) << '^'
                << (current ? 1 : 0) << '^'
                << (eligible ? 1 : 0);
    }

    SendHealRunePayload(player, payload.str());
}

void SendHealRuneStateToPlayer(Player* player)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "HR_STATE:";

    uint32 currentRuneId = HealRuneMgr::Instance()->GetHighestUnlockedRuneId(player);
    std::vector<HealRuneEntry const*> entries = HealRuneMgr::Instance()->GetUiEntries(player);
    bool first = true;

    for (HealRuneEntry const* entryPtr : entries)
    {
        if (!entryPtr)
            continue;

        HealRuneEntry const& entry = *entryPtr;
        if (!first)
            payload << '~';
        first = false;

        bool unlocked = HealRuneMgr::Instance()->IsPlayerUnlocked(player, entry.id);
        bool current = currentRuneId == entry.id;
        bool eligible = !unlocked && HealRuneMgr::Instance()->CanActivateEntry(player, entry, false, false);

        payload << entry.id << '^'
                << (unlocked ? 1 : 0) << '^'
                << (current ? 1 : 0) << '^'
                << (eligible ? 1 : 0);
    }

    SendHealRunePayload(player, payload.str());
}

void SendHealRuneAllDataToPlayer(Player* player)
{
    if (!player)
        return;

    HealRuneMgr::Instance()->LoadPlayerData(player, true);
    SendHealRuneListToPlayer(player);
    SendHealRuneStateToPlayer(player);
}

void SendHealRuneActionResult(Player* player, char const* action, bool success, uint32 runeId, std::string const& message)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "HR_RESULT:"
            << (action ? action : "") << '^'
            << (success ? "OK" : "FAIL") << '^'
            << runeId << '^'
            << SanitizeAddonText(message);

    SendHealRunePayload(player, payload.str());
}

class HealRuneWorldScript : public WorldScript
{
public:
    HealRuneWorldScript() : WorldScript("HealRuneWorldScript") { }

    void OnAfterConfigLoad(bool reload) override
    {
        // 【reload 短路修复】理由同 cut-system：enable 缓存由 LoadEntries 刷新，
        // 先判 IsEnabled 会导致禁用后无法通过 reload 重新启用
        if (!reload)
            return;

        HealRuneMgr::Instance()->LoadEntries();
        LOG_INFO("module", "回血神符系统配置已重载。");
    }

    void OnStartup() override
    {
        if (!HealRuneMgr::Instance()->IsEnabled())
            return;

        _initialized = false;
        _loadTime = 0;
        _displayedLog = false;
    }

    void OnUpdate(uint32 diff) override
    {
        if (!HealRuneMgr::Instance()->IsEnabled() || _initialized)
            return;

        _loadTime += diff;

        if (_loadTime >= 3000 && !_displayedLog)
        {
            LOG_INFO("server.loading", "→回血神符系统√");
            _displayedLog = true;
        }

        if (_loadTime >= 3000 && _displayedLog)
        {
            HealRuneMgr::Instance()->LoadEntries();
            _initialized = true;
        }
    }

private:
    bool _initialized = false;
    uint32 _loadTime = 0;
    bool _displayedLog = false;
};

class HealRunePlayerScript : public PlayerScript
{
public:
    HealRunePlayerScript() : PlayerScript("HealRunePlayerScript",
    {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_ON_CHAT_WITH_RECEIVER,
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        if (!HealRuneMgr::Instance()->IsEnabled())
        {
            HealRuneMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
            return;
        }

        HealRuneMgr::Instance()->LoadPlayerData(player);
        HealRuneMgr::Instance()->ResetPlayerTick(player->GetGUID().GetCounter());
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        HealRuneMgr::Instance()->UnloadPlayerData(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        HealRuneMgr::Instance()->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        HealRuneMgr::Instance()->UpdatePlayer(player, diff);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!HealRuneMgr::Instance()->IsEnabled() || !player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != HEAL_RUNE_ADDON_PREFIX)
            return;

        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "HEALRUNE"))
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendHealRuneAllDataToPlayer(player);
            return;
        }

        if (command == "REQ_LIST")
        {
            HealRuneMgr::Instance()->LoadPlayerData(player, true);
            SendHealRuneListToPlayer(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            HealRuneMgr::Instance()->LoadPlayerData(player, true);
            SendHealRuneStateToPlayer(player);
            return;
        }

        if (command == "OPEN")
        {
            SendHealRuneOpenUI(player);
            return;
        }

        if (command == "ACT_ALL")
        {
            uint32 count = HealRuneMgr::Instance()->UnlockAllAvailableRunes(player, false);
            std::ostringstream message;
            message << "本次激活 " << count << " 个神符";
            SendHealRuneActionResult(player, "ACT_ALL", true, 0, message.str());
            SendHealRuneAllDataToPlayer(player);
            return;
        }

        if (command.rfind("ACT:", 0) == 0)
        {
            std::string args = command.substr(4);
            uint32 runeId = 0;

            try
            {
                runeId = static_cast<uint32>(std::stoul(args));
            }
            catch (...)
            {
                SendHealRuneActionResult(player, "ACTIVATE", false, 0, "参数错误");
                return;
            }

            if (!runeId)
            {
                SendHealRuneActionResult(player, "ACTIVATE", false, 0, "神符ID无效");
                return;
            }

            std::string failureMessage;
            bool success = HealRuneMgr::Instance()->UnlockRune(player, runeId, true, &failureMessage);
            SendHealRuneActionResult(player, "ACTIVATE", success, runeId, success ? "激活成功" : (failureMessage.empty() ? "激活失败" : failureMessage));
            SendHealRuneAllDataToPlayer(player);
            return;
        }
    }
};

class HealRuneCommandScript : public CommandScript
{
public:
    HealRuneCommandScript() : CommandScript("HealRuneCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable healRuneCommandTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("重载", HandleReloadCommand, SEC_ADMINISTRATOR, Acore::ChatCommands::Console::Yes),
            Acore::ChatCommands::ChatCommandBuilder("界面", HandleOpenUICommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("激活", HandleActivateCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("刷新", HandleRefreshCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("查看", HandleInfoCommand, SEC_PLAYER, Acore::ChatCommands::Console::No),
            Acore::ChatCommands::ChatCommandBuilder("列表", HandleListCommand, SEC_PLAYER, Acore::ChatCommands::Console::No)
        };

        static Acore::ChatCommands::ChatCommandTable commandTable =
        {
            Acore::ChatCommands::ChatCommandBuilder("回血神符", healRuneCommandTable)
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        HealRuneMgr::Instance()->LoadEntries();
        handler->SendSysMessage("回血神符系统配置已重新加载。");
        return true;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        SendHealRuneOpenUI(player);
        return true;
    }

    static bool HandleActivateCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::string argText = args ? args : "";
        if (argText.empty())
        {
            handler->SendSysMessage("语法: .回血神符 激活 <神符ID|全部>");
            return false;
        }

        if (argText == "全部" || argText == "all" || argText == "ALL")
        {
            HealRuneMgr::Instance()->UnlockAllAvailableRunes(player, true);
            return true;
        }

        uint32 runeId = static_cast<uint32>(std::strtoul(argText.c_str(), nullptr, 10));
        if (!runeId)
        {
            handler->SendSysMessage("回血神符：神符ID无效。");
            return false;
        }

        std::string failureMessage;
        if (!HealRuneMgr::Instance()->UnlockRune(player, runeId, true, &failureMessage))
        {
            handler->PSendSysMessage("回血神符：{}。", failureMessage.empty() ? "激活失败" : failureMessage);
            return false;
        }

        return true;
    }

    static bool HandleRefreshCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        HealRuneMgr::Instance()->LoadPlayerData(player);
        HealRuneEntry const* current = HealRuneMgr::Instance()->GetHighestUnlockedEntry(player);
        if (!current)
        {
            handler->SendSysMessage("回血神符：当前没有任何已激活神符。");
            return true;
        }

        handler->PSendSysMessage("回血神符：当前生效神符 {} [id:{}] 等级:{} 模式:{} 效果:{}",
            GetHealRuneDisplayName(*current),
            current->id,
            current->healLevel,
            BuildHealRuneModeText(current->healMode),
            BuildHealRuneValueText(*current));
        return true;
    }

    static bool HandleInfoCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        HealRuneEntry const* current = HealRuneMgr::Instance()->GetHighestUnlockedEntry(player);
        if (!current)
        {
            handler->SendSysMessage("回血神符：当前没有任何已激活神符。");
            return true;
        }

        handler->PSendSysMessage("回血神符：当前生效神符 {} [id:{}] 等级:{} 需求系统:{} 模式:{} 效果:{}",
            GetHealRuneDisplayName(*current),
            current->id,
            current->healLevel,
            current->requirementTemplateId,
            BuildHealRuneModeText(current->healMode),
            BuildHealRuneValueText(*current));
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<HealRuneEntry> const& entries = HealRuneMgr::Instance()->GetEntries();
        if (entries.empty())
        {
            handler->SendSysMessage("回血神符：当前没有任何配置。");
            return true;
        }

        handler->PSendSysMessage("回血神符共有 {} 条配置：", static_cast<uint32>(entries.size()));

        uint32 currentRuneId = HealRuneMgr::Instance()->GetHighestUnlockedRuneId(player);
        for (HealRuneEntry const& entry : entries)
        {
            char const* status = "未激活";
            if (currentRuneId == entry.id)
                status = "当前生效";
            else if (HealRuneMgr::Instance()->IsPlayerUnlocked(player, entry.id))
                status = "已激活";

            handler->PSendSysMessage("{} [id:{}] 等级:{} 需求:{} 模式:{} 效果:{} 状态:{}",
                GetHealRuneDisplayName(entry),
                entry.id,
                entry.healLevel,
                entry.requirementTemplateId,
                BuildHealRuneModeText(entry.healMode),
                BuildHealRuneValueText(entry),
                status);
        }

        return true;
    }
};
}

void AddSC_mod_heal_rune_system()
{
    new HealRuneWorldScript();
    new HealRunePlayerScript();
    new HealRuneCommandScript();
}

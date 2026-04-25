/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 */

#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr char PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX[] = "PATTRPANEL";

struct PlayerAttributePanelStatDef
{
    uint32 id;
    Stats stat;
};

constexpr PlayerAttributePanelStatDef PLAYER_ATTRIBUTE_PANEL_STATS[] =
{
    { 4, STAT_STRENGTH  },
    { 3, STAT_AGILITY   },
    { 7, STAT_STAMINA   },
    { 5, STAT_INTELLECT },
    { 6, STAT_SPIRIT    }
};

uint64 SaturateToUInt64(double value)
{
    if (std::isnan(value) || value <= 0.0)
        return 0;

    if (std::isinf(value) || value >= static_cast<double>(std::numeric_limits<uint64>::max()))
        return std::numeric_limits<uint64>::max();

    return static_cast<uint64>(value);
}

std::string BuildRangePayload(double minValue, double maxValue)
{
    std::ostringstream range;
    range << SaturateToUInt64(minValue) << '~' << SaturateToUInt64(maxValue);
    return range.str();
}

std::string ToPanelValue(int64 value)
{
    return std::to_string(value > 0 ? value : 0);
}

std::string ToPanelValue(uint64 value)
{
    return std::to_string(value);
}

int64 GetPanelStat(Player* player, Stats stat)
{
    int64 extendedValue = player->GetExtendedStat(stat);
    return extendedValue > 0 ? extendedValue : player->GetStat(stat);
}

int64 GetPanelCombatRating(Player* player, CombatRating combatRating)
{
    int64 extendedValue = player->GetExtendedCombatRating(combatRating);
    if (extendedValue > 0)
        return extendedValue;

    int32 fieldValue = player->GetInt32Value(static_cast<uint16>(PLAYER_FIELD_COMBAT_RATING_1) + combatRating);
    return fieldValue > 0 ? fieldValue : 0;
}

int64 GetPanelHealingBonus(Player* player)
{
    int64 extendedValue = player->GetExtendedHealingBonus();
    if (extendedValue > 0)
        return extendedValue;

    int32 fieldValue = player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS);
    return fieldValue > 0 ? fieldValue : 0;
}

void AddPanelStat(std::vector<std::string>& stats, uint32 id, std::string const& value)
{
    std::ostringstream stat;
    stat << id << '=' << value;
    stats.push_back(stat.str());
}

void SendPanelStats(Player* player, std::vector<std::string> const& stats)
{
    constexpr size_t MaxPayloadLength = 190;

    std::string payload = "STATS:";
    for (std::string const& stat : stats)
    {
        size_t separatorLength = payload == "STATS:" ? 0 : 1;
        if (payload.size() + separatorLength + stat.size() > MaxPayloadLength && payload != "STATS:")
        {
            std::string fullMessage = std::string(PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX) + '\t' + payload;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);

            payload = "STATS:";
            separatorLength = 0;
        }

        if (separatorLength)
            payload += '|';

        payload += stat;
    }

    if (payload != "STATS:")
    {
        std::string fullMessage = std::string(PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX) + '\t' + payload;

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendPlayerAttributePanelData(Player* player)
{
    if (!player || !player->GetSession())
        return;

    std::vector<std::string> stats;
    stats.reserve(48);

    stats.push_back(std::string("CUR_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    stats.push_back(std::string("CURRENT_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    AddPanelStat(stats, 1, ToPanelValue(player->GetExtendedMaxHealth()));
    AddPanelStat(stats, 0, ToPanelValue(player->GetExtendedMaxPower(POWER_MANA)));
    for (PlayerAttributePanelStatDef const& statDef : PLAYER_ATTRIBUTE_PANEL_STATS)
        AddPanelStat(stats, statDef.id, ToPanelValue(GetPanelStat(player, statDef.stat)));

    AddPanelStat(stats, 12, ToPanelValue(GetPanelCombatRating(player, CR_DEFENSE_SKILL)));
    AddPanelStat(stats, 13, ToPanelValue(GetPanelCombatRating(player, CR_DODGE)));
    AddPanelStat(stats, 14, ToPanelValue(GetPanelCombatRating(player, CR_PARRY)));
    AddPanelStat(stats, 15, ToPanelValue(GetPanelCombatRating(player, CR_BLOCK)));
    AddPanelStat(stats, 16, ToPanelValue(GetPanelCombatRating(player, CR_HIT_MELEE)));
    AddPanelStat(stats, 17, ToPanelValue(GetPanelCombatRating(player, CR_HIT_RANGED)));
    AddPanelStat(stats, 18, ToPanelValue(GetPanelCombatRating(player, CR_HIT_SPELL)));
    AddPanelStat(stats, 19, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_MELEE)));
    AddPanelStat(stats, 20, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_RANGED)));
    AddPanelStat(stats, 21, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_SPELL)));
    AddPanelStat(stats, 22, ToPanelValue(GetPanelCombatRating(player, CR_HIT_TAKEN_MELEE)));
    AddPanelStat(stats, 23, ToPanelValue(GetPanelCombatRating(player, CR_HIT_TAKEN_RANGED)));
    AddPanelStat(stats, 24, ToPanelValue(GetPanelCombatRating(player, CR_HIT_TAKEN_SPELL)));
    AddPanelStat(stats, 25, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_TAKEN_MELEE)));
    AddPanelStat(stats, 26, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_TAKEN_RANGED)));
    AddPanelStat(stats, 27, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_TAKEN_SPELL)));
    AddPanelStat(stats, 28, ToPanelValue(GetPanelCombatRating(player, CR_HASTE_MELEE)));
    AddPanelStat(stats, 29, ToPanelValue(GetPanelCombatRating(player, CR_HASTE_RANGED)));
    AddPanelStat(stats, 30, ToPanelValue(GetPanelCombatRating(player, CR_HASTE_SPELL)));
    AddPanelStat(stats, 31, ToPanelValue(GetPanelCombatRating(player, CR_HIT_MELEE)));
    AddPanelStat(stats, 32, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_MELEE)));
    AddPanelStat(stats, 33, ToPanelValue(GetPanelCombatRating(player, CR_HIT_TAKEN_MELEE)));
    AddPanelStat(stats, 34, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_TAKEN_MELEE)));
    AddPanelStat(stats, 35, ToPanelValue(GetPanelCombatRating(player, CR_CRIT_TAKEN_MELEE)));
    AddPanelStat(stats, 36, ToPanelValue(GetPanelCombatRating(player, CR_HASTE_MELEE)));
    AddPanelStat(stats, 37, ToPanelValue(GetPanelCombatRating(player, CR_EXPERTISE)));
    AddPanelStat(stats, 38, ToPanelValue(SaturateToUInt64(player->GetExtendedTotalAttackPowerValue(BASE_ATTACK))));
    AddPanelStat(stats, 39, ToPanelValue(SaturateToUInt64(player->GetExtendedTotalAttackPowerValue(RANGED_ATTACK))));
    AddPanelStat(stats, 41, ToPanelValue(GetPanelHealingBonus(player)));
    AddPanelStat(stats, 42, ToPanelValue(player->GetExtendedSpellDamageBonus()));
    AddPanelStat(stats, 43, "0");
    AddPanelStat(stats, 44, ToPanelValue(GetPanelCombatRating(player, CR_ARMOR_PENETRATION)));
    AddPanelStat(stats, 45, ToPanelValue(player->GetExtendedSpellPowerBonus()));
    AddPanelStat(stats, 46, "0");
    AddPanelStat(stats, 47, ToPanelValue(static_cast<int64>(player->GetInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE))));
    AddPanelStat(stats, 48, ToPanelValue(static_cast<int64>(player->GetShieldBlockValue())));
    stats.push_back(std::string("MAINHAND_DAMAGE=") + BuildRangePayload(player->GetExtendedDamageMin(BASE_ATTACK), player->GetExtendedDamageMax(BASE_ATTACK)));
    stats.push_back(std::string("OFFHAND_DAMAGE=") + (player->HasOffhandWeaponForAttack() ? BuildRangePayload(player->GetExtendedDamageMin(OFF_ATTACK), player->GetExtendedDamageMax(OFF_ATTACK)) : "0~0"));
    stats.push_back(std::string("RANGED_DAMAGE=") + BuildRangePayload(player->GetExtendedDamageMin(RANGED_ATTACK), player->GetExtendedDamageMax(RANGED_ATTACK)));

    SendPanelStats(player, stats);
}

void SendPlayerAttributePanelHealthData(Player* player)
{
    if (!player || !player->GetSession())
        return;

    std::vector<std::string> stats;
    stats.reserve(3);
    stats.push_back(std::string("CUR_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    stats.push_back(std::string("CURRENT_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    AddPanelStat(stats, 1, ToPanelValue(player->GetExtendedMaxHealth()));
    SendPanelStats(player, stats);
}

class PlayerAttributePanelPlayerScript : public PlayerScript
{
public:
    PlayerAttributePanelPlayerScript() : PlayerScript("PlayerAttributePanelPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        SendPlayerAttributePanelData(player);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_STATS" || command == "OPEN_PANEL")
            SendPlayerAttributePanelData(player);
    }

    void OnPlayerEquip(Player* player, Item* /*it*/, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        SendPlayerAttributePanelData(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player || !player->GetSession())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32& elapsed = _healthSyncElapsed[guid];
        elapsed += diff;
        if (elapsed < 250)
            return;

        elapsed = 0;

        uint64 currentHealth = player->GetExtendedHealth();
        uint64 maxHealth = player->GetExtendedMaxHealth();
        if (_lastHealth[guid] == currentHealth && _lastMaxHealth[guid] == maxHealth)
            return;

        _lastHealth[guid] = currentHealth;
        _lastMaxHealth[guid] = maxHealth;
        SendPlayerAttributePanelHealthData(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        _healthSyncElapsed.erase(guid);
        _lastHealth.erase(guid);
        _lastMaxHealth.erase(guid);
    }

private:
    std::unordered_map<uint32, uint32> _healthSyncElapsed;
    std::unordered_map<uint32, uint64> _lastHealth;
    std::unordered_map<uint32, uint64> _lastMaxHealth;
};
}

void AddSC_mod_player_attribute_panel()
{
    new PlayerAttributePanelPlayerScript();
}

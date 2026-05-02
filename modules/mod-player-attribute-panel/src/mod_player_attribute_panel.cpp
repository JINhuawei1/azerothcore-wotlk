/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 */

#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#if __has_include("HuanJingSystem.h")
    #ifndef MODULE_HUANJING_SYSTEM
        #define MODULE_HUANJING_SYSTEM
    #endif
    #include "HuanJingSystem.h"
#endif
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
constexpr uint32 MaxPanelClientResourceValue = 2000000000u;

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

uint64 SaturateToUInt64(long double value)
{
    if (std::isnan(value) || value <= 0.0L)
        return 0;

    long double maxValue = static_cast<long double>(std::numeric_limits<uint64>::max());
    if (std::isinf(value) || value >= maxValue)
        return std::numeric_limits<uint64>::max();

    return static_cast<uint64>(value);
}

float GetCreatureRankHealthModForPanel(uint32 rank)
{
    switch (rank)
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld->getRate(RATE_CREATURE_NORMAL_HP);
        case CREATURE_ELITE_ELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);
        case CREATURE_ELITE_RAREELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RAREELITE_HP);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld->getRate(RATE_CREATURE_ELITE_WORLDBOSS_HP);
        case CREATURE_ELITE_RARE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RARE_HP);
        default:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);
    }
}

uint64 GetCreatureTemplateMaxHealthForPanel(Creature* creature)
{
    if (!creature)
        return 0;

    CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
    if (!creatureTemplate)
        return 0;

    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), creatureTemplate->unit_class);
    if (!stats)
        return 0;

    uint64 baseHealth = stats->GenerateHealth(creatureTemplate);
    long double rankHealth = static_cast<long double>(baseHealth) * static_cast<long double>(GetCreatureRankHealthModForPanel(creatureTemplate->rank));
    return SaturateToUInt64(rankHealth);
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

uint32 ScalePanelCombatValueToClient(uint64 currentValue, uint64 maxValue, uint32 clientMaxValue)
{
    if (!currentValue || !maxValue)
        return 0;

    if (!clientMaxValue)
        return 1;

    if (currentValue >= maxValue)
        return clientMaxValue;

    if (maxValue <= clientMaxValue)
        return currentValue > clientMaxValue ? clientMaxValue : static_cast<uint32>(currentValue);

    long double scaled = (static_cast<long double>(clientMaxValue) * static_cast<long double>(currentValue)) / static_cast<long double>(maxValue);
    uint32 clientValue = static_cast<uint32>(scaled + 0.5L);
    if (!clientValue)
        return 1;

    return clientValue > clientMaxValue ? clientMaxValue : clientValue;
}

std::string SanitizePayloadValue(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch)
    {
        return ch < 32 || ch == '|' || ch == '=';
    }), value.end());

    if (value.size() > 32)
        value.resize(32);

    return value;
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

int64 GetPanelBaseSpellPowerBonus(Player* player)
{
    uint64 baseSpellPower = player->GetBaseSpellPowerBonus();
    if (baseSpellPower > static_cast<uint64>(std::numeric_limits<int64>::max()))
        return std::numeric_limits<int64>::max();

    return static_cast<int64>(baseSpellPower);
}

int64 GetPanelHealingBonus(Player* player)
{
    int64 extendedValue = player->GetExtendedHealingBonus();
    if (extendedValue > 0)
        return extendedValue;

    int32 fieldValue = player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS);
    if (fieldValue > 0)
        return fieldValue;

    return GetPanelBaseSpellPowerBonus(player);
}

int64 GetPanelSpellDamageBonus(Player* player)
{
    int64 extendedValue = player->GetExtendedSpellDamageBonus();
    if (extendedValue > 0)
        return extendedValue;

    int32 fieldValue = 0;
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        int32 schoolBonus = player->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i);
        if (schoolBonus > fieldValue)
            fieldValue = schoolBonus;
    }

    if (fieldValue > 0)
        return fieldValue;

    return GetPanelBaseSpellPowerBonus(player);
}

int64 GetPanelSpellPowerBonus(Player* player)
{
    int64 extendedValue = player->GetExtendedSpellPowerBonus();
    if (extendedValue > 0)
        return extendedValue;

    int32 fieldValue = player->GetInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS);
    if (fieldValue < 0)
        fieldValue = 0;

    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        int32 schoolBonus = player->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i);
        if (schoolBonus > fieldValue)
            fieldValue = schoolBonus;
    }

    if (fieldValue > 0)
        return fieldValue;

    return GetPanelBaseSpellPowerBonus(player);
}

int64 GetPanelSpellPenetration(Player* player)
{
    int32 itemMod = player->GetSpellPenetrationItemMod();
    if (itemMod > 0)
        return itemMod;

    int64 fieldValue = static_cast<int64>(player->GetInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE));
    if (fieldValue < 0)
        return -fieldValue;

    return fieldValue > 0 ? fieldValue : 0;
}

void AddPanelStat(std::vector<std::string>& stats, uint32 id, std::string const& value)
{
    std::ostringstream stat;
    stat << id << '=' << value;
    stats.push_back(stat.str());
}

void SendPanelMessage(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession() || payload.empty())
        return;

    std::string fullMessage = std::string(PLAYER_ATTRIBUTE_PANEL_ADDON_PREFIX) + '\t' + payload;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&data);
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
            SendPanelMessage(player, payload);

            payload = "STATS:";
            separatorLength = 0;
        }

        if (separatorLength)
            payload += '|';

        payload += stat;
    }

    if (payload != "STATS:")
        SendPanelMessage(player, payload);
}

void SendPlayerAttributePanelData(Player* player)
{
    if (!player || !player->GetSession())
        return;

    std::vector<std::string> stats;
    stats.reserve(50);

    stats.push_back(std::string("CUR_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    stats.push_back(std::string("CURRENT_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    stats.push_back(std::string("CUR_MANA=") + ToPanelValue(player->GetPowerForCombat(POWER_MANA)));
    stats.push_back(std::string("CURRENT_MANA=") + ToPanelValue(player->GetPowerForCombat(POWER_MANA)));
    AddPanelStat(stats, 1, ToPanelValue(player->GetExtendedMaxHealth()));
    AddPanelStat(stats, 0, ToPanelValue(player->GetExtendedMaxPower(POWER_MANA)));
    for (PlayerAttributePanelStatDef const& statDef : PLAYER_ATTRIBUTE_PANEL_STATS)
        AddPanelStat(stats, statDef.id, ToPanelValue(GetPanelStat(player, statDef.stat)));

    AddPanelStat(stats, 8, ToPanelValue(player->GetTrueDamageBonus()));
    AddPanelStat(stats, 9, ToPanelValue(player->GetCuttingDamageBonus()));
    AddPanelStat(stats, 10, ToPanelValue(player->GetCooldownReductionBonus()));
    AddPanelStat(stats, 11, ToPanelValue(player->GetSkillDamageBonus()));
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
    AddPanelStat(stats, 42, ToPanelValue(GetPanelSpellDamageBonus(player)));
    AddPanelStat(stats, 43, ToPanelValue(static_cast<uint64>(player->GetBaseManaRegenBonus())));
    AddPanelStat(stats, 44, ToPanelValue(GetPanelCombatRating(player, CR_ARMOR_PENETRATION)));
    AddPanelStat(stats, 45, ToPanelValue(GetPanelSpellPowerBonus(player)));
    AddPanelStat(stats, 46, ToPanelValue(static_cast<uint64>(player->GetBaseHealthRegenBonus())));
    AddPanelStat(stats, 47, ToPanelValue(GetPanelSpellPenetration(player)));
    AddPanelStat(stats, 48, ToPanelValue(static_cast<int64>(player->GetShieldBlockValue())));
    stats.push_back(std::string("MAINHAND_DAMAGE=") + BuildRangePayload(player->GetExtendedDamageMin(BASE_ATTACK), player->GetExtendedDamageMax(BASE_ATTACK)));
    stats.push_back(std::string("OFFHAND_DAMAGE=") + (player->HasOffhandWeaponForAttack() ? BuildRangePayload(player->GetExtendedDamageMin(OFF_ATTACK), player->GetExtendedDamageMax(OFF_ATTACK)) : "0~0"));
    stats.push_back(std::string("RANGED_DAMAGE=") + BuildRangePayload(player->GetExtendedDamageMin(RANGED_ATTACK), player->GetExtendedDamageMax(RANGED_ATTACK)));

    SendPanelStats(player, stats);
}

void SendPlayerAttributePanelResourceData(Player* player)
{
    if (!player || !player->GetSession())
        return;

    std::vector<std::string> stats;
    stats.reserve(6);
    stats.push_back(std::string("CUR_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    stats.push_back(std::string("CURRENT_HEALTH=") + ToPanelValue(player->GetExtendedHealth()));
    stats.push_back(std::string("CUR_MANA=") + ToPanelValue(player->GetPowerForCombat(POWER_MANA)));
    stats.push_back(std::string("CURRENT_MANA=") + ToPanelValue(player->GetPowerForCombat(POWER_MANA)));
    AddPanelStat(stats, 1, ToPanelValue(player->GetExtendedMaxHealth()));
    AddPanelStat(stats, 0, ToPanelValue(player->GetExtendedMaxPower(POWER_MANA)));
    SendPanelStats(player, stats);
}

uint64 GetPanelMaxMana(Unit* unit)
{
    if (!unit)
        return 0;

    return unit->GetMaxPowerForCombat(POWER_MANA);
}

void GetPanelTargetHealth(Player* player, Unit* target, uint64& currentHealth, uint64& maxHealth)
{
    currentHealth = target ? target->GetHealthForCombat() : 0;
    maxHealth = target ? target->GetMaxHealthForCombat() : 0;
    uint64 fieldCurrentHealth = currentHealth;
    uint64 fieldMaxHealth = maxHealth;

#ifdef MODULE_HUANJING_SYSTEM
    if (target && target->IsCreature() && sHuanJingSystem)
    {
        uint64 virtualCurrentHealth = 0;
        uint64 virtualMaxHealth = 0;
        if (sHuanJingSystem->GetCreatureVirtualHealth(target->GetGUID(), virtualCurrentHealth, virtualMaxHealth))
        {
            currentHealth = virtualCurrentHealth;
            maxHealth = virtualMaxHealth;
            return;
        }
    }
#endif

    if (target && target->IsCreature())
    {
        Creature* creature = target->ToCreature();
        uint64 templateMaxHealth = GetCreatureTemplateMaxHealthForPanel(creature);
        if (templateMaxHealth > maxHealth)
        {
            maxHealth = templateMaxHealth;
            if (fieldMaxHealth <= 1 || fieldCurrentHealth >= fieldMaxHealth)
            {
                currentHealth = maxHealth;
            }
            else if (fieldMaxHealth > 0)
            {
                long double scaledHealth = (static_cast<long double>(maxHealth) * static_cast<long double>(fieldCurrentHealth)) / static_cast<long double>(fieldMaxHealth);
                currentHealth = SaturateToUInt64(scaledHealth);
            }
        }
    }
}

void GetPanelTargetMana(Unit* target, uint64& currentMana, uint64& maxMana)
{
    currentMana = target ? target->GetPowerForCombat(POWER_MANA) : 0;
    maxMana = GetPanelMaxMana(target);

#ifdef MODULE_HUANJING_SYSTEM
    if (target && target->IsCreature() && sHuanJingSystem)
    {
        if (Creature* creature = target->ToCreature())
        {
            uint64 virtualCurrentMana = 0;
            uint64 virtualMaxMana = 0;
            if (sHuanJingSystem->GetCreatureVirtualMana(creature, virtualCurrentMana, virtualMaxMana))
            {
                currentMana = virtualCurrentMana;
                maxMana = virtualMaxMana;
            }
        }
    }
#endif
}

void SendPlayerAttributePanelTargetData(Player* player, std::string const& requestToken)
{
    if (!player || !player->GetSession())
        return;

    std::string payload = "TARGET:";
    std::string token = SanitizePayloadValue(requestToken);
    if (!token.empty())
        payload += "TOKEN=" + token + '|';

    Unit* target = player->GetSelectedUnit();
    if (!target)
    {
        payload += "NONE=1";
        SendPanelMessage(player, payload);
        return;
    }

    uint64 currentHealth = 0;
    uint64 maxHealth = 0;
    uint64 currentMana = 0;
    uint64 maxMana = 0;
    GetPanelTargetHealth(player, target, currentHealth, maxHealth);
    GetPanelTargetMana(target, currentMana, maxMana);

    payload += "GUID=" + std::to_string(target->GetGUID().GetRawValue());
    payload += "|CUR_HEALTH=" + ToPanelValue(currentHealth);
    payload += "|MAX_HEALTH=" + ToPanelValue(maxHealth);
    payload += "|CUR_MANA=" + ToPanelValue(currentMana);
    payload += "|MAX_MANA=" + ToPanelValue(maxMana);
    payload += "|POWER_TYPE=" + std::to_string(static_cast<uint32>(target->getPowerType()));

    SendPanelMessage(player, payload);
}

std::string ExtractTargetRequestToken(std::string const& command)
{
    std::string const prefix = "REQ_TARGET:";
    if (command.compare(0, prefix.size(), prefix) != 0)
        return "";

    return command.substr(prefix.size());
}

void SendLargeDamageTextAck(Player* player, bool enabled)
{
    if (!player || !player->GetSession())
        return;

    SendPanelMessage(player, std::string("LDT_ACK:") + (enabled ? "1" : "0"));
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
        if (command == "LDT_READY")
        {
            if (WorldSession* session = player->GetSession())
                session->SetLargeDamageTextAddonEnabled(true);
            SendLargeDamageTextAck(player, true);
            return;
        }

        if (command == "LDT_OFF")
        {
            if (WorldSession* session = player->GetSession())
                session->SetLargeDamageTextAddonEnabled(false);
            SendLargeDamageTextAck(player, false);
            return;
        }

        if (command == "REQ_STATS" || command == "OPEN_PANEL")
            SendPlayerAttributePanelData(player);

        if (command == "REQ_TARGET" || command.compare(0, std::string("REQ_TARGET:").size(), "REQ_TARGET:") == 0)
            SendPlayerAttributePanelTargetData(player, ExtractTargetRequestToken(command));
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

        bool clientResourceSynced = SyncPlayerClientResourceFields(player);

        uint64 currentHealth = player->GetExtendedHealth();
        uint64 maxHealth = player->GetExtendedMaxHealth();
        uint64 currentMana = player->GetPowerForCombat(POWER_MANA);
        uint64 maxMana = player->GetExtendedMaxPower(POWER_MANA);
        if (!clientResourceSynced && _lastHealth[guid] == currentHealth && _lastMaxHealth[guid] == maxHealth && _lastMana[guid] == currentMana && _lastMaxMana[guid] == maxMana)
            return;

        _lastHealth[guid] = currentHealth;
        _lastMaxHealth[guid] = maxHealth;
        _lastMana[guid] = currentMana;
        _lastMaxMana[guid] = maxMana;
        SendPlayerAttributePanelResourceData(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        _healthSyncElapsed.erase(guid);
        _lastHealth.erase(guid);
        _lastMaxHealth.erase(guid);
        _lastMana.erase(guid);
        _lastMaxMana.erase(guid);
    }

private:
    bool SyncPlayerClientResourceFields(Player* player)
    {
        bool synced = false;

        uint64 extendedHealth = player->GetExtendedHealth();
        uint64 extendedMaxHealth = player->GetExtendedMaxHealth();
        uint32 clientMaxHealth = player->GetMaxHealth();
        if (clientMaxHealth > MaxPanelClientResourceValue)
        {
            player->SetMaxHealth(MaxPanelClientResourceValue);
            clientMaxHealth = player->GetMaxHealth();
            player->SyncClientHealthFromExtended();
            synced = true;
        }

        if ((player->HasExtendedHealthForCombat() || extendedMaxHealth > clientMaxHealth) && clientMaxHealth > 0)
        {
            uint32 expectedClientHealth = ScalePanelCombatValueToClient(extendedHealth, extendedMaxHealth, clientMaxHealth);
            uint32 actualClientHealth = player->GetHealth();
            if (actualClientHealth != expectedClientHealth)
            {
                player->SyncClientHealthFromExtended();
                synced = true;
            }
        }

        uint64 extendedMana = player->GetPowerForCombat(POWER_MANA);
        uint64 extendedMaxMana = player->GetExtendedMaxPower(POWER_MANA);
        uint32 clientMaxMana = player->GetMaxPower(POWER_MANA);
        if (clientMaxMana > MaxPanelClientResourceValue)
        {
            player->SetMaxPower(POWER_MANA, MaxPanelClientResourceValue);
            clientMaxMana = player->GetMaxPower(POWER_MANA);
            player->SyncClientPowerFromExtended(POWER_MANA);
            synced = true;
        }

        if ((player->HasExtendedPowerForCombat(POWER_MANA) || extendedMaxMana > clientMaxMana) && clientMaxMana > 0)
        {
            uint32 expectedClientMana = ScalePanelCombatValueToClient(extendedMana, extendedMaxMana, clientMaxMana);
            uint32 actualClientMana = player->GetPower(POWER_MANA);
            if (actualClientMana != expectedClientMana)
            {
                player->SyncClientPowerFromExtended(POWER_MANA);
                synced = true;
            }
        }

        return synced;
    }

    std::unordered_map<uint32, uint32> _healthSyncElapsed;
    std::unordered_map<uint32, uint64> _lastHealth;
    std::unordered_map<uint32, uint64> _lastMaxHealth;
    std::unordered_map<uint32, uint64> _lastMana;
    std::unordered_map<uint32, uint64> _lastMaxMana;
};
}

void AddSC_mod_player_attribute_panel()
{
    new PlayerAttributePanelPlayerScript();
}

/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Chat.h"
#include "CommandScript.h"
#include "Language.h"
#include "Player.h"
#include "StringConvert.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;

class gear_commandscript : public CommandScript
{
public:
    gear_commandscript() : CommandScript("gear_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable gearCommandTable =
        {
            { "repair",  HandleGearRepairCommand, SEC_GAMEMASTER, Console::No },
            { "stats",   HandleGearStatsCommand,  SEC_PLAYER,     Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "gear", gearCommandTable }
        };

        return commandTable;
    }

    static bool HandleGearRepairCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        if (!target)
        {
            target = PlayerIdentifier::FromTargetOrSelf(handler);
        }

        if (!target || !target->IsConnected())
        {
            return false;
        }

        // check online security
        if (handler->HasLowerSecurity(target->GetConnectedPlayer()))
        {
            return false;
        }

        // Repair items
        target->GetConnectedPlayer()->DurabilityRepairAll(false, 0, false);

        std::string nameLink = handler->playerLink(target->GetName());

        handler->PSendSysMessage(LANG_YOU_REPAIR_ITEMS, nameLink);

        if (handler->needReportToTarget(target->GetConnectedPlayer()))
        {
            ChatHandler(target->GetConnectedPlayer()->GetSession()).PSendSysMessage(LANG_YOUR_ITEMS_REPAIRED, nameLink);
        }

        return true;
    }

    static bool HandleGearStatsCommand(ChatHandler* handler)
    {
        Player* player = handler->getSelectedPlayerOrSelf();

        if (!player)
        {
            return false;
        }

        handler->PSendSysMessage("Character: {}", player->GetPlayerName());
        handler->PSendSysMessage("Current equipment average item level: |cff00ffff{}|r", (int16)player->GetAverageItemLevel());

        if (sWorld->getIntConfig(CONFIG_MIN_LEVEL_STAT_SAVE))
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_STATS);
            stmt->SetData(0, player->GetGUID().GetCounter());
            PreparedQueryResult result = CharacterDatabase.Query(stmt);

            if (result)
            {
                Field* fields = result->Fetch();
                auto toString = [](uint256 const& value) { return Acore::ToString(value); };

                uint256 MaxHealth = fields[0].GetUInt256();
                uint256 Strength = fields[1].GetUInt256();
                uint256 Agility = fields[2].GetUInt256();
                uint256 Stamina = fields[3].GetUInt256();
                uint256 Intellect = fields[4].GetUInt256();
                uint256 Spirit = fields[5].GetUInt256();
                uint256 Armor = fields[6].GetUInt256();
                uint256 AttackPower = fields[7].GetUInt256();
                uint256 SpellPower = fields[8].GetUInt256();
                uint256 Resilience = fields[9].GetUInt256();

                handler->PSendSysMessage("Health: |cff00ffff{}|r - Stamina: |cff00ffff{}|r", toString(MaxHealth), toString(Stamina));
                handler->PSendSysMessage("Strength: |cff00ffff{}|r - Agility: |cff00ffff{}|r", toString(Strength), toString(Agility));
                handler->PSendSysMessage("Intellect: |cff00ffff{}|r - Spirit: |cff00ffff{}|r", toString(Intellect), toString(Spirit));
                handler->PSendSysMessage("AttackPower: |cff00ffff{}|r - SpellPower: |cff00ffff{}|r", toString(AttackPower), toString(SpellPower));
                handler->PSendSysMessage("Armor: |cff00ffff{}|r - Resilience: |cff00ffff{}|r", toString(Armor), toString(Resilience));
            }
        }

        return true;
    }
};

void AddSC_gear_commandscript()
{
    new gear_commandscript();
}

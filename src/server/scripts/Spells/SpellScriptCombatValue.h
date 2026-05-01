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

#ifndef SPELL_SCRIPT_COMBAT_VALUE_H
#define SPELL_SCRIPT_COMBAT_VALUE_H

#include "Player.h"
#include "Unit.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace SpellScriptCombat
{
    constexpr int32 MaxClientSpellValue = 1999999999;

    inline bool IsFinite(long double value)
    {
        return std::isfinite(value);
    }

    inline int32 ToInt32Saturated(long double value)
    {
        if (!IsFinite(value))
            return 0;

        if (value >= static_cast<long double>(std::numeric_limits<int32>::max()))
            return std::numeric_limits<int32>::max();

        if (value <= static_cast<long double>(std::numeric_limits<int32>::min()))
            return std::numeric_limits<int32>::min();

        return static_cast<int32>(value);
    }

    inline int32 ToPositiveInt32Saturated(long double value)
    {
        if (!IsFinite(value) || value <= 0.0L)
            return 0;

        if (value >= static_cast<long double>(std::numeric_limits<int32>::max()))
            return std::numeric_limits<int32>::max();

        return static_cast<int32>(value);
    }

    inline int32 ToClientSpellValue(long double value)
    {
        if (!IsFinite(value))
            return 0;

        if (value >= static_cast<long double>(MaxClientSpellValue))
            return MaxClientSpellValue;

        if (value <= static_cast<long double>(std::numeric_limits<int32>::min()))
            return std::numeric_limits<int32>::min();

        return static_cast<int32>(value);
    }

    inline int64 ToInt64Saturated(long double value)
    {
        if (!IsFinite(value))
            return 0;

        if (value >= static_cast<long double>(std::numeric_limits<int64>::max()))
            return std::numeric_limits<int64>::max();

        if (value <= static_cast<long double>(std::numeric_limits<int64>::min()))
            return std::numeric_limits<int64>::min();

        return static_cast<int64>(value);
    }

    inline uint64 ToUInt64Saturated(long double value)
    {
        if (!IsFinite(value) || value <= 0.0L)
            return 0;

        if (value >= static_cast<long double>(std::numeric_limits<uint64>::max()))
            return std::numeric_limits<uint64>::max();

        return static_cast<uint64>(value);
    }

    inline long double PercentOf(long double base, long double pct)
    {
        return base * pct / 100.0L;
    }

    inline uint64 CalculatePctUInt64(uint64 base, long double pct)
    {
        return ToUInt64Saturated(PercentOf(static_cast<long double>(base), pct));
    }

    inline int32 CalculatePctInt32Saturated(uint64 base, long double pct)
    {
        return ToPositiveInt32Saturated(PercentOf(static_cast<long double>(base), pct));
    }

    inline int32 CalculatePctClientSpellValue(uint64 base, long double pct)
    {
        return ToClientSpellValue(PercentOf(static_cast<long double>(base), pct));
    }

    inline int64 AddPctInt64Saturated(int64 base, long double pct)
    {
        return ToInt64Saturated(static_cast<long double>(base) + PercentOf(static_cast<long double>(base), pct));
    }

    inline int32 AddPctClientSpellValue(int32 base, long double pct)
    {
        return ToClientSpellValue(static_cast<long double>(base) + PercentOf(static_cast<long double>(base), pct));
    }

    inline long double GetStat(Unit* unit, Stats stat)
    {
        if (!unit)
            return 0.0L;

        if (Player* player = unit->ToPlayer())
        {
            int64 extended = player->GetExtendedStat(stat);
            if (extended > 0)
                return static_cast<long double>(extended);
        }

        float value = unit->GetStat(stat);
        return value > 0.0f ? static_cast<long double>(value) : 0.0L;
    }

    inline long double GetAttackPower(Unit* unit, WeaponAttackType attType)
    {
        if (!unit)
            return 0.0L;

        if (Player* player = unit->ToPlayer())
        {
            double extended = player->GetExtendedTotalAttackPowerValue(attType);
            if (extended > 0.0)
                return static_cast<long double>(extended);
        }

        float value = unit->GetTotalAttackPowerValue(attType);
        return value > 0.0f ? static_cast<long double>(value) : 0.0L;
    }

    inline long double GetSpellDamageBonus(Unit* unit, SpellSchoolMask schoolMask)
    {
        if (!unit)
            return 0.0L;

        if (Player* player = unit->ToPlayer())
        {
            int64 extended = player->GetExtendedSpellDamageBonus(schoolMask);
            if (extended > 0)
                return static_cast<long double>(extended);
        }

        int32 value = unit->SpellBaseDamageBonusDone(schoolMask);
        return value > 0 ? static_cast<long double>(value) : 0.0L;
    }

    inline long double GetSpellHealingBonus(Unit* unit, SpellSchoolMask schoolMask)
    {
        if (!unit)
            return 0.0L;

        if (Player* player = unit->ToPlayer())
        {
            int64 extended = player->GetExtendedHealingBonus();
            if (extended > 0)
                return static_cast<long double>(extended);
        }

        int32 value = unit->SpellBaseHealingBonusDone(schoolMask);
        return value > 0 ? static_cast<long double>(value) : 0.0L;
    }
}

#endif

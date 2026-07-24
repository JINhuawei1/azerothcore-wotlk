/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MOD_XIANMEN_SYSTEM_XIANQI_SOUL_DEBT_TIMERS_H
#define MOD_XIANMEN_SYSTEM_XIANQI_SOUL_DEBT_TIMERS_H

#include "Define.h"

#include <map>
#include <vector>

namespace Xianqi
{
template <typename Key, typename Compare, typename Allocator>
std::vector<Key> ExtractExpiredSoulDebts(std::map<Key, uint32, Compare, Allocator>& debts, uint32 diff)
{
    std::vector<Key> expired;
    for (auto itr = debts.begin(); itr != debts.end();)
    {
        if (itr->second > diff)
        {
            itr->second -= diff;
            ++itr;
            continue;
        }

        expired.push_back(itr->first);
        itr = debts.erase(itr);
    }

    return expired;
}
}

#endif

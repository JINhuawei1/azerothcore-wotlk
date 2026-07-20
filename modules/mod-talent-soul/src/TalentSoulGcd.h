/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 */

#ifndef TALENT_SOUL_GCD_H
#define TALENT_SOUL_GCD_H

#include "Define.h"
#include <algorithm>

struct TalentSoulGcdAdjustment
{
    int32 serverDurationMs = 0;
    bool clearClientCooldown = false;
};

inline TalentSoulGcdAdjustment CalculateTalentSoulGcdAdjustment(int32 originalDurationMs, float reductionPercent)
{
    if (originalDurationMs <= 0)
        return {};

    float const clampedReduction = std::clamp(reductionPercent, 0.0f, 100.0f);
    int32 const reducedAmount = static_cast<int32>(originalDurationMs * clampedReduction / 100.0f);
    int32 const serverDurationMs = std::max<int32>(originalDurationMs - reducedAmount, 0);
    return { serverDurationMs, serverDurationMs < 500 };
}

#endif // TALENT_SOUL_GCD_H

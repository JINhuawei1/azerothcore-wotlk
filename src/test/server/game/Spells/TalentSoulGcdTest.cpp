/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 */

#include "TalentSoulGcd.h"
#include "gtest/gtest.h"

TEST(TalentSoulGcdTest, KeepsReducedServerCooldownWhenClientVisualIsCleared)
{
    TalentSoulGcdAdjustment adjustment = CalculateTalentSoulGcdAdjustment(1500, 99.0f);

    EXPECT_EQ(15, adjustment.serverDurationMs);
    EXPECT_TRUE(adjustment.clearClientCooldown);
}

TEST(TalentSoulGcdTest, ClampsReductionToValidRange)
{
    EXPECT_EQ(1500, CalculateTalentSoulGcdAdjustment(1500, -1.0f).serverDurationMs);
    EXPECT_EQ(0, CalculateTalentSoulGcdAdjustment(1500, 101.0f).serverDurationMs);
    EXPECT_EQ(0, CalculateTalentSoulGcdAdjustment(0, 99.0f).serverDurationMs);
}

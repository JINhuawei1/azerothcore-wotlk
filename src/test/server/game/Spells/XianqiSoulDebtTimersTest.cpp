/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "XianqiSoulDebtTimers.h"
#include <gtest/gtest.h>

#include <map>

TEST(XianqiSoulDebtTimersTest, ExtractsAllExpiredDebtsBeforeReentrantProcessing)
{
    std::map<uint32, uint32> debts = {
        { 10, 50 },
        { 20, 100 },
        { 30, 150 }
    };

    std::vector<uint32> expired = Xianqi::ExtractExpiredSoulDebts(debts, 100);

    ASSERT_EQ(2, expired.size());
    EXPECT_EQ(10, expired[0]);
    EXPECT_EQ(20, expired[1]);
    EXPECT_FALSE(debts.contains(10));
    EXPECT_FALSE(debts.contains(20));
    ASSERT_TRUE(debts.contains(30));
    EXPECT_EQ(50, debts.at(30));

    // Damage callbacks may erase other debts. No map iterator remains live here.
    debts.erase(30);
    EXPECT_TRUE(debts.empty());
}

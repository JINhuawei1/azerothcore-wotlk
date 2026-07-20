/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 */

#include "MagicHitBatch.h"
#include "gtest/gtest.h"
#include <limits>
#include <string>

TEST(MagicHitPendingBatchesTest, SchedulesOnlyOnceForTheSamePlayerSpellAndTarget)
{
    MagicHitPendingBatches pending;
    MagicHitBatchKey key{ 30, 123456, 42833 };

    EXPECT_TRUE(pending.Accumulate(key, uint256(100), false, 42833));
    EXPECT_FALSE(pending.Accumulate(key, uint256(150), false, 42833));
    EXPECT_FALSE(pending.Accumulate(key, uint256(25), true, 42833));
    EXPECT_EQ(1, pending.Size());

    std::optional<MagicHitPendingBatch> batch = pending.Take(key);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(2, batch->direct.triggerCount);
    EXPECT_EQ(uint256(250), batch->direct.totalBaseDamage);
    EXPECT_EQ(1, batch->periodic.triggerCount);
    EXPECT_EQ(uint256(25), batch->periodic.totalBaseDamage);
    EXPECT_EQ(0, pending.Size());
}

TEST(MagicHitPendingBatchesTest, KeepsDifferentTargetsInSeparateBatches)
{
    MagicHitPendingBatches pending;

    EXPECT_TRUE(pending.Accumulate({ 30, 1001, 53385 }, uint256(10), false, 0));
    EXPECT_TRUE(pending.Accumulate({ 30, 1002, 53385 }, uint256(20), false, 0));
    EXPECT_EQ(2, pending.Size());

    pending.ErasePlayer(30);
    EXPECT_EQ(0, pending.Size());
}

TEST(MagicHitBatchDamageTest, AggregatedDamageEqualsIndividualTriggerDamage)
{
    MagicHitPendingPart part;
    part.Accumulate(uint256(100), 42833);
    part.Accumulate(uint256(150), 42833);

    uint32 const hitsPerTrigger = 2000;
    uint64 const bonusDamage = 1998000;
    MagicHitBatchDamage totals = CalculateMagicHitBatchDamage(part, hitsPerTrigger, bonusDamage);
    uint256 expected = (uint256(100) + bonusDamage) * hitsPerTrigger;
    expected += (uint256(150) + bonusDamage) * hitsPerTrigger;

    EXPECT_EQ(4000, totals.logicalHitCount);
    EXPECT_EQ(expected, totals.plannedDamage);
}

TEST(MagicHitBatchDamageTest, SaturatesCountersAndDamageOnOverflow)
{
    MagicHitPendingPart part;
    part.triggerCount = std::numeric_limits<uint64>::max();
    part.totalBaseDamage = std::numeric_limits<uint256>::max();

    MagicHitBatchDamage totals = CalculateMagicHitBatchDamage(part, 2, std::numeric_limits<uint64>::max());

    EXPECT_EQ(std::numeric_limits<uint64>::max(), totals.logicalHitCount);
    EXPECT_EQ(std::numeric_limits<uint256>::max(), totals.plannedDamage);
}

TEST(MagicHitBatchDamageTest, VisualCastsDoNotConsumeBonusDamageFromTheDirectRemainder)
{
    MagicHitPendingPart part;
    part.Accumulate(uint256(100), 42833);
    part.Accumulate(uint256(150), 42833);

    MagicHitBatchDamage totals = CalculateMagicHitBatchDamage(part, 2000, 1998000);
    uint256 directRemainder = CalculateMagicHitCastDirectDamage(totals.plannedDamage, part, 3);
    uint256 visualBaseDamageCredit = (part.totalBaseDamage / part.triggerCount) * 3;

    EXPECT_EQ(totals.plannedDamage, directRemainder + visualBaseDamageCredit);
    EXPECT_GT(directRemainder, totals.plannedDamage - (uint256(1998000) + 125) * 3);
}

TEST(MagicHitPendingCountTest, AccumulatesConcurrentProjectileHits)
{
    uint32 pendingCount = 1;

    pendingCount = AccumulateMagicHitPendingCount(pendingCount, 1);
    pendingCount = AccumulateMagicHitPendingCount(pendingCount, 1);

    EXPECT_EQ(3, pendingCount);
}

TEST(MagicHitPendingCountTest, SaturatesInsteadOfWrapping)
{
    EXPECT_EQ(std::numeric_limits<uint32>::max(),
        AccumulateMagicHitPendingCount(std::numeric_limits<uint32>::max(), 1));
}

TEST(MagicHitDamageHookTest, FinalDamageHookSkipsPeriodicDamageHandledByAuraHook)
{
    EXPECT_TRUE(ShouldQueueMagicHitFromFinalDamageHook(false));
    EXPECT_FALSE(ShouldQueueMagicHitFromFinalDamageHook(true));
}

TEST(MagicHitConfigReloadTest, KeepsPointersIntoPreviousConfigGenerationAlive)
{
    std::unordered_map<uint32, std::string> current{ { 1, "old" } };
    std::unordered_map<uint32, std::string> replacement{ { 1, "new" } };
    std::vector<std::unordered_map<uint32, std::string>> retired;
    std::string const* oldConfig = &current.at(1);

    ReplaceMagicHitConfigMap(current, replacement, retired);

    EXPECT_EQ("new", current.at(1));
    EXPECT_EQ("old", *oldConfig);
    EXPECT_EQ(1, retired.size());
}

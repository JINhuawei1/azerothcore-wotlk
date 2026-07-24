/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 */

#include "PlayerCastRateLimiter.h"
#include "gtest/gtest.h"

TEST(PlayerCastRateLimiterTest, AllowsOnlyConfiguredBurstAtTheSameInstant)
{
    PlayerCastRateLimiter limiter(20, 4);

    EXPECT_TRUE(limiter.TryConsume(0));
    EXPECT_TRUE(limiter.TryConsume(0));
    EXPECT_TRUE(limiter.TryConsume(0));
    EXPECT_TRUE(limiter.TryConsume(0));
    EXPECT_FALSE(limiter.TryConsume(0));
}

TEST(PlayerCastRateLimiterTest, RefillsAtTwentyTokensPerSecond)
{
    PlayerCastRateLimiter limiter(20, 4);
    for (uint32 i = 0; i < 4; ++i)
        ASSERT_TRUE(limiter.TryConsume(0));

    EXPECT_FALSE(limiter.TryConsume(49));
    EXPECT_TRUE(limiter.TryConsume(50));
    EXPECT_FALSE(limiter.TryConsume(50));

    EXPECT_TRUE(limiter.TryConsume(250));
    EXPECT_TRUE(limiter.TryConsume(250));
    EXPECT_TRUE(limiter.TryConsume(250));
    EXPECT_TRUE(limiter.TryConsume(250));
    EXPECT_FALSE(limiter.TryConsume(250));
}

TEST(PlayerCastRateLimiterTest, MovingClockBackwardDoesNotRefillTokens)
{
    PlayerCastRateLimiter limiter(20, 4);
    for (uint32 i = 0; i < 4; ++i)
        ASSERT_TRUE(limiter.TryConsume(1000));

    EXPECT_FALSE(limiter.TryConsume(900));
}

TEST(PlayerCastRateLimiterTest, QueuedReplayDoesNotConsumeAnotherToken)
{
    PlayerCastRateLimiter limiter(20, 4);
    for (uint32 i = 0; i < 4; ++i)
        ASSERT_TRUE(limiter.TryConsume(0, PlayerCastRequestSource::ClientPacket));

    EXPECT_TRUE(limiter.TryConsume(0, PlayerCastRequestSource::QueuedReplay));
    EXPECT_TRUE(limiter.TryConsume(0, PlayerCastRequestSource::QueuedReplay));
    EXPECT_FALSE(limiter.TryConsume(0, PlayerCastRequestSource::ClientPacket));
}

TEST(PlayerCastRateLimiterTest, ResetRestoresBurstForTheNextPlayer)
{
    PlayerCastRateLimiter limiter(20, 4);
    for (uint32 i = 0; i < 4; ++i)
        ASSERT_TRUE(limiter.TryConsume(0));
    ASSERT_FALSE(limiter.TryConsume(0));

    limiter.Reset();

    for (uint32 i = 0; i < 4; ++i)
        EXPECT_TRUE(limiter.TryConsume(0));
    EXPECT_FALSE(limiter.TryConsume(0));
}

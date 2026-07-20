/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 */

#ifndef ACORE_PLAYER_CAST_RATE_LIMITER_H
#define ACORE_PLAYER_CAST_RATE_LIMITER_H

#include "Define.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

enum class PlayerCastRequestSource
{
    ClientPacket,
    QueuedReplay
};

struct PlayerCastRateLimitLogSnapshot
{
    std::string playerName;
    uint32 playerGuid = 0;
    uint32 spellId = 0;
    uint32 dropped = 0;
};

class PlayerCastRateLimitLogAccumulator
{
public:
    void Record(std::string_view playerName, uint32 playerGuid, uint32 spellId)
    {
        _snapshot.playerName = playerName;
        _snapshot.playerGuid = playerGuid;
        _snapshot.spellId = spellId;
        if (_snapshot.dropped < std::numeric_limits<uint32>::max())
            ++_snapshot.dropped;
    }

    std::optional<PlayerCastRateLimitLogSnapshot> Take()
    {
        if (_snapshot.dropped == 0)
            return std::nullopt;

        PlayerCastRateLimitLogSnapshot snapshot = std::move(_snapshot);
        _snapshot = {};
        return snapshot;
    }

    void Reset() { _snapshot = {}; }

private:
    PlayerCastRateLimitLogSnapshot _snapshot;
};

class PlayerCastRateLimiter
{
public:
    explicit PlayerCastRateLimiter(uint32 tokensPerSecond = 20, uint32 burstCapacity = 4)
        : _tokensPerSecond(tokensPerSecond),
          _capacityTokenMillis(static_cast<uint64>(burstCapacity) * TOKEN_SCALE),
          _availableTokenMillis(_capacityTokenMillis)
    {
    }

    bool TryConsume(uint64 nowMs, PlayerCastRequestSource source = PlayerCastRequestSource::ClientPacket)
    {
        if (source == PlayerCastRequestSource::QueuedReplay)
            return true;

        if (!_initialized)
        {
            _lastRefillMs = nowMs;
            _initialized = true;
        }
        else if (nowMs > _lastRefillMs)
        {
            uint64 const elapsedMs = nowMs - _lastRefillMs;
            uint64 refill = std::numeric_limits<uint64>::max();
            if (_tokensPerSecond == 0 || elapsedMs <= std::numeric_limits<uint64>::max() / _tokensPerSecond)
                refill = elapsedMs * _tokensPerSecond;

            _availableTokenMillis = refill >= _capacityTokenMillis - _availableTokenMillis
                ? _capacityTokenMillis
                : _availableTokenMillis + refill;
            _lastRefillMs = nowMs;
        }

        if (_availableTokenMillis < TOKEN_SCALE)
            return false;

        _availableTokenMillis -= TOKEN_SCALE;
        return true;
    }

    void Reset()
    {
        _availableTokenMillis = _capacityTokenMillis;
        _lastRefillMs = 0;
        _initialized = false;
    }

private:
    static constexpr uint64 TOKEN_SCALE = 1000;

    uint64 _tokensPerSecond;
    uint64 _capacityTokenMillis;
    uint64 _availableTokenMillis;
    uint64 _lastRefillMs = 0;
    bool _initialized = false;
};

#endif // ACORE_PLAYER_CAST_RATE_LIMITER_H

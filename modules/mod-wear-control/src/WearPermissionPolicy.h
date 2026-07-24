#ifndef WEAR_PERMISSION_POLICY_H
#define WEAR_PERMISSION_POLICY_H

#include <cstdint>
#include <map>

namespace WearControl
{
using PermissionKey = std::uint16_t;
using PermissionLevels = std::map<PermissionKey, std::uint32_t>;

constexpr PermissionKey MakePermissionKey(std::uint8_t limitType, std::uint8_t slotPosition)
{
    return static_cast<PermissionKey>((static_cast<PermissionKey>(limitType) << 8) | slotPosition);
}

inline std::uint32_t ResolvePermissionLevel(PermissionLevels const& levels, std::uint8_t limitType,
    std::uint8_t slotPosition, std::uint32_t defaultLevel)
{
    auto itr = levels.find(MakePermissionKey(limitType, slotPosition));
    return itr == levels.end() ? defaultLevel : itr->second;
}

constexpr bool CanAdvancePermissionLevel(std::uint32_t currentLevel, std::uint32_t requestedLevel)
{
    return requestedLevel <= currentLevel || requestedLevel - currentLevel == 1;
}
}

#endif // WEAR_PERMISSION_POLICY_H

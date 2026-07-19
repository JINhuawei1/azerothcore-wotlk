#ifndef MODULE_ITEM_IDENTIFICATION_SCROLL_POLICY_H
#define MODULE_ITEM_IDENTIFICATION_SCROLL_POLICY_H

#include <cstdint>
#include <string>

enum class IdentificationScrollType : std::uint8_t
{
    Identify = 1,
    Cleanup = 2
};

struct IdentificationScrollTargetState
{
    bool isIdentified = false;
    bool hasAdditionalAttributes = false;
    bool hasMultiplier = false;
};

enum class IdentificationScrollAction : std::uint8_t
{
    RunFullIdentificationAndOverwriteMultiplier,
    SupplementAdditionalAndOverwriteMultiplier,
    PreserveIdentificationAndOverwriteMultiplier,
    ClearAllCustomIdentificationData,
    RejectNotEligible
};

inline IdentificationScrollAction ResolveIdentificationScrollAction(
    IdentificationScrollType scrollType,
    IdentificationScrollTargetState const& targetState)
{
    if (scrollType == IdentificationScrollType::Cleanup)
    {
        return targetState.isIdentified &&
                (targetState.hasAdditionalAttributes || targetState.hasMultiplier)
            ? IdentificationScrollAction::ClearAllCustomIdentificationData
            : IdentificationScrollAction::RejectNotEligible;
    }

    if (!targetState.isIdentified)
        return IdentificationScrollAction::RunFullIdentificationAndOverwriteMultiplier;

    if (!targetState.hasAdditionalAttributes)
        return IdentificationScrollAction::SupplementAdditionalAndOverwriteMultiplier;

    return IdentificationScrollAction::PreserveIdentificationAndOverwriteMultiplier;
}

inline std::string BuildIdentificationCleanupRefreshPayload(std::uint32_t itemEntry, std::uint32_t itemGuid)
{
    return "IDENTIFY_CLEANUP_REFRESH:" + std::to_string(itemEntry) + ":" + std::to_string(itemGuid);
}

inline std::string BuildIdentificationRefreshPayload(std::uint32_t itemEntry, std::uint32_t itemGuid)
{
    return "IDENTIFY_REFRESH:" + std::to_string(itemEntry) + ":" + std::to_string(itemGuid);
}

#endif

/*
 * 穿戴控制模块 - 表驱动的跨系统物品穿戴限制
 */

#ifndef WEAR_CONTROL_H
#define WEAR_CONTROL_H

#include "Define.h"

#include <string>
#include <vector>

class Player;

enum WearControlLimitType : uint8
{
    WEAR_LIMIT_NONE = 0,
    WEAR_LIMIT_ASCENSION = 1,
    WEAR_LIMIT_XIANQI = 2
};

namespace WearControl
{
struct WearRule
{
    uint32 itemId = 0;
    uint8 wearPosition = 0;
    uint8 slotPosition = 0;
    uint32 requirementId = 0;
    uint32 wearLevel = 0;
    uint8 limitType = WEAR_LIMIT_NONE;
    std::string comment;
};

void Reload();
bool HasExclusiveLimit(uint32 itemId);
bool IsLimitedTo(uint32 itemId, uint8 limitType);
uint8 GetExclusiveLimit(uint32 itemId);
std::vector<uint8> GetItemWearSlots(uint32 itemId, uint8 limitType);
uint32 GetPlayerWearLevel(Player* player, uint8 limitType, uint8 slotPosition);
bool UnlockPlayerWearLevel(Player* player, uint8 limitType, uint8 slotPosition, uint32 wearLevel, bool notify = true);
bool CanEquipItem(Player* player, uint32 itemId, uint8 limitType, uint8 slotPosition, std::string* error = nullptr, bool showRequirementMessages = true);
char const* GetLimitName(uint8 limitType);
}

#endif // WEAR_CONTROL_H

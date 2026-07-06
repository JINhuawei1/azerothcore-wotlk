/*
 * 仙门系统 - 仙器槽位对外只读接口
 */

#ifndef XIANMEN_ARTIFACT_SLOTS_H
#define XIANMEN_ARTIFACT_SLOTS_H

#include "Define.h"

#include <functional>

class Item;
class Player;

namespace XianmenArtifactSlots
{
void ForEachEquippedWeaponItem(Player* player, std::function<void(Item*)> const& visitor);
bool HasEquippedArtifactFeatureSpell(Player* player, uint32 spellId);
bool IsRegisteredArtifactItem(uint32 itemId);
}

#endif // XIANMEN_ARTIFACT_SLOTS_H

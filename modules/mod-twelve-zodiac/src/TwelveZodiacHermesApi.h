#ifndef TWELVE_ZODIAC_HERMES_API_H
#define TWELVE_ZODIAC_HERMES_API_H

#include "Define.h"

#include <string>

class Player;

std::string TwelveZodiacHermesGetState(Player* player);
std::string TwelveZodiacHermesGetDetail(Player* player, uint32 zodiacId);
std::string TwelveZodiacHermesEquip(Player* player, uint32 zodiacId, uint32 slot);
std::string TwelveZodiacHermesUnequip(Player* player, uint32 slot);

#endif

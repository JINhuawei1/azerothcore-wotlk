#ifndef HERMES_BRIDGE_ADDON_API_H
#define HERMES_BRIDGE_ADDON_API_H

#include <string>

class Player;

bool HermesBridge_SendAddonMessage(Player* player, std::string const& prefix, std::string const& payload);

#endif // HERMES_BRIDGE_ADDON_API_H

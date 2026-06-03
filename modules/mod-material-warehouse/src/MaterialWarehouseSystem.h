#ifndef MODULE_MATERIAL_WAREHOUSE_SYSTEM_H
#define MODULE_MATERIAL_WAREHOUSE_SYSTEM_H

#include "Define.h"

#define ACORE_WITH_MATERIAL_WAREHOUSE

class Player;

uint32 MaterialWarehouseGetItemCount(Player* player, uint32 itemId);
uint32 MaterialWarehouseConsumeItemCount(Player* player, uint32 itemId, uint32 count);

#endif // MODULE_MATERIAL_WAREHOUSE_SYSTEM_H

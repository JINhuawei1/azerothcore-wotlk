/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "loader.h"

// 从主模块文件
void AddItemIdentificationSystemScripts();

// 添加所有脚本
// 遵循命名约定 https://github.com/azerothcore/azerothcore-wotlk/blob/master/doc/changelog/master.md#how-to-upgrade-4
// 将模块文件夹名称中的所有'-'替换为'_'
void Addmod_item_identification_systemScripts()
{
    AddItemIdentificationSystemScripts();
}

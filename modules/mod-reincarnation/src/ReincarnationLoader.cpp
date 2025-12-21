/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

// 声明脚本添加函数
void AddSC_ReincarnationModule();
void AddSC_ReincarnationCommands();

// 添加所有转身系统模块脚本
void Addmod_reincarnationScripts()
{
    AddSC_ReincarnationModule();
    AddSC_ReincarnationCommands();
}

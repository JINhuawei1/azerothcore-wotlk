/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

// 声明脚本添加函数
void AddSC_TalentSoulModule();
void AddSC_TalentSoulCommands();
void AddSC_TalentSoulAddon();

// 添加所有天赋之魂模块脚本
void Addmod_talent_soulScripts()
{
    AddSC_TalentSoulModule();
    AddSC_TalentSoulCommands();
    AddSC_TalentSoulAddon();
}

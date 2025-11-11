/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ModuleManager.h"
#include "RequirementSystem.h"
#include "Log.h"

ModuleManager* ModuleManager::instance()
{
    static ModuleManager instance;
    return &instance;
}

void ModuleManager::RegisterAnnouncementModule(AnnouncementInterface* module)
{
    _announcementModule = module;
    LOG_INFO("module", "公告模块已注册到模块管理器");
}

void ModuleManager::RegisterRequirementModule(RequirementInterface* module)
{
    _requirementModule = module;
    LOG_INFO("module", "需求模块已注册到模块管理器");
}

void ModuleManager::RegisterRewardModule(RewardInterface* module)
{
    _rewardModule = module;
    LOG_INFO("module", "奖励模块已注册到模块管理器");
}

AnnouncementInterface* ModuleManager::GetAnnouncementModule()
{
    return _announcementModule;
}

RequirementInterface* ModuleManager::GetRequirementModule()
{
    return _requirementModule;
}

RewardInterface* ModuleManager::GetRewardModule()
{
    return _rewardModule;
}

/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include "AnnouncementInterface.h"
#include "RequirementInterface.h"
#include "RewardInterface.h"

/**
 * @class ModuleManager
 * @brief 模块管理器
 * 
 * 此类管理所有模块接口，提供注册和获取接口的方法
 */
class ModuleManager
{
public:
    /**
     * 获取单例实例
     */
    static ModuleManager* instance();

    /**
     * 注册公告模块
     */
    void RegisterAnnouncementModule(AnnouncementInterface* module);
    
    /**
     * 注册需求模块
     */
    void RegisterRequirementModule(RequirementInterface* module);
    
    /**
     * 注册奖励模块
     */
    void RegisterRewardModule(RewardInterface* module);

    /**
     * 获取公告模块接口
     */
    AnnouncementInterface* GetAnnouncementModule();
    
    /**
     * 获取需求模块接口
     */
    RequirementInterface* GetRequirementModule();
    
    /**
     * 获取奖励模块接口
     */
    RewardInterface* GetRewardModule();

private:
    ModuleManager() = default;
    ~ModuleManager() = default;

    AnnouncementInterface* _announcementModule = nullptr;
    RequirementInterface* _requirementModule = nullptr;
    RewardInterface* _rewardModule = nullptr;
};

#define sModuleManager ModuleManager::instance()

#endif // MODULE_MANAGER_H

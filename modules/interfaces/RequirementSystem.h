/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef REQUIREMENT_SYSTEM_H
#define REQUIREMENT_SYSTEM_H

#include "RequirementInterface.h"

/**
 * @class RequirementSystem
 * @brief 需求系统全局接口
 * 
 * 此接口提供了全局访问需求系统的方法，避免直接包含需求模板模块的头文件
 */
class RequirementSystem : public RequirementInterface
{
public:
    /**
     * 获取单例实例
     */
    static RequirementSystem* instance();

    /**
     * 初始化需求系统
     */
    virtual void Initialize() = 0;

    /**
     * 检查玩家是否满足需求
     * 
     * @param player 玩家指针
     * @param templateId 需求模板ID
     * @param showMessages 是否显示消息
     * @return 是否满足需求
     */
    virtual bool CheckRequirements(Player* player, uint32 templateId, bool showMessages = true) = 0;
    
    /**
     * 消耗需求（例如扣除金币、物品等）
     * 
     * @param player 玩家指针
     * @param templateId 需求模板ID
     * @return 是否成功消耗
     */
    virtual bool ConsumeRequirements(Player* player, uint32 templateId) = 0;
};

#define sRequirementSystem RequirementSystem::instance()

#endif // REQUIREMENT_SYSTEM_H

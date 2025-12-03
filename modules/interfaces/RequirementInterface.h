/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef REQUIREMENT_INTERFACE_H
#define REQUIREMENT_INTERFACE_H

#include "Player.h"

/**
 * @class RequirementInterface
 * @brief 需求模块接口
 * 
 * 此接口定义了需求模块的标准API，允许其他模块检查和消耗需求
 */
class RequirementInterface
{
public:
    virtual ~RequirementInterface() = default;
    
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

#endif // REQUIREMENT_INTERFACE_H

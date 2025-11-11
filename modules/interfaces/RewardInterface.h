/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef REWARD_INTERFACE_H
#define REWARD_INTERFACE_H

#include "Player.h"
#include <vector>

/**
 * @class RewardInterface
 * @brief 奖励模块接口
 * 
 * 此接口定义了奖励模块的标准API，允许其他模块发放奖励
 */
class RewardInterface
{
public:
    virtual ~RewardInterface() = default;
    
    /**
     * 给予奖励
     * 
     * @param player 玩家指针
     * @param rewardId 奖励模板ID
     * @param checkChance 是否检查几率
     * @return 是否成功发放
     */
    virtual bool GiveReward(Player* player, uint32 rewardId, bool checkChance = true) = 0;
    
    /**
     * 给予随机奖励
     * 
     * @param player 玩家指针
     * @param rewardIds 奖励模板ID列表
     * @return 是否成功发放
     */
    virtual bool GiveRandomReward(Player* player, const std::vector<uint32>& rewardIds) = 0;
};

#endif // REWARD_INTERFACE_H

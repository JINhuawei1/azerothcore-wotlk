/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef REWARD_INTERFACE_H
#define REWARD_INTERFACE_H

#include "Player.h"
#include "../../interfaces/RewardReceipt.h"
#include <vector>
#include <string>

/**
 * @class RewardInterface
 * @brief 奖励模块接口
 */
class RewardInterface
{
public:
    /**
     * 给予奖励
     * @param player 玩家指针
     * @param rewardId 奖励ID
     * @param checkChance 是否检查概率
     * @param showNotification 是否显示奖励提示（默认true）
     * @return 是否成功给予奖励
     */
    virtual bool GiveReward(Player* player, uint32 rewardId, bool checkChance = true, bool showNotification = true) = 0;

    /**
     * 给予奖励并返回可逆奖励回执。
     * 默认实现保持旧接口语义，具体奖励实现可选择性覆盖以填充回执。
     */
    virtual bool GiveRewardWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt,
        bool checkChance = true, bool showNotification = true)
    {
        return GiveReward(player, rewardId, checkChance, showNotification);
    }

    /**
     * 给予随机奖励
     * @param player 玩家指针
     * @param rewardIds 奖励ID列表
     * @return 是否成功给予奖励
     */
    virtual bool GiveRandomReward(Player* player, const std::vector<uint32>& rewardIds) = 0;

    /**
     * 获取奖励描述（用于显示给玩家）
     * @param player 玩家指针（用于生成物品链接）
     * @param rewardId 奖励模板ID
     * @return 奖励内容的描述字符串列表（包含可点击的物品链接）
     */
    virtual std::vector<std::string> GetRewardDescription(Player* player, uint32 rewardId) = 0;

    virtual ~RewardInterface() = default;
};

#endif // REWARD_INTERFACE_H

/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef MODULE_USAGE_EXAMPLE_H
#define MODULE_USAGE_EXAMPLE_H

#include "ModuleManager.h"
#include "Player.h"
#include "ScriptMgr.h"

/**
 * 示例：如何在其他模块中使用公告模块、需求模块和奖励模块
 */
class ModuleUsageExample : public PlayerScript
{
public:
    ModuleUsageExample() : PlayerScript("ModuleUsageExample") { }

    void OnLogin(Player* player) override
    {
        // 示例1：使用公告模块发送公告
        if (AnnouncementInterface* announceModule = sModuleManager->GetAnnouncementModule())
        {
            // 发送ID为1的成功公告
            announceModule->SendAnnouncement(player, 1, true);
        }
        else
        {
            // 公告模块未加载，使用备用方案
            ChatHandler(player->GetSession()).SendSysMessage("欢迎回来！公告模块未加载，使用备用消息。");
        }

        // 示例2：使用需求模块检查需求
        if (RequirementInterface* reqModule = sModuleManager->GetRequirementModule())
        {
            // 检查玩家是否满足ID为1的需求
            if (reqModule->CheckRequirements(player, 1))
            {
                // 满足需求，消耗需求
                reqModule->ConsumeRequirements(player, 1);
                
                // 执行后续操作...
            }
        }

        // 示例3：使用奖励模块发放奖励
        if (RewardInterface* rewardModule = sModuleManager->GetRewardModule())
        {
            // 发放ID为1的奖励
            rewardModule->GiveReward(player, 1);
            
            // 发放随机奖励
            std::vector<uint32> rewardIds = {1, 2, 3, 4, 5};
            rewardModule->GiveRandomReward(player, rewardIds);
        }
    }
};

#endif // MODULE_USAGE_EXAMPLE_H

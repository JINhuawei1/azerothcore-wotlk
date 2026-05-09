/*
 * 宣传奖励系统 - 命令脚本声明
 *
 * 命令树:
 *   .宣传奖励 帮助
 *   .宣传奖励 发放 [玩家名] [天数=1]   管理员: 累加宣传天数 + 写入 _奖励_兑换码
 *   .宣传奖励 查询 [玩家名]            GM(无参=查自己)
 *   .宣传奖励 重载                     管理员
 *
 * 玩家用 .兑换码 兑换 [CDK] (mod-redemption-code 命令) 拿奖励
 */

#ifndef PROMOTION_REWARD_COMMANDS_H
#define PROMOTION_REWARD_COMMANDS_H

#include "ScriptMgr.h"
#include "Chat/ChatCommands/ChatCommand.h"
#include <string>

using namespace Acore::ChatCommands;

class PromotionReward_CommandScript : public CommandScript
{
public:
    PromotionReward_CommandScript();

    [[nodiscard]] ChatCommandTable GetCommands() const override;

    static bool HandleHelpCommand   (ChatHandler* handler, char const* args);
    static bool HandleIssueCommand  (ChatHandler* handler, Optional<std::string> targetName, Optional<uint32> days);
    static bool HandleQueryCommand  (ChatHandler* handler, Optional<std::string> targetName);
    static bool HandleReloadCommand (ChatHandler* handler, char const* args);
    static bool HandleOpenUICommand (ChatHandler* handler, char const* args);
};

#endif // PROMOTION_REWARD_COMMANDS_H

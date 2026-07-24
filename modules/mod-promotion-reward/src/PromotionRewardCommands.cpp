/*
 * 宣传奖励系统 - 命令脚本实现
 */

#include "PromotionRewardCommands.h"
#include "PromotionRewardAudit.h"
#include "PromotionRewardModule.h"
#include "Chat.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "World.h"

#ifndef SEC_PLAYER
#define SEC_PLAYER 0
#endif
#ifndef SEC_GAMEMASTER
#define SEC_GAMEMASTER 3
#endif
#ifndef SEC_ADMINISTRATOR
#define SEC_ADMINISTRATOR 4
#endif

PromotionReward_CommandScript::PromotionReward_CommandScript()
    : CommandScript("PromotionReward_CommandScript") { }

ChatCommandTable PromotionReward_CommandScript::GetCommands() const
{
    static ChatCommandTable promotionSubTable =
    {
        { "帮助", HandleHelpCommand,    SEC_PLAYER,         Console::No  },
        { "发放", HandleIssueCommand,   SEC_ADMINISTRATOR,  Console::Yes },
        { "查询", HandleQueryCommand,   SEC_GAMEMASTER,     Console::Yes },
        { "审核通过", HandleApproveSubmissionCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "审核无效", HandleRejectSubmissionCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "回收重试", HandleRetryRollbackCommand, SEC_GAMEMASTER, Console::Yes },
        { "回收查询", HandleQueryRollbackCommand, SEC_GAMEMASTER, Console::Yes },
        { "欠账清除", HandleClearRecoveryDebtCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "重载", HandleReloadCommand,  SEC_ADMINISTRATOR,  Console::Yes },
        { "界面", HandleOpenUICommand,  SEC_PLAYER,         Console::No  },
        { "ui",   HandleOpenUICommand,  SEC_PLAYER,         Console::No  },
    };

    static ChatCommandTable commandTable =
    {
        { "宣传奖励", promotionSubTable }
    };

    return commandTable;
}

bool PromotionReward_CommandScript::HandleHelpCommand(ChatHandler* handler, char const* /*args*/)
{
    handler->PSendSysMessage("========================================");
    handler->PSendSysMessage("宣传奖励系统命令帮助");
    handler->PSendSysMessage("========================================");
    handler->PSendSysMessage("玩家命令:");
    handler->PSendSysMessage("  .兑换码 兑换 [兑换码]");
    handler->PSendSysMessage("    使用CDK领取奖励(走 mod-redemption-code 现有命令)");
    handler->PSendSysMessage("    奖励内容由 _模板_奖励 配置(含宣传武器+其他物品)");
    handler->PSendSysMessage("");
    handler->PSendSysMessage("GM 命令:");
    handler->PSendSysMessage("  .宣传奖励 查询 [玩家名]");
    handler->PSendSysMessage("    无参=查自己。显示累计宣传天数与当前武器属性");
    handler->PSendSysMessage("  .宣传奖励 回收查询 <提交ID>");
    handler->PSendSysMessage("  .宣传奖励 回收重试 <提交ID>");
    handler->PSendSysMessage("");
    handler->PSendSysMessage("管理员命令:");
    handler->PSendSysMessage("  .宣传奖励 发放 [玩家名] [数量=1]");
    handler->PSendSysMessage("    生成N张通用宣传CDK。玩家兑换成功时才宣传天数+1并升级武器");
    handler->PSendSysMessage("    第1次兑换给宣传神器1,第2次回收旧武器并给宣传神器2");
    handler->PSendSysMessage("  .宣传奖励 重载");
    handler->PSendSysMessage("    重新加载 world.`_宣传奖励系统` 配置");
    handler->PSendSysMessage("  .宣传奖励 审核通过 <提交ID>");
    handler->PSendSysMessage("  .宣传奖励 审核无效 <提交ID> <理由>");
    handler->PSendSysMessage("  .宣传奖励 欠账清除 <提交ID> <线下处理说明>");
    handler->PSendSysMessage("========================================");
    handler->PSendSysMessage("注意:宣传CDK是通用码,不绑定固定武器等级");
    handler->PSendSysMessage("武器只要在玩家身上(背包或装备槽)就生效");
    handler->PSendSysMessage("属性 = 初始值 + (宣传天数-1) * 每日增量");
    return true;
}

bool PromotionReward_CommandScript::HandleIssueCommand(ChatHandler* handler, Optional<std::string> targetName, Optional<uint32> days)
{
    if (!sPromotionRewardMgr->IsEnabled())
    {
        handler->PSendSysMessage("宣传奖励系统已禁用");
        handler->SetSentErrorMessage(true);
        return false;
    }

    if (!targetName || targetName->empty())
    {
        handler->PSendSysMessage("用法: .宣传奖励 发放 [玩家名] [数量=1]");
        handler->PSendSysMessage("说明: 生成通用宣传CDK,玩家兑换成功时才宣传天数+1并升级武器");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 count = days.value_or(1);

    ObjectGuid targetGuidObj = sCharacterCache->GetCharacterGuidByName(*targetName);
    if (targetGuidObj.IsEmpty())
    {
        handler->PSendSysMessage("找不到玩家: {}", *targetName);
        handler->SetSentErrorMessage(true);
        return false;
    }
    uint32 targetGuid = targetGuidObj.GetCounter();

    std::vector<std::string> codes;
    std::string errMsg;
    if (!sPromotionRewardMgr->IssueCodes(targetGuid, *targetName, count, codes, errMsg))
    {
        handler->PSendSysMessage("发放失败: {}", errMsg);
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->PSendSysMessage("========================================");
    handler->PSendSysMessage("已为 {} 生成 {} 张通用宣传CDK", *targetName, codes.size());
    handler->PSendSysMessage("玩家每兑换1张,宣传天数+1,自动回收旧宣传神器并发放下一等级:");
    for (auto const& c : codes)
        handler->PSendSysMessage("  {}", c);
    handler->PSendSysMessage("玩家请用: .兑换码 兑换 [CDK]");
    handler->PSendSysMessage("========================================");

    if (Player* target = ObjectAccessor::FindPlayerByName(*targetName, false))
    {
        ChatHandler th(target->GetSession());
        th.PSendSysMessage("|cff00ff00[宣传奖励]|r 您获得 {} 张通用宣传CDK,使用 .兑换码 兑换 [CDK] 升级宣传神器:", codes.size());
        for (auto const& c : codes)
            th.PSendSysMessage("  |cffffff00{}|r", c);
    }

    return true;
}

bool PromotionReward_CommandScript::HandleQueryCommand(ChatHandler* handler, Optional<std::string> targetName)
{
    uint32 targetGuid = 0;
    std::string name;
    Player* online = nullptr;

    if (targetName && !targetName->empty())
    {
        ObjectGuid g = sCharacterCache->GetCharacterGuidByName(*targetName);
        if (g.IsEmpty())
        {
            handler->PSendSysMessage("找不到玩家: {}", *targetName);
            handler->SetSentErrorMessage(true);
            return false;
        }
        targetGuid = g.GetCounter();
        name = *targetName;
        online = ObjectAccessor::FindPlayerByName(*targetName, false);
    }
    else
    {
        Player* self = handler->GetPlayer();
        if (!self)
        {
            handler->PSendSysMessage("控制台需提供玩家名");
            handler->SetSentErrorMessage(true);
            return false;
        }
        targetGuid = self->GetGUID().GetCounter();
        name = self->GetName();
        online = self;
    }

    PromotionPlayerData* d = sPromotionRewardMgr->GetPlayerData(targetGuid);
    if (!d)
    {
        handler->PSendSysMessage("玩家 {} 暂无宣传记录", name);
        return true;
    }

    int256 attr = sPromotionRewardMgr->CalcTotalAttr(d->days);
    bool held = online ? sPromotionRewardMgr->IsWeaponHeld(online) : false;

    handler->PSendSysMessage("========================================");
    handler->PSendSysMessage("玩家: {}", name);
    handler->PSendSysMessage("  累计宣传天数: {}", d->days);
    handler->PSendSysMessage("  持有宣传武器: {}", online ? (held ? "是" : "否") : "(离线无法判定)");
    handler->PSendSysMessage("  全属性加成: +{} {}", Acore::ToString(attr), held ? "(已生效)" : "(未生效,需持有武器)");
    handler->PSendSysMessage("========================================");
    return true;
}

bool PromotionReward_CommandScript::HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
{
    sPromotionRewardMgr->LoadConfig();
    sPromotionRewardMgr->LoadAllPlayers();

    PromotionConfig const& c = sPromotionRewardMgr->GetConfig();
    handler->PSendSysMessage("========================================");
    handler->PSendSysMessage("宣传奖励系统已重新加载");
    handler->PSendSysMessage("  启用: {}", c.enabled ? "是" : "否");
    handler->PSendSysMessage("  武器entry: {}", c.weaponEntry);
    handler->PSendSysMessage("  初始全属性: {} / 每日增量: {}", Acore::ToString(c.baseAttrValue), Acore::ToString(c.perDayAttrValue));
    handler->PSendSysMessage("  对接 _奖励_兑换码 [组={}, 需求={}, 奖励={}]",
        c.groupId, c.requireId, c.rewardId);
    handler->PSendSysMessage("========================================");
    return true;
}

bool PromotionReward_CommandScript::HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
{
    Player* player = handler->GetPlayer();
    if (!player)
    {
        handler->PSendSysMessage("只有玩家可以打开界面");
        handler->SetSentErrorMessage(true);
        return false;
    }

    if (!sPromotionRewardMgr->IsEnabled())
    {
        handler->PSendSysMessage("宣传奖励系统已禁用");
        handler->SetSentErrorMessage(true);
        return false;
    }

    sPromotionRewardMgr->SendInfoToClient(player);
    sPromotionRewardMgr->SendOpenUIToClient(player);
    return true;
}

bool PromotionReward_CommandScript::HandleApproveSubmissionCommand(ChatHandler* handler, uint64 submissionId)
{
    uint32 reviewerAccountId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;
    if (!sPromotionRewardAuditMgr->ApplyReviewDecision(
        submissionId, true, reviewerAccountId, "GM命令审核通过"))
    {
        handler->PSendSysMessage("提交 {} 不存在、已审核或状态冲突", submissionId);
        handler->SetSentErrorMessage(true);
        return false;
    }

    sPromotionRewardAuditMgr->ConsumeReviewQueue(1);
    handler->PSendSysMessage("提交 {} 已登记为审核通过", submissionId);
    return true;
}

bool PromotionReward_CommandScript::HandleRejectSubmissionCommand(
    ChatHandler* handler,
    uint64 submissionId,
    Tail reason)
{
    if (reason.empty())
    {
        handler->PSendSysMessage("用法: .宣传奖励 审核无效 <提交ID> <理由>");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 reviewerAccountId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;
    if (!sPromotionRewardAuditMgr->ApplyReviewDecision(
        submissionId, false, reviewerAccountId, std::string(reason)))
    {
        handler->PSendSysMessage("提交 {} 不存在、已审核或状态冲突", submissionId);
        handler->SetSentErrorMessage(true);
        return false;
    }

    sPromotionRewardAuditMgr->ConsumeReviewQueue(1);
    handler->PSendSysMessage("提交 {} 已登记为审核无效，奖励进入独立回收队列", submissionId);
    return true;
}

bool PromotionReward_CommandScript::HandleRetryRollbackCommand(ChatHandler* handler, uint64 submissionId)
{
    if (!sPromotionRewardAuditMgr->RetryRollback(submissionId))
    {
        handler->PSendSysMessage("提交 {} 不存在或当前状态不能重试", submissionId);
        handler->SetSentErrorMessage(true);
        return false;
    }

    sPromotionRewardAuditMgr->ConsumeRollbackQueue(1);
    std::string summary;
    if (sPromotionRewardAuditMgr->GetRollbackSummary(submissionId, summary))
        handler->PSendSysMessage("{}", summary);
    return true;
}

bool PromotionReward_CommandScript::HandleQueryRollbackCommand(ChatHandler* handler, uint64 submissionId)
{
    std::string summary;
    if (!sPromotionRewardAuditMgr->GetRollbackSummary(submissionId, summary))
    {
        handler->PSendSysMessage("找不到提交 {} 的奖励流水", submissionId);
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->PSendSysMessage("{}", summary);
    return true;
}

bool PromotionReward_CommandScript::HandleClearRecoveryDebtCommand(
    ChatHandler* handler,
    uint64 submissionId,
    Tail reason)
{
    if (reason.empty())
    {
        handler->PSendSysMessage("用法: .宣传奖励 欠账清除 <提交ID> <线下处理说明>");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 operatorAccountId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;
    if (!sPromotionRewardAuditMgr->ClearRecoveryDebt(
        submissionId, operatorAccountId, std::string(reason)))
    {
        handler->PSendSysMessage("提交 {} 不存在或当前不是追回欠账状态", submissionId);
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->PSendSysMessage("提交 {} 的追回欠账已按管理员说明关闭", submissionId);
    return true;
}

void AddSC_PromotionReward_CommandScript()
{
    new PromotionReward_CommandScript();
}

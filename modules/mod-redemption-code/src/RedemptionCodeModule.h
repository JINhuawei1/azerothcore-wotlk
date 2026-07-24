/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef REDEMPTION_CODE_MODULE_H
#define REDEMPTION_CODE_MODULE_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Common.h"
#include "Logging/Log.h"
#include "Chat/ChatCommands/ChatCommand.h"
#include <string>
#include <vector>
#include <map>

// 包含接口定义
#include "RequirementInterface.h"
#include "RewardInterface.h"

class RedemptionCodeMgr
{
public:
    static RedemptionCodeMgr* instance();

    // 初始化
    void Initialize();

    // 生成兑换码
    std::string GenerateCode();

    // 【安全】兑换码格式白名单校验：仅允许字母/数字/下划线/连字符，长度 1-64。
    // 所有以玩家输入或外部字符串拼接 SQL 的入口必须先通过此校验。
    static bool IsValidCodeFormat(std::string const& code);

    // 添加兑换码
    bool AddCode(std::string const& code, uint32 groupId, uint32 requireId, uint32 rewardId, uint32 count, std::string const& comment);

    // 批量添加兑换码（本地去重 + 事务批量写入），返回成功写入数量
    uint32 AddCodesBatch(std::vector<std::string> const& codes, uint32 groupId, uint32 requireId, uint32 rewardId, uint32 countPerCode, std::string const& commentPrefix);

    // 使用兑换码
    bool UseCode(Player* player, std::string const& code);

    // 使用兑换码（返回奖励ID）
    bool UseCodeWithRewardId(Player* player, std::string const& code, uint32& outRewardId);

    // 查询兑换码
    bool QueryCode(ChatHandler* handler, std::string const& code);

    // 删除兑换码
    bool DeleteCode(std::string const& code);

    // 检查系统状态（懒加载模式，动态获取接口）
    bool IsRewardSystemAvailable();
    bool IsRequirementSystemAvailable();

    // 获取接口（懒加载）
    RewardInterface* GetRewardInterface();
    RequirementInterface* GetRequirementInterface();

    // 获取奖励描述
    std::vector<std::string> GetRewardDescription(Player* player, uint32 rewardId);

private:
    RedemptionCodeMgr() {}
    ~RedemptionCodeMgr() {}
    RedemptionCodeMgr(const RedemptionCodeMgr&);
    RedemptionCodeMgr& operator=(const RedemptionCodeMgr&);

    // 检查兑换码是否存在
    bool CodeExists(std::string const& code);

    // 检查玩家是否已经使用过此兑换码
    bool HasPlayerUsedCode(Player* player, std::string const& code);

    // 记录玩家使用兑换码
    void RecordCodeUsage(Player* player, std::string const& code);

    // 发放奖励
    bool GiveReward(Player* player, uint32 rewardId);
    bool GiveRewardWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt);
    bool GiveRewardWithoutItems(Player* player, uint32 rewardId);
    bool GiveRewardWithoutItemsWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt);

    // 检查需求
    bool CheckRequirement(Player* player, uint32 requireId);

    // 配置
    bool m_enabled;
    uint32 m_codeLength;
    std::string m_charset;
    bool m_announce;

    // 接口指针
    RequirementInterface* m_requirementInterface;
    RewardInterface* m_rewardInterface;
};

#define sRedemptionCodeMgr RedemptionCodeMgr::instance()

class RedemptionCode_WorldScript : public WorldScript
{
public:
    RedemptionCode_WorldScript();

    void OnAfterConfigLoad(bool reload) override;
    void OnStartup() override;
    void OnUpdate(uint32 diff) override;

private:
    bool m_initialized;
    uint32 m_initTimer;
    uint32 m_initDelay;
    uint32 m_loadedCount;
};

#endif // REDEMPTION_CODE_MODULE_H

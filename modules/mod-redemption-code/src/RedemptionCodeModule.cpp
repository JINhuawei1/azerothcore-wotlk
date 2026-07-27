/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "RedemptionCodeModule.h"
#include "mod-promotion-reward/src/PromotionRewardAudit.h"
#include "mod-promotion-reward/src/PromotionRewardModule.h"
#include "mod-reward-template/src/RewardTemplate.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "TaskScheduler.h"
#include "Common.h"
#include "Logging/Log.h"
#include "Chat/ChatCommands/ChatCommand.h"
#include <random>

// 包含模块管理器以获取需求和奖励系统接口
#include "ModuleManager.h"

// 获取配置管理器实例
#define sConfigMgr ConfigMgr::instance()

// 定义日志宏，以便与AzerothCore兼容
#ifndef LOG_INFO
#define LOG_INFO(category, ...) sLog->outMessage(category, LogLevel::LOG_LEVEL_INFO, __VA_ARGS__)
#endif

RedemptionCodeMgr* RedemptionCodeMgr::instance()
{
    static RedemptionCodeMgr instance;
    return &instance;
}

void RedemptionCodeMgr::Initialize()
{
    // 从配置文件加载设置
    m_enabled = sConfigMgr->GetOption<bool>("RedemptionCode.Enable", true);
    m_codeLength = sConfigMgr->GetOption<uint32>("RedemptionCode.Length", 16);
    // 与 IsValidCodeFormat 的长度上限保持一致
    m_codeLength = std::min(std::max(m_codeLength, 4u), 64u);
    m_charset = sConfigMgr->GetOption<std::string>("RedemptionCode.Charset", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    // 字符集过滤为白名单字符（与 IsValidCodeFormat 一致），防止配置特殊字符导致生成的码被校验拒绝
    {
        std::string filtered;
        for (char c : m_charset)
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
                filtered += c;
        m_charset = filtered.empty() ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" : filtered;
    }
    m_announce = sConfigMgr->GetOption<bool>("RedemptionCode.Announce", true);

    // 【重要改动】不再在初始化时获取接口指针
    // 改为懒加载模式，在实际使用时动态获取，避免启动顺序问题
    m_requirementInterface = nullptr;
    m_rewardInterface = nullptr;

    // 仅在配置重载时显示信息
    if (!m_enabled)
    {
        LOG_INFO("server.loading", "兑换码模块已禁用");
    }
}

std::string RedemptionCodeMgr::GenerateCode()
{
    std::string code;

    // 【健壮性】字符集为空时 uniform_int_distribution(0, -1) 是 UB
    if (m_charset.empty())
        m_charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    // 【性能】随机引擎复用，避免每个码重建 random_device+mt19937
    static thread_local std::mt19937 gen{ std::random_device{}() };
    std::uniform_int_distribution<size_t> dis(0, m_charset.size() - 1);

    for (uint32 i = 0; i < m_codeLength; ++i)
    {
        code += m_charset[dis(gen)];
    }

    return code;
}

bool RedemptionCodeMgr::IsValidCodeFormat(std::string const& code)
{
    if (code.empty() || code.size() > 64)
        return false;

    for (char c : code)
    {
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok)
            return false;
    }

    return true;
}

bool RedemptionCodeMgr::AddCode(std::string const& code, uint32 groupId, uint32 requireId, uint32 rewardId, uint32 count, std::string const& comment)
{
    if (!m_enabled)
        return false;

    // 【安全修复】兑换码白名单校验（防 SQL 注入），注释转义
    if (!IsValidCodeFormat(code))
        return false;

    if (CodeExists(code))
        return false;

    std::string safeComment = comment;
    WorldDatabase.EscapeString(safeComment);

    // 【修复】同步写入：后续 CodeExists 去重依赖立即可见性（异步队列中的 INSERT 对同步 SELECT 不可见）
    WorldDatabase.DirectExecute("INSERT INTO `_奖励_兑换码` (`注释`, `兑换码`, `组`, `需求`, `奖励`, `兑换次数`) VALUES ('{}', '{}', {}, {}, {}, {})",
        safeComment, code, groupId, requireId, rewardId, count);

    return true;
}

uint32 RedemptionCodeMgr::AddCodesBatch(std::vector<std::string> const& codes, uint32 groupId, uint32 requireId, uint32 rewardId, uint32 countPerCode, std::string const& commentPrefix)
{
    if (!m_enabled || codes.empty())
        return 0;

    std::string safePrefix = commentPrefix;
    WorldDatabase.EscapeString(safePrefix);

    // 事务 + 多行 INSERT，避免逐条同步往返；调用方负责本地去重与格式校验
    WorldDatabaseTransaction trans = WorldDatabase.BeginTransaction();

    uint32 inserted = 0;
    constexpr size_t ROWS_PER_STATEMENT = 250;
    std::string sql;

    for (size_t i = 0; i < codes.size(); ++i)
    {
        if (!IsValidCodeFormat(codes[i]))
            continue;

        if (sql.empty())
            sql = "INSERT INTO `_奖励_兑换码` (`注释`, `兑换码`, `组`, `需求`, `奖励`, `兑换次数`) VALUES ";
        else
            sql += ",";

        sql += Acore::StringFormat("('{}-{}', '{}', {}, {}, {}, {})",
            safePrefix, inserted + 1, codes[i], groupId, requireId, rewardId, countPerCode);
        ++inserted;

        if (inserted % ROWS_PER_STATEMENT == 0)
        {
            trans->Append(sql.c_str());
            sql.clear();
        }
    }

    if (!sql.empty())
        trans->Append(sql.c_str());

    WorldDatabase.CommitTransaction(trans);
    return inserted;
}

bool RedemptionCodeMgr::UseCode(Player* player, std::string const& code)
{
    // 【修复】原实现是 UseCodeWithRewardId 的 75 行复制粘贴副本（两处维护必然分叉），改为委托
    uint32 unusedRewardId = 0;
    return UseCodeWithRewardId(player, code, unusedRewardId);
}

bool RedemptionCodeMgr::UseCodeWithRewardId(Player* player, std::string const& code, uint32& outRewardId)
{
    outRewardId = 0;

    if (!m_enabled || !player)
        return false;

    // 【安全修复】玩家输入白名单校验：防止把任意字符串拼进后续 SELECT/UPDATE（SQL 注入可全表污染）
    if (!IsValidCodeFormat(code))
        return false;

    // 检查兑换码是否存在
    QueryResult result = WorldDatabase.Query("SELECT `组`, `需求`, `奖励`, `兑换次数`, `兑换角色`, `兑换账号`, `兑换IP`, `领取公告` FROM `_奖励_兑换码` WHERE `兑换码` = '{}'", code);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    uint32 groupId = fields[0].Get<uint32>();
    uint32 requireId = fields[1].Get<uint32>();
    uint32 rewardId = fields[2].Get<uint32>();
    uint32 count = fields[3].Get<uint32>();
    uint32 announceType = fields[7].Get<uint32>();

    // 检查兑换次数
    if (count <= 0)
        return false;

    // 检查玩家是否已经使用过此兑换码
    if (HasPlayerUsedCode(player, code))
        return false;

    // 检查需求
    if (!CheckRequirement(player, requireId))
        return false;

    PromotionRedeemSnapshot snapshot;
    bool trackedPromotionCode = sPromotionRewardAuditMgr->BeginCodeRedeem(player, code, groupId, snapshot);
    if (snapshot.grantId != 0 && !trackedPromotionCode)
        return false;

    if (sPromotionRewardMgr->IsPromotionCodeGroup(groupId))
    {
        uint32 promoRewardId = rewardId;
        if (!sPromotionRewardMgr->RedeemPromotionCode(player, groupId, promoRewardId))
        {
            if (trackedPromotionCode)
                sPromotionRewardAuditMgr->CompleteCodeRedeem(player, code, promoRewardId, snapshot, false);
            return false;
        }

        bool extraRewardGranted = trackedPromotionCode
            ? GiveRewardWithoutItemsWithReceipt(player, promoRewardId, snapshot.rewardReceipt)
            : GiveRewardWithoutItems(player, promoRewardId);
        if (!extraRewardGranted)
        {
            if (trackedPromotionCode)
            {
                snapshot.receiptComplete = false;
                snapshot.receiptError = "宣传神器已发放，但奖励模板的额外奖励发放失败，回执可能不完整";
            }
            LOG_ERROR("module.redemption", "玩家 {} 领取宣传兑换码额外奖励ID {} 失败", player->GetName(), promoRewardId);
            ChatHandler(player->GetSession()).PSendSysMessage("宣传神器已发放，但额外奖励发放失败，请联系管理员。");
        }

        RecordCodeUsage(player, code);
        if (trackedPromotionCode)
            sPromotionRewardAuditMgr->CompleteCodeRedeem(player, code, promoRewardId, snapshot, true);
        // 【修复】同步条件扣减：异步 Execute 对下一次同步 SELECT 不可见，共享码可被连刷；条件防止下溢
        WorldDatabase.DirectExecute("UPDATE `_奖励_兑换码` SET `兑换次数` = `兑换次数` - 1 WHERE `兑换码` = '{}' AND `兑换次数` > 0", code);

        if (m_announce && announceType > 1000)
        {
            std::string playerName = player->GetName();
            ChatHandler(nullptr).SendWorldText(announceType, playerName.c_str(), code.c_str());
        }

        outRewardId = promoRewardId;
        return true;
    }

    // 发放奖励
    bool rewardGranted = trackedPromotionCode
        ? GiveRewardWithReceipt(player, rewardId, snapshot.rewardReceipt)
        : GiveReward(player, rewardId);
    if (!rewardGranted)
    {
        if (trackedPromotionCode)
            sPromotionRewardAuditMgr->CompleteCodeRedeem(player, code, rewardId, snapshot, false);
        return false;
    }

    // 记录玩家使用兑换码
    RecordCodeUsage(player, code);
    if (trackedPromotionCode)
        sPromotionRewardAuditMgr->CompleteCodeRedeem(player, code, rewardId, snapshot, true);

    // 更新兑换次数
    // 【修复】同步条件扣减（理由同上）
    WorldDatabase.DirectExecute("UPDATE `_奖励_兑换码` SET `兑换次数` = `兑换次数` - 1 WHERE `兑换码` = '{}' AND `兑换次数` > 0", code);

    // 公告（公告ID需要大于1000以避免与系统消息冲突）
    if (m_announce && announceType > 1000)
    {
        std::string playerName = player->GetName();
        // 使用兼容的方式发送世界消息
        ChatHandler(nullptr).SendWorldText(announceType, playerName.c_str(), code.c_str());
    }

    // 返回奖励ID
    outRewardId = rewardId;
    return true;
}

bool RedemptionCodeMgr::QueryCode(ChatHandler* handler, std::string const& code)
{
    if (!m_enabled || !handler)
        return false;

    // 【安全修复】白名单校验后再拼 SQL
    if (!IsValidCodeFormat(code))
    {
        handler->PSendSysMessage("兑换码格式无效（仅允许字母/数字/下划线/连字符）");
        return false;
    }

    // 查询兑换码信息
    QueryResult result = WorldDatabase.Query("SELECT `注释`, `组`, `需求`, `奖励`, `兑换次数`, `兑换角色`, `兑换账号`, `兑换IP`, `领取公告` FROM `_奖励_兑换码` WHERE `兑换码` = '{}'", code);
    if (!result)
    {
        handler->PSendSysMessage("兑换码 {} 不存在", code);
        return false;
    }

    Field* fields = result->Fetch();
    std::string comment = fields[0].Get<std::string>();
    uint32 groupId = fields[1].Get<uint32>();
    uint32 requireId = fields[2].Get<uint32>();
    uint32 rewardId = fields[3].Get<uint32>();
    uint32 count = fields[4].Get<uint32>();
    std::string usedChars = fields[5].Get<std::string>();
    std::string usedAccounts = fields[6].Get<std::string>();
    std::string usedIPs = fields[7].Get<std::string>();
    uint32 announceType = fields[8].Get<uint32>();

    handler->PSendSysMessage("兑换码: {}", code);
    handler->PSendSysMessage("注释: {}", comment);
    handler->PSendSysMessage("组ID: {}", groupId);
    handler->PSendSysMessage("需求ID: {}", requireId);
    handler->PSendSysMessage("奖励ID: {}", rewardId);
    handler->PSendSysMessage("剩余次数: {}", count);
    handler->PSendSysMessage("公告类型: {}", announceType);

    return true;
}

bool RedemptionCodeMgr::DeleteCode(std::string const& code)
{
    if (!m_enabled)
        return false;

    // 【安全修复】白名单校验后再拼 SQL
    if (!IsValidCodeFormat(code))
        return false;

    // 检查兑换码是否存在
    if (!CodeExists(code))
        return false;

    // 删除兑换码
    WorldDatabase.Execute("DELETE FROM `_奖励_兑换码` WHERE `兑换码` = '{}'", code);

    return true;
}

bool RedemptionCodeMgr::CodeExists(std::string const& code)
{
    if (!IsValidCodeFormat(code))
        return false;

    QueryResult result = WorldDatabase.Query("SELECT 1 FROM `_奖励_兑换码` WHERE `兑换码` = '{}'", code);
    return result != nullptr;
}

bool RedemptionCodeMgr::HasPlayerUsedCode(Player* player, std::string const& code)
{
    if (!player)
        return true;

    if (!IsValidCodeFormat(code))
        return true;

    QueryResult result = WorldDatabase.Query(
        "SELECT `兑换角色`, `兑换账号`, `兑换IP` FROM `_奖励_兑换码` WHERE `兑换码` = '{}'", code);

    if (!result)
        return true;

    Field* fields = result->Fetch();
    std::string usedChars = fields[0].Get<std::string>();
    std::string usedAccounts = fields[1].Get<std::string>();
    std::string usedIPs = fields[2].Get<std::string>();

    // 【修复】原实现用 find 子串匹配：GUID 23 会命中记录 123、账号 5 命中 15，
    // 低位数字玩家被误判"已使用"。改为按逗号切分后整段精确比较（空段跳过）。
    auto containsToken = [](std::string const& list, std::string const& token)
    {
        size_t start = 0;
        while (start <= list.size())
        {
            size_t end = list.find(',', start);
            if (end == std::string::npos)
                end = list.size();

            if (end > start && list.compare(start, end - start, token) == 0)
                return true;

            start = end + 1;
        }
        return false;
    };

    // 检查角色是否已使用
    if (!usedChars.empty() && containsToken(usedChars, std::to_string(player->GetGUID().GetCounter())))
        return true;

    // 检查账号是否已使用
    if (!usedAccounts.empty() && containsToken(usedAccounts, std::to_string(player->GetSession()->GetAccountId())))
        return true;

    // 检查IP是否已使用
    if (!usedIPs.empty() && containsToken(usedIPs, player->GetSession()->GetRemoteAddress()))
        return true;

    return false;
}

void RedemptionCodeMgr::RecordCodeUsage(Player* player, std::string const& code)
{
    if (!player)
        return;

    if (!IsValidCodeFormat(code))
        return;

    std::string charGuid = std::to_string(player->GetGUID().GetCounter());
    std::string accountId = std::to_string(player->GetSession()->GetAccountId());
    std::string ip = player->GetSession()->GetRemoteAddress();

    // 更新使用记录
    // 【修复】同步写入：异步写对紧随其后的 HasPlayerUsedCode 同步读不可见，快速连点可重复兑换
    WorldDatabase.DirectExecute(
        "UPDATE `_奖励_兑换码` SET "
        "`兑换角色` = CONCAT(IFNULL(`兑换角色`, ''), ',', '{}'), "
        "`兑换账号` = CONCAT(IFNULL(`兑换账号`, ''), ',', '{}'), "
        "`兑换IP` = CONCAT(IFNULL(`兑换IP`, ''), ',', '{}') "
        "WHERE `兑换码` = '{}'",
        charGuid, accountId, ip, code);
}

// ============================================================================
// 懒加载接口获取方法
// ============================================================================

RequirementInterface* RedemptionCodeMgr::GetRequirementInterface()
{
    // 懒加载：如果接口为空，尝试从模块管理器获取
    if (!m_requirementInterface)
    {
        m_requirementInterface = sModuleManager->GetRequirementModule();
    }
    return m_requirementInterface;
}

RewardInterface* RedemptionCodeMgr::GetRewardInterface()
{
    // 懒加载：如果接口为空，尝试从模块管理器获取
    if (!m_rewardInterface)
    {
        m_rewardInterface = sModuleManager->GetRewardModule();
    }
    return m_rewardInterface;
}

bool RedemptionCodeMgr::IsRequirementSystemAvailable()
{
    return GetRequirementInterface() != nullptr;
}

bool RedemptionCodeMgr::IsRewardSystemAvailable()
{
    return GetRewardInterface() != nullptr;
}

bool RedemptionCodeMgr::GiveReward(Player* player, uint32 rewardId)
{
    if (!player || rewardId == 0)
        return false;

    // 【懒加载】动态获取奖励系统接口
    RewardInterface* rewardInterface = GetRewardInterface();

    // 如果奖励系统接口存在，使用奖励系统发放奖励
    // 第四个参数 false 表示不显示奖励提示（由兑换码模块统一显示）
    if (rewardInterface)
    {
        bool success = rewardInterface->GiveReward(player, rewardId, true, false);
        if (success)
        {
            LOG_INFO("module.redemption", "玩家 {} 成功领取奖励ID: {}", player->GetName(), rewardId);
        }
        else
        {
            LOG_ERROR("module.redemption", "玩家 {} 领取奖励ID {} 失败", player->GetName(), rewardId);
        }
        return success;
    }

    // 如果没有奖励系统，返回失败
    LOG_WARN("module.redemption", "奖励系统未加载，无法发放奖励ID: {}", rewardId);
    return false;
}

bool RedemptionCodeMgr::GiveRewardWithReceipt(Player* player, uint32 rewardId, RewardGrantReceipt& receipt)
{
    receipt = {};
    if (!player || rewardId == 0)
        return false;

    RewardInterface* rewardInterface = GetRewardInterface();
    if (!rewardInterface)
    {
        LOG_WARN("module.redemption", "奖励系统未加载，无法发放奖励ID: {}", rewardId);
        return false;
    }

    bool success = rewardInterface->GiveRewardWithReceipt(player, rewardId, receipt, true, false);
    if (!success)
    {
        receipt = {};
        LOG_ERROR("module.redemption", "玩家 {} 领取奖励ID {} 失败", player->GetName(), rewardId);
    }
    return success;
}

bool RedemptionCodeMgr::GiveRewardWithoutItems(Player* player, uint32 rewardId)
{
    if (!player || rewardId == 0)
        return false;

    RewardInterface* rewardInterface = GetRewardInterface();
    if (!rewardInterface)
    {
        LOG_WARN("module.redemption", "奖励系统未加载，无法发放奖励ID: {}", rewardId);
        return false;
    }

    RewardTemplate* rewardTemplate = dynamic_cast<RewardTemplate*>(rewardInterface);
    if (!rewardTemplate)
    {
        LOG_WARN("module.redemption", "当前奖励系统不支持跳过物品发放，奖励ID: {}", rewardId);
        return false;
    }

    bool success = rewardTemplate->GiveRewardWithoutItems(player, rewardId, false, false);
    if (!success)
        LOG_ERROR("module.redemption", "玩家 {} 领取奖励ID {} 的非物品奖励失败", player->GetName(), rewardId);

    return success;
}

bool RedemptionCodeMgr::GiveRewardWithoutItemsWithReceipt(
    Player* player,
    uint32 rewardId,
    RewardGrantReceipt& receipt)
{
    receipt = {};
    if (!player || rewardId == 0)
        return false;

    RewardInterface* rewardInterface = GetRewardInterface();
    if (!rewardInterface)
    {
        LOG_WARN("module.redemption", "奖励系统未加载，无法发放奖励ID: {}", rewardId);
        return false;
    }

    RewardTemplate* rewardTemplate = dynamic_cast<RewardTemplate*>(rewardInterface);
    if (!rewardTemplate)
    {
        LOG_WARN("module.redemption", "当前奖励系统不支持跳过物品发放，奖励ID: {}", rewardId);
        return false;
    }

    bool success = rewardTemplate->GiveRewardWithoutItemsWithReceipt(player, rewardId, receipt, false, false);
    if (!success)
    {
        receipt = {};
        LOG_ERROR("module.redemption", "玩家 {} 领取奖励ID {} 的非物品奖励失败", player->GetName(), rewardId);
    }
    return success;
}

bool RedemptionCodeMgr::CheckRequirement(Player* player, uint32 requireId)
{
    if (!player)
        return false;

    // 如果没有需求，直接返回成功
    if (requireId == 0)
        return true;

    // 【懒加载】动态获取需求系统接口
    RequirementInterface* requirementInterface = GetRequirementInterface();

    // 如果需求系统接口存在，使用需求系统检查需求
    if (requirementInterface)
    {
        bool success = requirementInterface->CheckRequirements(player, requireId, true);
        if (!success)
        {
            LOG_INFO("module.redemption", "玩家 {} 不满足需求ID: {}", player->GetName(), requireId);
        }
        return success;
    }

    // 如果没有需求系统，认为满足需求
    LOG_WARN("module.redemption", "需求系统未加载，跳过需求检查ID: {}", requireId);
    return true;
}

std::vector<std::string> RedemptionCodeMgr::GetRewardDescription(Player* player, uint32 rewardId)
{
    std::vector<std::string> descriptions;

    // 【懒加载】动态获取奖励系统接口
    RewardInterface* rewardInterface = GetRewardInterface();

    if (rewardInterface)
    {
        return rewardInterface->GetRewardDescription(player, rewardId);
    }

    descriptions.push_back("奖励系统未连接，无法获取奖励描述");
    return descriptions;
}

RedemptionCode_WorldScript::RedemptionCode_WorldScript()
    : WorldScript("RedemptionCode_WorldScript"),
    m_initialized(false),
    m_initTimer(0),
    m_loadedCount(0)
{
    // 【优化】不再需要等待其他模块注册，因为使用懒加载模式
    // 设置较短的初始化延迟即可
    m_initDelay = 500; // 0.5秒，仅用于确保服务器基本启动完成
}

void RedemptionCode_WorldScript::OnAfterConfigLoad(bool reload)
{
    // 重新加载配置
    if (reload)
    {
        sRedemptionCodeMgr->Initialize();
    }
}

void RedemptionCode_WorldScript::OnStartup()
{
    // 在OnUpdate中处理初始化
    m_initialized = false;
    m_initTimer = 0;
}

void RedemptionCode_WorldScript::OnUpdate(uint32 diff)
{
    // 如果已经初始化，则不再处理
    if (m_initialized)
        return;

    // 累加时间
    m_initTimer += diff;

    // 检查是否达到初始化延迟
    if (m_initTimer >= m_initDelay)
    {
        // 初始化模块
        sRedemptionCodeMgr->Initialize();

        // 标记为已初始化
        m_initialized = true;

        // 查询数据库中的兑换码数量
        QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `_奖励_兑换码`");
        if (result)
        {
            Field* fields = result->Fetch();
            m_loadedCount = fields[0].Get<uint32>();
        }

        LOG_INFO("server.loading", "→兑换码系统√");
    }
}

// 添加脚本
void AddSC_RedemptionCode_WorldScript()
{
    new RedemptionCode_WorldScript();
}

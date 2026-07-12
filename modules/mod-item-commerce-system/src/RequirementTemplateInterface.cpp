/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Logging/Log.h"
#include <sstream>
#include <limits>

namespace
{
    // 需求_模板.需要人物等级 来自数据库，可能存在脏数据；stoi 抛出的异常若未捕获会直接终止进程
    bool TryParseLevelReq(std::string const& value, int& out)
    {
        try
        {
            out = std::stoi(value);
            return true;
        }
        catch (std::exception const&)
        {
            return false;
        }
    }
}

// 提供一个简单的接口函数，用于检查需求模板
bool CheckRequirementTemplate(Player* player, uint32 templateId, bool showMessages)
{
    if (!player || templateId == 0)
        return true;

    // 直接查询数据库获取需求信息
    QueryResult result = WorldDatabase.Query(
        "SELECT `需要人物等级`, `消耗金币`, `消耗物品`, `是否消耗物品` FROM `需求_模板` WHERE `id` = {}", templateId);

    if (!result)
        return true; // 如果找不到模板，默认允许

    Field* fields = result->Fetch();
    std::string levelReq = fields[0].Get<std::string>();
    uint64 goldCost = fields[1].Get<uint64>();
    std::string itemsReq = fields[2].Get<std::string>();
    int consumeItems = fields[3].Get<int>();

    bool meetsRequirements = true;
    ChatHandler handler(player->GetSession());

    // 检查等级要求
    if (!levelReq.empty() && levelReq != "0")
    {
        int reqLevel = 0;
        bool parsed = false;
        // 简单处理：只检查大于等于
        if (levelReq[0] == '>')
        {
            parsed = TryParseLevelReq(levelReq.substr(1), reqLevel);
            if (parsed && player->GetLevel() <= reqLevel)
            {
                meetsRequirements = false;
                if (showMessages)
                    handler.PSendSysMessage("您的等级不满足要求: {}", levelReq.c_str());
            }
        }
        else if (levelReq[0] == '<')
        {
            parsed = TryParseLevelReq(levelReq.substr(1), reqLevel);
            if (parsed && player->GetLevel() >= reqLevel)
            {
                meetsRequirements = false;
                if (showMessages)
                    handler.PSendSysMessage("您的等级不满足要求: {}", levelReq.c_str());
            }
        }
        else if (levelReq.substr(0, 2) == "!=")
        {
            parsed = TryParseLevelReq(levelReq.substr(2), reqLevel);
            if (parsed && player->GetLevel() == reqLevel)
            {
                meetsRequirements = false;
                if (showMessages)
                    handler.PSendSysMessage("您的等级不满足要求: {}", levelReq.c_str());
            }
        }
        else if (levelReq[0] == '=')
        {
            parsed = TryParseLevelReq(levelReq.substr(1), reqLevel);
            if (parsed && player->GetLevel() != reqLevel)
            {
                meetsRequirements = false;
                if (showMessages)
                    handler.PSendSysMessage("您的等级不满足要求: {}", levelReq.c_str());
            }
        }
        else
        {
            parsed = TryParseLevelReq(levelReq, reqLevel);
            if (parsed && player->GetLevel() < reqLevel)
            {
                meetsRequirements = false;
                if (showMessages)
                    handler.PSendSysMessage("您的等级不满足要求: 需要等级 {}", reqLevel);
            }
        }

        if (!parsed)
        {
            LOG_ERROR("module", "需求_模板 id={} 的 需要人物等级 字段格式非法: '{}'，按不满足条件处理", templateId, levelReq);
            meetsRequirements = false;
            if (showMessages)
                handler.PSendSysMessage("需求模板配置错误，请联系管理员 (模板ID: {})", templateId);
        }
    }

    // 检查金币要求
    if (goldCost > 0)
    {
        uint64 copperCost = goldCost > std::numeric_limits<uint64>::max() / 10000 ? std::numeric_limits<uint64>::max() : goldCost * 10000;
        if (player->GetMoney() < copperCost) // 转换为铜币
        {
            meetsRequirements = false;
            if (showMessages)
                handler.PSendSysMessage("您的金币不足: 需要 {} 金币", goldCost);
        }
    }

    // 检查物品要求
    if (!itemsReq.empty() && itemsReq != "0")
    {
        std::istringstream ss(itemsReq);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            std::istringstream itemSS(token);
            uint32 itemId, count;
            itemSS >> itemId >> count;

            if (itemId > 0 && count > 0)
            {
                if (player->GetItemCount(itemId, false) < count)
                {
                    meetsRequirements = false;
                    if (showMessages)
                        handler.PSendSysMessage("您的物品不足: 需要物品ID {} x{}", itemId, count);
                    break;
                }
            }
        }
    }

    return meetsRequirements;
}

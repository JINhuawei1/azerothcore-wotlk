/*
 * 天赋之魂系统 - GM命令
 */

#include "TalentSoul.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "WorldSession.h"
#include "Configuration/Config.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <sstream>

using namespace Acore::ChatCommands;

// Addon消息前缀
static constexpr char const* TALENT_SOUL_ADDON_PREFIX = "TALENTSOUL";

// 职业名称映射
static const char* GetClassName(uint32 classType)
{
    switch (classType)
    {
        case 0: return "全职业";
        case 1: return "战士";
        case 2: return "圣骑士";
        case 3: return "猎人";
        case 4: return "盗贼";
        case 5: return "牧师";
        case 6: return "死亡骑士";
        case 7: return "萨满";
        case 8: return "法师";
        case 9: return "术士";
        case 11: return "德鲁伊";
        default: return "未知";
    }
}

class TalentSoulCommandScript : public CommandScript
{
public:
    TalentSoulCommandScript() : CommandScript("TalentSoulCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable talentSoulUpgradeTable =
        {
            { "公共cd",   HandleUpgradeGCDCommand,      SEC_PLAYER,        Console::No },
            { "冷却",     HandleUpgradeCooldownCommand, SEC_PLAYER,        Console::No },
            { "消耗",     HandleUpgradeCostCommand,     SEC_PLAYER,        Console::No },
            { "伤害",     HandleUpgradeDamageCommand,   SEC_PLAYER,        Console::No },
        };

        static ChatCommandTable talentSoulCommandTable =
        {
            { "重载",     HandleTalentSoulReloadCommand,  SEC_ADMINISTRATOR, Console::Yes },
            { "信息",     HandleTalentSoulInfoCommand,    SEC_GAMEMASTER,    Console::No  },
            { "列表",     HandleTalentSoulListCommand,    SEC_GAMEMASTER,    Console::Yes },
            { "查看",     HandleTalentSoulCheckCommand,   SEC_PLAYER,        Console::No  },
            { "我的",     HandleTalentSoulMyDataCommand,  SEC_PLAYER,        Console::No  },
            { "天赋点",   HandleTalentSoulPointsCommand,  SEC_PLAYER,        Console::No  },
            { "升级",     talentSoulUpgradeTable },
            { "设置",     HandleTalentSoulSetCommand,     SEC_ADMINISTRATOR, Console::No  },
            { "重置",     HandleTalentSoulResetCommand,   SEC_ADMINISTRATOR, Console::No  },
            { "界面",     HandleTalentSoulOpenUICommand,  SEC_PLAYER,        Console::No  },
            { "ui",       HandleTalentSoulOpenUICommand,  SEC_PLAYER,        Console::No  },
        };

        static ChatCommandTable commandTable =
        {
            { "天赋之魂", talentSoulCommandTable },
        };

        return commandTable;
    }

    // 重载天赋之魂数据
    static bool HandleTalentSoulReloadCommand(ChatHandler* handler, Optional<std::string> /*args*/)
    {
        sTalentSoulMgr->LoadTalentSoulData();
        handler->PSendSysMessage("天赋之魂数据已重载，共加载 {} 条记录", sTalentSoulMgr->GetDataCount());
        return true;
    }

    // 查看指定技能的天赋之魂配置信息
    static bool HandleTalentSoulInfoCommand(ChatHandler* handler, uint32 spellId)
    {
        TalentSoulData const* data = sTalentSoulMgr->GetTalentSoulData(spellId);

        if (!data)
        {
            handler->PSendSysMessage("未找到技能ID {} 的天赋之魂配置", spellId);
            return true;
        }

        handler->PSendSysMessage("=== 天赋之魂配置信息 ===");
        handler->PSendSysMessage("ID: {}", data->id);
        handler->PSendSysMessage("职业: {} ({})", GetClassName(data->classType), data->classType);
        handler->PSendSysMessage("天赋点需求: {}", data->talentPointCost);
        handler->PSendSysMessage("技能ID: {}", data->spellId);
        handler->PSendSysMessage("公共CD: 每级-{:.2f}% (上限{}级)", data->gcdPerLevel, data->gcdMaxLevel);
        handler->PSendSysMessage("技能冷却: 每级-{:.2f}% (上限{}级)", data->cooldownPerLevel, data->cooldownMaxLevel);
        handler->PSendSysMessage("技能消耗: 每级-{:.2f}% (上限{}级)", data->costPerLevel, data->costMaxLevel);
        handler->PSendSysMessage("伤害加成: 每级+{:.2f}% (上限{}级)", data->damagePerLevel, data->damageMaxLevel);
        handler->PSendSysMessage("效果描述: {}", data->description);

        return true;
    }

    // 列出所有天赋之魂配置数据
    static bool HandleTalentSoulListCommand(ChatHandler* handler, Optional<uint32> page)
    {
        uint32 pageNum = page.value_or(1);
        uint32 pageSize = 10;

        auto const& allData = sTalentSoulMgr->GetAllTalentSoulData();

        if (allData.empty())
        {
            handler->PSendSysMessage("没有天赋之魂配置数据");
            return true;
        }

        uint32 totalCount = static_cast<uint32>(allData.size());
        uint32 totalPages = (totalCount + pageSize - 1) / pageSize;

        if (pageNum > totalPages)
            pageNum = totalPages;

        uint32 startIndex = (pageNum - 1) * pageSize;
        uint32 endIndex = std::min(startIndex + pageSize, totalCount);

        handler->PSendSysMessage("=== 天赋之魂配置列表 (第 {}/{} 页) ===", pageNum, totalPages);

        uint32 index = 0;
        for (auto const& pair : allData)
        {
            if (index >= startIndex && index < endIndex)
            {
                TalentSoulData const& data = pair.second;
                handler->PSendSysMessage("[{}] 技能:{} 职业:{} 天赋点:{}",
                    data.id, data.spellId, GetClassName(data.classType), data.talentPointCost);
            }
            ++index;
        }

        handler->PSendSysMessage("总计: {} 条数据", totalCount);

        return true;
    }

    // 检查技能的天赋之魂配置
    static bool HandleTalentSoulCheckCommand(ChatHandler* handler, uint32 spellId)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);

        if (!config)
        {
            handler->PSendSysMessage("技能ID {} 没有配置天赋之魂效果", spellId);
            return true;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerSkillData* skillData = sTalentSoulMgr->GetPlayerSkillData(playerGuid, spellId);

        handler->PSendSysMessage("=== 技能 {} 天赋之魂 ===", spellId);
        handler->PSendSysMessage("职业限制: {} | 天赋点需求: {}",
            GetClassName(config->classType), config->talentPointCost);
        handler->PSendSysMessage("配置: GCD每级-{:.2f}% CD每级-{:.2f}% 消耗每级-{:.2f}% 伤害每级+{:.2f}%",
            config->gcdPerLevel, config->cooldownPerLevel, config->costPerLevel, config->damagePerLevel);

        if (skillData)
        {
            handler->PSendSysMessage("你的等级: GCD:{}/{} CD:{}/{} 消耗:{}/{} 伤害:{}/{}",
                skillData->gcdLevel, config->gcdMaxLevel,
                skillData->cooldownLevel, config->cooldownMaxLevel,
                skillData->costLevel, config->costMaxLevel,
                skillData->damageLevel, config->damageMaxLevel);

            float gcdReduction = sTalentSoulMgr->GetPlayerGCDReduction(playerGuid, spellId);
            float cdReduction = sTalentSoulMgr->GetPlayerCooldownReduction(playerGuid, spellId);
            float costReduction = sTalentSoulMgr->GetPlayerCostReduction(playerGuid, spellId);
            float damageBonus = sTalentSoulMgr->GetPlayerDamageBonus(playerGuid, spellId);

            handler->PSendSysMessage("实际效果: GCD-{:.2f}% CD-{:.2f}% 消耗-{:.2f}% 伤害+{:.2f}%",
                gcdReduction, cdReduction, costReduction, damageBonus);
        }
        else
        {
            handler->PSendSysMessage("你尚未升级此技能");
        }

        return true;
    }

    // 查看玩家天赋点信息
    static bool HandleTalentSoulPointsCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 totalPoints = player->CalculateTalentsPoints();
        uint32 usedPoints = sTalentSoulMgr->GetPlayerUsedTalentPoints(player->GetGUID().GetCounter());
        uint32 availablePoints = sTalentSoulMgr->GetPlayerAvailableTalentPoints(player);

        handler->PSendSysMessage("=== 天赋点信息 ===");
        handler->PSendSysMessage("总天赋点: {} (含倍率)", totalPoints);
        handler->PSendSysMessage("已使用: {}", usedPoints);
        handler->PSendSysMessage("可用: {}", availablePoints);

        return true;
    }

    // 查看玩家自己的所有天赋之魂数据
    static bool HandleTalentSoulMyDataCommand(ChatHandler* handler, Optional<uint32> page)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerTalentSoulData* playerData = sTalentSoulMgr->GetPlayerData(playerGuid);

        if (!playerData || playerData->skills.empty())
        {
            handler->PSendSysMessage("你还没有任何天赋之魂数据");
            return true;
        }

        uint32 pageNum = page.value_or(1);
        uint32 pageSize = 10;
        uint32 totalCount = static_cast<uint32>(playerData->skills.size());
        uint32 totalPages = (totalCount + pageSize - 1) / pageSize;

        if (pageNum > totalPages)
            pageNum = totalPages;

        uint32 startIndex = (pageNum - 1) * pageSize;
        uint32 endIndex = std::min(startIndex + pageSize, totalCount);

        handler->PSendSysMessage("=== 我的天赋之魂 (第 {}/{} 页) ===", pageNum, totalPages);
        handler->PSendSysMessage("已使用天赋点: {} | 可用: {}",
            playerData->usedTalentPoints,
            sTalentSoulMgr->GetPlayerAvailableTalentPoints(player));

        uint32 index = 0;
        for (auto const& pair : playerData->skills)
        {
            if (index >= startIndex && index < endIndex)
            {
                PlayerSkillData const& skill = pair.second;
                TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(skill.spellId);

                if (config)
                {
                    handler->PSendSysMessage("技能{}: GCD:{}/{} CD:{}/{} 消耗:{}/{} 伤害:{}/{}",
                        skill.spellId,
                        skill.gcdLevel, config->gcdMaxLevel,
                        skill.cooldownLevel, config->cooldownMaxLevel,
                        skill.costLevel, config->costMaxLevel,
                        skill.damageLevel, config->damageMaxLevel);
                }
            }
            ++index;
        }

        handler->PSendSysMessage("总计: {} 个技能", totalCount);

        return true;
    }

    // 升级公共CD
    static bool HandleUpgradeGCDCommand(ChatHandler* handler, uint32 spellId)
    {
        return HandleUpgrade(handler, spellId, TALENT_SOUL_UPGRADE_GCD, "公共CD");
    }

    // 升级技能冷却
    static bool HandleUpgradeCooldownCommand(ChatHandler* handler, uint32 spellId)
    {
        return HandleUpgrade(handler, spellId, TALENT_SOUL_UPGRADE_COOLDOWN, "技能冷却");
    }

    // 升级技能消耗
    static bool HandleUpgradeCostCommand(ChatHandler* handler, uint32 spellId)
    {
        return HandleUpgrade(handler, spellId, TALENT_SOUL_UPGRADE_COST, "技能消耗");
    }

    // 升级伤害加成
    static bool HandleUpgradeDamageCommand(ChatHandler* handler, uint32 spellId)
    {
        return HandleUpgrade(handler, spellId, TALENT_SOUL_UPGRADE_DAMAGE, "伤害加成");
    }

    // 通用升级处理
    static bool HandleUpgrade(ChatHandler* handler, uint32 spellId, TalentSoulUpgradeType type, const char* typeName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 先检查是否可以升级
        std::string errorMsg;
        if (!sTalentSoulMgr->CanUpgradeSpell(player, spellId, errorMsg))
        {
            handler->PSendSysMessage("无法升级: {}", errorMsg.c_str());
            return true;
        }

        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);
        if (!config)
        {
            handler->PSendSysMessage("技能ID {} 没有配置天赋之魂效果", spellId);
            return true;
        }

        if (sTalentSoulMgr->UpgradePlayerSpell(player, spellId, type))
        {
            PlayerSkillData* data = sTalentSoulMgr->GetPlayerSkillData(player->GetGUID().GetCounter(), spellId);
            uint32 currentLevel = 0;
            uint32 maxLevel = 0;

            switch (type)
            {
                case TALENT_SOUL_UPGRADE_GCD:
                    currentLevel = data ? data->gcdLevel : 0;
                    maxLevel = config->gcdMaxLevel;
                    break;
                case TALENT_SOUL_UPGRADE_COOLDOWN:
                    currentLevel = data ? data->cooldownLevel : 0;
                    maxLevel = config->cooldownMaxLevel;
                    break;
                case TALENT_SOUL_UPGRADE_COST:
                    currentLevel = data ? data->costLevel : 0;
                    maxLevel = config->costMaxLevel;
                    break;
                case TALENT_SOUL_UPGRADE_DAMAGE:
                    currentLevel = data ? data->damageLevel : 0;
                    maxLevel = config->damageMaxLevel;
                    break;
                default:
                    break;
            }

            handler->PSendSysMessage("技能 {} 的{}升级成功！当前等级: {}/{}",
                spellId, typeName, currentLevel, maxLevel);
        }
        else
        {
            handler->PSendSysMessage("技能 {} 的{}已达到最大等级，无法继续升级", spellId, typeName);
        }

        return true;
    }

    // GM设置玩家技能等级 (使用紧凑格式)
    static bool HandleTalentSoulSetCommand(ChatHandler* handler, uint32 spellId, uint32 gcdLv, uint32 cdLv, uint32 costLv, uint32 damageLv)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Player* target = handler->getSelectedPlayer();

        if (!target)
            target = player;

        if (!target)
            return false;

        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);
        if (!config)
        {
            handler->PSendSysMessage("技能ID {} 没有配置天赋之魂效果", spellId);
            return true;
        }

        // 限制等级不超过上限
        gcdLv = std::min(gcdLv, config->gcdMaxLevel);
        cdLv = std::min(cdLv, config->cooldownMaxLevel);
        costLv = std::min(costLv, config->costMaxLevel);
        damageLv = std::min(damageLv, config->damageMaxLevel);

        uint32 targetGuid = target->GetGUID().GetCounter();

        // 获取或创建玩家数据
        PlayerTalentSoulData* playerData = sTalentSoulMgr->GetPlayerData(targetGuid);
        if (!playerData)
        {
            // 需要先加载玩家数据
            sTalentSoulMgr->LoadPlayerData(target);
            playerData = sTalentSoulMgr->GetPlayerData(targetGuid);
        }

        if (playerData)
        {
            // 检查是否是新技能
            bool isNew = playerData->skills.find(spellId) == playerData->skills.end();

            // 更新技能数据
            PlayerSkillData& skill = playerData->skills[spellId];
            skill.spellId = spellId;
            skill.gcdLevel = gcdLv;
            skill.cooldownLevel = cdLv;
            skill.costLevel = costLv;
            skill.damageLevel = damageLv;

            // 如果是新技能，增加天赋点消耗
            if (isNew && config->talentPointCost > 0)
            {
                playerData->usedTalentPoints += config->talentPointCost;
            }

            // 保存数据
            sTalentSoulMgr->SavePlayerData(target);
        }

        handler->PSendSysMessage("已设置玩家 {} 的技能 {}: GCD:{} CD:{} 消耗:{} 伤害:{}",
            target->GetName().c_str(), spellId, gcdLv, cdLv, costLv, damageLv);

        return true;
    }

    // GM重置玩家技能等级
    static bool HandleTalentSoulResetCommand(ChatHandler* handler, Optional<uint32> spellId)
    {
        Player* target = handler->getSelectedPlayer();
        if (!target)
        {
            handler->PSendSysMessage("请选择一个玩家");
            return true;
        }

        uint32 targetGuid = target->GetGUID().GetCounter();

        if (spellId.has_value())
        {
            // 重置指定技能
            PlayerTalentSoulData* playerData = sTalentSoulMgr->GetPlayerData(targetGuid);
            if (playerData)
            {
                auto it = playerData->skills.find(spellId.value());
                if (it != playerData->skills.end())
                {
                    // 返还天赋点
                    TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId.value());
                    if (config && config->talentPointCost > 0)
                    {
                        if (playerData->usedTalentPoints >= config->talentPointCost)
                            playerData->usedTalentPoints -= config->talentPointCost;
                    }

                    playerData->skills.erase(it);
                    sTalentSoulMgr->SavePlayerData(target);
                }
            }
            handler->PSendSysMessage("已重置玩家 {} 的技能 {} 天赋之魂数据",
                target->GetName().c_str(), spellId.value());
        }
        else
        {
            // 重置所有技能 - 使用 DirectExecute 同步执行
            CharacterDatabase.DirectExecute(
                "DELETE FROM `_天赋之魂_玩家数据` WHERE `角色id` = {}",
                targetGuid);

            // 重新加载玩家数据（会清空内存数据）
            sTalentSoulMgr->LoadPlayerData(target);

            handler->PSendSysMessage("已重置玩家 {} 的所有天赋之魂数据",
                target->GetName().c_str());
        }

        return true;
    }

    // .天赋之魂 界面 / .天赋之魂 ui - 打开UI界面
    static bool HandleTalentSoulOpenUICommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 发送打开UI界面的Addon消息
        std::string fullMessage = std::string(TALENT_SOUL_ADDON_PREFIX) + "\tOPEN_UI";
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);

        return true;
    }
};

void AddSC_TalentSoulCommands()
{
    new TalentSoulCommandScript();
}

// ============================================================
// Addon 消息处理脚本 - 为客户端插件提供结构化接口
// ============================================================

class TalentSoulAddonScript : public PlayerScript
{
public:
    TalentSoulAddonScript() : PlayerScript("TalentSoulAddonScript") { }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        {
            return;
        }

        // 服务器收到的格式: "前缀\t消息"
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
        {
            return;
        }

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != TALENT_SOUL_ADDON_PREFIX)
        {
            return;
        }

        std::string command = msg.substr(tabPos + 1);

        // 处理技能列表请求 - 格式: SKILL_LIST 或 SKILL_LIST:职业ID
        if (command == "SKILL_LIST" || command.rfind("SKILL_LIST:", 0) == 0)
        {
            std::string classArg = "";
            if (command.rfind("SKILL_LIST:", 0) == 0)
            {
                classArg = command.substr(strlen("SKILL_LIST:"));
            }
            HandleSkillList(player, classArg);
        }
        else if (command.rfind("SKILL_UPGRADE:", 0) == 0)
        {
            HandleSkillUpgrade(player, command.substr(strlen("SKILL_UPGRADE:")));
        }
        else if (command == "RESET_TALENT")
        {
            HandleResetTalent(player);
        }
        else if (command == "TALENT_POINTS")
        {
            HandleTalentPoints(player);
        }
    }

private:
    // Addon消息最大有效载荷长度 (255 - 前缀长度 - 分隔符)
    static constexpr size_t MAX_ADDON_PAYLOAD = 200;

    // 向客户端发送Addon消息（自动分块）
    void SendAddonMessage(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
        {
            return;
        }

        // 如果消息足够短，直接发送
        if (payload.length() <= MAX_ADDON_PAYLOAD)
        {
            std::string fullMessage = std::string(TALENT_SOUL_ADDON_PREFIX) + '\t' + payload;
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
            return;
        }

        // 消息过长，需要分块发送
        // 格式: CHUNK:当前块:总块数:数据
        size_t totalChunks = (payload.length() + MAX_ADDON_PAYLOAD - 1) / MAX_ADDON_PAYLOAD;

        for (size_t i = 0; i < totalChunks; ++i)
        {
            size_t start = i * MAX_ADDON_PAYLOAD;
            size_t len = std::min(MAX_ADDON_PAYLOAD, payload.length() - start);
            std::string chunk = payload.substr(start, len);

            std::ostringstream chunkMsg;
            chunkMsg << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << chunk;

            std::string fullMessage = std::string(TALENT_SOUL_ADDON_PREFIX) + '\t' + chunkMsg.str();
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
            player->SendDirectMessage(&data);
        }
    }

    // 处理技能列表请求
    // 格式: SKILL_LIST:职业ID (职业ID为0表示全部职业，不传参数则返回玩家当前职业)
    void HandleSkillList(Player* player, std::string const& args)
    {
        if (!player)
            return;

        uint8 playerClass = player->getClass();
        uint32 playerGuid = player->GetGUID().GetCounter();
        // 使用官方API获取天赋点 (已包含rate.talent倍率)
        uint32 totalPoints = player->CalculateTalentsPoints();
        uint32 usedPoints = sTalentSoulMgr->GetPlayerUsedTalentPoints(playerGuid);

        // 解析请求的职业ID
        uint32 requestedClass = playerClass;  // 默认为玩家当前职业
        if (!args.empty())
        {
            try
            {
                requestedClass = std::stoul(args);
            }
            catch (...)
            {
                requestedClass = playerClass;
            }
        }

        // 获取所有技能配置
        auto const& allData = sTalentSoulMgr->GetAllTalentSoulData();

        // 构建响应: TALENTSOUL_SKILLS:请求的职业:玩家职业:总天赋点:已用天赋点:技能数据列表
        std::ostringstream response;
        response << "TALENTSOUL_SKILLS:" << requestedClass << ":" << static_cast<uint32>(playerClass) << ":" << totalPoints << ":" << usedPoints << ":";

        bool first = true;
        for (auto const& pair : allData)
        {
            TalentSoulData const& config = pair.second;

            // 按职业过滤：requestedClass为0表示全部职业，否则只返回匹配的职业技能
            // 技能的classType为0表示全职业通用
            if (requestedClass != 0 && config.classType != 0 && config.classType != requestedClass)
                continue;

            if (!first)
                response << ";";
            first = false;

            // 获取玩家该技能的数据
            PlayerSkillData* skillData = sTalentSoulMgr->GetPlayerSkillData(playerGuid, config.spellId);
            uint32 gcdLevel = skillData ? skillData->gcdLevel : 0;
            uint32 cdLevel = skillData ? skillData->cooldownLevel : 0;
            uint32 costLevel = skillData ? skillData->costLevel : 0;
            uint32 damageLevel = skillData ? skillData->damageLevel : 0;

            // 转义描述中的特殊字符
            std::string desc = config.description;
            for (char& ch : desc)
            {
                if (ch == ',' || ch == ';' || ch == '\n' || ch == '\r')
                    ch = ' ';
            }

            // 格式: 技能ID,职业ID,GCD等级,CD等级,消耗等级,伤害等级,GCD上限,CD上限,消耗上限,伤害上限,需要天赋点,描述
            response << config.spellId << ","
                     << config.classType << ","
                     << gcdLevel << ","
                     << cdLevel << ","
                     << costLevel << ","
                     << damageLevel << ","
                     << config.gcdMaxLevel << ","
                     << config.cooldownMaxLevel << ","
                     << config.costMaxLevel << ","
                     << config.damageMaxLevel << ","
                     << config.talentPointCost << ","
                     << desc;
        }

        SendAddonMessage(player, response.str());
    }

    // 处理技能升级请求
    // 格式: spellId:upgradeType (upgradeType: 1=GCD, 2=冷却, 3=消耗, 4=伤害)
    void HandleSkillUpgrade(Player* player, std::string const& args)
    {
        if (!player)
            return;

        // 解析参数
        uint32 spellId = 0;
        uint32 upgradeType = 0;

        try
        {
            size_t colonPos = args.find(':');
            if (colonPos == std::string::npos)
                return;

            spellId = std::stoul(args.substr(0, colonPos));
            upgradeType = std::stoul(args.substr(colonPos + 1));
        }
        catch (...)
        {
            return;
        }

        if (spellId == 0 || upgradeType == 0 || upgradeType > 4)
            return;

        // 转换升级类型 (客户端1-based -> 服务端0-based)
        TalentSoulUpgradeType type = static_cast<TalentSoulUpgradeType>(upgradeType - 1);

        // 检查是否可以升级
        std::string errorMsg;
        if (!sTalentSoulMgr->CanUpgradeSpell(player, spellId, errorMsg))
        {
            // 发送失败响应
            std::ostringstream failResponse;
            failResponse << "TALENTSOUL_UPGRADE_FAIL:" << spellId << ":" << upgradeType << ":NOT_ENOUGH_POINTS";
            SendAddonMessage(player, failResponse.str());

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[天赋之魂]|r {}", errorMsg.c_str());
            return;
        }

        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);
        if (!config)
        {
            std::ostringstream failResponse;
            failResponse << "TALENTSOUL_UPGRADE_FAIL:" << spellId << ":" << upgradeType << ":CONFIG_NOT_FOUND";
            SendAddonMessage(player, failResponse.str());
            return;
        }

        // 执行升级
        if (sTalentSoulMgr->UpgradePlayerSpell(player, spellId, type))
        {
            PlayerSkillData* data = sTalentSoulMgr->GetPlayerSkillData(player->GetGUID().GetCounter(), spellId);
            uint32 currentLevel = 0;

            switch (type)
            {
                case TALENT_SOUL_UPGRADE_GCD:
                    currentLevel = data ? data->gcdLevel : 0;
                    break;
                case TALENT_SOUL_UPGRADE_COOLDOWN:
                    currentLevel = data ? data->cooldownLevel : 0;
                    break;
                case TALENT_SOUL_UPGRADE_COST:
                    currentLevel = data ? data->costLevel : 0;
                    break;
                case TALENT_SOUL_UPGRADE_DAMAGE:
                    currentLevel = data ? data->damageLevel : 0;
                    break;
                default:
                    break;
            }

            uint32 remainPoints = sTalentSoulMgr->GetPlayerAvailableTalentPoints(player);

            // 发送成功响应
            std::ostringstream successResponse;
            successResponse << "TALENTSOUL_UPGRADE:" << spellId << ":" << upgradeType << ":" << currentLevel << ":" << remainPoints;
            SendAddonMessage(player, successResponse.str());

            const char* typeNames[] = { "公共CD", "技能冷却", "技能消耗", "伤害加成" };
            ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[天赋之魂]|r 技能 {} 的{}升级成功，当前等级: {}",
                spellId, typeNames[type], currentLevel);
        }
        else
        {
            // 升级失败（已达最大等级）
            std::ostringstream failResponse;
            failResponse << "TALENTSOUL_UPGRADE_FAIL:" << spellId << ":" << upgradeType << ":MAX_LEVEL";
            SendAddonMessage(player, failResponse.str());

            ChatHandler(player->GetSession()).PSendSysMessage("|cffffff00[天赋之魂]|r 该属性已达到最大等级");
        }
    }

    // 处理重置天赋请求
    void HandleResetTalent(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerTalentSoulData* playerData = sTalentSoulMgr->GetPlayerData(playerGuid);

        if (!playerData || playerData->skills.empty())
        {
            std::ostringstream failResponse;
            failResponse << "TALENTSOUL_RESET_FAIL:NO_DATA";
            SendAddonMessage(player, failResponse.str());

            ChatHandler(player->GetSession()).PSendSysMessage("|cffffff00[天赋之魂]|r 你还没有任何天赋数据");
            return;
        }

        // 计算返还的天赋点
        uint32 returnedPoints = playerData->usedTalentPoints;

        // 使用 DirectExecute 同步清空所有技能数据（确保在 LoadPlayerData 之前完成）
        CharacterDatabase.DirectExecute(
            "DELETE FROM `_天赋之魂_玩家数据` WHERE `角色id` = {}",
            playerGuid);

        // 重新加载玩家数据（清空内存）
        sTalentSoulMgr->LoadPlayerData(player);

        // 发送成功响应
        std::ostringstream successResponse;
        successResponse << "TALENTSOUL_RESET:" << returnedPoints;
        SendAddonMessage(player, successResponse.str());

        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[天赋之魂]|r 天赋重置成功，返还 {} 点天赋点", returnedPoints);
    }

    // 处理天赋点查询请求
    void HandleTalentPoints(Player* player)
    {
        if (!player)
            return;

        uint32 totalPoints = player->CalculateTalentsPoints();
        uint32 usedPoints = sTalentSoulMgr->GetPlayerUsedTalentPoints(player->GetGUID().GetCounter());
        uint32 availablePoints = sTalentSoulMgr->GetPlayerAvailableTalentPoints(player);

        // 发送响应
        std::ostringstream response;
        response << "TALENTSOUL_POINTS:" << totalPoints << ":" << usedPoints << ":" << availablePoints;
        SendAddonMessage(player, response.str());
    }
};

// 注册Addon脚本
void AddSC_TalentSoulAddon()
{
    new TalentSoulAddonScript();
}

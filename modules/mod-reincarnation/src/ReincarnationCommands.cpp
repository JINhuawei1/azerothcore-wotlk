/*
 * 转身系统 - 命令脚本
 *
 * 提供命令:
 * .转身 升级 [数量] - 升级转身等级，数量可为负数表示降级
 * .转身 设置 <等级> - 设置转身等级
 * .转身 重载 - 重载配置
 * .转身 信息 - 查看转身信息
 * .转身 界面 / .转身 ui - 打开UI界面
 */

#include "Reincarnation.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "Log.h"
#include "Language.h"
#include "WorldPacket.h"
#include "Opcodes.h"

using namespace Acore::ChatCommands;

// Addon消息前缀
static constexpr char const* REINCARNATION_ADDON_PREFIX = "ReincarnationUI";

class ReincarnationCommandScript : public CommandScript
{
public:
    ReincarnationCommandScript() : CommandScript("ReincarnationCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable reincarnationSubTable =
        {
            { "升级", HandleReincarnationUpgradeCommand, SEC_PLAYER,     Console::No },
            { "设置", HandleReincarnationSetCommand,     SEC_GAMEMASTER, Console::No },
            { "重载", HandleReincarnationReloadCommand,  SEC_GAMEMASTER, Console::Yes },
            { "信息", HandleReincarnationInfoCommand,    SEC_PLAYER,     Console::No },
            { "界面", HandleReincarnationOpenUICommand,  SEC_PLAYER,     Console::No },
            { "ui",   HandleReincarnationOpenUICommand,  SEC_PLAYER,     Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "转身", reincarnationSubTable },
        };

        return commandTable;
    }

    // .转身 升级 [数量]
    // 数量可以为正数(升级)或负数(降级)，默认为1
    static bool HandleReincarnationUpgradeCommand(ChatHandler* handler, Optional<int32> amount)
    {
        if (!sConfigMgr->GetOption("Reincarnation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000转身系统已禁用|r");
            return true;
        }

        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        int32 delta = amount.value_or(1);

        if (delta == 0)
        {
            handler->PSendSysMessage("|cffff0000数量不能为0|r");
            return true;
        }

        // 升级时检查等级要求
        if (delta > 0)
        {
            std::string errorMsg;
            if (!sReincarnationMgr->CanReincarnate(player, errorMsg))
            {
                handler->PSendSysMessage("|cffff0000转身失败: {}|r", errorMsg);
                return true;
            }

            // 执行多次转身
            for (int32 i = 0; i < delta; ++i)
            {
                if (!sReincarnationMgr->DoReincarnate(player))
                {
                    handler->PSendSysMessage("|cffff0000第{}次转身失败|r", i + 1);
                    break;
                }
            }
        }
        else
        {
            // 降级
            if (sReincarnationMgr->ModifyReincarnationLevel(player, delta))
            {
                uint32 playerGuid = player->GetGUID().GetCounter();
                uint32 newLevel = sReincarnationMgr->GetPlayerReincarnationLevel(playerGuid);
                float bonusStats = sReincarnationMgr->GetPlayerBonusStats(playerGuid);
                uint32 bonusTalent = sReincarnationMgr->GetPlayerBonusTalentPoints(playerGuid);

                handler->PSendSysMessage("|cff00ff00转身等级已降低到 {}|r", newLevel);
                handler->PSendSysMessage("|cff00ffff当前属性加成: {:.1f}%，天赋点: {}|r", bonusStats, bonusTalent);
            }
            else
            {
                handler->PSendSysMessage("|cffff0000降级失败|r");
            }
        }

        return true;
    }

    // .转身 设置 <等级>
    static bool HandleReincarnationSetCommand(ChatHandler* handler, uint32 level)
    {
        if (!sConfigMgr->GetOption("Reincarnation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000转身系统已禁用|r");
            return true;
        }

        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 maxLevel = sReincarnationMgr->GetMaxReincarnationLevel();
        if (level > maxLevel)
        {
            handler->PSendSysMessage("|cffff0000设置失败: 转身等级不能超过当前上限 {}|r", maxLevel);
            return true;
        }

        if (sReincarnationMgr->SetReincarnationLevel(player, level))
        {
            uint32 playerGuid = player->GetGUID().GetCounter();
            float bonusStats = sReincarnationMgr->GetPlayerBonusStats(playerGuid);
            uint32 bonusTalent = sReincarnationMgr->GetPlayerBonusTalentPoints(playerGuid);

            handler->PSendSysMessage("|cff00ff00转身等级已设置为 {}|r", level);
            handler->PSendSysMessage("|cff00ffff累计属性加成: {:.1f}%|r", bonusStats);
            handler->PSendSysMessage("|cffff00ff累计天赋点: {}|r", bonusTalent);
        }
        else
        {
            handler->PSendSysMessage("|cffff0000设置失败|r");
        }

        return true;
    }

    // .转身 重载
    static bool HandleReincarnationReloadCommand(ChatHandler* handler)
    {
        sReincarnationMgr->LoadReincarnationConfig();

        handler->PSendSysMessage("|cff00ff00转身系统配置已重载，共 {} 条配置，最高转身等级 {}|r",
            sReincarnationMgr->GetConfigCount(), sReincarnationMgr->GetMaxReincarnationLevel());

        return true;
    }

    // .转身 信息
    static bool HandleReincarnationInfoCommand(ChatHandler* handler)
    {
        if (!sConfigMgr->GetOption("Reincarnation.Enable", true))
        {
            handler->PSendSysMessage("|cffff0000转身系统已禁用|r");
            return true;
        }

        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 reincarnationLevel = sReincarnationMgr->GetPlayerReincarnationLevel(playerGuid);
        float bonusStats = sReincarnationMgr->GetPlayerBonusStats(playerGuid);
        uint32 bonusTalent = sReincarnationMgr->GetPlayerBonusTalentPoints(playerGuid);

        handler->PSendSysMessage("|cff00ff00========== 转身信息 ==========|r");
        handler->PSendSysMessage("|cffffd700当前转身等级:|r {} 转", reincarnationLevel);
        handler->PSendSysMessage("|cffffd700全属性加成:|r +{:.1f}%", bonusStats);
        handler->PSendSysMessage("|cffffd700额外天赋点:|r +{}", bonusTalent);
        handler->PSendSysMessage("|cffffd700当前角色等级:|r {}级", player->GetLevel());
        handler->PSendSysMessage("|cffffd700转身等级上限:|r {} 转", sReincarnationMgr->GetMaxReincarnationLevel());

        // 显示下一转信息
        uint32 nextLevel = reincarnationLevel + 1;
        uint32 maxLevel = sReincarnationMgr->GetMaxReincarnationLevel();
        if (reincarnationLevel >= maxLevel)
        {
            handler->PSendSysMessage("|cff00ffff已达到转身等级上限|r");
        }
        else
        {
            ReincarnationConfig const* nextConfig = sReincarnationMgr->GetConfigForLevel(nextLevel);
            if (nextConfig)
            {
                handler->PSendSysMessage("|cff00ffff--- 下一转({}转)奖励 ---|r", nextLevel);
                handler->PSendSysMessage("|cff00ffff全属性加成:|r +{:.1f}%", nextConfig->bonusStats);
                handler->PSendSysMessage("|cff00ffff天赋点奖励:|r +{}", nextConfig->bonusTalentPoints);
                if (nextConfig->requirementTemplateId > 0)
                {
                    handler->PSendSysMessage("|cff00ffff需求模板ID:|r {}", nextConfig->requirementTemplateId);
                }
            }
        }

        handler->PSendSysMessage("|cff00ff00================================|r");

        return true;
    }

    // .转身 界面 / .转身 ui - 打开UI界面
    static bool HandleReincarnationOpenUICommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        // 发送打开UI界面的Addon消息
        std::string fullMessage = std::string(REINCARNATION_ADDON_PREFIX) + "\tOPEN_UI";
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);

        return true;
    }
};

// 添加脚本
void AddSC_ReincarnationCommands()
{
    new ReincarnationCommandScript();
}

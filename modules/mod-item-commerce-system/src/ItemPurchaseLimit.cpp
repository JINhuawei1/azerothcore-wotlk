#include "ScriptMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "ItemPurchaseLimit.h"
#include "Chat.h"
#include "Config.h"
#include "WorldScript.h"
#include "World.h"
#include "Log.h"
#include "GameTime.h"

// 用于在服务器启动后显示 Banner 的 WorldScript
class StartupBannerScript : public WorldScript
{
private:
    bool _bannerPrinted; // 标志位，确保 Banner 只打印一次

public:
    StartupBannerScript() : WorldScript("StartupBannerScript"), _bannerPrinted(false) { }

    // OnUpdate 会被服务器主循环频繁调用
    void OnUpdate(uint32 /* diff */) override
    {
        // 检查运行时间是否超过1秒且 Banner 尚未打印
        if (!_bannerPrinted && GameTime::GetUptime().count() >= 1)
        {
            LOG_INFO("server.loading", "→物品购买限制系统√");
            // 设置标志位，防止重复打印
            _bannerPrinted = true;
        }
    }
};

class ItemPurchaseLimit : public PlayerScript
{
public:
    ItemPurchaseLimit() : PlayerScript("ItemPurchaseLimit") {}

    void OnBeforeBuyItem(Player* player, uint32 itemId, uint32 count)
    {
        if (!player || !itemId)
            return;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 characterId = player->GetGUID().GetCounter();
        std::string itemName = itemTemplate->Name1;

        // 查询物品购买限制配置
        std::string query = "SELECT `每日限制账号次数`, `每日限制角色次数`, `每日限制全服次数`, `永久限制账号次数`, `永久限制角色次数`, `永久限制全服次数` FROM `_物品_每日购买次数` WHERE `物品id` = " + std::to_string(itemId);
        QueryResult configResult = WorldDatabase.Query(query.c_str());

        if (!configResult)
            return;

        Field* configFields = configResult->Fetch();
        int32 dailyAccountLimit = static_cast<int32>(configFields[0].Get<uint32>());
        int32 dailyCharacterLimit = static_cast<int32>(configFields[1].Get<uint32>());
        int32 dailyServerLimit = static_cast<int32>(configFields[2].Get<uint32>());
        int32 permanentAccountLimit = static_cast<int32>(configFields[3].Get<uint32>());
        int32 permanentCharacterLimit = static_cast<int32>(configFields[4].Get<uint32>());
        int32 permanentServerLimit = static_cast<int32>(configFields[5].Get<uint32>());

        // 检查每日账号限制
        if (dailyAccountLimit > 0)
        {
            std::string dailyAccountQuery = "SELECT SUM(`购买数量`) FROM `_物品_购买记录` WHERE `物品id` = " + std::to_string(itemId) + " AND `账号id` = " + std::to_string(accountId) + " AND `日期` = CURDATE()";
            QueryResult result = CharacterDatabase.Query(dailyAccountQuery.c_str());
            if (result)
            {
                Field* fields = result->Fetch();
                int32 dailyAccountCount = static_cast<int32>(fields[0].Get<uint32>());
                if (dailyAccountCount + count > dailyAccountLimit)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品每日账号限制为{}个，您今日已购买{}个。", dailyAccountLimit, dailyAccountCount);
                    player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
                    return;
                }
            }
        }

        // 检查每日角色限制
        if (dailyCharacterLimit > 0)
        {
            std::string dailyCharacterQuery = "SELECT SUM(`购买数量`) FROM `_物品_购买记录` WHERE `物品id` = " + std::to_string(itemId) + " AND `角色id` = " + std::to_string(characterId) + " AND `日期` = CURDATE()";
            QueryResult result = CharacterDatabase.Query(dailyCharacterQuery.c_str());
            if (result)
            {
                Field* fields = result->Fetch();
                int32 dailyCharacterCount = static_cast<int32>(fields[0].Get<uint32>());
                if (dailyCharacterCount + count > dailyCharacterLimit)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品每日角色限制为{}个，您今日已购买{}个。", dailyCharacterLimit, dailyCharacterCount);
                    player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
                    return;
                }
            }
        }

        // 检查每日全服限制
        if (dailyServerLimit > 0)
        {
            std::string dailyServerQuery = "SELECT SUM(`购买数量`) FROM `_物品_购买记录` WHERE `物品id` = " + std::to_string(itemId) + " AND `日期` = CURDATE()";
            QueryResult result = CharacterDatabase.Query(dailyServerQuery.c_str());
            if (result)
            {
                Field* fields = result->Fetch();
                int32 dailyServerCount = static_cast<int32>(fields[0].Get<uint32>());
                if (dailyServerCount + count > dailyServerLimit)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品每日全服限制为{}个，今日已售出{}个。", dailyServerLimit, dailyServerCount);
                    player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
                    return;
                }
            }
        }

        // 检查永久账号限制
        if (permanentAccountLimit > 0)
        {
            std::string permanentAccountQuery = "SELECT SUM(`购买数量`) FROM `_物品_购买记录` WHERE `物品id` = " + std::to_string(itemId) + " AND `账号id` = " + std::to_string(accountId);
            QueryResult result = CharacterDatabase.Query(permanentAccountQuery.c_str());
            if (result)
            {
                Field* fields = result->Fetch();
                int32 permanentAccountCount = static_cast<int32>(fields[0].Get<uint32>());
                if (permanentAccountCount + count > permanentAccountLimit)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品永久账号限制为{}个，您已购买{}个。", permanentAccountLimit, permanentAccountCount);
                    player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
                    return;
                }
            }
        }

        // 检查永久角色限制
        if (permanentCharacterLimit > 0)
        {
            std::string permanentCharacterQuery = "SELECT SUM(`购买数量`) FROM `_物品_购买记录` WHERE `物品id` = " + std::to_string(itemId) + " AND `角色id` = " + std::to_string(characterId);
            QueryResult result = CharacterDatabase.Query(permanentCharacterQuery.c_str());
            if (result)
            {
                Field* fields = result->Fetch();
                int32 permanentCharacterCount = static_cast<int32>(fields[0].Get<uint32>());
                if (permanentCharacterCount + count > permanentCharacterLimit)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品永久角色限制为{}个，您已购买{}个。", permanentCharacterLimit, permanentCharacterCount);
                    player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
                    return;
                }
            }
        }

        // 检查永久全服限制
        if (permanentServerLimit > 0)
        {
            std::string permanentServerQuery = "SELECT SUM(`购买数量`) FROM `_物品_购买记录` WHERE `物品id` = " + std::to_string(itemId);
            QueryResult result = CharacterDatabase.Query(permanentServerQuery.c_str());
            if (result)
            {
                Field* fields = result->Fetch();
                int32 permanentServerCount = static_cast<int32>(fields[0].Get<uint32>());
                if (permanentServerCount + count > permanentServerLimit)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品永久全服限制为{}个，已售出{}个。", permanentServerLimit, permanentServerCount);
                    player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
                    return;
                }
            }
        }
    }

    void OnAfterBuyItem(Player* player, uint32 itemId, uint32 count)
    {
        if (!player || !itemId)
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 characterId = player->GetGUID().GetCounter();

        // 记录购买信息
        std::string insertQuery = "INSERT INTO `_物品_购买记录` (`物品id`, `账号id`, `角色id`, `购买数量`) VALUES (" + std::to_string(itemId) + ", " + std::to_string(accountId) + ", " + std::to_string(characterId) + ", " + std::to_string(count) + ")";
        CharacterDatabase.Execute(insertQuery.c_str());
    }
};

void AddSC_ItemPurchaseLimitScripts()
{
    new ItemPurchaseLimit();
    new StartupBannerScript();
}
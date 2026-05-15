#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "World.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "ScriptedGossip.h"
#include "CommandScript.h"
#include "Logging/Log.h"
#include <unordered_map>

using namespace Acore::ChatCommands;

struct LeaseItemInfo
{
    uint32 itemId;
    uint32 leaseDuration;
    std::string comment;
};

struct CharacterLeaseItemInfo
{
    uint32 charId;
    uint32 itemGuid;
    uint32 expireTime;
};

class ItemLeaseSystem : public PlayerScript
{
public:
    ItemLeaseSystem() : PlayerScript("ItemLeaseSystemScript")
    {
        _checkTimer = 0;
        _enabled = false;
        _checkInterval = 60;
        _notifyBeforeExpire = 300;
        _isLoaded = false;
    }

    // 加载模块数据
    uint32 LoadModule()
    {
        // 配置加载
        _enabled = sConfigMgr->GetOption<bool>("ItemLease.Enable", true);

        if (!_enabled)
            return 0;

        _checkInterval = sConfigMgr->GetOption<uint32>("ItemLease.CheckInterval", 60);
        _notifyBeforeExpire = sConfigMgr->GetOption<uint32>("ItemLease.NotifyBeforeExpire", 300);

        // 加载物品租赁和玩家租赁数据
        LoadLeaseItems();
        LoadCharLeaseItems();

        _isLoaded = true;
        return _leaseItems.size();
    }

    // 加载物品租赁数据
    void LoadLeaseItems()
    {
        // 加载物品租赁数据
        _leaseItems.clear();

        QueryResult result = WorldDatabase.Query("SELECT `物品id`, `租赁时间_单位秒`, `注释` FROM `_物品_租赁`");
        if (!result)
            return;

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            LeaseItemInfo info;
            info.itemId = fields[0].Get<uint32>();
            info.leaseDuration = fields[1].Get<uint32>();
            if (!fields[2].IsNull())
                info.comment = fields[2].Get<std::string>();

            _leaseItems[info.itemId] = info;
            ++count;
        }
        while (result->NextRow());
    }

    // 加载玩家物品租赁记录
    void LoadCharLeaseItems()
    {
        // 加载玩家物品租赁记录
        _charLeaseItems.clear();

        QueryResult result = CharacterDatabase.Query("SELECT `角色ID`, `物品GUID`, `过期时间` FROM `_物品_物品租赁`");
        if (!result)
            return;

        uint32 count = 0;
        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        std::vector<std::pair<uint32, uint32>> expiredItems;

        do
        {
            Field* fields = result->Fetch();

            CharacterLeaseItemInfo info;
            info.charId = fields[0].Get<uint32>();
            info.itemGuid = fields[1].Get<uint32>();
            info.expireTime = fields[2].Get<uint32>();

            // 检查是否已过期
            if (info.expireTime <= now)
            {
                expiredItems.push_back(std::make_pair(info.charId, info.itemGuid));
                continue;
            }

            _charLeaseItems.push_back(info);
            ++count;
        }
        while (result->NextRow());

        // 处理已过期物品
        if (!expiredItems.empty())
        {
            for (auto& item : expiredItems)
            {
                CharacterDatabase.Execute("DELETE FROM `_物品_物品租赁` WHERE `角色ID` = {} AND `物品GUID` = {}",
                    item.first, item.second);
            }
        }
    }

    // 玩家获得物品
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override
    {
        if (!_enabled || !player || !item)
            return;

        uint32 itemId = item->GetEntry();
        auto it = _leaseItems.find(itemId);
        if (it == _leaseItems.end())
            return;

        uint32 leaseDuration = it->second.leaseDuration;
        uint32 expireTime = static_cast<uint32>(GameTime::GetGameTime().count()) + leaseDuration;
        uint32 itemGuid = item->GetGUID().GetCounter();

        // 记录租赁信息
        CharacterDatabase.Execute("REPLACE INTO `_物品_物品租赁` (`角色ID`, `物品GUID`, `过期时间`) VALUES ({}, {}, {})",
            player->GetGUID().GetCounter(), itemGuid, expireTime);

        // 添加到内存记录
        CharacterLeaseItemInfo info;
        info.charId = player->GetGUID().GetCounter();
        info.itemGuid = itemGuid;
        info.expireTime = expireTime;
        _charLeaseItems.push_back(info);

        // 通知玩家
        ChatHandler(player->GetSession()).PSendSysMessage("物品 [{}] 将在 {} 秒后过期",
            item->GetTemplate()->Name1, leaseDuration);
    }

    // 玩家失去物品
    void OnItemRemove(Player* player, Item* item)
    {
        if (!_enabled || !player || !item)
            return;

        uint32 itemId = item->GetEntry();
        auto it = _leaseItems.find(itemId);
        if (it == _leaseItems.end())
            return;

        uint32 charId = player->GetGUID().GetCounter();
        uint32 itemGuid = item->GetGUID().GetCounter();

        // 从数据库删除记录
        CharacterDatabase.Execute("DELETE FROM `_物品_物品租赁` WHERE `角色ID` = {} AND `物品GUID` = {}",
            charId, itemGuid);

        // 从内存中移除
        _charLeaseItems.erase(
            std::remove_if(_charLeaseItems.begin(), _charLeaseItems.end(),
                [charId, itemGuid](const CharacterLeaseItemInfo& info) {
                    return info.charId == charId && info.itemGuid == itemGuid;
                }
            ),
            _charLeaseItems.end()
        );
    }

    // 检查过期物品的计时器更新
    void UpdateExpireCheck(uint32 diff)
    {
        if (!_enabled)
            return;

        _checkTimer += diff;
        if (_checkTimer < _checkInterval * IN_MILLISECONDS)
            return;

        _checkTimer = 0;
        CheckExpiredItems();
    }

    // 检查物品过期
    void CheckExpiredItems()
    {
        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        std::vector<CharacterLeaseItemInfo> expiredItems;
        std::vector<CharacterLeaseItemInfo> notifyItems;

        for (auto it = _charLeaseItems.begin(); it != _charLeaseItems.end();)
        {
            if (it->expireTime <= now)
            {
                expiredItems.push_back(*it);
                it = _charLeaseItems.erase(it);
            }
            else
            {
                // 检查是否需要通知
                if (_notifyBeforeExpire > 0 && it->expireTime <= now + _notifyBeforeExpire &&
                    it->expireTime > now + _notifyBeforeExpire - _checkInterval)
                {
                    notifyItems.push_back(*it);
                }
                ++it;
            }
        }

        // 处理过期物品
        for (auto& info : expiredItems)
        {
            // 从数据库删除记录
            CharacterDatabase.Execute("DELETE FROM `_物品_物品租赁` WHERE `角色ID` = {} AND `物品GUID` = {}",
                info.charId, info.itemGuid);

            // 查找玩家
            Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, info.charId));
            if (!player)
                continue;

            // 查找物品
            Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, info.itemGuid));
            if (!item)
                continue;

            // 通知玩家
            ChatHandler(player->GetSession()).PSendSysMessage("物品 [{}] 租赁时间已过期，物品将被移除",
                item->GetTemplate()->Name1);

            // 删除物品
            player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        }

        // 处理需要通知的物品
        for (auto& info : notifyItems)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, info.charId));
            if (!player)
                continue;

            Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, info.itemGuid));
            if (!item)
                continue;

            uint32 remainingTime = info.expireTime - now;
            ChatHandler(player->GetSession()).PSendSysMessage("警告：物品 [{}] 将在 {} 秒后过期",
                item->GetTemplate()->Name1, remainingTime);
        }
    }

    // 获取单例实例
    static ItemLeaseSystem* instance()
    {
        static ItemLeaseSystem instance;
        return &instance;
    }

    // 获取租赁物品信息
    const std::unordered_map<uint32, LeaseItemInfo>& GetLeaseItems() const
    {
        return _leaseItems;
    }

    // 获取玩家租赁物品信息
    const std::vector<CharacterLeaseItemInfo>& GetCharLeaseItems() const
    {
        return _charLeaseItems;
    }

    // 添加租赁物品
    bool AddLeaseItem(uint32 itemId, uint32 duration, const std::string& comment)
    {
        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
            return false;

        // 添加到数据库
        WorldDatabase.Execute("REPLACE INTO `_物品_租赁` (`物品id`, `租赁时间_单位秒`, `注释`) VALUES ({}, {}, {})",
            itemId, duration, comment);

        // 添加到内存
        LeaseItemInfo info;
        info.itemId = itemId;
        info.leaseDuration = duration;
        info.comment = comment;
        _leaseItems[itemId] = info;

        return true;
    }

    // 删除租赁物品
    bool RemoveLeaseItem(uint32 itemId)
    {
        auto it = _leaseItems.find(itemId);
        if (it == _leaseItems.end())
            return false;

        // 从数据库中删除
        WorldDatabase.Execute("DELETE FROM `_物品_租赁` WHERE `物品id` = {}", itemId);

        // 从内存中删除
        _leaseItems.erase(it);

        return true;
    }

private:
    bool _enabled;
    bool _isLoaded;
    uint32 _checkTimer;
    uint32 _checkInterval;
    uint32 _notifyBeforeExpire;
    std::unordered_map<uint32, LeaseItemInfo> _leaseItems;
    std::vector<CharacterLeaseItemInfo> _charLeaseItems;
};

#define sItemLeaseSystem ItemLeaseSystem::instance()

// 中文命令处理类
class ItemLeaseCommandScript : public CommandScript
{
public:
    ItemLeaseCommandScript() : CommandScript("ItemLeaseCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable leaseCommandTable =
        {
            { "查看",   HandleLeaseListCommand,    SEC_GAMEMASTER,     Console::Yes },
            { "添加",   HandleLeaseAddCommand,     SEC_ADMINISTRATOR,  Console::Yes },
            { "删除",   HandleLeaseRemoveCommand,  SEC_ADMINISTRATOR,  Console::Yes },
            { "玩家",   HandleLeasePlayerCommand,  SEC_GAMEMASTER,     Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "租赁",   leaseCommandTable },
        };

        return commandTable;
    }

    // .租赁查看 - 列出所有可租赁物品
    static bool HandleLeaseListCommand(ChatHandler* handler, std::string_view /*args*/)
    {
        const auto& leaseItems = sItemLeaseSystem->GetLeaseItems();

        if (leaseItems.empty())
        {
            handler->SendSysMessage("当前没有可租赁物品。");
            return true;
        }

        handler->SendSysMessage("可租赁物品列表:");
        handler->SendSysMessage("物品ID | 物品名称 | 租赁时间(秒) | 备注");
        handler->SendSysMessage("----------------------------------------");

        for (const auto& pair : leaseItems)
        {
            const LeaseItemInfo& info = pair.second;
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(info.itemId);
            if (!itemTemplate)
                continue;

            handler->PSendSysMessage("{} | {} | {} | {}",
                info.itemId,
                itemTemplate->Name1,
                info.leaseDuration,
                info.comment);
        }

        return true;
    }

    // .租赁添加 <物品ID> <租赁时间(秒)> [备注]
    static bool HandleLeaseAddCommand(ChatHandler* handler, std::string_view args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("命令格式：.租赁添加 <物品ID> <租赁时间(秒)> [备注]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* itemIdStr = strtok((char*)args.data(), " ");
        char* durationStr = strtok(nullptr, " ");
        char* comment = strtok(nullptr, "\0");

        if (!itemIdStr || !durationStr)
        {
            handler->SendSysMessage("命令格式：.租赁添加 <物品ID> <租赁时间(秒)> [备注]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 duration = atoi(durationStr);
        std::string commentStr = comment ? comment : "";

        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("物品ID {} 不存在。", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (sItemLeaseSystem->AddLeaseItem(itemId, duration, commentStr))
        {
            handler->PSendSysMessage("已添加物品 [{}](ID: {}) 到租赁系统，租赁时间：{} 秒。",
                itemTemplate->Name1, itemId, duration);
            return true;
        }
        else
        {
            handler->SendSysMessage("添加租赁物品失败。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // .租赁删除 <物品ID>
    static bool HandleLeaseRemoveCommand(ChatHandler* handler, std::string_view args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("命令格式：.租赁删除 <物品ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(args.data());

        // 检查物品是否存在于租赁系统中
        const auto& leaseItems = sItemLeaseSystem->GetLeaseItems();
        auto it = leaseItems.find(itemId);
        if (it == leaseItems.end())
        {
            handler->PSendSysMessage("物品ID {} 不在租赁系统中。", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("物品ID {} 不存在。", itemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (sItemLeaseSystem->RemoveLeaseItem(itemId))
        {
            handler->PSendSysMessage("已从租赁系统中移除物品 [{}](ID: {})。",
                itemTemplate->Name1, itemId);
            return true;
        }
        else
        {
            handler->SendSysMessage("移除租赁物品失败。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // .租赁玩家 [玩家名称]
    static bool HandleLeasePlayerCommand(ChatHandler* handler, std::string_view args)
    {
        Player* target = nullptr;

        if (args.empty())
        {
            target = handler->getSelectedPlayer();
            if (!target)
            {
                handler->SendSysMessage("请选择一个玩家或提供玩家名称。");
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            std::string playerName = std::string(args);
            target = ObjectAccessor::FindPlayerByName(playerName);
            if (!target)
            {
                handler->PSendSysMessage("找不到玩家：{}。", playerName);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        const auto& charLeaseItems = sItemLeaseSystem->GetCharLeaseItems();
        bool found = false;

        uint32 charId = target->GetGUID().GetCounter();
        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());

        handler->PSendSysMessage("玩家 {} 的租赁物品:", target->GetName());
        handler->SendSysMessage("物品名称 | 剩余时间(秒)");
        handler->SendSysMessage("----------------------------");

        for (const auto& info : charLeaseItems)
        {
            if (info.charId != charId)
                continue;

            Item* item = target->GetItemByGuid(ObjectGuid(HighGuid::Item, info.itemGuid));
            if (!item)
                continue;

            uint32 remainingTime = 0;
            if (info.expireTime > now)
                remainingTime = info.expireTime - now;

            handler->PSendSysMessage("{} | {}",
                item->GetTemplate()->Name1, remainingTime);

            found = true;
        }

        if (!found)
        {
            handler->PSendSysMessage("玩家 {} 没有租赁物品。", target->GetName());
        }

        return true;
    }
};

// 延迟加载器类
class ItemLeaseDelayedLoader : public WorldScript
{
public:
    ItemLeaseDelayedLoader() : WorldScript("ItemLeaseDelayedLoader")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟

        // 不显示准备中的日志
    }

    void OnUpdate(uint32 diff) override
    {
        if (!_isLoaded)
        {
            // 未加载模块，检查是否到达加载时间
            _loadTimer += diff;
            if (_loadTimer >= _loadDelay)
            {
                // 加载模块
                uint32 count = sItemLeaseSystem->LoadModule();

                // 加载完成后显示结果
                LOG_INFO("server.loading", "→物品租赁系统√");

                _isLoaded = true;
            }
        }
        else
        {
            // 模块已加载，定期更新检查
            sItemLeaseSystem->UpdateExpireCheck(diff);
        }
    }

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;
};

// 添加脚本
void AddItemLeaseSystemScripts()
{
    new ItemLeaseSystem();
    new ItemLeaseCommandScript();
    new ItemLeaseDelayedLoader();
}
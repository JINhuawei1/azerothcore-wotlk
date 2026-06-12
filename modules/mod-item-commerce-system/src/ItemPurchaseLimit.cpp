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
#include <ctime>
#include <unordered_map>

// 限购实现说明（2026-06-12 死钩子修复 + 缓存化重写）：
// - 原 OnBeforeBuyItem/OnAfterBuyItem 并非核心钩子名，整个限购系统从未生效；
//   且原 OnBeforeBuyItem 返回 void 即使生效也无法真正拦截购买。
// - 现改挂真实钩子 OnPlayerBeforeBuyItemFromVendor（item 置 0 即取消购买，核心明确支持）
//   与 OnPlayerAfterStoreOrEquipNewItem（仅商人购买路径触发）。
// - 配置整表启动缓存；购买计数按物品惰性聚合加载一次后纯内存（写库仅为审计持久化），
//   无限购配置的物品（绝大多数）在购买路径零成本早退——避免"修活死钩子即引爆查库"。
// - 线程模型：按 MapUpdate.Threads=1 部署红线编写，不加锁。
namespace
{
struct PurchaseLimitConfig
{
    int32 dailyAccount = 0;
    int32 dailyCharacter = 0;
    int32 dailyServer = 0;
    int32 permAccount = 0;
    int32 permCharacter = 0;
    int32 permServer = 0;
};

struct PurchaseCounters
{
    std::unordered_map<uint32, uint32> dailyAccount;    // accountId -> 今日已购
    std::unordered_map<uint32, uint32> dailyCharacter;  // charGuid  -> 今日已购
    uint32 dailyServer = 0;
    std::unordered_map<uint32, uint32> permAccount;
    std::unordered_map<uint32, uint32> permCharacter;
    uint32 permServer = 0;
    bool loaded = false;
};

std::unordered_map<uint32, PurchaseLimitConfig> s_limitConfigs;
std::unordered_map<uint32, PurchaseCounters> s_purchaseCounters;
uint32 s_dayStamp = 0;

uint32 LocalDayStamp()
{
    time_t now = time(nullptr);
    tm timeInfo{};
#ifdef _WIN32
    localtime_s(&timeInfo, &now);
#else
    localtime_r(&now, &timeInfo);
#endif
    return static_cast<uint32>(timeInfo.tm_year) * 1000 + static_cast<uint32>(timeInfo.tm_yday);
}

// 本地日期翻转时清空每日计数（永久计数不受影响），与 DB 侧 CURDATE() 语义对齐
void RolloverDailyCountersIfNeeded()
{
    uint32 today = LocalDayStamp();
    if (s_dayStamp == today)
        return;

    s_dayStamp = today;
    for (auto& pair : s_purchaseCounters)
    {
        pair.second.dailyAccount.clear();
        pair.second.dailyCharacter.clear();
        pair.second.dailyServer = 0;
    }
}

void LoadPurchaseLimitConfigs()
{
    s_limitConfigs.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `物品id`, `每日限制账号次数`, `每日限制角色次数`, `每日限制全服次数`, "
        "`永久限制账号次数`, `永久限制角色次数`, `永久限制全服次数` FROM `_物品_每日购买次数`");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].Get<uint32>();
        if (!itemId)
            continue;

        PurchaseLimitConfig cfg;
        cfg.dailyAccount = static_cast<int32>(fields[1].Get<uint32>());
        cfg.dailyCharacter = static_cast<int32>(fields[2].Get<uint32>());
        cfg.dailyServer = static_cast<int32>(fields[3].Get<uint32>());
        cfg.permAccount = static_cast<int32>(fields[4].Get<uint32>());
        cfg.permCharacter = static_cast<int32>(fields[5].Get<uint32>());
        cfg.permServer = static_cast<int32>(fields[6].Get<uint32>());

        s_limitConfigs[itemId] = cfg;
    } while (result->NextRow());
}

// 首次遇到该限购物品时做一次全维度聚合加载，之后纯内存
PurchaseCounters& EnsurePurchaseCounters(uint32 itemId)
{
    PurchaseCounters& counters = s_purchaseCounters[itemId];
    if (counters.loaded)
        return counters;

    counters.loaded = true;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `账号id`, `角色id`, SUM(`购买数量`), "
        "SUM(CASE WHEN `日期` = CURDATE() THEN `购买数量` ELSE 0 END) "
        "FROM `_物品_购买记录` WHERE `物品id` = {} GROUP BY `账号id`, `角色id`", itemId);
    if (!result)
        return counters;

    do
    {
        Field* fields = result->Fetch();
        uint32 accountId = fields[0].Get<uint32>();
        uint32 charGuid = fields[1].Get<uint32>();
        uint32 total = fields[2].Get<uint32>();
        uint32 today = fields[3].Get<uint32>();

        counters.permAccount[accountId] += total;
        counters.permCharacter[charGuid] += total;
        counters.permServer += total;

        if (today)
        {
            counters.dailyAccount[accountId] += today;
            counters.dailyCharacter[charGuid] += today;
            counters.dailyServer += today;
        }
    } while (result->NextRow());

    return counters;
}

uint32 GetCount(std::unordered_map<uint32, uint32> const& map, uint32 key)
{
    auto itr = map.find(key);
    return itr != map.end() ? itr->second : 0;
}
}

// 用于在服务器启动后显示 Banner 并加载限购配置的 WorldScript
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
            LoadPurchaseLimitConfigs();
            s_dayStamp = LocalDayStamp();
            LOG_INFO("server.loading", "→物品购买限制系统√ 限购配置={}", s_limitConfigs.size());
            // 设置标志位，防止重复打印
            _bannerPrinted = true;
        }
    }
};

class ItemPurchaseLimit : public PlayerScript
{
public:
    ItemPurchaseLimit() : PlayerScript("ItemPurchaseLimit") {}

    void OnPlayerBeforeBuyItemFromVendor(Player* player, ObjectGuid /*vendorguid*/, uint32 /*vendorslot*/, uint32& item, uint8 count, uint8 /*bag*/, uint8 /*slot*/) override
    {
        if (!player || !item)
            return;

        auto cfgItr = s_limitConfigs.find(item);
        if (cfgItr == s_limitConfigs.end())
            return; // 无限购配置（绝大多数物品），零成本放行

        RolloverDailyCountersIfNeeded();

        PurchaseLimitConfig const& cfg = cfgItr->second;
        PurchaseCounters& counters = EnsurePurchaseCounters(item);

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 charGuid = player->GetGUID().GetCounter();
        uint32 buyCount = count ? count : 1;
        uint32 itemId = item;

        // 置 0 取消本次购买（核心 BuyItemFromVendorSlot 明确支持）
        auto deny = [&]()
        {
            player->SendBuyError(BUY_ERR_CANT_CARRY_MORE, nullptr, itemId, 0);
            item = 0;
        };

        if (cfg.dailyAccount > 0)
        {
            uint32 current = GetCount(counters.dailyAccount, accountId);
            if (current + buyCount > static_cast<uint32>(cfg.dailyAccount))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品每日账号限制为{}个，您今日已购买{}个。", cfg.dailyAccount, current);
                deny();
                return;
            }
        }

        if (cfg.dailyCharacter > 0)
        {
            uint32 current = GetCount(counters.dailyCharacter, charGuid);
            if (current + buyCount > static_cast<uint32>(cfg.dailyCharacter))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品每日角色限制为{}个，您今日已购买{}个。", cfg.dailyCharacter, current);
                deny();
                return;
            }
        }

        if (cfg.dailyServer > 0 && counters.dailyServer + buyCount > static_cast<uint32>(cfg.dailyServer))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品每日全服限制为{}个，今日已售出{}个。", cfg.dailyServer, counters.dailyServer);
            deny();
            return;
        }

        if (cfg.permAccount > 0)
        {
            uint32 current = GetCount(counters.permAccount, accountId);
            if (current + buyCount > static_cast<uint32>(cfg.permAccount))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品永久账号限制为{}个，您已购买{}个。", cfg.permAccount, current);
                deny();
                return;
            }
        }

        if (cfg.permCharacter > 0)
        {
            uint32 current = GetCount(counters.permCharacter, charGuid);
            if (current + buyCount > static_cast<uint32>(cfg.permCharacter))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品永久角色限制为{}个，您已购买{}个。", cfg.permCharacter, current);
                deny();
                return;
            }
        }

        if (cfg.permServer > 0 && counters.permServer + buyCount > static_cast<uint32>(cfg.permServer))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("购买失败：该物品永久全服限制为{}个，已售出{}个。", cfg.permServer, counters.permServer);
            deny();
            return;
        }
    }

    void OnPlayerAfterStoreOrEquipNewItem(Player* player, uint32 /*vendorslot*/, Item* /*item*/, uint8 count, uint8 /*bag*/, uint8 /*slot*/, ItemTemplate const* pProto, Creature* pVendor, VendorItem const* /*crItem*/, bool /*bStore*/) override
    {
        // pVendor 非空保证是商人购买路径
        if (!player || !pProto || !pVendor)
            return;

        uint32 itemId = pProto->ItemId;

        // 只统计/记录配置了限购的物品（原实现对所有购买写库，记录表无意义膨胀）
        if (s_limitConfigs.find(itemId) == s_limitConfigs.end())
            return;

        RolloverDailyCountersIfNeeded();

        PurchaseCounters& counters = EnsurePurchaseCounters(itemId);
        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 charGuid = player->GetGUID().GetCounter();
        uint32 buyCount = count ? count : 1;

        counters.dailyAccount[accountId] += buyCount;
        counters.dailyCharacter[charGuid] += buyCount;
        counters.dailyServer += buyCount;
        counters.permAccount[accountId] += buyCount;
        counters.permCharacter[charGuid] += buyCount;
        counters.permServer += buyCount;

        // 内存即权威，DB 记录仅为审计与重启恢复，异步写
        CharacterDatabase.Execute(
            "INSERT INTO `_物品_购买记录` (`物品id`, `账号id`, `角色id`, `购买数量`) VALUES ({}, {}, {}, {})",
            itemId, accountId, charGuid, buyCount);
    }
};

void AddSC_ItemPurchaseLimitScripts()
{
    new ItemPurchaseLimit();
    new StartupBannerScript();
}

#include "ItemSellReward.h"
#include "ScriptMgr.h"
#include "World.h"
#include "Logging/Log.h"
#include "Config.h"

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemSellRewardLoader : public WorldScript
{
public:
    ItemSellRewardLoader() : WorldScript("ItemSellRewardLoader")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟
    }

    void OnUpdate(uint32 diff) override
    {
        if (_isLoaded)
            return;

        _loadTimer += diff;
        if (_loadTimer >= _loadDelay)
        {
            // 加载模块
            LoadModule();
            _isLoaded = true;
        }
    }

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;

    uint32 LoadModule()
    {
        uint32 count = 0;

        // 加载配置
        if (ItemSellReward::LoadConfig(false))
        {
            // 加载数据库数据
            ItemSellReward::LoadFromDB();

            // 获取加载的数据条数
            QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `_物品_售卖获得`");
            if (result)
            {
                Field* fields = result->Fetch();
                count = fields[0].Get<uint32>();
            }

            // 显示ASCII艺术框
            LOG_INFO("server.loading", "→物品售卖奖励系统√");
        }

        return count;
    }
};

// 添加脚本
void AddItemSellRewardLoaderScripts()
{
    new ItemSellRewardLoader();
}

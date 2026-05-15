#include "ItemUseReward.h"
#include "ScriptMgr.h"
#include "World.h"
#include "Logging/Log.h"

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemUseRewardLoader : public WorldScript
{
public:
    ItemUseRewardLoader() : WorldScript("ItemUseRewardLoader")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟

        // 不显示准备中的日志
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
        if (ItemUseReward::LoadConfig(false))
        {
            // 加载数据库数据
            ItemUseReward::LoadFromDB();

            // 获取加载的数据条数
            QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `_物品_使用获得`");
            if (result)
            {
                Field* fields = result->Fetch();
                count = fields[0].Get<uint32>();
            }

            // 加载完成后显示结果
            LOG_INFO("server.loading", "→物品使用获得系统√");
        }

        return count;
    }
};

// 添加脚本
void AddItemUseRewardLoaderScripts()
{
    new ItemUseRewardLoader();
}

/*
 * 模块通用：玩家 Addon/聊天命令节流（令牌桶）
 *
 * 背景（见 modules/全模块代码审查报告_第二轮_2026-06-11.md 三-4）：
 * 几乎所有带 UI 的模块的 OnPlayerChat/Addon 入口无频率限制，且多数每条消息触发
 * 同步查库或大量发包——客户端可按包速率任意触发 = 世界线程 DoS 面。
 *
 * 用法（在 Addon/命令入口最前面）：
 *   #include "AddonThrottle.h"
 *   if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "XIANMEN"))
 *       return; // 超频静默丢弃
 *
 * 参数建议：
 *   - 重型端点（REQ_ALL 全量推送、批量操作）：minIntervalMs=1000, burst=2
 *   - 普通查询端点：默认 minIntervalMs=500, burst=4（每秒稳态 2 条，允许短突发 4 条）
 *
 * 线程安全：全局互斥锁；仅在玩家消息入口调用，频率低，无热路径开销。
 * 内存有界：桶数量超限时惰性清扫 5 分钟未活动的条目。
 */

#ifndef MODULE_ADDON_THROTTLE_H
#define MODULE_ADDON_THROTTLE_H

#include "GameTime.h"
#include "ObjectGuid.h"
#include <mutex>
#include <unordered_map>

namespace ModuleAddon
{
class Throttle
{
public:
    // 返回 true = 放行；false = 超频，调用方应直接丢弃本条消息（不要回包，避免拒绝响应本身被刷）
    static bool Allow(ObjectGuid const& playerGuid, char const* channel, uint32 minIntervalMs = 500, uint32 burst = 4)
    {
        if (!minIntervalMs || !burst)
            return true;

        uint64 const key = (uint64(playerGuid.GetCounter()) << 32) | Fnv1a(channel);
        uint64 const nowMs = uint64(GameTime::GetGameTimeMS().count());

        std::lock_guard<std::mutex> lock(GetMutex());
        auto& buckets = GetBuckets();

        // 惰性清理：限制长期内存（5 分钟未活动的桶清除）
        if (buckets.size() > MAX_BUCKETS)
        {
            for (auto itr = buckets.begin(); itr != buckets.end();)
            {
                if (nowMs - itr->second.lastMs > 5 * 60 * 1000)
                    itr = buckets.erase(itr);
                else
                    ++itr;
            }
        }

        Bucket& b = buckets[key];
        if (b.lastMs == 0)
        {
            // 新桶：满额令牌
            b.tokens = burst;
            b.lastMs = nowMs;
        }
        else if (nowMs > b.lastMs)
        {
            // 按流逝时间补充令牌
            uint64 refill = (nowMs - b.lastMs) / minIntervalMs;
            if (refill)
            {
                b.tokens = uint32(std::min<uint64>(burst, b.tokens + refill));
                b.lastMs += refill * minIntervalMs;
            }
        }

        if (!b.tokens)
            return false;

        --b.tokens;
        return true;
    }

private:
    struct Bucket
    {
        uint64 lastMs = 0;
        uint32 tokens = 0;
    };

    static constexpr size_t MAX_BUCKETS = 50000;

    static constexpr uint32 Fnv1a(char const* s)
    {
        uint32 h = 2166136261u;
        while (*s)
        {
            h ^= uint8(*s++);
            h *= 16777619u;
        }
        return h;
    }

    // 函数内静态：inline 成员函数中的局部静态在全部 TU 间共享（C++ 标准保证单实例）
    static std::mutex& GetMutex()
    {
        static std::mutex mtx;
        return mtx;
    }

    static std::unordered_map<uint64, Bucket>& GetBuckets()
    {
        static std::unordered_map<uint64, Bucket> buckets;
        return buckets;
    }
};
} // namespace ModuleAddon

#endif // MODULE_ADDON_THROTTLE_H

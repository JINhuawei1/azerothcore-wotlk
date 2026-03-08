#ifndef MODULE_ITEM_IDENTIFICATION_SYSTEM_H
#define MODULE_ITEM_IDENTIFICATION_SYSTEM_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "TradeData.h"
#include "ModuleManager.h"
#include "AnnouncementInterface.h"
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <string>
#include <random>
#include <ctime>
#include <mutex>

// 物品鉴定记录结构
struct ItemIdentificationRecord
{
    uint32 id;
    uint32 playerGuid;
    uint32 itemGuid;
    uint32 itemEntry;
    uint32 templateId;
    
    // 鉴定获得的效果
    bool hasGrowth;
    uint32 growthGroup;
    bool hasEnhancement;
    uint32 enhancementGroup;
    bool hasBaseAttributes;
    uint32 baseAttrCount;
    uint32 baseAttrGroup;
    std::string baseAttrDetails; // 格式：属性ID,值 属性ID,值
    bool hasAdditionalAttributes;
    uint32 additionalAttrCount;
    std::string additionalAttrGroups;
    bool hasRuneSlots;
    uint32 runeSlotCount;
    bool hasSkills;
    std::string skillGroups;
    bool hasMagicHits;
    uint32 magicHitCount;
    std::string magicHitGroups;
    bool hasSet;
    uint32 setGroup;
    uint32 setId;
    uint32 costGold;
    uint32 successRate;
};

// 物品鉴定系统类
class ItemIdentificationSystem
{
public:
    static ItemIdentificationSystem* instance();

    // 初始化系统
    void Initialize();

    // 加载配置
    void LoadConfig(bool reload);

    // 鉴定物品（需要指定组ID）
    bool IdentifyItem(Player* player, Item* item, uint32 groupId);

    // 检查物品是否可以鉴定
    bool CanIdentify(Player* player, Item* item, bool sendError = true);

    // 获取鉴定成功率
    uint32 GetSuccessRate(Player* player, Item* item);

    // 应用鉴定结果到物品（需要指定组ID）
    bool ApplyIdentification(Player* player, Item* item, uint32 groupId);

    // 处理鉴定失败
    void HandleFailure(Player* player, Item* item);

    // 发送鉴定公告
    void SendAnnouncement(Player* player, Item* item, uint32 identificationId);

    // 鉴定记录管理（公开给命令和脚本使用）
    bool IsItemIdentified(uint32 itemGuid);

    // 将鉴定属性数据发送到客户端（调用各模块自己的查询函数）
    void SendAllModuleData(Player* player, Item* item);

    // 发送鉴定系统自己的数据（基础属性和追加属性）
    void SendIdentificationDataToClient(Player* player, Item* item);

    // 全局随机数生成器（单例模式）
    std::mt19937& GetRandomGenerator()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return gen;
    }

    // 性能计时辅助类
    class PerformanceTimer
    {
    public:
        PerformanceTimer(const std::string& name) : _name(name), _start(std::chrono::high_resolution_clock::now()) {}

        ~PerformanceTimer()
        {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - _start).count();
        }

    private:
        std::string _name;
        std::chrono::high_resolution_clock::time_point _start;
    };

    // 清除已鉴定物品缓存（在物品被销毁或交易时调用）
    void ClearIdentifiedCache(uint32 itemGuid);

    // 添加物品到已鉴定缓存（鉴定成功后调用，确保缓存同步）
    // 【线程安全修复】添加锁保护
    void AddToIdentifiedCache(uint32 itemGuid)
    {
        std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
        _identifiedItemsCache.insert(itemGuid);
    }

    // 批量检查物品是否已鉴定（优化版，减少数据库查询）
    std::set<uint32> BatchCheckIdentified(const std::vector<uint32>& itemGuids);

    // 新增：批量查询所有模块数据（一次性返回所有系统的数据）
    struct AllModuleData {
        // 鉴定系统数据
        std::string baseAttributes;      // 格式：attrType value,attrType value
        std::string additionalAttributes; // 格式：attrType value,attrType value

        // 成长系统数据
        std::string growthData;          // 格式：level:exp:maxExp:attrs

        // 强化系统数据
        std::string enhancementData;     // 格式：level:attrs

        // 技能系统数据
        std::string skillsData;          // 格式：skillId:skillName:level,skillId:skillName:level

        // 魔次系统数据
        std::string magicHitData;        // 格式：configId:count:desc,configId:count:desc

        // 符文系统数据
        std::string runeData;            // 格式：totalSlots:filledSlots:slotData

        // 套装系统数据
        std::string setData;             // 格式：setId:setName:attrs:effects

        // 幻境系统数据（新增）
        std::string huanjingData;        // 格式：multiplier|enhancedAttrs  例如：3|3 10 30,4 20 60

        bool hasData;                    // 是否有任何数据
    };

    // 批量查询所有模块数据
    AllModuleData QueryAllModuleData(uint32 itemID, uint32 guid);

    // 批量查询命令处理器（一次性返回所有数据）
    void HandleBatchQueryCommand(Player* player, uint32 itemID, uint32 guid);

    // 通过 Addon 消息发送批量数据（ALL_MODULE_DATA:itemID:guid:...）
    void SendAllModuleDataAddon(Player* player, uint32 itemID, uint32 guid);

public:
    // 配置变量
    bool _enabled;
    uint32 _baseSuccessRate;
    bool _destroyOnFail;
    uint32 _cost;
    bool _enableAnnounce;
    bool _debugMode;

    // 线程安全保护（用于异步预加载，需要public访问）
    mutable std::mutex _batchCacheMutex;

    // 属性数据缓存结构（用于查询命令优化）
    struct ItemAttrCache {
        uint32 itemID;
        uint32 guid;
        std::string baseAttributes;
        std::string additionalAttributes;
        time_t cacheTime;
    };
    std::unordered_map<uint64, ItemAttrCache> _attrCache;  // key = (itemID << 32) | guid
    const uint32 ATTR_CACHE_EXPIRE_TIME = 300;  // 5分钟过期

    // 新增：批量查询数据缓存（避免重复查询数据库）
    struct BatchQueryCache {
        AllModuleData data;
        time_t cacheTime;
    };
    std::unordered_map<uint64, BatchQueryCache> _batchQueryCache;  // key = (itemID << 32) | guid
    const uint32 BATCH_CACHE_EXPIRE_TIME = 86400;  // 【性能优化】24小时过期（物品数据通常不频繁变化）

    // 清理过期缓存（定期调用）
    void CleanExpiredCache();

    // 【性能优化】主动清除特定物品的缓存（当物品被修改时调用）
    // 【线程安全修复】添加锁保护，防止与异步线程竞争
    void ClearItemCache(uint32 itemID, uint32 guid)
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);
        uint64 key = (static_cast<uint64>(itemID) << 32) | guid;
        _batchQueryCache.erase(key);
    }

    // 新增：预加载玩家装备数据（登录时调用）
    void PreloadPlayerEquipment(Player* player);

    // 新增：性能统计
    struct PerformanceStats {
        uint32 totalQueries;           // 总查询次数
        uint32 cacheHits;              // 缓存命中次数
        uint32 cacheMisses;            // 缓存未命中次数
        uint32 dbQueries;              // 数据库查询次数
        uint64 totalQueryTime;         // 总查询时间（微秒）
        uint64 avgQueryTime;           // 平均查询时间（微秒）
        uint32 preloadCount;           // 预加载次数
        time_t startTime;              // 统计开始时间

        PerformanceStats() : totalQueries(0), cacheHits(0), cacheMisses(0),
                           dbQueries(0), totalQueryTime(0), avgQueryTime(0),
                           preloadCount(0), startTime(time(nullptr)) {}

        float GetCacheHitRate() const {
            return totalQueries > 0 ? (float)cacheHits / totalQueries * 100.0f : 0.0f;
        }
    };

    PerformanceStats _perfStats;
    mutable std::mutex _perfStatsMutex;  // 【线程安全修复】保护_perfStats

    // 获取性能统计
    const PerformanceStats& GetPerformanceStats() const { return _perfStats; }

    // 重置性能统计
    void ResetPerformanceStats();

    // 【线程安全】更新性能统计的原子操作
    void UpdatePerfStats(uint64 duration, bool cacheHit, bool dbQuery = false)
    {
        std::lock_guard<std::mutex> lock(_perfStatsMutex);
        _perfStats.totalQueries++;
        if (cacheHit)
            _perfStats.cacheHits++;
        else
            _perfStats.cacheMisses++;
        if (dbQuery)
            _perfStats.dbQueries++;
        _perfStats.totalQueryTime += duration;
        if (_perfStats.totalQueries > 0)
            _perfStats.avgQueryTime = _perfStats.totalQueryTime / _perfStats.totalQueries;
    }

    void IncrementPreloadCount(uint32 count)
    {
        std::lock_guard<std::mutex> lock(_perfStatsMutex);
        _perfStats.preloadCount += count;
    }

    // 获取缓存大小（用于性能统计）
    size_t GetIdentifiedCacheSize() const { return _identifiedItemsCache.size(); }
    size_t GetAttrCacheSize() const { return _attrCache.size(); }
    size_t GetBatchQueryCacheSize() const { return _batchQueryCache.size(); }

private:
    // 已鉴定物品GUID缓存（内存缓存，提升性能）
    std::set<uint32> _identifiedItemsCache;
    mutable std::mutex _identifiedCacheMutex;  // 【线程安全修复】保护_identifiedItemsCache
    bool _cacheInitialized;

    // 初始化缓存
    void InitializeCache();

private:
    // 构造函数只初始化成员变量，不执行任何数据库查询或初始化操作
    ItemIdentificationSystem() : _enabled(true), _baseSuccessRate(100), _destroyOnFail(false),
                                _cost(10000), _enableAnnounce(true), _debugMode(false), _cacheInitialized(false) {}
    ~ItemIdentificationSystem() {}
    static ItemIdentificationSystem* _instance;

    // 从数据库加载鉴定模板
    void LoadIdentificationTemplates();

    // 根据组ID和物品ID随机选择一个鉴定模板（根据几率加权）
    uint32 SelectIdentificationTemplate(uint32 groupId, Item* item);

    // 检查需求条件
    bool CheckRequirements(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用物品成长系统（返回实际应用的组号，0表示失败）
    uint32 ApplyItemGrowth(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用物品强化系统（返回实际应用的组号，0表示失败）
    uint32 ApplyItemEnhancement(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用基础属性
    void ApplyBaseAttributes(Player* player, Item* item, const struct IdentificationTemplate& tmpl, std::string& outAttrDetails, uint32& outAttrCount, uint32& outAttrGroup);

    // 应用追加属性
    uint32 ApplyAdditionalAttributes(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用追加技能
    void ApplyAdditionalSkills(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用技能魔次
    void ApplyMagicHits(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用符文系统
    void ApplyRuneSystem(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用技能套装（返回实际分配的套装ID，0表示未分配）
    uint32 ApplySkillSets(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 鉴定记录管理（私有）
    void SaveIdentificationRecord(const ItemIdentificationRecord& record);
    ItemIdentificationRecord* GetIdentificationRecord(uint32 itemGuid);

    // 物品刷新
    void RefreshItem(Player* player, Item* item);

    // 调试日志
    template<typename... Args>
    void DebugLog(std::string_view format, Args&&... args)
    {
        if (!_debugMode)
            return;

        LOG_DEBUG("module.itemidentification", "{}", Acore::StringFormat(format, std::forward<Args>(args)...));
    }
};

#define sItemIdentificationSystem ItemIdentificationSystem::instance()

// 物品鉴定命令处理类
class ItemIdentificationCommandScript : public CommandScript
{
public:
    ItemIdentificationCommandScript();

    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

private:
    static bool HandleIdentifyCommand(ChatHandler* handler, const char* args);

    // 已删除旧的查询命令：
    // static bool HandleQueryAttributesCommand(ChatHandler* handler, const char* args);  // 旧格式，已废弃
    // static bool HandleQueryCommand(ChatHandler* handler, const char* args);  // 旧格式，已废弃

    // 批量查询命令处理器
    static bool HandleBatchQueryCommand(ChatHandler* handler, const char* args);

    // 新增：性能统计命令
    static bool HandlePerformanceStatsCommand(ChatHandler* handler, const char* args);

    // ========== 【新增】手动鉴定相关命令 ==========
    // 手动鉴定单个物品（使用物品GUID）
    static bool HandleManualIdentifyCommand(ChatHandler* handler, const char* args);
    // 查询待鉴定物品列表
    static bool HandleListPendingCommand(ChatHandler* handler, const char* args);
    // 批量鉴定所有待鉴定物品
    static bool HandleBatchIdentifyCommand(ChatHandler* handler, const char* args);
    // 打开UI界面
    static bool HandleOpenUICommand(ChatHandler* handler, const char* args);
};

// 物品鉴定系统模块加载器
class ItemIdentificationSystemModuleLoader : public WorldScript
{
public:
    ItemIdentificationSystemModuleLoader();

    // 配置加载后触发
    void OnAfterConfigLoad(bool reload) override;

    // 服务器更新时触发
    void OnUpdate(uint32 diff) override;

    // 【关键修复】服务器关闭时保存数据
    void OnShutdownInitiate(ShutdownExitCode code, ShutdownMask mask) override;

private:
    bool _loaded;        // 是否已加载
    uint32 _startTime;   // 开始时间

    // 清理孤立的物品数据（服务器启动时执行）
    void CleanupOrphanedItemData();

    // 定期清理待鉴定物品标记表的孤立数据（每30分钟执行一次）
    void CleanupPendingIdentificationData();
};

// 玩家登录时自动发送属性数据
class ItemIdentificationPlayerScript : public PlayerScript
{
public:
    ItemIdentificationPlayerScript();

    // 玩家登录后触发
    void OnLogin(Player* player, bool firstLogin);
};

#endif // MODULE_ITEM_IDENTIFICATION_SYSTEM_H

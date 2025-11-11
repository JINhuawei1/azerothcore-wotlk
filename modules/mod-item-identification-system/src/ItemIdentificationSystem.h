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
#include <string>
#include <random>

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
            LOG_INFO("module.itemidentification.perf", "[性能监控] {} 耗时: {} 微秒 ({:.2f} 毫秒)",
                     _name, duration, duration / 1000.0);
        }

    private:
        std::string _name;
        std::chrono::high_resolution_clock::time_point _start;
    };

    // 清除已鉴定物品缓存（在物品被销毁或交易时调用）
    void ClearIdentifiedCache(uint32 itemGuid);

    // 批量检查物品是否已鉴定（优化版，减少数据库查询）
    std::set<uint32> BatchCheckIdentified(const std::vector<uint32>& itemGuids);

public:
    // 配置变量
    bool _enabled;
    uint32 _baseSuccessRate;
    bool _destroyOnFail;
    uint32 _cost;
    bool _enableAnnounce;
    bool _debugMode;

private:
    // 已鉴定物品GUID缓存（内存缓存，提升性能）
    std::set<uint32> _identifiedItemsCache;
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
    void ApplyAdditionalAttributes(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用追加技能
    void ApplyAdditionalSkills(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用技能魔次
    void ApplyMagicHits(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用符文系统
    void ApplyRuneSystem(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用技能套装
    void ApplySkillSets(Player* player, Item* item, const struct IdentificationTemplate& tmpl);

    // 应用名称和描述
    void ApplyNameAndDescription(Item* item, const struct IdentificationTemplate& tmpl);

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

    std::vector<ChatCommand> GetCommands() const override;

private:
    static bool HandleIdentifyCommand(ChatHandler* handler, const char* args);
    static bool HandleQueryAttributesCommand(ChatHandler* handler, const char* args);
    static bool HandleQueryCommand(ChatHandler* handler, const char* args);
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

private:
    bool _loaded;        // 是否已加载
    uint32 _startTime;   // 开始时间
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

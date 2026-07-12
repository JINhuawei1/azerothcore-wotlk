#ifndef ITEM_ATTRIBUTES_EFFECTS_H
#define ITEM_ATTRIBUTES_EFFECTS_H

#include "ItemAttributesLoader.h"
#include "Player.h"
#include "Item.h"
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <unordered_set>

// 物品属性效果处理器
class ItemAttributesEffects
{
public:
    static ItemAttributesEffects* instance();

    // 初始化效果处理器
    void Initialize();

    // 【根本性修复】检查是否已初始化
    bool IsInitialized() const { return _isInitialized; }

    // 【根本性修复】安全获取实例（未初始化时返回nullptr）
    static ItemAttributesEffects* SafeInstance();

    // 应用物品属性效果
    void ApplyItemAttributeEffects(Player* player, Item* item);

    // 移除物品的属性效果
    void RemoveItemAttributeEffects(Player* player, Item* item);

    // 根据物品GUID移除属性效果（用于无法获取Item对象的情况）
    void RemoveItemAttributeEffectsByGuid(Player* player, uint64 itemGuid);

    // 更新物品属性效果
    void UpdateItemAttributeEffects(Player* player);

    // 【审计修复】合并式全量属性刷新请求（公开给登录兜底刷新使用）
    void RequestDeferredStatsUpdate(Player* player);

    // 【审计修复】登出时清理该玩家的"已排程刷新/批量更新中"标记：
    // 刷新事件若因玩家对象销毁而未执行，标记会永久残留（登出不清理时甚至跨会话残留），
    // 此后该角色的所有刷新请求都被"已排程"判定挡掉，属性永不再刷新（表现为上线少属性）
    void ClearPlayerPendingUpdateFlags(uint64 playerGuid);

    // 获取物品属性描述
    std::string GetAttributeDescription(Item* item, uint32 attributeId);

private:
    ItemAttributesEffects() : _isInitialized(false), _lastPlayerGuid(0), _pendingUpdateCount(0) {}
    ~ItemAttributesEffects() = default;

    // 【根本性修复】初始化状态标志
    bool _isInitialized;

    // 【审计修复】将全局批量更新标志改为按玩家跟踪，避免跨玩家并发问题
    std::mutex _batchUpdateMutex;
    std::unordered_set<uint64> _batchUpdatePlayers;  // 正在进行批量更新的玩家GUID集合

    // 检查玩家是否正在进行批量更新
    bool IsBatchUpdateInProgress(uint64 playerGuid);
    void SetBatchUpdateInProgress(uint64 playerGuid, bool inProgress);

    // 【性能优化-防抖】防抖机制：短时间内连续调用时延迟UpdateStats
    uint64 _lastPlayerGuid;          // 上次操作的玩家GUID
    uint32 _pendingUpdateCount;      // 待处理的更新计数
    std::chrono::steady_clock::time_point _lastOperationTime;  // 上次操作时间

    // 【性能优化】合并短时间内重复的属性刷新请求
    std::mutex _deferredUpdateMutex;
    std::unordered_set<uint64> _deferredUpdatePlayers;

    // 【性能优化-防抖】检查是否需要立即更新
    bool ShouldUpdateImmediately(Player* player);

    // 属性效果处理函数类型
    using AttributeEffectHandler = std::function<void(Player*, Item*, ItemAttributeTemplate const*, int256)>;
    
    // 属性效果移除函数类型（添加 int256 value 参数，用于传递数据库中保存的值）
    using AttributeEffectRemover = std::function<void(Player*, Item*, ItemAttributeTemplate const*, int256)>;
    
    // 属性描述生成函数类型
    using AttributeDescriptionGenerator = std::function<std::string(Item*, ItemAttributeTemplate const*, int256)>;

    // 注册属性效果处理器
    void RegisterAttributeEffectHandler(uint32 attributeType, AttributeEffectHandler handler, AttributeEffectRemover remover, AttributeDescriptionGenerator descGenerator);
    
    // 属性效果处理器映射
    std::map<uint32, AttributeEffectHandler> _attributeEffectHandlers;
    
    // 属性效果移除器映射
    std::map<uint32, AttributeEffectRemover> _attributeEffectRemovers;
    
    // 属性描述生成器映射
    std::map<uint32, AttributeDescriptionGenerator> _attributeDescriptionGenerators;
};

#define sItemAttributesEffects ItemAttributesEffects::instance()

// 【根本性修复】安全版本的宏，未初始化时返回nullptr
#define sItemAttributesEffectsSafe ItemAttributesEffects::SafeInstance()

#endif // ITEM_ATTRIBUTES_EFFECTS_H

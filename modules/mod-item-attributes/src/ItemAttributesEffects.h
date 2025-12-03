#ifndef ITEM_ATTRIBUTES_EFFECTS_H
#define ITEM_ATTRIBUTES_EFFECTS_H

#include "ItemAttributesLoader.h"
#include "Player.h"
#include "Item.h"
#include <map>
#include <functional>

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
    
    // 获取物品属性描述
    std::string GetAttributeDescription(Item* item, uint32 attributeId);

private:
    ItemAttributesEffects() : _isInitialized(false) {}
    ~ItemAttributesEffects() = default;

    // 【根本性修复】初始化状态标志
    bool _isInitialized;

    // 属性效果处理函数类型
    using AttributeEffectHandler = std::function<void(Player*, Item*, ItemAttributeTemplate const*, int32)>;
    
    // 属性效果移除函数类型（添加 int32 value 参数，用于传递数据库中保存的值）
    using AttributeEffectRemover = std::function<void(Player*, Item*, ItemAttributeTemplate const*, int32)>;
    
    // 属性描述生成函数类型
    using AttributeDescriptionGenerator = std::function<std::string(Item*, ItemAttributeTemplate const*, int32)>;

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

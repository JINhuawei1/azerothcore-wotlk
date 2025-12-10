#ifndef ITEM_ATTRIBUTES_LOADER_H
#define ITEM_ATTRIBUTES_LOADER_H

#include "Define.h"
#include <map>
#include <string>
#include <vector>

class Item;
class Player;

struct ItemAttributeTemplate
{
    uint32 id;                  // 属性ID，唯一标识符
    std::string comment;        // 属性描述，仅供开发者参考
    std::string clientDisplay;  // 客户端显示的属性名称
    uint32 group;               // 属性组，用于分类属性
    uint32 attributeType;       // 属性类型，决定属性的效果
    uint32 chance;              // 随机生成时的获取几率，1-100
    int32 minPercent;           // 属性值的最小百分比
    int32 maxPercent;           // 属性值的最大百分比
    uint32 calcType;            // 0=常规值 1=乘以物品等级 2=固定值
    uint32 skillGroup;          // 用于技能相关属性
    uint32 qualityRequirement;  // 物品最低品质要求，0=全部
    uint32 levelRequirement;    // 物品最低等级要求
    uint32 classRequirement;    // 物品职业要求，0=全部
    std::string color;          // 属性文本颜色，格式为RRGGBB
};

// 物品属性应用结果
enum class ItemAttributeResult
{
    SUCCESS,
    FAILED,
    ALREADY_APPLIED,
    INVALID_ITEM,
    INVALID_ATTRIBUTE,
    NOT_ENOUGH_MATERIALS,
    ITEM_NOT_SUITABLE
};

// 属性类型（基础或追加）
enum class AttributeCategory
{
    BASE,        // 基础属性（替换官方属性）
    ADDITIONAL   // 追加属性（额外添加）
};

class ItemAttributesLoader
{
public:
    static ItemAttributesLoader* instance();

    // 【根本性修复】检查是否已初始化
    bool IsInitialized() const { return _isInitialized; }

    // 【根本性修复】安全获取实例（未初始化时返回nullptr）
    static ItemAttributesLoader* SafeInstance();

    // 加载和获取模板
    void LoadItemAttributeTemplates();
    ItemAttributeTemplate const* GetItemAttributeTemplate(uint32 id) const;
    ItemAttributeTemplate const* GetItemAttributeTemplateByType(uint32 attributeType) const;  // 通过属性类型查询
    std::vector<ItemAttributeTemplate const*> GetItemAttributeTemplatesByGroup(uint32 group) const;
    std::vector<ItemAttributeTemplate const*> GetItemAttributeTemplatesByType(uint32 type) const;
    std::vector<ItemAttributeTemplate const*> GetAllItemAttributeTemplates() const;

    // 物品属性应用和移除
    ItemAttributeResult ApplyAttributeToItem(Item* item, uint32 attributeId, Player* player = nullptr, AttributeCategory category = AttributeCategory::ADDITIONAL, int32 minValue = 0, int32 maxValue = 0);
    ItemAttributeResult RemoveAttributeFromItem(Item* item, uint32 attributeId);

    // 物品属性检查
    bool HasAttribute(Item* item, uint32 attributeId) const;
    bool HasAttributeByType(Item* item, uint32 attributeType) const;  // 根据属性类型检查
    bool GetItemAttributesWithValues(Item* item, std::vector<uint32>& attributes, std::vector<int32>& values, uint32* itemId = nullptr) const;
    std::vector<uint32> GetItemAttributes(Item* item) const;
    std::vector<int32> GetItemAttributeValues(Item* item) const;

    // 物品属性计算
    int32 CalculateAttributeValue(Item* item, ItemAttributeTemplate const* attributeTemplate) const;
    int32 CalculateAttributeValueWithRange(Item* item, ItemAttributeTemplate const* attributeTemplate, int32 minValue, int32 maxValue) const;

    // 保存物品属性值到数据库
    void SaveItemAttributeValues(Item* item, const std::vector<int32>& values);
    
    // 删除物品的属性数据（当物品被删除时）
    void DeleteItemAttributeData(uint64 itemGuid);
    
    // 清理孤立的属性数据（物品已不存在但数据还在）
    void CleanupOrphanedAttributeData();

    // 调试功能
    void DumpItemAttributes(Item* item) const;

private:
    ItemAttributesLoader() : _isInitialized(false) {}
    ~ItemAttributesLoader() = default;

    // 【根本性修复】初始化状态标志
    bool _isInitialized;

    std::map<uint32, ItemAttributeTemplate> _itemAttributeTemplateStore;
};

// 【关键修复】让sItemAttributesLoader指向SafeInstance，自动进行初始化检查
#define sItemAttributesLoader ItemAttributesLoader::SafeInstance()

// 【不推荐使用】不安全的版本，跳过初始化检查
#define sItemAttributesLoaderUnsafe ItemAttributesLoader::instance()

#endif // ITEM_ATTRIBUTES_LOADER_H

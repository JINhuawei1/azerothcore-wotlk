#include "ItemAttributesGenerator.h"
#include "Logging/Log.h"
#include <random>
#include <algorithm>

// 【线程安全】Meyer's Singleton - C++11保证静态局部变量初始化的线程安全性
// 编译器会自动添加同步机制，确保多线程并发调用时只初始化一次
ItemAttributesGenerator* ItemAttributesGenerator::instance()
{
    static ItemAttributesGenerator instance;
    return &instance;
}

void ItemAttributesGenerator::Initialize()
{
    // 【根本性修复】标记初始化完成
    _isInitialized = true;
}

bool ItemAttributesGenerator::GenerateRandomAttributes(Item* item, ItemAttributeGenerateOptions const& options)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.item-attributes", "【致命错误】GenerateRandomAttributes 被调用，但系统尚未初始化！");
        return false;
    }

    if (!item)
        return false;

    // 获取适合物品的属性列表
    std::vector<ItemAttributeTemplate const*> suitableAttributes = GetSuitableAttributes(item, options.attributeGroup);

    if (suitableAttributes.empty())
        return false;

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());

    // 【修复】如果启用了值范围过滤，进行额外的过滤
    if (options.useValueRangeFilter && options.minItemLevel <= options.maxItemLevel && options.maxItemLevel > 0)
    {
        std::vector<ItemAttributeTemplate const*> filteredByValue;

        for (ItemAttributeTemplate const* attributeTemplate : suitableAttributes)
        {
            // 检查属性模板的值范围是否与要求的范围有交集
            uint32 attrMin = attributeTemplate->minPercent;
            uint32 attrMax = attributeTemplate->maxPercent;
            uint32 reqMin = options.minItemLevel;
            uint32 reqMax = options.maxItemLevel;

            // 如果两个范围有交集，则该属性合格
            // 两个范围无交集的条件是：attrMax < reqMin 或 attrMin > reqMax
            // 取反就是有交集
            if (!(attrMax < reqMin || attrMin > reqMax))
            {
                filteredByValue.push_back(attributeTemplate);
            }
        }

        // 如果有符合值范围的属性，使用过滤后的列表；否则继续使用原列表（降级处理）
        if (!filteredByValue.empty())
        {
            suitableAttributes = filteredByValue;
        }
    }

    // 如果考虑属性获取几率，则根据几率筛选属性
    if (options.respectChance)
    {
        std::vector<ItemAttributeTemplate const*> filteredAttributes;

        for (ItemAttributeTemplate const* attributeTemplate : suitableAttributes)
        {
            std::uniform_int_distribution<> dis(1, 100);
            if (dis(gen) <= attributeTemplate->chance)
            {
                filteredAttributes.push_back(attributeTemplate);
            }
        }

        if (!filteredAttributes.empty())
        {
            suitableAttributes = filteredAttributes;
        }
    }

    // 随机决定属性数量
    uint32 attributeCount = options.minAttributes;
    if (options.maxAttributes > options.minAttributes)
    {
        std::uniform_int_distribution<> dis(options.minAttributes, options.maxAttributes);
        attributeCount = dis(gen);
    }

    // 限制属性数量不超过可用属性数量
    attributeCount = std::min(attributeCount, static_cast<uint32>(suitableAttributes.size()));

    // 如果不允许重复属性类型，则需要进一步筛选
    if (!options.allowDuplicateTypes)
    {
        std::vector<ItemAttributeTemplate const*> uniqueTypeAttributes;
        std::set<uint32> usedTypes;

        for (ItemAttributeTemplate const* attributeTemplate : suitableAttributes)
        {
            if (usedTypes.find(attributeTemplate->attributeType) == usedTypes.end())
            {
                uniqueTypeAttributes.push_back(attributeTemplate);
                usedTypes.insert(attributeTemplate->attributeType);
            }
        }

        if (!uniqueTypeAttributes.empty())
        {
            suitableAttributes = uniqueTypeAttributes;
        }

        // 再次限制属性数量
        attributeCount = std::min(attributeCount, static_cast<uint32>(suitableAttributes.size()));
    }

    // 随机选择属性
    std::shuffle(suitableAttributes.begin(), suitableAttributes.end(), gen);

    // 应用选中的属性
    for (uint32 i = 0; i < attributeCount; ++i)
    {
        ItemAttributeTemplate const* attributeTemplate = suitableAttributes[i];
        // 传递自定义值范围（如果有）
        sItemAttributesLoader->ApplyAttributeToItem(item, attributeTemplate->id, nullptr, options.category, options.minItemLevel, options.maxItemLevel);
    }

    return true;
}

bool ItemAttributesGenerator::GenerateAttributesByItemLevel(Item* item)
{
    if (!item)
        return false;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return false;

    // 根据物品等级设置生成选项
    ItemAttributeGenerateOptions options;
    options.minAttributes = 1;
    options.maxAttributes = 1 + (itemTemplate->ItemLevel / 20); // 每20级增加1个属性上限
    options.attributeGroup = 999; // 所有组（使用999表示）
    options.minItemLevel = 0;
    options.maxItemLevel = 0;
    options.respectChance = true;
    options.allowDuplicateTypes = false;

    return GenerateRandomAttributes(item, options);
}

bool ItemAttributesGenerator::GenerateAttributesByItemQuality(Item* item)
{
    if (!item)
        return false;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return false;

    // 根据物品品质设置生成选项
    ItemAttributeGenerateOptions options;

    switch (itemTemplate->Quality)
    {
        case ITEM_QUALITY_POOR: // 灰色
            options.minAttributes = 0;
            options.maxAttributes = 0;
            break;
        case ITEM_QUALITY_NORMAL: // 白色
            options.minAttributes = 0;
            options.maxAttributes = 1;
            break;
        case ITEM_QUALITY_UNCOMMON: // 绿色
            options.minAttributes = 1;
            options.maxAttributes = 2;
            break;
        case ITEM_QUALITY_RARE: // 蓝色
            options.minAttributes = 2;
            options.maxAttributes = 3;
            break;
        case ITEM_QUALITY_EPIC: // 紫色
            options.minAttributes = 3;
            options.maxAttributes = 4;
            break;
        case ITEM_QUALITY_LEGENDARY: // 橙色
            options.minAttributes = 4;
            options.maxAttributes = 5;
            break;
        case ITEM_QUALITY_ARTIFACT: // 神器
            options.minAttributes = 5;
            options.maxAttributes = 6;
            break;
        default:
            options.minAttributes = 0;
            options.maxAttributes = 0;
            break;
    }

    options.attributeGroup = 999; // 所有组（使用999表示）
    options.minItemLevel = 0;
    options.maxItemLevel = 0;
    options.respectChance = true;
    options.allowDuplicateTypes = false;

    return GenerateRandomAttributes(item, options);
}

std::vector<ItemAttributeTemplate const*> ItemAttributesGenerator::GetSuitableAttributes(Item* item, uint32 attributeGroup)
{
    std::vector<ItemAttributeTemplate const*> result;

    if (!item)
        return result;

    std::vector<ItemAttributeTemplate const*> allAttributes;

    // 修复：attributeGroup为0时也应该按组筛选，而不是获取所有组
    // 只有当attributeGroup为特殊值（如-1或999）时才获取所有组
    if (attributeGroup == 999 || attributeGroup == static_cast<uint32>(-1))
    {
        allAttributes = sItemAttributesLoader->GetAllItemAttributeTemplates();
    }
    else
    {
        allAttributes = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(attributeGroup);
    }

    for (ItemAttributeTemplate const* attributeTemplate : allAttributes)
    {
        if (IsAttributeSuitableForItem(item, attributeTemplate))
        {
            result.push_back(attributeTemplate);
        }
    }

    return result;
}

bool ItemAttributesGenerator::IsAttributeSuitableForItem(Item* item, ItemAttributeTemplate const* attributeTemplate)
{
    if (!item || !attributeTemplate)
        return false;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return false;

    // 检查物品类型是否适合属性
    // 这里可以根据需要添加更多的检查逻辑

    // 示例：武器类物品不适合某些属性
    if (itemTemplate->Class == ITEM_CLASS_WEAPON)
    {
        // 示例：武器不适合防御类属性
        if (attributeTemplate->attributeType == 12) // 防御等级
            return false;
    }

    // 示例：护甲类物品不适合某些属性
    if (itemTemplate->Class == ITEM_CLASS_ARMOR)
    {
        // 示例：护甲不适合武器伤害属性
        if (attributeTemplate->attributeType >= 121 && attributeTemplate->attributeType <= 134) // 伤害属性
            return false;
    }

    return true;
}


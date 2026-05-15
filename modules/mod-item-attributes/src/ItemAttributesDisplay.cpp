#include "ItemAttributesDisplay.h"
#include "Logging/Log.h"
#include <sstream>

// 【线程安全】Meyer's Singleton - C++11保证静态局部变量初始化的线程安全性
// 编译器会自动添加同步机制，确保多线程并发调用时只初始化一次
ItemAttributesDisplay* ItemAttributesDisplay::instance()
{
    static ItemAttributesDisplay instance;
    return &instance;
}

void ItemAttributesDisplay::Initialize()
{
    // 【根本性修复】标记初始化完成
    _isInitialized = true;
}

void ItemAttributesDisplay::AddItemAttributesToTooltip(Player* player, Item* item, std::vector<std::string>& lines)
{
    // 【根本性修复】检查依赖系统是否已初始化
    if (!_isInitialized || !sItemAttributesLoader || !sItemAttributesLoader->IsInitialized())
    {
        LOG_ERROR("module.item-attributes", "【致命错误】AddItemAttributesToTooltip 被调用，但系统或依赖未初始化！");
        return;
    }

    if (!player || !item)
        return;

    std::vector<uint32> attributes = sItemAttributesLoader->GetItemAttributes(item);

    if (attributes.empty())
        return;

    // 添加属性标题
    lines.push_back("|cFFFFFF00物品属性:|r");

    // 添加各个属性描述
    for (uint32 attributeId : attributes)
    {
        // 【重要】GetItemAttributes返回的是属性类型，不是模板ID
        ItemAttributeTemplate const* attributeTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(attributeId);
        if (!attributeTemplate)
            continue;

        // 【关键修复】添加空指针检查
        if (!sItemAttributesEffects)
            continue;

        std::string description = sItemAttributesEffects->GetAttributeDescription(item, attributeId);
        std::string colorCode = GetAttributeColorCode(attributeTemplate);

        lines.push_back(colorCode + description + "|r");
    }
}

std::string ItemAttributesDisplay::GenerateItemAttributesDescription(Item* item)
{
    if (!item)
        return "";

    std::vector<uint32> attributes = sItemAttributesLoader->GetItemAttributes(item);

    if (attributes.empty())
        return "";

    std::ostringstream ss;
    ss << "|cFFFFFF00物品属性:|r";

    for (uint32 attributeId : attributes)
    {
        // 【重要】GetItemAttributes返回的是属性类型，不是模板ID
        ItemAttributeTemplate const* attributeTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(attributeId);
        if (!attributeTemplate)
            continue;

        // 【关键修复】添加空指针检查
        if (!sItemAttributesEffects)
            continue;

        std::string description = sItemAttributesEffects->GetAttributeDescription(item, attributeId);
        std::string colorCode = GetAttributeColorCode(attributeTemplate);

        ss << "\n" << colorCode << description << "|r";
    }

    return ss.str();
}

std::string ItemAttributesDisplay::GetAttributeColorCode(ItemAttributeTemplate const* attributeTemplate)
{
    if (!attributeTemplate)
        return "|cFFFFFFFF"; // 白色

    // 根据属性组或类型返回不同的颜色代码
    switch (attributeTemplate->group)
    {
        case 0: // 默认组
            return "|cFFFFFFFF"; // 白色
        case 999: // 特殊组
            return "|cFF00FF00"; // 绿色
        case 2000: // 装备强化组
            return "|cFF00FFFF"; // 青色
        case 2100: // 装备成长组
            return "|cFFFF00FF"; // 品红色
        case 2200: // 装备追加属性组
            return "|cFFFFFF00"; // 黄色
        case 3001: // 特殊属性组1
            return "|cFF33FF66"; // 浅绿色
        case 3002: // 特殊属性组2
            return "|cFFF0B42A"; // 橙色
        case 3005: // 特殊属性组5
            return "|cFFFF00FF"; // 品红色
        case 3006: // 特殊属性组6
            return "|cFFFF00FF"; // 品红色
        default:
            break;
    }

    // 根据属性类型返回颜色代码
    if (attributeTemplate->attributeType >= 0 && attributeTemplate->attributeType <= 7)
        return "|cFFFFFFFF"; // 基础属性，白色
    else if (attributeTemplate->attributeType >= 12 && attributeTemplate->attributeType <= 48)
        return "|cFF00FF00"; // 战斗属性，绿色
    else if (attributeTemplate->attributeType >= 120 && attributeTemplate->attributeType <= 136)
        return "|cFF00FFFF"; // 伤害属性，青色
    else if (attributeTemplate->attributeType >= 150 && attributeTemplate->attributeType <= 155)
        return "|cFFFF00FF"; // 抗性属性，品红色
    else if (attributeTemplate->attributeType >= 200 && attributeTemplate->attributeType <= 230)
        return "|cFFFF0000"; // PVP属性，红色
    else if (attributeTemplate->attributeType >= 300 && attributeTemplate->attributeType <= 330)
        return "|cFF00FF00"; // PVE属性，绿色
    else if (attributeTemplate->attributeType >= 400 && attributeTemplate->attributeType <= 408)
        return "|cFFFF00FF"; // 特殊属性，品红色
    else if (attributeTemplate->attributeType >= 500 && attributeTemplate->attributeType <= 545)
        return "|cFFF0B42A"; // 百分比属性，橙色
    else if (attributeTemplate->attributeType >= 600 && attributeTemplate->attributeType <= 609)
        return "|cFFF0B42A"; // 转换属性，橙色
    else if (attributeTemplate->attributeType >= 1000 && attributeTemplate->attributeType <= 1200)
        return "|cFFF0B42A"; // 技能属性，橙色

    return "|cFFFFFFFF"; // 默认白色
}


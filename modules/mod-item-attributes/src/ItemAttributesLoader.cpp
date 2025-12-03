#include "ItemAttributesLoader.h"
#include "ItemAttributesDatabase.h"
#include "ItemAttributesEffects.h"
#include "ItemAttributesDBHelper.h"
#include "DatabaseEnv.h"
#include "Logging/Log.h"
#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Item.h"
#include "Player.h"
#include "ObjectMgr.h"
#include <random>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>

// 将时间戳转换为可读的时间字符串
std::string TimeToTimestampStr(time_t t)
{
    if (t == 0)
        return "未知";

    std::tm* timeinfo = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(timeinfo, "{Y}-{m}-{d} {H}:{M}:{S}");
    return ss.str();
}

ItemAttributesLoader* ItemAttributesLoader::instance()
{
    static ItemAttributesLoader instance;
    return &instance;
}

void ItemAttributesLoader::LoadItemAttributeTemplates()
{
    _itemAttributeTemplateStore.clear();

    uint32 oldMSTime = getMSTime();

    // 检查表是否存在，如果不存在则跳过加载
    QueryResult checkTable = WorldDatabase.Query("SHOW TABLES LIKE '物品属性_模板'");
    if (!checkTable || checkTable->GetRowCount() == 0)
    {
        LOG_ERROR("module.item-attributes", ">> 表 `物品属性_模板` 不存在，请导入SQL文件");
        LOG_ERROR("module.item-attributes", ">> 请确保已导入 modules/mod-item-attributes/data/sql/db_world/ 目录下的所有SQL文件");
        return;
    }

    QueryResult result = WorldDatabase.Query("SELECT `id`, `注释`, `客户端显示`, `组`, `属性类型`, `获取几率`, `属性最小百分比`, `属性最大百分比`, `属性计算方式`, `技能模板_组`, `属性品质要求`, `属性等级要求`, `属性职业要求`, `属性颜色` FROM `物品属性_模板`");

    if (!result)
    {
        LOG_WARN("module.item-attributes", ">> 未找到任何物品属性模板数据");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        ItemAttributeTemplate attributeTemplate;
        attributeTemplate.id = fields[0].Get<uint32>();
        attributeTemplate.comment = fields[1].Get<std::string>();
        attributeTemplate.clientDisplay = fields[2].Get<std::string>();
        attributeTemplate.group = fields[3].Get<uint32>();
        attributeTemplate.attributeType = fields[4].Get<uint32>();
        attributeTemplate.chance = fields[5].Get<uint32>();
        attributeTemplate.minPercent = fields[6].Get<int32>();
        attributeTemplate.maxPercent = fields[7].Get<int32>();
        attributeTemplate.calcType = fields[8].Get<uint32>();
        attributeTemplate.skillGroup = fields[9].Get<uint32>();
        attributeTemplate.qualityRequirement = fields[10].Get<uint32>();
        attributeTemplate.levelRequirement = fields[11].Get<uint32>();
        attributeTemplate.classRequirement = fields[12].Get<uint32>();
        attributeTemplate.color = fields[13].Get<std::string>();

        _itemAttributeTemplateStore[attributeTemplate.id] = attributeTemplate;
        ++count;
    } while (result->NextRow());

    LOG_INFO("module.item-attributes", ">> 已加载 {} 个物品属性模板", count);


}

ItemAttributeTemplate const* ItemAttributesLoader::GetItemAttributeTemplate(uint32 id) const
{
    auto itr = _itemAttributeTemplateStore.find(id);
    if (itr != _itemAttributeTemplateStore.end())
        return &itr->second;

    return nullptr;
}

std::vector<ItemAttributeTemplate const*> ItemAttributesLoader::GetItemAttributeTemplatesByGroup(uint32 group) const
{
    std::vector<ItemAttributeTemplate const*> result;

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        if (pair.second.group == group)
            result.push_back(&pair.second);
    }

    return result;
}

std::vector<ItemAttributeTemplate const*> ItemAttributesLoader::GetItemAttributeTemplatesByType(uint32 type) const
{
    std::vector<ItemAttributeTemplate const*> result;

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        if (pair.second.attributeType == type)
            result.push_back(&pair.second);
    }

    return result;
}

ItemAttributeTemplate const* ItemAttributesLoader::GetItemAttributeTemplateByType(uint32 type) const
{
    for (auto const& pair : _itemAttributeTemplateStore)
    {
        if (pair.second.attributeType == type)
            return &pair.second;
    }

    return nullptr;
}

std::vector<ItemAttributeTemplate const*> ItemAttributesLoader::GetAllItemAttributeTemplates() const
{
    std::vector<ItemAttributeTemplate const*> result;
    result.reserve(_itemAttributeTemplateStore.size());

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        result.push_back(&pair.second);
    }

    return result;
}

ItemAttributeResult ItemAttributesLoader::ApplyAttributeToItem(Item* item, uint32 attributeId, Player* player, AttributeCategory category, int32 minValue, int32 maxValue)
{
    if (!item)
        return ItemAttributeResult::INVALID_ITEM;

    ItemAttributeTemplate const* attributeTemplate = GetItemAttributeTemplate(attributeId);
    if (!attributeTemplate)
        return ItemAttributeResult::INVALID_ATTRIBUTE;

    // 检查物品是否已经有该属性（使用属性类型检查）
    if (HasAttributeByType(item, attributeTemplate->attributeType))
        return ItemAttributeResult::ALREADY_APPLIED;

    // 检查物品品质是否符合要求
    if (attributeTemplate->qualityRequirement > 0 && item->GetTemplate()->Quality < attributeTemplate->qualityRequirement)
        return ItemAttributeResult::ITEM_NOT_SUITABLE;

    // 检查物品等级是否符合要求
    if (attributeTemplate->levelRequirement > 0 && item->GetTemplate()->ItemLevel < attributeTemplate->levelRequirement)
        return ItemAttributeResult::ITEM_NOT_SUITABLE;

    // 检查物品职业要求
    if (attributeTemplate->classRequirement > 0 && !(item->GetTemplate()->AllowableClass & attributeTemplate->classRequirement))
        return ItemAttributeResult::ITEM_NOT_SUITABLE;

    // 计算属性值
    int32 attributeValue;
    if (minValue > 0 || maxValue > 0)
    {
        // 如果指定了自定义值范围，使用自定义范围
        attributeValue = CalculateAttributeValueWithRange(item, attributeTemplate, minValue, maxValue);
    }
    else
    {
        // 否则使用模板中的默认范围
        attributeValue = CalculateAttributeValue(item, attributeTemplate);
    }

    // 使用新的DBHelper API保存属性
    // 【重要】保存属性类型而不是属性模板ID，确保数据一致性
    uint64 itemGuid = item->GetGUID().GetCounter();
    uint32 itemId = item->GetEntry();
    uint32 attrTypeToSave = attributeTemplate->attributeType;  // 使用属性类型

    bool success = false;
    if (category == AttributeCategory::BASE)
    {
        // 添加到基础属性（保存属性类型）
        success = ItemAttributesDBHelper::AddBaseAttributeToItem(itemGuid, itemId, attrTypeToSave, attributeValue);
    }
    else
    {
        // 添加到追加属性（保存属性类型）
        success = ItemAttributesDBHelper::AddAdditionalAttributeToItem(itemGuid, itemId, attrTypeToSave, attributeValue);
    }

    if (!success)
        return ItemAttributeResult::FAILED;

    // 如果有玩家参数，可以发送消息通知
    if (player)
    {
        std::string categoryStr = (category == AttributeCategory::BASE) ? "基础属性" : "追加属性";
        std::string message = "物品 " + std::string(item->GetTemplate()->Name1) + " 已添加" + categoryStr + ": " + attributeTemplate->clientDisplay;
        player->GetSession()->SendAreaTriggerMessage("{}", message);
    }

    return ItemAttributeResult::SUCCESS;
}

ItemAttributeResult ItemAttributesLoader::RemoveAttributeFromItem(Item* item, uint32 attributeId)
{
    if (!item)
        return ItemAttributeResult::INVALID_ITEM;

    // 检查物品是否有该属性
    if (!HasAttribute(item, attributeId))
        return ItemAttributeResult::FAILED;

    // 使用DBHelper移除属性
    uint64 itemGuid = item->GetGUID().GetCounter();
    bool success = ItemAttributesDBHelper::RemoveAttributeFromItem(itemGuid, attributeId);

    return success ? ItemAttributeResult::SUCCESS : ItemAttributeResult::FAILED;
}

bool ItemAttributesLoader::HasAttribute(Item* item, uint32 attributeId) const
{
    if (!item)
        return false;

    std::vector<uint32> attributes;
    std::vector<int32> values;
    if (!GetItemAttributesWithValues(item, attributes, values))
        return false;

    return std::find(attributes.begin(), attributes.end(), attributeId) != attributes.end();
}

bool ItemAttributesLoader::HasAttributeByType(Item* item, uint32 attributeType) const
{
    if (!item)
        return false;

    std::vector<uint32> attributes;
    std::vector<int32> values;
    if (!GetItemAttributesWithValues(item, attributes, values))
        return false;

    return std::find(attributes.begin(), attributes.end(), attributeType) != attributes.end();
}

bool ItemAttributesLoader::GetItemAttributesWithValues(Item* item, std::vector<uint32>& attributes, std::vector<int32>& values, uint32* itemId) const
{
    attributes.clear();
    values.clear();

    if (!item)
        return false;

    uint64 itemGuid = item->GetGUID().GetCounter();
    ItemAttributesDBHelper::ItemAttributeData* data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);

    if (!data)
        return false;

    if (itemId)
        *itemId = data->itemId;

    attributes.reserve(data->baseAttributeIds.size() + data->additionalAttributeIds.size());
    values.reserve(data->baseAttributeValues.size() + data->additionalAttributeValues.size());

    attributes.insert(attributes.end(), data->baseAttributeIds.begin(), data->baseAttributeIds.end());
    attributes.insert(attributes.end(), data->additionalAttributeIds.begin(), data->additionalAttributeIds.end());

    values.insert(values.end(), data->baseAttributeValues.begin(), data->baseAttributeValues.end());
    values.insert(values.end(), data->additionalAttributeValues.begin(), data->additionalAttributeValues.end());

    delete data;
    return true;
}

std::vector<uint32> ItemAttributesLoader::GetItemAttributes(Item* item) const
{
    std::vector<uint32> result;

    if (!item)
        return result;

    std::vector<int32> values;
    GetItemAttributesWithValues(item, result, values);
    return result;
}

std::vector<int32> ItemAttributesLoader::GetItemAttributeValues(Item* item) const
{
    std::vector<int32> result;

    if (!item)
        return result;

    std::vector<uint32> attributes;
    GetItemAttributesWithValues(item, attributes, result);
    return result;
}

int32 ItemAttributesLoader::CalculateAttributeValue(Item* item, ItemAttributeTemplate const* attributeTemplate) const
{
    if (!item || !attributeTemplate)
        return 0;

    int32 baseValue = 0;

    // 根据计算方式确定基础值
    switch (attributeTemplate->calcType)
    {
        case 0: // 常规
            baseValue = attributeTemplate->minPercent;
            break;
        case 1: // 乘以物品等级
            baseValue = attributeTemplate->minPercent * item->GetTemplate()->ItemLevel / 100;
            break;
        case 2: // 固定值
            baseValue = attributeTemplate->minPercent;
            break;
        default:
            baseValue = attributeTemplate->minPercent;
            break;
    }

    // 如果最小值和最大值不同，随机生成一个范围内的值
    if (attributeTemplate->minPercent != attributeTemplate->maxPercent)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(attributeTemplate->minPercent, attributeTemplate->maxPercent);

        if (attributeTemplate->calcType == 1) // 乘以物品等级
            baseValue = dis(gen) * item->GetTemplate()->ItemLevel / 100;
        else
            baseValue = dis(gen);
    }

    return baseValue;
}

int32 ItemAttributesLoader::CalculateAttributeValueWithRange(Item* item, ItemAttributeTemplate const* attributeTemplate, int32 minValue, int32 maxValue) const
{
    if (!item || !attributeTemplate)
        return 0;

    // 使用指定的值范围直接生成属性值
    if (minValue <= 0 && maxValue <= 0)
    {
        // 如果范围无效，回退到默认计算
        return CalculateAttributeValue(item, attributeTemplate);
    }

    // 确保最小值不大于最大值
    if (minValue > maxValue)
    {
        std::swap(minValue, maxValue);
    }

    // 在指定范围内随机生成属性值
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(minValue, maxValue);
    
    return dis(gen);
}

void ItemAttributesLoader::DumpItemAttributes(Item* item) const
{
    if (!item)
    {
        LOG_ERROR("module.item-attributes", "ItemAttributesLoader::DumpItemAttributes: 无效的物品");
        return;
    }

    ItemTemplate const* itemTemplate = item->GetTemplate();
    LOG_INFO("module.item-attributes", "物品属性信息 - [{}] {}:", itemTemplate->ItemId, itemTemplate->Name1);

    std::vector<uint32> attributes;
    std::vector<int32> values;
    uint32 itemId = 0;

    if (!GetItemAttributesWithValues(item, attributes, values, &itemId) || attributes.empty())
    {
        LOG_INFO("module.item-attributes", "  该物品没有任何属性");
        return;
    }

    // 显示属性基本信息
    LOG_INFO("module.item-attributes", "  物品ID: {}", itemId);
    LOG_INFO("module.item-attributes", "  属性总数: {}", static_cast<uint32>(attributes.size()));

    // 显示每个属性的详细信息
    LOG_INFO("module.item-attributes", "  属性列表:");
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeType = attributes[i];
        // 【重要】attributes中存的是属性类型，不是模板ID
        ItemAttributeTemplate const* attributeTemplate = GetItemAttributeTemplateByType(attributeType);
        if (!attributeTemplate)
        {
            LOG_ERROR("module.item-attributes", "    无效的属性类型: {}", attributeType);
            continue;
        }

        int32 value = (i < values.size()) ? values[i] : 0;
        LOG_INFO("module.item-attributes", "    [{}] {}: {} (类型: {}, 组: {})",
            attributeTemplate->id,
            attributeTemplate->clientDisplay,
            value,
            attributeTemplate->attributeType,
            attributeTemplate->group);

        // 【关键修复】添加空指针检查
        // 显示属性的详细描述
        if (sItemAttributesEffects)
        {
            std::string description = sItemAttributesEffects->GetAttributeDescription(item, attributeType);
            LOG_INFO("module.item-attributes", "      描述: {}", description);
        }
    }
}

void ItemAttributesLoader::DeleteItemAttributeData(uint64 itemGuid)
{
    if (itemGuid == 0)
        return;

    ItemAttributesDBHelper::ClearItemAttributes(itemGuid);
}

void ItemAttributesLoader::CleanupOrphanedAttributeData()
{
    CharacterDatabase.Execute(
        "DELETE FROM `物品属性_数据` WHERE `物品GUID` NOT IN (SELECT `guid` FROM `item_instance`)"
    );
    ItemAttributesDBHelper::FlushCache();
}


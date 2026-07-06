#include "ItemAttributesDBHelper.h"
#include "ItemAttributesLoader.h"
#include "Log.h"
#include "StringConvert.h"
#include <sstream>

std::mutex ItemAttributesDBHelper::_cacheMutex;
std::unordered_map<uint64, ItemAttributesDBHelper::ItemAttributeData> ItemAttributesDBHelper::_cache;

// 读取物品的所有属性
// 辅助方法：将属性转换为紧凑字符串格式 "id value,id value,..."
std::string ItemAttributesDBHelper::ItemAttributeData::ToCompactString(const std::vector<uint32>& ids, const std::vector<int256>& values)
{
    if (ids.empty() || ids.size() != values.size())
        return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i > 0)
            oss << ",";
        oss << ids[i] << " " << Acore::ToString(values[i]);
    }
    return oss.str();
}

// 辅助方法：从紧凑字符串解析属性 "id value,id value,..."
void ItemAttributesDBHelper::ItemAttributeData::FromCompactString(const std::string& str, std::vector<uint32>& ids, std::vector<int256>& values)
{
    ids.clear();
    values.clear();
    
    if (str.empty())
        return;
    
    std::istringstream iss(str);
    std::string pair;
    
    while (std::getline(iss, pair, ','))
    {
        std::istringstream pairStream(pair);
        std::string idToken;
        std::string valueToken;
        
        if (pairStream >> idToken >> valueToken)
        {
            Optional<uint32> id = Acore::StringTo<uint32>(idToken);
            Optional<int256> value = Acore::StringTo<int256>(valueToken);
            if (id && value)
            {
                ids.push_back(*id);
                values.push_back(*value);
            }
        }
    }
}

// 【智能指针修复】读取物品的所有属性（返回 unique_ptr，自动管理内存）
std::unique_ptr<ItemAttributesDBHelper::ItemAttributeData> ItemAttributesDBHelper::LoadItemAttributes(uint64 itemGuid)
{
    if (itemGuid == 0)
        return nullptr;

    ItemAttributeData cached;
    if (TryGetCachedData(itemGuid, cached))
        return std::make_unique<ItemAttributeData>(cached);  // 【修复】使用 make_unique

    QueryResult result = CharacterDatabase.Query(
        "SELECT `物品ID`, `基础属性`, `追加属性` "
        "FROM `物品属性_数据` WHERE `物品GUID` = {}",
        itemGuid
    );

    if (!result)
        return nullptr;

    Field* fields = result->Fetch();

    auto data = std::make_unique<ItemAttributeData>();  // 【修复】使用 make_unique
    data->itemGuid = itemGuid;
    data->itemId = fields[0].Get<uint32>();

    ItemAttributeData::FromCompactString(fields[1].Get<std::string>(), data->baseAttributeIds, data->baseAttributeValues);
    ItemAttributeData::FromCompactString(fields[2].Get<std::string>(), data->additionalAttributeIds, data->additionalAttributeValues);

    UpdateCache(itemGuid, *data);
    return data;  // 【修复】直接返回 unique_ptr，自动转移所有权
}

// 保存物品属性（插入或更新）
bool ItemAttributesDBHelper::SaveItemAttributes(const ItemAttributeData& data)
{
    if (data.baseAttributeIds.empty() && data.additionalAttributeIds.empty())
    {
        // 如果没有任何属性，删除记录
        return ClearItemAttributes(data.itemGuid);
    }

    // 转换为紧凑格式
    std::string baseAttrStr = ItemAttributeData::ToCompactString(data.baseAttributeIds, data.baseAttributeValues);
    std::string additionalAttrStr = ItemAttributeData::ToCompactString(data.additionalAttributeIds, data.additionalAttributeValues);

    // 【数据库错误处理】添加 try-catch 保护，防止数据库异常导致崩溃
    try
    {
        // 避免 REPLACE 的 delete+insert 行为（更易引发锁竞争/死锁），改用 UPSERT
        // 【修复】使用 DirectExecute 同步写入数据库
        // 原因：异步写入时，后续的鉴定系统查询可能还没有写入完成
        // 导致鉴定后需要重启服务器才能看到属性数据
        CharacterDatabase.DirectExecute(
            "INSERT INTO `物品属性_数据` (`物品GUID`, `物品ID`, `基础属性`, `追加属性`) "
            "VALUES ({}, {}, '{}', '{}') "
            "ON DUPLICATE KEY UPDATE "
            "`物品ID` = VALUES(`物品ID`), "
            "`基础属性` = VALUES(`基础属性`), "
            "`追加属性` = VALUES(`追加属性`)",
            data.itemGuid,
            data.itemId,
            baseAttrStr,
            additionalAttrStr
        );

        UpdateCache(data.itemGuid, data);
        return true;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("module.item-attributes", "【数据库错误】SaveItemAttributes 失败: itemGuid={}, 错误: {}",
                  data.itemGuid, ex.what());
        return false;
    }
}

// 添加单个属性到物品（默认添加到追加属性）
bool ItemAttributesDBHelper::AddAttributeToItem(uint64 itemGuid, uint32 itemId, uint32 attributeId, int256 attributeValue)
{
    return AddAdditionalAttributeToItem(itemGuid, itemId, attributeId, attributeValue);
}

// 添加单个基础属性
bool ItemAttributesDBHelper::AddBaseAttributeToItem(uint64 itemGuid, uint32 itemId, uint32 attributeId, int256 attributeValue)
{
    auto existingData = LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存
    ItemAttributeData data;

    if (existingData)
    {
        data = *existingData;
        // 【智能指针修复】移除手动 delete，unique_ptr 自动清理

        // 检查基础属性是否已存在
        for (size_t i = 0; i < data.baseAttributeIds.size(); ++i)
        {
            if (data.baseAttributeIds[i] == attributeId)
            {
                // 属性已存在，更新值
                data.baseAttributeValues[i] = attributeValue;
                return SaveItemAttributes(data);
            }
        }
    }
    else
    {
        data.itemGuid = itemGuid;
        data.itemId = itemId;
    }

    // 添加新基础属性
    data.baseAttributeIds.push_back(attributeId);
    data.baseAttributeValues.push_back(attributeValue);

    return SaveItemAttributes(data);
}

// 添加单个追加属性
bool ItemAttributesDBHelper::AddAdditionalAttributeToItem(uint64 itemGuid, uint32 itemId, uint32 attributeId, int256 attributeValue)
{
    auto existingData = LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存
    ItemAttributeData data;

    if (existingData)
    {
        data = *existingData;
        // 【智能指针修复】移除手动 delete，unique_ptr 自动清理

        // 检查追加属性是否已存在
        for (size_t i = 0; i < data.additionalAttributeIds.size(); ++i)
        {
            if (data.additionalAttributeIds[i] == attributeId)
            {
                // 属性已存在，更新值
                data.additionalAttributeValues[i] = attributeValue;
                return SaveItemAttributes(data);
            }
        }
    }
    else
    {
        data.itemGuid = itemGuid;
        data.itemId = itemId;
    }

    // 添加新追加属性
    data.additionalAttributeIds.push_back(attributeId);
    data.additionalAttributeValues.push_back(attributeValue);

    return SaveItemAttributes(data);
}

// 保存基础属性（替换官方属性）
bool ItemAttributesDBHelper::SaveBaseAttributes(uint64 itemGuid, uint32 itemId, const std::vector<uint32>& attributeIds, const std::vector<int256>& attributeValues)
{
    // 【调试日志】记录基础属性完整替换
    std::string newAttrs = ItemAttributeData::ToCompactString(attributeIds, attributeValues);
    LOG_INFO("module.itemattributes", "【!!!基础属性替换追踪!!!】SaveBaseAttributes被调用: itemGuid={}, itemId={}, 新基础属性='{}'",
              itemGuid, itemId, newAttrs);

    auto existingData = LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存
    ItemAttributeData data;

    if (existingData)
    {
        data = *existingData;

        // 【调试日志】记录被替换的旧基础属性
        std::string oldAttrs = ItemAttributeData::ToCompactString(data.baseAttributeIds, data.baseAttributeValues);
        LOG_INFO("module.itemattributes", "【基础属性替换追踪】被替换的旧基础属性: '{}'", oldAttrs);

        // 【智能指针修复】移除手动 delete，unique_ptr 自动清理
    }
    else
    {
        data.itemGuid = itemGuid;
        data.itemId = itemId;
        LOG_INFO("module.itemattributes", "【基础属性替换追踪】首次创建记录");
    }

    // 替换基础属性
    data.baseAttributeIds = attributeIds;
    data.baseAttributeValues = attributeValues;

    LOG_INFO("module.itemattributes", "【基础属性替换追踪】替换完成，保存到数据库");

    return SaveItemAttributes(data);
}

// 保存追加属性（额外添加）
bool ItemAttributesDBHelper::SaveAdditionalAttributes(uint64 itemGuid, uint32 itemId, const std::vector<uint32>& attributeIds, const std::vector<int256>& attributeValues)
{
    auto existingData = LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存
    ItemAttributeData data;

    if (existingData)
    {
        data = *existingData;
        // 【智能指针修复】移除手动 delete，unique_ptr 自动清理
    }
    else
    {
        data.itemGuid = itemGuid;
        data.itemId = itemId;
    }

    // 替换追加属性
    data.additionalAttributeIds = attributeIds;
    data.additionalAttributeValues = attributeValues;

    return SaveItemAttributes(data);
}

// 从物品移除单个属性（从基础和追加属性中查找）
bool ItemAttributesDBHelper::RemoveAttributeFromItem(uint64 itemGuid, uint32 attributeId)
{
    auto existingData = LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存
    if (!existingData)
        return false;

    ItemAttributeData data = *existingData;
    // 【智能指针修复】移除手动 delete，unique_ptr 自动清理

    bool found = false;

    // 先从基础属性中查找
    for (size_t i = 0; i < data.baseAttributeIds.size(); ++i)
    {
        if (data.baseAttributeIds[i] == attributeId)
        {
            data.baseAttributeIds.erase(data.baseAttributeIds.begin() + i);
            data.baseAttributeValues.erase(data.baseAttributeValues.begin() + i);
            found = true;
            break;
        }
    }

    // 如果没找到，从追加属性中查找
    if (!found)
    {
        for (size_t i = 0; i < data.additionalAttributeIds.size(); ++i)
        {
            if (data.additionalAttributeIds[i] == attributeId)
            {
                data.additionalAttributeIds.erase(data.additionalAttributeIds.begin() + i);
                data.additionalAttributeValues.erase(data.additionalAttributeValues.begin() + i);
                found = true;
                break;
            }
        }
    }

    if (!found)
        return false;

    // 如果没有剩余属性，删除记录
    if (data.baseAttributeIds.empty() && data.additionalAttributeIds.empty())
    {
        return ClearItemAttributes(itemGuid);
    }

    return SaveItemAttributes(data);
}

// 清空物品的所有属性
bool ItemAttributesDBHelper::ClearItemAttributes(uint64 itemGuid)
{
    // 【数据库错误处理】添加 try-catch 保护
    try
    {
        CharacterDatabase.Execute(
            "DELETE FROM `物品属性_数据` WHERE `物品GUID` = {}",
            itemGuid
        );

        LOG_DEBUG("module.itemattributes", "ClearItemAttributes: itemGuid={}", itemGuid);
        EraseCache(itemGuid);
        return true;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("module.item-attributes", "【数据库错误】ClearItemAttributes 失败: itemGuid={}, 错误: {}",
                  itemGuid, ex.what());
        return false;
    }
}

// 检查物品是否有属性
bool ItemAttributesDBHelper::HasAttributes(uint64 itemGuid)
{
    ItemAttributeData cached;
    if (TryGetCachedData(itemGuid, cached))
        return !cached.baseAttributeIds.empty() || !cached.additionalAttributeIds.empty();

    auto data = LoadItemAttributes(itemGuid);  // 【智能指针修复】
    if (!data)
        return false;

    bool hasAttributes = !data->baseAttributeIds.empty() || !data->additionalAttributeIds.empty();
    // 【智能指针修复】移除 delete，自动清理
    return hasAttributes;
}

// 获取物品属性数量（基础+追加）
uint32 ItemAttributesDBHelper::GetAttributeCount(uint64 itemGuid)
{
    auto data = LoadItemAttributes(itemGuid);  // 【智能指针修复】
    if (!data)
        return 0;

    uint32 count = static_cast<uint32>(data->baseAttributeIds.size() + data->additionalAttributeIds.size());
    // 【智能指针修复】移除 delete，自动清理
    return count;
}

// 格式化属性数据为响应字符串
std::string ItemAttributesDBHelper::FormatAttributesForClient(const ItemAttributeData& data)
{
    std::string result;
    bool first = true;

    // 【关键修复】检查 sItemAttributesLoader 是否已初始化
    if (!sItemAttributesLoader)
    {
        LOG_ERROR("module.item-attributes", "【致命错误】FormatAttributesForClient 调用时 sItemAttributesLoader 未初始化！");
        return "";
    }

    // 添加基础属性
    for (size_t i = 0; i < data.baseAttributeIds.size() && i < data.baseAttributeValues.size(); ++i)
    {
        ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(data.baseAttributeIds[i]);
        if (attrTemplate)
        {
            if (!first)
                result += ",";
            result += attrTemplate->clientDisplay + "+" + Acore::ToString(data.baseAttributeValues[i]);
            first = false;
        }
    }

    // 添加追加属性
    for (size_t i = 0; i < data.additionalAttributeIds.size() && i < data.additionalAttributeValues.size(); ++i)
    {
        ItemAttributeTemplate const* attrTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(data.additionalAttributeIds[i]);
        if (attrTemplate)
        {
            if (!first)
                result += ",";
            result += attrTemplate->clientDisplay + "+" + Acore::ToString(data.additionalAttributeValues[i]);
            first = false;
        }
    }

    return result;
}

bool ItemAttributesDBHelper::TryGetCachedData(uint64 itemGuid, ItemAttributeData& data)
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    auto itr = _cache.find(itemGuid);
    if (itr == _cache.end())
        return false;

    data = itr->second;
    return true;
}

void ItemAttributesDBHelper::UpdateCache(uint64 itemGuid, const ItemAttributeData& data)
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    _cache[itemGuid] = data;
}

void ItemAttributesDBHelper::EraseCache(uint64 itemGuid)
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    _cache.erase(itemGuid);
}

void ItemAttributesDBHelper::FlushCache()
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    _cache.clear();
}

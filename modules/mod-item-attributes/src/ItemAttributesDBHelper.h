#ifndef ITEM_ATTRIBUTES_DB_HELPER_H
#define ITEM_ATTRIBUTES_DB_HELPER_H

#include "DatabaseEnv.h"
#include "Define.h"
#include "Item.h"
#include "Player.h"
#include "StringConvert.h"
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <memory>  // 【智能指针修复】添加 unique_ptr 支持
#include <type_traits>

// 物品属性数据库辅助类
class ItemAttributesDBHelper
{
public:
    // 物品属性数据结构
    struct ItemAttributeData
    {
        uint64 itemGuid;
        uint32 itemId;
        std::vector<uint32> baseAttributeIds;      // 基础属性ID列表（替换官方属性）
        std::vector<int256> baseAttributeValues;   // 基础属性值列表
        std::vector<uint32> additionalAttributeIds; // 追加属性ID列表（额外添加）
        std::vector<int256> additionalAttributeValues; // 追加属性值列表
        
        // 辅助方法：将属性转换为紧凑字符串格式 "id value,id value,..."
        static std::string ToCompactString(const std::vector<uint32>& ids, const std::vector<int256>& values);
        // 辅助方法：从紧凑字符串解析属性 "id value,id value,..."
        static void FromCompactString(const std::string& str, std::vector<uint32>& ids, std::vector<int256>& values);
    };

    // 【智能指针修复】读取物品的所有属性（返回智能指针，自动管理内存）
    static std::unique_ptr<ItemAttributeData> LoadItemAttributes(uint64 itemGuid);

    // 保存物品属性（插入或更新）
    static bool SaveItemAttributes(const ItemAttributeData& data);

    // 保存基础属性（替换官方属性）
    static bool SaveBaseAttributes(uint64 itemGuid, uint32 itemId, const std::vector<uint32>& attributeIds, const std::vector<int256>& attributeValues);

    // 保存追加属性（额外添加）
    static bool SaveAdditionalAttributes(uint64 itemGuid, uint32 itemId, const std::vector<uint32>& attributeIds, const std::vector<int256>& attributeValues);

    // 添加单个属性到物品
    static bool AddAttributeToItem(uint64 itemGuid, uint32 itemId, uint32 attributeId, int256 attributeValue);
    
    // 添加单个基础属性
    static bool AddBaseAttributeToItem(uint64 itemGuid, uint32 itemId, uint32 attributeId, int256 attributeValue);
    
    // 添加单个追加属性
    static bool AddAdditionalAttributeToItem(uint64 itemGuid, uint32 itemId, uint32 attributeId, int256 attributeValue);

    // 从物品移除单个属性
    static bool RemoveAttributeFromItem(uint64 itemGuid, uint32 attributeId);

    // 清空物品的所有属性
    static bool ClearItemAttributes(uint64 itemGuid);

    // 检查物品是否有属性
    static bool HasAttributes(uint64 itemGuid);

    // 获取物品属性数量
    static uint32 GetAttributeCount(uint64 itemGuid);

    // 格式化属性数据为响应字符串
    static std::string FormatAttributesForClient(const ItemAttributeData& data);
    static void FlushCache();

private:
    static bool TryGetCachedData(uint64 itemGuid, ItemAttributeData& data);
    static void UpdateCache(uint64 itemGuid, const ItemAttributeData& data);
    static void EraseCache(uint64 itemGuid);

    static std::mutex _cacheMutex;
    static std::unordered_map<uint64, ItemAttributeData> _cache;

    // 将vector转换为逗号分隔的字符串
    template<typename T>
    static std::string VectorToString(const std::vector<T>& vec)
    {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            if (i > 0)
                result += ",";
            result += Acore::ToString(vec[i]);
        }
        return result;
    }

    // 将逗号分隔的字符串转换为vector
    template<typename T>
    static std::vector<T> StringToVector(const std::string& str)
    {
        std::vector<T> result;
        std::stringstream ss(str);
        std::string token;
        
        while (std::getline(ss, token, ','))
        {
            if (!token.empty())
            {
                if constexpr (std::is_same_v<T, uint32>)
                    result.push_back(static_cast<uint32>(std::stoul(token)));
                else if constexpr (std::is_same_v<T, int256>)
                {
                    if (Optional<int256> value = Acore::StringTo<int256>(token))
                        result.push_back(*value);
                }
            }
        }
        
        return result;
    }
};

#endif // ITEM_ATTRIBUTES_DB_HELPER_H

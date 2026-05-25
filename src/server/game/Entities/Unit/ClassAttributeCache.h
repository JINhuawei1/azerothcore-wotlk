/*
 * ClassAttributeCache - 缓存 `_属性调整_职业` 表数据，避免属性计算时的同步数据库查询。
 * 服务器启动时加载一次，运行时通过 .reload 命令可热重载。
 */

#ifndef CLASS_ATTRIBUTE_CACHE_H
#define CLASS_ATTRIBUTE_CACHE_H

#include "Define.h"
#include <array>
#include <mutex>

// MAX_CLASSES = 12 (index 0 unused, 1-11 for classes)
constexpr uint8 CLASS_ATTR_MAX_CLASSES = 12;

struct ClassAttributeData
{
    // 属性上限
    uint128 力量上限 = 0;
    uint128 敏捷上限 = 0;
    uint128 耐力上限 = 0;
    uint128 智力上限 = 0;
    uint128 精神上限 = 0;

    // 生命/法力
    uint128 血量上限 = 0;
    uint128 法力上限 = 0;

    // 护甲
    uint128 护甲上限 = 0;

    // 伤害上限
    uint128 主手伤害上限 = 0;
    uint128 副手伤害上限 = 0;
    uint128 远程伤害上限 = 0;

    // 转换率
    float 耐力转生命转换率 = 0.0f;  // 百分比值，0表示未配置
    float 智力转法力转换率 = 0.0f;
    float 力量转攻强转换率 = 0.0f;
    float 敏捷转攻强转换率 = 0.0f;

    // 攻强
    float 攻强倍率 = 0.0f;
    uint128 攻强上限 = 0;
    float 远程攻强倍率 = 0.0f;
    uint128 远程攻强上限 = 0;

    // 法强/治疗
    float 法强倍率 = 0.0f;
    float 治疗倍率 = 0.0f;

    // 几率上限
    float 暴击几率上限 = 0.0f;
    float 格挡几率上限 = 0.0f;
    float 招架几率上限 = 0.0f;
    float 闪避几率上限 = 0.0f;

    // 等级转换率
    float 格挡等级转换率 = 0.0f;
    float 招架等级转换率 = 0.0f;
    float 命中等级转换率 = 0.0f;

    bool loaded = false;  // 是否有有效数据
};

class ClassAttributeCache
{
public:
    static ClassAttributeCache* instance();

    void Load();

    // 获取指定职业的属性数据（已合并通用配置 class_=0）
    ClassAttributeData const& GetData(uint8 classId) const;

private:
    ClassAttributeCache() = default;

    // index 0 = 通用配置, 1-11 = 各职业最终合并后的数据
    std::array<ClassAttributeData, CLASS_ATTR_MAX_CLASSES> _data{};
    ClassAttributeData _empty{};  // 返回空数据用
};

#define sClassAttributeCache ClassAttributeCache::instance()

#endif // CLASS_ATTRIBUTE_CACHE_H

/*
 * ClassAttributeCache - 缓存 `_属性调整_职业` 表数据
 */

#include "ClassAttributeCache.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"
#include "Timer.h"
#include "Util.h"

namespace
{
uint256 GetUInt256Compatible(Field& field)
{
    switch (field.GetType())
    {
        case DatabaseFieldTypes::Float:
            return Acore::Number::ToUInt256Saturated(static_cast<long double>(field.Get<float>()));
        case DatabaseFieldTypes::Double:
            return Acore::Number::ToUInt256Saturated(static_cast<long double>(field.Get<double>()));
        default:
            return field.GetUInt256();
    }
}
}

ClassAttributeCache* ClassAttributeCache::instance()
{
    static ClassAttributeCache inst;
    return &inst;
}

void ClassAttributeCache::Load()
{
    uint32 oldMSTime = getMSTime();

    // 清空旧数据
    for (auto& d : _data)
        d = ClassAttributeData{};

    QueryResult result = WorldDatabase.Query(
        "SELECT `class_`, `力量上限`, `敏捷上限`, `耐力上限`, `智力上限`, `精神上限`,"
        " `血量上限`, `法力上限`, `护甲上限`,"
        " `主手伤害上限`, `副手伤害上限`, `远程伤害上限`,"
        " `耐力转生命转换率`, `智力转法力转换率`, `力量转攻强转换率`, `敏捷转攻强转换率`,"
        " `攻强倍率`, `攻强上限`, `远程攻强倍率`, `远程攻强上限`,"
        " `法强倍率`, `治疗倍率`,"
        " `暴击几率上限`, `格挡几率上限`, `招架几率上限`, `闪避几率上限`,"
        " `格挡等级转换率`, `招架等级转换率`, `命中等级转换率`"
        " FROM `_属性调整_职业` WHERE `启用` = 1 ORDER BY `class_` ASC");

    if (!result)
    {
        LOG_INFO("server.loading", ">> 加载 _属性调整_职业 缓存: 表为空或不存在，跳过。耗时 {} ms", GetMSTimeDiffToNow(oldMSTime));
        return;
    }

    // 先加载 class_=0 的通用配置，再加载各职业配置覆盖
    // 由于 ORDER BY class_ ASC，class_=0 的行会先被处理
    uint32 count = 0;
    do
    {
        Field* f = result->Fetch();
        uint8 classId = f[0].Get<uint8>();

        if (classId >= CLASS_ATTR_MAX_CLASSES)
            continue;

        ClassAttributeData& d = _data[classId];
        d.力量上限           = f[1].GetUInt256();
        d.敏捷上限           = f[2].GetUInt256();
        d.耐力上限           = f[3].GetUInt256();
        d.智力上限           = f[4].GetUInt256();
        d.精神上限           = f[5].GetUInt256();
        d.血量上限           = f[6].GetUInt256();
        d.法力上限           = f[7].GetUInt256();
        d.护甲上限           = f[8].GetUInt256();
        d.主手伤害上限       = GetUInt256Compatible(f[9]);
        d.副手伤害上限       = GetUInt256Compatible(f[10]);
        d.远程伤害上限       = GetUInt256Compatible(f[11]);
        d.耐力转生命转换率   = f[12].Get<float>();
        d.智力转法力转换率   = f[13].Get<float>();
        d.力量转攻强转换率   = f[14].Get<float>();
        d.敏捷转攻强转换率   = f[15].Get<float>();
        d.攻强倍率           = f[16].Get<float>();
        d.攻强上限           = f[17].GetUInt256();
        d.远程攻强倍率       = f[18].Get<float>();
        d.远程攻强上限       = f[19].GetUInt256();
        d.法强倍率           = f[20].Get<float>();
        d.治疗倍率           = f[21].Get<float>();
        d.暴击几率上限       = f[22].Get<float>();
        d.格挡几率上限       = f[23].Get<float>();
        d.招架几率上限       = f[24].Get<float>();
        d.闪避几率上限       = f[25].Get<float>();
        d.格挡等级转换率     = f[26].Get<float>();
        d.招架等级转换率     = f[27].Get<float>();
        d.命中等级转换率     = f[28].Get<float>();
        d.loaded = true;
        ++count;
    } while (result->NextRow());

    // 对每个职业，如果没有专属配置，则使用通用配置(class_=0)
    // 如果有专属配置，则已经直接使用专属配置（SQL中 ORDER BY class_ DESC LIMIT 1 的语义）
    // 这里的逻辑：原始SQL是 WHERE (class_ = X OR class_ = 0) ORDER BY class_ DESC LIMIT 1
    // 即：优先取 class_=X 的行，没有则取 class_=0 的行
    // 所以我们只需要在 GetData 中实现这个 fallback 逻辑

    LOG_INFO("server.loading", ">> 加载 _属性调整_职业 缓存: {} 条记录，耗时 {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

ClassAttributeData const& ClassAttributeCache::GetData(uint8 classId) const
{
    if (classId == 0 || classId >= CLASS_ATTR_MAX_CLASSES)
        return _data[0].loaded ? _data[0] : _empty;

    // 优先返回职业专属配置，没有则返回通用配置
    if (_data[classId].loaded)
        return _data[classId];
    if (_data[0].loaded)
        return _data[0];
    return _empty;
}

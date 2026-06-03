#include "ItemAttributesEffects.h"
#include "Logging/Log.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "ItemAttributesDBHelper.h"
#include "StringConvert.h"
#include "Util.h"
#include <sstream>
#include <chrono>

namespace
{
float ToStatFloat(int128 const& value)
{
    return Acore::Number::ToFloat(value);
}

int128 ToRatingValue(int128 const& value)
{
    return value;
}

int32 ToLegacyInt32(int128 const& value)
{
    return Acore::Number::ToInt32Saturated(value);
}

uint32 ToSettingUInt32(int128 const& value)
{
    if (value <= 0)
        return 0;

    return Acore::Number::ToUInt32Saturated(static_cast<uint128>(value));
}
}

// 【线程安全】Meyer's Singleton - C++11保证静态局部变量初始化的线程安全性
// 编译器会自动添加同步机制，确保多线程并发调用时只初始化一次
ItemAttributesEffects* ItemAttributesEffects::instance()
{
    static ItemAttributesEffects instance;
    return &instance;
}

// 【根本性修复】安全获取实例方法
ItemAttributesEffects* ItemAttributesEffects::SafeInstance()
{
    ItemAttributesEffects* inst = instance();
    return (inst && inst->IsInitialized()) ? inst : nullptr;
}

void ItemAttributesEffects::Initialize()
{
    // 防止重复初始化
    if (_isInitialized)
    {
        LOG_WARN("module.itemattributes", "ItemAttributesEffects::Initialize() 被重复调用，跳过初始化");
        return;
    }

    // 注册基础属性效果处理器

    // 力量属性 (类型 4)
    RegisterAttributeEffectHandler(4,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 【关键修复】安全获取物品GUID用于日志
            uint64 itemGuid = 0;
            if (item) {
                ObjectGuid guidObj = item->GetGUID();
                if (!guidObj.IsEmpty()) itemGuid = guidObj.GetCounter();
            }
            LOG_DEBUG("module.itemattributes.effects", "【应用】力量 +{} (玩家:{}, 物品GUID:{})",
                Acore::ToString(value), player->GetName(), itemGuid);
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 【关键修复】安全获取物品GUID用于日志
            uint64 itemGuid = 0;
            if (item) {
                ObjectGuid guidObj = item->GetGUID();
                if (!guidObj.IsEmpty()) itemGuid = guidObj.GetCounter();
            }
            LOG_DEBUG("module.itemattributes.effects", "【移除】力量 -{} (玩家:{}, 物品GUID:{})",
                Acore::ToString(value), player->GetName(), itemGuid);
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "力量 +" << value;
            return ss.str();
        }
    );

    // 敏捷属性 (类型 3)
    RegisterAttributeEffectHandler(3,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "敏捷 +" << value;
            return ss.str();
        }
    );

    // 耐力属性 (类型 7)
    RegisterAttributeEffectHandler(7,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "耐力 +" << value;
            return ss.str();
        }
    );

    // 智力属性 (类型 5)
    RegisterAttributeEffectHandler(5,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "智力 +" << value;
            return ss.str();
        }
    );

    // 精神属性 (类型 6)
    RegisterAttributeEffectHandler(6,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "精神 +" << value;
            return ss.str();
        }
    );

    // 生命值属性 (类型 1)
    RegisterAttributeEffectHandler(1,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "生命值 +" << value;
            return ss.str();
        }
    );

    // 法力值属性 (类型 0)
    RegisterAttributeEffectHandler(0,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "法力值 +" << value;
            return ss.str();
        }
    );

    // 攻击强度属性 (类型 38)
    RegisterAttributeEffectHandler(38,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "攻击强度 +" << value;
            return ss.str();
        }
    );

    // 法术强度属性 (类型 45)
    RegisterAttributeEffectHandler(45,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplySpellPowerBonus(value, true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplySpellPowerBonus(value, false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "法术强度 +" << value;
            return ss.str();
        }
    );

    // 暴击等级属性 (类型 32)
    RegisterAttributeEffectHandler(32,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_CRIT_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_CRIT_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "暴击等级 +" << value;
            return ss.str();
        }
    );

    // 命中等级属性 (类型 31)
    RegisterAttributeEffectHandler(31,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_HIT_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HIT_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HIT_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_HIT_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HIT_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HIT_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "命中等级 +" << value;
            return ss.str();
        }
    );

    // 急速等级属性 (类型 36)
    RegisterAttributeEffectHandler(36,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_HASTE_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HASTE_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HASTE_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_HASTE_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HASTE_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HASTE_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "急速等级 +" << value;
            return ss.str();
        }
    );

    // 护甲穿透等级属性 (类型 44)
    RegisterAttributeEffectHandler(44,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "护甲穿透等级 +" << value;
            return ss.str();
        }
    );

    // 法术穿透属性 (类型 47)
    RegisterAttributeEffectHandler(47,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplySpellPenetrationBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplySpellPenetrationBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "法术穿透 +" << value;
            return ss.str();
        }
    );

    // 防御等级属性 (类型 12)
    RegisterAttributeEffectHandler(12,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_DEFENSE_SKILL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_DEFENSE_SKILL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "防御等级 +" << value;
            return ss.str();
        }
    );

    // 躲闪等级属性 (类型 13)
    RegisterAttributeEffectHandler(13,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_DODGE, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_DODGE, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "躲闪等级 +" << value;
            return ss.str();
        }
    );

    // 招架等级属性 (类型 14)
    RegisterAttributeEffectHandler(14,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_PARRY, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_PARRY, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "招架等级 +" << value;
            return ss.str();
        }
    );

    // 格挡等级属性 (类型 15)
    RegisterAttributeEffectHandler(15,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_BLOCK, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_BLOCK, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "格挡等级 +" << value;
            return ss.str();
        }
    );

    // 韧性等级属性 (类型 35)
    RegisterAttributeEffectHandler(35,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "韧性等级 +" << value;
            return ss.str();
        }
    );

    // 精准等级属性 (类型 37)
    RegisterAttributeEffectHandler(37,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_EXPERTISE, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyRatingMod(CR_EXPERTISE, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "精准等级 +" << value;
            return ss.str();
        }
    );

    // 远程攻击强度属性 (类型 39)
    RegisterAttributeEffectHandler(39,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "远程攻击强度 +" << value;
            return ss.str();
        }
    );

    // 法力恢复属性 (类型 43)
    RegisterAttributeEffectHandler(43,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyManaRegenBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyManaRegenBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "法力恢复 +" << value;
            return ss.str();
        }
    );

    // 生命恢复属性 (类型 46)
    RegisterAttributeEffectHandler(46,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyHealthRegenBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            player->ApplyHealthRegenBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "生命恢复 +" << value;
            return ss.str();
        }
    );

    // 格挡值属性 (类型 48)
    // 注意：AzerothCore 中格挡值是盾牌的固有属性，无法通过附加属性直接修改
    // 这里仅提供描述支持，不实际应用效果
    RegisterAttributeEffectHandler(48,
        // 应用效果（占位，不实际应用）
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 格挡值是装备固有属性，在AzerothCore中无法通过附加属性系统修改
            // 这里仅记录日志
            LOG_DEBUG("module.itemattributes", "格挡值属性 +{} 已添加到物品（仅显示，不影响实际游戏）", Acore::ToString(value));
        },
        // 移除效果（占位，不实际移除）
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 格挡值是装备固有属性，无需移除
            LOG_DEBUG("module.itemattributes", "格挡值属性 +{} 已移除（仅显示）", Acore::ToString(value));
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "格挡值 +" << value << " (显示)";
            return ss.str();
        }
    );

    // ============================================================
    // 自定义属性 (400+)
    // ============================================================

    // 经验获取加成属性 (类型 400)
    RegisterAttributeEffectHandler(400,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 这里需要自定义实现经验获取加成逻辑
            // 示例：记录玩家的经验获取加成
            // 使用玩家设置系统存储经验加成
            player->UpdatePlayerSetting("item_attributes", 400, ToSettingUInt32(value));
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 移除经验获取加成
            player->UpdatePlayerSetting("item_attributes", 400, 0);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "经验获取 +" << value << "%";
            return ss.str();
        }
    );

    // 掉落加成属性 (类型 401)
    RegisterAttributeEffectHandler(401,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 这里需要自定义实现掉落加成逻辑
            // 示例：记录玩家的掉落加成
            // 使用玩家设置系统存储掉落加成
            player->UpdatePlayerSetting("item_attributes", 401, ToSettingUInt32(value));
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            // 移除掉落加成
            player->UpdatePlayerSetting("item_attributes", 401, 0);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, int128 value) {
            std::ostringstream ss;
            ss << "掉落几率 +" << value << "%";
            return ss.str();
        }
    );

    // 【根本性修复】标记为已初始化
    _isInitialized = true;
}

void ItemAttributesEffects::RegisterAttributeEffectHandler(uint32 attributeType, AttributeEffectHandler handler, AttributeEffectRemover remover, AttributeDescriptionGenerator descGenerator)
{
    _attributeEffectHandlers[attributeType] = handler;
    _attributeEffectRemovers[attributeType] = remover;
    _attributeDescriptionGenerators[attributeType] = descGenerator;
}

// 【审计修复】按玩家跟踪批量更新状态
bool ItemAttributesEffects::IsBatchUpdateInProgress(uint64 playerGuid)
{
    std::lock_guard<std::mutex> lock(_batchUpdateMutex);
    return _batchUpdatePlayers.find(playerGuid) != _batchUpdatePlayers.end();
}

void ItemAttributesEffects::SetBatchUpdateInProgress(uint64 playerGuid, bool inProgress)
{
    std::lock_guard<std::mutex> lock(_batchUpdateMutex);
    if (inProgress)
        _batchUpdatePlayers.insert(playerGuid);
    else
        _batchUpdatePlayers.erase(playerGuid);
}

void ItemAttributesEffects::RequestDeferredStatsUpdate(Player* player)
{
    if (!player)
        return;

    // 登录加载阶段尽量不刷新，交给登录流程/其他系统统一刷新一次
    if (player->isBeingLoaded())
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    {
        std::lock_guard<std::mutex> lock(_deferredUpdateMutex);
        if (!_deferredUpdatePlayers.insert(playerGuid).second)
            return; // 已经安排过更新
    }

    player->m_Events.AddEventAtOffset([this, player, playerGuid]()
    {
        {
            std::lock_guard<std::mutex> lock(_deferredUpdateMutex);
            _deferredUpdatePlayers.erase(playerGuid);
        }

        if (!player || !player->IsInWorld() || player->isBeingLoaded())
            return;

        if (!player->CanModifyStats())
            return;

        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();
    }, std::chrono::milliseconds(1));
}

void ItemAttributesEffects::ApplyItemAttributeEffects(Player* player, Item* item)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.itemattributes", "【致命错误】ApplyItemAttributeEffects 被调用，但系统尚未初始化！");
        LOG_ERROR("module.itemattributes", "  → 这通常表示模块初始化顺序错误");
        LOG_ERROR("module.itemattributes", "  → Player: {}, Item: {}",
                 player ? player->GetName() : "nullptr",
                 item ? item->GetEntry() : 0);
        return;
    }

    if (!player || !item)
        return;

    std::vector<uint32> attributes;
    std::vector<int128> values;

    // 【关键修复】添加 sItemAttributesLoader 的空指针检查
    if (!sItemAttributesLoader)
    {
        LOG_ERROR("module.itemattributes", "【应用属性】错误 - sItemAttributesLoader 未初始化！");
        return;
    }

    if (!sItemAttributesLoader->GetItemAttributesWithValues(item, attributes, values))
    {
        return;
    }

    // 确保属性和值的数量匹配
    if (attributes.size() != values.size())
    {
        // 【关键修复】安全获取物品GUID用于日志
        ObjectGuid itemGuidObj = item->GetGUID();
        uint64 itemGuid = itemGuidObj.IsEmpty() ? 0 : itemGuidObj.GetCounter();
        LOG_ERROR("module.itemattributes", "【应用属性】错误 - 物品 GUID {} 的属性数量({})和值数量({})不匹配！",
            itemGuid, attributes.size(), values.size());
        return;
    }

    if (attributes.empty())
        return;

    // 【审计修复】使用按玩家跟踪的批量更新标志，避免跨玩家并发问题
    uint64 playerGuid = player->GetGUID().GetCounter();
    bool batchInProgress = IsBatchUpdateInProgress(playerGuid);

    // 【性能优化】批量应用期间由上层统一刷新；普通情况下合并到一次延迟刷新
    bool canModifyBefore = player->CanModifyStats();
    bool needsDeferUpdate = !batchInProgress;
    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(false);

    // 应用每个属性效果
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeType = attributes[i];
        int128 value = values[i];

        auto handlerItr = _attributeEffectHandlers.find(attributeType);
        if (handlerItr != _attributeEffectHandlers.end())
        {
            if (handlerItr->second)
            {
                handlerItr->second(player, item, nullptr, value);
            }
            else
            {
                LOG_ERROR("module.itemattributes", "属性类型 {} 的处理器为空指针！", attributeType);
            }
        }
        else
        {
            LOG_WARN("module.itemattributes", "未找到属性类型 {} 的处理器", attributeType);
        }
    }

    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(true);

    if (needsDeferUpdate)
        RequestDeferredStatsUpdate(player);
}

void ItemAttributesEffects::RemoveItemAttributeEffects(Player* player, Item* item)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.itemattributes", "【致命错误】RemoveItemAttributeEffects 被调用，但系统尚未初始化！");
        LOG_ERROR("module.itemattributes", "  → 这通常表示模块初始化顺序错误");
        return;
    }

    if (!player || !item)
        return;

    std::vector<uint32> attributes;
    std::vector<int128> values;

    // 【关键修复】添加 sItemAttributesLoader 的空指针检查
    if (!sItemAttributesLoader)
    {
        LOG_ERROR("module.itemattributes", "【移除属性】错误 - sItemAttributesLoader 未初始化！");
        return;
    }

    if (!sItemAttributesLoader->GetItemAttributesWithValues(item, attributes, values))
    {
        return;
    }

    // 确保属性和值的数量匹配
    if (attributes.size() != values.size())
    {
        // 【关键修复】安全获取物品GUID用于日志
        ObjectGuid itemGuidObj = item->GetGUID();
        uint64 itemGuid = itemGuidObj.IsEmpty() ? 0 : itemGuidObj.GetCounter();
        LOG_ERROR("module.itemattributes", "【移除属性】错误 - 物品 GUID {} 的属性数量({})和值数量({})不匹配！",
            itemGuid, attributes.size(), values.size());
        return;
    }

    if (attributes.empty())
        return;

    // 【审计修复】使用按玩家跟踪的批量更新标志，避免跨玩家并发问题
    uint64 playerGuid = player->GetGUID().GetCounter();
    bool batchInProgress = IsBatchUpdateInProgress(playerGuid);

    bool canModifyBefore = player->CanModifyStats();
    bool needsDeferUpdate = !batchInProgress;
    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(false);

    // 移除每个属性效果
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeType = attributes[i];
        int128 value = values[i];

        auto removerItr = _attributeEffectRemovers.find(attributeType);
        if (removerItr != _attributeEffectRemovers.end())
        {
            if (removerItr->second)
            {
                removerItr->second(player, item, nullptr, value);
            }
            else
            {
                LOG_ERROR("module.itemattributes", "属性类型 {} 的移除器为空指针！", attributeType);
            }
        }
        else
        {
            LOG_WARN("module.itemattributes", "未找到属性类型 {} 的移除器", attributeType);
        }
    }

    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(true);

    if (needsDeferUpdate)
        RequestDeferredStatsUpdate(player);
}

void ItemAttributesEffects::UpdateItemAttributeEffects(Player* player)
{
    if (!player)
        return;

    // 【审计修复】使用按玩家跟踪的批量更新标志，避免跨玩家并发问题
    uint64 playerGuid = player->GetGUID().GetCounter();

    // 【性能优化-批量更新】设置批量更新标志，防止每个装备都触发UpdateAllStats
    SetBatchUpdateInProgress(playerGuid, true);

    // 【性能优化-批量更新】禁用自动属性更新，处理完所有装备后统一更新一次
    player->SetCanModifyStats(false);

    // 移除所有物品属性效果
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            RemoveItemAttributeEffects(player, item);
        }
    }

    // 重新应用所有物品属性效果
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            ApplyItemAttributeEffects(player, item);
        }
    }

    // 【性能优化-批量更新】恢复批量更新标志
    SetBatchUpdateInProgress(playerGuid, false);

    // 【性能优化-批量更新】重新启用属性更新并统一计算一次
    // 这样所有装备的属性变化只触发一次UpdateStats
    player->SetCanModifyStats(true);
    player->UpdateAllStats();
}

void ItemAttributesEffects::RemoveItemAttributeEffectsByGuid(Player* player, uint64 itemGuid)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.itemattributes", "【致命错误】RemoveItemAttributeEffectsByGuid 被调用，但系统尚未初始化！");
        return;
    }

    if (!player || itemGuid == 0)
        return;

    // 使用DBHelper读取属性
    auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存

    if (!data)
        return;

    // 合并基础属性和追加属性
    std::vector<uint32> attributes;
    std::vector<int128> values;

    attributes.insert(attributes.end(), data->baseAttributeIds.begin(), data->baseAttributeIds.end());
    attributes.insert(attributes.end(), data->additionalAttributeIds.begin(), data->additionalAttributeIds.end());

    values.insert(values.end(), data->baseAttributeValues.begin(), data->baseAttributeValues.end());
    values.insert(values.end(), data->additionalAttributeValues.begin(), data->additionalAttributeValues.end());

    // 【智能指针修复】移除手动 delete，unique_ptr 自动清理

    if (attributes.empty())
        return;

    // 确保属性和值的数量匹配
    if (attributes.size() != values.size())
    {
        LOG_ERROR("module.itemattributes", "【根据GUID移除属性】错误 - 物品 GUID {} 的属性数量({})和值数量({})不匹配！", 
            itemGuid, attributes.size(), values.size());
        return;
    }

    bool canModifyBefore = player->CanModifyStats();
    if (canModifyBefore)
        player->SetCanModifyStats(false);

    // 移除每个属性效果
    // 【重要】数据库中保存的是属性类型，不是属性模板ID
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeId = attributes[i];
        int128 value = values[i];

        // 使用属性类型查询属性模板（因为数据库中保存的是属性类型）
        ItemAttributeTemplate const* attributeTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(attributeId);
        if (!attributeTemplate)
        {
            LOG_WARN("module.itemattributes", "未找到属性类型对应的属性模板 类型ID: {}", attributeId);
            continue;
        }

        // 查找属性类型对应的效果移除器
        auto removerItr = _attributeEffectRemovers.find(attributeTemplate->attributeType);
        if (removerItr != _attributeEffectRemovers.end())
        {
            // 注意：这里我们没有Item对象，传nullptr
            // 移除器不应该依赖Item对象，只使用attributeTemplate和value
            removerItr->second(player, nullptr, attributeTemplate, value);
        }
        else
        {
            LOG_WARN("module.itemattributes", "未找到属性类型 {} 的移除器", attributeTemplate->attributeType);
        }
    }

    if (canModifyBefore)
        player->SetCanModifyStats(true);

    // 根据GUID移除时同样请求一次合并刷新，避免客户端属性不同步
    RequestDeferredStatsUpdate(player);
}

std::string ItemAttributesEffects::GetAttributeDescription(Item* item, uint32 attributeId)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.itemattributes", "【致命错误】GetAttributeDescription 被调用，但系统尚未初始化！");
        return "【未初始化】";
    }

    if (!item)
        return "";

    // 【重要】attributeId实际上是属性类型，不是模板ID
    ItemAttributeTemplate const* attributeTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(attributeId);
    if (!attributeTemplate)
        return "";

    // 【审计修复】从数据库读取实际保存的值，而不是重新计算随机值
    int128 value = 0;
    uint64 itemGuid = item->GetGUID().GetCounter();
    auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
    if (data)
    {
        // 在基础属性中查找
        for (size_t i = 0; i < data->baseAttributeIds.size(); ++i)
        {
            if (data->baseAttributeIds[i] == attributeId)
            {
                value = data->baseAttributeValues[i];
                break;
            }
        }
        // 如果没找到，在追加属性中查找
        if (value == 0)
        {
            for (size_t i = 0; i < data->additionalAttributeIds.size(); ++i)
            {
                if (data->additionalAttributeIds[i] == attributeId)
                {
                    value = data->additionalAttributeValues[i];
                    break;
                }
            }
        }
    }

    auto generatorItr = _attributeDescriptionGenerators.find(attributeTemplate->attributeType);
    if (generatorItr != _attributeDescriptionGenerators.end())
    {
        return generatorItr->second(item, attributeTemplate, value);
    }

    // 默认描述
    std::ostringstream ss;
    ss << attributeTemplate->clientDisplay << ": " << value;
    return ss.str();
}

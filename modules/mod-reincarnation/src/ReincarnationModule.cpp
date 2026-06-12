/*
 * 转身系统 - 模块入口
 *
 * 实现方式：
 * 1. 属性加成 - 通过 OnPlayerAfterUpdateStat 钩子在计算属性时应用百分比加成
 * 2. 天赋点加成 - 通过 OnCalculateTalentsPoints 钩子增加天赋点
 * 3. 生命/法力加成 - 通过 OnPlayerAfterUpdateMaxHealth/Power 钩子增加
 */

#include "Reincarnation.h"
#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Log.h"
#include "World.h"
#include "Player.h"
#include "Unit.h"
#include "Util.h"
#include "Chat.h"
#include <limits>

namespace
{
// 模块开关缓存：转身系统的属性钩子（OnPlayerAfterUpdateStat 等 10 个）在每次
// UpdateAllStats 期间被高频调用，不能每次都做 GetOption 的字符串查找。
// 启动与配置重载时刷新。
bool sReincarnationEnabled = true;

bool IsReincarnationEnabled()
{
    return sReincarnationEnabled;
}

void RefreshReincarnationEnabled()
{
    sReincarnationEnabled = sConfigMgr->GetOption<bool>("Reincarnation.Enable", true);
}

int128 ScaleRatingForReincarnation(int128 const& amount, float bonusPercent)
{
    long double scaled = Acore::Number::ToLongDouble(amount) * (1.0L + static_cast<long double>(bonusPercent) / 100.0L);
    if (scaled <= 0.0L)
        return 0;

    return Acore::Number::ToInt128Saturated(scaled);
}

int128 ScaleSpellPowerForReincarnation(int128 const& amount, float bonusPercent)
{
    long double scaled = Acore::Number::ToLongDouble(amount) * (1.0L + static_cast<long double>(bonusPercent) / 100.0L);
    if (scaled <= 0.0L)
        return 0;

    return Acore::Number::ToInt128Saturated(scaled);
}
}

// 辅助函数：保留空实现以避免链接错误（这些函数在头文件中声明，可能被其他地方调用）
void ApplyReincarnationStats(Player* player, float bonusPercent)
{
    // 不再需要手动应用，通过钩子自动完成
    if (player)
        player->UpdateAllStats();
}

void RemoveReincarnationStats(Player* player, float bonusPercent)
{
    // 不再需要手动移除，通过钩子自动完成
    if (player)
        player->UpdateAllStats();
}

void UpdateReincarnationStats(Player* player, float oldBonusPercent, float newBonusPercent)
{
    // 不再需要手动更新，通过钩子自动完成
    if (player)
        player->UpdateAllStats();
}

// 世界脚本类，用于延迟加载转身系统数据
class ReincarnationWorldScript : public WorldScript
{
public:
    ReincarnationWorldScript() : WorldScript("ReincarnationWorldScript"), _loaded(false), _updateTimer(0)
    {
    }

    void OnUpdate(uint32 diff) override
    {
        if (_loaded)
            return;

        _updateTimer += diff;

        // 等待1秒后加载数据
        if (_updateTimer >= 1000)
        {
            RefreshReincarnationEnabled();
            bool enabled = IsReincarnationEnabled();
            if (!enabled)
            {
                LOG_INFO("server.loading", ">> 转身系统模块已禁用");
                _loaded = true;
                return;
            }

            sReincarnationMgr->LoadReincarnationConfig();

            LOG_INFO("server.loading", "→转身系统√");
            _loaded = true;
        }
    }

    void OnAfterConfigLoad(bool reload) override
    {
        RefreshReincarnationEnabled();

        if (reload && _loaded)
        {
            bool enabled = IsReincarnationEnabled();
            if (!enabled)
            {
                LOG_INFO("module", "转身系统模块已禁用");
                return;
            }

            sReincarnationMgr->LoadReincarnationConfig();

            LOG_INFO("module", "┌───────────────────────────────────────┐");
            LOG_INFO("module", "│          转身系统配置已重载           │");
            LOG_INFO("module", "└───────────────────────────────────────┘");
        }
    }

private:
    bool _loaded;
    uint32 _updateTimer;
};

// 玩家脚本类，用于处理玩家登录/登出事件和属性/天赋加成
class ReincarnationPlayerScript : public PlayerScript
{
public:
    ReincarnationPlayerScript() : PlayerScript("ReincarnationPlayerScript") { }

    // 玩家数据加载时（在InitTalentForLevel之前调用）
    void OnPlayerLoadFromDB(Player* player) override
    {
        if (!player)
            return;

        if (!IsReincarnationEnabled())
            return;

        // 在这里加载玩家转身数据
        sReincarnationMgr->LoadPlayerData(player);
    }

    // 玩家登录时显示信息并强制刷新属性
    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        if (!IsReincarnationEnabled())
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 reincarnationLevel = sReincarnationMgr->GetPlayerReincarnationLevel(playerGuid);

        if (reincarnationLevel > 0)
        {
            float bonusStats = sReincarnationMgr->GetPlayerBonusStats(playerGuid);
            uint32 bonusTalent = sReincarnationMgr->GetPlayerBonusTalentPoints(playerGuid);

            // 在登录完成后强制刷新所有属性
            player->UpdateAllStats();

            // 强制刷新所有评级属性
            player->UpdateAllRatings();

            // 显示转身信息
            std::string message = "|cff00ff00[转身系统]|r 当前转身等级: |cffffd700" +
                                  std::to_string(reincarnationLevel) + "转|r，全属性加成: |cff00ffff+" +
                                  std::to_string(static_cast<int>(bonusStats)) + "%|r，额外天赋点: |cffff00ff+" +
                                  std::to_string(bonusTalent) + "|r";
            ChatHandler(player->GetSession()).PSendSysMessage("{}", message);
        }
    }

    // 玩家登出时保存并清理数据
    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        if (!IsReincarnationEnabled())
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        // 保存玩家数据
        sReincarnationMgr->SavePlayerData(player);

        // 清理内存数据
        sReincarnationMgr->OnPlayerLogout(playerGuid);
    }

    // 【核心钩子】计算基础属性（力量、敏捷、耐力、智力、精神）时调用
    void OnPlayerAfterUpdateStat(Player* player, Stats stat, float& value) override
    {
        if (!player)
            return;

        if (!IsReincarnationEnabled())
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(playerGuid);

        // 应用加成（静默处理，不输出日志）
        if (bonusPercent > 0)
        {
            value *= (1.0f + bonusPercent / 100.0f);
        }
    }

    // 计算生命上限时调用
    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            value *= (1.0f + bonusPercent / 100.0f);
        }
    }

    // 计算能量上限时调用
    void OnPlayerAfterUpdateMaxPower(Player* player, Powers& power, float& value) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            value *= (1.0f + bonusPercent / 100.0f);
        }
    }

    // 计算天赋点时调用
    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& talentPointsForLevel) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        uint32 bonusTalent = sReincarnationMgr->GetPlayerBonusTalentPoints(player->GetGUID().GetCounter());
        if (bonusTalent > 0)
        {
            talentPointsForLevel += bonusTalent;
        }
    }

    // 计算攻击强度时调用
    void OnPlayerAfterUpdateAttackPowerAndDamage(Player* player, float& level, float& base_attPower, float& attPowerMod, float& attPowerMultiplier, bool ranged) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            // 通过修改攻击强度倍数来增加攻击强度
            float bonusMultiplier = bonusPercent / 100.0f;
            attPowerMultiplier += bonusMultiplier;
        }
    }

    // 计算护甲时调用
    void OnPlayerAfterUpdateArmor(Player* player, float& value) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            value *= (1.0f + bonusPercent / 100.0f);
        }
    }

    // 计算物理暴击率时调用
    void OnPlayerAfterUpdateCritPercentage(Player* player, WeaponAttackType attType, float& value) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            // 暴击率使用加法加成，避免数值过高
            value += bonusPercent * 0.1f;  // 每100%加成增加10%暴击
        }
    }

    // 计算法术暴击率时调用
    void OnPlayerAfterUpdateSpellCritChance(Player* player, uint32 school, float& value) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            // 法术暴击率使用加法加成，避免数值过高
            value += bonusPercent * 0.1f;  // 每100%加成增加10%暴击
        }
    }

    // 【核心钩子】计算评级属性时调用（命中、急速、精准、护甲穿透等）
    // CombatRating 包括：CR_WEAPON_SKILL, CR_DEFENSE_SKILL, CR_DODGE, CR_PARRY, CR_BLOCK,
    // CR_HIT_MELEE, CR_HIT_RANGED, CR_HIT_SPELL, CR_CRIT_MELEE, CR_CRIT_RANGED, CR_CRIT_SPELL,
    // CR_HIT_TAKEN_MELEE, CR_HIT_TAKEN_RANGED, CR_HIT_TAKEN_SPELL, CR_CRIT_TAKEN_MELEE,
    // CR_CRIT_TAKEN_RANGED, CR_CRIT_TAKEN_SPELL, CR_HASTE_MELEE, CR_HASTE_RANGED, CR_HASTE_SPELL,
    // CR_WEAPON_SKILL_MAINHAND, CR_WEAPON_SKILL_OFFHAND, CR_WEAPON_SKILL_RANGED, CR_EXPERTISE, CR_ARMOR_PENETRATION
    void OnPlayerAfterUpdateRating(Player* player, CombatRating cr, int128& amount) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0 && amount > 0)
        {
            // 对评级属性应用百分比加成
            amount = ScaleRatingForReincarnation(amount, bonusPercent);
        }
    }

    // 计算法术强度和治疗强度时调用
    void OnPlayerAfterUpdateSpellDamageAndHealing(Player* player, int128& healingBonus, int128 spellDamage[7]) override
    {
        if (!player || !IsReincarnationEnabled())
            return;

        float bonusPercent = sReincarnationMgr->GetPlayerBonusStats(player->GetGUID().GetCounter());
        if (bonusPercent > 0)
        {
            // 对治疗强度应用百分比加成
            if (healingBonus > 0)
                healingBonus = ScaleSpellPowerForReincarnation(healingBonus, bonusPercent);

            // 对所有学派的法术强度应用百分比加成
            // spellDamage[0] = 物理(不处理), [1]=神圣, [2]=火焰, [3]=自然, [4]=冰霜, [5]=暗影, [6]=奥术
            for (int i = 1; i < 7; ++i)
            {
                if (spellDamage[i] > 0)
                    spellDamage[i] = ScaleSpellPowerForReincarnation(spellDamage[i], bonusPercent);
            }
        }
    }
};

// 单位脚本类，用于处理基础属性加成
class ReincarnationUnitScript : public UnitScript
{
public:
    ReincarnationUnitScript() : UnitScript("ReincarnationUnitScript") { }

    // 修改属性值
    void OnUnitUpdate(Unit* unit, uint32 /*diff*/) override
    {
        // 此方法性能消耗大，不建议在这里处理属性
    }
};

// 添加脚本
void AddSC_ReincarnationModule()
{
    new ReincarnationWorldScript();
    new ReincarnationPlayerScript();
    new ReincarnationUnitScript();
}

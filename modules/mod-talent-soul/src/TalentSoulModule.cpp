/*
 * 天赋之魂系统 - 模块入口
 *
 * 实现方式：
 * 1. 伤害加成 - 通过 UnitScript::ModifySpellDamageTaken hook
 * 2. 冷却减少 - 通过 AllSpellScript::OnSpellCast 在施法后修改冷却
 * 3. GCD减少 - 通过多种机制协同工作：
 *    a) OnCalcGlobalCooldown - 减少服务端GCD计算值
 *    b) OnSpellCast - 发送 SMSG_CLEAR_COOLDOWN 清除客户端GCD动画
 * 4. 消耗减少 - 通过 AllSpellScript::OnSpellPrepare 修改消耗
 *
 * 关于客户端GCD的说明：
 * WoW 3.3.5 客户端的 GCD 动画是基于本地 Spell.dbc 数据计算的。
 * 我们通过发送 SMSG_CLEAR_COOLDOWN 包来清除客户端的 GCD 动画，
 * 同时服务端通过 OnCalcGlobalCooldown 钩子正确计算减少后的 GCD 时间。
 */

#include "TalentSoul.h"
#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Log.h"
#include "World.h"
#include "Player.h"
#include "Unit.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Spell.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "WorldSession.h"
#include "ObjectAccessor.h"

// 世界脚本类，用于延迟加载天赋之魂数据
class TalentSoulWorldScript : public WorldScript
{
public:
    TalentSoulWorldScript() : WorldScript("TalentSoulWorldScript"), _loaded(false), _updateTimer(0), _saveTimer(0)
    {
    }

    void OnUpdate(uint32 diff) override
    {
        if (!_loaded)
        {
            _updateTimer += diff;

            // 等待1秒后加载数据
            if (_updateTimer >= 1000)
            {
                bool enabled = sConfigMgr->GetOption("TalentSoul.Enable", true);
                if (!enabled)
                {
                    LOG_INFO("server.loading", ">> 天赋之魂模块已禁用");
                    _loaded = true;
                    return;
                }

                sTalentSoulMgr->LoadTalentSoulData();

                // 设置独立GCD类别，使每个配置了GCD减少的技能有独立的GCD计时器
                sTalentSoulMgr->SetupIndependentGCDCategories();

                LOG_INFO("server.loading", "→天赋之魂系统加载成功√");
                _loaded = true;
                _updateTimer = 0;
            }

            return;
        }

        _saveTimer += diff;
        if (_saveTimer >= 5000)
        {
            _saveTimer = 0;
            if (sConfigMgr->GetOption("TalentSoul.Enable", true))
                sTalentSoulMgr->FlushDirtyPlayerData();
        }
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload && _loaded)
        {
            bool enabled = sConfigMgr->GetOption("TalentSoul.Enable", true);
            if (!enabled)
            {
                LOG_INFO("module", "天赋之魂模块已禁用");
                return;
            }

            sTalentSoulMgr->LoadTalentSoulData();

            // 重载时也需要重新设置独立GCD类别
            sTalentSoulMgr->SetupIndependentGCDCategories();

            LOG_INFO("module", "┌───────────────────────────────────────┐");
            LOG_INFO("module", "│        天赋之魂配置已重载             │");
            LOG_INFO("module", "└───────────────────────────────────────┘");
        }
    }

private:
    bool _loaded;
    uint32 _updateTimer;
    uint32 _saveTimer;
};

// 玩家脚本类，用于处理玩家登录/登出事件
class TalentSoulPlayerScript : public PlayerScript
{
public:
    TalentSoulPlayerScript() : PlayerScript("TalentSoulPlayerScript") { }

    // 玩家登录时加载数据
    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        // 加载玩家的天赋之魂数据
        sTalentSoulMgr->LoadPlayerData(player);
    }

    // 玩家登出时保存并清理数据
    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        // 保存玩家数据
        sTalentSoulMgr->SavePlayerData(player);

        // 清理内存数据
        sTalentSoulMgr->OnPlayerLogout(playerGuid);
    }
};

// 单位脚本类，用于处理伤害修改
class TalentSoulUnitScript : public UnitScript
{
public:
    TalentSoulUnitScript() : UnitScript("TalentSoulUnitScript") { }

    // 修改法术伤害
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int64& damage, SpellInfo const* spellInfo) override
    {
        if (!attacker || !spellInfo || damage <= 0)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        Player* player = attacker->ToPlayer();
        if (!player)
            return;

        uint32 spellId = spellInfo->Id;
        int64 originalDamage = damage;

        // 应用伤害加成
        sTalentSoulMgr->ApplyDamageBonus(attacker, spellId, damage);

        if (damage != originalDamage)
        {
            LOG_DEBUG("module", "[天赋之魂-伤害] 玩家 {} 技能 {} 伤害 {} -> {} (+{:.1f}%)",
                player->GetName(), spellId, originalDamage, damage,
                static_cast<double>((static_cast<long double>(damage - originalDamage) / static_cast<long double>(originalDamage)) * 100.0L));
        }
    }

    // 修改近战伤害
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        // 近战伤害不在此系统处理范围内
    }
};

// 全技能脚本类，用于处理GCD、冷却和消耗修改
class TalentSoulAllSpellScript : public AllSpellScript
{
public:
    TalentSoulAllSpellScript() : AllSpellScript("TalentSoulAllSpellScript") { }

    // 技能准备时 - 处理消耗减少
    void OnSpellPrepare(Spell* spell, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!spell || !caster || !spellInfo)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        uint32 spellId = spellInfo->Id;

        // 检查是否有天赋之魂配置
        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);
        if (!config)
            return;

        // 应用消耗减少
        int64 currentCost = spell->GetPowerCost();
        if (currentCost > 0)
        {
            int64 newCost = currentCost;
            sTalentSoulMgr->ApplyCostReduction(player, spellId, newCost);

            if (newCost != currentCost)
            {
                spell->SetPowerCost(newCost);

                LOG_DEBUG("module", "[天赋之魂-消耗] 玩家 {} 技能 {} 消耗 {} -> {} (-{:.1f}%)",
                    player->GetName(), spellId, currentCost, newCost,
                    ((float)(currentCost - newCost) / currentCost) * 100.0f);
            }
        }
    }

    // 技能施放时 - 修改冷却时间，并同步客户端GCD
    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (!spell || !caster || !spellInfo)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        uint32 spellId = spellInfo->Id;
        uint32 playerGuid = player->GetGUID().GetCounter();

        // 检查是否有天赋之魂配置
        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);
        if (!config)
            return;

        // 获取GCD减少百分比
        float gcdReductionPercent = sTalentSoulMgr->GetPlayerGCDReduction(playerGuid, spellId);

        // 如果有GCD减少，处理客户端同步
        if (gcdReductionPercent > 0)
        {
            int32 originalGCD = spellInfo->StartRecoveryTime;
            if (originalGCD <= 0)
                originalGCD = 1500;

            int32 reducedAmount = static_cast<int32>(originalGCD * gcdReductionPercent / 100.0f);
            int32 newGCD = originalGCD - reducedAmount;
            if (newGCD < 0)
                newGCD = 0;

            // 如果GCD减少到很低，发送清除包清除客户端GCD动画
            if (newGCD < 500)
            {
                // 取消服务端的GCD记录
                player->GetGlobalCooldownMgr().CancelGlobalCooldown(spellInfo);

                // 发送SMSG_CLEAR_COOLDOWN清除客户端GCD动画
                WorldPacket clearData(SMSG_CLEAR_COOLDOWN, 4 + 8);
                clearData << uint32(spellId);
                clearData << player->GetGUID();
                player->SendDirectMessage(&clearData);

                LOG_DEBUG("module", "[天赋之魂-GCD] 玩家 {} 技能 {} GCD {}ms -> {}ms, 已清除客户端GCD",
                    player->GetName(), spellId, originalGCD, newGCD);
            }
        }

        // 应用冷却减少
        int32 cooldownReduction = 0;
        sTalentSoulMgr->ApplyCooldownReduction(player, spellId, cooldownReduction);

        if (cooldownReduction < 0)
        {
            // cooldownReduction 是负值表示减少的毫秒数
            player->ModifySpellCooldown(spellId, cooldownReduction);

            LOG_DEBUG("module", "[天赋之魂-冷却] 玩家 {} 技能 {} 冷却减少 {}ms",
                player->GetName(), spellId, -cooldownReduction);
        }
    }

    // GCD计算时 - 修改服务端GCD
    void OnCalcGlobalCooldown(Spell* spell, Unit* caster, SpellInfo const* spellInfo, int32& gcd) override
    {
        if (!spell || !caster || !spellInfo || gcd <= 0)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        uint32 spellId = spellInfo->Id;

        // 检查是否有天赋之魂配置
        TalentSoulData const* config = sTalentSoulMgr->GetTalentSoulData(spellId);
        if (!config)
            return;

        // 获取GCD减少百分比
        float reductionPercent = sTalentSoulMgr->GetPlayerGCDReduction(player->GetGUID().GetCounter(), spellId);
        if (reductionPercent > 0)
        {
            int32 originalGCD = gcd;
            int32 reducedAmount = static_cast<int32>(gcd * reductionPercent / 100.0f);
            gcd -= reducedAmount;

            if (gcd < 0)
                gcd = 0;

            LOG_DEBUG("module", "[天赋之魂-GCD(服务端)] 玩家 {} 技能 {} GCD {} -> {} (-{:.1f}%)",
                player->GetName(), spellId, originalGCD, gcd, reductionPercent);
        }
    }
};

// 添加脚本
void AddSC_TalentSoulModule()
{
    new TalentSoulWorldScript();
    new TalentSoulPlayerScript();
    new TalentSoulUnitScript();
    new TalentSoulAllSpellScript();
}

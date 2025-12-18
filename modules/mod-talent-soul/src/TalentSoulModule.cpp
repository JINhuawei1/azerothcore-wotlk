/*
 * 天赋之魂系统 - 模块入口
 *
 * 实现方式：
 * 1. 伤害加成 - 通过 UnitScript::ModifySpellDamageTaken hook
 * 2. 冷却减少 - 通过 AllSpellScript::OnSpellCast 在施法后修改冷却
 * 3. GCD减少 - 通过 AllSpellScript::OnSpellCast 修改GCD (需要核心支持)
 * 4. 消耗减少 - 通过 AllSpellScript::OnSpellPrepare 修改消耗
 */

#include "TalentSoul.h"
#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Log.h"
#include "World.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Spell.h"

// 世界脚本类，用于延迟加载天赋之魂数据
class TalentSoulWorldScript : public WorldScript
{
public:
    TalentSoulWorldScript() : WorldScript("TalentSoulWorldScript"), _loaded(false), _updateTimer(0)
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
            bool enabled = sConfigMgr->GetOption("TalentSoul.Enable", true);
            if (!enabled)
            {
                LOG_INFO("server.loading", ">> 天赋之魂模块已禁用");
                _loaded = true;
                return;
            }

            sTalentSoulMgr->LoadTalentSoulData();
            LOG_INFO("server.loading", "→天赋之魂系统加载成功√");
            _loaded = true;
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
            LOG_INFO("module", "┌───────────────────────────────────────┐");
            LOG_INFO("module", "│        天赋之魂配置已重载             │");
            LOG_INFO("module", "└───────────────────────────────────────┘");
        }
    }

private:
    bool _loaded;
    uint32 _updateTimer;
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

        // 保存玩家数据
        sTalentSoulMgr->SavePlayerData(player);

        // 清理内存数据
        sTalentSoulMgr->OnPlayerLogout(player->GetGUID().GetCounter());
    }
};

// 单位脚本类，用于处理伤害修改
class TalentSoulUnitScript : public UnitScript
{
public:
    TalentSoulUnitScript() : UnitScript("TalentSoulUnitScript") { }

    // 修改法术伤害
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!attacker || !spellInfo || damage <= 0)
            return;

        if (!sConfigMgr->GetOption("TalentSoul.Enable", true))
            return;

        // 应用伤害加成
        sTalentSoulMgr->ApplyDamageBonus(attacker, spellInfo->Id, damage);
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

    // 技能准备时 - 修改消耗
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

        // 应用消耗减少
        int32 currentCost = spell->GetPowerCost();
        if (currentCost > 0)
        {
            int32 newCost = currentCost;
            sTalentSoulMgr->ApplyCostReduction(player, spellId, newCost);

            if (newCost != currentCost)
            {
                spell->SetPowerCost(newCost);

                if (sConfigMgr->GetOption("TalentSoul.Debug", false))
                {
                    LOG_INFO("module", "[天赋之魂] 技能 {} 消耗从 {} 减少到 {}", spellId, currentCost, newCost);
                }
            }
        }
    }

    // 技能施放时 - 修改冷却和GCD
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

        // 应用冷却减少
        int32 cooldownReduction = 0;
        sTalentSoulMgr->ApplyCooldownReduction(player, spellId, cooldownReduction);

        if (cooldownReduction < 0)
        {
            // cooldownReduction 是负值表示减少的毫秒数
            player->ModifySpellCooldown(spellId, cooldownReduction);

            if (sConfigMgr->GetOption("TalentSoul.Debug", false))
            {
                LOG_INFO("module", "[天赋之魂] 技能 {} 冷却减少 {} 毫秒", spellId, -cooldownReduction);
            }
        }

        // 应用GCD减少
        int32 gcdReduction = 0;
        sTalentSoulMgr->ApplyGCDReduction(player, spellId, gcdReduction);

        if (gcdReduction < 0)
        {
            // 尝试修改GCD
            auto& gcdMgr = player->GetGlobalCooldownMgr();
            if (gcdMgr.HasGlobalCooldown(spellInfo))
            {
                // 修改GCD结束时间
                gcdMgr.ModifyGlobalCooldown(spellInfo, gcdReduction);

                if (sConfigMgr->GetOption("TalentSoul.Debug", false))
                {
                    LOG_INFO("module", "[天赋之魂] 技能 {} GCD减少 {} 毫秒", spellId, -gcdReduction);
                }
            }
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

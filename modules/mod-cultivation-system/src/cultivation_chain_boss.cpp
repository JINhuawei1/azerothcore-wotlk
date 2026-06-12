/*
 * 修仙系统 - 连续召唤Boss战斗脚本
 * 100个Boss (390101-390200) 循环使用10套AI脚本
 * 每套6个技能，全部使用客户端 Spell.dbc 已存在法术
 * 击杀后5秒在第一层Boss出生点召唤下一个Boss
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"
#include "SpellMgr.h"

void RegisterCultivationTribulationBoss(uint32 bossGuid, uint32 playerGuid);

enum ChainBossSpells
{
    // 脚本1: 火焰系
    SPELL_FIRE_BOLT         = 42833, // 火球术
    SPELL_FIRE_NOVA         = 42945, // 冲击波
    SPELL_FIRE_SHIELD       = 43010, // 火焰防护结界
    SPELL_METEOR            = 42891, // 炎爆术
    SPELL_IGNITE            = 55360, // 活体炸弹
    SPELL_INFERNO           = 42926, // 烈焰风暴

    // 脚本2: 冰霜系
    SPELL_FROST_BOLT        = 42842, // 寒冰箭
    SPELL_BLIZZARD          = 42940, // 暴风雪
    SPELL_FROST_ARMOR       = 43039, // 冰霜护甲
    SPELL_ICE_LANCE         = 42914, // 冰枪术
    SPELL_FROZEN_ORB        = 42917, // 冰霜新星
    SPELL_DEEP_FREEZE       = 44572, // 深度冻结

    // 脚本3: 暗影系
    SPELL_SHADOW_BOLT       = 47809, // 暗影箭
    SPELL_SHADOW_NOVA       = 10890, // 心灵尖啸
    SPELL_DRAIN_LIFE        = 47857, // 吸取生命
    SPELL_CURSE_AGONY       = 47864, // 痛苦诅咒
    SPELL_SHADOW_FURY       = 47847, // 暗影之怒
    SPELL_DARK_PACT         = 47891, // 暗影防护结界

    // 脚本4: 自然系
    SPELL_WRATH             = 48461, // 愤怒
    SPELL_STARFIRE          = 48465, // 星火术
    SPELL_THORNS            = 53307, // 荆棘术
    SPELL_ENTANGLE          = 53308, // 纠缠根须
    SPELL_STARFALL          = 53201, // 星辰坠落
    SPELL_REJUVENATE_BOSS   = 48441, // 回春术

    // 脚本5: 神圣系
    SPELL_SMITE             = 48123, // 惩击
    SPELL_HOLY_NOVA_BOSS    = 48078, // 神圣新星
    SPELL_DIVINE_SHIELD     = 642,   // 圣盾术
    SPELL_HAMMER_JUSTICE    = 10308, // 制裁之锤
    SPELL_CONSECRATION      = 48819, // 奉献
    SPELL_HOLY_WRATH        = 48817, // 神圣愤怒

    // 脚本6: 奥术系
    SPELL_ARCANE_BLAST      = 42897, // 奥术冲击
    SPELL_ARCANE_EXPLOSION  = 42921, // 魔爆术
    SPELL_MANA_SHIELD       = 43020, // 法力护盾
    SPELL_ARCANE_MISSILES   = 42846, // 奥术飞弹
    SPELL_COUNTERSPELL_BOSS = 2139,  // 法术反制
    SPELL_ARCANE_BARRAGE    = 44781, // 奥术弹幕

    // 脚本7: 物理系
    SPELL_MORTAL_STRIKE     = 47486, // 致死打击
    SPELL_WHIRLWIND_BOSS    = 1680,  // 旋风斩
    SPELL_BATTLE_SHOUT      = 47436, // 战斗怒吼
    SPELL_CHARGE_BOSS       = 11578, // 冲锋
    SPELL_THUNDER_CLAP      = 47502, // 雷霆一击
    SPELL_EXECUTE_BOSS      = 47471, // 斩杀

    // 脚本8: 毒系
    SPELL_POISON_BOLT       = 21067, // 毒箭
    SPELL_POISON_CLOUD      = 57061, // 毒云
    SPELL_ENVENOM           = 57993, // 毒伤
    SPELL_CRIPPLING_POISON  = 3409,  // 致残毒药
    SPELL_VENOM_SPIT        = 45525, // 毒液喷吐
    SPELL_DEADLY_POISON     = 57970, // 致命药膏

    // 脚本9: 雷电系
    SPELL_LIGHTNING_BOLT    = 49238, // 闪电箭
    SPELL_CHAIN_LIGHTNING   = 49271, // 闪电链
    SPELL_LIGHTNING_SHIELD  = 49281, // 闪电之盾
    SPELL_THUNDERSTORM      = 59159, // 雷霆风暴
    SPELL_STORMSTRIKE       = 17364, // 风暴打击
    SPELL_EARTH_SHOCK       = 49231, // 地震术

    // 脚本10: 混沌系
    SPELL_CHAOS_BOLT        = 47825, // 混乱之箭
    SPELL_VOID_ZONE         = 47836, // 腐蚀之种
    SPELL_BERSERK_BOSS      = 47893, // 恶魔护甲
    SPELL_SOUL_FIRE         = 47811, // 灵魂之火
    SPELL_RAIN_OF_CHAOS     = 47820, // 火焰之雨
    SPELL_NETHER_PORTAL     = 47867, // 末日灾祸
};

// ============================================================
// 基类：处理连续召唤逻辑
// ============================================================
struct npc_cultivation_chain_boss_base : public ScriptedAI
{
    npc_cultivation_chain_boss_base(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        events.Reset();
        me->SetFullHealth();
    }

    void CastVictimSpell(uint32 spellId)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
        {
            LOG_INFO("server.loading", "修仙连续Boss: Spell.dbc 缺少法术 {}, Boss {} 无法施放。", spellId, me->GetEntry());
            return;
        }

        DoCastVictim(spellId, true);
    }

    void CastAoeSpell(uint32 spellId)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
        {
            LOG_INFO("server.loading", "修仙连续Boss: Spell.dbc 缺少法术 {}, Boss {} 无法施放。", spellId, me->GetEntry());
            return;
        }

        DoCastAOE(spellId, true);
    }

    void CastSelfSpell(uint32 spellId)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
        {
            LOG_INFO("server.loading", "修仙连续Boss: Spell.dbc 缺少法术 {}, Boss {} 无法施放。", spellId, me->GetEntry());
            return;
        }

        DoCast(me, spellId, true);
    }

    uint32 GetBossIndex() const
    {
        return me->GetEntry() - 390100; // 1-100
    }

    void JustDied(Unit* killer) override
    {
        uint32 bossIndex = GetBossIndex();

        if (bossIndex >= 100)
            return; // 最后一个Boss，不再召唤

        Player* player = killer ? killer->ToPlayer() : nullptr;
        if (!player && killer)
            player = killer->GetCharmerOrOwnerPlayerOrPlayerItself();

        if (player)
        {
            // 通知玩家
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[修仙试炼]|r 第 {} 重天劫已渡！5秒后降临第 {} 重...",
                bossIndex, bossIndex + 1);
        }

        if (!player)
            return; // 没有可归属的玩家，无法续召下一重

        // 5秒后召唤下一个Boss
        uint32 nextEntry = 390100 + bossIndex + 1;
        float x, y, z, o;
        me->GetHomePosition(x, y, z, o);

        // 不能捕获裸 Unit* —— 5 秒后 killer（尤其是宠物/守护者）可能已被析构，
        // 捕获 ObjectGuid 并在回调里重新解析
        ObjectGuid playerGuid = player->GetGUID();

        me->m_Events.AddEventAtOffset([nextEntry, x, y, z, o, playerGuid]()
        {
            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (!p || !p->IsInWorld())
                return;

            if (Creature* next = p->SummonCreature(nextEntry, x, y, z, o,
                TEMPSUMMON_TIMED_DESPAWN_OOC_ALIVE, 30000))
            {
                next->SetCorpseDelay(30);
                next->SetInCombatWith(p);
                next->AddThreat(p, 1000.0f);
                next->AI()->AttackStart(p);
                RegisterCultivationTribulationBoss(next->GetGUID().GetCounter(), p->GetGUID().GetCounter());
            }
        }, 5s);
    }
};

// ============================================================
// 脚本1: 火焰系
// ============================================================
struct npc_cultivation_chain_boss_1 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_1(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);       // Fire Bolt
        events.ScheduleEvent(2, 8s);       // Fire Nova
        events.ScheduleEvent(3, 3s);       // Fire Shield
        events.ScheduleEvent(4, 15s);      // Meteor
        events.ScheduleEvent(5, 10s);      // Ignite
        events.ScheduleEvent(6, 20s);      // Inferno
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_FIRE_BOLT);    events.Repeat(3s); break;
                case 2: CastAoeSpell(SPELL_FIRE_NOVA);       events.Repeat(12s); break;
                case 3: CastSelfSpell(SPELL_FIRE_SHIELD);    events.Repeat(30s); break;
                case 4: CastVictimSpell(SPELL_METEOR);       events.Repeat(18s); break;
                case 5: CastVictimSpell(SPELL_IGNITE);       events.Repeat(14s); break;
                case 6: CastAoeSpell(SPELL_INFERNO);         events.Repeat(25s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本2: 冰霜系
// ============================================================
struct npc_cultivation_chain_boss_2 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_2(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 10s);
        events.ScheduleEvent(3, 2s);
        events.ScheduleEvent(4, 6s);
        events.ScheduleEvent(5, 14s);
        events.ScheduleEvent(6, 20s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_FROST_BOLT);   events.Repeat(3s); break;
                case 2: CastAoeSpell(SPELL_BLIZZARD);        events.Repeat(15s); break;
                case 3: CastSelfSpell(SPELL_FROST_ARMOR);    events.Repeat(35s); break;
                case 4: CastVictimSpell(SPELL_ICE_LANCE);    events.Repeat(8s); break;
                case 5: CastVictimSpell(SPELL_FROZEN_ORB);   events.Repeat(18s); break;
                case 6: CastVictimSpell(SPELL_DEEP_FREEZE);  events.Repeat(22s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本3: 暗影系
// ============================================================
struct npc_cultivation_chain_boss_3 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_3(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 12s);
        events.ScheduleEvent(3, 5s);
        events.ScheduleEvent(4, 8s);
        events.ScheduleEvent(5, 16s);
        events.ScheduleEvent(6, 4s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_SHADOW_BOLT);  events.Repeat(3s); break;
                case 2: CastAoeSpell(SPELL_SHADOW_NOVA);     events.Repeat(14s); break;
                case 3: CastVictimSpell(SPELL_DRAIN_LIFE);   events.Repeat(10s); break;
                case 4: CastVictimSpell(SPELL_CURSE_AGONY);  events.Repeat(12s); break;
                case 5: CastAoeSpell(SPELL_SHADOW_FURY);     events.Repeat(18s); break;
                case 6: CastSelfSpell(SPELL_DARK_PACT);      events.Repeat(25s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本4: 自然系
// ============================================================
struct npc_cultivation_chain_boss_4 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_4(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 10s);
        events.ScheduleEvent(3, 1s);
        events.ScheduleEvent(4, 7s);
        events.ScheduleEvent(5, 18s);
        events.ScheduleEvent(6, 14s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_WRATH);            events.Repeat(3s); break;
                case 2: CastVictimSpell(SPELL_STARFIRE);         events.Repeat(16s); break;
                case 3: CastSelfSpell(SPELL_THORNS);             events.Repeat(30s); break;
                case 4: CastVictimSpell(SPELL_ENTANGLE);         events.Repeat(12s); break;
                case 5: CastAoeSpell(SPELL_STARFALL);            events.Repeat(22s); break;
                case 6: CastSelfSpell(SPELL_REJUVENATE_BOSS);    events.Repeat(20s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本5: 神圣系
// ============================================================
struct npc_cultivation_chain_boss_5 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_5(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 9s);
        events.ScheduleEvent(3, 20s);
        events.ScheduleEvent(4, 6s);
        events.ScheduleEvent(5, 12s);
        events.ScheduleEvent(6, 16s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_SMITE);            events.Repeat(3s); break;
                case 2: CastAoeSpell(SPELL_HOLY_NOVA_BOSS);      events.Repeat(11s); break;
                case 3: CastSelfSpell(SPELL_DIVINE_SHIELD);      events.Repeat(40s); break;
                case 4: CastVictimSpell(SPELL_HAMMER_JUSTICE);    events.Repeat(10s); break;
                case 5: CastAoeSpell(SPELL_CONSECRATION);        events.Repeat(15s); break;
                case 6: CastAoeSpell(SPELL_HOLY_WRATH);          events.Repeat(20s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本6: 奥术系
// ============================================================
struct npc_cultivation_chain_boss_6 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_6(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 7s);
        events.ScheduleEvent(3, 2s);
        events.ScheduleEvent(4, 5s);
        events.ScheduleEvent(5, 14s);
        events.ScheduleEvent(6, 10s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_ARCANE_BLAST);     events.Repeat(3s); break;
                case 2: CastAoeSpell(SPELL_ARCANE_EXPLOSION);    events.Repeat(10s); break;
                case 3: CastSelfSpell(SPELL_MANA_SHIELD);        events.Repeat(30s); break;
                case 4: CastVictimSpell(SPELL_ARCANE_MISSILES);  events.Repeat(8s); break;
                case 5: CastVictimSpell(SPELL_COUNTERSPELL_BOSS);events.Repeat(16s); break;
                case 6: CastAoeSpell(SPELL_ARCANE_BARRAGE);      events.Repeat(12s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本7: 物理系
// ============================================================
struct npc_cultivation_chain_boss_7 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_7(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 3s);
        events.ScheduleEvent(2, 8s);
        events.ScheduleEvent(3, 1s);
        events.ScheduleEvent(4, 0s);
        events.ScheduleEvent(5, 12s);
        events.ScheduleEvent(6, 20s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_MORTAL_STRIKE);    events.Repeat(6s); break;
                case 2: CastAoeSpell(SPELL_WHIRLWIND_BOSS);      events.Repeat(10s); break;
                case 3: CastSelfSpell(SPELL_BATTLE_SHOUT);       events.Repeat(30s); break;
                case 4: CastVictimSpell(SPELL_CHARGE_BOSS);      events.Repeat(15s); break;
                case 5: CastAoeSpell(SPELL_THUNDER_CLAP);        events.Repeat(14s); break;
                case 6: CastVictimSpell(SPELL_EXECUTE_BOSS);     events.Repeat(12s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本8: 毒系
// ============================================================
struct npc_cultivation_chain_boss_8 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_8(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 12s);
        events.ScheduleEvent(3, 3s);
        events.ScheduleEvent(4, 6s);
        events.ScheduleEvent(5, 9s);
        events.ScheduleEvent(6, 15s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_POISON_BOLT);      events.Repeat(4s); break;
                case 2: CastAoeSpell(SPELL_POISON_CLOUD);        events.Repeat(16s); break;
                case 3: CastSelfSpell(SPELL_ENVENOM);            events.Repeat(25s); break;
                case 4: CastVictimSpell(SPELL_CRIPPLING_POISON); events.Repeat(10s); break;
                case 5: CastAoeSpell(SPELL_VENOM_SPIT);          events.Repeat(12s); break;
                case 6: CastVictimSpell(SPELL_DEADLY_POISON);    events.Repeat(8s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本9: 雷电系
// ============================================================
struct npc_cultivation_chain_boss_9 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_9(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 6s);
        events.ScheduleEvent(3, 2s);
        events.ScheduleEvent(4, 14s);
        events.ScheduleEvent(5, 8s);
        events.ScheduleEvent(6, 10s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_LIGHTNING_BOLT);   events.Repeat(3s); break;
                case 2: CastVictimSpell(SPELL_CHAIN_LIGHTNING);  events.Repeat(9s); break;
                case 3: CastSelfSpell(SPELL_LIGHTNING_SHIELD);   events.Repeat(28s); break;
                case 4: CastAoeSpell(SPELL_THUNDERSTORM);        events.Repeat(18s); break;
                case 5: CastVictimSpell(SPELL_STORMSTRIKE);      events.Repeat(7s); break;
                case 6: CastVictimSpell(SPELL_EARTH_SHOCK);      events.Repeat(12s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 脚本10: 混沌系
// ============================================================
struct npc_cultivation_chain_boss_10 : public npc_cultivation_chain_boss_base
{
    npc_cultivation_chain_boss_10(Creature* c) : npc_cultivation_chain_boss_base(c) {}

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(1, 0s);
        events.ScheduleEvent(2, 10s);
        events.ScheduleEvent(3, 25s);
        events.ScheduleEvent(4, 5s);
        events.ScheduleEvent(5, 15s);
        events.ScheduleEvent(6, 20s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim()) return;
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: CastVictimSpell(SPELL_CHAOS_BOLT);       events.Repeat(4s); break;
                case 2: CastAoeSpell(SPELL_VOID_ZONE);           events.Repeat(14s); break;
                case 3: CastSelfSpell(SPELL_BERSERK_BOSS);       events.Repeat(45s); break;
                case 4: CastVictimSpell(SPELL_SOUL_FIRE);        events.Repeat(8s); break;
                case 5: CastAoeSpell(SPELL_RAIN_OF_CHAOS);       events.Repeat(20s); break;
                case 6: CastAoeSpell(SPELL_NETHER_PORTAL);       events.Repeat(30s); break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// ============================================================
// 注册脚本
// ============================================================
void AddSC_cultivation_chain_boss()
{
    RegisterCreatureAI(npc_cultivation_chain_boss_1);
    RegisterCreatureAI(npc_cultivation_chain_boss_2);
    RegisterCreatureAI(npc_cultivation_chain_boss_3);
    RegisterCreatureAI(npc_cultivation_chain_boss_4);
    RegisterCreatureAI(npc_cultivation_chain_boss_5);
    RegisterCreatureAI(npc_cultivation_chain_boss_6);
    RegisterCreatureAI(npc_cultivation_chain_boss_7);
    RegisterCreatureAI(npc_cultivation_chain_boss_8);
    RegisterCreatureAI(npc_cultivation_chain_boss_9);
    RegisterCreatureAI(npc_cultivation_chain_boss_10);
}

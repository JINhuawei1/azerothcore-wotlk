/*
 * 修仙系统 - 连续召唤Boss战斗脚本
 * 100个Boss (390101-390200) 循环使用10套AI脚本
 * 每套6个技能，技能ID 371101-371160
 * 击杀后5秒在同位置召唤下一个Boss
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "Chat.h"

enum ChainBossSpells
{
    // 脚本1: 火焰系 (371101-371106)
    SPELL_FIRE_BOLT         = 371101, // 火球术：单体伤害
    SPELL_FIRE_NOVA         = 371102, // 烈焰新星：AOE
    SPELL_FIRE_SHIELD       = 371103, // 火焰护盾：自身buff
    SPELL_METEOR            = 371104, // 陨石坠落：延迟AOE
    SPELL_IGNITE            = 371105, // 点燃：DOT
    SPELL_INFERNO           = 371106, // 地狱火：持续AOE

    // 脚本2: 冰霜系 (371107-371112)
    SPELL_FROST_BOLT        = 371107, // 寒冰箭：单体+减速
    SPELL_BLIZZARD          = 371108, // 暴风雪：区域AOE
    SPELL_FROST_ARMOR       = 371109, // 冰甲术：自身buff
    SPELL_ICE_LANCE         = 371110, // 冰枪术：单体爆发
    SPELL_FROZEN_ORB        = 371111, // 冰冻之球：移动AOE
    SPELL_DEEP_FREEZE       = 371112, // 深度冻结：控制

    // 脚本3: 暗影系 (371113-371118)
    SPELL_SHADOW_BOLT       = 371113, // 暗影箭：单体
    SPELL_SHADOW_NOVA       = 371114, // 暗影新星：AOE+恐惧
    SPELL_DRAIN_LIFE        = 371115, // 生命吸取：吸血
    SPELL_CURSE_AGONY       = 371116, // 痛苦诅咒：DOT
    SPELL_SHADOW_FURY       = 371117, // 暗影之怒：AOE眩晕
    SPELL_DARK_PACT         = 371118, // 黑暗契约：自身强化

    // 脚本4: 自然系 (371119-371124)
    SPELL_WRATH             = 371119, // 愤怒：单体
    SPELL_HURRICANE         = 371120, // 飓风：区域AOE
    SPELL_THORNS            = 371121, // 荆棘术：反伤buff
    SPELL_ENTANGLE          = 371122, // 纠缠根须：定身
    SPELL_STARFALL          = 371123, // 星辰坠落：大范围AOE
    SPELL_REJUVENATE_BOSS   = 371124, // 回春术：自身HOT

    // 脚本5: 神圣系 (371125-371130)
    SPELL_SMITE             = 371125, // 惩击：单体
    SPELL_HOLY_NOVA_BOSS    = 371126, // 神圣新星：AOE
    SPELL_DIVINE_SHIELD     = 371127, // 神圣护盾：免伤buff
    SPELL_HAMMER_JUSTICE    = 371128, // 制裁之锤：眩晕
    SPELL_CONSECRATION      = 371129, // 奉献：脚下AOE
    SPELL_HOLY_WRATH        = 371130, // 神圣愤怒：大AOE

    // 脚本6: 奥术系 (371131-371136)
    SPELL_ARCANE_BLAST      = 371131, // 奥术冲击：单体
    SPELL_ARCANE_EXPLOSION  = 371132, // 奥术爆炸：AOE
    SPELL_MANA_SHIELD       = 371133, // 法力护盾：吸收buff
    SPELL_ARCANE_MISSILES   = 371134, // 奥术飞弹：连击
    SPELL_COUNTERSPELL_BOSS = 371135, // 法术反制：沉默
    SPELL_ARCANE_BARRAGE    = 371136, // 奥术弹幕：多目标

    // 脚本7: 物理系 (371137-371142)
    SPELL_MORTAL_STRIKE     = 371137, // 致死打击：单体+治疗减半
    SPELL_WHIRLWIND_BOSS    = 371138, // 旋风斩：AOE
    SPELL_BATTLE_SHOUT      = 371139, // 战斗怒吼：自身强化
    SPELL_CHARGE_BOSS       = 371140, // 冲锋：突进+眩晕
    SPELL_THUNDER_CLAP      = 371141, // 雷霆一击：AOE减速
    SPELL_EXECUTE_BOSS      = 371142, // 斩杀：低血量爆发

    // 脚本8: 毒系 (371143-371148)
    SPELL_POISON_BOLT       = 371143, // 毒箭：单体DOT
    SPELL_POISON_CLOUD      = 371144, // 毒云：区域AOE
    SPELL_ENVENOM           = 371145, // 毒化：自身强化
    SPELL_CRIPPLING_POISON  = 371146, // 致残毒药：减速
    SPELL_VENOM_SPIT        = 371147, // 毒液喷射：锥形AOE
    SPELL_DEADLY_POISON     = 371148, // 致命毒药：叠加DOT

    // 脚本9: 雷电系 (371149-371154)
    SPELL_LIGHTNING_BOLT    = 371149, // 闪电箭：单体
    SPELL_CHAIN_LIGHTNING   = 371150, // 闪电链：弹射
    SPELL_LIGHTNING_SHIELD  = 371151, // 闪电之盾：反击buff
    SPELL_THUNDERSTORM      = 371152, // 雷暴：AOE+击退
    SPELL_STORMSTRIKE       = 371153, // 风暴打击：近战爆发
    SPELL_EARTH_SHOCK       = 371154, // 地震术：打断+伤害

    // 脚本10: 混沌系 (371155-371160)
    SPELL_CHAOS_BOLT        = 371155, // 混沌箭：单体高伤
    SPELL_VOID_ZONE         = 371156, // 虚空区域：站桩AOE
    SPELL_BERSERK_BOSS      = 371157, // 狂暴：自身强化
    SPELL_SOUL_FIRE         = 371158, // 灵魂之火：延迟高伤
    SPELL_RAIN_OF_CHAOS     = 371159, // 混沌之雨：大范围AOE
    SPELL_NETHER_PORTAL     = 371160, // 虚空传送门：召唤小怪
};

// ============================================================
// 基类：处理连续召唤逻辑
// ============================================================
struct npc_cultivation_chain_boss_base : public ScriptedAI
{
    npc_cultivation_chain_boss_base(Creature* creature) : ScriptedAI(creature) {}

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

        // 5秒后召唤下一个Boss
        uint32 nextEntry = 390100 + bossIndex + 1;
        float x = me->GetPositionX();
        float y = me->GetPositionY();
        float z = me->GetPositionZ();
        float o = me->GetOrientation();

        me->m_Events.AddEventAtOffset([nextEntry, x, y, z, o, killer]()
        {
            if (!killer || !killer->IsInWorld())
                return;

            Player* p = killer->ToPlayer();
            if (!p)
                p = killer->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (!p || !p->IsInWorld())
                return;

            if (Creature* next = p->SummonCreature(nextEntry, x, y, z, o,
                TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300000))
            {
                next->SetInCombatWith(p);
                next->AddThreat(p, 1000.0f);
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
                case 1: DoCastVictim(SPELL_FIRE_BOLT);    events.Repeat(3s); break;
                case 2: DoCastAOE(SPELL_FIRE_NOVA);       events.Repeat(12s); break;
                case 3: DoCast(me, SPELL_FIRE_SHIELD);    events.Repeat(30s); break;
                case 4: DoCastVictim(SPELL_METEOR);       events.Repeat(18s); break;
                case 5: DoCastVictim(SPELL_IGNITE);       events.Repeat(14s); break;
                case 6: DoCastAOE(SPELL_INFERNO);         events.Repeat(25s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_FROST_BOLT);   events.Repeat(3s); break;
                case 2: DoCastAOE(SPELL_BLIZZARD);        events.Repeat(15s); break;
                case 3: DoCast(me, SPELL_FROST_ARMOR);    events.Repeat(35s); break;
                case 4: DoCastVictim(SPELL_ICE_LANCE);    events.Repeat(8s); break;
                case 5: DoCastVictim(SPELL_FROZEN_ORB);   events.Repeat(18s); break;
                case 6: DoCastVictim(SPELL_DEEP_FREEZE);  events.Repeat(22s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_SHADOW_BOLT);  events.Repeat(3s); break;
                case 2: DoCastAOE(SPELL_SHADOW_NOVA);     events.Repeat(14s); break;
                case 3: DoCastVictim(SPELL_DRAIN_LIFE);   events.Repeat(10s); break;
                case 4: DoCastVictim(SPELL_CURSE_AGONY);  events.Repeat(12s); break;
                case 5: DoCastAOE(SPELL_SHADOW_FURY);     events.Repeat(18s); break;
                case 6: DoCast(me, SPELL_DARK_PACT);      events.Repeat(25s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_WRATH);            events.Repeat(3s); break;
                case 2: DoCastAOE(SPELL_HURRICANE);           events.Repeat(16s); break;
                case 3: DoCast(me, SPELL_THORNS);             events.Repeat(30s); break;
                case 4: DoCastVictim(SPELL_ENTANGLE);         events.Repeat(12s); break;
                case 5: DoCastAOE(SPELL_STARFALL);            events.Repeat(22s); break;
                case 6: DoCast(me, SPELL_REJUVENATE_BOSS);    events.Repeat(20s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_SMITE);            events.Repeat(3s); break;
                case 2: DoCastAOE(SPELL_HOLY_NOVA_BOSS);      events.Repeat(11s); break;
                case 3: DoCast(me, SPELL_DIVINE_SHIELD);      events.Repeat(40s); break;
                case 4: DoCastVictim(SPELL_HAMMER_JUSTICE);    events.Repeat(10s); break;
                case 5: DoCastAOE(SPELL_CONSECRATION);        events.Repeat(15s); break;
                case 6: DoCastAOE(SPELL_HOLY_WRATH);          events.Repeat(20s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_ARCANE_BLAST);     events.Repeat(3s); break;
                case 2: DoCastAOE(SPELL_ARCANE_EXPLOSION);    events.Repeat(10s); break;
                case 3: DoCast(me, SPELL_MANA_SHIELD);        events.Repeat(30s); break;
                case 4: DoCastVictim(SPELL_ARCANE_MISSILES);  events.Repeat(8s); break;
                case 5: DoCastVictim(SPELL_COUNTERSPELL_BOSS);events.Repeat(16s); break;
                case 6: DoCastAOE(SPELL_ARCANE_BARRAGE);      events.Repeat(12s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_MORTAL_STRIKE);    events.Repeat(6s); break;
                case 2: DoCastAOE(SPELL_WHIRLWIND_BOSS);      events.Repeat(10s); break;
                case 3: DoCast(me, SPELL_BATTLE_SHOUT);       events.Repeat(30s); break;
                case 4: DoCastVictim(SPELL_CHARGE_BOSS);      events.Repeat(15s); break;
                case 5: DoCastAOE(SPELL_THUNDER_CLAP);        events.Repeat(14s); break;
                case 6: DoCastVictim(SPELL_EXECUTE_BOSS);     events.Repeat(12s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_POISON_BOLT);      events.Repeat(4s); break;
                case 2: DoCastAOE(SPELL_POISON_CLOUD);        events.Repeat(16s); break;
                case 3: DoCast(me, SPELL_ENVENOM);            events.Repeat(25s); break;
                case 4: DoCastVictim(SPELL_CRIPPLING_POISON); events.Repeat(10s); break;
                case 5: DoCastAOE(SPELL_VENOM_SPIT);          events.Repeat(12s); break;
                case 6: DoCastVictim(SPELL_DEADLY_POISON);    events.Repeat(8s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_LIGHTNING_BOLT);   events.Repeat(3s); break;
                case 2: DoCastVictim(SPELL_CHAIN_LIGHTNING);  events.Repeat(9s); break;
                case 3: DoCast(me, SPELL_LIGHTNING_SHIELD);   events.Repeat(28s); break;
                case 4: DoCastAOE(SPELL_THUNDERSTORM);        events.Repeat(18s); break;
                case 5: DoCastVictim(SPELL_STORMSTRIKE);      events.Repeat(7s); break;
                case 6: DoCastVictim(SPELL_EARTH_SHOCK);      events.Repeat(12s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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
                case 1: DoCastVictim(SPELL_CHAOS_BOLT);       events.Repeat(4s); break;
                case 2: DoCastAOE(SPELL_VOID_ZONE);           events.Repeat(14s); break;
                case 3: DoCast(me, SPELL_BERSERK_BOSS);       events.Repeat(45s); break;
                case 4: DoCastVictim(SPELL_SOUL_FIRE);        events.Repeat(8s); break;
                case 5: DoCastAOE(SPELL_RAIN_OF_CHAOS);       events.Repeat(20s); break;
                case 6: DoCastAOE(SPELL_NETHER_PORTAL);       events.Repeat(30s); break;
            }
        }
        DoMeleeAttackIfReady();
    }

    EventMap events;
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

ALTER TABLE `item_template`
  MODIFY `stat_value1` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value2` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value3` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value4` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value5` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value6` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value7` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value8` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value9` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `stat_value10` decimal(39,0) NOT NULL DEFAULT '0',
  MODIFY `armor` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `creature`
  MODIFY `curhealth` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `curmana` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `player_class_stats`
  MODIFY `BaseHP` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `BaseMana` decimal(39,0) unsigned NOT NULL DEFAULT '1';

ALTER TABLE `creature_classlevelstats`
  MODIFY `basehp0` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `basehp1` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `basehp2` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `basemana` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `attackpower` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `rangedattackpower` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `pet_levelstats`
  MODIFY `hp` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `mana` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_属性调整_生物`
  MODIFY `护甲值` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_幻境生物属性`
  MODIFY `护甲值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `攻击强度值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `法术强度值` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_任务奖励属性`
  MODIFY `每任务奖励` decimal(39,0) NOT NULL DEFAULT '1' COMMENT '每完成一个任务奖励的该属性点数',
  MODIFY `最大值` decimal(39,0) NOT NULL DEFAULT '0' COMMENT '该属性的最大值限制，0表示无限制';

ALTER TABLE `_属性调整_生物`
  MODIFY `血量值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `魔法值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `抗性值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `物理攻击最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `物理攻击最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `魔法攻击最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `魔法攻击最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `DOT攻击最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `DOT攻击最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `治疗最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `治疗最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `真实伤害值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '仅对配置过的生物生效，近战附加真实伤害，无视玩家护甲并强制命中',
  MODIFY `被攻击掉血上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '被攻击掉血上限',
  MODIFY `物理攻击切割伤害值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '公式：自身物理伤害 + 切割伤害',
  MODIFY `魔法攻击切割伤害值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '公式：自身魔法伤害 + 切割伤害',
  MODIFY `DOT攻击切割伤害值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '公式：自身魔法伤害 + 切割伤害';

ALTER TABLE `_切割系统`
  MODIFY `切割伤害` decimal(43,4) NOT NULL DEFAULT '0' COMMENT '固定值直接生效；百分比按目标当前血量计算，最大100';

ALTER TABLE `_回血神符`
  MODIFY `血蓝值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '自定义模式时写每秒恢复点数，百分比模式时写每秒恢复百分比，最高 100；怒气/能量/符文能量按 10:1 恢复';

ALTER TABLE `_图鉴系统`
  MODIFY `固定全属性值` decimal(39,0) NOT NULL DEFAULT '0' COMMENT '仅在属性生效模式=0时生效；每激活一个图鉴，为力量/敏捷/耐力/智力/精神各增加该数值';

ALTER TABLE `_宣传奖励系统`
  MODIFY `初始全属性值` decimal(39,0) NOT NULL DEFAULT '1999' COMMENT '宣传1天时的全属性数值',
  MODIFY `每日增量` decimal(39,0) NOT NULL DEFAULT '1000' COMMENT '每多1天宣传增加的全属性';

ALTER TABLE `_属性调整_宠物`
  MODIFY `物理伤害上限` bigint unsigned NOT NULL DEFAULT '0' COMMENT '普通伤害上限',
  MODIFY `魔法伤害上限` bigint unsigned NOT NULL DEFAULT '0' COMMENT '技能伤害上限',
  MODIFY `治疗伤害上限` bigint unsigned NOT NULL DEFAULT '0' COMMENT '治疗伤害上限';

ALTER TABLE `_属性调整_职业`
  MODIFY `力量下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `力量上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '力量上限数值',
  MODIFY `敏捷下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `敏捷上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '敏捷上限数值',
  MODIFY `智力下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `智力上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '智力上限数值',
  MODIFY `精神下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `精神上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '精神上限数值',
  MODIFY `耐力下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `耐力上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '耐力上限数值',
  MODIFY `法强下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `法强上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '法强上限数值',
  MODIFY `攻强下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `攻强上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '近战攻强上限数值',
  MODIFY `远程攻强下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `远程攻强上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '远程攻强上限数值',
  MODIFY `治疗下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `治疗上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '治疗上限数值',
  MODIFY `远程命中上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '远程命中等级上限',
  MODIFY `法术命中下限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '法术命中等级下限',
  MODIFY `法术命中上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '法术命中等级上限 (0=无限制, 建议设置为防止溢出)',
  MODIFY `韧性下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `韧性上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '韧性上限',
  MODIFY `护甲下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `护甲上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '护甲上限',
  MODIFY `精准下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `精准上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '精准上限',
  MODIFY `血量下限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '血量下限',
  MODIFY `血量上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '血量上限',
  MODIFY `法力下限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '法力下限',
  MODIFY `法力上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '法力上限',
  MODIFY `急速下限` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `急速上限` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '急速上限';

ALTER TABLE `_幻境生物属性`
  MODIFY `血量值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '血量固定值(0=使用倍率,>0=使用固定值)',
  MODIFY `法力值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '法力固定值(0=使用倍率,>0=使用固定值)',
  MODIFY `战力门槛` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '0=按500级毕业预算曲线自动计算,>0=本配置固定达标战力';

ALTER TABLE `_深渊装备模板`
  MODIFY `主属性预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '主属性预算最小值',
  MODIFY `主属性预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '主属性预算最大值',
  MODIFY `次属性预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '次属性预算最小值',
  MODIFY `次属性预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '次属性预算最大值',
  MODIFY `特效预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '特效预算最小值',
  MODIFY `特效预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '特效预算最大值';

ALTER TABLE `_深渊套装配置`
  MODIFY `2件全属性加成` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '2件全属性加值',
  MODIFY `2件暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '2件暴击等级加值',
  MODIFY `2件急速等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '2件急速等级加值',
  MODIFY `2件攻击强度` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '2件攻击强度加值',
  MODIFY `2件法术强度` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '2件法术强度加值',
  MODIFY `4件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '4件全伤害加成%',
  MODIFY `4件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '4件最大生命值加成%',
  MODIFY `4件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '4件额外暴击等级',
  MODIFY `4件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '4件额外急速等级',
  MODIFY `6件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '6件全伤害加成%',
  MODIFY `6件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '6件最大生命值加成%',
  MODIFY `6件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '6件额外暴击等级',
  MODIFY `6件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '6件额外急速等级',
  MODIFY `8件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '8件全伤害加成%',
  MODIFY `8件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '8件最大生命值加成%',
  MODIFY `8件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '8件额外暴击等级',
  MODIFY `8件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '8件额外急速等级';

ALTER TABLE `_物品鉴定_模板`
  MODIFY `成长属性最小属性值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '成长属性单条最小属性值',
  MODIFY `成长属性最大属性值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '成长属性单条最大属性值',
  MODIFY `强化属性最小属性值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '强化属性单条最小属性值',
  MODIFY `强化属性最大属性值` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '强化属性单条最大属性值',
  MODIFY `基础最小属性值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `基础最大属性值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `追加属性最小值` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `追加属性最大值` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_物品属性_模板`
  MODIFY `属性最小百分比` decimal(39,0) NOT NULL DEFAULT '100' COMMENT '属性值的最小百分比',
  MODIFY `属性最大百分比` decimal(39,0) NOT NULL DEFAULT '100' COMMENT '属性值的最大百分比';

ALTER TABLE `_物品强化_模板`
  MODIFY `属性值1` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小值',
  MODIFY `属性值2` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大值',
  MODIFY `属性百分比1` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小百分比',
  MODIFY `属性百分比2` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大百分比',
  MODIFY `达到该强化等级后的奖励属性` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `达到该强化等级后的奖励属性百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_物品成长_模板`
  MODIFY `属性值1` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小值',
  MODIFY `属性值2` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大值',
  MODIFY `属性百分比1` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小百分比',
  MODIFY `属性百分比2` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大百分比',
  MODIFY `达到该成长等级后的奖励属性` decimal(39,0) unsigned NOT NULL DEFAULT '0',
  MODIFY `达到该成长等级后的奖励属性百分比` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_武魂系统_模板`
  MODIFY `激活魂力消耗` decimal(39,0) unsigned NOT NULL DEFAULT '1000' COMMENT '激活武魂分身所需魂力',
  MODIFY `进化魂力消耗` decimal(39,0) unsigned NOT NULL DEFAULT '0' COMMENT '进化到该模板所需魂力，模板1为0',
  MODIFY `副本Boss魂力` decimal(39,0) unsigned NOT NULL DEFAULT '1',
  MODIFY `世界Boss魂力` decimal(39,0) unsigned NOT NULL DEFAULT '1';

ALTER TABLE `_武魂系统_魂环`
  MODIFY `魂力消耗` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_武魂系统_装备`
  MODIFY `魂力消耗` decimal(39,0) unsigned NOT NULL DEFAULT '0';

ALTER TABLE `_武魂系统_技能`
  MODIFY `学习消耗` decimal(39,0) unsigned NOT NULL DEFAULT '100',
  MODIFY `升级消耗` decimal(39,0) unsigned NOT NULL DEFAULT '100';

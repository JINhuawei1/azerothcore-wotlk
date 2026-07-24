-- 挑战幻境：古达克仙门试炼

SET @MIRAGE_LEVEL := 10001;

DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_gundrak_template`;
CREATE TEMPORARY TABLE `_tmp_mirage_gundrak_template` (
  `entry` int unsigned NOT NULL PRIMARY KEY,
  `source_entry` int unsigned NOT NULL,
  `name` varchar(100) NOT NULL,
  `script` varchar(64) NOT NULL,
  `is_boss` tinyint unsigned NOT NULL DEFAULT 0,
  `health_modifier` double NOT NULL DEFAULT 1,
  `damage_modifier` double NOT NULL DEFAULT 1,
  `rank_value` tinyint unsigned NOT NULL DEFAULT 1,
  `scale_value` float NOT NULL DEFAULT 1
) ENGINE=MEMORY;

INSERT INTO `_tmp_mirage_gundrak_template`
  (`entry`, `source_entry`, `name`, `script`, `is_boss`, `health_modifier`, `damage_modifier`, `rank_value`, `scale_value`) VALUES
(800001, 29304, '仙门试炼·毒牙尊者', 'npc_challenge_mirage_gundrak_venom', 1, 260, 28, 3, 3.00),
(800002, 29307, '仙门试炼·魔晶巨像', 'npc_challenge_mirage_gundrak_colossus', 1, 320, 32, 3, 3.00),
(800003, 29305, '仙门试炼·兽魂长老', 'npc_challenge_mirage_gundrak_beast', 1, 300, 34, 3, 3.00),
(800004, 29306, '仙门试炼·荒神先知', 'npc_challenge_mirage_gundrak_prophet', 1, 340, 36, 3, 3.00),
(800005, 29932, '仙门试炼·深渊伊克', 'npc_challenge_mirage_gundrak_abyss', 1, 420, 42, 3, 3.00),
(800011, 29774, '仙门毒鳞蛇', 'npc_challenge_mirage_gundrak_minion', 0, 22, 10, 1, 1.00),
(800012, 29768, '仙门缠身巨蟒', 'npc_challenge_mirage_gundrak_minion', 0, 26, 11, 1, 1.05),
(800013, 29819, '仙门达卡莱长枪卫', 'npc_challenge_mirage_gundrak_minion', 0, 30, 12, 1, 1.00),
(800014, 29820, '仙门达卡莱猎神者', 'npc_challenge_mirage_gundrak_minion', 0, 28, 12, 1, 1.00),
(800015, 29822, '仙门达卡莱织火者', 'npc_challenge_mirage_gundrak_minion', 0, 24, 13, 1, 1.00),
(800016, 29826, '仙门达卡莱医师', 'npc_challenge_mirage_gundrak_minion', 0, 24, 10, 1, 1.00),
(800017, 29830, '仙门活体魔精', 'npc_challenge_mirage_gundrak_minion', 0, 24, 12, 1, 1.00),
(800018, 29838, '仙门荒蹄犀牛', 'npc_challenge_mirage_gundrak_minion', 0, 34, 13, 1, 1.08);

DELETE FROM `_挑战幻境生物`
WHERE `生物组` = @MIRAGE_LEVEL
   OR `生物Entry` BETWEEN 800000 AND 800099;

DELETE FROM `_挑战幻境等级`
WHERE `等级` = @MIRAGE_LEVEL;

DELETE FROM `_属性调整_生物`
WHERE `id` BETWEEN 800000 AND 800099
   OR `生物id` BETWEEN 800000 AND 800099;

DELETE FROM `creature`
WHERE `guid` BETWEEN 5460001 AND 5460100
   OR `id1` BETWEEN 800000 AND 800099
   OR `id2` BETWEEN 800000 AND 800099
   OR `id3` BETWEEN 800000 AND 800099;
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 800000 AND 800099;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 800000 AND 800099;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 800000 AND 800099;

INSERT INTO `_挑战幻境等级`
  (`等级`, `名称`, `需求物品`, `需求数量`, `生物组`, `持续秒数`, `备注`)
VALUES
  (@MIRAGE_LEVEL, '古达克仙门试炼', 0, 0, @MIRAGE_LEVEL, 0, '自定义副本：古达克地图604，挑战幻境隔离层10001');

INSERT INTO `creature_template`
  (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`,
   `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
   `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`,
   `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`,
   `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `trainer_class`,
   `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`,
   `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`,
   `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`,
   `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  t.`entry`, 0, 0, 0, 0, 0,
  t.`name`, CASE WHEN t.`is_boss` = 1 THEN '仙门试炼首领' ELSE '仙门试炼守卫' END, src.`IconName`, 0,
  CASE WHEN t.`is_boss` = 1 THEN 83 ELSE 80 END,
  CASE WHEN t.`is_boss` = 1 THEN 83 ELSE 80 END,
  src.`exp`, 16, 0,
  src.`speed_walk`, src.`speed_run`, src.`speed_swim`, src.`speed_flight`,
  CASE WHEN t.`is_boss` = 1 THEN 60 ELSE 35 END,
  t.`scale_value`, t.`rank_value`, src.`dmgschool`, t.`damage_modifier`,
  src.`BaseAttackTime`, src.`RangeAttackTime`, src.`BaseVariance`, src.`RangeVariance`, src.`unit_class`,
  src.`unit_flags`, src.`unit_flags2`, 0, src.`family`, 0, 0, 0, 0,
  src.`type`, src.`type_flags`, t.`entry`, 0, 0, 0, 0,
  CASE WHEN t.`is_boss` = 1 THEN 5000000 ELSE 50000 END,
  CASE WHEN t.`is_boss` = 1 THEN 8000000 ELSE 150000 END,
  '', 0, src.`HoverHeight`, t.`health_modifier`, src.`ManaModifier`,
  src.`ArmorModifier`, 1, 0, 0, 0,
  CASE WHEN t.`is_boss` = 1 THEN 650854271 ELSE src.`mechanic_immune_mask` END,
  src.`spell_school_immune_mask`, src.`flags_extra`, t.`script`, 12340
FROM `_tmp_mirage_gundrak_template` t
INNER JOIN `creature_template` src ON src.`entry` = t.`source_entry`;

REPLACE INTO `creature_template_model`
  (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  t.`entry`, m.`Idx`, m.`CreatureDisplayID`, CASE WHEN t.`is_boss` = 1 THEN 3.0 ELSE m.`DisplayScale` END, m.`Probability`, 12340
FROM `_tmp_mirage_gundrak_template` t
INNER JOIN `creature_template_model` m ON m.`CreatureID` = t.`source_entry`;

INSERT INTO `_挑战幻境生物`
  (`生物组`, `生物Entry`, `地图`, `区域`, `坐标X`, `坐标Y`, `坐标Z`, `朝向O`, `数量`, `刷新秒数`, `游荡距离`, `备注`)
VALUES
(@MIRAGE_LEVEL, 800001, 604, 0, 1775.13, 674.981, 129.3, 1.67552, 1, 600, 0, '古达克仙门试炼Boss1：斯拉德兰位置'),
(@MIRAGE_LEVEL, 800002, 604, 0, 1672.96, 743.488, 143.338, 3.12414, 1, 600, 0, '古达克仙门试炼Boss2：达卡莱巨像位置'),
(@MIRAGE_LEVEL, 800003, 604, 0, 1772.47, 809.537, 129.3, 4.72984, 1, 600, 0, '古达克仙门试炼Boss3：莫拉比位置'),
(@MIRAGE_LEVEL, 800004, 604, 0, 1914.75, 743.654, 136.579, 0.017453, 1, 600, 0, '古达克仙门试炼Boss4：迦尔达拉位置'),
(@MIRAGE_LEVEL, 800005, 604, 0, 1642.712, 934.646, 107.205, 0.767, 1, 900, 0, '古达克仙门试炼Boss5：官方Eck位置'),
(@MIRAGE_LEVEL, 800011, 604, 0, 1690.23, 639.766, 126.83, 2.89725, 1, 300, 4, '古达克仙门试炼小怪：毒鳞蛇'),
(@MIRAGE_LEVEL, 800012, 604, 0, 1696.84, 632.797, 128.934, 6.24828, 1, 300, 4, '古达克仙门试炼小怪：缠身巨蟒'),
(@MIRAGE_LEVEL, 800011, 604, 0, 1781.69, 648.869, 124.469, 5.43348, 1, 300, 4, '古达克仙门试炼小怪：毒鳞蛇'),
(@MIRAGE_LEVEL, 800012, 604, 0, 1802.6, 638.811, 129.116, 3.21141, 1, 300, 4, '古达克仙门试炼小怪：缠身巨蟒'),
(@MIRAGE_LEVEL, 800017, 604, 0, 1634.21, 760.221, 142.794, 1.57854, 1, 300, 4, '古达克仙门试炼小怪：活体魔精'),
(@MIRAGE_LEVEL, 800017, 604, 0, 1624.94, 762.231, 143.036, 4.69548, 1, 300, 4, '古达克仙门试炼小怪：活体魔精'),
(@MIRAGE_LEVEL, 800018, 604, 0, 1576.46, 718.917, 143.152, 1.65806, 1, 300, 4, '古达克仙门试炼小怪：荒蹄犀牛'),
(@MIRAGE_LEVEL, 800013, 604, 0, 1747.83, 839.048, 129.292, 0.034907, 1, 300, 4, '古达克仙门试炼小怪：长枪卫'),
(@MIRAGE_LEVEL, 800015, 604, 0, 1747.6, 849.333, 129.286, 0.017453, 1, 300, 4, '古达克仙门试炼小怪：织火者'),
(@MIRAGE_LEVEL, 800016, 604, 0, 1747.66, 858.003, 129.283, 0.0, 1, 300, 4, '古达克仙门试炼小怪：医师'),
(@MIRAGE_LEVEL, 800014, 604, 0, 1669.13, 867.701, 137.364, 3.33358, 1, 300, 4, '古达克仙门试炼小怪：猎神者'),
(@MIRAGE_LEVEL, 800013, 604, 0, 1797.61, 855.951, 129.284, 3.10669, 1, 300, 4, '古达克仙门试炼小怪：长枪卫'),
(@MIRAGE_LEVEL, 800015, 604, 0, 1797.65, 846.981, 129.288, 3.15905, 1, 300, 4, '古达克仙门试炼小怪：织火者'),
(@MIRAGE_LEVEL, 800018, 604, 0, 1706.76, 857.179, 129.981, 0.845717, 1, 300, 4, '古达克仙门试炼小怪：荒蹄犀牛'),
(@MIRAGE_LEVEL, 800014, 604, 0, 1881.24, 799.22, 169.85, 1.15, 1, 300, 4, '古达克仙门试炼小怪：猎神者'),
(@MIRAGE_LEVEL, 800016, 604, 0, 1898.08, 816.44, 176.2, 4.65, 1, 300, 4, '古达克仙门试炼小怪：医师'),
(@MIRAGE_LEVEL, 800017, 604, 0, 1884.6, 845.12, 176.6, 5.88, 1, 300, 4, '古达克仙门试炼小怪：活体魔精');

INSERT INTO `_属性调整_生物`
  (`id`, `生物id`, `注释`, `组`, `组内随机几率`, `等级`,
   `移动速度`, `移动速度百分比`, `攻击间隔`, `攻击间隔百分比`,
   `血量值`, `血量百分比`, `魔法值`, `魔法百分比`,
   `护甲值`, `护甲百分比`, `抗性值`, `抗性百分比`,
   `物理攻击最小值`, `物理攻击最大值`, `物理攻击百分比`,
   `魔法攻击最小值`, `魔法攻击最大值`, `魔法攻击百分比`,
   `DOT攻击最小值`, `DOT攻击最大值`, `DOT攻击百分比`,
   `治疗最小值`, `治疗最大值`, `治疗百分比`,
   `真实伤害值`, `物理受伤百分比`, `魔法受伤百分比`, `被攻击掉血上限`,
   `物理攻击切割伤害值`, `物理攻击切割伤害百分比`,
   `魔法攻击切割伤害值`, `魔法攻击切割伤害百分比`,
   `DOT攻击切割伤害值`, `DOT攻击切割伤害百分比`,
   `是否可以被切割`, `离开原位置重置距离`,
   `随机移动距离_多个随机逗号隔开`, `刷新后自动移动到坐标`,
   `_自定义Ai_组_多个随机逗号分开`, `_物品_鉴定_组`,
   `官方掉落ID_多个逗号隔开`, `官方掉落ID_多个随机逗号隔开`, `是否加载原掉落`,
   `模型id`, `主手模型id_多个随机逗号隔开`, `副手模型id_多个随机逗号隔开`, `远程模型id_多个随机逗号隔开`,
   `模型大小倍率`, `生物类型`, `击杀奖励`, `击杀奖励几率`,
   `击杀队长奖励`, `击杀队长奖励几率`, `击杀队伍奖励`, `击杀队伍奖励几率`,
   `击杀弹窗`, `击杀公告`, `刷新公告`, `血条数量`, `生物描述`)
SELECT
  t.`entry`, t.`entry`, CONCAT('古达克仙门试炼：', t.`name`), 0, 1, 0,
  0, 100, 0, 100,
  CASE WHEN t.`is_boss` = 1
    THEN CAST('100000000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('10000000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('1000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('100000000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  100,
  0, 100,
  0, CASE WHEN t.`is_boss` = 1 THEN 300 ELSE 160 END,
  0, CASE WHEN t.`is_boss` = 1 THEN 260 ELSE 140 END,
  CASE WHEN t.`is_boss` = 1
    THEN CAST('500000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('100000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('8000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('1000000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  CASE WHEN t.`is_boss` = 1
    THEN CAST('1000000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('200000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('16000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('2000000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  100,
  CASE WHEN t.`is_boss` = 1
    THEN CAST('400000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('100000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('6000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('800000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  CASE WHEN t.`is_boss` = 1
    THEN CAST('800000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('150000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('12000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('1500000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  100,
  CASE WHEN t.`is_boss` = 1
    THEN CAST('300000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('80000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('3000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('500000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  CASE WHEN t.`is_boss` = 1
    THEN CAST('600000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('120000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('8000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('1000000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  100,
  0, 0, 100,
  CASE WHEN t.`is_boss` = 1
    THEN CAST('500000000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800001) * CAST('100000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('5000000000000000000000000000000' AS DECIMAL(65,0)) + (t.`entry` - 800011) * CAST('1000000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  CASE WHEN t.`is_boss` = 1 THEN 5 ELSE 15 END,
  CASE WHEN t.`is_boss` = 1 THEN 5 ELSE 15 END,
  CASE WHEN t.`is_boss` = 1
    THEN CAST('10000000000000000000000000000000000000' AS DECIMAL(65,0))
    ELSE CAST('20000000000000000000000000000000' AS DECIMAL(65,0))
  END / CAST(1000000 AS DECIMAL(65,0)),
  0, 0, 0, 0, 0, 0,
  1, CASE WHEN t.`is_boss` = 1 THEN 150 ELSE 80 END,
  '', '', '', 0,
  '', '', '否',
  NULL, NULL, NULL, NULL,
  t.`scale_value`, CASE WHEN t.`is_boss` = 1 THEN 3 ELSE 0 END, 0, 100,
  0, 100, 0, 100,
  0, CASE WHEN t.`is_boss` = 1 THEN 1 ELSE 0 END, 0,
  CASE WHEN t.`is_boss` = 1 THEN '50' ELSE '100000' END,
  ''
FROM `_tmp_mirage_gundrak_template` t;

INSERT INTO `creature_loot_template`
  (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
(800001, 0, 952200, 100, 0, 1, 0, 3, 3, '古达克仙门试炼Boss1随机1万池'),
(800002, 0, 952200, 100, 0, 1, 0, 3, 3, '古达克仙门试炼Boss2随机1万池'),
(800003, 0, 952200, 100, 0, 1, 0, 3, 3, '古达克仙门试炼Boss3随机1万池'),
(800004, 0, 952200, 100, 0, 1, 0, 3, 3, '古达克仙门试炼Boss4随机1万池'),
(800005, 0, 952200, 100, 0, 1, 0, 3, 3, '古达克仙门试炼Boss5随机1万池'),
(800001, 62002, 0, 100, 0, 1, 0, 5, 5, '古达克仙门试炼Boss1必掉突破石x5'),
(800002, 62002, 0, 100, 0, 1, 0, 5, 5, '古达克仙门试炼Boss2必掉突破石x5'),
(800003, 62002, 0, 100, 0, 1, 0, 5, 5, '古达克仙门试炼Boss3必掉突破石x5'),
(800004, 62002, 0, 100, 0, 1, 0, 5, 5, '古达克仙门试炼Boss4必掉突破石x5'),
(800005, 62002, 0, 100, 0, 1, 0, 5, 5, '古达克仙门试炼Boss5必掉突破石x5'),
(800001, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼Boss1必掉灵气石x10'),
(800002, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼Boss2必掉灵气石x10'),
(800003, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼Boss3必掉灵气石x10'),
(800004, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼Boss4必掉灵气石x10'),
(800005, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼Boss5必掉灵气石x10'),
(800011, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800012, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800013, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800014, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800015, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800016, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800017, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10'),
(800018, 62001, 0, 100, 0, 1, 0, 10, 10, '古达克仙门试炼小怪必掉灵气石x10');

DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_gundrak_template`;

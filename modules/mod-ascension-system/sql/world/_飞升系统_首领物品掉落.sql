-- 飞升系统：18个首领、18个槽位物品、18条解锁需求、掉落表、3倍人形模型、神器武器外观
-- 说明：
-- 1. 本文件只创建 creature_template / creature_template_model / creature_equip_template / creature_loot_template / item_template / _模板_需求 / _深渊装备模板 数据
-- 2. 不创建 creature 刷新点；如需落地到世界地图，请后续补充 creature 坐标
-- 3. 飞升系统通过 _深渊装备模板 识别“只能装备到飞升槽”的物品，因此 18 个槽位物品会同步写入该表

DROP TEMPORARY TABLE IF EXISTS `_tmp_ascension_slot_defs`;
CREATE TEMPORARY TABLE `_tmp_ascension_slot_defs` (
  `slot_id` tinyint unsigned NOT NULL,
  `slot_name` varchar(32) NOT NULL,
  `boss_entry` int unsigned NOT NULL,
  `item_entry` int unsigned NOT NULL,
  `requirement_id` int unsigned NOT NULL,
  `slot_mask` int unsigned NOT NULL,
  `item_class` tinyint unsigned NOT NULL,
  `item_subclass` tinyint unsigned NOT NULL,
  `inventory_type` tinyint unsigned NOT NULL,
  `item_displayid` int unsigned NOT NULL,
  `boss_displayid` int unsigned NOT NULL,
  `boss_weapon_main` int unsigned NOT NULL,
  `boss_weapon_off` int unsigned NOT NULL,
  `boss_weapon_ranged` int unsigned NOT NULL,
  `boss_health` bigint unsigned NOT NULL,
  `boss_damage_modifier` int unsigned NOT NULL,
  `stat_budget` int unsigned NOT NULL,
  PRIMARY KEY (`slot_id`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4;

INSERT INTO `_tmp_ascension_slot_defs`
(`slot_id`, `slot_name`, `boss_entry`, `item_entry`, `requirement_id`, `slot_mask`, `item_class`, `item_subclass`, `inventory_type`, `item_displayid`, `boss_displayid`, `boss_weapon_main`, `boss_weapon_off`, `boss_weapon_ranged`, `boss_health`, `boss_damage_modifier`, `stat_budget`) VALUES
(0,  '头部',   399801, 998801, 939801,   2,   4, 4,  1, 33822, 30190, 32837, 32837,     0, 1000000000, 30, 45),
(1,  '颈部',   399802, 998802, 939802, 128,   4, 0,  2, 64190, 30191, 50732, 29458,     0, 1100000000, 34, 35),
(2,  '肩部',   399803, 998803, 939803,   4,   4, 4,  3, 65000, 30193, 50734, 32375,     0, 1200000000, 38, 40),
(3,  '衬衣',   399804, 998804, 939804,   4,   4, 0,  4, 61552, 30194, 32374,     0,     0, 1300000000, 42, 25),
(4,  '胸甲',   399805, 998805, 939805,   4,   4, 4,  5, 45017, 30196, 50730,     0,     0, 1400000000, 46, 50),
(5,  '腰带',   399806, 998806, 939806,  16,   4, 4,  6, 61962, 30197, 47522,     0,     0, 1500000000, 50, 35),
(6,  '腿部',   399807, 998807, 939807,   4,   4, 4,  7, 47991, 30198, 51454, 51455,     0, 1600000000, 54, 45),
(7,  '脚部',   399808, 998808, 939808,  32,   4, 4,  8, 64789, 30199, 51515, 51518,     0, 1700000000, 58, 35),
(8,  '手腕',   399809, 998809, 939809,   8,   4, 4,  9, 64345, 30200, 47422, 47422,     0, 1800000000, 62, 30),
(9,  '手套',   399810, 998810, 939810,   8,   4, 4, 10, 64578, 30201, 47500, 50729,     0, 1900000000, 66, 35),
(10, '戒指1',  399811, 998811, 939811,  64,   4, 0, 11, 64176, 30202, 34331,     0,     0, 2000000000, 70, 28),
(11, '戒指2',  399812, 998812, 939812,  64,   4, 0, 11, 39126, 30071, 45533,     0,     0, 2100000000, 74, 28),
(12, '饰品1',  399813, 998813, 939813, 128,   4, 0, 12, 59316, 30072, 50040,     0,     0, 2200000000, 78, 30),
(13, '饰品2',  399814, 998814, 939814, 128,   4, 0, 12, 42609, 30073, 50731,     0,     0, 2300000000, 82, 30),
(14, '披风',   399815, 998815, 939815, 256,   4, 0, 16, 64648, 30310, 46017, 47079,     0, 2400000000, 86, 35),
(15, '主手',   399816, 998816, 939816,   1,   2, 7, 21, 64530, 30311, 50761,     0,     0, 2500000000, 90, 55),
(16, '副手',   399817, 998817, 939817, 512,   4, 6, 14, 65029, 30259, 40396, 48013,     0, 2600000000, 94, 45),
(17, '远程',   399818, 998818, 939818,   1,   2, 3, 26, 64366, 30260, 32450,     0, 34530, 2700000000, 98, 45);

-- 清理本文件使用的专属号段，确保重复导入安全
DELETE FROM `creature_loot_template`
WHERE `Entry` BETWEEN 399801 AND 399818;

DELETE FROM `creature_template_model`
WHERE `CreatureID` BETWEEN 399801 AND 399818;

DELETE FROM `creature_equip_template`
WHERE `CreatureID` BETWEEN 399801 AND 399818;

DELETE FROM `creature_template`
WHERE `entry` BETWEEN 399801 AND 399818;

DELETE FROM `_模板_需求`
WHERE `id` BETWEEN 939801 AND 939818;

DELETE FROM `_深渊装备模板`
WHERE `物品模板ID` BETWEEN 998801 AND 998818;

DELETE FROM `item_template`
WHERE `entry` BETWEEN 998801 AND 998818;

-- 18个飞升槽位物品模板
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `StatsCount`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`, `description`, `bonding`, `Material`, `sheath`, `MaxDurability`, `ScriptName`, `flagsCustom`, `VerifiedBuild`)
SELECT
  s.`item_entry`,
  s.`item_class`,
  s.`item_subclass`,
  -1,
  CONCAT('飞升-', s.`slot_name`),
  s.`item_displayid`,
  4,
  0,
  0,
  1,
  0,
  0,
  s.`inventory_type`,
  -1,
  -1,
  200,
  80,
  0,
  1,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  CONCAT('飞升槽位专属物品：', s.`slot_name`, '。仅用于飞升系统槽位与解锁消耗，不提供任何基础属性。'),
  1,
  CASE WHEN s.`item_class` = 2 THEN 1 ELSE 4 END,
  CASE
    WHEN s.`inventory_type` = 21 THEN 3
    WHEN s.`inventory_type` = 14 THEN 4
    WHEN s.`inventory_type` = 26 THEN 3
    ELSE 0
  END,
  CASE
    WHEN s.`inventory_type` IN (2, 11, 12, 16) THEN 0
    WHEN s.`inventory_type` = 4 THEN 50
    ELSE 100
  END,
  '',
  0,
  12340
FROM `_tmp_ascension_slot_defs` s
ORDER BY s.`slot_id`;

-- 将18个槽位物品登记为飞升系统专用物品
INSERT INTO `_深渊装备模板`
(`物品模板ID`, `装备名称`, `装备类型`, `来源章节`, `来源模式`, `部位掩码`, `幕ID`, `基础装等`, `护甲类型`, `伤害类型`, `主属性预算最小值`, `主属性预算最大值`, `次属性预算最小值`, `次属性预算最大值`, `特效预算最小值`, `特效预算最大值`, `固定词缀组`, `固定特效ID`, `是否来自秘藏首领`, `是否需要碎片`, `风味文本`, `套装ID`, `是否启用`)
SELECT
  s.`item_entry`,
  CONCAT('飞升-', s.`slot_name`),
  1,
  99,
  2,
  s.`slot_mask`,
  9,
  200,
  CASE
    WHEN s.`inventory_type` IN (1, 3, 5, 6, 7, 8, 9, 10) THEN 4
    ELSE 0
  END,
  CASE
    WHEN s.`inventory_type` IN (21, 26) THEN 1
    WHEN s.`inventory_type` = 14 THEN 3
    ELSE 0
  END,
  GREATEST(1, s.`stat_budget` - 10),
  s.`stat_budget`,
  GREATEST(1, FLOOR(s.`stat_budget` * 0.5)),
  FLOOR(s.`stat_budget` * 0.8),
  0,
  0,
  '',
  0,
  1,
  0,
  CONCAT('飞升槽位专属物品：', s.`slot_name`),
  0,
  1
FROM `_tmp_ascension_slot_defs` s
ORDER BY s.`slot_id`;

-- 18条槽位解锁需求：每个槽位均消耗100个对应的飞升物品
INSERT INTO `_模板_需求`
(`注释`, `id`, `是否消耗物品`, `消耗物品`, `客户端显示`)
SELECT
  CONCAT('飞升槽位解锁-', LPAD(s.`slot_id`, 2, '0'), '-', s.`slot_name`),
  s.`requirement_id`,
  0,
  CONCAT(s.`item_entry`, ' 100'),
  CONCAT('解锁飞升槽位【', s.`slot_name`, '】需要消耗 飞升-', s.`slot_name`, ' x100')
FROM `_tmp_ascension_slot_defs` s
ORDER BY s.`slot_id`;

-- 将18个槽位控制表绑定到新的需求模板
UPDATE `_飞升系统_控制` c
JOIN `_tmp_ascension_slot_defs` s
  ON c.`槽位` = s.`slot_id`
SET
  c.`解锁需求` = s.`requirement_id`,
  c.`备注` = CONCAT('由 Boss ', s.`boss_entry`, ' 掉落 飞升-', s.`slot_name`, '；解锁消耗飞升-', s.`slot_name`, ' x100');

-- 18个飞升Boss模板
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `type`, `type_flags`, `lootid`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  s.`boss_entry`,
  CONCAT('|cFFCC0033飞升首领', s.`slot_id` + 1, '|r'),
  CONCAT('掉落飞升-', s.`slot_name`),
  83,
  83,
  2,
  14,
  0,
  1,
  1.14286,
  30,
  1,
  3,
  0,
  s.`boss_damage_modifier`,
  2000,
  2000,
  1,
  1,
  1,
  0,
  0,
  0,
  7,
  0,
  s.`boss_entry`,
  0,
  0,
  'AggressorAI',
  0,
  1,
  ROUND(s.`boss_health` / 13945 / 5.0, 6),
  1,
  8,
  1,
  1,
  0,
  0,
  2097152,
  'npc_ascension_chain_boss',
  12340
FROM `_tmp_ascension_slot_defs` s
ORDER BY s.`slot_id`;

-- 18个Boss绑定18个不同的人形模型，大小3倍
INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  s.`boss_entry`,
  0,
  s.`boss_displayid`,
  3.0,
  1.0,
  12340
FROM `_tmp_ascension_slot_defs` s
ORDER BY s.`slot_id`;

-- 18个Boss绑定神器武器外观
INSERT INTO `creature_equip_template`
(`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
SELECT
  s.`boss_entry`,
  1,
  s.`boss_weapon_main`,
  s.`boss_weapon_off`,
  s.`boss_weapon_ranged`,
  12340
FROM `_tmp_ascension_slot_defs` s
ORDER BY s.`slot_id`;

-- 掉落表：每个Boss只掉落1个对应槽位物品
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  s.`boss_entry`,
  s.`item_entry`,
  0,
  100,
  0,
  1,
  0,
  1,
  1,
  CONCAT('飞升首领', s.`slot_id` + 1, ' -> 飞升-', s.`slot_name`)
FROM `_tmp_ascension_slot_defs` s
;

DROP TEMPORARY TABLE IF EXISTS `_tmp_ascension_slot_defs`;

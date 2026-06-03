-- 神器任务链：world 数据
-- 说明：
-- 1. 本文件只写入 item_template / quest_template / quest_template_addon / creature_template /
--    creature_template_model / creature_queststarter / creature_questender / creature_loot_template 数据。
-- 2. 不导入数据库、不生成客户端 DBC、不打包客户端补丁、不创建 creature 世界刷新点。
-- 3. 可重复导入：导入前会清理本模块专用 ID 段的数据。

SET NAMES utf8mb4;

DROP TEMPORARY TABLE IF EXISTS `_tmp_artifact_chain_defs`;
CREATE TEMPORARY TABLE `_tmp_artifact_chain_defs` (
  `rank_id` tinyint unsigned NOT NULL,
  `weapon_entry` int unsigned NOT NULL,
  `weapon_name` varchar(64) NOT NULL,
  `stat_value` decimal(65,0) NOT NULL,
  `material_entry` int unsigned NOT NULL,
  `material_name` varchar(64) NOT NULL,
  `quest_id` int unsigned NOT NULL,
  `boss_entry` int unsigned NOT NULL,
  `boss_name` varchar(64) NOT NULL,
  `boss_displayid` int unsigned NOT NULL,
  PRIMARY KEY (`rank_id`)
) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `_tmp_artifact_chain_defs`
(`rank_id`, `weapon_entry`, `weapon_name`, `stat_value`, `material_entry`, `material_name`, `quest_id`, `boss_entry`, `boss_name`, `boss_displayid`) VALUES
(1,  90001, '神器·初醒', 90000,                 90017, '初醒神核',   30001, 91002, '初醒神核守卫',   30190),
(2,  90002, '神器·玄纹', 990000,                90018, '玄铁神纹',   30002, 91003, '玄铁神纹守卫',   30191),
(3,  90003, '神器·星陨', 9990000,               90019, '星陨神砂',   30003, 91004, '星陨神砂守卫',   30193),
(4,  90004, '神器·太古', 99990000,              90020, '太古龙髓',   30004, 91005, '太古龙髓守卫',   30194),
(5,  90005, '神器·虚空', 900000000,             90021, '虚空灵晶',   30005, 91006, '虚空灵晶守卫',   30196),
(6,  90006, '神器·混沌', 9900000000,            90022, '混沌血玉',   30006, 91007, '混沌血玉守卫',   30197),
(7,  90007, '神器·雷劫', 99900000000,           90023, '雷劫天珠',   30007, 91008, '雷劫天珠守卫',   30198),
(8,  90008, '神器·日月', 999900000000,          90024, '日月圣辉',   30008, 91009, '日月圣辉守卫',   30199),
(9,  90009, '神器·乾坤', 9000000000000,         90025, '乾坤道印',   30009, 91010, '乾坤道印守卫',   30200),
(10, 90010, '神器·轮回', 99000000000000,        90026, '轮回魂火',   30010, 91011, '轮回魂火守卫',   30201),
(11, 90011, '神器·鸿蒙', 999000000000000,       90027, '鸿蒙源尘',   30011, 91012, '鸿蒙源尘守卫',   30202),
(12, 90012, '神器·无极', 9999000000000000,      90028, '无极帝骨',   30012, 91013, '无极帝骨守卫',   30071),
(13, 90013, '神器·创世', 90000000000000000,     90029, '创世神髓',   30013, 91014, '创世神髓守卫',   30072),
(14, 90014, '神器·灭界', 990000000000000000,    90030, '灭界神晶',   30014, 91015, '灭界神晶守卫',   30073),
(15, 90015, '神器·万象', 9990000000000000000,   90031, '万象天命石', 30015, 91016, '万象天命石守卫', 30310),
(16, 90016, '神器·终焉', 99990000000000000000,  90032, '终焉神格',   30016, 91017, '终焉神格守卫',   30311);

-- 清理本模块专用数据，保证重复导入安全。
DELETE FROM `creature_loot_template`
WHERE `Entry` BETWEEN 91002 AND 91017;

DELETE FROM `creature_queststarter`
WHERE `id` = 91001 AND `quest` BETWEEN 30001 AND 30016;

DELETE FROM `creature_questender`
WHERE `id` = 91001 AND `quest` BETWEEN 30001 AND 30016;

DELETE FROM `quest_template_addon`
WHERE `ID` BETWEEN 30001 AND 30016;

DELETE FROM `quest_template`
WHERE `ID` BETWEEN 30001 AND 30016;

DELETE FROM `creature_template_model`
WHERE `CreatureID` BETWEEN 91001 AND 91017;

DELETE FROM `creature_template`
WHERE `entry` BETWEEN 91001 AND 91017;

DELETE FROM `item_template`
WHERE `entry` BETWEEN 90001 AND 90032;

-- 16 阶神器武器。10 个属性槽固定为：
-- 力量(4)、敏捷(3)、耐力(7)、智力(5)、精神(6)、真实伤害(8)、切割伤害(9)、命中等级(31)、攻击强度(38)、法术强度(45)。
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
 `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,
 `maxcount`, `stackable`, `StatsCount`,
 `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`,
 `stat_type6`, `stat_value6`, `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`, `stat_type10`, `stat_value10`,
 `dmg_min1`, `dmg_max1`, `dmg_type1`,
 `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`,
 `delay`,
 `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,
 `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`,
 `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`,
 `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`,
 `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`,
 `bonding`, `description`, `Material`, `sheath`, `MaxDurability`, `ScriptName`, `flagsCustom`, `VerifiedBuild`)
SELECT
  d.`weapon_entry`,
  2,
  7,
  -1,
  d.`weapon_name`,
  30606,
  5,
  0,
  0,
  1,
  0,
  0,
  13,
  -1,
  -1,
  999,
  80,
  0,
  1,
  10,
  4,  d.`stat_value`,
  3,  d.`stat_value`,
  7,  d.`stat_value`,
  5,  d.`stat_value`,
  6,  d.`stat_value`,
  8,  d.`stat_value`,
  9,  d.`stat_value`,
  31, d.`stat_value`,
  38, d.`stat_value`,
  45, d.`stat_value`,
  10000 * POW(2, d.`rank_id` - 1),
  20000 * POW(2, d.`rank_id` - 1),
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  1900,
  381001 + (d.`rank_id` - 1) * 5 + 0, 2, 0, 0, 0, 0, 0,
  381001 + (d.`rank_id` - 1) * 5 + 1, 2, 0, 0, 0, 0, 0,
  381001 + (d.`rank_id` - 1) * 5 + 2, 2, 0, 0, 0, 0, 0,
  381001 + (d.`rank_id` - 1) * 5 + 3, 2, 0, 0, 0, 0, 0,
  381001 + (d.`rank_id` - 1) * 5 + 4, 2, 0, 0, 0, 0, 0,
  1,
  CONCAT('神器任务链第', d.`rank_id`, '阶。击中特效绑定 Spell.dbc ID ', 381001 + (d.`rank_id` - 1) * 5, '..', 381001 + (d.`rank_id` - 1) * 5 + 4, '。'),
  1,
  3,
  65535,
  'item_artifact_chain_weapon',
  0,
  12340
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

-- 16 种任务材料。每阶任务需求 100 个，对应 Boss 每次掉落 1 个。
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
 `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,
 `maxcount`, `stackable`, `StatsCount`,
 `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`,
 `bonding`, `description`, `Material`, `VerifiedBuild`)
SELECT
  d.`material_entry`,
  12,
  0,
  -1,
  d.`material_name`,
  63554,
  5,
  0,
  0,
  1,
  0,
  0,
  0,
  -1,
  -1,
  80,
  1,
  0,
  1000,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  1,
  CONCAT('神器任务链第', d.`rank_id`, '阶升级材料。需要收集 100 个。'),
  4,
  12340
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

-- 16 个连续任务。
INSERT INTO `quest_template`
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `QuestInfoID`, `RewardNextQuest`,
 `RewardItem1`, `RewardAmount1`,
 `LogTitle`, `LogDescription`, `QuestDescription`, `QuestCompletionLog`,
 `RequiredItemId1`, `RequiredItemId2`, `RequiredItemCount1`, `RequiredItemCount2`,
 `ObjectiveText1`, `ObjectiveText2`, `VerifiedBuild`)
SELECT
  d.`quest_id`,
  2,
  80,
  80,
  0,
  0,
  0,
  d.`weapon_entry`,
  1,
  CONCAT('神器任务链·第', d.`rank_id`, '阶：', d.`weapon_name`),
  CASE
    WHEN d.`rank_id` = 1 THEN CONCAT('收集 ', d.`material_name`, ' x100，换取 ', d.`weapon_name`, '。')
    ELSE CONCAT('交出上一阶神器 x1，并收集 ', d.`material_name`, ' x100，升级为 ', d.`weapon_name`, '。')
  END,
  CASE
    WHEN d.`rank_id` = 1 THEN CONCAT('击败 ', d.`boss_name`, '，收集 ', d.`material_name`, ' x100。神器铸造师会为你铸成第一阶神器。')
    ELSE CONCAT('击败 ', d.`boss_name`, '，收集 ', d.`material_name`, ' x100，并带上上一阶神器完成升阶。')
  END,
  '返回神器铸造师，完成神器升阶。',
  CASE WHEN d.`rank_id` = 1 THEN d.`material_entry` ELSE d.`weapon_entry` - 1 END,
  CASE WHEN d.`rank_id` = 1 THEN 0 ELSE d.`material_entry` END,
  CASE WHEN d.`rank_id` = 1 THEN 100 ELSE 1 END,
  CASE WHEN d.`rank_id` = 1 THEN 0 ELSE 100 END,
  CASE
    WHEN d.`rank_id` = 1 THEN CONCAT(d.`material_name`, ' x100')
    ELSE CONCAT('上一阶神器 x1')
  END,
  CASE
    WHEN d.`rank_id` = 1 THEN ''
    ELSE CONCAT(d.`material_name`, ' x100')
  END,
  12340
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

INSERT INTO `quest_template_addon`
(`ID`, `PrevQuestID`, `NextQuestID`, `SpecialFlags`)
SELECT
  d.`quest_id`,
  0,
  0,
  0
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

-- 任务 NPC：接取并交付全部 16 个任务。
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `IconName`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`,
 `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`,
 `type`, `type_flags`, `lootid`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`,
 `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RegenHealth`,
 `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(91001, '神器铸造师·玄天', '神器任务链', 'Quest', 80, 80, 2, 35, 2,
 1, 1.14286, 20, 1.1, 0, 0, 1,
 2000, 2000, 1, 1, 1, 2, 2048, 0,
 7, 0, 0, 0, 0, '', 0, 1,
 100, 1, 1, 1, 1,
 0, 0, 2, '', 12340);

-- 16 个材料 Boss 模板。
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `IconName`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`,
 `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`,
 `type`, `type_flags`, `lootid`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`,
 `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RegenHealth`,
 `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  d.`boss_entry`,
  d.`boss_name`,
  CONCAT('掉落：', d.`material_name`),
  '',
  83,
  83,
  2,
  14,
  0,
  1,
  1.14286,
  40,
  1,
  3,
  0,
  100 + d.`rank_id` * 50,
  1500,
  2000,
  1,
  1,
  1,
  0,
  2048,
  0,
  7,
  0,
  d.`boss_entry`,
  0,
  0,
  'AggressorAI',
  0,
  1,
  1000 + d.`rank_id` * 500,
  1,
  5 + d.`rank_id`,
  1,
  1,
  0,
  0,
  0,
  '',
  12340
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

-- 模型数据。当前不创建 creature 世界刷新点。
INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(91001, 0, 3343, 1.0, 1.0, 12340);

INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  d.`boss_entry`,
  0,
  d.`boss_displayid`,
  2.5,
  1.0,
  12340
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

INSERT INTO `creature_queststarter`
(`id`, `quest`)
SELECT 91001, d.`quest_id`
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

INSERT INTO `creature_questender`
(`id`, `quest`)
SELECT 91001, d.`quest_id`
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

-- Boss 掉落：每个 Boss 每次掉落对应材料 1 个，掉率 100%，任务需求掉落。
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  d.`boss_entry`,
  d.`material_entry`,
  0,
  100,
  1,
  1,
  0,
  1,
  1,
  CONCAT(d.`boss_name`, ' -> ', d.`material_name`, ' x1，任务需求材料')
FROM `_tmp_artifact_chain_defs` d
ORDER BY d.`rank_id`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_artifact_chain_defs`;

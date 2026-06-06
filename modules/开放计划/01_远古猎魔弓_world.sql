-- 开放计划：远古-猎魔弓 world 数据
-- 说明：
-- 1. 物品段：93001..93004 为四把神兵，93005..93024 为远古-猎魔弓 1..20 级。
-- 2. 任务段：30031..30050，全部为可重复任务。
-- 3. NPC：91022，参考神器铸造师 91001 的任务 NPC 配置。
-- 4. 四把神兵为巫妖王 36597/39166/39167/39168 专属掉落。

SET NAMES utf8mb4;

SET @ANCIENT_HUNTER_BOW_NPC := 91022;

DROP TEMPORARY TABLE IF EXISTS `_tmp_ancient_hunter_bow_swords`;
CREATE TEMPORARY TABLE `_tmp_ancient_hunter_bow_swords` (
  `sort_id` tinyint unsigned NOT NULL,
  `sword_entry` int unsigned NOT NULL,
  `sword_name` varchar(64) NOT NULL,
  `dmg_type` tinyint unsigned NOT NULL,
  `effect_spell` int unsigned NOT NULL,
  PRIMARY KEY (`sort_id`)
) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `_tmp_ancient_hunter_bow_swords`
(`sort_id`, `sword_entry`, `sword_name`, `dmg_type`, `effect_spell`) VALUES
(1, 93001, '神兵·电剑', 3, 383001),
(2, 93002, '神兵·火剑', 2, 383002),
(3, 93003, '神兵·冰剑', 4, 383003),
(4, 93004, '神兵·爆炸剑', 2, 383004);

DROP TEMPORARY TABLE IF EXISTS `_tmp_ancient_hunter_bow_defs`;
CREATE TEMPORARY TABLE `_tmp_ancient_hunter_bow_defs` (
  `rank_id` tinyint unsigned NOT NULL,
  `bow_entry` int unsigned NOT NULL,
  `bow_name` varchar(64) NOT NULL,
  `stat_value` decimal(65,0) NOT NULL,
  `dmg_min` double NOT NULL,
  `dmg_max` double NOT NULL,
  `material_entry` int unsigned NOT NULL,
  `material_name` varchar(64) NOT NULL,
  `quest_id` int unsigned NOT NULL,
  PRIMARY KEY (`rank_id`)
) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `_tmp_ancient_hunter_bow_defs`
(`rank_id`, `bow_entry`, `bow_name`, `stat_value`, `dmg_min`, `dmg_max`, `material_entry`, `material_name`, `quest_id`) VALUES
(1,  93005, '远古-猎魔弓·1级',  90000,                         10000,      20000,       90017, '初醒神核',   30031),
(2,  93006, '远古-猎魔弓·2级',  990000,                        20000,      40000,       90018, '玄铁神纹',   30032),
(3,  93007, '远古-猎魔弓·3级',  9990000,                       40000,      80000,       90019, '星陨神砂',   30033),
(4,  93008, '远古-猎魔弓·4级',  99990000,                      80000,      160000,      90020, '太古龙髓',   30034),
(5,  93009, '远古-猎魔弓·5级',  900000000,                     160000,     320000,      90021, '虚空灵晶',   30035),
(6,  93010, '远古-猎魔弓·6级',  9900000000,                    320000,     640000,      90022, '混沌血玉',   30036),
(7,  93011, '远古-猎魔弓·7级',  99900000000,                   640000,     1280000,     90023, '雷劫天珠',   30037),
(8,  93012, '远古-猎魔弓·8级',  999900000000,                  1280000,    2560000,     90024, '日月圣辉',   30038),
(9,  93013, '远古-猎魔弓·9级',  9000000000000,                 2560000,    5120000,     90025, '乾坤道印',   30039),
(10, 93014, '远古-猎魔弓·10级', 99000000000000,                5120000,    10240000,    90026, '轮回魂火',   30040),
(11, 93015, '远古-猎魔弓·11级', 999000000000000,               10240000,   20480000,    90027, '鸿蒙源尘',   30041),
(12, 93016, '远古-猎魔弓·12级', 9999000000000000,              20480000,   40960000,    90028, '无极帝骨',   30042),
(13, 93017, '远古-猎魔弓·13级', 90000000000000000,             40960000,   81920000,    90029, '创世神髓',   30043),
(14, 93018, '远古-猎魔弓·14级', 990000000000000000,            81920000,   163840000,   90030, '灭界神晶',   30044),
(15, 93019, '远古-猎魔弓·15级', 9990000000000000000,           163840000,  327680000,   90031, '万象天命石', 30045),
(16, 93020, '远古-猎魔弓·16级', 99990000000000000000,          327680000,  655360000,   90032, '终焉神格',   30046),
(17, 93021, '远古-猎魔弓·17级', 900000000000000000000,         655360000,  1310720000,  90217, '太初源晶',   30047),
(18, 93022, '远古-猎魔弓·18级', 9900000000000000000000,        1310720000, 2621440000,  90218, '归墟神砂',   30048),
(19, 93023, '远古-猎魔弓·19级', 99900000000000000000000,       2621440000, 5242880000,  90219, '天道法印',   30049),
(20, 93024, '远古-猎魔弓·20级', 999900000000000000000000,      5242880000, 10485760000, 90220, '永恒神髓',   30050);

-- 清理本计划专用数据，保证重复导入安全。
DELETE FROM `creature_loot_template`
WHERE `Entry` IN (36597, 39166, 39167, 39168)
  AND `Item` BETWEEN 93001 AND 93004;

DELETE FROM `creature_queststarter`
WHERE `id` = @ANCIENT_HUNTER_BOW_NPC
  AND `quest` BETWEEN 30031 AND 30050;

DELETE FROM `creature_questender`
WHERE `id` = @ANCIENT_HUNTER_BOW_NPC
  AND `quest` BETWEEN 30031 AND 30050;

DELETE FROM `quest_template_addon`
WHERE `ID` BETWEEN 30031 AND 30050;

DELETE FROM `quest_template`
WHERE `ID` BETWEEN 30031 AND 30050;

DELETE FROM `creature_template_model`
WHERE `CreatureID` = @ANCIENT_HUNTER_BOW_NPC;

DELETE FROM `creature_template`
WHERE `entry` = @ANCIENT_HUNTER_BOW_NPC;

DELETE FROM `item_template`
WHERE `entry` BETWEEN 93001 AND 93024;

-- 四把神兵：基础属性 99 万，作为第一步合成材料，巫妖王专属掉落。
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
 `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,
 `maxcount`, `stackable`, `StatsCount`,
 `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`,
 `stat_type6`, `stat_value6`, `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`, `stat_type10`, `stat_value10`,
 `dmg_min1`, `dmg_max1`, `dmg_type1`,
 `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,
 `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`,
 `delay`, `bonding`, `description`, `Material`, `sheath`, `MaxDurability`, `ScriptName`, `flagsCustom`, `VerifiedBuild`)
SELECT
  s.`sword_entry`,
  2,
  7,
  -1,
  s.`sword_name`,
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
  4,  990000,
  3,  990000,
  7,  990000,
  5,  990000,
  6,  990000,
  8,  990000,
  9,  990000,
  31, 990000,
  38, 990000,
  45, 990000,
  20000,
  40000,
  s.`dmg_type`,
  s.`effect_spell`, 2, 0, 0, 0, 0, 0,
  0,
  0,
  0,
  0,
  0,
  0,
  1900,
  1,
  CONCAT('合成远古-猎魔弓的神兵材料。', s.`sword_name`, '，巫妖王专属掉落。'),
  1,
  3,
  65535,
  '',
  0,
  12340
FROM `_tmp_ancient_hunter_bow_swords` s
ORDER BY s.`sort_id`;

-- 20 级远程神器：远古-猎魔弓。
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
 `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,
 `maxcount`, `stackable`, `StatsCount`,
 `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`,
 `stat_type6`, `stat_value6`, `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`, `stat_type10`, `stat_value10`,
 `dmg_min1`, `dmg_max1`, `dmg_type1`,
 `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`,
 `delay`, `ammo_type`, `RangedModRange`,
 `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,
 `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`,
 `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`,
 `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`,
 `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`,
 `bonding`, `description`, `Material`, `sheath`, `MaxDurability`, `ScriptName`, `flagsCustom`, `VerifiedBuild`)
SELECT
  d.`bow_entry`,
  2,
  2,
  -1,
  d.`bow_name`,
  64356,
  5,
  0,
  0,
  1,
  0,
  0,
  15,
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
  d.`dmg_min`,
  d.`dmg_max`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  2200,
  0,
  100,
  383005 + (d.`rank_id` - 1) * 4, 2, 0, 0, 0, 0, 0,
  383006 + (d.`rank_id` - 1) * 4, 2, 0, 0, 0, 0, 0,
  383007 + (d.`rank_id` - 1) * 4, 2, 0, 0, 0, 0, 0,
  383008 + (d.`rank_id` - 1) * 4, 2, 0, 0, 0, 0, 0,
  46699, 1, 0, 0, -1, 0, -1,
  1,
  CONCAT('远程神器远古-猎魔弓第', d.`rank_id`, '级。绑定随等级递增的电、火、冰、爆炸四神兵命中特效，并继承群星之怒无需弹药特效。'),
  1,
  3,
  65535,
  '',
  0,
  12340
FROM `_tmp_ancient_hunter_bow_defs` d
ORDER BY d.`rank_id`;

-- 20 个可重复合成/升阶任务。第 1 个任务在神器任务 30001 材料基础上追加四把神兵。
INSERT INTO `quest_template`
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `QuestInfoID`, `RewardNextQuest`,
 `RewardItem1`, `RewardAmount1`,
 `LogTitle`, `LogDescription`, `QuestDescription`, `QuestCompletionLog`,
 `RequiredItemId1`, `RequiredItemId2`, `RequiredItemId3`, `RequiredItemId4`, `RequiredItemId5`, `RequiredItemId6`,
 `RequiredItemCount1`, `RequiredItemCount2`, `RequiredItemCount3`, `RequiredItemCount4`, `RequiredItemCount5`, `RequiredItemCount6`,
 `ObjectiveText1`, `ObjectiveText2`, `ObjectiveText3`, `ObjectiveText4`, `VerifiedBuild`)
SELECT
  d.`quest_id`,
  2,
  80,
  80,
  0,
  0,
  0,
  d.`bow_entry`,
  1,
  CONCAT('远古猎魔弓·第', d.`rank_id`, '级：', d.`bow_name`),
  CASE
    WHEN d.`rank_id` = 1 THEN CONCAT('收集 ', d.`material_name`, ' x100，并交出四把神兵，合成 ', d.`bow_name`, '。')
    ELSE CONCAT('交出上一阶远古-猎魔弓 x1，并收集 ', d.`material_name`, ' x100、专用材料 x9999，升级为 ', d.`bow_name`, '。')
  END,
  CASE
    WHEN d.`rank_id` = 1 THEN CONCAT('准备 ', d.`material_name`, ' x100，以及巫妖王掉落的电剑、火剑、冰剑、爆炸剑各 1 把。')
    ELSE CONCAT('带上上一阶远古-猎魔弓，准备 ', d.`material_name`, ' x100 和专用材料 x9999。')
  END,
  '返回猎魔弓铸造师，完成远古-猎魔弓合成或升阶。',
  CASE WHEN d.`rank_id` = 1 THEN d.`material_entry` ELSE d.`bow_entry` - 1 END,
  CASE WHEN d.`rank_id` = 1 THEN 93001 ELSE d.`material_entry` END,
  CASE WHEN d.`rank_id` = 1 THEN 93002 ELSE 62001 END,
  CASE WHEN d.`rank_id` = 1 THEN 93003 ELSE 0 END,
  CASE WHEN d.`rank_id` = 1 THEN 93004 ELSE 0 END,
  0,
  CASE WHEN d.`rank_id` = 1 THEN 100 ELSE 1 END,
  CASE WHEN d.`rank_id` = 1 THEN 1 ELSE 100 END,
  CASE WHEN d.`rank_id` = 1 THEN 1 ELSE 9999 END,
  CASE WHEN d.`rank_id` = 1 THEN 1 ELSE 0 END,
  CASE WHEN d.`rank_id` = 1 THEN 1 ELSE 0 END,
  0,
  CASE WHEN d.`rank_id` = 1 THEN CONCAT(d.`material_name`, ' x100') ELSE '上一阶远古-猎魔弓 x1' END,
  CASE WHEN d.`rank_id` = 1 THEN '电剑 x1' ELSE CONCAT(d.`material_name`, ' x100') END,
  CASE WHEN d.`rank_id` = 1 THEN '火剑 x1' ELSE '专用材料 x9999' END,
  CASE WHEN d.`rank_id` = 1 THEN '冰剑/爆炸剑各 x1' ELSE '' END,
  12340
FROM `_tmp_ancient_hunter_bow_defs` d
ORDER BY d.`rank_id`;

INSERT INTO `quest_template_addon`
(`ID`, `PrevQuestID`, `NextQuestID`, `SpecialFlags`)
SELECT
  d.`quest_id`,
  0,
  0,
  1
FROM `_tmp_ancient_hunter_bow_defs` d
ORDER BY d.`rank_id`;

-- 任务 NPC：只创建模板与模型，不创建世界刷新点。
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `IconName`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`,
 `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`,
 `type`, `type_flags`, `lootid`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`,
 `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RegenHealth`,
 `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(@ANCIENT_HUNTER_BOW_NPC, '猎魔弓铸造师·玄天', '远古-猎魔弓合成', 'Quest', 80, 80, 2, 35, 2,
 1, 1.14286, 20, 1.1, 0, 0, 1,
 2000, 2000, 1, 1, 1, 2, 2048, 0,
 7, 0, 0, 0, 0, '', 0, 1,
 100, 1, 1, 1, 1,
 0, 0, 2, '', 12340);

INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(@ANCIENT_HUNTER_BOW_NPC, 0, 3343, 1.0, 1.0, 12340);

INSERT INTO `creature_queststarter`
(`id`, `quest`)
SELECT @ANCIENT_HUNTER_BOW_NPC, d.`quest_id`
FROM `_tmp_ancient_hunter_bow_defs` d
ORDER BY d.`rank_id`;

INSERT INTO `creature_questender`
(`id`, `quest`)
SELECT @ANCIENT_HUNTER_BOW_NPC, d.`quest_id`
FROM `_tmp_ancient_hunter_bow_defs` d
ORDER BY d.`rank_id`;

-- 巫妖王四难度模板均掉落四把神兵，作为远古-猎魔弓第一步合成材料。
-- 每把神兵掉落几率 1%。
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  lk.`entry`,
  s.`sword_entry`,
  0,
  1,
  0,
  1,
  0,
  1,
  1,
  CONCAT('巫妖王 -> ', s.`sword_name`, '，远古-猎魔弓合成材料')
FROM (
  SELECT 36597 AS `entry`
  UNION ALL SELECT 39166
  UNION ALL SELECT 39167
  UNION ALL SELECT 39168
) lk
CROSS JOIN `_tmp_ancient_hunter_bow_swords` s
ORDER BY lk.`entry`, s.`sort_id`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_ancient_hunter_bow_defs`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_ancient_hunter_bow_swords`;

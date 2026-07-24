-- 古达克仙门试炼：二级至五级扩展
-- 以数据库中的一级副本（10001、800001-800018）为复制基线。

SET NAMES utf8mb4;
START TRANSACTION;

DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_xianmen_tiers`;
CREATE TEMPORARY TABLE `_tmp_gundrak_xianmen_tiers` (
  `mirage_level` int unsigned NOT NULL,
  `entry_shift` int unsigned NOT NULL,
  `tier_name` varchar(16) NOT NULL,
  `core_multiplier` int unsigned NOT NULL,
  `stat_multiplier` decimal(65,0) NOT NULL,
  `loot_reference` int unsigned NOT NULL,
  `item_min` int unsigned NOT NULL,
  `item_max` int unsigned NOT NULL,
  `aura_stone_count` tinyint unsigned NOT NULL,
  `breakthrough_stone_count` tinyint unsigned NOT NULL,
  PRIMARY KEY (`mirage_level`)
) ENGINE=MEMORY;

INSERT INTO `_tmp_gundrak_xianmen_tiers`
  (`mirage_level`, `entry_shift`, `tier_name`, `core_multiplier`, `stat_multiplier`,
   `loot_reference`, `item_min`, `item_max`, `aura_stone_count`, `breakthrough_stone_count`) VALUES
(10002, 100, '二级', 2, 100000, 962200, 96100, 96299, 20, 5),
(10003, 200, '三级', 4, 1000000000, 970200, 96900, 97099, 40, 10),
(10004, 300, '四级', 8, 10000000000000, 978200, 97700, 97899, 80, 20),
(10005, 400, '五级', 16, 100000000000000000, 986200, 98500, 98699, 160, 40);

DELETE FROM `_挑战幻境生物`
WHERE `生物组` BETWEEN 10002 AND 10005
   OR (`生物Entry` BETWEEN 800101 AND 800118)
   OR (`生物Entry` BETWEEN 800201 AND 800218)
   OR (`生物Entry` BETWEEN 800301 AND 800318)
   OR (`生物Entry` BETWEEN 800401 AND 800418);

DELETE FROM `_挑战幻境等级` WHERE `等级` BETWEEN 10002 AND 10005;
DELETE FROM `_属性调整_生物`
WHERE (`id` BETWEEN 800101 AND 800118)
   OR (`id` BETWEEN 800201 AND 800218)
   OR (`id` BETWEEN 800301 AND 800318)
   OR (`id` BETWEEN 800401 AND 800418)
   OR (`生物id` BETWEEN 800101 AND 800118)
   OR (`生物id` BETWEEN 800201 AND 800218)
   OR (`生物id` BETWEEN 800301 AND 800318)
   OR (`生物id` BETWEEN 800401 AND 800418);
DELETE FROM `creature_loot_template`
WHERE (`Entry` BETWEEN 800101 AND 800118)
   OR (`Entry` BETWEEN 800201 AND 800218)
   OR (`Entry` BETWEEN 800301 AND 800318)
   OR (`Entry` BETWEEN 800401 AND 800418);
DELETE FROM `creature_template_model`
WHERE (`CreatureID` BETWEEN 800101 AND 800118)
   OR (`CreatureID` BETWEEN 800201 AND 800218)
   OR (`CreatureID` BETWEEN 800301 AND 800318)
   OR (`CreatureID` BETWEEN 800401 AND 800418);
DELETE FROM `creature_template`
WHERE (`entry` BETWEEN 800101 AND 800118)
   OR (`entry` BETWEEN 800201 AND 800218)
   OR (`entry` BETWEEN 800301 AND 800318)
   OR (`entry` BETWEEN 800401 AND 800418);
DELETE FROM `reference_loot_template` WHERE `Entry` IN (962200, 970200, 978200, 986200);

INSERT INTO `_挑战幻境等级`
  (`等级`, `名称`, `需求物品`, `需求数量`, `生物组`, `持续秒数`, `备注`)
SELECT
  t.`mirage_level`,
  CONCAT('古达克仙门试炼·', t.`tier_name`),
  base.`需求物品`,
  base.`需求数量`,
  t.`mirage_level`,
  base.`持续秒数`,
  CONCAT('自定义副本：古达克地图604，挑战幻境隔离层', t.`mirage_level`)
FROM `_挑战幻境等级` base
CROSS JOIN `_tmp_gundrak_xianmen_tiers` t
WHERE base.`等级` = 10001;

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
  base.`entry` + t.`entry_shift`,
  0, 0, 0, base.`KillCredit1`, base.`KillCredit2`,
  CONCAT(base.`name`, '·', t.`tier_name`),
  CONCAT('仙门试炼', t.`tier_name`, CASE WHEN base.`entry` BETWEEN 800001 AND 800005 THEN '首领' ELSE '守卫' END),
  base.`IconName`, base.`gossip_menu_id`, base.`minlevel`, base.`maxlevel`, base.`exp`, base.`faction`, base.`npcflag`,
  base.`speed_walk`, base.`speed_run`, base.`speed_swim`, base.`speed_flight`, base.`detection_range`, base.`scale`,
  base.`rank`, base.`dmgschool`, base.`DamageModifier` * t.`core_multiplier`,
  base.`BaseAttackTime`, base.`RangeAttackTime`, base.`BaseVariance`, base.`RangeVariance`, base.`unit_class`,
  base.`unit_flags`, base.`unit_flags2`, base.`dynamicflags`, base.`family`, base.`trainer_type`, base.`trainer_spell`,
  base.`trainer_class`, base.`trainer_race`, base.`type`, base.`type_flags`, base.`entry` + t.`entry_shift`,
  base.`pickpocketloot`, base.`skinloot`, base.`PetSpellDataId`, base.`VehicleId`, base.`mingold`, base.`maxgold`,
  base.`AIName`, base.`MovementType`, base.`HoverHeight`, base.`HealthModifier` * t.`core_multiplier`,
  base.`ManaModifier`, base.`ArmorModifier` * t.`core_multiplier`, base.`ExperienceModifier`, base.`RacialLeader`,
  base.`movementId`, base.`RegenHealth`, base.`mechanic_immune_mask`, base.`spell_school_immune_mask`,
  base.`flags_extra`, base.`ScriptName`, base.`VerifiedBuild`
FROM `creature_template` base
CROSS JOIN `_tmp_gundrak_xianmen_tiers` t
WHERE base.`entry` BETWEEN 800001 AND 800018;

INSERT INTO `creature_template_model`
  (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  base.`CreatureID` + t.`entry_shift`, base.`Idx`, base.`CreatureDisplayID`,
  base.`DisplayScale`, base.`Probability`, base.`VerifiedBuild`
FROM `creature_template_model` base
CROSS JOIN `_tmp_gundrak_xianmen_tiers` t
WHERE base.`CreatureID` BETWEEN 800001 AND 800018;

INSERT INTO `_挑战幻境生物`
  (`生物组`, `生物Entry`, `地图`, `区域`, `坐标X`, `坐标Y`, `坐标Z`, `朝向O`,
   `数量`, `刷新秒数`, `游荡距离`, `备注`)
SELECT
  t.`mirage_level`, base.`生物Entry` + t.`entry_shift`, base.`地图`, base.`区域`,
  base.`坐标X`, base.`坐标Y`, base.`坐标Z`, base.`朝向O`, base.`数量`, base.`刷新秒数`,
  base.`游荡距离`, CONCAT('古达克仙门试炼·', t.`tier_name`, '：', SUBSTRING_INDEX(base.`备注`, '：', -1))
FROM `_挑战幻境生物` base
CROSS JOIN `_tmp_gundrak_xianmen_tiers` t
WHERE base.`生物组` = 10001;

INSERT INTO `reference_loot_template`
  (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  t.`loot_reference`, item.`entry`, 0, 0, 0, 1, 1, 1, 1,
  CONCAT('古达克仙门试炼·', t.`tier_name`, '随机装备池')
FROM `_tmp_gundrak_xianmen_tiers` t
INNER JOIN `item_template` item ON item.`entry` BETWEEN t.`item_min` AND t.`item_max`
WHERE item.`InventoryType` <> 16;

INSERT INTO `creature_loot_template`
  (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  base.`Entry` + t.`entry_shift`,
  base.`Item`,
  CASE WHEN base.`Reference` = 952200 THEN t.`loot_reference` ELSE base.`Reference` END,
  base.`Chance`, base.`QuestRequired`, base.`LootMode`, base.`GroupId`,
  CASE
    WHEN base.`Item` = 62001 THEN t.`aura_stone_count`
    WHEN base.`Item` = 62002 THEN t.`breakthrough_stone_count`
    ELSE base.`MinCount`
  END,
  CASE
    WHEN base.`Item` = 62001 THEN t.`aura_stone_count`
    WHEN base.`Item` = 62002 THEN t.`breakthrough_stone_count`
    ELSE base.`MaxCount`
  END,
  CASE
    WHEN base.`Reference` = 952200 THEN CONCAT('古达克仙门试炼·', t.`tier_name`, 'Boss随机装备池')
    WHEN base.`Item` = 62001 THEN CONCAT('古达克仙门试炼·', t.`tier_name`, '必掉灵气石x', t.`aura_stone_count`)
    WHEN base.`Item` = 62002 THEN CONCAT('古达克仙门试炼·', t.`tier_name`, 'Boss必掉突破石x', t.`breakthrough_stone_count`)
    ELSE CONCAT('古达克仙门试炼·', t.`tier_name`)
  END
FROM `creature_loot_template` base
CROSS JOIN `_tmp_gundrak_xianmen_tiers` t
WHERE base.`Entry` BETWEEN 800001 AND 800018;

DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_attr`;
CREATE TEMPORARY TABLE `_tmp_gundrak_attr` AS
SELECT * FROM `_属性调整_生物` WHERE `id` BETWEEN 800001 AND 800018 AND `生物id` BETWEEN 800001 AND 800018;
UPDATE `_tmp_gundrak_attr` SET
  `id` = `id` + 100, `生物id` = `生物id` + 100,
  `注释` = CONCAT('古达克仙门试炼·二级：', SUBSTRING_INDEX(`注释`, '：', -1)),
  `血量值` = `血量值` * 100000, `魔法值` = `魔法值` * 100000,
  `护甲值` = `护甲值` * 100000, `抗性值` = `抗性值` * 100000,
  `物理攻击最小值` = `物理攻击最小值` * 100000, `物理攻击最大值` = `物理攻击最大值` * 100000,
  `魔法攻击最小值` = `魔法攻击最小值` * 100000, `魔法攻击最大值` = `魔法攻击最大值` * 100000,
  `DOT攻击最小值` = `DOT攻击最小值` * 100000, `DOT攻击最大值` = `DOT攻击最大值` * 100000,
  `治疗最小值` = `治疗最小值` * 100000, `治疗最大值` = `治疗最大值` * 100000,
  `真实伤害值` = `真实伤害值` * 100000, `被攻击掉血上限` = `被攻击掉血上限` * 100000,
  `物理攻击切割伤害值` = `物理攻击切割伤害值` * 100000,
  `魔法攻击切割伤害值` = `魔法攻击切割伤害值` * 100000,
  `DOT攻击切割伤害值` = `DOT攻击切割伤害值` * 100000;
INSERT INTO `_属性调整_生物` SELECT * FROM `_tmp_gundrak_attr`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_attr`;
CREATE TEMPORARY TABLE `_tmp_gundrak_attr` AS
SELECT * FROM `_属性调整_生物` WHERE `id` BETWEEN 800001 AND 800018 AND `生物id` BETWEEN 800001 AND 800018;
UPDATE `_tmp_gundrak_attr` SET
  `id` = `id` + 200, `生物id` = `生物id` + 200,
  `注释` = CONCAT('古达克仙门试炼·三级：', SUBSTRING_INDEX(`注释`, '：', -1)),
  `血量值` = `血量值` * 1000000000, `魔法值` = `魔法值` * 1000000000,
  `护甲值` = `护甲值` * 1000000000, `抗性值` = `抗性值` * 1000000000,
  `物理攻击最小值` = `物理攻击最小值` * 1000000000, `物理攻击最大值` = `物理攻击最大值` * 1000000000,
  `魔法攻击最小值` = `魔法攻击最小值` * 1000000000, `魔法攻击最大值` = `魔法攻击最大值` * 1000000000,
  `DOT攻击最小值` = `DOT攻击最小值` * 1000000000, `DOT攻击最大值` = `DOT攻击最大值` * 1000000000,
  `治疗最小值` = `治疗最小值` * 1000000000, `治疗最大值` = `治疗最大值` * 1000000000,
  `真实伤害值` = `真实伤害值` * 1000000000, `被攻击掉血上限` = `被攻击掉血上限` * 1000000000,
  `物理攻击切割伤害值` = `物理攻击切割伤害值` * 1000000000,
  `魔法攻击切割伤害值` = `魔法攻击切割伤害值` * 1000000000,
  `DOT攻击切割伤害值` = `DOT攻击切割伤害值` * 1000000000;
INSERT INTO `_属性调整_生物` SELECT * FROM `_tmp_gundrak_attr`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_attr`;
CREATE TEMPORARY TABLE `_tmp_gundrak_attr` AS
SELECT * FROM `_属性调整_生物` WHERE `id` BETWEEN 800001 AND 800018 AND `生物id` BETWEEN 800001 AND 800018;
UPDATE `_tmp_gundrak_attr` SET
  `id` = `id` + 300, `生物id` = `生物id` + 300,
  `注释` = CONCAT('古达克仙门试炼·四级：', SUBSTRING_INDEX(`注释`, '：', -1)),
  `血量值` = `血量值` * 10000000000000, `魔法值` = `魔法值` * 10000000000000,
  `护甲值` = `护甲值` * 10000000000000, `抗性值` = `抗性值` * 10000000000000,
  `物理攻击最小值` = `物理攻击最小值` * 10000000000000, `物理攻击最大值` = `物理攻击最大值` * 10000000000000,
  `魔法攻击最小值` = `魔法攻击最小值` * 10000000000000, `魔法攻击最大值` = `魔法攻击最大值` * 10000000000000,
  `DOT攻击最小值` = `DOT攻击最小值` * 10000000000000, `DOT攻击最大值` = `DOT攻击最大值` * 10000000000000,
  `治疗最小值` = `治疗最小值` * 10000000000000, `治疗最大值` = `治疗最大值` * 10000000000000,
  `真实伤害值` = `真实伤害值` * 10000000000000, `被攻击掉血上限` = `被攻击掉血上限` * 10000000000000,
  `物理攻击切割伤害值` = `物理攻击切割伤害值` * 10000000000000,
  `魔法攻击切割伤害值` = `魔法攻击切割伤害值` * 10000000000000,
  `DOT攻击切割伤害值` = `DOT攻击切割伤害值` * 10000000000000;
INSERT INTO `_属性调整_生物` SELECT * FROM `_tmp_gundrak_attr`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_attr`;
CREATE TEMPORARY TABLE `_tmp_gundrak_attr` AS
SELECT * FROM `_属性调整_生物` WHERE `id` BETWEEN 800001 AND 800018 AND `生物id` BETWEEN 800001 AND 800018;
UPDATE `_tmp_gundrak_attr` SET
  `id` = `id` + 400, `生物id` = `生物id` + 400,
  `注释` = CONCAT('古达克仙门试炼·五级：', SUBSTRING_INDEX(`注释`, '：', -1)),
  `血量值` = `血量值` * 100000000000000000, `魔法值` = `魔法值` * 100000000000000000,
  `护甲值` = `护甲值` * 100000000000000000, `抗性值` = `抗性值` * 100000000000000000,
  `物理攻击最小值` = `物理攻击最小值` * 100000000000000000, `物理攻击最大值` = `物理攻击最大值` * 100000000000000000,
  `魔法攻击最小值` = `魔法攻击最小值` * 100000000000000000, `魔法攻击最大值` = `魔法攻击最大值` * 100000000000000000,
  `DOT攻击最小值` = `DOT攻击最小值` * 100000000000000000, `DOT攻击最大值` = `DOT攻击最大值` * 100000000000000000,
  `治疗最小值` = `治疗最小值` * 100000000000000000, `治疗最大值` = `治疗最大值` * 100000000000000000,
  `真实伤害值` = `真实伤害值` * 100000000000000000, `被攻击掉血上限` = `被攻击掉血上限` * 100000000000000000,
  `物理攻击切割伤害值` = `物理攻击切割伤害值` * 100000000000000000,
  `魔法攻击切割伤害值` = `魔法攻击切割伤害值` * 100000000000000000,
  `DOT攻击切割伤害值` = `DOT攻击切割伤害值` * 100000000000000000;
INSERT INTO `_属性调整_生物` SELECT * FROM `_tmp_gundrak_attr`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_attr`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_gundrak_xianmen_tiers`;

COMMIT;

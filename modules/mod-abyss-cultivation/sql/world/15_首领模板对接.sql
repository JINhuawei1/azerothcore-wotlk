-- ============================================
-- 首版 creature_template 对接：由 _深渊首领配置 批量生成
-- 说明：尽量复用章节官方首领模板作为骨架和模型来源，后续再补掉落和技能
-- ============================================

DELETE FROM `creature_template_addon`
WHERE `entry` BETWEEN 910001 AND 910074 OR `entry` BETWEEN 919001 AND 919074;

DELETE FROM `creature_template_model`
WHERE `CreatureID` BETWEEN 910001 AND 910074 OR `CreatureID` BETWEEN 919001 AND 919074;

DELETE FROM `creature_template`
WHERE `entry` BETWEEN 910001 AND 910074 OR `entry` BETWEEN 919001 AND 919074;

REPLACE INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`,
 `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`,
 `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`,
 `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`,
 `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
 `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  d.`首领入口`,
  0,
  0,
  0,
  0,
  0,
  d.`首领名称`,
  '',
  '',
  0,
  d.`建议等级`,
  d.`建议等级`,
  s.`exp`,
  d.`建议阵营`,
  0,
  s.`speed_walk`,
  s.`speed_run`,
  s.`speed_swim`,
  s.`speed_flight`,
  s.`detection_range`,
  s.`scale`,
  s.`rank`,
  s.`dmgschool`,
  s.`DamageModifier`,
  s.`BaseAttackTime`,
  s.`RangeAttackTime`,
  s.`BaseVariance`,
  s.`RangeVariance`,
  s.`unit_class`,
  0,
  0,
  0,
  s.`family`,
  s.`trainer_type`,
  s.`trainer_spell`,
  s.`trainer_class`,
  s.`trainer_race`,
  s.`type`,
  s.`type_flags`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  '',
  0,
  s.`HoverHeight`,
  s.`HealthModifier`,
  s.`ManaModifier`,
  s.`ArmorModifier`,
  s.`ExperienceModifier`,
  s.`RacialLeader`,
  0,
  s.`RegenHealth`,
  0,
  0,
  0,
  '',
  NULL
FROM (
  SELECT
    `首领入口`,
    `首领名称`,
    `首领类型`,
    `幕ID`,
    CASE
      WHEN `章节ID` <> 0 THEN `章节ID`
      WHEN `首领类型` = 3 AND `幕ID` = 1 THEN 1
      WHEN `首领类型` = 3 AND `幕ID` = 2 THEN 21
      WHEN `首领类型` = 3 AND `幕ID` = 4 THEN 53
      WHEN `首领类型` = 3 AND `幕ID` = 5 THEN 58
      WHEN `首领类型` = 3 AND `幕ID` = 6 THEN 67
      ELSE `章节ID`
    END AS `章节ID`,
    CASE WHEN `幕ID` <= 1 THEN 35 WHEN `幕ID` = 2 THEN 70 WHEN `幕ID` = 3 THEN 80 WHEN `幕ID` = 4 THEN 83 WHEN `幕ID` = 5 THEN 83 ELSE 83 END AS `建议等级`,
    14 AS `建议阵营`
  FROM `_深渊首领配置`
  WHERE `首领类型` IN (2, 3)
) d
JOIN `_深渊章节配置` c ON c.`章节ID` = d.`章节ID`
JOIN `creature_template` s
  ON s.`entry` = CASE
    WHEN c.`最终首领入口` <> 0 THEN c.`最终首领入口`
    ELSE c.`锚点首领入口`
  END
WHERE d.`首领入口` BETWEEN 910001 AND 910074 OR d.`首领入口` BETWEEN 919001 AND 919074;

-- 兜底：处理章节骨架不存在或秘藏首领章节ID=0时的 creature_template 生成
REPLACE INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`,
 `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`,
 `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`,
 `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`,
 `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
 `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  d.`首领入口`,
  0,
  0,
  0,
  0,
  0,
  d.`首领名称`,
  '',
  '',
  0,
  d.`建议等级`,
  d.`建议等级`,
  s.`exp`,
  d.`建议阵营`,
  0,
  s.`speed_walk`,
  s.`speed_run`,
  s.`speed_swim`,
  s.`speed_flight`,
  s.`detection_range`,
  s.`scale`,
  s.`rank`,
  s.`dmgschool`,
  s.`DamageModifier`,
  s.`BaseAttackTime`,
  s.`RangeAttackTime`,
  s.`BaseVariance`,
  s.`RangeVariance`,
  s.`unit_class`,
  0,
  0,
  0,
  s.`family`,
  s.`trainer_type`,
  s.`trainer_spell`,
  s.`trainer_class`,
  s.`trainer_race`,
  s.`type`,
  s.`type_flags`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  '',
  0,
  s.`HoverHeight`,
  s.`HealthModifier`,
  s.`ManaModifier`,
  s.`ArmorModifier`,
  s.`ExperienceModifier`,
  s.`RacialLeader`,
  0,
  s.`RegenHealth`,
  0,
  0,
  0,
  '',
  NULL
FROM (
  SELECT
    `首领入口`,
    `首领名称`,
    `首领类型`,
    `幕ID`,
    CASE
      WHEN `章节ID` <> 0 THEN `章节ID`
      WHEN `首领类型` = 3 AND `幕ID` = 1 THEN 1
      WHEN `首领类型` = 3 AND `幕ID` = 2 THEN 21
      WHEN `首领类型` = 3 AND `幕ID` = 4 THEN 53
      WHEN `首领类型` = 3 AND `幕ID` = 5 THEN 58
      WHEN `首领类型` = 3 AND `幕ID` = 6 THEN 67
      ELSE `章节ID`
    END AS `章节ID`,
    CASE WHEN `幕ID` <= 1 THEN 35 WHEN `幕ID` = 2 THEN 70 WHEN `幕ID` = 3 THEN 80 WHEN `幕ID` = 4 THEN 83 WHEN `幕ID` = 5 THEN 83 ELSE 83 END AS `建议等级`,
    14 AS `建议阵营`
  FROM `_深渊首领配置`
  WHERE `首领类型` IN (2, 3)
) d
JOIN `creature_template` s
  ON s.`entry` = CASE
    WHEN d.`首领入口` = 910046 THEN 26533
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 1 THEN 11520
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 2 THEN 17537
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 3 THEN 23954
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 4 THEN 11502
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 5 THEN 15690
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 6 THEN 15990
    ELSE 0
  END
WHERE d.`首领入口` = 910046;

REPLACE INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  d.`首领入口`,
  m.`Idx`,
  m.`CreatureDisplayID`,
  m.`DisplayScale`,
  m.`Probability`,
  NULL
FROM (
  SELECT
    `首领入口`,
    `首领名称`,
    `首领类型`,
    `幕ID`,
    CASE
      WHEN `章节ID` <> 0 THEN `章节ID`
      WHEN `首领类型` = 3 AND `幕ID` = 1 THEN 1
      WHEN `首领类型` = 3 AND `幕ID` = 2 THEN 21
      WHEN `首领类型` = 3 AND `幕ID` = 4 THEN 53
      WHEN `首领类型` = 3 AND `幕ID` = 5 THEN 58
      WHEN `首领类型` = 3 AND `幕ID` = 6 THEN 67
      ELSE `章节ID`
    END AS `章节ID`
  FROM `_深渊首领配置`
  WHERE `首领类型` IN (2, 3)
) d
JOIN `_深渊章节配置` c ON c.`章节ID` = d.`章节ID`
JOIN `creature_template_model` m
  ON m.`CreatureID` = CASE
    WHEN c.`最终首领入口` <> 0 THEN c.`最终首领入口`
    ELSE c.`锚点首领入口`
  END
WHERE d.`首领入口` BETWEEN 910001 AND 910074 OR d.`首领入口` BETWEEN 919001 AND 919074;

-- 兜底：处理章节骨架不存在或秘藏首领章节ID=0时的模型复制
REPLACE INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  d.`首领入口`,
  m.`Idx`,
  m.`CreatureDisplayID`,
  m.`DisplayScale`,
  m.`Probability`,
  NULL
FROM (
  SELECT
    `首领入口`,
    `首领类型`,
    `幕ID`
  FROM `_深渊首领配置`
  WHERE `首领类型` IN (2, 3)
) d
JOIN `creature_template_model` m
  ON m.`CreatureID` = CASE
    WHEN d.`首领入口` = 910046 THEN 26533
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 1 THEN 11520
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 2 THEN 17537
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 3 THEN 23954
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 4 THEN 11502
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 5 THEN 15690
    WHEN d.`首领类型` = 3 AND d.`幕ID` = 6 THEN 15990
    ELSE 0
  END
WHERE d.`首领入口` = 910046;

REPLACE INTO `creature_template_addon`
(`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
SELECT
  d.`首领入口`,
  0,
  0,
  0,
  1,
  0,
  0,
  NULL
FROM (
  SELECT
    `首领入口`
  FROM `_深渊首领配置`
  WHERE `首领类型` IN (2, 3)
) d
JOIN `creature_template` ct ON ct.`entry` = d.`首领入口`
WHERE d.`首领入口` BETWEEN 910001 AND 910074 OR d.`首领入口` BETWEEN 919001 AND 919074;


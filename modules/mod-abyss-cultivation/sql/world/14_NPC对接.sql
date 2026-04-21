-- ============================================
-- 首版任务发布/交付 NPC 对接：补 creature_queststarter / creature_questender
-- 说明：为每个章节生成“接引 NPC / 验收 NPC”两类友善任务 NPC，
--       避免把任务错误绑定到敌对首领身上
-- ============================================

DELETE FROM `creature_queststarter`
WHERE `quest` BETWEEN 700001 AND 740074;

DELETE FROM `creature_questender`
WHERE `quest` BETWEEN 700001 AND 740074;

DELETE FROM `creature`
WHERE `id1` BETWEEN 191001 AND 191074
   OR `id1` BETWEEN 192001 AND 192074;

DELETE FROM `creature_template_model`
WHERE `CreatureID` BETWEEN 191001 AND 191074
   OR `CreatureID` BETWEEN 192001 AND 192074;

DELETE FROM `creature_template_addon`
WHERE `entry` BETWEEN 191001 AND 191074
   OR `entry` BETWEEN 192001 AND 192074;

DELETE FROM `creature_template`
WHERE `entry` BETWEEN 191001 AND 191074
   OR `entry` BETWEEN 192001 AND 192074;

-- 回滚上一版误加到敌对首领身上的 questgiver 标记
UPDATE `creature_template` ct
JOIN `_深渊章节配置` c ON ct.`entry` IN (c.`锚点首领入口`, c.`最终首领入口`)
SET ct.`npcflag` = (ct.`npcflag` & 4294967293)
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`,
 `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`,
 `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`,
 `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`,
 `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
 `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  191000 + c.`章节ID`,
  0,
  0,
  0,
  0,
  0,
  CONCAT('深渊接引使·', c.`章节名称`),
  '章节任务发布',
  'Speak',
  0,
  GREATEST(1, LEAST(c.`需求修仙等级`, 80)),
  GREATEST(1, LEAST(c.`需求修仙等级`, 80)),
  s.`exp`,
  35,
  2,
  s.`speed_walk`,
  s.`speed_run`,
  s.`speed_swim`,
  s.`speed_flight`,
  s.`detection_range`,
  s.`scale`,
  0,
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
  0,
  0,
  0,
  0,
  s.`type`,
  s.`type_flags`,
  0,
  0,
  0,
  s.`PetSpellDataId`,
  s.`VehicleId`,
  0,
  0,
  '',
  0,
  s.`HoverHeight`,
  1,
  1,
  1,
  1,
  0,
  0,
  1,
  0,
  0,
  0,
  '',
  NULL
FROM `_深渊章节配置` c
JOIN `creature_template` s ON s.`entry` = 392
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`,
 `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`,
 `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`,
 `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`,
 `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
 `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
SELECT
  192000 + c.`章节ID`,
  0,
  0,
  0,
  0,
  0,
  CONCAT('深渊验收使·', c.`章节名称`),
  '章节任务交付',
  'Speak',
  0,
  GREATEST(1, LEAST(c.`需求修仙等级`, 80)),
  GREATEST(1, LEAST(c.`需求修仙等级`, 80)),
  s.`exp`,
  35,
  2,
  s.`speed_walk`,
  s.`speed_run`,
  s.`speed_swim`,
  s.`speed_flight`,
  s.`detection_range`,
  s.`scale`,
  0,
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
  0,
  0,
  0,
  0,
  s.`type`,
  s.`type_flags`,
  0,
  0,
  0,
  s.`PetSpellDataId`,
  s.`VehicleId`,
  0,
  0,
  '',
  0,
  s.`HoverHeight`,
  1,
  1,
  1,
  1,
  0,
  0,
  1,
  0,
  0,
  0,
  '',
  NULL
FROM `_深渊章节配置` c
JOIN `creature_template` s ON s.`entry` = 392
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT 191000 + c.`章节ID`, 0, 1279, 1, 1, NULL
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT 192000 + c.`章节ID`, 0, 1279, 1, 1, NULL
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_template_addon`
(`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
SELECT 191000 + c.`章节ID`, 0, 0, 0, 1, 0, 0, NULL
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_template_addon`
(`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
SELECT 192000 + c.`章节ID`, 0, 0, 0, 1, 0, 0, NULL
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature`
(`id1`, `id2`, `id3`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
 `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
 `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
 `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
SELECT
  191000 + c.`章节ID`,
  0,
  0,
  c.`副本地图ID`,
  0,
  0,
  1,
  1,
  0,
  CASE
    WHEN c.`副本地图ID` = 229 THEN
      t.`target_position_x` + COS(t.`target_orientation` + 1.5707963) * 3.0 + COS(t.`target_orientation`) * 2.5
    ELSE
      t.`target_position_x` + COS(t.`target_orientation` + 1.5707963) * 2.5
  END,
  CASE
    WHEN c.`副本地图ID` = 229 THEN
      t.`target_position_y` + SIN(t.`target_orientation` + 1.5707963) * 3.0 + SIN(t.`target_orientation`) * 2.5
    ELSE
      t.`target_position_y` + SIN(t.`target_orientation` + 1.5707963) * 2.5
  END,
  t.`target_position_z`,
  t.`target_orientation` + 3.1415926,
  120,
  0,
  0,
  1000,
  0,
  0,
  2,
  0,
  0,
  '',
  NULL,
  0,
  CONCAT('深渊章节起始任务 NPC·章节', c.`章节ID`)
FROM `_深渊章节配置` c
JOIN `areatrigger_teleport` t
  ON t.`ID` = (
    SELECT MIN(t2.`ID`)
    FROM `areatrigger_teleport` t2
    WHERE t2.`target_map` = c.`副本地图ID`
  )
WHERE c.`章节ID` BETWEEN 1 AND 74
  AND c.`副本地图ID` <> 229;

REPLACE INTO `creature`
(`id1`, `id2`, `id3`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
 `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
 `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
 `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
SELECT
  192000 + c.`章节ID`,
  0,
  0,
  c.`副本地图ID`,
  0,
  0,
  1,
  1,
  0,
  t.`target_position_x` + COS(t.`target_orientation` - 1.5707963) *
    CASE WHEN c.`副本地图ID` = 229 THEN 4.5 ELSE 2.5 END,
  t.`target_position_y` + SIN(t.`target_orientation` - 1.5707963) *
    CASE WHEN c.`副本地图ID` = 229 THEN 4.5 ELSE 2.5 END,
  t.`target_position_z`,
  t.`target_orientation` + 3.1415926,
  120,
  0,
  0,
  1000,
  0,
  0,
  2,
  0,
  0,
  '',
  NULL,
  0,
  CONCAT('深渊章节完成任务 NPC·章节', c.`章节ID`)
FROM `_深渊章节配置` c
JOIN `areatrigger_teleport` t
  ON t.`ID` = (
    SELECT MIN(t2.`ID`)
    FROM `areatrigger_teleport` t2
    WHERE t2.`target_map` = c.`副本地图ID`
  )
WHERE c.`章节ID` BETWEEN 1 AND 74;

-- 黑石塔上下层任务固定使用 192016 / 192017 这一组 NPC，单独固定站位，避免共享地图时刷错 entry。
UPDATE `creature` c
JOIN `areatrigger_teleport` t
  ON t.`ID` = (
    SELECT MIN(t2.`ID`)
    FROM `areatrigger_teleport` t2
    WHERE t2.`target_map` = 229
  )
SET
  c.`position_x` = t.`target_position_x` + COS(t.`target_orientation` + 1.5707963) * -1.5 + COS(t.`target_orientation`) * 1.4,
  c.`position_y` = t.`target_position_y` + SIN(t.`target_orientation` + 1.5707963) * -1.5 + SIN(t.`target_orientation`) * 1.4,
  c.`position_z` = t.`target_position_z`,
  c.`orientation` = t.`target_orientation` + 3.1415926
WHERE c.`id1` = 192016
  AND c.`map` = 229;

UPDATE `creature` c
JOIN `areatrigger_teleport` t
  ON t.`ID` = (
    SELECT MIN(t2.`ID`)
    FROM `areatrigger_teleport` t2
    WHERE t2.`target_map` = 229
  )
SET
  c.`position_x` = t.`target_position_x` + COS(t.`target_orientation` + 1.5707963) * 1.5 + COS(t.`target_orientation`) * 1.4,
  c.`position_y` = t.`target_position_y` + SIN(t.`target_orientation` + 1.5707963) * 1.5 + SIN(t.`target_orientation`) * 1.4,
  c.`position_z` = t.`target_position_z`,
  c.`orientation` = t.`target_orientation` + 3.1415926
WHERE c.`id1` = 192017
  AND c.`map` = 229;

-- 黑石塔(229)上下层只绑定到 192016 / 192017 这两个 NPC，自成一组。
REPLACE INTO `creature_queststarter`
(`id`, `quest`)
SELECT
  CASE
    WHEN c.`副本地图ID` = 229 THEN 192000 + c.`章节ID`
    ELSE 191000 + c.`章节ID`
  END,
  c.`起始任务ID`
FROM `_深渊章节配置` c
WHERE c.`起始任务ID` BETWEEN 700001 AND 700074;

REPLACE INTO `creature_queststarter`
(`id`, `quest`)
SELECT
  CASE
    WHEN c.`副本地图ID` = 229 THEN 192000 + c.`章节ID`
    ELSE 191000 + c.`章节ID`
  END,
  c.`完成任务ID`
FROM `_深渊章节配置` c
WHERE c.`完成任务ID` BETWEEN 710001 AND 710074;

REPLACE INTO `creature_queststarter`
(`id`, `quest`)
SELECT
  CASE
    WHEN c.`副本地图ID` = 229 THEN 192000 + c.`章节ID`
    ELSE 191000 + c.`章节ID`
  END,
  720000 + c.`章节ID`
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_queststarter`
(`id`, `quest`)
SELECT
  CASE
    WHEN c.`副本地图ID` = 229 THEN 192000 + c.`章节ID`
    ELSE 191000 + c.`章节ID`
  END,
  730000 + c.`章节ID`
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_queststarter`
(`id`, `quest`)
SELECT
  CASE
    WHEN c.`副本地图ID` = 229 THEN 192000 + c.`章节ID`
    ELSE 191000 + c.`章节ID`
  END,
  740000 + c.`章节ID`
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_questender`
(`id`, `quest`)
SELECT
  192000 + c.`章节ID`,
  c.`起始任务ID`
FROM `_深渊章节配置` c
WHERE c.`起始任务ID` BETWEEN 700001 AND 700074;

REPLACE INTO `creature_questender`
(`id`, `quest`)
SELECT
  192000 + c.`章节ID`,
  c.`完成任务ID`
FROM `_深渊章节配置` c
WHERE c.`完成任务ID` BETWEEN 710001 AND 710074;

REPLACE INTO `creature_questender`
(`id`, `quest`)
SELECT
  192000 + c.`章节ID`,
  720000 + c.`章节ID`
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_questender`
(`id`, `quest`)
SELECT
  192000 + c.`章节ID`,
  730000 + c.`章节ID`
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

REPLACE INTO `creature_questender`
(`id`, `quest`)
SELECT
  192000 + c.`章节ID`,
  740000 + c.`章节ID`
FROM `_深渊章节配置` c
WHERE c.`章节ID` BETWEEN 1 AND 74;

-- 秘藏首领改为每章唯一 entry，便于使用 creature_loot_template 按章节独立掉落
UPDATE `_深渊章节配置`
SET `秘藏首领入口` = 919000 + `章节ID`
WHERE `章节ID` BETWEEN 1 AND 74;

DELETE FROM `_深渊首领配置`
WHERE `首领类型` = 3
  AND `首领入口` BETWEEN 919001 AND 919074;

INSERT INTO `_深渊首领配置`
(`首领入口`, `首领名称`, `首领类型`, `幕ID`, `章节ID`, `生命倍率`, `伤害倍率`,
 `正传模式倍率`, `深渊模式倍率`, `腐化模式倍率`, `轮回模式倍率`,
 `阶段1血量阈值`, `阶段1技能组`, `阶段2血量阈值`, `阶段2技能组`,
 `阶段3血量阈值`, `阶段3技能组`, `场地效果`, `掉落包ID`, `出场文本`, `死亡文本`, `是否启用`)
SELECT
  919000 + c.`章节ID`,
  CONCAT('秘藏守藏者·', c.`章节名称`),
  3,
  c.`幕ID`,
  c.`章节ID`,
  t.`生命倍率`,
  t.`伤害倍率`,
  1.00,
  1.18,
  1.40,
  1.75,
  100,
  t.`阶段1技能组`,
  65,
  t.`阶段2技能组`,
  20,
  t.`阶段3技能组`,
  t.`场地效果`,
  4900 + c.`章节ID`,
  CONCAT('【', c.`章节名称`, '】的秘藏裂隙正在打开。'),
  CONCAT('【', c.`章节名称`, '】的秘藏守藏者已被击退。'),
  1
FROM `_深渊章节配置` c
JOIN (
  SELECT 1 AS `幕ID`, 2.45 AS `生命倍率`, 1.50 AS `伤害倍率`, '秘藏_灰烬_阶段1' AS `阶段1技能组`, '秘藏_灰烬_阶段2' AS `阶段2技能组`, '秘藏_灰烬_阶段3' AS `阶段3技能组`, '灰烬追猎' AS `场地效果`
  UNION ALL
  SELECT 2 AS `幕ID`, 2.60 AS `生命倍率`, 1.58 AS `伤害倍率`, '秘藏_虚空_阶段1' AS `阶段1技能组`, '秘藏_虚空_阶段2' AS `阶段2技能组`, '秘藏_虚空_阶段3' AS `阶段3技能组`, '虚空拆解' AS `场地效果`
  UNION ALL
  SELECT 3 AS `幕ID`, 2.75 AS `生命倍率`, 1.65 AS `伤害倍率`, '秘藏_冰脉_阶段1' AS `阶段1技能组`, '秘藏_冰脉_阶段2' AS `阶段2技能组`, '秘藏_冰脉_阶段3' AS `阶段3技能组`, '冰脉冻结' AS `场地效果`
  UNION ALL
  SELECT 4 AS `幕ID`, 2.95 AS `生命倍率`, 1.72 AS `伤害倍率`, '秘藏_耳语_阶段1' AS `阶段1技能组`, '秘藏_耳语_阶段2' AS `阶段2技能组`, '秘藏_耳语_阶段3' AS `阶段3技能组`, '耳语压迫' AS `场地效果`
  UNION ALL
  SELECT 5 AS `幕ID`, 3.15 AS `生命倍率`, 1.82 AS `伤害倍率`, '秘藏_日蚀_阶段1' AS `阶段1技能组`, '秘藏_日蚀_阶段2' AS `阶段2技能组`, '秘藏_日蚀_阶段3' AS `阶段3技能组`, '日蚀灼照' AS `场地效果`
  UNION ALL
  SELECT 6 AS `幕ID`, 3.35 AS `生命倍率`, 1.92 AS `伤害倍率`, '秘藏_统御_阶段1' AS `阶段1技能组`, '秘藏_统御_阶段2' AS `阶段2技能组`, '秘藏_统御_阶段3' AS `阶段3技能组`, '统御镇压' AS `场地效果`
) t ON t.`幕ID` = c.`幕ID`;

DROP TABLE IF EXISTS `_深渊自定义首领对接`;
CREATE TABLE `_深渊自定义首领对接` (
  `首领入口` int unsigned NOT NULL COMMENT '目标 creature_template.entry',
  `首领名称` varchar(96) NOT NULL DEFAULT '' COMMENT '首领名称',
  `首领类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '2=深渊首领 3=秘藏首领',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '所属幕',
  `章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '所属章节',
  `建议等级` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '建议等级',
  `建议阵营` smallint unsigned NOT NULL DEFAULT 14 COMMENT '建议 faction',
  `建议模型组` int unsigned NOT NULL DEFAULT 0 COMMENT '建议 modelid 预留组',
  `建议脚本名` varchar(64) NOT NULL DEFAULT '' COMMENT '建议 AI / ScriptName',
  `是否写入生物模板` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否需要写 creature_template',
  `是否写入掉落模板` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否需要写 creature_loot_template',
  `对接状态` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0待建 1已建 2已校验',
  `说明` varchar(255) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`首领入口`),
  KEY `索引_首领类型` (`首领类型`),
  KEY `索引_章节ID` (`章节ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊自定义首领对接';

DELETE FROM `_深渊自定义首领对接` WHERE `首领入口` BETWEEN 910001 AND 910074 OR `首领入口` BETWEEN 919001 AND 919074;
INSERT INTO `_深渊自定义首领对接`
(`首领入口`, `首领名称`, `首领类型`, `幕ID`, `章节ID`, `建议等级`, `建议阵营`, `建议模型组`, `建议脚本名`, `是否写入生物模板`, `是否写入掉落模板`, `对接状态`, `说明`)
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
  END,
  CASE WHEN `幕ID` <= 1 THEN 35 WHEN `幕ID` = 2 THEN 70 WHEN `幕ID` = 3 THEN 80 WHEN `幕ID` = 4 THEN 83 WHEN `幕ID` = 5 THEN 83 ELSE 83 END,
  14,
  0,
  '',
  1,
  1,
  0,
  CASE WHEN `首领类型` = 2 THEN '章节深渊首领，占位待写 creature_template 与 AI'
       WHEN `首领类型` = 3 THEN '秘藏首领，占位待写 creature_template 与 AI'
       ELSE '未分类' END
FROM `_深渊首领配置`
WHERE `首领类型` IN (2, 3);


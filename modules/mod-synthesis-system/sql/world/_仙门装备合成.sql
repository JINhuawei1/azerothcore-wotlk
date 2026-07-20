SET NAMES utf8mb4;

-- 仙门装备合成数据
-- 规则：当前阶同部位装备 x3 + 灵气石(62001) x10000 + 突破石(62002) x5000，合成下一阶同部位装备。
-- 覆盖范围：20 阶；生成 19 段升级链，每段 96 件，共 1824 条合成配置。
-- 每段包含：10 件仙器、10 职业 × 8 件仙装、5 件仙饰、1 件披风。
-- 已排除：武器、衬衫、战袍、旧通用 8 件套。
-- ID 号段：_模板_需求 / _模板_奖励 使用 951000-952823。

SET @xianmen_has_synthesis_class_type := (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_物品合成'
    AND COLUMN_NAME = '职业类型'
);
SET @xianmen_add_synthesis_class_type := IF(
  @xianmen_has_synthesis_class_type = 0,
  'ALTER TABLE `_物品合成` ADD COLUMN `职业类型` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT ''客户端职业类型：0通用/仙器，1战士，2法师，3牧师，4盗贼，5术士，6猎人，7萨满，8德鲁伊，9圣骑士，10死亡骑士'' AFTER `升级等级`',
  'SELECT 1'
);
PREPARE xianmen_add_synthesis_class_type_stmt FROM @xianmen_add_synthesis_class_type;
EXECUTE xianmen_add_synthesis_class_type_stmt;
DEALLOCATE PREPARE xianmen_add_synthesis_class_type_stmt;

SET @xianmen_has_synthesis_unlock_wear_level := (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_物品合成'
    AND COLUMN_NAME = '解锁穿戴等级'
);
SET @xianmen_add_synthesis_unlock_wear_level := IF(
  @xianmen_has_synthesis_unlock_wear_level = 0,
  'ALTER TABLE `_物品合成` ADD COLUMN `解锁穿戴等级` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT ''合成成功后授予玩家的穿戴等级；0表示不解锁'' AFTER `失败是否摧毁`',
  'SELECT 1'
);
PREPARE xianmen_add_synthesis_unlock_wear_level_stmt FROM @xianmen_add_synthesis_unlock_wear_level;
EXECUTE xianmen_add_synthesis_unlock_wear_level_stmt;
DEALLOCATE PREPARE xianmen_add_synthesis_unlock_wear_level_stmt;

DELETE FROM `_物品合成` WHERE `物品id` BETWEEN 95100 AND 99099;
DELETE FROM `_模板_需求` WHERE `id` BETWEEN 951000 AND 952823;
DELETE FROM `_模板_奖励` WHERE `id` BETWEEN 951000 AND 952823;

-- 合成列表的 tooltip 依赖 item_template；单独导入本文件时同步修正职业仙装限制。
UPDATE `item_template`
SET `AllowableClass` = CASE
    WHEN MOD(`entry` - 95200, 200) BETWEEN 20 AND 27 THEN 128
    WHEN MOD(`entry` - 95200, 200) BETWEEN 28 AND 35 THEN 256
    WHEN MOD(`entry` - 95200, 200) BETWEEN 36 AND 43 THEN 16
    WHEN MOD(`entry` - 95200, 200) BETWEEN 44 AND 51 THEN 1024
    WHEN MOD(`entry` - 95200, 200) BETWEEN 52 AND 59 THEN 8
    WHEN MOD(`entry` - 95200, 200) BETWEEN 60 AND 67 THEN 4
    WHEN MOD(`entry` - 95200, 200) BETWEEN 68 AND 75 THEN 64
    WHEN MOD(`entry` - 95200, 200) BETWEEN 76 AND 83 THEN 1
    WHEN MOD(`entry` - 95200, 200) BETWEEN 84 AND 91 THEN 32
    WHEN MOD(`entry` - 95200, 200) BETWEEN 92 AND 99 THEN 2
    ELSE `AllowableClass`
END
WHERE `entry` BETWEEN 95220 AND 99099
  AND MOD(`entry` - 95200, 200) BETWEEN 20 AND 99;

DROP TEMPORARY TABLE IF EXISTS `_xianmen_synthesis_stage`;
CREATE TEMPORARY TABLE `_xianmen_synthesis_stage` (
  `stage_index` tinyint UNSIGNED NOT NULL,
  `upgrade_level` tinyint UNSIGNED NOT NULL,
  `artifact_base` int UNSIGNED NOT NULL,
  `gear_base` int UNSIGNED NOT NULL,
  `next_artifact_base` int UNSIGNED NOT NULL,
  `next_gear_base` int UNSIGNED NOT NULL,
  PRIMARY KEY (`stage_index`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4;

INSERT INTO `_xianmen_synthesis_stage`
(`stage_index`, `upgrade_level`, `artifact_base`, `gear_base`, `next_artifact_base`, `next_gear_base`) VALUES
(0, 1, 95100, 95200, 95300, 95400),
(1, 2, 95300, 95400, 95500, 95600),
(2, 3, 95500, 95600, 95700, 95800),
(3, 4, 95700, 95800, 95900, 96000),
(4, 5, 95900, 96000, 96100, 96200),
(5, 6, 96100, 96200, 96300, 96400),
(6, 7, 96300, 96400, 96500, 96600),
(7, 8, 96500, 96600, 96700, 96800),
(8, 9, 96700, 96800, 96900, 97000),
(9, 10, 96900, 97000, 97100, 97200),
(10, 11, 97100, 97200, 97300, 97400),
(11, 12, 97300, 97400, 97500, 97600),
(12, 13, 97500, 97600, 97700, 97800),
(13, 14, 97700, 97800, 97900, 98000),
(14, 15, 97900, 98000, 98100, 98200),
(15, 16, 98100, 98200, 98300, 98400),
(16, 17, 98300, 98400, 98500, 98600),
(17, 18, 98500, 98600, 98700, 98800),
(18, 19, 98700, 98800, 98900, 99000);

DROP TEMPORARY TABLE IF EXISTS `_xianmen_synthesis_slot`;
CREATE TEMPORARY TABLE `_xianmen_synthesis_slot` (
  `seq` tinyint UNSIGNED NOT NULL,
  `entry_family` tinyint UNSIGNED NOT NULL COMMENT '1=仙器，2=仙装/仙饰/披风',
  `entry_offset` tinyint UNSIGNED NOT NULL,
  PRIMARY KEY (`seq`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4;

INSERT INTO `_xianmen_synthesis_slot` (`seq`, `entry_family`, `entry_offset`) VALUES
(0, 1, 0),
(1, 1, 1),
(2, 1, 2),
(3, 1, 3),
(4, 1, 4),
(5, 1, 5),
(6, 1, 6),
(7, 1, 7),
(8, 1, 8),
(9, 1, 9),
(10, 2, 20),
(11, 2, 21),
(12, 2, 22),
(13, 2, 23),
(14, 2, 24),
(15, 2, 25),
(16, 2, 26),
(17, 2, 27),
(18, 2, 28),
(19, 2, 29),
(20, 2, 30),
(21, 2, 31),
(22, 2, 32),
(23, 2, 33),
(24, 2, 34),
(25, 2, 35),
(26, 2, 36),
(27, 2, 37),
(28, 2, 38),
(29, 2, 39),
(30, 2, 40),
(31, 2, 41),
(32, 2, 42),
(33, 2, 43),
(34, 2, 44),
(35, 2, 45),
(36, 2, 46),
(37, 2, 47),
(38, 2, 48),
(39, 2, 49),
(40, 2, 50),
(41, 2, 51),
(42, 2, 52),
(43, 2, 53),
(44, 2, 54),
(45, 2, 55),
(46, 2, 56),
(47, 2, 57),
(48, 2, 58),
(49, 2, 59),
(50, 2, 60),
(51, 2, 61),
(52, 2, 62),
(53, 2, 63),
(54, 2, 64),
(55, 2, 65),
(56, 2, 66),
(57, 2, 67),
(58, 2, 68),
(59, 2, 69),
(60, 2, 70),
(61, 2, 71),
(62, 2, 72),
(63, 2, 73),
(64, 2, 74),
(65, 2, 75),
(66, 2, 76),
(67, 2, 77),
(68, 2, 78),
(69, 2, 79),
(70, 2, 80),
(71, 2, 81),
(72, 2, 82),
(73, 2, 83),
(74, 2, 84),
(75, 2, 85),
(76, 2, 86),
(77, 2, 87),
(78, 2, 88),
(79, 2, 89),
(80, 2, 90),
(81, 2, 91),
(82, 2, 92),
(83, 2, 93),
(84, 2, 94),
(85, 2, 95),
(86, 2, 96),
(87, 2, 97),
(88, 2, 98),
(89, 2, 99),
(90, 2, 8),
(91, 2, 9),
(92, 2, 10),
(93, 2, 11),
(94, 2, 12),
(95, 2, 17);

DROP TEMPORARY TABLE IF EXISTS `_xianmen_synthesis_recipe`;
CREATE TEMPORARY TABLE `_xianmen_synthesis_recipe` AS
SELECT
  951000 + stage.`stage_index` * 96 + slot.`seq` AS `template_id`,
  stage.`upgrade_level`,
  CASE
    WHEN slot.`entry_family` = 1 THEN 0
    WHEN slot.`entry_offset` BETWEEN 76 AND 83 THEN 1
    WHEN slot.`entry_offset` BETWEEN 20 AND 27 THEN 2
    WHEN slot.`entry_offset` BETWEEN 36 AND 43 THEN 3
    WHEN slot.`entry_offset` BETWEEN 52 AND 59 THEN 4
    WHEN slot.`entry_offset` BETWEEN 28 AND 35 THEN 5
    WHEN slot.`entry_offset` BETWEEN 60 AND 67 THEN 6
    WHEN slot.`entry_offset` BETWEEN 68 AND 75 THEN 7
    WHEN slot.`entry_offset` BETWEEN 44 AND 51 THEN 8
    WHEN slot.`entry_offset` BETWEEN 92 AND 99 THEN 9
    WHEN slot.`entry_offset` BETWEEN 84 AND 91 THEN 10
    ELSE 0
  END AS `class_type`,
  CASE slot.`entry_family`
    WHEN 1 THEN stage.`artifact_base` + slot.`entry_offset`
    ELSE stage.`gear_base` + slot.`entry_offset`
  END AS `source_entry`,
  CASE slot.`entry_family`
    WHEN 1 THEN stage.`next_artifact_base` + slot.`entry_offset`
    ELSE stage.`next_gear_base` + slot.`entry_offset`
  END AS `target_entry`
FROM `_xianmen_synthesis_stage` stage
JOIN `_xianmen_synthesis_slot` slot;

INSERT INTO `_模板_需求` (`注释`, `id`, `需要人物等级`, `是否消耗物品`, `消耗物品`, `客户端显示`)
SELECT
  CONCAT('仙门装备合成需求：', source_item.`name`, ' → ', target_item.`name`) AS `注释`,
  recipe.`template_id` AS `id`,
  '1' AS `需要人物等级`,
  0 AS `是否消耗物品`,
  CONCAT(recipe.`source_entry`, ' 3,62001 10000,62002 5000') AS `消耗物品`,
  CONCAT('消耗 ', source_item.`name`, ' x3、灵气石 x10000、突破石 x5000') AS `客户端显示`
FROM `_xianmen_synthesis_recipe` recipe
JOIN `item_template` source_item ON source_item.`entry` = recipe.`source_entry`
JOIN `item_template` target_item ON target_item.`entry` = recipe.`target_entry`
ORDER BY recipe.`template_id`;

INSERT INTO `_模板_奖励` (`id`, `注释`, `几率`, `奖励物品`, `客户端显示`)
SELECT
  recipe.`template_id` AS `id`,
  CONCAT('仙门装备合成奖励：', target_item.`name`) AS `注释`,
  100 AS `几率`,
  CONCAT(recipe.`target_entry`, ' 1') AS `奖励物品`,
  CONCAT('获得 ', target_item.`name`) AS `客户端显示`
FROM `_xianmen_synthesis_recipe` recipe
JOIN `item_template` source_item ON source_item.`entry` = recipe.`source_entry`
JOIN `item_template` target_item ON target_item.`entry` = recipe.`target_entry`
ORDER BY recipe.`template_id`;

INSERT INTO `_物品合成`
(`物品id`, `升级等级`, `职业类型`, `需求id`, `升级成功奖励id`, `成功几率`, `合成几率物品id`, `合成几率提升`, `失败是否摧毁`, `解锁穿戴等级`)
SELECT
  recipe.`source_entry` AS `物品id`,
  recipe.`upgrade_level` AS `升级等级`,
  recipe.`class_type` AS `职业类型`,
  recipe.`template_id` AS `需求id`,
  recipe.`template_id` AS `升级成功奖励id`,
  100 AS `成功几率`,
  0 AS `合成几率物品id`,
  0 AS `合成几率提升`,
  0 AS `失败是否摧毁`,
  recipe.`upgrade_level` + 1 AS `解锁穿戴等级`
FROM `_xianmen_synthesis_recipe` recipe
JOIN `item_template` source_item ON source_item.`entry` = recipe.`source_entry`
JOIN `item_template` target_item ON target_item.`entry` = recipe.`target_entry`
ORDER BY recipe.`template_id`
ON DUPLICATE KEY UPDATE
  `职业类型` = VALUES(`职业类型`),
  `需求id` = VALUES(`需求id`),
  `升级成功奖励id` = VALUES(`升级成功奖励id`),
  `成功几率` = VALUES(`成功几率`),
  `合成几率物品id` = VALUES(`合成几率物品id`),
  `合成几率提升` = VALUES(`合成几率提升`),
  `失败是否摧毁` = VALUES(`失败是否摧毁`),
  `解锁穿戴等级` = VALUES(`解锁穿戴等级`);

DROP TEMPORARY TABLE IF EXISTS `_xianmen_synthesis_recipe`;
DROP TEMPORARY TABLE IF EXISTS `_xianmen_synthesis_slot`;
DROP TEMPORARY TABLE IF EXISTS `_xianmen_synthesis_stage`;

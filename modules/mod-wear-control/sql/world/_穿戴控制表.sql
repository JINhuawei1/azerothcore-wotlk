SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS `_穿戴控制表` (
  `物品id` INT UNSIGNED NOT NULL COMMENT 'item_template.entry',
  `穿戴位置` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品InventoryType/穿戴部位提示，0=不限制',
  `槽位位置` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '目标系统槽位；0=任意槽',
  `穿戴需求` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联 _模板_需求.id，0=无需求；穿戴时只检查不消耗',
  `穿戴等级` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=不限制；大于0时玩家穿戴等级必须 >= 此值',
  `限制类型` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=无限制 1=飞升 2=仙器',
  `注释` VARCHAR(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`物品id`, `限制类型`, `槽位位置`),
  KEY `idx_穿戴控制_限制类型` (`限制类型`),
  KEY `idx_穿戴控制_槽位位置` (`槽位位置`),
  KEY `idx_穿戴控制_穿戴需求` (`穿戴需求`),
  KEY `idx_穿戴控制_穿戴等级` (`穿戴等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='跨系统物品穿戴限制表';

SET @wear_control_has_wear_level := (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_穿戴控制表'
    AND COLUMN_NAME = '穿戴等级'
);
SET @wear_control_add_wear_level := IF(
  @wear_control_has_wear_level = 0,
  'ALTER TABLE `_穿戴控制表` ADD COLUMN `穿戴等级` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT ''0=不限制；大于0时玩家穿戴等级必须 >= 此值'' AFTER `穿戴需求`',
  'SELECT 1'
);
PREPARE wear_control_add_wear_level_stmt FROM @wear_control_add_wear_level;
EXECUTE wear_control_add_wear_level_stmt;
DEALLOCATE PREPARE wear_control_add_wear_level_stmt;

-- 飞升专属：沿用 _深渊装备模板 的启用物品，槽位按飞升系统 0-17 编号。
INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT
  it.`entry`,
  it.`InventoryType`,
  CASE it.`InventoryType`
    WHEN 1 THEN 0
    WHEN 2 THEN 1
    WHEN 3 THEN 2
    WHEN 4 THEN 3
    WHEN 5 THEN 4
    WHEN 20 THEN 4
    WHEN 6 THEN 5
    WHEN 7 THEN 6
    WHEN 8 THEN 7
    WHEN 9 THEN 8
    WHEN 10 THEN 9
    WHEN 11 THEN 10
    WHEN 12 THEN 12
    WHEN 16 THEN 14
    WHEN 13 THEN 15
    WHEN 17 THEN 15
    WHEN 21 THEN 15
    WHEN 14 THEN 16
    WHEN 22 THEN 16
    WHEN 23 THEN 16
    WHEN 15 THEN 17
    WHEN 25 THEN 17
    WHEN 26 THEN 17
    WHEN 28 THEN 17
    ELSE 0
  END,
  0,
  0,
  1,
  CONCAT('飞升专属：', it.`name`)
FROM `_深渊装备模板` e
JOIN `item_template` it ON it.`entry` = e.`物品模板ID`
WHERE e.`是否启用` = 1
  AND it.`InventoryType` NOT IN (11, 12)
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, 0, 1, CONCAT('飞升专属：', it.`name`)
FROM `_深渊装备模板` e
JOIN `item_template` it ON it.`entry` = e.`物品模板ID`
JOIN (
  SELECT 10 AS `slot_id`
  UNION ALL SELECT 11
) slots
WHERE e.`是否启用` = 1
  AND it.`InventoryType` = 11
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, 0, 1, CONCAT('飞升专属：', it.`name`)
FROM `_深渊装备模板` e
JOIN `item_template` it ON it.`entry` = e.`物品模板ID`
JOIN (
  SELECT 12 AS `slot_id`
  UNION ALL SELECT 13
) slots
WHERE e.`是否启用` = 1
  AND it.`InventoryType` = 12
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

-- 仙器槽 1-10：沿用 _仙门_仙器物品 的登记槽位。
INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT
  x.`物品ID`,
  COALESCE(it.`InventoryType`, 0),
  x.`仙器槽位ID`,
  0,
  CASE
    WHEN x.`物品ID` BETWEEN 95100 AND 99099 THEN FLOOR((x.`物品ID` - 95100) / 200) + 1
    ELSE 0
  END,
  2,
  CONCAT('仙器专属：', COALESCE(NULLIF(x.`备注`, ''), it.`name`, CAST(x.`物品ID` AS CHAR)))
FROM `_仙门_仙器物品` x
LEFT JOIN `item_template` it ON it.`entry` = x.`物品ID`
WHERE x.`仙器槽位ID` BETWEEN 1 AND 10
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

-- 仙器系统扩展槽 11-29：仙门装备号段中未登记为仙器槽的装备，按 InventoryType 映射。
INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT
  it.`entry`,
  it.`InventoryType`,
  CASE it.`InventoryType`
    WHEN 1 THEN 11
    WHEN 2 THEN 12
    WHEN 3 THEN 13
    WHEN 4 THEN 14
    WHEN 5 THEN 15
    WHEN 20 THEN 15
    WHEN 6 THEN 16
    WHEN 7 THEN 17
    WHEN 8 THEN 18
    WHEN 9 THEN 19
    WHEN 10 THEN 20
    WHEN 16 THEN 25
    WHEN 15 THEN 28
    WHEN 25 THEN 28
    WHEN 26 THEN 28
    WHEN 28 THEN 28
    WHEN 19 THEN 29
    ELSE 0
  END,
  0,
  FLOOR((it.`entry` - 95100) / 200) + 1,
  2,
  CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
WHERE it.`entry` BETWEEN 95100 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` IN (1, 2, 3, 4, 5, 20, 6, 7, 8, 9, 10, 16, 15, 25, 26, 28, 19)
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, FLOOR((it.`entry` - 95100) / 200) + 1, 2, CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
JOIN (
  SELECT 21 AS `slot_id`
  UNION ALL SELECT 22
) slots
WHERE it.`entry` BETWEEN 95100 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` = 11
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, FLOOR((it.`entry` - 95100) / 200) + 1, 2, CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
JOIN (
  SELECT 23 AS `slot_id`
  UNION ALL SELECT 24
) slots
WHERE it.`entry` BETWEEN 95100 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` = 12
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, FLOOR((it.`entry` - 95100) / 200) + 1, 2, CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
JOIN (
  SELECT 26 AS `slot_id`
  UNION ALL SELECT 27
) slots
WHERE it.`entry` BETWEEN 95100 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` IN (13, 14, 17, 21, 22, 23)
ON DUPLICATE KEY UPDATE
  `穿戴位置` = VALUES(`穿戴位置`),
  `穿戴需求` = VALUES(`穿戴需求`),
  `穿戴等级` = VALUES(`穿戴等级`),
  `注释` = VALUES(`注释`);

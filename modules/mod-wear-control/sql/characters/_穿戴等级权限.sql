SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS `_穿戴等级权限` (
  `玩家GUID` INT UNSIGNED NOT NULL COMMENT 'characters.guid',
  `限制类型` TINYINT UNSIGNED NOT NULL COMMENT '1=飞升，2=仙器',
  `槽位位置` TINYINT UNSIGNED NOT NULL COMMENT '目标系统的独立槽位编号',
  `穿戴等级` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '该槽位已解锁的最高穿戴等级；无记录时默认1级',
  `更新时间` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`玩家GUID`, `限制类型`, `槽位位置`),
  KEY `idx_穿戴等级权限_穿戴等级` (`穿戴等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家各专属槽位的独立穿戴等级权限';

SET @wear_permission_has_limit_type := (
  SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_穿戴等级权限' AND COLUMN_NAME = '限制类型'
);
SET @wear_permission_add_limit_type := IF(
  @wear_permission_has_limit_type = 0,
  'ALTER TABLE `_穿戴等级权限` ADD COLUMN `限制类型` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `玩家GUID`',
  'SELECT 1'
);
PREPARE wear_permission_stmt FROM @wear_permission_add_limit_type;
EXECUTE wear_permission_stmt;
DEALLOCATE PREPARE wear_permission_stmt;

SET @wear_permission_has_slot := (
  SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_穿戴等级权限' AND COLUMN_NAME = '槽位位置'
);
SET @wear_permission_add_slot := IF(
  @wear_permission_has_slot = 0,
  'ALTER TABLE `_穿戴等级权限` ADD COLUMN `槽位位置` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `限制类型`',
  'SELECT 1'
);
PREPARE wear_permission_stmt FROM @wear_permission_add_slot;
EXECUTE wear_permission_stmt;
DEALLOCATE PREPARE wear_permission_stmt;

-- 旧版只有玩家全局等级，无法判断来自哪个槽位；不再让这类记录影响任何仙器槽位。
DELETE FROM `_穿戴等级权限` WHERE `限制类型` = 0 OR `槽位位置` = 0;

SET @wear_permission_primary_key := (
  SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION SEPARATOR ',')
  FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_穿戴等级权限' AND CONSTRAINT_NAME = 'PRIMARY'
);
SET @wear_permission_upgrade_primary_key := IF(
  COALESCE(@wear_permission_primary_key, '') <> '玩家GUID,限制类型,槽位位置',
  'ALTER TABLE `_穿戴等级权限` DROP PRIMARY KEY, ADD PRIMARY KEY (`玩家GUID`, `限制类型`, `槽位位置`)',
  'SELECT 1'
);
PREPARE wear_permission_stmt FROM @wear_permission_upgrade_primary_key;
EXECUTE wear_permission_stmt;
DEALLOCATE PREPARE wear_permission_stmt;

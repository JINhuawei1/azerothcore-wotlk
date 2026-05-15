-- ============================================
-- 修仙系统 - 玩家数据表
-- ============================================

CREATE TABLE IF NOT EXISTS `_玩家修仙数据` (
  `角色id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家GUID',
  `修仙等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前修仙等级(0-100)',
  `渡劫冷却时间` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '渡劫冷却截止时间戳',
  `渡劫通过等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '已通关但尚未通过插件请求突破的境界等级',
  PRIMARY KEY (`角色id`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家修仙数据' ROW_FORMAT = DYNAMIC;

SET @stmt := (
  SELECT IF(
    COUNT(*) = 0,
    'ALTER TABLE `_玩家修仙数据` ADD COLUMN `渡劫通过等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT ''已通关但尚未通过插件请求突破的境界等级'' AFTER `渡劫冷却时间`',
    'SELECT 1'
  )
  FROM `information_schema`.`COLUMNS`
  WHERE `TABLE_SCHEMA` = DATABASE()
    AND `TABLE_NAME` = '_玩家修仙数据'
    AND `COLUMN_NAME` = '渡劫通过等级'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

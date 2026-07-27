-- ============================================================
-- 宣传奖励系统 - 玩家分活动累计次数表 (characters 数据库)
-- 奖励组就是活动标识：旧宣传神器=9001，新宣传神武实验=1。
-- ============================================================

CREATE TABLE IF NOT EXISTS `_宣传奖励系统玩家` (
  `奖励组`   int UNSIGNED NOT NULL DEFAULT 9001 COMMENT '对应 world._宣传奖励系统.对接奖励组',
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.guid',
  `宣传天数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '该奖励组累计兑换次数，1张CDK=1级',
  PRIMARY KEY (`奖励组`,`玩家GUID`) USING BTREE,
  KEY `idx_宣传奖励系统玩家_GUID` (`玩家GUID`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci
COMMENT = '宣传奖励-玩家按奖励组累计次数';

-- 兼容旧版：旧表只有 玩家GUID/宣传天数，补字段时全部归入旧组9001。
SET @promotion_player_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统玩家` ADD COLUMN `奖励组` int UNSIGNED NOT NULL DEFAULT 9001 COMMENT ''对应 world._宣传奖励系统.对接奖励组'' FIRST',
    'SELECT 1')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统玩家' AND COLUMN_NAME = '奖励组'
);
PREPARE promotion_player_schema_stmt FROM @promotion_player_schema_sql;
EXECUTE promotion_player_schema_stmt;
DEALLOCATE PREPARE promotion_player_schema_stmt;

SET @promotion_player_primary_columns = (
  SELECT GROUP_CONCAT(`COLUMN_NAME` ORDER BY `SEQ_IN_INDEX` SEPARATOR ',')
  FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统玩家' AND INDEX_NAME = 'PRIMARY'
);
SET @promotion_player_schema_sql = IF(
  @promotion_player_primary_columns = '奖励组,玩家GUID',
  'SELECT 1',
  'ALTER TABLE `_宣传奖励系统玩家` DROP PRIMARY KEY, ADD PRIMARY KEY (`奖励组`,`玩家GUID`) USING BTREE'
);
PREPARE promotion_player_schema_stmt FROM @promotion_player_schema_sql;
EXECUTE promotion_player_schema_stmt;
DEALLOCATE PREPARE promotion_player_schema_stmt;

SET @promotion_player_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统玩家` ADD KEY `idx_宣传奖励系统玩家_GUID` (`玩家GUID`) USING BTREE',
    'SELECT 1')
  FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统玩家'
    AND INDEX_NAME = 'idx_宣传奖励系统玩家_GUID'
);
PREPARE promotion_player_schema_stmt FROM @promotion_player_schema_sql;
EXECUTE promotion_player_schema_stmt;
DEALLOCATE PREPARE promotion_player_schema_stmt;

ALTER TABLE `_宣传奖励系统玩家`
  MODIFY COLUMN `奖励组` int UNSIGNED NOT NULL DEFAULT 9001 COMMENT '对应 world._宣传奖励系统.对接奖励组',
  MODIFY COLUMN `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.guid',
  MODIFY COLUMN `宣传天数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '该奖励组累计兑换次数，1张CDK=1级',
  COMMENT = '宣传奖励-玩家按奖励组累计次数';

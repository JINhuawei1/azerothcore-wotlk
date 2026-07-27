-- ============================================================
-- 宣传奖励系统 - 多活动配置表 (world 数据库)
-- 同一套系统可并存多个 CDK 奖励组，每组独立维护武器范围、等级上限和玩家次数。
-- ============================================================

CREATE TABLE IF NOT EXISTS `_宣传奖励系统` (
  `id`             int UNSIGNED NOT NULL COMMENT '活动配置ID',
  `注释`           varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `启用`           tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=禁用，1=启用',
  `默认活动`       tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT 'GM发码和插件界面默认使用的活动',
  `武器名称`       varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '宣传武器',
  `武器entry`      int UNSIGNED NOT NULL COMMENT '第1级物品entry，后续等级按连续entry递增',
  `最大等级`       int UNSIGNED NOT NULL DEFAULT 1 COMMENT '本活动武器总等级数',
  `初始全属性值`   decimal(65,0) NOT NULL DEFAULT 0 COMMENT '第1级全属性数值；物品模板缺失时作为回退',
  `每日增量`       decimal(65,0) NOT NULL DEFAULT 0 COMMENT '线性回退增量；0表示优先按每级物品模板属性',
  `对接奖励组`     int UNSIGNED NOT NULL COMMENT '_奖励_兑换码.组，同时作为玩家成长活动标识',
  `对接需求ID`     int UNSIGNED NOT NULL DEFAULT 0 COMMENT '_奖励_兑换码.需求，0=无需求',
  `对接奖励ID`     int UNSIGNED NOT NULL COMMENT '第1级对应的 _模板_奖励.id，后续等级连续递增',
  `领取公告`       int UNSIGNED NOT NULL DEFAULT 27 COMMENT '_奖励_兑换码.领取公告',
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `uk_宣传奖励系统_奖励组` (`对接奖励组`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci
COMMENT = '宣传奖励系统多活动配置' ROW_FORMAT = DYNAMIC;

-- 兼容旧版单活动表：按需补齐新字段，不删除任何既有配置或玩家数据。
SET @promotion_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统` ADD COLUMN `启用` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT ''0=禁用，1=启用'' AFTER `注释`',
    'SELECT 1')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统' AND COLUMN_NAME = '启用'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统` ADD COLUMN `默认活动` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT ''GM发码和插件界面默认使用的活动'' AFTER `启用`',
    'SELECT 1')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统' AND COLUMN_NAME = '默认活动'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统` ADD COLUMN `武器名称` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT ''宣传武器'' AFTER `默认活动`',
    'SELECT 1')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统' AND COLUMN_NAME = '武器名称'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统` ADD COLUMN `最大等级` int UNSIGNED NOT NULL DEFAULT 1000 COMMENT ''本活动武器总等级数'' AFTER `武器entry`',
    'SELECT 1')
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统' AND COLUMN_NAME = '最大等级'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

SET @promotion_schema_sql = (
  SELECT IF(COUNT(*) = 0,
    'ALTER TABLE `_宣传奖励系统` ADD UNIQUE KEY `uk_宣传奖励系统_奖励组` (`对接奖励组`)',
    'SELECT 1')
  FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传奖励系统'
    AND INDEX_NAME = 'uk_宣传奖励系统_奖励组'
);
PREPARE promotion_schema_stmt FROM @promotion_schema_sql;
EXECUTE promotion_schema_stmt;
DEALLOCATE PREPARE promotion_schema_stmt;

ALTER TABLE `_宣传奖励系统`
  MODIFY COLUMN `id` int UNSIGNED NOT NULL COMMENT '活动配置ID',
  MODIFY COLUMN `武器entry` int UNSIGNED NOT NULL COMMENT '第1级物品entry，后续等级按连续entry递增',
  MODIFY COLUMN `初始全属性值` decimal(65,0) NOT NULL DEFAULT 0 COMMENT '第1级全属性数值；物品模板缺失时作为回退',
  MODIFY COLUMN `每日增量` decimal(65,0) NOT NULL DEFAULT 0 COMMENT '线性回退增量；0表示优先按每级物品模板属性',
  MODIFY COLUMN `对接奖励组` int UNSIGNED NOT NULL COMMENT '_奖励_兑换码.组，同时作为玩家成长活动标识',
  MODIFY COLUMN `对接奖励ID` int UNSIGNED NOT NULL COMMENT '第1级对应的 _模板_奖励.id，后续等级连续递增';

-- 旧宣传神器活动：保留组9001、物品997001-998000、奖励模板100-1099。
INSERT INTO `_宣传奖励系统`
  (`id`,`注释`,`启用`,`默认活动`,`武器名称`,`武器entry`,`最大等级`,
   `初始全属性值`,`每日增量`,`对接奖励组`,`对接需求ID`,`对接奖励ID`,`领取公告`)
VALUES
  (1,'旧宣传神器：997001-998000，奖励100-1099',1,0,'宣传神器',997001,1000,
   1000000,1000000,9001,0,100,27)
ON DUPLICATE KEY UPDATE
  `注释`=VALUES(`注释`),
  `启用`=VALUES(`启用`),
  `默认活动`=VALUES(`默认活动`),
  `武器名称`=VALUES(`武器名称`),
  `武器entry`=VALUES(`武器entry`),
  `最大等级`=VALUES(`最大等级`),
  `初始全属性值`=VALUES(`初始全属性值`),
  `每日增量`=VALUES(`每日增量`),
  `对接奖励组`=VALUES(`对接奖励组`),
  `对接需求ID`=VALUES(`对接需求ID`),
  `对接奖励ID`=VALUES(`对接奖励ID`),
  `领取公告`=VALUES(`领取公告`);

-- 新宣传神武实验活动：组1、物品450001-450200、奖励模板1100-1299。
-- 1级1亿、2级10亿、3级100亿，随后倍率逐级平滑降低，第200级精确达到9999垓。
INSERT IGNORE INTO `_宣传奖励系统`
  (`id`,`注释`,`启用`,`默认活动`,`武器名称`,`武器entry`,`最大等级`,
   `初始全属性值`,`每日增量`,`对接奖励组`,`对接需求ID`,`对接奖励ID`,`领取公告`)
VALUES
  (2,'宣传神武实验：200级，第200级全属性9999垓',1,1,'宣传神武',450001,200,
   100000000,0,1,0,1100,27);

-- 重复导入只更新本模块已确认拥有的 id=2 / 组1，不覆盖任何冲突活动。
UPDATE `_宣传奖励系统`
SET `注释`='宣传神武实验：200级，第200级全属性9999垓',
    `启用`=1,
    `默认活动`=1,
    `武器名称`='宣传神武',
    `武器entry`=450001,
    `最大等级`=200,
    `初始全属性值`=100000000,
    `每日增量`=0,
    `对接需求ID`=0,
    `对接奖励ID`=1100,
    `领取公告`=27
WHERE `id`=2 AND `对接奖励组`=1;

-- ============================================
-- 仙门系统 - 角色库数据
-- ============================================

CREATE TABLE IF NOT EXISTS `_仙门_玩家` (
  `角色GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `修为等级` smallint UNSIGNED NOT NULL DEFAULT 1,
  `当日贡献` bigint UNSIGNED NOT NULL DEFAULT 0,
  `历史贡献` decimal(39,0) NOT NULL DEFAULT 0,
  `已解锁技能` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '逗号分隔技能ID，最多10个',
  `加入时间` int UNSIGNED NOT NULL DEFAULT 0,
  `更新时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`角色GUID`) USING BTREE,
  KEY `idx_仙门玩家_门派贡献` (`门派ID`, `当日贡献`)
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门玩家数据' ROW_FORMAT = DYNAMIC;

SET @xianmen_add_history_contribution_column = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_玩家' AND COLUMN_NAME = '历史贡献') = 0,
  'ALTER TABLE `_仙门_玩家` ADD COLUMN `历史贡献` decimal(39,0) NOT NULL DEFAULT 0 AFTER `当日贡献`',
  'SELECT 1'
);
PREPARE xianmen_add_history_contribution_column_stmt FROM @xianmen_add_history_contribution_column;
EXECUTE xianmen_add_history_contribution_column_stmt;
DEALLOCATE PREPARE xianmen_add_history_contribution_column_stmt;

CREATE TABLE IF NOT EXISTS `_仙门_门派状态` (
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `当前门主GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `结算日期` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'YYYYMMDD',
  `结算时间` int UNSIGNED NOT NULL DEFAULT 0,
  `激活技能` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '门主选定生效技能ID，逗号分隔，最多5个',
  `更新时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`门派ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门门派运行状态' ROW_FORMAT = DYNAMIC;

INSERT IGNORE INTO `_仙门_门派状态` (`门派ID`, `当前门主GUID`, `结算日期`, `结算时间`, `激活技能`, `更新时间`) VALUES
(1, 0, 0, 0, '', 0), (2, 0, 0, 0, '', 0), (3, 0, 0, 0, '', 0), (4, 0, 0, 0, '', 0), (5, 0, 0, 0, '', 0);

CREATE TABLE IF NOT EXISTS `_仙门_当日日常` (
  `日常ID` int UNSIGNED NOT NULL AUTO_INCREMENT,
  `日期` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'YYYYMMDD',
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `模板ID` int UNSIGNED NOT NULL DEFAULT 0,
  `发布者GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `目标数量` int UNSIGNED NOT NULL DEFAULT 1,
  `奖励档` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `奖励类型` tinyint UNSIGNED NOT NULL DEFAULT 2,
  `创建时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`日常ID`) USING BTREE,
  KEY `idx_仙门当日日常_日期门派` (`日期`, `门派ID`)
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门门主发布的当日日常' ROW_FORMAT = DYNAMIC;

CREATE TABLE IF NOT EXISTS `_仙门_玩家日常记录` (
  `角色GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `日常ID` int UNSIGNED NOT NULL DEFAULT 0,
  `日期` int UNSIGNED NOT NULL DEFAULT 0,
  `进度` int UNSIGNED NOT NULL DEFAULT 0,
  `已完成` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `完成时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`角色GUID`, `日常ID`, `日期`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门玩家日常进度' ROW_FORMAT = DYNAMIC;

CREATE TABLE IF NOT EXISTS `_仙门_仙器玩家槽位` (
  `角色GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `槽位ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `物品GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `已解锁` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `更新时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`角色GUID`, `槽位ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门仙器与扩展装备槽玩家数据' ROW_FORMAT = DYNAMIC;

CREATE TABLE IF NOT EXISTS `_仙门_玩家丹药属性` (
  `角色GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `属性类型` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ItemModType/自定义属性ID，逻辑以数值为准',
  `当前数值` decimal(39,0) NOT NULL DEFAULT 0,
  `更新时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`角色GUID`, `属性类型`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门玩家永久丹药属性' ROW_FORMAT = DYNAMIC;

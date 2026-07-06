-- ============================================
-- 仙门系统 - 角色库数据
-- ============================================

CREATE TABLE IF NOT EXISTS `_仙门_玩家` (
  `角色GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `修为等级` smallint UNSIGNED NOT NULL DEFAULT 1,
  `当日贡献` bigint UNSIGNED NOT NULL DEFAULT 0,
  `历史贡献` decimal(65,0) NOT NULL DEFAULT 0,
  `已解锁技能` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '逗号分隔技能ID，最多10个',
  `个人生效技能` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '成员自选实际生效技能ID，逗号分隔，最多5个',
  `加入时间` int UNSIGNED NOT NULL DEFAULT 0,
  `更新时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`角色GUID`) USING BTREE,
  KEY `idx_仙门玩家_门派贡献` (`门派ID`, `当日贡献`)
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门玩家数据' ROW_FORMAT = DYNAMIC;

SET @xianmen_add_history_contribution_column = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_玩家' AND COLUMN_NAME = '历史贡献') = 0,
  'ALTER TABLE `_仙门_玩家` ADD COLUMN `历史贡献` decimal(65,0) NOT NULL DEFAULT 0 AFTER `当日贡献`',
  'SELECT 1'
);
PREPARE xianmen_add_history_contribution_column_stmt FROM @xianmen_add_history_contribution_column;
EXECUTE xianmen_add_history_contribution_column_stmt;
DEALLOCATE PREPARE xianmen_add_history_contribution_column_stmt;

SET @xianmen_add_personal_active_column = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_玩家' AND COLUMN_NAME = '个人生效技能') = 0,
  'ALTER TABLE `_仙门_玩家` ADD COLUMN `个人生效技能` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '''' COMMENT ''成员自选实际生效技能ID，逗号分隔，最多5个'' AFTER `已解锁技能`',
  'SELECT 1'
);
PREPARE xianmen_add_personal_active_column_stmt FROM @xianmen_add_personal_active_column;
EXECUTE xianmen_add_personal_active_column_stmt;
DEALLOCATE PREPARE xianmen_add_personal_active_column_stmt;

CREATE TABLE IF NOT EXISTS `_仙门_门派状态` (
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `当前门主GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `结算日期` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'YYYYMMDD',
  `结算时间` int UNSIGNED NOT NULL DEFAULT 0,
  `激活技能` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '门主开放技能ID，逗号分隔，最多10个',
  `更新时间` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`门派ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门门派运行状态' ROW_FORMAT = DYNAMIC;

DELETE FROM `_仙门_门派状态` WHERE `门派ID` = 2;
DELETE FROM `_仙门_玩家` WHERE `门派ID` = 2;

UPDATE `_仙门_门派状态` SET `门派ID` = 102 WHERE `门派ID` = 3;
UPDATE `_仙门_门派状态` SET `门派ID` = 103 WHERE `门派ID` = 4;
UPDATE `_仙门_门派状态` SET `门派ID` = 104 WHERE `门派ID` = 5;
UPDATE `_仙门_门派状态` SET `门派ID` = 2 WHERE `门派ID` = 102;
UPDATE `_仙门_门派状态` SET `门派ID` = 3 WHERE `门派ID` = 103;
UPDATE `_仙门_门派状态` SET `门派ID` = 4 WHERE `门派ID` = 104;
DELETE FROM `_仙门_门派状态` WHERE `门派ID` NOT IN (1, 2, 3, 4);

UPDATE `_仙门_玩家` SET `门派ID` = 2 WHERE `门派ID` = 3;
UPDATE `_仙门_玩家` SET `门派ID` = 3 WHERE `门派ID` = 4;
UPDATE `_仙门_玩家` SET `门派ID` = 4 WHERE `门派ID` = 5;

INSERT IGNORE INTO `_仙门_门派状态` (`门派ID`, `当前门主GUID`, `结算日期`, `结算时间`, `激活技能`, `更新时间`) VALUES
(1, 0, 0, 0, '', 0), (2, 0, 0, 0, '', 0), (3, 0, 0, 0, '', 0), (4, 0, 0, 0, '', 0);

UPDATE `_仙门_玩家` p
JOIN `_仙门_门派状态` s ON s.`门派ID` = p.`门派ID`
SET p.`个人生效技能` = s.`激活技能`
WHERE p.`个人生效技能` = '' AND s.`激活技能` <> '';

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

DELETE r FROM `_仙门_玩家日常记录` r
JOIN `_仙门_当日日常` d ON d.`日常ID` = r.`日常ID` AND d.`日期` = r.`日期`
WHERE d.`门派ID` = 2;
DELETE FROM `_仙门_当日日常` WHERE `门派ID` = 2;
UPDATE `_仙门_当日日常` SET `门派ID` = 2 WHERE `门派ID` = 3;
UPDATE `_仙门_当日日常` SET `门派ID` = 3 WHERE `门派ID` = 4;
UPDATE `_仙门_当日日常` SET `门派ID` = 4 WHERE `门派ID` = 5;

-- ============================================
-- 仙器玩家槽位：每玩家一行，每个槽位一列（`槽位N` = 1 表示该槽位已解锁，0 表示未解锁）
-- 手动改库时一目了然：要给玩家开第 5 槽，把 `槽位5` 改成 1 即可
-- ============================================

-- 旧结构（多行 `槽位ID` 版 / 单行 `已解锁掩码` 版）存在时直接重建（不迁移旧解锁数据）
SET @xianmen_xianqi_rebuild = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_仙器玩家槽位') = 1
  AND (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_仙器玩家槽位' AND COLUMN_NAME = '槽位1') = 0,
  'DROP TABLE `_仙门_仙器玩家槽位`', 'SELECT 1');
PREPARE xianmen_xianqi_stmt FROM @xianmen_xianqi_rebuild; EXECUTE xianmen_xianqi_stmt; DEALLOCATE PREPARE xianmen_xianqi_stmt;

CREATE TABLE IF NOT EXISTS `_仙门_仙器玩家槽位` (
  `角色GUID` int UNSIGNED NOT NULL DEFAULT 0,
  `槽位1` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位2` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位3` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位4` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位5` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位6` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位7` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位8` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位9` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位10` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位11` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位12` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位13` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位14` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位15` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位16` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位17` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位18` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位19` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位20` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位21` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位22` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位23` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位24` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位25` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位26` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位27` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位28` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位29` tinyint UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`角色GUID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门仙器与扩展装备槽玩家数据(每槽位一列,1=解锁)' ROW_FORMAT = DYNAMIC;

DROP TABLE IF EXISTS `_仙门_玩家丹药属性`;

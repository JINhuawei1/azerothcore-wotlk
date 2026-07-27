-- 宣传审核账号授权表
-- 仅保存 auth.account 的账号 ID 与授权元数据；绝不保存密码、salt 或 verifier。
-- 可重复导入，INSERT IGNORE 不覆盖网站后来修改过的授权状态。

CREATE TABLE IF NOT EXISTS `_宣传审核账号` (
  `账号ID`             INT UNSIGNED NOT NULL COMMENT '关联 auth.account.id',
  `权限角色`           VARCHAR(16) NOT NULL DEFAULT 'REVIEWER' COMMENT 'MANAGER=授权主管，REVIEWER=宣传审核员',
  `启用`               TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=停用，1=启用',
  `备注`               VARCHAR(255) NOT NULL DEFAULT '',
  `授权人账号ID`       INT UNSIGNED NOT NULL DEFAULT 0,
  `授权时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `更新时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`账号ID`),
  KEY `idx_宣传审核账号_角色状态` (`权限角色`, `启用`),
  KEY `idx_宣传审核账号_授权人` (`授权人账号ID`)
)
COMMENT = '宣传审核网站授权账号，不保存密码'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

CREATE TABLE IF NOT EXISTS `_宣传审核账号日志` (
  `日志ID`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `目标账号ID`         INT UNSIGNED NOT NULL,
  `操作类型`           VARCHAR(24) NOT NULL COMMENT 'ADD/UPDATE/ENABLE/DISABLE/DELETE',
  `操作前状态`         LONGTEXT NOT NULL,
  `操作后状态`         LONGTEXT NOT NULL,
  `操作人账号ID`       INT UNSIGNED NOT NULL DEFAULT 0,
  `操作人名称`         VARCHAR(64) NOT NULL DEFAULT '',
  `操作理由`           VARCHAR(1024) NOT NULL DEFAULT '',
  `操作IP`             VARCHAR(45) NOT NULL DEFAULT '',
  `操作时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`日志ID`),
  KEY `idx_宣传审核账号日志_目标时间` (`目标账号ID`, `操作时间`),
  KEY `idx_宣传审核账号日志_操作人时间` (`操作人账号ID`, `操作时间`)
)
COMMENT = '宣传审核账号授权变更审计'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

-- 只在账号存在且尚未授权时建立初始主管；不覆盖后续网站配置。
INSERT IGNORE INTO `_宣传审核账号`
  (`账号ID`, `权限角色`, `启用`, `备注`, `授权人账号ID`)
SELECT `id`, 'MANAGER', 1, '初始宣传审核授权主管', 0
FROM `account`
WHERE UPPER(`username`) = 'QQ5354414'
LIMIT 1;

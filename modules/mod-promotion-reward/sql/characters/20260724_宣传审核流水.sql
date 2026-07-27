-- ============================================================
-- 自动宣传审核 - 提交、奖励、兑换、统计与审核流水 (characters 数据库)
-- 幂等迁移：只创建新表，不删除或改写现有宣传奖励数据。
-- ============================================================

CREATE TABLE IF NOT EXISTS `_宣传全局状态` (
  `状态ID`             TINYINT UNSIGNED NOT NULL COMMENT '固定为1的全局状态行',
  `当前限额周期`       BIGINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '每次全服重置后递增',
  `限额重置时间`       DATETIME NULL,
  `最后操作人账号ID`   INT UNSIGNED NOT NULL DEFAULT 0,
  `最后操作人名称`     VARCHAR(64) NOT NULL DEFAULT '',
  `最后操作理由`       VARCHAR(1024) NOT NULL DEFAULT '',
  `最后操作IP`         VARCHAR(45) NOT NULL DEFAULT '',
  `更新时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`状态ID`)
)
COMMENT = '自动宣传全服限额周期状态'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

INSERT INTO `_宣传全局状态` (`状态ID`, `当前限额周期`)
VALUES (1, 1)
ON DUPLICATE KEY UPDATE `状态ID`=VALUES(`状态ID`);

CREATE TABLE IF NOT EXISTS `_宣传提交记录` (
  `提交ID`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '宣传提交唯一ID',
  `任务ID`             INT UNSIGNED NOT NULL COMMENT '对应 world._宣传审核任务.任务ID',
  `限额周期`           BIGINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '提交时的全服限额周期',
  `账号ID`             INT UNSIGNED NOT NULL COMMENT 'auth.account.id',
  `账号名`             VARCHAR(64) NOT NULL DEFAULT '' COMMENT '提交时的游戏账号名',
  `角色GUID`           BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '提交时使用的角色GUID，网页提交可为0',
  `角色名`             VARCHAR(64) NOT NULL DEFAULT '' COMMENT '提交时的角色名',
  `来源IP`             VARCHAR(45) NOT NULL DEFAULT '' COMMENT 'IPv4/IPv6来源地址',
  `宣传类型`           VARCHAR(16) NOT NULL DEFAULT 'IMAGE' COMMENT 'IMAGE=宣传图，URL=论坛链接，BOTH=两者都有',
  `图片存储引用`       VARCHAR(1024) NOT NULL DEFAULT '' COMMENT '图片文件或对象存储引用',
  `论坛链接`           VARCHAR(2048) NOT NULL DEFAULT '' COMMENT '公开可访问的宣传页面URL',
  `内容哈希`           CHAR(64) NOT NULL DEFAULT '' COMMENT '图片或正文规范化内容SHA-256',
  `预检状态`           VARCHAR(16) NOT NULL DEFAULT 'PENDING' COMMENT 'PENDING/PASS/FAIL',
  `预检结果`           VARCHAR(2048) NOT NULL DEFAULT '' COMMENT '自动检查结果和检测说明',
  `审核状态`           VARCHAR(16) NOT NULL DEFAULT 'PENDING' COMMENT 'PENDING/APPROVED/REJECTED/REVOKED',
  `审核人账号ID`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '管理员账号ID，自动预检为0',
  `审核人名称`         VARCHAR(64) NOT NULL DEFAULT '' COMMENT '管理员账号或显示名',
  `审核理由`           VARCHAR(1024) NOT NULL DEFAULT '' COMMENT '审核通过或驳回理由',
  `审核处理状态`       VARCHAR(16) NOT NULL DEFAULT 'PENDING' COMMENT 'PENDING/PROCESSING/APPLIED',
  `审核处理令牌`       VARCHAR(160) NOT NULL DEFAULT '' COMMENT 'worldserver审核副作用独占claim token',
  `审核处理时间`       DATETIME NULL,
  `提交时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `更新时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`提交ID`),
  KEY `idx_宣传提交记录_任务账号时间` (`任务ID`, `账号ID`, `提交时间`),
  KEY `idx_宣传提交记录_任务周期` (`任务ID`, `限额周期`),
  KEY `idx_宣传提交记录_IP时间` (`来源IP`, `提交时间`),
  KEY `idx_宣传提交记录_状态` (`审核状态`, `预检状态`),
  KEY `idx_宣传提交记录_处理状态` (`审核处理状态`, `更新时间`),
  KEY `idx_宣传提交记录_内容哈希` (`任务ID`, `账号ID`, `内容哈希`)
)
COMMENT = '自动宣传审核提交记录'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

SET @promotion_review_state_column_exists := (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传提交记录' AND COLUMN_NAME = '审核处理状态'
);
SET @promotion_review_state_column_sql := IF(
  @promotion_review_state_column_exists = 0,
  'ALTER TABLE `_宣传提交记录` ADD COLUMN `审核处理状态` VARCHAR(16) NOT NULL DEFAULT ''PENDING'' COMMENT ''PENDING/PROCESSING/APPLIED'' AFTER `审核理由`',
  'SELECT 1'
);
PREPARE promotion_review_state_column_stmt FROM @promotion_review_state_column_sql;
EXECUTE promotion_review_state_column_stmt;
DEALLOCATE PREPARE promotion_review_state_column_stmt;

SET @promotion_review_token_column_exists := (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传提交记录' AND COLUMN_NAME = '审核处理令牌'
);
SET @promotion_review_token_column_sql := IF(
  @promotion_review_token_column_exists = 0,
  'ALTER TABLE `_宣传提交记录` ADD COLUMN `审核处理令牌` VARCHAR(160) NOT NULL DEFAULT '''' COMMENT ''worldserver审核副作用独占claim token'' AFTER `审核处理状态`',
  'SELECT 1'
);
PREPARE promotion_review_token_column_stmt FROM @promotion_review_token_column_sql;
EXECUTE promotion_review_token_column_stmt;
DEALLOCATE PREPARE promotion_review_token_column_stmt;

SET @promotion_review_time_column_exists := (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传提交记录' AND COLUMN_NAME = '审核处理时间'
);
SET @promotion_review_time_column_sql := IF(
  @promotion_review_time_column_exists = 0,
  'ALTER TABLE `_宣传提交记录` ADD COLUMN `审核处理时间` DATETIME NULL AFTER `审核处理令牌`',
  'SELECT 1'
);
PREPARE promotion_review_time_column_stmt FROM @promotion_review_time_column_sql;
EXECUTE promotion_review_time_column_stmt;
DEALLOCATE PREPARE promotion_review_time_column_stmt;

SET @promotion_limit_cycle_column_exists := (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传提交记录' AND COLUMN_NAME = '限额周期'
);
SET @promotion_limit_cycle_column_sql := IF(
  @promotion_limit_cycle_column_exists = 0,
  'ALTER TABLE `_宣传提交记录` ADD COLUMN `限额周期` BIGINT UNSIGNED NOT NULL DEFAULT 1 COMMENT ''提交时的全服限额周期'' AFTER `任务ID`',
  'SELECT 1'
);
PREPARE promotion_limit_cycle_column_stmt FROM @promotion_limit_cycle_column_sql;
EXECUTE promotion_limit_cycle_column_stmt;
DEALLOCATE PREPARE promotion_limit_cycle_column_stmt;

SET @promotion_limit_cycle_index_exists := (
  SELECT COUNT(*) FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_宣传提交记录' AND INDEX_NAME = 'idx_宣传提交记录_任务周期'
);
SET @promotion_limit_cycle_index_sql := IF(
  @promotion_limit_cycle_index_exists = 0,
  'ALTER TABLE `_宣传提交记录` ADD KEY `idx_宣传提交记录_任务周期` (`任务ID`, `限额周期`)',
  'SELECT 1'
);
PREPARE promotion_limit_cycle_index_stmt FROM @promotion_limit_cycle_index_sql;
EXECUTE promotion_limit_cycle_index_stmt;
DEALLOCATE PREPARE promotion_limit_cycle_index_stmt;

CREATE TABLE IF NOT EXISTS `_宣传提交素材` (
  `素材ID`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '宣传素材唯一ID',
  `提交ID`             BIGINT UNSIGNED NOT NULL COMMENT '所属宣传提交ID',
  `素材类型`           VARCHAR(16) NOT NULL COMMENT 'IMAGE=群聊截图，URL=论坛链接',
  `素材序号`           INT UNSIGNED NOT NULL COMMENT '同一提交内从1开始的展示顺序',
  `素材引用`           VARCHAR(2048) NOT NULL DEFAULT '' COMMENT '图片存储引用或规范化公开URL',
  `内容哈希`           CHAR(64) NOT NULL DEFAULT '' COMMENT '单个素材SHA-256',
  `预检状态`           VARCHAR(16) NOT NULL DEFAULT 'PENDING' COMMENT 'PENDING/PASS/FAIL',
  `预检结果`           VARCHAR(2048) NOT NULL DEFAULT '' COMMENT '单个素材检查结果',
  `创建时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`素材ID`),
  UNIQUE KEY `uk_宣传提交素材_提交序号` (`提交ID`, `素材序号`),
  KEY `idx_宣传提交素材_提交类型` (`提交ID`, `素材类型`),
  KEY `idx_宣传提交素材_内容哈希` (`内容哈希`)
)
COMMENT = '自动宣传审核提交素材明细'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

CREATE TABLE IF NOT EXISTS `_宣传奖励流水` (
  `流水ID`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '奖励流水唯一ID',
  `提交ID`             BIGINT UNSIGNED NOT NULL COMMENT '对应宣传提交记录',
  `任务ID`             INT UNSIGNED NOT NULL,
  `账号ID`             INT UNSIGNED NOT NULL,
  `角色GUID`           BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `request_id`         CHAR(36) NOT NULL COMMENT 'Web/API发奖请求唯一ID',
  `发放模式`           VARCHAR(16) NOT NULL DEFAULT 'CDK' COMMENT 'CDK 或 ITEM',
  `CDK`                VARCHAR(128) NOT NULL DEFAULT '' COMMENT '发放的兑换码，ITEM模式为空',
  `奖励ID`             INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '奖励模板ID',
  `物品entry`          INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ITEM模式物品entry',
  `物品数量`           INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ITEM模式物品数量',
  `物品GUID`           BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ITEM模式主物品GUID',
  `新增奖励GUID`       LONGTEXT NULL COMMENT '本次发放新增资源的GUID列表或JSON快照',
  `资源快照`           LONGTEXT NULL COMMENT '发放前后可回滚资源快照',
  `发放状态`           VARCHAR(16) NOT NULL DEFAULT 'PENDING' COMMENT 'PENDING/ISSUED/FAILED/REVOKED',
  `发放时间`           DATETIME NULL,
  `回滚状态`           VARCHAR(16) NOT NULL DEFAULT 'NONE' COMMENT 'NONE/PENDING/SUCCESS/FAILED/DEBT',
  `回滚时间`           DATETIME NULL,
  `回滚错误`           VARCHAR(2048) NOT NULL DEFAULT '' COMMENT '追回失败或欠账原因',
  `处理令牌`           VARCHAR(160) NOT NULL DEFAULT '' COMMENT '发奖PROCESSING阶段的独占claim token',
  `创建时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `更新时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`流水ID`),
  UNIQUE KEY `uk_宣传奖励流水_提交ID` (`提交ID`),
  UNIQUE KEY `uk_宣传奖励流水_request_id` (`request_id`),
  KEY `idx_宣传奖励流水_CDK` (`CDK`),
  KEY `idx_宣传奖励流水_账号状态` (`账号ID`, `发放状态`, `回滚状态`),
  KEY `idx_宣传奖励流水_任务时间` (`任务ID`, `创建时间`)
)
COMMENT = '自动宣传奖励发放与回滚流水'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

SET @promotion_claim_column_exists := (
  SELECT COUNT(*)
  FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = '_宣传奖励流水'
    AND COLUMN_NAME = '处理令牌'
);
SET @promotion_claim_column_sql := IF(
  @promotion_claim_column_exists = 0,
  'ALTER TABLE `_宣传奖励流水` ADD COLUMN `处理令牌` VARCHAR(160) NOT NULL DEFAULT '''' COMMENT ''发奖PROCESSING阶段的独占claim token'' AFTER `回滚错误`',
  'SELECT 1'
);
PREPARE promotion_claim_column_stmt FROM @promotion_claim_column_sql;
EXECUTE promotion_claim_column_stmt;
DEALLOCATE PREPARE promotion_claim_column_stmt;

CREATE TABLE IF NOT EXISTS `_宣传兑换流水` (
  `兑换流水ID`         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '实际兑换流水唯一ID',
  `奖励流水ID`         BIGINT UNSIGNED NOT NULL COMMENT '对应宣传奖励流水',
  `提交ID`             BIGINT UNSIGNED NOT NULL,
  `任务ID`             INT UNSIGNED NOT NULL,
  `账号ID`             INT UNSIGNED NOT NULL COMMENT '从兑换记录解析出的实际账号',
  `request_id`         CHAR(36) NOT NULL,
  `CDK`                VARCHAR(128) NOT NULL DEFAULT '' COMMENT '实际兑换的CDK',
  `兑换角色GUID`       BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '实际使用CDK的角色GUID',
  `兑换角色名`         VARCHAR(64) NOT NULL DEFAULT '',
  `兑换账号名`         VARCHAR(64) NOT NULL DEFAULT '',
  `兑换IP`             VARCHAR(45) NOT NULL DEFAULT '',
  `兑换时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `兑换前宣传天数`     INT UNSIGNED NOT NULL DEFAULT 0,
  `兑换后宣传天数`     INT UNSIGNED NOT NULL DEFAULT 0,
  `旧宣传物品GUID`     BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '兑换前被回收的宣传物品GUID',
  `新宣传物品GUID`     BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '兑换后发放的宣传物品GUID',
  `新增奖励GUID`       LONGTEXT NULL COMMENT '兑换流程新增的其他奖励GUID列表或JSON',
  `资源快照`           LONGTEXT NULL COMMENT '兑换前后资源快照，用于审核无效时回滚',
  `回滚状态`           VARCHAR(16) NOT NULL DEFAULT 'NONE' COMMENT 'NONE/PENDING/SUCCESS/FAILED/DEBT',
  `回滚时间`           DATETIME NULL,
  `回滚错误`           VARCHAR(2048) NOT NULL DEFAULT '',
  `创建时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`兑换流水ID`),
  UNIQUE KEY `uk_宣传兑换流水_奖励流水ID` (`奖励流水ID`),
  KEY `idx_宣传兑换流水_request_id` (`request_id`),
  KEY `idx_宣传兑换流水_CDK` (`CDK`),
  KEY `idx_宣传兑换流水_角色时间` (`兑换角色GUID`, `兑换时间`),
  KEY `idx_宣传兑换流水_账号时间` (`账号ID`, `兑换时间`),
  KEY `idx_宣传兑换流水_回滚状态` (`回滚状态`)
)
COMMENT = '自动宣传CDK实际兑换与回滚流水'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

CREATE TABLE IF NOT EXISTS `_宣传账号统计` (
  `账号ID`             INT UNSIGNED NOT NULL,
  `统计日期`           DATE NOT NULL COMMENT '按服务器日期统计',
  `账号名`             VARCHAR(64) NOT NULL DEFAULT '',
  `今日提交次数`       INT UNSIGNED NOT NULL DEFAULT 0,
  `今日发放次数`       INT UNSIGNED NOT NULL DEFAULT 0,
  `今日无效次数`       INT UNSIGNED NOT NULL DEFAULT 0,
  `连续无效次数`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '审核通过后清零',
  `封禁状态`           TINYINT NOT NULL DEFAULT 0 COMMENT '0=未封禁，1=已封禁，2=等待封禁',
  `封禁时间`           DATETIME NULL,
  `封禁原因`           VARCHAR(1024) NOT NULL DEFAULT '',
  `最后审核时间`       DATETIME NULL,
  `最后IP`             VARCHAR(45) NOT NULL DEFAULT '',
  `更新时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`账号ID`, `统计日期`),
  KEY `idx_宣传账号统计_账号日期` (`账号ID`, `统计日期`),
  KEY `idx_宣传账号统计_状态` (`封禁状态`, `连续无效次数`)
)
COMMENT = '自动宣传账号每日次数与连续无效统计'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

CREATE TABLE IF NOT EXISTS `_宣传IP统计` (
  `IP地址`             VARCHAR(45) NOT NULL,
  `统计日期`           DATE NOT NULL COMMENT '按服务器日期统计',
  `今日提交次数`       INT UNSIGNED NOT NULL DEFAULT 0,
  `今日发放次数`       INT UNSIGNED NOT NULL DEFAULT 0,
  `今日无效次数`       INT UNSIGNED NOT NULL DEFAULT 0,
  `最后账号ID`         INT UNSIGNED NOT NULL DEFAULT 0,
  `更新时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`IP地址`, `统计日期`),
  KEY `idx_宣传IP统计_IP日期` (`IP地址`, `统计日期`),
  KEY `idx_宣传IP统计_日期` (`统计日期`)
)
COMMENT = '自动宣传IP每日次数统计'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

CREATE TABLE IF NOT EXISTS `_宣传审核日志` (
  `日志ID`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `提交ID`             BIGINT UNSIGNED NOT NULL,
  `奖励流水ID`         BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `操作类型`           VARCHAR(32) NOT NULL DEFAULT '' COMMENT 'SUBMIT/PRECHECK/APPROVE/REJECT/REVOKE/BAN',
  `原状态`             VARCHAR(16) NOT NULL DEFAULT '',
  `新状态`             VARCHAR(16) NOT NULL DEFAULT '',
  `审核人账号ID`       INT UNSIGNED NOT NULL DEFAULT 0,
  `审核人名称`         VARCHAR(64) NOT NULL DEFAULT '',
  `审核理由`           VARCHAR(1024) NOT NULL DEFAULT '',
  `回滚状态`           VARCHAR(16) NOT NULL DEFAULT 'NONE',
  `回滚错误`           VARCHAR(2048) NOT NULL DEFAULT '',
  `操作IP`             VARCHAR(45) NOT NULL DEFAULT '',
  `操作时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`日志ID`),
  KEY `idx_宣传审核日志_提交时间` (`提交ID`, `操作时间`),
  KEY `idx_宣传审核日志_审核人时间` (`审核人账号ID`, `操作时间`),
  KEY `idx_宣传审核日志_状态` (`原状态`, `新状态`)
)
COMMENT = '自动宣传审核操作审计日志'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

CREATE TABLE IF NOT EXISTS `_宣传管理日志` (
  `日志ID`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `关联提交ID`         BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '账号操作来源提交，全服操作为0',
  `目标账号ID`         INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '全服操作为0',
  `操作类型`           VARCHAR(32) NOT NULL COMMENT 'RESET_ALL_COUNTS/CANCEL_PENDING_BAN/UNBAN_ACCOUNT',
  `操作前状态`         LONGTEXT NOT NULL,
  `操作后状态`         LONGTEXT NOT NULL,
  `审核人账号ID`       INT UNSIGNED NOT NULL DEFAULT 0,
  `审核人名称`         VARCHAR(64) NOT NULL DEFAULT '',
  `操作理由`           VARCHAR(1024) NOT NULL DEFAULT '',
  `操作IP`             VARCHAR(45) NOT NULL DEFAULT '',
  `操作结果`           VARCHAR(32) NOT NULL DEFAULT 'SUCCESS',
  `操作时间`           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`日志ID`),
  KEY `idx_宣传管理日志_账号时间` (`目标账号ID`, `操作时间`),
  KEY `idx_宣传管理日志_审核人时间` (`审核人账号ID`, `操作时间`),
  KEY `idx_宣传管理日志_操作时间` (`操作类型`, `操作时间`)
)
COMMENT = '自动宣传后台全服与账号管理审计日志'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

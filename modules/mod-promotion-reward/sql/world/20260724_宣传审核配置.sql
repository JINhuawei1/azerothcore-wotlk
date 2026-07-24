-- ============================================================
-- 自动宣传审核 - 任务配置表 (world 数据库)
-- 幂等迁移：重复导入只确保默认任务存在，不覆盖运营配置。
-- ============================================================

CREATE TABLE IF NOT EXISTS `_宣传审核任务` (
  `任务ID`                 INT UNSIGNED NOT NULL COMMENT '宣传任务唯一ID',
  `任务名称`               VARCHAR(128) NOT NULL DEFAULT '' COMMENT '任务显示名称',
  `启用`                   TINYINT NOT NULL DEFAULT 1 COMMENT '是否启用，0=禁用，1=启用',
  `奖励模式`               VARCHAR(16) NOT NULL DEFAULT 'CDK' COMMENT '奖励模式：CDK 或 ITEM',
  `奖励组`                 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'mod-redemption-code 使用的兑换码分组',
  `需求ID`                 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '兑换前置需求模板ID，0=无需求',
  `奖励ID`                 INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'CDK 对接的奖励模板ID',
  `物品entry`              INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ITEM 模式发放的 item_template entry',
  `物品数量`               INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'ITEM 模式发放数量',
  `账号每日上限`           INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '同一账号每日最多获得次数',
  `IP每日上限`             INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '同一IP每日最多获得次数',
  `任务每日上限`           INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '同一任务每日最多发放次数',
  `关键词`                 VARCHAR(512) NOT NULL DEFAULT '' COMMENT '自动预检关键词，使用逗号分隔',
  `连续无效封号次数`       INT UNSIGNED NOT NULL DEFAULT 3 COMMENT '连续无效达到此次数后封禁账号',
  `创建时间`               DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `更新时间`               DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`任务ID`),
  KEY `idx_宣传审核任务_启用` (`启用`)
)
COMMENT = '自动宣传审核任务配置'
CHARACTER SET = utf8mb4
COLLATE = utf8mb4_unicode_ci
ENGINE = InnoDB
ROW_FORMAT = DEFAULT
;

INSERT INTO `_宣传审核任务`
  (`任务ID`, `任务名称`, `启用`, `奖励模式`, `奖励组`, `需求ID`, `奖励ID`,
   `物品entry`, `物品数量`, `账号每日上限`, `IP每日上限`, `任务每日上限`,
   `关键词`, `连续无效封号次数`)
VALUES
  (1, '默认宣传任务', 1, 'CDK', 9001, 0, 100, 0, 1, 1, 1, 1, '服务器,宣传', 3)
ON DUPLICATE KEY UPDATE
  `更新时间` = CURRENT_TIMESTAMP;

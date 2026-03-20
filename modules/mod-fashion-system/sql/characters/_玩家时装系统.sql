DROP TABLE IF EXISTS `_玩家时装系统_收集`;
CREATE TABLE `_玩家时装系统_收集` (
  `玩家GUID` int unsigned NOT NULL,
  `时装id` int unsigned NOT NULL,
  `解锁时间` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`玩家GUID`, `时装id`),
  KEY `idx_时装id` (`时装id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家已解锁的时装收集记录';

DROP TABLE IF EXISTS `_玩家时装系统_方案`;
CREATE TABLE `_玩家时装系统_方案` (
  `玩家GUID` int unsigned NOT NULL,
  `是否启用` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1=启用时装外观与套装效果 0=关闭',
  `手` int unsigned NOT NULL DEFAULT '0',
  `腰带` int unsigned NOT NULL DEFAULT '0',
  `裤子` int unsigned NOT NULL DEFAULT '0',
  `脚` int unsigned NOT NULL DEFAULT '0',
  `背部` int unsigned NOT NULL DEFAULT '0',
  `肩部` int unsigned NOT NULL DEFAULT '0',
  `头` int unsigned NOT NULL DEFAULT '0',
  `护腕` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家当前时装搭配方案';

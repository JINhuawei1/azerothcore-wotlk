-- ============================================================
-- 挑战幻境系统 - Characters 数据库
-- 文件名按需求保留：_挑战幻境玩家数据.sql
-- ============================================================

CREATE TABLE IF NOT EXISTS `_挑战幻境玩家数据` (
  `玩家GUID` int unsigned NOT NULL COMMENT 'characters.guid',
  `当前幻境等级` int unsigned NOT NULL DEFAULT 0 COMMENT '0=原世界，>0=当前挑战幻境等级',
  `当前生物层` int unsigned NOT NULL DEFAULT 0 COMMENT '当前生物隔离层。默认等于当前幻境等级',
  `最高解锁等级` int unsigned NOT NULL DEFAULT 0 COMMENT '可选进度字段',
  `累计进入次数` int unsigned NOT NULL DEFAULT 0,
  `累计击杀数` int unsigned NOT NULL DEFAULT 0,
  `最后进入时间` int unsigned NOT NULL DEFAULT 0 COMMENT 'Unix time',
  `更新时间` int unsigned NOT NULL DEFAULT 0 COMMENT 'Unix time',
  PRIMARY KEY (`玩家GUID`),
  KEY `idx_当前幻境等级` (`当前幻境等级`),
  KEY `idx_当前生物层` (`当前生物层`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='挑战幻境-玩家当前生物层数据';

CREATE TABLE IF NOT EXISTS `_挑战幻境运行记录` (
  `运行ID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `玩家GUID` int unsigned NOT NULL COMMENT 'characters.guid',
  `幻境等级` int unsigned NOT NULL DEFAULT 0,
  `生物层` int unsigned NOT NULL DEFAULT 0,
  `地图` smallint unsigned NOT NULL DEFAULT 0,
  `状态` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0=进行中,1=完成,2=失败,3=主动离开',
  `开始时间` int unsigned NOT NULL DEFAULT 0,
  `结束时间` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`运行ID`),
  KEY `idx_玩家_状态` (`玩家GUID`, `状态`),
  KEY `idx_等级_层` (`幻境等级`, `生物层`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='挑战幻境-运行记录';

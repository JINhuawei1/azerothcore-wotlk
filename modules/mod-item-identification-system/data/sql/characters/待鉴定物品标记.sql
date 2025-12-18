-- 待鉴定物品标记表
-- 用于记录从幻境生物掉落的物品，玩家需要手动鉴定
-- 性能优化：拾取时只写入轻量标记，手动鉴定时才执行完整流程

DROP TABLE IF EXISTS `待鉴定物品标记`;
CREATE TABLE `待鉴定物品标记` (
  `物品GUID` INT UNSIGNED NOT NULL COMMENT '物品GUID（主键）',
  `物品ID` INT UNSIGNED NOT NULL COMMENT '物品Entry ID',
  `玩家GUID` INT UNSIGNED NOT NULL COMMENT '玩家GUID',
  `幻境倍率` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '拾取时的幻境倍率',
  `鉴定组ID` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '预设的鉴定组ID',
  PRIMARY KEY (`物品GUID`),
  KEY `idx_玩家GUID` (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='待鉴定物品标记表 - 性能优化：拾取时只标记，手动鉴定';

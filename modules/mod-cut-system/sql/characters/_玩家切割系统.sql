DROP TABLE IF EXISTS `_玩家切割系统`;
CREATE TABLE `_玩家切割系统` (
  `玩家GUID` int unsigned NOT NULL,
  `切割等级` int unsigned NOT NULL DEFAULT '0' COMMENT '当前角色切割等级',
  PRIMARY KEY (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家切割系统数据';

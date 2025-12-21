-- 转身系统玩家数据表
-- 存储玩家的转身等级和累计奖励
DROP TABLE IF EXISTS `_转身系统_玩家数据`;
CREATE TABLE `_转身系统_玩家数据` (
  `角色id` int unsigned NOT NULL COMMENT '角色GUID',
  `转身等级` int unsigned NOT NULL DEFAULT 0 COMMENT '当前转身等级(转身次数)',
  `累计属性加成` float NOT NULL DEFAULT 0.0 COMMENT '累计全属性加成百分比',
  `累计天赋点` int unsigned NOT NULL DEFAULT 0 COMMENT '累计奖励天赋点数',
  PRIMARY KEY (`角色id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='转身系统-玩家数据存储表';

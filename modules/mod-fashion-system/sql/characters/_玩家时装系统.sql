DROP TABLE IF EXISTS `_玩家时装系统_穿戴`;
CREATE TABLE `_玩家时装系统_穿戴` (
  `玩家GUID` int unsigned NOT NULL,
  `是否启用` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1=启用时装外观 0=关闭',
  `槽位` varchar(127) NOT NULL DEFAULT '0 0,1 0,2 0,3 0,4 0,5 0,6 0,7 0,8 0,9 0,10 0,11 0,12 0,13 0,14 0,15 0,16 0,17 0,18 0' COMMENT '19个装备槽位激活状态，格式: 槽位 激活,槽位 激活...',
  `物品id` varchar(255) NOT NULL DEFAULT '0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0' COMMENT '19个槽位对应的覆盖显示物品模板ID，按槽位顺序存储',
  `套装id` varchar(255) NOT NULL DEFAULT '0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0' COMMENT '19个槽位最终解析出的实际套装ID，按槽位顺序存储；固定模式为配置套装ID，分组模式为随机结果',
  PRIMARY KEY (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家时装系统紧凑存储';

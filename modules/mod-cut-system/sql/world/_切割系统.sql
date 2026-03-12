DROP TABLE IF EXISTS `_切割系统`;
CREATE TABLE `_切割系统` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `切割等级` int unsigned NOT NULL DEFAULT '0' COMMENT '支持设置 1-99999，按等级门槛匹配',
  `需求系统id` int unsigned NOT NULL DEFAULT '0' COMMENT '关联需求模板系统ID，0表示无需求',
  `伤害类型` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1=固定值 2=百分比',
  `切割伤害` float NOT NULL DEFAULT '0' COMMENT '固定值直接生效；百分比按目标当前血量计算，最大100',
  `触发几率` float NOT NULL DEFAULT '100' COMMENT '1-100%',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_切割等级` (`切割等级`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='切割系统配置表';

INSERT INTO `_切割系统` (`id`, `切割等级`, `需求系统id`, `伤害类型`, `切割伤害`, `触发几率`) VALUES
(1, 1, 0, 1, 1000, 100),
(2, 10, 0, 2, 10, 25);

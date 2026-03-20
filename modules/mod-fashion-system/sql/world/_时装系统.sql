DROP TABLE IF EXISTS `_时装系统`;
CREATE TABLE `_时装系统` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `物品id` int unsigned NOT NULL DEFAULT '0' COMMENT '客户端显示的物品模板ID',
  `需求系统id` int unsigned NOT NULL DEFAULT '0' COMMENT '关联 mod-requirement-template 的需求模板ID，0表示无要求',
  `时装穿戴位置` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '1=手 2=腰带 3=裤子 4=脚 5=背部 6=肩部 7=头 8=护腕',
  `组` int unsigned NOT NULL DEFAULT '1' COMMENT '时装分组ID，同一组表示同一整套时装',
  `套装id` int unsigned NOT NULL DEFAULT '0' COMMENT '关联 mod-item-sets 的套装ID，0表示不参与时装套装效果',
  PRIMARY KEY (`id`),
  KEY `idx_物品id` (`物品id`),
  KEY `idx_组` (`组`),
  KEY `idx_时装穿戴位置` (`时装穿戴位置`),
  KEY `idx_套装id` (`套装id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='时装系统配置表';

INSERT INTO `_时装系统` (`id`, `物品id`, `需求系统id`, `时装穿戴位置`, `组`, `套装id`) VALUES
(1, 16864, 0, 6, 1, 1),
(2, 16861, 0, 7, 1, 1),
(3, 16866, 0, 5, 1, 1),
(4, 16867, 0, 2, 1, 1),
(5, 16868, 0, 3, 1, 1),
(6, 16865, 0, 4, 1, 1),
(7, 16863, 0, 1, 1, 1),
(8, 16819, 0, 8, 1, 1);

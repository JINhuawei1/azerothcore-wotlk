DROP TABLE IF EXISTS `_时装系统`;
CREATE TABLE `_时装系统` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `需求系统id` int unsigned NOT NULL DEFAULT '0' COMMENT '关联需求系统模板ID，0=无需求限制',
  `套装分配模式` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=无套装效果 1=按套装ID固定分配 2=按套装组随机分配',
  `套装id` int unsigned NOT NULL DEFAULT '0' COMMENT '当套装分配模式=1时使用该套装ID',
  `套装组` int unsigned NOT NULL DEFAULT '0' COMMENT '当套装分配模式=2时，从该套装组中随机分配套装ID',
  `时装位置` tinyint unsigned NOT NULL COMMENT '装备槽位 0-18，对应游戏 EQUIPMENT_SLOT',
  `时装描述` varchar(255) NOT NULL DEFAULT '' COMMENT '该时装位置的描述文本',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_时装位置` (`时装位置`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='时装系统世界配置';

-- 默认配置：19个槽位
INSERT INTO `_时装系统` (`需求系统id`, `套装分配模式`, `套装id`, `套装组`, `时装位置`, `时装描述`) VALUES
(0, 0, 0, 0, 0,  '头部'),
(0, 0, 0, 0, 1,  '项链'),
(0, 0, 0, 0, 2,  '肩部'),
(0, 0, 0, 0, 3,  '衬衣'),
(0, 0, 0, 0, 4,  '胸甲'),
(0, 0, 0, 0, 5,  '腰带'),
(0, 0, 0, 0, 6,  '腿部'),
(0, 0, 0, 0, 7,  '脚部'),
(0, 0, 0, 0, 8,  '护腕'),
(0, 0, 0, 0, 9,  '手套'),
(0, 0, 0, 0, 10, '戒指1'),
(0, 0, 0, 0, 11, '戒指2'),
(0, 0, 0, 0, 12, '饰品1'),
(0, 0, 0, 0, 13, '饰品2'),
(0, 0, 0, 0, 14, '背部'),
(0, 0, 0, 0, 15, '主手'),
(0, 0, 0, 0, 16, '副手'),
(0, 0, 0, 0, 17, '远程'),
(0, 0, 0, 0, 18, '战袍');

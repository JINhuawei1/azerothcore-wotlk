-- 武魂系统 - 装备槽配置

CREATE TABLE IF NOT EXISTS `_武魂系统_装备` (
  `槽位` tinyint UNSIGNED NOT NULL COMMENT '对应玩家装备槽位',
  `名称` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `魂力消耗` bigint UNSIGNED NOT NULL DEFAULT 0,
  `需求模板ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=无额外需求',
  `备注` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`槽位`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='武魂系统装备槽解锁配置';

DELETE FROM `_武魂系统_装备` WHERE `槽位` BETWEEN 0 AND 18;
INSERT INTO `_武魂系统_装备`
(`槽位`, `名称`, `魂力消耗`, `需求模板ID`, `备注`) VALUES
(0, '头部', 1200, 0, ''),
(1, '项链', 1500, 0, ''),
(2, '肩部', 1200, 0, ''),
(3, '衬衣', 500, 0, ''),
(4, '胸甲', 1800, 0, ''),
(5, '腰带', 900, 0, ''),
(6, '腿部', 1500, 0, ''),
(7, '脚部', 900, 0, ''),
(8, '护腕', 800, 0, ''),
(9, '手套', 900, 0, ''),
(10, '戒指1', 2000, 0, ''),
(11, '戒指2', 2500, 0, ''),
(12, '饰品1', 3000, 0, ''),
(13, '饰品2', 4000, 0, ''),
(14, '披风', 1600, 0, ''),
(15, '主手', 5000, 0, ''),
(16, '副手', 3500, 0, ''),
(17, '远程', 3500, 0, ''),
(18, '战袍', 1000, 0, '');

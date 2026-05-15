-- 创建物品购买记录表（整合了所有购买记录和统计功能）
DROP TABLE IF EXISTS `_物品_购买记录`;
CREATE TABLE `_物品_购买记录` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `物品id` int(10) unsigned NOT NULL DEFAULT '0',
  `账号id` int(10) unsigned NOT NULL DEFAULT '0',
  `角色id` int(10) unsigned NOT NULL DEFAULT '0',
  `购买时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `购买数量` int(10) unsigned NOT NULL DEFAULT '1',
  `日期` date GENERATED ALWAYS AS (DATE(购买时间)) STORED,
  PRIMARY KEY (`id`),
  KEY `物品id` (`物品id`),
  KEY `账号id` (`账号id`),
  KEY `角色id` (`角色id`),
  KEY `日期` (`日期`),
  KEY `物品账号日期` (`物品id`, `账号id`, `日期`),
  KEY `物品角色日期` (`物品id`, `角色id`, `日期`),
  KEY `物品日期` (`物品id`, `日期`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='物品购买记录';

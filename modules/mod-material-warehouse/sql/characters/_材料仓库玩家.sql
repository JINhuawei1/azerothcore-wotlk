DROP TABLE IF EXISTS `_材料仓库玩家`;

CREATE TABLE `_材料仓库玩家` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `物品ID` int UNSIGNED NOT NULL COMMENT 'item_template.entry',
  `数量` bigint UNSIGNED NOT NULL DEFAULT 0 COMMENT '仓库内累计数量',
  `自动存储` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=拾取/获得后自动转入仓库',
  PRIMARY KEY (`玩家GUID`, `物品ID`) USING BTREE,
  KEY `idx_物品ID` (`物品ID`) USING BTREE,
  KEY `idx_自动存储` (`玩家GUID`, `自动存储`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家材料仓库存储数据';

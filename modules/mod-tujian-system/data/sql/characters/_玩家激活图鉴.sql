DROP TABLE IF EXISTS `_玩家激活图鉴`;
CREATE TABLE `_玩家激活图鉴`  (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.guid',
  `图鉴ID` int UNSIGNED NOT NULL COMMENT '对应 world 表 _图鉴系统.id',
  `套装ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '对应 world 表 _图鉴系统_套装.id',
  `当前等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家当前已激活的图鉴等级',
  `当前物品entry` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '对应当前等级所使用的物品entry',
  PRIMARY KEY (`玩家GUID`, `图鉴ID`) USING BTREE,
  KEY `idx_玩家GUID` (`玩家GUID`) USING BTREE,
  KEY `idx_套装ID` (`套装ID`) USING BTREE,
  KEY `idx_当前物品entry` (`当前物品entry`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家图鉴激活记录';

SET FOREIGN_KEY_CHECKS = 1;

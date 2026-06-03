DROP TABLE IF EXISTS `_玩家称号系统`;
CREATE TABLE `_玩家称号系统`  (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `称号ID` int UNSIGNED NOT NULL COMMENT 'world._称号系统.id',
  `称号等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效的称号等级快照',
  `属性值1` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效称号属性值1快照：生命加成',
  `属性值2` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效称号属性值2快照：减伤百分比',
  PRIMARY KEY (`玩家GUID`) USING BTREE,
  KEY `idx_称号ID` (`称号ID`) USING BTREE,
  KEY `idx_称号等级` (`称号等级`) USING BTREE,
  KEY `idx_属性值1` (`属性值1`) USING BTREE,
  KEY `idx_属性值2` (`属性值2`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家称号系统当前生效记录';

SET FOREIGN_KEY_CHECKS = 1;

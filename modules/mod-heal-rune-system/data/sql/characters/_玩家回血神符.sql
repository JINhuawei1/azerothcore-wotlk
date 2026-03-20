DROP TABLE IF EXISTS `_玩家回血神符`;
CREATE TABLE `_玩家回血神符` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `神符ID` int UNSIGNED NOT NULL COMMENT 'world._回血神符.id',
  `回血等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '激活时的回血等级快照',
  PRIMARY KEY (`玩家GUID`, `神符ID`) USING BTREE,
  KEY `idx_玩家GUID` (`玩家GUID`) USING BTREE,
  KEY `idx_回血等级` (`回血等级`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家回血神符激活记录';

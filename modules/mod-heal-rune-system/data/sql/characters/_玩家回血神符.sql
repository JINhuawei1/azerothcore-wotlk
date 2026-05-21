DROP TABLE IF EXISTS `_玩家回血神符_new`;

CREATE TABLE `_玩家回血神符_new` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `神符ID` int UNSIGNED NOT NULL COMMENT '当前生效的 world._回血神符.id',
  `回血等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效的回血等级快照',
  PRIMARY KEY (`玩家GUID`) USING BTREE,
  KEY `idx_神符ID` (`神符ID`) USING BTREE,
  KEY `idx_回血等级` (`回血等级`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家回血神符激活记录';

SET @heal_rune_table_exists := (
  SELECT COUNT(*)
  FROM `information_schema`.`TABLES`
  WHERE `TABLE_SCHEMA` = DATABASE()
    AND `TABLE_NAME` = '_玩家回血神符'
);

SET @heal_rune_migrate_sql := IF(
  @heal_rune_table_exists > 0,
  'INSERT INTO `_玩家回血神符_new` (`玩家GUID`, `神符ID`, `回血等级`)
   SELECT
     p.`玩家GUID`,
     MAX(p.`神符ID`) AS `神符ID`,
     p.`回血等级`
   FROM `_玩家回血神符` p
   INNER JOIN (
     SELECT `玩家GUID`, MAX(`回血等级`) AS `最高回血等级`
     FROM `_玩家回血神符`
     GROUP BY `玩家GUID`
   ) m
     ON m.`玩家GUID` = p.`玩家GUID`
    AND m.`最高回血等级` = p.`回血等级`
   GROUP BY p.`玩家GUID`, p.`回血等级`',
  'SELECT 1'
);

PREPARE heal_rune_migrate_stmt FROM @heal_rune_migrate_sql;
EXECUTE heal_rune_migrate_stmt;
DEALLOCATE PREPARE heal_rune_migrate_stmt;

DROP TABLE IF EXISTS `_玩家回血神符_old`;

SET @heal_rune_swap_sql := IF(
  @heal_rune_table_exists > 0,
  'RENAME TABLE `_玩家回血神符` TO `_玩家回血神符_old`, `_玩家回血神符_new` TO `_玩家回血神符`',
  'RENAME TABLE `_玩家回血神符_new` TO `_玩家回血神符`'
);

PREPARE heal_rune_swap_stmt FROM @heal_rune_swap_sql;
EXECUTE heal_rune_swap_stmt;
DEALLOCATE PREPARE heal_rune_swap_stmt;

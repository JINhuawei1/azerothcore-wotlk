-- 在 characters 数据库执行。
-- 目标：把 `_玩家回血神符` 从“每级一行”迁移为“每个玩家只保留当前最高回血神符一行”。
-- 安全策略：
-- 1. `_玩家回血神符_迁移备份_20260524` 保存迁移前完整数据；
-- 2. `_玩家回血神符_迁移旧表_20260524` 保存本次替换前的原表；
-- 3. 如果存在旧迁移表 `_玩家回血神符_old`，会一起纳入迁移源，用于恢复之前迁移保留的数据；
-- 4. 正式表 `_玩家回血神符` 只保留每个玩家最高 `回血等级` 的记录，同等级时保留 `神符ID` 较大的一条。

SET FOREIGN_KEY_CHECKS = 0;

CREATE TABLE IF NOT EXISTS `_玩家回血神符_迁移备份_20260524` LIKE `_玩家回血神符`;
INSERT IGNORE INTO `_玩家回血神符_迁移备份_20260524`
SELECT *
FROM `_玩家回血神符`;

DROP TABLE IF EXISTS `_玩家回血神符_迁移源_20260524`;

CREATE TABLE `_玩家回血神符_迁移源_20260524` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `神符ID` int UNSIGNED NOT NULL COMMENT '当前生效的 world._回血神符.id',
  `回血等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效的回血等级快照',
  KEY `idx_玩家GUID` (`玩家GUID`) USING BTREE,
  KEY `idx_神符ID` (`神符ID`) USING BTREE,
  KEY `idx_回血等级` (`回血等级`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家回血神符迁移源数据';

INSERT INTO `_玩家回血神符_迁移源_20260524` (`玩家GUID`, `神符ID`, `回血等级`)
SELECT `玩家GUID`, `神符ID`, `回血等级`
FROM `_玩家回血神符`;

SET @heal_rune_old_table_exists := (
  SELECT COUNT(*)
  FROM `information_schema`.`TABLES`
  WHERE `TABLE_SCHEMA` = DATABASE()
    AND `TABLE_NAME` = '_玩家回血神符_old'
);

SET @heal_rune_old_insert_sql := IF(
  @heal_rune_old_table_exists > 0,
  'INSERT INTO `_玩家回血神符_迁移源_20260524` (`玩家GUID`, `神符ID`, `回血等级`)
   SELECT `玩家GUID`, `神符ID`, `回血等级`
   FROM `_玩家回血神符_old`',
  'SELECT 1'
);

PREPARE heal_rune_old_insert_stmt FROM @heal_rune_old_insert_sql;
EXECUTE heal_rune_old_insert_stmt;
DEALLOCATE PREPARE heal_rune_old_insert_stmt;

DROP TABLE IF EXISTS `_玩家回血神符_new`;

CREATE TABLE `_玩家回血神符_new` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `神符ID` int UNSIGNED NOT NULL COMMENT '当前生效的 world._回血神符.id',
  `回血等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效的回血等级快照',
  PRIMARY KEY (`玩家GUID`) USING BTREE,
  KEY `idx_神符ID` (`神符ID`) USING BTREE,
  KEY `idx_回血等级` (`回血等级`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家回血神符激活记录';

INSERT INTO `_玩家回血神符_new` (`玩家GUID`, `神符ID`, `回血等级`)
SELECT
  p.`玩家GUID`,
  MAX(p.`神符ID`) AS `神符ID`,
  p.`回血等级`
FROM `_玩家回血神符_迁移源_20260524` p
INNER JOIN (
  SELECT `玩家GUID`, MAX(`回血等级`) AS `最高回血等级`
  FROM `_玩家回血神符_迁移源_20260524`
  WHERE `玩家GUID` > 0
    AND `神符ID` > 0
    AND `回血等级` > 0
  GROUP BY `玩家GUID`
) m
  ON m.`玩家GUID` = p.`玩家GUID`
 AND m.`最高回血等级` = p.`回血等级`
WHERE p.`玩家GUID` > 0
  AND p.`神符ID` > 0
  AND p.`回血等级` > 0
GROUP BY p.`玩家GUID`, p.`回血等级`;

DROP TABLE IF EXISTS `_玩家回血神符_迁移旧表_20260524`;
RENAME TABLE `_玩家回血神符` TO `_玩家回血神符_迁移旧表_20260524`,
             `_玩家回血神符_new` TO `_玩家回血神符`;

SET FOREIGN_KEY_CHECKS = 1;

SELECT COUNT(*) AS `迁移后正式表玩家数`
FROM `_玩家回血神符`;

SELECT COUNT(*) AS `迁移前完整备份行数`
FROM `_玩家回血神符_迁移备份_20260524`;

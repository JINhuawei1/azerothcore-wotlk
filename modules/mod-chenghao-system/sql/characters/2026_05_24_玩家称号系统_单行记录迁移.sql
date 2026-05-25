-- 在 characters 数据库执行。
-- 目标：把 `_玩家称号系统` 从“每个称号一行”迁移为“每个玩家只保留当前最高称号一行”。
-- 安全策略：
-- 1. `_玩家称号系统_迁移备份_20260524` 保存迁移前完整数据；
-- 2. `_玩家称号系统_迁移旧表_20260524` 保存本次替换前的原表；
-- 3. 正式表 `_玩家称号系统` 只保留每个玩家最高 `称号等级` 的记录，同等级时保留 `称号ID` 较小的一条。

SET FOREIGN_KEY_CHECKS = 0;

CREATE TABLE IF NOT EXISTS `_玩家称号系统_迁移备份_20260524` LIKE `_玩家称号系统`;
INSERT IGNORE INTO `_玩家称号系统_迁移备份_20260524`
SELECT *
FROM `_玩家称号系统`;

DROP TABLE IF EXISTS `_玩家称号系统_new`;

CREATE TABLE `_玩家称号系统_new` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.characters.guid',
  `称号ID` int UNSIGNED NOT NULL COMMENT 'world._称号系统.id',
  `称号等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前生效的称号等级快照',
  `光环技能id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '登录时直接加载到玩家的光环技能',
  PRIMARY KEY (`玩家GUID`) USING BTREE,
  KEY `idx_称号ID` (`称号ID`) USING BTREE,
  KEY `idx_称号等级` (`称号等级`) USING BTREE,
  KEY `idx_光环技能id` (`光环技能id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家称号系统当前生效记录';

INSERT INTO `_玩家称号系统_new` (`玩家GUID`, `称号ID`, `称号等级`, `光环技能id`)
SELECT
  p.`玩家GUID`,
  p.`称号ID`,
  p.`称号等级`,
  p.`光环技能id`
FROM `_玩家称号系统` p
WHERE p.`玩家GUID` > 0
  AND p.`称号ID` > 0
  AND p.`称号等级` > 0
  AND p.`光环技能id` > 0
  AND NOT EXISTS (
    SELECT 1
    FROM `_玩家称号系统` b
    WHERE b.`玩家GUID` = p.`玩家GUID`
      AND b.`称号ID` > 0
      AND b.`称号等级` > 0
      AND b.`光环技能id` > 0
      AND (
        b.`称号等级` > p.`称号等级`
        OR (b.`称号等级` = p.`称号等级` AND b.`称号ID` < p.`称号ID`)
      )
  );

DROP TABLE IF EXISTS `_玩家称号系统_迁移旧表_20260524`;
RENAME TABLE `_玩家称号系统` TO `_玩家称号系统_迁移旧表_20260524`,
             `_玩家称号系统_new` TO `_玩家称号系统`;

SET FOREIGN_KEY_CHECKS = 1;

SELECT COUNT(*) AS `迁移后正式表玩家数`
FROM `_玩家称号系统`;

SELECT COUNT(*) AS `迁移前完整备份行数`
FROM `_玩家称号系统_迁移备份_20260524`;

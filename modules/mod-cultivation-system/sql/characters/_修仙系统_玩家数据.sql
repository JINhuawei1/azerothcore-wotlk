-- ============================================
-- 修仙系统 - 玩家数据表
-- ============================================

DROP TABLE IF EXISTS `_玩家修仙数据`;
CREATE TABLE `_玩家修仙数据` (
  `角色id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家GUID',
  `修仙等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前修仙等级(0-100)',
  `渡劫冷却时间` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '渡劫冷却截止时间戳',
  PRIMARY KEY (`角色id`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家修仙数据' ROW_FORMAT = DYNAMIC;

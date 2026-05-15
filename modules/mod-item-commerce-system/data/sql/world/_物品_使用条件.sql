DROP TABLE IF EXISTS `_物品_使用条件`;
CREATE TABLE `_物品_使用条件`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `entry` mediumint UNSIGNED NOT NULL DEFAULT 0,
  `物品使用需求` int NOT NULL DEFAULT 0 COMMENT '物品使用条件（对应需求_系统id）',
  PRIMARY KEY (`entry`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = FIXED; 
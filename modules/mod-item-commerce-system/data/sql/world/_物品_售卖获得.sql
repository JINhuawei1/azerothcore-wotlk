DROP TABLE IF EXISTS `_物品_售卖获得`;
CREATE TABLE `_物品_售卖获得`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `entry` mediumint UNSIGNED NOT NULL DEFAULT 0,
  `物品出售奖励` int NOT NULL DEFAULT 0 COMMENT '物品出售奖励',
  PRIMARY KEY (`entry`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = FIXED; 
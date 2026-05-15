DROP TABLE IF EXISTS `_物品_租赁`;
CREATE TABLE `_物品_租赁`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `物品id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品id',
  `租赁时间_单位秒` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '超过租赁时间，物品永久消失',
  PRIMARY KEY (`物品id`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = FIXED; 
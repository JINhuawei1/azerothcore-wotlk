DROP TABLE IF EXISTS `_物品_购买条件`;
CREATE TABLE `_物品_购买条件`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `生物id` int UNSIGNED NOT NULL DEFAULT 0,
  `物品id` int NOT NULL DEFAULT 0,
  `物品购买条件` int NOT NULL DEFAULT 0 COMMENT '调用需求_模板id',
  PRIMARY KEY (`物品id`, `物品购买条件`) USING BTREE  
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item Purchase Condition System' ROW_FORMAT = FIXED;

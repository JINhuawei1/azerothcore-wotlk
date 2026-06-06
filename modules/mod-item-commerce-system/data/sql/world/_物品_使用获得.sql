-- 创建物品使用奖励表
DROP TABLE IF EXISTS `_物品_使用获得`;
CREATE TABLE `_物品_使用获得`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL COMMENT '物品描述',
  `entry` mediumint UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品ID',
  `物品使用奖励` int NOT NULL DEFAULT 0 COMMENT '物品使用奖励（对应_模板_奖励.id）',
  `GM命令` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL COMMENT '执行GM命令,格式 .add 25',
  `消耗物品` tinyint NOT NULL DEFAULT 1 COMMENT '是否消耗物品(1=是,0=否)',
  PRIMARY KEY (`entry`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item Use Reward System' ROW_FORMAT = FIXED;

-- 添加示例数据
-- INSERT INTO `_物品_使用获得` VALUES ('测试物品', 12345, 1, '.add 25', 1);

-- 创建物品购买限制表
DROP TABLE IF EXISTS `_物品_每日购买次数`;
CREATE TABLE `_物品_每日购买次数`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL COMMENT '备注描述',
  `物品id` int UNSIGNED NOT NULL DEFAULT 0,
  `每日限制账号次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到次数将，无法继续购买',
  `每日限制角色次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到次数将，无法继续购买',
  `每日限制全服次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到次数将，无法继续购买',
  `永久限制账号次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到次数将，无法继续购买',
  `永久限制角色次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到次数将，无法继续购买',
  `永久限制全服次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到次数将，无法继续购买',
  PRIMARY KEY (`物品id`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = FIXED;


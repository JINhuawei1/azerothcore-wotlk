SET FOREIGN_KEY_CHECKS = 0;

-- 本版图鉴不再提供套装/羁绊效果；清理旧版图鉴套装承载技能。
DELETE FROM `_物品技能_模板`
WHERE (`id` BETWEEN 960001 AND 960370)
   OR (`id` BETWEEN 961001 AND 961370);

DROP TABLE IF EXISTS `_图鉴系统_套装`;
CREATE TABLE `_图鉴系统_套装` (
  `注释` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `id` int UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '保留兼容字段，本版不配置数据',
  `需要激活数量` int UNSIGNED NOT NULL DEFAULT 0,
  `激活描述` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `激活模版_物品技能_多个逗号隔开` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `激活后执行GM命令` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `idx_组` (`组`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '图鉴套装兼容空表' ROW_FORMAT = DYNAMIC;

SET FOREIGN_KEY_CHECKS = 1;

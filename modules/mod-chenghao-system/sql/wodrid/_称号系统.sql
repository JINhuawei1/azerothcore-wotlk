DROP TABLE IF EXISTS `_称号系统`;
CREATE TABLE `_称号系统`  (
  `id` int UNSIGNED NOT NULL DEFAULT 0,
  `称号等级` int UNSIGNED NOT NULL DEFAULT 0,
  `需求系统id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联 mod-requirement-template 的 _模板_需求.id',
  `属性值1` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '属性值1：生命加成',
  `属性值2` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '属性值2：减伤百分比',
  `称号描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '插件和游戏内显示的描述',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `idx_称号等级` (`称号等级`) USING BTREE,
  KEY `idx_需求系统id` (`需求系统id`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '自定义UI称号系统' ROW_FORMAT = DYNAMIC;

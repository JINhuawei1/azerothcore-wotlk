DROP TABLE IF EXISTS `_回血神符`;
CREATE TABLE `_回血神符` (
  `id` int UNSIGNED NOT NULL DEFAULT 0,
  `回血神符名称` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '客户端插件显示名称',
  `回血神符描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '客户端插件显示描述',
  `回血等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '1-999999',
  `需求系统id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联 mod-requirement-template 的 _模板_需求.id',
  `血蓝设置` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '1=自定义值 2=百分比',
  `血蓝值` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '自定义模式时写每秒恢复点数，百分比模式时写每秒恢复百分比，最高 100；怒气/能量/符文能量按 10:1 恢复',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `idx_回血等级` (`回血等级`) USING BTREE,
  KEY `idx_需求系统id` (`需求系统id`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '回血神符系统配置' ROW_FORMAT = DYNAMIC;

-- 合成系统配置表
DROP TABLE IF EXISTS `_物品合成`;
CREATE TABLE `_物品合成` (
  `物品id` int unsigned NOT NULL DEFAULT 0 COMMENT '需要合成/升级的物品ID',
  `升级等级` int unsigned NOT NULL DEFAULT 0 COMMENT '合成/升级等级，同一物品可配置多个等级',
  `需求id` int unsigned NOT NULL DEFAULT 0 COMMENT '关联 _模板_需求.id，0表示无需求',
  `升级成功奖励id` int unsigned NOT NULL DEFAULT 0 COMMENT '关联 _模板_奖励.id，合成成功后发放',
  `成功几率` float unsigned NOT NULL DEFAULT 100 COMMENT '基础成功几率，1-100，写多少就是多少几率',
  `合成几率物品id` int unsigned NOT NULL DEFAULT 0 COMMENT '可选几率提升物品ID，0表示不用提升物品',
  `合成几率提升` float unsigned NOT NULL DEFAULT 0 COMMENT '使用上方物品后提升的成功几率，1-100',
  `失败是否摧毁` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0失败不摧毁合成物品，1失败摧毁合成物品',
  PRIMARY KEY (`物品id`, `升级等级`),
  KEY `idx_需求id` (`需求id`),
  KEY `idx_升级成功奖励id` (`升级成功奖励id`),
  KEY `idx_合成几率物品id` (`合成几率物品id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='合成系统配置表';

-- 示例：把下面物品ID、需求ID、奖励ID替换成你的正式配置后再启用
-- INSERT INTO `_物品合成`
-- (`物品id`, `升级等级`, `需求id`, `升级成功奖励id`, `成功几率`, `合成几率物品id`, `合成几率提升`, `失败是否摧毁`)
-- VALUES
-- (19019, 1, 1, 1, 50, 6948, 10, 1);

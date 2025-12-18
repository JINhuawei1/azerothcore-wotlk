-- 创建_天赋之魂_玩家数据表
-- 使用紧凑格式存储玩家的多技能数据
-- 格式: 技能ID,公共cd等级,冷却等级,消耗等级,伤害等级;技能ID,公共cd等级,冷却等级,消耗等级,伤害等级;...
-- 例如: 133,1,2,1,3;78,5,5,5,5;100,10,10,10,10
DROP TABLE IF EXISTS `_天赋之魂_玩家数据`;
CREATE TABLE `_天赋之魂_玩家数据` (
  `角色id` int unsigned NOT NULL COMMENT '角色GUID',
  `天赋点` int unsigned NOT NULL DEFAULT '0' COMMENT '玩家已使用的天赋点总数',
  `技能数据` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT '紧凑格式: 技能ID,公共cd等级,冷却等级,消耗等级,伤害等级;...',
  PRIMARY KEY (`角色id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='天赋之魂-玩家数据存储表(紧凑格式)';

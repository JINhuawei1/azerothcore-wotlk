-- 创建_天赋之魂表
-- 用于配置技能的各种效果修改（每级的效果值）
DROP TABLE IF EXISTS `_天赋之魂`;
CREATE TABLE `_天赋之魂` (
  `id` int unsigned NOT NULL AUTO_INCREMENT COMMENT '唯一标识',
  `职业类型` int unsigned NOT NULL DEFAULT '0' COMMENT '职业类型(0=全职业,1=战士,2=圣骑士,3=猎人,4=盗贼,5=牧师,6=DK,7=萨满,8=法师,9=术士,11=德鲁伊)',
  `天赋点` int unsigned NOT NULL DEFAULT '0' COMMENT '需要的天赋点数(0=无需求)',
  `技能id` int unsigned NOT NULL DEFAULT '0' COMMENT '目标技能ID',
  `公共cd` float NOT NULL DEFAULT '0' COMMENT '每级公共CD减少百分比',
  `公共cd上限` int unsigned NOT NULL DEFAULT '10' COMMENT '公共CD最大等级',
  `技能冷却` float NOT NULL DEFAULT '0' COMMENT '每级技能冷却减少百分比',
  `技能冷却上限` int unsigned NOT NULL DEFAULT '10' COMMENT '技能冷却最大等级',
  `技能消耗` float NOT NULL DEFAULT '0' COMMENT '每级技能消耗减少百分比',
  `技能消耗上限` int unsigned NOT NULL DEFAULT '10' COMMENT '技能消耗最大等级',
  `伤害加成` float NOT NULL DEFAULT '0' COMMENT '每级伤害加成百分比',
  `伤害加成上限` int unsigned NOT NULL DEFAULT '10' COMMENT '伤害加成最大等级',
  `效果描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '' COMMENT '效果描述',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_skill_id` (`技能id`),
  KEY `idx_class` (`职业类型`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='天赋之魂-技能效果配置表';

-- 插入示例数据
-- 每次点击升级一次，效果叠加
-- 职业类型: 0=全职业, 1=战士, 2=圣骑士, 3=猎人, 4=盗贼, 5=牧师, 6=DK, 7=萨满, 8=法师, 9=术士, 11=德鲁伊
INSERT INTO `_天赋之魂` (`职业类型`, `天赋点`, `技能id`, `公共cd`, `公共cd上限`, `技能冷却`, `技能冷却上限`, `技能消耗`, `技能消耗上限`, `伤害加成`, `伤害加成上限`, `效果描述`) VALUES
(0, 0, 100, 1, 10, 2, 10, 1.5, 10, 2.5, 10, '示例技能(全职业)：每级公共CD-1%，冷却-2%，消耗-1.5%，伤害+2.5%'),
(1, 5, 78, 1, 10, 2, 10, 1, 10, 3, 10, '战士-英勇打击：需要5天赋点，每级公共CD-1%，冷却-2%，消耗-1%，伤害+3%');

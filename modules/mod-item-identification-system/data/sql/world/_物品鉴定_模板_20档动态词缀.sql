SET NAMES utf8mb4;

DROP TEMPORARY TABLE IF EXISTS `temp_item_ident_rank`;
CREATE TEMPORARY TABLE `temp_item_ident_rank` (
  `rank_id` int unsigned NOT NULL,
  `chance` int unsigned NOT NULL,
  `pct_min` int NOT NULL,
  `pct_max` int NOT NULL,
  `attr_min_count` int NOT NULL,
  `attr_max_count` int NOT NULL,
  `color_name` varchar(32) NOT NULL,
  `color_prefix` varchar(16) NOT NULL,
  PRIMARY KEY (`rank_id`)
) ENGINE=Memory DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

INSERT INTO `temp_item_ident_rank`
(`rank_id`, `chance`, `pct_min`, `pct_max`, `attr_min_count`, `attr_max_count`, `color_name`, `color_prefix`) VALUES
(1, 1600, 5, 20, 0, 1, '|cFF33FF00 绿色', '|cFF33FF00'),
(2, 1400, 10, 25, 0, 1, '|cFF33FF00 绿色', '|cFF33FF00'),
(3, 1200, 15, 35, 0, 1, '|cFF33FF00 绿色', '|cFF33FF00'),
(4, 1000, 20, 45, 1, 1, '|cFF33FF00 绿色', '|cFF33FF00'),
(5, 850, 25, 55, 1, 1, '|cFF33FF00 绿色', '|cFF33FF00'),
(6, 700, 30, 65, 1, 1, '|cFF33FF00 绿色', '|cFF33FF00'),
(7, 600, 35, 75, 1, 1, '|cFF3300FF 蓝色', '|cFF3300FF'),
(8, 500, 40, 90, 1, 1, '|cFF3300FF 蓝色', '|cFF3300FF'),
(9, 420, 50, 105, 1, 2, '|cFF3300FF 蓝色', '|cFF3300FF'),
(10, 360, 60, 120, 1, 2, '|cFF3300FF 蓝色', '|cFF3300FF'),
(11, 300, 70, 135, 1, 2, '|cFFFF00FF 紫色', '|cFFFF00FF'),
(12, 250, 80, 150, 1, 2, '|cFFFF00FF 紫色', '|cFFFF00FF'),
(13, 210, 95, 170, 2, 3, '|cFFFF00FF 紫色', '|cFFFF00FF'),
(14, 170, 110, 190, 2, 3, '|cFFFF00FF 紫色', '|cFFFF00FF'),
(15, 140, 130, 210, 2, 3, '|cFFFFCC00 橙色', '|cFFFFCC00'),
(16, 110, 150, 230, 2, 3, '|cFFFFCC00 橙色', '|cFFFFCC00'),
(17, 80, 175, 250, 3, 4, '|cFFFFCC00 橙色', '|cFFFFCC00'),
(18, 60, 200, 270, 3, 4, '|cFFCC0033 红色', '|cFFCC0033'),
(19, 35, 225, 290, 3, 5, '|cFFCC0033 红色', '|cFFCC0033'),
(20, 15, 250, 300, 4, 5, '|cFFCC0033 红色', '|cFFCC0033');

DELETE FROM `_物品鉴定_模板`;

INSERT INTO `_物品鉴定_模板` (
  `注释`,
  `id`,
  `组`,
  `等级`,
  `随机几率`,
  `物品成长_系统`,
  `成长属性最小数量`,
  `成长属性最大数量`,
  `成长属性最小属性值`,
  `成长属性最大属性值`,
  `物品强化_系统`,
  `强化属性最小数量`,
  `强化属性最大数量`,
  `强化属性最小属性值`,
  `强化属性最大属性值`,
  `物品属性_模板`,
  `基础属性最小数量`,
  `基础属性最大数量`,
  `基础最小属性值`,
  `基础最大属性值`,
  `基础属性允许重复`,
  `物品属性_模板_组`,
  `追加属性最小数量`,
  `追加属性最大数量`,
  `追加属性最小值`,
  `追加属性最大值`,
  `追加属性允许重复`,
  `物品技能_模板_组`,
  `追加技能最小数量`,
  `追加技能最大数量`,
  `追加技能允许重复`,
  `技能魔次_模板_组`,
  `技能魔次最小数量`,
  `技能魔次最大数量`,
  `技能魔次最小魔次`,
  `技能魔次最大魔次`,
  `技能魔次允许重复`,
  `需求_模板`,
  `符文系统_符文`,
  `符文凹槽最小数量`,
  `符文凹槽最大数量`,
  `技能模板_套装_组`,
  `公告模板`,
  `品质颜色`,
  `物品名字前缀`,
  `物品名字后缀`,
  `物品名字颜色_多个逗号隔开`,
  `物品底部描述`
)
SELECT
  CONCAT('动态词缀 ', LPAD(r.`rank_id`, 2, '0')) AS `注释`,
  (g.`group_id` - 1) * 20 + r.`rank_id` AS `id`,
  g.`group_id` AS `组`,
  0 AS `等级`,
  r.`chance` AS `随机几率`,
  '' AS `物品成长_系统`,
  0 AS `成长属性最小数量`,
  0 AS `成长属性最大数量`,
  0 AS `成长属性最小属性值`,
  0 AS `成长属性最大属性值`,
  '' AS `物品强化_系统`,
  0 AS `强化属性最小数量`,
  0 AS `强化属性最大数量`,
  0 AS `强化属性最小属性值`,
  0 AS `强化属性最大属性值`,
  '0' AS `物品属性_模板`,
  r.`attr_min_count` AS `基础属性最小数量`,
  r.`attr_max_count` AS `基础属性最大数量`,
  CEIL(r.`pct_min` * (1.20 + ((g.`group_id` - 1) / 50.0))) AS `基础最小属性值`,
  CEIL(r.`pct_max` * (1.20 + ((g.`group_id` - 1) / 50.0))) AS `基础最大属性值`,
  1 AS `基础属性允许重复`,
  '2' AS `物品属性_模板_组`,
  r.`attr_min_count` AS `追加属性最小数量`,
  r.`attr_max_count` AS `追加属性最大数量`,
  CEIL(r.`pct_min` * (1.50 + ((g.`group_id` - 1) / 40.0))) AS `追加属性最小值`,
  CEIL(r.`pct_max` * (1.50 + ((g.`group_id` - 1) / 40.0))) AS `追加属性最大值`,
  1 AS `追加属性允许重复`,
  CASE WHEN r.`rank_id` >= 17 THEN '1' ELSE '' END AS `物品技能_模板_组`,
  CASE WHEN r.`rank_id` >= 17 THEN 1 ELSE 0 END AS `追加技能最小数量`,
  CASE WHEN r.`rank_id` >= 17 THEN 1 ELSE 0 END AS `追加技能最大数量`,
  1 AS `追加技能允许重复`,
  '' AS `技能魔次_模板_组`,
  0 AS `技能魔次最小数量`,
  0 AS `技能魔次最大数量`,
  0 AS `技能魔次最小魔次`,
  0 AS `技能魔次最大魔次`,
  1 AS `技能魔次允许重复`,
  0 AS `需求_模板`,
  '' AS `符文系统_符文`,
  0 AS `符文凹槽最小数量`,
  0 AS `符文凹槽最大数量`,
  '' AS `技能模板_套装_组`,
  0 AS `公告模板`,
  r.`color_name` AS `品质颜色`,
  r.`color_prefix` AS `物品名字前缀`,
  CONCAT('|cFFFF0000 ', r.`rank_id`) AS `物品名字后缀`,
  '' AS `物品名字颜色_多个逗号隔开`,
  '' AS `物品底部描述`
FROM (
  SELECT ones.`n` + tens.`n` * 10 + hundreds.`n` * 100 + thousands.`n` * 1000 + 1 AS `group_id`
  FROM
    (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) ones
    CROSS JOIN (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) tens
    CROSS JOIN (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) hundreds
    CROSS JOIN (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4) thousands
) g
CROSS JOIN `temp_item_ident_rank` r
WHERE g.`group_id` BETWEEN 1 AND 5000
ORDER BY g.`group_id`, r.`rank_id`;

DROP TEMPORARY TABLE IF EXISTS `temp_item_ident_rank`;

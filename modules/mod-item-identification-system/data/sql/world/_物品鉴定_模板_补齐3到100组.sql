-- 物品鉴定模板：补齐组3到组100
-- 以当前数据库中的组2为模板生成，每组递增5点基础属性值和追加属性值。
-- 组内10条模板都参与随机几率抽取，等级统一设为0作为通配。
-- 只重建组3到组100；最后会把组1到组100的等级统一修正为0。

SET @GROUP_STEP := 5;

DELETE FROM `_物品鉴定_模板`
WHERE `id` BETWEEN 21 AND 1000
  AND `组` BETWEEN 3 AND 100;

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
  t.`注释`,
  (g.`组` - 1) * 10 + (t.`id` - 10) AS `id`,
  g.`组`,
  0 AS `等级`,
  t.`随机几率`,
  t.`物品成长_系统`,
  t.`成长属性最小数量`,
  t.`成长属性最大数量`,
  t.`成长属性最小属性值`,
  t.`成长属性最大属性值`,
  t.`物品强化_系统`,
  t.`强化属性最小数量`,
  t.`强化属性最大数量`,
  t.`强化属性最小属性值`,
  t.`强化属性最大属性值`,
  t.`物品属性_模板`,
  t.`基础属性最小数量`,
  t.`基础属性最大数量`,
  t.`基础最小属性值` + ((g.`组` - 2) * @GROUP_STEP) AS `基础最小属性值`,
  t.`基础最大属性值` + ((g.`组` - 2) * @GROUP_STEP) AS `基础最大属性值`,
  t.`基础属性允许重复`,
  t.`物品属性_模板_组`,
  t.`追加属性最小数量`,
  t.`追加属性最大数量`,
  t.`追加属性最小值` + ((g.`组` - 2) * @GROUP_STEP) AS `追加属性最小值`,
  t.`追加属性最大值` + ((g.`组` - 2) * @GROUP_STEP) AS `追加属性最大值`,
  t.`追加属性允许重复`,
  t.`物品技能_模板_组`,
  t.`追加技能最小数量`,
  t.`追加技能最大数量`,
  t.`追加技能允许重复`,
  t.`技能魔次_模板_组`,
  t.`技能魔次最小数量`,
  t.`技能魔次最大数量`,
  t.`技能魔次最小魔次`,
  t.`技能魔次最大魔次`,
  t.`技能魔次允许重复`,
  t.`需求_模板`,
  t.`符文系统_符文`,
  t.`符文凹槽最小数量`,
  t.`符文凹槽最大数量`,
  t.`技能模板_套装_组`,
  t.`公告模板`,
  t.`品质颜色`,
  t.`物品名字前缀`,
  t.`物品名字后缀`,
  t.`物品名字颜色_多个逗号隔开`,
  t.`物品底部描述`
FROM `_物品鉴定_模板` AS t
CROSS JOIN (
  SELECT 3 AS `组` UNION ALL
  SELECT 4 UNION ALL
  SELECT 5 UNION ALL
  SELECT 6 UNION ALL
  SELECT 7 UNION ALL
  SELECT 8 UNION ALL
  SELECT 9 UNION ALL
  SELECT 10 UNION ALL
  SELECT 11 UNION ALL
  SELECT 12 UNION ALL
  SELECT 13 UNION ALL
  SELECT 14 UNION ALL
  SELECT 15 UNION ALL
  SELECT 16 UNION ALL
  SELECT 17 UNION ALL
  SELECT 18 UNION ALL
  SELECT 19 UNION ALL
  SELECT 20 UNION ALL
  SELECT 21 UNION ALL
  SELECT 22 UNION ALL
  SELECT 23 UNION ALL
  SELECT 24 UNION ALL
  SELECT 25 UNION ALL
  SELECT 26 UNION ALL
  SELECT 27 UNION ALL
  SELECT 28 UNION ALL
  SELECT 29 UNION ALL
  SELECT 30 UNION ALL
  SELECT 31 UNION ALL
  SELECT 32 UNION ALL
  SELECT 33 UNION ALL
  SELECT 34 UNION ALL
  SELECT 35 UNION ALL
  SELECT 36 UNION ALL
  SELECT 37 UNION ALL
  SELECT 38 UNION ALL
  SELECT 39 UNION ALL
  SELECT 40 UNION ALL
  SELECT 41 UNION ALL
  SELECT 42 UNION ALL
  SELECT 43 UNION ALL
  SELECT 44 UNION ALL
  SELECT 45 UNION ALL
  SELECT 46 UNION ALL
  SELECT 47 UNION ALL
  SELECT 48 UNION ALL
  SELECT 49 UNION ALL
  SELECT 50 UNION ALL
  SELECT 51 UNION ALL
  SELECT 52 UNION ALL
  SELECT 53 UNION ALL
  SELECT 54 UNION ALL
  SELECT 55 UNION ALL
  SELECT 56 UNION ALL
  SELECT 57 UNION ALL
  SELECT 58 UNION ALL
  SELECT 59 UNION ALL
  SELECT 60 UNION ALL
  SELECT 61 UNION ALL
  SELECT 62 UNION ALL
  SELECT 63 UNION ALL
  SELECT 64 UNION ALL
  SELECT 65 UNION ALL
  SELECT 66 UNION ALL
  SELECT 67 UNION ALL
  SELECT 68 UNION ALL
  SELECT 69 UNION ALL
  SELECT 70 UNION ALL
  SELECT 71 UNION ALL
  SELECT 72 UNION ALL
  SELECT 73 UNION ALL
  SELECT 74 UNION ALL
  SELECT 75 UNION ALL
  SELECT 76 UNION ALL
  SELECT 77 UNION ALL
  SELECT 78 UNION ALL
  SELECT 79 UNION ALL
  SELECT 80 UNION ALL
  SELECT 81 UNION ALL
  SELECT 82 UNION ALL
  SELECT 83 UNION ALL
  SELECT 84 UNION ALL
  SELECT 85 UNION ALL
  SELECT 86 UNION ALL
  SELECT 87 UNION ALL
  SELECT 88 UNION ALL
  SELECT 89 UNION ALL
  SELECT 90 UNION ALL
  SELECT 91 UNION ALL
  SELECT 92 UNION ALL
  SELECT 93 UNION ALL
  SELECT 94 UNION ALL
  SELECT 95 UNION ALL
  SELECT 96 UNION ALL
  SELECT 97 UNION ALL
  SELECT 98 UNION ALL
  SELECT 99 UNION ALL
  SELECT 100
) AS g
WHERE t.`组` = 2
ORDER BY g.`组`, t.`id`;

UPDATE `_物品鉴定_模板`
SET `等级` = 0
WHERE `组` BETWEEN 1 AND 100;

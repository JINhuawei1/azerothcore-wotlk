-- ============================================================
-- 宣传神器物品模板 (world.item_template)
-- 号段: 997001-998000,对应 宣传神器1-宣传神器1000
-- 属性: 每级全属性 = 1999 + (等级 - 1) * 1000
-- 奖励: 同步对接 `_模板_奖励` 100-1099,每级奖励对应同级宣传神器
-- 背包/银行生效: 同步对接 `_物品_放背包加属性`,生效位置=2(背包+银行)
-- ============================================================

DROP TEMPORARY TABLE IF EXISTS `_tmp_promotion_weapon_levels`;
CREATE TEMPORARY TABLE `_tmp_promotion_weapon_levels` (
  `lvl` int UNSIGNED NOT NULL,
  PRIMARY KEY (`lvl`) USING BTREE
) ENGINE = MEMORY;

INSERT INTO `_tmp_promotion_weapon_levels` (`lvl`)
SELECT d0.`n` + d1.`n` * 10 + d2.`n` * 100 + 1 AS `lvl`
FROM
  (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
   UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d0
CROSS JOIN
  (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
   UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d1
CROSS JOIN
  (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
   UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d2
ORDER BY `lvl`;

DELETE FROM `item_template`
WHERE `entry` BETWEEN 997001 AND 998000;

INSERT INTO `item_template`
  (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
   `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`,
   `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `StatsCount`,
   `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`,
   `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`, `stat_type6`, `stat_value6`,
   `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`,
   `stat_type10`, `stat_value10`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `delay`,
   `bonding`, `description`, `Material`, `sheath`, `MaxDurability`, `VerifiedBuild`)
SELECT
  997000 + l.`lvl`,
  2,
  7,
  -1,
  CONCAT('宣传神器', l.`lvl`),
  30606,
  5,
  0,
  0,
  1,
  0,
  0,
  13,
  -1,
  -1,
  l.`lvl`,
  1,
  0,
  1,
  10,
  4,  999 + l.`lvl` * 1000,
  3,  999 + l.`lvl` * 1000,
  5,  999 + l.`lvl` * 1000,
  6,  999 + l.`lvl` * 1000,
  31, 999 + l.`lvl` * 1000,
  32, 999 + l.`lvl` * 1000,
  36, 999 + l.`lvl` * 1000,
  38, 999 + l.`lvl` * 1000,
  45, 999 + l.`lvl` * 1000,
  47, 999 + l.`lvl` * 1000,
  999 + l.`lvl` * 1000,
  1999 + l.`lvl` * 1000,
  0,
  1900,
  1,
  CONCAT('宣传神器', l.`lvl`, '级,全属性+', 999 + l.`lvl` * 1000, ',伤害', 999 + l.`lvl` * 1000, '-', 1999 + l.`lvl` * 1000),
  1,
  1,
  125,
  12340
FROM `_tmp_promotion_weapon_levels` l
ORDER BY l.`lvl`;

DELETE FROM `_模板_奖励`
WHERE `id` BETWEEN 100 AND 1099;

INSERT INTO `_模板_奖励`
  (`id`, `注释`, `几率`, `经验`, `金币`, `泡点`, `积分`, `奖励物品`, `是否关闭提示`, `客户端显示`)
SELECT
  99 + l.`lvl`,
  CONCAT('宣传神器', l.`lvl`, '奖励'),
  100,
  0,
  0,
  0,
  0,
  CONCAT(997000 + l.`lvl`, ' 1'),
  0,
  CONCAT('宣传神器', l.`lvl`, ' x1')
FROM `_tmp_promotion_weapon_levels` l
ORDER BY l.`lvl`;

DELETE FROM `_物品_放背包加属性`
WHERE `entry` BETWEEN 997001 AND 998000;

INSERT INTO `_物品_放背包加属性`
  (`注释`, `entry`, `生效背包位置`)
SELECT
  CONCAT('宣传神器', l.`lvl`, '背包银行属性生效'),
  997000 + l.`lvl`,
  2
FROM `_tmp_promotion_weapon_levels` l
ORDER BY l.`lvl`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_promotion_weapon_levels`;

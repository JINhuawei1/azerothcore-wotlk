-- ============================================================
-- 宣传神武实验 (world 数据库)
-- 物品: 450001-450200，对应 宣传神武1-宣传神武200
-- 奖励: _模板_奖励 1100-1299
-- CDK组: 1（由网页提交后 worldserver 自动生成）
--
-- 9999垓 = 999900000000000000000000
-- 递增曲线: 1级1亿、2级10亿、3级100亿、4级1000亿；之后增长倍率逐级平滑降低。
-- 曲线衰减率经过计算，使200级在持续严格递增的前提下精确落到9999垓。
-- 第200把全属性 = 999900000000000000000000（精确）
-- ============================================================

SET @promotion_weapon_entry_first = 450001;
SET @promotion_reward_id_first = 1100;
SET @promotion_weapon_levels = 200;
SET @promotion_curve_decay = 0.92857123960004195277;
SET @promotion_attr_max = CAST(999900000000000000000000 AS DECIMAL(65,0));

DROP TEMPORARY TABLE IF EXISTS `_tmp_promotion_divine_weapon_levels`;
CREATE TEMPORARY TABLE `_tmp_promotion_divine_weapon_levels` (
  `lvl` int UNSIGNED NOT NULL,
  `attr_value` decimal(65,0) NOT NULL DEFAULT 0,
  `next_attr_value` decimal(65,0) NOT NULL DEFAULT 0,
  PRIMARY KEY (`lvl`) USING BTREE
) ENGINE = MEMORY;

INSERT INTO `_tmp_promotion_divine_weapon_levels` (`lvl`)
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
WHERE d0.`n` + d1.`n` * 10 + d2.`n` * 100 + 1 <= @promotion_weapon_levels
ORDER BY `lvl`;

UPDATE `_tmp_promotion_divine_weapon_levels`
SET
  `attr_value` = CASE `lvl`
    WHEN 1 THEN CAST(100000000 AS DECIMAL(65,0))
    WHEN 2 THEN CAST(1000000000 AS DECIMAL(65,0))
    WHEN 3 THEN CAST(10000000000 AS DECIMAL(65,0))
    WHEN 200 THEN @promotion_attr_max
    ELSE CAST(ROUND(POW(
      10,
      10 + (1 - POW(@promotion_curve_decay, `lvl` - 3)) / (1 - @promotion_curve_decay)
    ), 0) AS DECIMAL(65,0))
  END,
  `next_attr_value` = CASE
    WHEN `lvl` >= @promotion_weapon_levels THEN CASE `lvl`
      WHEN 200 THEN @promotion_attr_max
      ELSE CAST(ROUND(POW(
        10,
        10 + (1 - POW(@promotion_curve_decay, `lvl` - 3)) / (1 - @promotion_curve_decay)
      ), 0) AS DECIMAL(65,0))
    END
    WHEN `lvl` + 1 = 2 THEN CAST(1000000000 AS DECIMAL(65,0))
    WHEN `lvl` + 1 = 3 THEN CAST(10000000000 AS DECIMAL(65,0))
    WHEN `lvl` + 1 = 200 THEN @promotion_attr_max
    ELSE CAST(ROUND(POW(
      10,
      10 + (1 - POW(@promotion_curve_decay, (`lvl` + 1) - 3)) / (1 - @promotion_curve_decay)
    ), 0) AS DECIMAL(65,0))
  END;

-- 只删除本模块自己命名的旧实验行；若号段被其它物品占用，后续INSERT会报冲突但不会覆盖。
DELETE FROM `item_template`
WHERE `entry` BETWEEN @promotion_weapon_entry_first
                  AND @promotion_weapon_entry_first + @promotion_weapon_levels - 1
  AND `name` REGEXP '^宣传神武[0-9]+$';

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
  @promotion_weapon_entry_first + l.`lvl` - 1,
  2,
  7,
  -1,
  CONCAT('宣传神武', l.`lvl`),
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
  4,  l.`attr_value`,
  3,  l.`attr_value`,
  5,  l.`attr_value`,
  6,  l.`attr_value`,
  31, l.`attr_value`,
  32, l.`attr_value`,
  36, l.`attr_value`,
  38, l.`attr_value`,
  45, l.`attr_value`,
  47, l.`attr_value`,
  CAST(l.`attr_value` AS DOUBLE),
  CAST(l.`next_attr_value` AS DOUBLE),
  0,
  1900,
  1,
  '',
  1,
  1,
  125,
  12340
FROM `_tmp_promotion_divine_weapon_levels` l
ORDER BY l.`lvl`;

-- 奖励模板只删除本模块自己创建的行，避免覆盖同ID的其它奖励。
DELETE FROM `_模板_奖励`
WHERE `id` BETWEEN @promotion_reward_id_first
               AND @promotion_reward_id_first + @promotion_weapon_levels - 1
  AND `注释` REGEXP '^宣传神武[0-9]+奖励$';

INSERT INTO `_模板_奖励`
  (`id`, `注释`, `几率`, `经验`, `金币`, `泡点`, `积分`, `奖励物品`, `是否关闭提示`, `客户端显示`)
SELECT
  @promotion_reward_id_first + l.`lvl` - 1,
  CONCAT('宣传神武', l.`lvl`, '奖励'),
  100,
  0,
  0,
  0,
  0,
  CONCAT(@promotion_weapon_entry_first + l.`lvl` - 1, ' 1'),
  0,
  CONCAT('宣传神武', l.`lvl`, ' x1')
FROM `_tmp_promotion_divine_weapon_levels` l
ORDER BY l.`lvl`;

DELETE FROM `_物品_放背包加属性`
WHERE `entry` BETWEEN @promotion_weapon_entry_first
                  AND @promotion_weapon_entry_first + @promotion_weapon_levels - 1
  AND `注释` REGEXP '^宣传神武[0-9]+背包银行属性生效$';

INSERT INTO `_物品_放背包加属性`
  (`注释`, `entry`, `生效背包位置`)
SELECT
  CONCAT('宣传神武', l.`lvl`, '背包银行属性生效'),
  @promotion_weapon_entry_first + l.`lvl` - 1,
  2
FROM `_tmp_promotion_divine_weapon_levels` l
ORDER BY l.`lvl`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_promotion_divine_weapon_levels`;

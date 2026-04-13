-- ============================================
-- 首版 item_template 对接：由 _深渊物品模板对接 批量生成
-- 说明：先落最小可用模板，displayid 优先复用同类现有物品作为占位
-- ============================================

DELETE FROM `item_template`
WHERE (`entry` BETWEEN 950001 AND 993666)
   OR (`entry` BETWEEN 960001 AND 960006)
   OR `entry` = 970001;

/*
REPLACE INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`,
 `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `StatsCount`,
 `ScalingStatDistribution`, `ScalingStatValue`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `dmg_min2`, `dmg_max2`, `dmg_type2`,
 `armor`, `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`, `delay`, `ammo_type`, `RangedModRange`,
 `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,
 `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`,
 `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`,
 `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`,
 `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`,
 `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`,
 `RandomProperty`, `RandomSuffix`, `block`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`,
 `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`,
 `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `duration`, `ItemLimitCategory`, `HolidayId`, `ScriptName`, `DisenchantID`,
 `FoodType`, `minMoneyLoot`, `maxMoneyLoot`, `flagsCustom`)
SELECT
  d.`物品ID`,
  d.`建议物品分类`,
  d.`建议物品子类`,
  -1,
  d.`物品名称`,
  COALESCE(
    NULLIF(d.`预留显示ID`, 0),
    (
      SELECT it.`displayid`
      FROM `item_template` it
      WHERE it.`class` = d.`建议物品分类`
        AND it.`subclass` = d.`建议物品子类`
        AND it.`InventoryType` = d.`建议装备槽位`
        AND it.`displayid` <> 0
      ORDER BY it.`entry`
      LIMIT 1
    ),
    (
      SELECT it.`displayid`
      FROM `item_template` it
      WHERE it.`class` = d.`建议物品分类`
        AND it.`subclass` = d.`建议物品子类`
        AND it.`displayid` <> 0
      ORDER BY it.`entry`
      LIMIT 1
    ),
    2588
  ),
  d.`建议物品品质`,
  0,
  0,
  1,
  0,
  0,
  d.`建议装备槽位`,
  -1,
  -1,
  CASE
    WHEN d.`章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  CASE
    WHEN d.`章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  CASE WHEN d.`是否唯一` = 1 THEN 1 ELSE 0 END,
  1,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  1,
  d.`说明`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  '',
  0,
  0,
  0,
  0,
  0
FROM `_深渊物品模板对接` d
LEFT JOIN `_深渊章节配置` c ON c.`章节ID` = d.`章节ID`
WHERE (d.`物品ID` BETWEEN 950001 AND 993666)
   OR (d.`物品ID` BETWEEN 960001 AND 960006)
   OR d.`物品ID` = 970001;
*/

REPLACE INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `description`, `Material`, `sheath`, `block`, `itemset`, `MaxDurability`,
 `area`, `Map`, `BagFamily`, `TotemCategory`, `FoodType`, `minMoneyLoot`, `maxMoneyLoot`, `flagsCustom`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `duration`,
 `ItemLimitCategory`, `HolidayId`, `DisenchantID`)
SELECT
  d.`物品ID`,
  d.`建议物品分类`,
  d.`建议物品子类`,
  -1,
  d.`物品名称`,
  COALESCE(
    NULLIF(d.`预留显示ID`, 0),
    (
      SELECT it.`displayid`
      FROM `item_template` it
      WHERE it.`class` = d.`建议物品分类`
        AND it.`subclass` = d.`建议物品子类`
        AND it.`InventoryType` = d.`建议装备槽位`
        AND it.`displayid` <> 0
      ORDER BY it.`entry`
      LIMIT 1
    ),
    (
      SELECT it.`displayid`
      FROM `item_template` it
      WHERE it.`class` = d.`建议物品分类`
        AND it.`subclass` = d.`建议物品子类`
        AND it.`displayid` <> 0
      ORDER BY it.`entry`
      LIMIT 1
    ),
    2588
  ),
  d.`建议物品品质`,
  0,
  0,
  1,
  0,
  0,
  d.`建议装备槽位`,
  -1,
  -1,
  CASE
    WHEN d.`章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  CASE
    WHEN d.`章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  1,
  1,
  d.`说明`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
FROM `_深渊物品模板对接` d
LEFT JOIN `_深渊章节配置` c ON c.`章节ID` = d.`章节ID`
WHERE (d.`物品ID` BETWEEN 950001 AND 993666)
   OR (d.`物品ID` BETWEEN 960001 AND 960006)
   OR d.`物品ID` = 970001;

-- item_template 兜底：若首轮批量对接后仍有缺失物品，则补建最小可用模板
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `description`, `Material`, `sheath`, `block`, `itemset`, `MaxDurability`,
 `area`, `Map`, `BagFamily`, `TotemCategory`, `FoodType`, `minMoneyLoot`, `maxMoneyLoot`, `flagsCustom`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `duration`,
 `ItemLimitCategory`, `HolidayId`, `DisenchantID`)
SELECT
  d.`物品ID`,
  d.`建议物品分类`,
  d.`建议物品子类`,
  -1,
  d.`物品名称`,
  CASE
    WHEN d.`预留显示ID` <> 0 THEN d.`预留显示ID`
    ELSE 2588
  END,
  d.`建议物品品质`,
  0,
  0,
  1,
  0,
  0,
  d.`建议装备槽位`,
  -1,
  -1,
  CASE
    WHEN d.`章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  CASE
    WHEN d.`章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  1,
  1,
  d.`说明`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
FROM `_深渊物品模板对接` d
LEFT JOIN `_深渊章节配置` c ON c.`章节ID` = d.`章节ID`
LEFT JOIN `item_template` it ON it.`entry` = d.`物品ID`
WHERE it.`entry` IS NULL
  AND (
    (d.`物品ID` BETWEEN 950001 AND 993666)
    OR (d.`物品ID` BETWEEN 960001 AND 960006)
    OR d.`物品ID` = 970001
  );

-- 遗物 / 神器：补描述、脚本名、唯一绑定
UPDATE `item_template` it
JOIN `_深渊遗物配置` r ON r.`物品ID` = it.`entry`
SET
  it.`description` = CASE
    WHEN r.`完整描述` IS NOT NULL AND r.`完整描述` <> '' THEN r.`完整描述`
    ELSE r.`简述`
  END,
  it.`bonding` = 1,
  it.`maxcount` = 1,
  it.`stackable` = 1,
  it.`ScriptName` = '',
  it.`flagsCustom` = CASE
    WHEN r.`激活规则` = 1 THEN 1
    ELSE 0
  END
WHERE it.`entry` BETWEEN 950001 AND 970001;

-- 深渊物品：补首版基础价格，方便查看与测试
UPDATE `item_template`
SET
  `SellPrice` = CASE
    WHEN `Quality` >= 5 THEN GREATEST(`ItemLevel` * 40, 1)
    WHEN `Quality` = 4 THEN GREATEST(`ItemLevel` * 30, 1)
    ELSE GREATEST(`ItemLevel` * 20, 1)
  END,
  `BuyPrice` = CASE
    WHEN `Quality` >= 5 THEN GREATEST(`ItemLevel` * 200, 1)
    WHEN `Quality` = 4 THEN GREATEST(`ItemLevel` * 150, 1)
    ELSE GREATEST(`ItemLevel` * 100, 1)
  END
WHERE (`entry` BETWEEN 950001 AND 993666)
   OR (`entry` BETWEEN 960001 AND 960006)
   OR `entry` = 970001;

-- 深渊装备：补部位映射、装等、绑定、风味文本
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
LEFT JOIN `_深渊章节配置` c ON c.`章节ID` = e.`来源章节`
SET
  it.`class` = CASE
    WHEN e.`部位掩码` = 1 THEN 2
    ELSE 4
  END,
  it.`subclass` = CASE
    WHEN e.`部位掩码` = 1 THEN CASE
      WHEN e.`伤害类型` = 5 THEN 0
      WHEN e.`伤害类型` = 6 THEN 4
      WHEN e.`伤害类型` = 7 THEN 15
      WHEN e.`伤害类型` = 8 THEN 13
      WHEN e.`伤害类型` = 9 THEN 8
      WHEN e.`伤害类型` = 10 THEN 1
      WHEN e.`伤害类型` = 11 THEN 5
      WHEN e.`伤害类型` = 12 THEN 6
      WHEN e.`伤害类型` = 13 THEN 10
      WHEN e.`伤害类型` = 14 THEN 2
      WHEN e.`伤害类型` = 15 THEN 3
      WHEN e.`伤害类型` = 16 THEN 18
      WHEN e.`伤害类型` = 17 THEN 19
      WHEN e.`伤害类型` = 2 THEN 10
      WHEN e.`伤害类型` = 3 THEN 4
      WHEN e.`伤害类型` = 4 THEN 10
      ELSE 7
    END
    WHEN e.`部位掩码` IN (64, 128, 256, 512) THEN 0
    ELSE CASE e.`护甲类型`
      WHEN 1 THEN 1
      WHEN 2 THEN 2
      WHEN 3 THEN 3
      WHEN 4 THEN 4
      ELSE 0
    END
  END,
  it.`InventoryType` = CASE
    WHEN e.`部位掩码` = 1 THEN CASE
      WHEN e.`伤害类型` IN (9, 10, 11, 12, 13) THEN 17
      WHEN e.`伤害类型` = 14 THEN 15
      WHEN e.`伤害类型` IN (15, 16, 17) THEN 26
      ELSE 13
    END
    WHEN e.`部位掩码` = 2 THEN 1
    WHEN e.`部位掩码` = 4 THEN 5
    WHEN e.`部位掩码` = 16 THEN 6
    WHEN e.`部位掩码` = 32 THEN 8
    WHEN e.`部位掩码` = 64 THEN 11
    WHEN e.`部位掩码` = 128 THEN 12
    WHEN e.`部位掩码` = 256 THEN 16
    WHEN e.`部位掩码` = 512 THEN 23
    ELSE 0
  END,
  it.`ItemLevel` = e.`基础装等`,
  it.`RequiredLevel` = CASE
    WHEN c.`需求修仙等级` IS NULL THEN LEAST(GREATEST(FLOOR(e.`基础装等` / 4), 1), 80)
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  it.`bonding` = CASE
    WHEN e.`装备类型` = 2 THEN 1
    ELSE 2
  END,
  it.`maxcount` = CASE
    WHEN e.`装备类型` = 2 THEN 1
    ELSE 0
  END,
  it.`stackable` = 1,
  it.`itemset` = CASE
    WHEN e.`套装ID` <> 0 AND e.`来源模式` BETWEEN 1 AND 4
      THEN 1001 + ((e.`来源章节` - 1) * 4) + (e.`来源模式` - 1)
    WHEN e.`套装ID` <> 0 AND e.`来源模式` = 5
      THEN 1297 + (e.`来源章节` - 1)
    ELSE 0
  END,
  it.`description` = e.`风味文本`
WHERE it.`entry` BETWEEN 980001 AND 993666;

-- 秘藏神器 / 神器套装部件：tooltip 统一走装备法术说明，不保留底部黄色风味描述
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`description` = ''
WHERE e.`装备类型` = 2
  AND e.`是否启用` = 1
  AND it.`entry` BETWEEN 980001 AND 981628;

-- 章节遗物：作为关键奖励物品，统一设为任务类佩戴/背包触发物
UPDATE `item_template` it
JOIN `_深渊遗物配置` r ON r.`物品ID` = it.`entry`
LEFT JOIN `_深渊章节配置` c ON c.`章节ID` = r.`关联章节ID`
SET
  it.`class` = 15,
  it.`subclass` = 0,
  it.`InventoryType` = 0,
  it.`ItemLevel` = CASE
    WHEN r.`关联章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END,
  it.`RequiredLevel` = CASE
    WHEN r.`关联章节ID` = 0 THEN 1
    ELSE LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
  END
WHERE it.`entry` = r.`物品ID`
  AND r.`类型` = 1;

-- 遗物 / 阶段神器 / 终极神器：统一橙色品质与说明风格
UPDATE `item_template` it
JOIN `_深渊遗物配置` r ON r.`物品ID` = it.`entry`
SET
  it.`Quality` = 5,
  it.`description` = CONCAT(
    CASE r.`类型`
      WHEN 2 THEN '[阶段神器] '
      WHEN 3 THEN '[终极神器] '
      ELSE ''
    END,
    CASE
      WHEN r.`完整描述` IS NOT NULL AND r.`完整描述` <> '' THEN r.`完整描述`
      ELSE r.`简述`
    END
  )
WHERE it.`entry` = r.`物品ID`
  AND r.`类型` IN (2, 3);

-- 遗物 / 阶段神器 / 终极神器：16 单独重导时也要把 tooltip / 托管技能重新挂回，避免 spellid 字段被模板重建覆盖
UPDATE `item_template`
SET
  `spellid_1` = 89601,
  `spelltrigger_1` = 1,
  `spellcharges_1` = 0,
  `spellppmRate_1` = 0,
  `spellcooldown_1` = -1,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = -1,
  `spellid_2` = 89602,
  `spelltrigger_2` = 1,
  `spellcharges_2` = 0,
  `spellppmRate_2` = 0,
  `spellcooldown_2` = -1,
  `spellcategory_2` = 0,
  `spellcategorycooldown_2` = -1,
  `spellid_3` = 89100 + (`entry` - 950000),
  `spelltrigger_3` = 1,
  `spellcharges_3` = 0,
  `spellppmRate_3` = 0,
  `spellcooldown_3` = -1,
  `spellcategory_3` = 0,
  `spellcategorycooldown_3` = -1,
  `description` = ''
WHERE `entry` BETWEEN 950001 AND 950074;

UPDATE `item_template`
SET
  `spellid_1` = 89603,
  `spelltrigger_1` = 1,
  `spellcharges_1` = 0,
  `spellppmRate_1` = 0,
  `spellcooldown_1` = -1,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = -1,
  `spellid_2` = 89174 + (`entry` - 960000),
  `spelltrigger_2` = 1,
  `spellcharges_2` = 0,
  `spellppmRate_2` = 0,
  `spellcooldown_2` = -1,
  `spellcategory_2` = 0,
  `spellcategorycooldown_2` = -1,
  `spellid_3` = 0,
  `spelltrigger_3` = 0,
  `spellcharges_3` = 0,
  `spellppmRate_3` = 0,
  `spellcooldown_3` = -1,
  `spellcategory_3` = 0,
  `spellcategorycooldown_3` = -1,
  `description` = ''
WHERE `entry` BETWEEN 960001 AND 960006;

UPDATE `item_template`
SET
  `spellid_1` = 89604,
  `spelltrigger_1` = 1,
  `spellcharges_1` = 0,
  `spellppmRate_1` = 0,
  `spellcooldown_1` = -1,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = -1,
  `spellid_2` = 89181,
  `spelltrigger_2` = 1,
  `spellcharges_2` = 0,
  `spellppmRate_2` = 0,
  `spellcooldown_2` = -1,
  `spellcategory_2` = 0,
  `spellcategorycooldown_2` = -1,
  `spellid_3` = 0,
  `spelltrigger_3` = 0,
  `spellcharges_3` = 0,
  `spellppmRate_3` = 0,
  `spellcooldown_3` = -1,
  `spellcategory_3` = 0,
  `spellcategorycooldown_3` = -1,
  `description` = ''
WHERE `entry` = 970001;

-- 遗物 / 神器：像普通装备一样附加 10 条属性，便于在物品 tooltip 中直接展示
-- 属性顺序：
-- 1敏捷 2力量 3智力 4精神 5耐力 6命中等级 7暴击等级 8急速等级 9攻击强度 10法术强度
-- 调整目标：章节遗物只做阶段强化，阶段神器明显高于遗物但不跨档，终极神器高于阶段神器但不压穿整条装备线
-- 展示目标：950001-970001 同样统一成“同一件物品内部 10 条属性显示同一个数”，避免 tooltip 数字参差不齐
UPDATE `item_template` it
JOIN `_深渊遗物配置` r ON r.`物品ID` = it.`entry`
SET
  it.`StatsCount` = 10,
  it.`stat_type1` = 3,
  it.`stat_value1` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type2` = 4,
  it.`stat_value2` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type3` = 5,
  it.`stat_value3` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type4` = 6,
  it.`stat_value4` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type5` = 7,
  it.`stat_value5` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type6` = 31,
  it.`stat_value6` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type7` = 32,
  it.`stat_value7` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type8` = 36,
  it.`stat_value8` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type9` = 38,
  it.`stat_value9` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  ))),
  it.`stat_type10` = 45,
  it.`stat_value10` = GREATEST(1, FLOOR((
    CASE r.`类型`
      WHEN 1 THEN (70 + r.`幕ID` * 12 + LEAST(GREATEST(r.`关联章节ID`, 1), 74)) * 0.09
      WHEN 2 THEN (118 + r.`幕ID` * 18) * 0.12
      WHEN 3 THEN (220 + r.`幕ID` * 24) * 0.14
      ELSE 7
    END
  )))
WHERE it.`entry` = r.`物品ID`
  AND it.`entry` BETWEEN 950001 AND 970001;

-- 深渊装备：唯一装备统一标记为史诗绑定，普通底材保留稀有/精良骨架
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`Quality` = CASE
    WHEN e.`装备类型` = 2 THEN 5
    ELSE GREATEST(it.`Quality`, 3)
  END,
  it.`ScriptName` = ''
WHERE it.`entry` = e.`物品模板ID`;

-- 深渊装备：补首版属性预算与基础护甲/武器伤害
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`StatsCount` = CASE
    WHEN FLOOR((e.`特效预算最小值` + e.`特效预算最大值`) / 2) > 0 THEN 3
    WHEN FLOOR((e.`次属性预算最小值` + e.`次属性预算最大值`) / 2) > 0 THEN 2
    WHEN FLOOR((e.`主属性预算最小值` + e.`主属性预算最大值`) / 2) > 0 THEN 1
    ELSE 0
  END,
  it.`stat_type1` = CASE
    WHEN e.`伤害类型` IN (2, 4) THEN 5
    WHEN e.`伤害类型` = 3 THEN 7
    WHEN e.`护甲类型` = 4 THEN 4
    WHEN e.`护甲类型` IN (2, 3) THEN 3
    WHEN e.`护甲类型` = 1 THEN 5
    ELSE 4
  END,
  it.`stat_value1` = FLOOR((e.`主属性预算最小值` + e.`主属性预算最大值`) / 2),
  it.`stat_type2` = CASE
    WHEN FLOOR((e.`次属性预算最小值` + e.`次属性预算最大值`) / 2) = 0 THEN 0
    WHEN e.`伤害类型` = 3 THEN 12
    WHEN e.`伤害类型` IN (2, 4) THEN 45
    ELSE 32
  END,
  it.`stat_value2` = FLOOR((e.`次属性预算最小值` + e.`次属性预算最大值`) / 2),
  it.`stat_type3` = CASE
    WHEN FLOOR((e.`特效预算最小值` + e.`特效预算最大值`) / 2) = 0 THEN 0
    WHEN e.`伤害类型` = 3 THEN 15
    WHEN e.`伤害类型` IN (2, 4) THEN 36
    ELSE 38
  END,
  it.`stat_value3` = FLOOR((e.`特效预算最小值` + e.`特效预算最大值`) / 2),
  it.`stat_type4` = 0,
  it.`stat_value4` = 0,
  it.`stat_type5` = 0,
  it.`stat_value5` = 0,
  it.`stat_type6` = 0,
  it.`stat_value6` = 0,
  it.`stat_type7` = 0,
  it.`stat_value7` = 0,
  it.`stat_type8` = 0,
  it.`stat_value8` = 0,
  it.`stat_type9` = 0,
  it.`stat_value9` = 0,
  it.`stat_type10` = 0,
  it.`stat_value10` = 0,
  it.`delay` = CASE
    WHEN it.`class` = 2 AND it.`InventoryType` IN (13, 15, 17, 21, 22, 26) THEN 2200
    ELSE it.`delay`
  END,
  it.`dmg_min1` = CASE
    WHEN it.`class` = 2 AND it.`InventoryType` IN (13, 15, 17, 21, 22, 26)
      THEN GREATEST(1, FLOOR(e.`基础装等` * 1.6))
    ELSE 0
  END,
  it.`dmg_max1` = CASE
    WHEN it.`class` = 2 AND it.`InventoryType` IN (13, 15, 17, 21, 22, 26)
      THEN GREATEST(2, FLOOR(e.`基础装等` * 2.4))
    ELSE 0
  END,
  it.`dmg_type1` = CASE
    WHEN it.`class` = 2 AND it.`InventoryType` IN (13, 15, 17, 21, 22, 26) THEN 0
    ELSE 0
  END,
  it.`armor` = CASE
    WHEN it.`class` = 4 AND it.`InventoryType` IN (1, 5, 6, 8, 16)
      THEN CASE e.`护甲类型`
        WHEN 1 THEN FLOOR(e.`基础装等` * 2.2)
        WHEN 2 THEN FLOOR(e.`基础装等` * 3.0)
        WHEN 3 THEN FLOOR(e.`基础装等` * 3.8)
        WHEN 4 THEN FLOOR(e.`基础装等` * 4.6)
        ELSE FLOOR(e.`基础装等` * 1.6)
      END
    ELSE 0
  END,
  it.`MaxDurability` = CASE
    WHEN it.`class` = 2 AND it.`InventoryType` IN (13, 15, 17, 21, 22, 26) THEN FLOOR(e.`基础装等` * 1.2)
    WHEN it.`class` = 4 AND it.`InventoryType` IN (1, 5, 6, 8, 16) THEN FLOOR(e.`基础装等` * 1.8)
    ELSE 0
  END
WHERE it.`entry` = e.`物品模板ID`;

-- 深渊底材：统一改为 10 条全职业属性，避免只出现力量/智力倾向
-- 属性顺序：
-- 1敏捷 2力量 3智力 4精神 5耐力 6命中等级 7暴击等级 8急速等级 9攻击强度 10法术强度
-- 档位目标：正传略高于官方，深渊/腐化/轮回逐档递增
-- 展示目标：同一件装备内部的 10 条属性统一写成同一个数字，避免 tooltip 看起来参差不齐
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`StatsCount` = CASE WHEN e.`装备类型` = 1 THEN 10 ELSE it.`StatsCount` END,
  it.`stat_type1` = CASE WHEN e.`装备类型` = 1 THEN 3 ELSE it.`stat_type1` END,
  it.`stat_value1` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value1` END,
  it.`stat_type2` = CASE WHEN e.`装备类型` = 1 THEN 4 ELSE it.`stat_type2` END,
  it.`stat_value2` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value2` END,
  it.`stat_type3` = CASE WHEN e.`装备类型` = 1 THEN 5 ELSE it.`stat_type3` END,
  it.`stat_value3` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value3` END,
  it.`stat_type4` = CASE WHEN e.`装备类型` = 1 THEN 6 ELSE it.`stat_type4` END,
  it.`stat_value4` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value4` END,
  it.`stat_type5` = CASE WHEN e.`装备类型` = 1 THEN 7 ELSE it.`stat_type5` END,
  it.`stat_value5` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value5` END,
  it.`stat_type6` = CASE WHEN e.`装备类型` = 1 THEN 31 ELSE it.`stat_type6` END,
  it.`stat_value6` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value6` END,
  it.`stat_type7` = CASE WHEN e.`装备类型` = 1 THEN 32 ELSE it.`stat_type7` END,
  it.`stat_value7` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value7` END,
  it.`stat_type8` = CASE WHEN e.`装备类型` = 1 THEN 36 ELSE it.`stat_type8` END,
  it.`stat_value8` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value8` END,
  it.`stat_type9` = CASE WHEN e.`装备类型` = 1 THEN 38 ELSE it.`stat_type9` END,
  it.`stat_value9` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value9` END,
  it.`stat_type10` = CASE WHEN e.`装备类型` = 1 THEN 45 ELSE it.`stat_type10` END,
  it.`stat_value10` = CASE WHEN e.`装备类型` = 1 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.10 * CASE e.`来源模式` WHEN 2 THEN 1.08 WHEN 3 THEN 1.16 WHEN 4 THEN 1.24 ELSE 1.00 END)) ELSE it.`stat_value10` END
WHERE it.`entry` = e.`物品模板ID`
  AND e.`装备类型` = 1;

-- 专属装备：重做为 10 条固定属性，并按来源层级成长
-- 展示目标同样改为“同一件装备内部属性值统一”，优先保证观感整齐
-- 属性顺序：
-- 1敏捷 2力量 3智力 4精神 5耐力 6命中等级 7暴击等级 8急速等级 9攻击强度 10法术强度
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`StatsCount` = CASE WHEN e.`装备类型` = 2 THEN 10 ELSE it.`StatsCount` END,
  it.`stat_type1` = CASE WHEN e.`装备类型` = 2 THEN 3 ELSE it.`stat_type1` END,
  it.`stat_value1` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value1` END,
  it.`stat_type2` = CASE WHEN e.`装备类型` = 2 THEN 4 ELSE it.`stat_type2` END,
  it.`stat_value2` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value2` END,
  it.`stat_type3` = CASE WHEN e.`装备类型` = 2 THEN 5 ELSE it.`stat_type3` END,
  it.`stat_value3` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value3` END,
  it.`stat_type4` = CASE WHEN e.`装备类型` = 2 THEN 6 ELSE it.`stat_type4` END,
  it.`stat_value4` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value4` END,
  it.`stat_type5` = CASE WHEN e.`装备类型` = 2 THEN 7 ELSE it.`stat_type5` END,
  it.`stat_value5` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value5` END,
  it.`stat_type6` = CASE WHEN e.`装备类型` = 2 THEN 31 ELSE it.`stat_type6` END,
  it.`stat_value6` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value6` END,
  it.`stat_type7` = CASE WHEN e.`装备类型` = 2 THEN 32 ELSE it.`stat_type7` END,
  it.`stat_value7` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value7` END,
  it.`stat_type8` = CASE WHEN e.`装备类型` = 2 THEN 36 ELSE it.`stat_type8` END,
  it.`stat_value8` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value8` END,
  it.`stat_type9` = CASE WHEN e.`装备类型` = 2 THEN 38 ELSE it.`stat_type9` END,
  it.`stat_value9` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value9` END,
  it.`stat_type10` = CASE WHEN e.`装备类型` = 2 THEN 45 ELSE it.`stat_type10` END,
  it.`stat_value10` = CASE WHEN e.`装备类型` = 2 THEN GREATEST(1, FLOOR(e.`基础装等` * 0.14 * CASE WHEN e.`是否来自秘藏首领` = 1 THEN 1.24 WHEN e.`来源模式` = 4 THEN 1.16 WHEN e.`来源模式` = 3 THEN 1.08 WHEN e.`来源模式` = 2 THEN 1.04 ELSE 1.00 END)) ELSE it.`stat_value10` END
WHERE it.`entry` = e.`物品模板ID`
  AND e.`装备类型` = 2;

-- ============================================
-- 专属装备：绑定WoW内置强力Spell特效（神器级）
-- spelltrigger=1 表示装备时生效（被动proc/光环）
-- 使用3.3.5已确认可用的proc类spell
-- ============================================

-- 火脉裂刃(武器/幕1): 黑色布鲁托尔吸血 - 近战攻击吸取2138-2362暗影伤害
UPDATE `item_template` SET
  `spellid_1` = 71880, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980001;

-- 狼王追猎披风(披风/幕1): 狂暴proc - 攻击/被攻击时触发400攻击强度10秒
UPDATE `item_template` SET
  `spellid_1` = 60066, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980002;

-- 梦沼禁印(戒指/幕1): 龙魂图鉴 - 施法叠加法术强度(最高200)
UPDATE `item_template` SET
  `spellid_1` = 60488, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980003;

-- 龙怒炽冠(头/幕1): 死神裁决 - 近战攻击触发765敏捷10秒
UPDATE `item_template` SET
  `spellid_1` = 67702, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980004;

-- 时痕逆表(饰品/幕2): 流放者日晷 - 施法触发590法术强度10秒
UPDATE `item_template` SET
  `spellid_1` = 60064, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980005;

-- 相位千机轮(法器/幕2): 天堂之耀 - 施法触发850法术强度
UPDATE `item_template` SET
  `spellid_1` = 64713, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980006;

-- 雷祖天罚锤(武器/幕3): 影之哀伤混沌之灾 - 近战叠10层后爆发大量暗影伤害
UPDATE `item_template` SET
  `spellid_1` = 71903, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980007;

-- 萨钢处刑者(武器/幕3): 死亡使者的意志 - 近战触发随机属性(力量/敏捷/急速)600+
UPDATE `item_template` SET
  `spellid_1` = 75473, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980008;

-- 古神耳语囚笼(饰品/幕4): 远古王者的祝福 - 治疗产生吸收护盾
UPDATE `item_template` SET
  `spellid_1` = 64411, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980009;

-- 影印王纹(戒指/幕5): 死神之择(敏捷版) - 近战触发510敏捷10秒
UPDATE `item_template` SET
  `spellid_1` = 67703, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980010;

-- 观察者抉择面甲(头/幕6): 死亡使者的意志 - 变形+随机属性600
UPDATE `item_template` SET
  `spellid_1` = 71519, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980011;

-- 霜王统御之核(饰品/幕6): 影之哀伤混沌之灾 - 叠层爆发暗影伤害(终极特效)
UPDATE `item_template` SET
  `spellid_1` = 71903, `spelltrigger_1` = 1, `spellcharges_1` = 0, `spellppmRate_1` = 0,
  `spellcooldown_1` = -1, `spellcategory_1` = 0, `spellcategorycooldown_1` = -1
WHERE `entry` = 980012;

-- 所有秘藏专属神器统一补首版被动触发骨架，避免只有少量样本才带被动
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`spellid_1` = CASE e.`部位掩码`
    WHEN 1 THEN 75473
    WHEN 512 THEN 64713
    WHEN 2 THEN 71519
    WHEN 4 THEN 71519
    WHEN 16 THEN 67702
    WHEN 32 THEN 67702
    WHEN 64 THEN 60488
    WHEN 128 THEN 64411
    WHEN 256 THEN 60066
    ELSE 60064
  END,
  it.`spelltrigger_1` = 1,
  it.`spellcharges_1` = 0,
  it.`spellppmRate_1` = 0,
  it.`spellcooldown_1` = -1,
  it.`spellcategory_1` = 0,
  it.`spellcategorycooldown_1` = -1
WHERE e.`装备类型` = 2
  AND it.`entry` BETWEEN 980001 AND 981628;

-- 专属神器武器/法轮：16 单独重导时也要把主动/触发技能重新挂回，避免 spellid_2 被模板重建覆盖
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`spellid_2` = CASE
    WHEN e.`固定特效ID` BETWEEN 40101 AND 40615 THEN 89401 + ((FLOOR((e.`固定特效ID` - 40000) / 100) - 1) * 15) + (MOD(e.`固定特效ID`, 100) - 1)
    ELSE it.`spellid_2`
  END,
  it.`spelltrigger_2` = CASE
    WHEN e.`固定特效ID` BETWEEN 40101 AND 40615 THEN 2
    ELSE it.`spelltrigger_2`
  END,
  it.`spellcharges_2` = CASE
    WHEN e.`固定特效ID` BETWEEN 40101 AND 40615 THEN 0
    ELSE it.`spellcharges_2`
  END,
  it.`spellppmRate_2` = CASE MOD(e.`固定特效ID`, 100)
    WHEN 1 THEN 2.8 WHEN 2 THEN 2.4 WHEN 3 THEN 2.4 WHEN 4 THEN 4.5 WHEN 5 THEN 3.4
    WHEN 6 THEN 2.0 WHEN 7 THEN 2.0 WHEN 8 THEN 1.6 WHEN 9 THEN 2.4 WHEN 10 THEN 2.1
    WHEN 11 THEN 2.8 WHEN 12 THEN 2.4 WHEN 13 THEN 2.2 WHEN 14 THEN 3.0 WHEN 15 THEN 2.6
    ELSE it.`spellppmRate_2`
  END,
  it.`spellcooldown_2` = CASE MOD(e.`固定特效ID`, 100)
    WHEN 1 THEN 2400 WHEN 2 THEN 3000 WHEN 3 THEN 3000 WHEN 4 THEN 1600 WHEN 5 THEN 2200
    WHEN 6 THEN 3400 WHEN 7 THEN 3400 WHEN 8 THEN 3800 WHEN 9 THEN 2600 WHEN 10 THEN 3200
    WHEN 11 THEN 2400 WHEN 12 THEN 2800 WHEN 13 THEN 3000 WHEN 14 THEN 2300 WHEN 15 THEN 2600
    ELSE it.`spellcooldown_2`
  END,
  it.`spellcategory_2` = CASE
    WHEN e.`固定特效ID` BETWEEN 40101 AND 40615 THEN 0
    ELSE it.`spellcategory_2`
  END,
  it.`spellcategorycooldown_2` = CASE
    WHEN e.`固定特效ID` BETWEEN 40101 AND 40615 THEN -1
    ELSE it.`spellcategorycooldown_2`
  END
WHERE e.`装备类型` = 2
  AND e.`是否启用` = 1
  AND (e.`部位掩码` = 1 OR e.`部位掩码` = 512)
  AND it.`entry` BETWEEN 980001 AND 981628;

-- 遗物 / 神器 / 专属装备名称：统一粉红色显示
UPDATE `item_template` it
JOIN `_深渊物品模板对接` d ON d.`物品ID` = it.`entry`
SET
  it.`name` = CONCAT('|cFFFF6699', d.`物品名称`, '|r')
WHERE (it.`entry` BETWEEN 950001 AND 950074)
   OR (it.`entry` BETWEEN 960001 AND 960006)
   OR it.`entry` = 970001;

UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`name` = CONCAT('|cFFFF6699', e.`装备名称`, '|r')
WHERE e.`装备类型` = 2
  AND it.`entry` BETWEEN 980001 AND 981628;

-- 深渊装备：品质直接提升为 5（橙色）
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`Quality` = 5,
  it.`name` = e.`装备名称`
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 2
  AND it.`entry` BETWEEN 990001 AND 993666;

-- 腐化装备：名称改为红色
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`name` = CONCAT('|cFFFF0000', e.`装备名称`, '|r')
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 3
  AND it.`entry` BETWEEN 990001 AND 993666;

-- 轮回装备：名称改为浅红色
UPDATE `item_template` it
JOIN `_深渊装备模板` e ON e.`物品模板ID` = it.`entry`
SET
  it.`name` = CONCAT('|cFFFF9999', e.`装备名称`, '|r')
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 4
  AND it.`entry` BETWEEN 990001 AND 993666;

-- 深渊物品：首版一致性修补，避免非装备类模板残留无意义战斗字段
UPDATE `item_template`
SET
  `delay` = 0,
  `dmg_min1` = 0,
  `dmg_max1` = 0,
  `dmg_type1` = 0,
  `armor` = 0,
  `MaxDurability` = 0
WHERE (`entry` BETWEEN 950001 AND 970001)
   OR (`class` = 15);

-- 饰品 / 戒指 / 披风 / 法器不参与护甲耐久计算
UPDATE `item_template`
SET
  `armor` = 0,
  `MaxDurability` = 0
WHERE (`entry` BETWEEN 980001 AND 993666)
  AND `InventoryType` IN (11, 12, 16, 23);

-- 武器类模板统一补可见持握样式，装备类统一补基础材质
UPDATE `item_template`
SET
  `sheath` = CASE
    WHEN `class` = 2 THEN 3
    WHEN `InventoryType` = 23 THEN 7
    WHEN `InventoryType` = 16 THEN 1
    ELSE `sheath`
  END,
  `Material` = CASE
    WHEN `class` = 2 THEN 1
    WHEN `class` = 4 THEN 4
    ELSE `Material`
  END
WHERE (`entry` BETWEEN 980001 AND 993666)
   OR (`entry` BETWEEN 950001 AND 970001);

-- 自定义套装部件：同步镜像到 item_set_names，避免加载时回退到 item_template 并刷错误
DELETE FROM `item_set_names`
WHERE `entry` BETWEEN 980001 AND 993666
  AND `entry` NOT IN (
    SELECT `entry`
    FROM `item_template`
    WHERE `itemset` <> 0
      AND `entry` BETWEEN 980001 AND 993666
  );

REPLACE INTO `item_set_names`
(`entry`, `name`, `InventoryType`, `VerifiedBuild`)
SELECT
  `entry`,
  `name`,
  `InventoryType`,
  -1
FROM `item_template`
WHERE `itemset` <> 0
  AND `entry` BETWEEN 980001 AND 993666;

UPDATE `_深渊物品模板对接`
SET `对接状态` = 1
WHERE (`物品ID` BETWEEN 950001 AND 993666)
   OR (`物品ID` BETWEEN 960001 AND 960006)
   OR `物品ID` = 970001;

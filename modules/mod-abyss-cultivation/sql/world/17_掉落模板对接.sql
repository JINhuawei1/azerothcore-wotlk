-- ============================================
-- 掉落模板对接：改回 AzerothCore 原生 creature_loot_template / reference_loot_template
-- 说明：
-- 1. 官方直打首领保持原版 loot mode；显式触发的正传 / 深渊 / 腐化 / 轮回首领统一走原生掉落表
-- 2. 自定义深渊首领 / 秘藏首领使用自身 entry 作为 lootid
-- 3. 深渊模式 LootMode 位：2=正传 4=深渊 8=腐化 16=轮回
-- 4. 秘藏首领专属神器统一通过 creature_loot_template 以 25% 概率掉落
-- ============================================

DELETE FROM `creature_loot_template`
WHERE (`Entry` BETWEEN 910001 AND 910074)
   OR (`Entry` BETWEEN 919001 AND 919074)
   OR (`Item` BETWEEN 191001 AND 195074)
   OR (`Reference` BETWEEN 191001 AND 195074);

DELETE FROM `reference_loot_template`
WHERE (`Entry` BETWEEN 191001 AND 195074);

-- 自定义深渊 / 秘藏首领统一使用自身 entry 作为 lootid
UPDATE `creature_template`
SET `lootid` = `entry`
WHERE ((`entry` BETWEEN 910001 AND 910074) OR (`entry` BETWEEN 919001 AND 919074))
  AND `lootid` <> `entry`;

-- 章节模式底材池：补齐正传 / 深渊 / 腐化 / 轮回 / 秘藏五类引用池
INSERT INTO `reference_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  191000 + e.`来源章节`,
  e.`物品模板ID`,
  0,
  0,
  0,
  2,
  1,
  1,
  1,
  CONCAT('第', LPAD(e.`来源章节`, 2, '0'), '章正传底材池 - ', e.`装备名称`)
FROM `_深渊装备模板` e
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 1
  AND e.`是否启用` = 1

UNION ALL

SELECT
  192000 + e.`来源章节`,
  e.`物品模板ID`,
  0,
  0,
  0,
  4,
  1,
  1,
  1,
  CONCAT('第', LPAD(e.`来源章节`, 2, '0'), '章深渊底材池 - ', e.`装备名称`)
FROM `_深渊装备模板` e
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 2
  AND e.`是否启用` = 1

UNION ALL

SELECT
  193000 + e.`来源章节`,
  e.`物品模板ID`,
  0,
  0,
  0,
  8,
  1,
  1,
  1,
  CONCAT('第', LPAD(e.`来源章节`, 2, '0'), '章腐化底材池 - ', e.`装备名称`)
FROM `_深渊装备模板` e
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 3
  AND e.`是否启用` = 1

UNION ALL

SELECT
  194000 + e.`来源章节`,
  e.`物品模板ID`,
  0,
  0,
  0,
  16,
  1,
  1,
  1,
  CONCAT('第', LPAD(e.`来源章节`, 2, '0'), '章轮回底材池 - ', e.`装备名称`)
FROM `_深渊装备模板` e
WHERE e.`装备类型` = 1
  AND e.`来源模式` = 4
  AND e.`是否启用` = 1

UNION ALL

SELECT
  195000 + e.`来源章节`,
  e.`物品模板ID`,
  0,
  0,
  0,
  30,
  1,
  1,
  1,
  CONCAT('第', LPAD(e.`来源章节`, 2, '0'), '章秘藏神器池 - ', e.`装备名称`)
FROM `_深渊装备模板` e
WHERE e.`装备类型` = 2
  AND e.`是否来自秘藏首领` = 1
  AND e.`是否启用` = 1;

-- 模式首领：在 LootMode=2 时掉落正传底材池，统一由 910xxx 自定义首领承载
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  COALESCE(NULLIF(ct.`lootid`, 0), ct.`entry`) AS `Entry`,
  0 AS `Item`,
  191000 + c.`章节ID` AS `Reference`,
  100.0 AS `Chance`,
  0,
  2,
  0,
  1,
  1,
  CONCAT('第', LPAD(c.`章节ID`, 2, '0'), '章模式首领-正传底材引用池')
FROM `_深渊章节配置` c
JOIN `creature_template` ct ON ct.`entry` = c.`深渊首领入口`
WHERE c.`深渊首领入口` <> 0;

-- 自定义深渊首领：只挂深渊 / 腐化 / 轮回三档底材池
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  COALESCE(NULLIF(ct.`lootid`, 0), ct.`entry`) AS `Entry`,
  0 AS `Item`,
  m.`引用池基数` + c.`章节ID` AS `Reference`,
  100.0 AS `Chance`,
  0,
  m.`LootMode`,
  0,
  1,
  1,
  CONCAT('第', LPAD(c.`章节ID`, 2, '0'), '章深渊首领-', m.`模式名称`, '底材引用池')
FROM `_深渊章节配置` c
JOIN `creature_template` ct ON ct.`entry` = c.`深渊首领入口`
JOIN (
  SELECT 2 AS `模式类型`, 192000 AS `引用池基数`, 4 AS `LootMode`, '深渊' AS `模式名称`
  UNION ALL
  SELECT 3 AS `模式类型`, 193000 AS `引用池基数`, 8 AS `LootMode`, '腐化' AS `模式名称`
  UNION ALL
  SELECT 4 AS `模式类型`, 194000 AS `引用池基数`, 16 AS `LootMode`, '轮回' AS `模式名称`
) m
WHERE c.`深渊首领入口` <> 0;

-- 秘藏首领：四模式统一 25% 掉落当前章节完整秘藏池
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
  COALESCE(NULLIF(ct.`lootid`, 0), ct.`entry`) AS `Entry`,
  0 AS `Item`,
  195000 + c.`章节ID` AS `Reference`,
  25.0 AS `Chance`,
  0,
  30,
  0,
  1,
  1,
  CONCAT('第', LPAD(c.`章节ID`, 2, '0'), '章秘藏首领-全模式专属神器引用池')
FROM `_深渊章节配置` c
JOIN `creature_template` ct ON ct.`entry` = c.`秘藏首领入口`
WHERE c.`秘藏首领入口` <> 0;

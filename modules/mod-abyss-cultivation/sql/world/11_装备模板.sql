-- ============================================
-- 装备模板：按章节生成四模式底材 + 按章节生成秘藏专属神器
-- 说明：
-- 1. 每章生成 9 件底材；四模式沿用 990001-993666 区间
-- 2. 秘藏专属池改为：每章 14 类武器 + 8 件套装，共 22 件，使用 980001-981628
-- 3. 这样秘藏首领不再只掉单手剑，而是能覆盖所有职业常用武器与完整套装位
-- ============================================

DELETE FROM `_深渊装备模板`
WHERE (`物品模板ID` BETWEEN 980001 AND 981628)
   OR (`物品模板ID` BETWEEN 990001 AND 993666);

-- ============================================
-- 装备模板：每章专属秘藏神器
-- 设计：每章生成完整秘藏池 = 14 类武器 + 8 件套装位
-- 备注：14 类秘藏武器继续沿用 4=轮回档分类；8 件秘藏套装位独立切到 5=神器套装，
--       实际终档强度在物品模板对接阶段统一拉到“秘藏 > 轮回”
-- ============================================

INSERT INTO `_深渊装备模板`
(`物品模板ID`, `装备名称`, `装备类型`, `来源章节`, `来源模式`, `部位掩码`, `幕ID`, `基础装等`, `护甲类型`, `伤害类型`, `主属性预算最小值`, `主属性预算最大值`, `次属性预算最小值`, `次属性预算最大值`, `特效预算最小值`, `特效预算最大值`, `固定词缀组`, `固定特效ID`, `是否来自秘藏首领`, `是否需要碎片`, `风味文本`, `套装ID`, `是否启用`)
SELECT
  980000 + ((g.`chapter_id` - 1) * 22) + g.`entry_index`,
  CONCAT('神器-', g.`chapter_name`, '·', g.`artifact_name`),
  2,
  g.`chapter_id`,
  CASE WHEN g.`slot_mask` IN (512, 2, 4, 16, 32, 64, 128, 256) THEN 5 ELSE 4 END,
  g.`slot_mask`,
  g.`act_id`,
  g.`base_item_level`,
  g.`armor_type`,
  g.`damage_type`,
  FLOOR(g.`base_item_level` * g.`primary_min_rate` * 1.10),
  FLOOR(g.`base_item_level` * g.`primary_max_rate` * 1.14),
  FLOOR(g.`base_item_level` * g.`secondary_min_rate` * 1.16),
  FLOOR(g.`base_item_level` * g.`secondary_max_rate` * 1.20),
  FLOOR(g.`base_item_level` * g.`effect_min_rate` * 1.28),
  FLOOR(g.`base_item_level` * g.`effect_max_rate` * 1.36),
  CASE
    WHEN g.`slot_mask` = 1 AND g.`damage_type` IN (13, 17) THEN '观测,双生'
    WHEN g.`slot_mask` = 1 AND g.`damage_type` IN (14, 15, 16) THEN '追命,断空'
    WHEN g.`slot_mask` = 1 THEN '狂怒,裁决'
    WHEN g.`slot_mask` = 512 THEN '观测,双生'
    WHEN g.`slot_mask` = 2 THEN '终临,观星'
    WHEN g.`slot_mask` = 4 THEN '裂甲,终临'
    WHEN g.`slot_mask` IN (16, 32) THEN '裂甲,断空'
    WHEN g.`slot_mask` = 64 THEN '渎神,观测'
    WHEN g.`slot_mask` = 128 THEN '观测,终临'
    WHEN g.`slot_mask` = 256 THEN '追命,断空'
    ELSE ''
  END,
  g.`fixed_effect_id`,
  1,
  0,
  CASE
    WHEN g.`slot_mask` IN (512, 2, 4, 16, 32, 64, 128, 256) THEN CONCAT('【', g.`chapter_name`, '】秘藏首领遗落的神器套装部件。')
    ELSE CONCAT('【', g.`chapter_name`, '】秘藏首领遗落的专属神器。')
  END,
  CASE
    WHEN g.`slot_mask` IN (512, 2, 4, 16, 32, 64, 128, 256) THEN g.`chapter_id` * 10 + 5
    ELSE 0
  END,
  1
FROM (
  SELECT
    c.`章节ID` AS `chapter_id`,
    c.`章节名称` AS `chapter_name`,
    c.`幕ID` AS `act_id`,
    s.`序号` AS `entry_index`,
    s.`部位掩码` AS `slot_mask`,
    s.`神器名称` AS `artifact_name`,
    s.`护甲类型` AS `armor_type`,
    s.`伤害类型` AS `damage_type`,
    s.`主属性最小系数` AS `primary_min_rate`,
    s.`主属性最大系数` AS `primary_max_rate`,
    s.`次属性最小系数` AS `secondary_min_rate`,
    s.`次属性最大系数` AS `secondary_max_rate`,
    s.`特效最小系数` AS `effect_min_rate`,
    s.`特效最大系数` AS `effect_max_rate`,
    CASE
      WHEN s.`序号` BETWEEN 1 AND 15 THEN 40000 + c.`幕ID` * 100 + s.`序号`
      ELSE 0
    END AS `fixed_effect_id`,
    CASE
      WHEN c.`章节ID` BETWEEN 1 AND 20 THEN 88 + FLOOR((c.`章节ID` - 1) * 24 / 19)
      WHEN c.`章节ID` BETWEEN 21 AND 36 THEN 122 + FLOOR((c.`章节ID` - 21) * 26 / 15)
      WHEN c.`章节ID` BETWEEN 37 AND 52 THEN 213 + FLOOR((c.`章节ID` - 37) * 32 / 15)
      WHEN c.`章节ID` BETWEEN 53 AND 57 THEN 258 + FLOOR((c.`章节ID` - 53) * 12 / 4)
      WHEN c.`章节ID` BETWEEN 58 AND 66 THEN 278 + FLOOR((c.`章节ID` - 58) * 18 / 8)
      ELSE 302 + FLOOR((c.`章节ID` - 67) * 18 / 7)
    END AS `base_item_level`
  FROM `_深渊章节配置` c
  CROSS JOIN (
    SELECT 1 AS `序号`, 1 AS `部位掩码`, '秘藏剑锋' AS `神器名称`, 0 AS `护甲类型`, 1 AS `伤害类型`, 0.45 AS `主属性最小系数`, 0.60 AS `主属性最大系数`, 0.08 AS `次属性最小系数`, 0.12 AS `次属性最大系数`, 0.14 AS `特效最小系数`, 0.20 AS `特效最大系数`
    UNION ALL
    SELECT 2, 1, '秘藏战斧', 0, 5, 0.45, 0.60, 0.08, 0.12, 0.14, 0.20
    UNION ALL
    SELECT 3, 1, '秘藏战锤', 0, 6, 0.45, 0.60, 0.08, 0.12, 0.14, 0.20
    UNION ALL
    SELECT 4, 1, '秘藏影匕', 0, 7, 0.42, 0.56, 0.10, 0.14, 0.15, 0.21
    UNION ALL
    SELECT 5, 1, '秘藏拳锋', 0, 8, 0.44, 0.58, 0.09, 0.13, 0.15, 0.21
    UNION ALL
    SELECT 6, 1, '秘藏巨刃', 0, 9, 0.52, 0.68, 0.08, 0.12, 0.16, 0.22
    UNION ALL
    SELECT 7, 1, '秘藏巨斧', 0, 10, 0.52, 0.68, 0.08, 0.12, 0.16, 0.22
    UNION ALL
    SELECT 8, 1, '秘藏巨锤', 0, 11, 0.52, 0.68, 0.08, 0.12, 0.16, 0.22
    UNION ALL
    SELECT 9, 1, '秘藏长戟', 0, 12, 0.50, 0.66, 0.08, 0.12, 0.16, 0.22
    UNION ALL
    SELECT 10, 1, '秘藏法杖', 0, 13, 0.60, 0.76, 0.08, 0.12, 0.16, 0.23
    UNION ALL
    SELECT 11, 1, '秘藏战弓', 0, 14, 0.46, 0.62, 0.09, 0.13, 0.15, 0.21
    UNION ALL
    SELECT 12, 1, '秘藏火枪', 0, 15, 0.46, 0.62, 0.09, 0.13, 0.15, 0.21
    UNION ALL
    SELECT 13, 1, '秘藏机弩', 0, 16, 0.46, 0.62, 0.09, 0.13, 0.15, 0.21
    UNION ALL
    SELECT 14, 1, '秘藏魔杖', 0, 17, 0.58, 0.72, 0.10, 0.14, 0.16, 0.22
    UNION ALL
    SELECT 15, 512, '秘藏法轮', 0, 2, 0.60, 0.76, 0.08, 0.12, 0.16, 0.23
    UNION ALL
    SELECT 16, 2, '秘藏头冠', 0, 0, 0.08, 0.12, 0.14, 0.20, 0.06, 0.10
    UNION ALL
    SELECT 17, 4, '秘藏胸铠', 0, 0, 0.11, 0.16, 0.18, 0.26, 0.08, 0.14
    UNION ALL
    SELECT 18, 16, '秘藏战带', 0, 0, 0.06, 0.10, 0.10, 0.15, 0.08, 0.14
    UNION ALL
    SELECT 19, 32, '秘藏战靴', 0, 0, 0.06, 0.10, 0.10, 0.15, 0.08, 0.14
    UNION ALL
    SELECT 20, 64, '秘藏指环', 0, 2, 0.05, 0.08, 0.06, 0.10, 0.12, 0.18
    UNION ALL
    SELECT 21, 128, '秘藏魂坠', 0, 2, 0.02, 0.06, 0.00, 0.00, 0.14, 0.22
    UNION ALL
    SELECT 22, 256, '秘藏披影', 0, 0, 0.06, 0.10, 0.10, 0.15, 0.09, 0.15
  ) s
) g;

-- ============================================
-- 装备模板：正传模式底材
-- 设计：74 章 * 9 个部位，每章都有独立底材，不再整幕共用
-- ============================================

INSERT INTO `_深渊装备模板`
(`物品模板ID`, `装备名称`, `装备类型`, `来源章节`, `来源模式`, `部位掩码`, `幕ID`, `基础装等`, `护甲类型`, `伤害类型`, `主属性预算最小值`, `主属性预算最大值`, `次属性预算最小值`, `次属性预算最大值`, `特效预算最小值`, `特效预算最大值`, `固定词缀组`, `固定特效ID`, `是否来自秘藏首领`, `是否需要碎片`, `风味文本`, `套装ID`, `是否启用`)
SELECT
  990000 + ((g.`chapter_id` - 1) * 9) + g.`slot_index`,
  CONCAT('正传-', g.`chapter_name`, '·', g.`base_name`),
  1,
  g.`chapter_id`,
  1,
  g.`slot_mask`,
  g.`act_id`,
  g.`base_item_level`,
  g.`armor_type`,
  g.`damage_type`,
  FLOOR(g.`base_item_level` * g.`primary_min_rate`),
  FLOOR(g.`base_item_level` * g.`primary_max_rate`),
  FLOOR(g.`base_item_level` * g.`secondary_min_rate`),
  FLOOR(g.`base_item_level` * g.`secondary_max_rate`),
  FLOOR(g.`base_item_level` * g.`effect_min_rate`),
  FLOOR(g.`base_item_level` * g.`effect_max_rate`),
  '',
  0,
  0,
  0,
  CONCAT('【', g.`chapter_name`, '】正传模式', g.`equipment_label`, '底材。'),
  g.`chapter_id` * 10 + 1,
  1
FROM (
  SELECT
    c.`章节ID` AS `chapter_id`,
    c.`章节名称` AS `chapter_name`,
    c.`幕ID` AS `act_id`,
    s.`槽位序号` AS `slot_index`,
    s.`部位掩码` AS `slot_mask`,
    s.`底材名称` AS `base_name`,
    s.`装备标签` AS `equipment_label`,
    s.`护甲类型` AS `armor_type`,
    s.`伤害类型` AS `damage_type`,
    s.`主属性最小系数` AS `primary_min_rate`,
    s.`主属性最大系数` AS `primary_max_rate`,
    s.`次属性最小系数` AS `secondary_min_rate`,
    s.`次属性最大系数` AS `secondary_max_rate`,
    s.`特效最小系数` AS `effect_min_rate`,
    s.`特效最大系数` AS `effect_max_rate`,
    CASE
      WHEN c.`章节ID` BETWEEN 1 AND 20 THEN 72 + FLOOR((c.`章节ID` - 1) * 24 / 19)
      WHEN c.`章节ID` BETWEEN 21 AND 36 THEN 108 + FLOOR((c.`章节ID` - 21) * 24 / 15)
      WHEN c.`章节ID` BETWEEN 37 AND 52 THEN 195 + FLOOR((c.`章节ID` - 37) * 37 / 15)
      WHEN c.`章节ID` BETWEEN 53 AND 57 THEN 245 + FLOOR((c.`章节ID` - 53) * 12 / 4)
      WHEN c.`章节ID` BETWEEN 58 AND 66 THEN 264 + FLOOR((c.`章节ID` - 58) * 18 / 8)
      ELSE 288 + FLOOR((c.`章节ID` - 67) * 20 / 7)
    END AS `base_item_level`
  FROM `_深渊章节配置` c
  CROSS JOIN (
    SELECT 1 AS `槽位序号`, 1 AS `部位掩码`, '断刃胚' AS `底材名称`, '秘藏断刃' AS `神器名称`, '近战武器' AS `装备标签`, 0 AS `护甲类型`, 1 AS `伤害类型`, 0.45 AS `主属性最小系数`, 0.60 AS `主属性最大系数`, 0.08 AS `次属性最小系数`, 0.12 AS `次属性最大系数`, 0.14 AS `特效最小系数`, 0.20 AS `特效最大系数`
    UNION ALL
    SELECT 2 AS `槽位序号`, 512 AS `部位掩码`, '法轮胚' AS `底材名称`, '秘藏法轮' AS `神器名称`, '法器' AS `装备标签`, 0 AS `护甲类型`, 2 AS `伤害类型`, 0.60 AS `主属性最小系数`, 0.76 AS `主属性最大系数`, 0.08 AS `次属性最小系数`, 0.12 AS `次属性最大系数`, 0.16 AS `特效最小系数`, 0.23 AS `特效最大系数`
    UNION ALL
    SELECT 3 AS `槽位序号`, 2 AS `部位掩码`, '头冠胚' AS `底材名称`, '秘藏头冠' AS `神器名称`, '头部' AS `装备标签`, 0 AS `护甲类型`, 0 AS `伤害类型`, 0.08 AS `主属性最小系数`, 0.12 AS `主属性最大系数`, 0.14 AS `次属性最小系数`, 0.20 AS `次属性最大系数`, 0.06 AS `特效最小系数`, 0.10 AS `特效最大系数`
    UNION ALL
    SELECT 4 AS `槽位序号`, 4 AS `部位掩码`, '胸铠胚' AS `底材名称`, '秘藏胸铠' AS `神器名称`, '胸甲' AS `装备标签`, 0 AS `护甲类型`, 0 AS `伤害类型`, 0.11 AS `主属性最小系数`, 0.16 AS `主属性最大系数`, 0.18 AS `次属性最小系数`, 0.26 AS `次属性最大系数`, 0.08 AS `特效最小系数`, 0.14 AS `特效最大系数`
    UNION ALL
    SELECT 5 AS `槽位序号`, 16 AS `部位掩码`, '战带胚' AS `底材名称`, '秘藏战带' AS `神器名称`, '腰带' AS `装备标签`, 0 AS `护甲类型`, 0 AS `伤害类型`, 0.06 AS `主属性最小系数`, 0.10 AS `主属性最大系数`, 0.10 AS `次属性最小系数`, 0.15 AS `次属性最大系数`, 0.08 AS `特效最小系数`, 0.14 AS `特效最大系数`
    UNION ALL
    SELECT 6 AS `槽位序号`, 32 AS `部位掩码`, '战靴胚' AS `底材名称`, '秘藏战靴' AS `神器名称`, '靴子' AS `装备标签`, 0 AS `护甲类型`, 0 AS `伤害类型`, 0.06 AS `主属性最小系数`, 0.10 AS `主属性最大系数`, 0.10 AS `次属性最小系数`, 0.15 AS `次属性最大系数`, 0.08 AS `特效最小系数`, 0.14 AS `特效最大系数`
    UNION ALL
    SELECT 7 AS `槽位序号`, 64 AS `部位掩码`, '指环胚' AS `底材名称`, '秘藏指环' AS `神器名称`, '戒指' AS `装备标签`, 0 AS `护甲类型`, 2 AS `伤害类型`, 0.05 AS `主属性最小系数`, 0.08 AS `主属性最大系数`, 0.06 AS `次属性最小系数`, 0.10 AS `次属性最大系数`, 0.12 AS `特效最小系数`, 0.18 AS `特效最大系数`
    UNION ALL
    SELECT 8 AS `槽位序号`, 128 AS `部位掩码`, '魂坠胚' AS `底材名称`, '秘藏魂坠' AS `神器名称`, '饰品' AS `装备标签`, 0 AS `护甲类型`, 2 AS `伤害类型`, 0.02 AS `主属性最小系数`, 0.06 AS `主属性最大系数`, 0.00 AS `次属性最小系数`, 0.00 AS `次属性最大系数`, 0.14 AS `特效最小系数`, 0.22 AS `特效最大系数`
    UNION ALL
    SELECT 9 AS `槽位序号`, 256 AS `部位掩码`, '披影胚' AS `底材名称`, '秘藏披影' AS `神器名称`, '披风' AS `装备标签`, 0 AS `护甲类型`, 0 AS `伤害类型`, 0.06 AS `主属性最小系数`, 0.10 AS `主属性最大系数`, 0.10 AS `次属性最小系数`, 0.15 AS `次属性最大系数`, 0.09 AS `特效最小系数`, 0.15 AS `特效最大系数`
  ) s
) g;

-- ============================================
-- 装备模板：深渊模式底材
-- ============================================

INSERT INTO `_深渊装备模板`
(`物品模板ID`, `装备名称`, `装备类型`, `来源章节`, `来源模式`, `部位掩码`, `幕ID`, `基础装等`, `护甲类型`, `伤害类型`, `主属性预算最小值`, `主属性预算最大值`, `次属性预算最小值`, `次属性预算最大值`, `特效预算最小值`, `特效预算最大值`, `固定词缀组`, `固定特效ID`, `是否来自秘藏首领`, `是否需要碎片`, `风味文本`, `套装ID`, `是否启用`)
SELECT
  `物品模板ID` + 3000,
  REPLACE(`装备名称`, '正传-', '深渊-'),
  1,
  `来源章节`,
  2,
  `部位掩码`,
  `幕ID`,
  `基础装等` + 4,
  `护甲类型`,
  `伤害类型`,
  FLOOR(`主属性预算最小值` * 1.05),
  FLOOR(`主属性预算最大值` * 1.05),
  FLOOR(`次属性预算最小值` * 1.07),
  FLOOR(`次属性预算最大值` * 1.07),
  FLOOR(`特效预算最小值` * 1.10),
  FLOOR(`特效预算最大值` * 1.10),
  '',
  0,
  0,
  0,
  REPLACE(`风味文本`, '正传模式', '深渊模式'),
  `来源章节` * 10 + 2,
  `是否启用`
FROM `_深渊装备模板`
WHERE `物品模板ID` BETWEEN 990001 AND 990666;

-- ============================================
-- 装备模板：腐化模式底材
-- ============================================

INSERT INTO `_深渊装备模板`
(`物品模板ID`, `装备名称`, `装备类型`, `来源章节`, `来源模式`, `部位掩码`, `幕ID`, `基础装等`, `护甲类型`, `伤害类型`, `主属性预算最小值`, `主属性预算最大值`, `次属性预算最小值`, `次属性预算最大值`, `特效预算最小值`, `特效预算最大值`, `固定词缀组`, `固定特效ID`, `是否来自秘藏首领`, `是否需要碎片`, `风味文本`, `套装ID`, `是否启用`)
SELECT
  `物品模板ID` + 1000,
  REPLACE(`装备名称`, '正传-', '腐化-'),
  1,
  `来源章节`,
  3,
  `部位掩码`,
  `幕ID`,
  `基础装等` + 9,
  `护甲类型`,
  `伤害类型`,
  FLOOR(`主属性预算最小值` * 1.11),
  FLOOR(`主属性预算最大值` * 1.11),
  FLOOR(`次属性预算最小值` * 1.14),
  FLOOR(`次属性预算最大值` * 1.14),
  FLOOR(`特效预算最小值` * 1.18),
  FLOOR(`特效预算最大值` * 1.18),
  '',
  0,
  0,
  0,
  REPLACE(`风味文本`, '正传模式', '腐化模式'),
  `来源章节` * 10 + 3,
  `是否启用`
FROM `_深渊装备模板`
WHERE `物品模板ID` BETWEEN 990001 AND 990666;

-- ============================================
-- 装备模板：轮回模式底材
-- ============================================

INSERT INTO `_深渊装备模板`
(`物品模板ID`, `装备名称`, `装备类型`, `来源章节`, `来源模式`, `部位掩码`, `幕ID`, `基础装等`, `护甲类型`, `伤害类型`, `主属性预算最小值`, `主属性预算最大值`, `次属性预算最小值`, `次属性预算最大值`, `特效预算最小值`, `特效预算最大值`, `固定词缀组`, `固定特效ID`, `是否来自秘藏首领`, `是否需要碎片`, `风味文本`, `套装ID`, `是否启用`)
SELECT
  `物品模板ID` + 2000,
  REPLACE(`装备名称`, '正传-', '轮回-'),
  1,
  `来源章节`,
  4,
  `部位掩码`,
  `幕ID`,
  `基础装等` + 16,
  `护甲类型`,
  `伤害类型`,
  FLOOR(`主属性预算最小值` * 1.18),
  FLOOR(`主属性预算最大值` * 1.18),
  FLOOR(`次属性预算最小值` * 1.22),
  FLOOR(`次属性预算最大值` * 1.22),
  FLOOR(`特效预算最小值` * 1.28),
  FLOOR(`特效预算最大值` * 1.28),
  '',
  0,
  0,
  0,
  REPLACE(`风味文本`, '正传模式', '轮回模式'),
  `来源章节` * 10 + 4,
  `是否启用`
FROM `_深渊装备模板`
WHERE `物品模板ID` BETWEEN 990001 AND 990666;

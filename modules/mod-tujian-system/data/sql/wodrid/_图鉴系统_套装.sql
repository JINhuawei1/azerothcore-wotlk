DROP TABLE IF EXISTS `_图鉴系统_套装`;
CREATE TABLE `_图鉴系统_套装`  (
  `注释` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `id` int UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '主键ID',
  `组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '同一组的物品，为套装组',
  `需要激活数量` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0表示整组全部激活才生效；大于0表示达到指定件数生效，例如2/4/6/8',
  `激活描述` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '客户端，羁绊效果显示',
  `激活模版_物品技能_多个逗号隔开` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '如果不写激活描述，显示物品技能描述',
  `激活后执行GM命令` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '激活套装后执行GM命令',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `idx_组` (`组`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of _图鉴系统_套装
-- ----------------------------

-- ============================================
-- 图鉴套装独立方案：
-- 1. 不再复用装备原生 ItemSet 套装效果
-- 2. 统一只保留 8 件激活档
-- 3. 效果按章节与模式递增：
--    怒焰裂谷：正传+5 / 深渊+10 / 腐化+15 / 轮回+20 / 神器+25
--    哀嚎洞穴：正传+10 / 深渊+15 / 腐化+20 / 轮回+25 / 神器+30
-- 4. 每章递增 5 点，固定增加 耐力/智力/精神/敏捷/力量/攻强/法强/急速
-- 5. 通过 _物品技能_模板 + 自定义 Spell.dbc 技能承载实际效果
-- ============================================

DELETE FROM `_物品技能_模板`
WHERE (`id` BETWEEN 960001 AND 960370)
   OR (`id` BETWEEN 961001 AND 961370);

INSERT INTO `_物品技能_模板`
(`id`, `注释`, `客户端显示`, `组`, `等级`, `技能id`, `几率`, `技能触发类型`, `触发几率`, `技能使用次数`,
 `技能每分钟触发次数`, `技能冷却时间`, `技能冷却值`, `技能消耗`, `技能施法`, `技能强化`, `技能伤害`,
 `技能类型`, `技能持续时间`, `启用状态`, `优先级`)
SELECT
  960000 + ((grp.`章节ID` - 1) * 5) + grp.`来源模式` AS `id`,
  CONCAT(
    '图鉴套装·',
    grp.`分类名称`, '·',
    '第', LPAD(grp.`章节ID`, 2, '0'), '章·', grp.`章节名称`,
    '·8件效果'
  ) AS `注释`,
  CONCAT(
    '8件：耐力+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，智力+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，精神+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，敏捷+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，力量+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5)
  ) AS `客户端显示`,
  960000 + grp.`来源模式` AS `组`,
  grp.`章节ID` AS `等级`,
  960000 + ((grp.`章节ID` - 1) * 5) + grp.`来源模式` AS `技能id`,
  100 AS `几率`,
  1 AS `技能触发类型`,
  100 AS `触发几率`,
  0 AS `技能使用次数`,
  0 AS `技能每分钟触发次数`,
  0 AS `技能冷却时间`,
  0 AS `技能冷却值`,
  0 AS `技能消耗`,
  0 AS `技能施法`,
  0 AS `技能强化`,
  0 AS `技能伤害`,
  0 AS `技能类型`,
  0 AS `技能持续时间`,
  1 AS `启用状态`,
  100 AS `优先级`
FROM (
  SELECT DISTINCT
    e.`套装ID` AS `图鉴套装组`,
    e.`来源章节` AS `章节ID`,
    c.`章节名称`,
    c.`幕ID`,
    e.`来源模式`,
    CASE
      WHEN e.`来源模式` = 1 THEN '正传'
      WHEN e.`来源模式` = 2 THEN '深渊'
      WHEN e.`来源模式` = 3 THEN '腐化'
      WHEN e.`来源模式` = 4 THEN '轮回'
      WHEN e.`来源模式` = 5 THEN '神器'
      ELSE '未知'
    END AS `分类名称`
  FROM `_深渊装备模板` e
  JOIN `_深渊章节配置` c ON c.`章节ID` = e.`来源章节`
  WHERE e.`是否启用` = 1
    AND c.`是否启用` = 1
    AND e.`套装ID` <> 0
    AND (
      (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
      OR (e.`装备类型` = 2 AND e.`来源模式` = 5 AND e.`是否来自秘藏首领` = 1)
    )
) grp
ORDER BY grp.`章节ID`, grp.`来源模式`;

INSERT INTO `_物品技能_模板`
(`id`, `注释`, `客户端显示`, `组`, `等级`, `技能id`, `几率`, `技能触发类型`, `触发几率`, `技能使用次数`,
 `技能每分钟触发次数`, `技能冷却时间`, `技能冷却值`, `技能消耗`, `技能施法`, `技能强化`, `技能伤害`,
 `技能类型`, `技能持续时间`, `启用状态`, `优先级`)
SELECT
  961000 + ((grp.`章节ID` - 1) * 5) + grp.`来源模式` AS `id`,
  CONCAT(
    '图鉴套装扩展·',
    grp.`分类名称`, '·',
    '第', LPAD(grp.`章节ID`, 2, '0'), '章·', grp.`章节名称`,
    '·8件效果'
  ) AS `注释`,
  CONCAT(
    '8件：攻强+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，法强+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，急速+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5)
  ) AS `客户端显示`,
  961000 + grp.`来源模式` AS `组`,
  grp.`章节ID` AS `等级`,
  961000 + ((grp.`章节ID` - 1) * 5) + grp.`来源模式` AS `技能id`,
  100 AS `几率`,
  1 AS `技能触发类型`,
  100 AS `触发几率`,
  0 AS `技能使用次数`,
  0 AS `技能每分钟触发次数`,
  0 AS `技能冷却时间`,
  0 AS `技能冷却值`,
  0 AS `技能消耗`,
  0 AS `技能施法`,
  0 AS `技能强化`,
  0 AS `技能伤害`,
  0 AS `技能类型`,
  0 AS `技能持续时间`,
  1 AS `启用状态`,
  100 AS `优先级`
FROM (
  SELECT DISTINCT
    e.`套装ID` AS `图鉴套装组`,
    e.`来源章节` AS `章节ID`,
    c.`章节名称`,
    c.`幕ID`,
    e.`来源模式`,
    CASE
      WHEN e.`来源模式` = 1 THEN '正传'
      WHEN e.`来源模式` = 2 THEN '深渊'
      WHEN e.`来源模式` = 3 THEN '腐化'
      WHEN e.`来源模式` = 4 THEN '轮回'
      WHEN e.`来源模式` = 5 THEN '神器'
      ELSE '未知'
    END AS `分类名称`
  FROM `_深渊装备模板` e
  JOIN `_深渊章节配置` c ON c.`章节ID` = e.`来源章节`
  WHERE e.`是否启用` = 1
    AND c.`是否启用` = 1
    AND e.`套装ID` <> 0
    AND (
      (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
      OR (e.`装备类型` = 2 AND e.`来源模式` = 5 AND e.`是否来自秘藏首领` = 1)
    )
) grp
ORDER BY grp.`章节ID`, grp.`来源模式`;

INSERT INTO `_图鉴系统_套装`
(`注释`, `id`, `组`, `需要激活数量`, `激活描述`, `激活模版_物品技能_多个逗号隔开`, `激活后执行GM命令`)
SELECT
  CONCAT(
    grp.`分类名称`, '·',
    '第', LPAD(grp.`章节ID`, 2, '0'), '章·', grp.`章节名称`,
    '·', cfg.`套装名称`,
    '·8件'
  ) AS `注释`,
  grp.`图鉴套装组` * 10 + 8 AS `id`,
  grp.`图鉴套装组` AS `组`,
  8 AS `需要激活数量`,
  CONCAT(
    '耐力+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，智力+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，精神+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，敏捷+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，力量+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，攻强+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，法强+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5),
    '，急速+', ((grp.`章节ID` + grp.`来源模式` - 1) * 5)
  ) AS `激活描述`,
  CONCAT(
    960000 + ((grp.`章节ID` - 1) * 5) + grp.`来源模式`,
    ',',
    961000 + ((grp.`章节ID` - 1) * 5) + grp.`来源模式`
  ) AS `激活模版_物品技能_多个逗号隔开`,
  '' AS `激活后执行GM命令`
FROM (
  SELECT DISTINCT
    e.`套装ID` AS `图鉴套装组`,
    e.`来源章节` AS `章节ID`,
    c.`章节名称`,
    c.`幕ID`,
    e.`来源模式`,
    CASE
      WHEN e.`来源模式` = 1 THEN '正传'
      WHEN e.`来源模式` = 2 THEN '深渊'
      WHEN e.`来源模式` = 3 THEN '腐化'
      WHEN e.`来源模式` = 4 THEN '轮回'
      WHEN e.`来源模式` = 5 THEN '神器'
      ELSE '未知'
    END AS `分类名称`
  FROM `_深渊装备模板` e
  JOIN `_深渊章节配置` c ON c.`章节ID` = e.`来源章节`
  WHERE e.`是否启用` = 1
    AND c.`是否启用` = 1
    AND e.`套装ID` <> 0
    AND (
      (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
      OR (e.`装备类型` = 2 AND e.`来源模式` = 5 AND e.`是否来自秘藏首领` = 1)
    )
) grp
JOIN `_深渊套装配置` cfg
  ON cfg.`幕ID` = grp.`幕ID`
 AND cfg.`来源模式` = grp.`来源模式`
 AND cfg.`是否启用` = 1
ORDER BY
  FIELD(grp.`分类名称`, '正传', '深渊', '腐化', '轮回', '神器'),
  grp.`幕ID`,
  grp.`章节ID`,
  grp.`图鉴套装组`;

SET FOREIGN_KEY_CHECKS = 1;

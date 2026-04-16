
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
-- 深渊修仙装备图鉴：章节套装羁绊
-- 规则：
-- 1. 正传 / 深渊 / 腐化 / 轮回：按章节 9 件组生成 2/4/6/8 档
-- 2. 神器：仅对接 8 件神器套装部位，14 把秘藏神兵不进入羁绊组
-- 3. 套装描述复用 _深渊套装配置 当前幕/模式的现行文案
-- ============================================

INSERT INTO `_图鉴系统_套装`
(`注释`, `id`, `组`, `需要激活数量`, `激活描述`, `激活模版_物品技能_多个逗号隔开`, `激活后执行GM命令`)
SELECT
  CONCAT(
    grp.`分类名称`, '·',
    '第', LPAD(grp.`章节ID`, 2, '0'), '章·', grp.`章节名称`,
    '·', cfg.`套装名称`,
    '·', tier.`需要激活数量`, '件'
  ) AS `注释`,
  grp.`图鉴套装组` * 10 + tier.`需要激活数量` AS `id`,
  grp.`图鉴套装组` AS `组`,
  tier.`需要激活数量`,
  CASE tier.`需要激活数量`
    WHEN 2 THEN cfg.`2件效果描述`
    WHEN 4 THEN cfg.`4件效果描述`
    WHEN 6 THEN cfg.`6件效果描述`
    WHEN 8 THEN cfg.`8件效果描述`
    ELSE ''
  END AS `激活描述`,
  '' AS `激活模版_物品技能_多个逗号隔开`,
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
JOIN (
  SELECT 2 AS `需要激活数量`
  UNION ALL
  SELECT 4
  UNION ALL
  SELECT 6
  UNION ALL
  SELECT 8
) tier
WHERE CASE tier.`需要激活数量`
        WHEN 2 THEN cfg.`2件效果描述`
        WHEN 4 THEN cfg.`4件效果描述`
        WHEN 6 THEN cfg.`6件效果描述`
        WHEN 8 THEN cfg.`8件效果描述`
        ELSE ''
      END <> ''
ORDER BY
  FIELD(grp.`分类名称`, '正传', '深渊', '腐化', '轮回', '神器'),
  grp.`幕ID`,
  grp.`章节ID`,
  grp.`图鉴套装组`,
  tier.`需要激活数量`;

SET FOREIGN_KEY_CHECKS = 1;

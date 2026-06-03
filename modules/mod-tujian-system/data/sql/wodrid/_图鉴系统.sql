
DROP TABLE IF EXISTS `_图鉴系统`;
CREATE TABLE `_图鉴系统`  (
  `注释` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '同一个栏位，id一样，按等级升级',
  `一级菜单名称` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `一级菜单图标` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `二级菜单名称1` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `二级菜单名称2` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `二级菜单图标` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `第几页` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '每页id不要超过28个',
  `等级` int UNSIGNED NOT NULL DEFAULT 0,
  `最大等级` int UNSIGNED NOT NULL DEFAULT 0,
  `物品entry` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '必须是唯一的，支持物品鉴定',
  `套装ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '对应 _图鉴系统_套装.组；同组可配置多档套装效果，0表示无套装',
  `激活需求` int UNSIGNED NOT NULL DEFAULT 0,
  `激活后执行GM命令` varchar(2000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '激活图鉴后执行GM命令',
  `属性生效模式` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '0=固定全属性值,1=装备属性',
  `固定全属性值` decimal(65,0) NOT NULL DEFAULT 0 COMMENT '仅在属性生效模式=0时生效；每激活一个图鉴，为力量/敏捷/耐力/智力/精神各增加该数值',
  PRIMARY KEY (`物品entry`) USING BTREE,
  KEY `idx_套装ID` (`套装ID`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item System' ROW_FORMAT = DYNAMIC;

ALTER TABLE `_图鉴系统`
  MODIFY COLUMN `固定全属性值` decimal(65,0) NOT NULL DEFAULT 0 COMMENT '仅在属性生效模式=0时生效；每激活一个图鉴，为力量/敏捷/耐力/智力/精神各增加该数值';

-- ----------------------------
-- Records of _图鉴系统
-- ----------------------------

-- ============================================
-- 深渊修仙装备图鉴：激活需求模板
-- 规则：
-- 1. 仅对接 正传 / 深渊 / 腐化 / 轮回 / 神器 五条装备线
-- 2. 需求模板 ID 直接复用物品 entry，激活时消耗对应装备 1 件
-- 3. 不处理遗物 / 阶段神器 / 终极神器
-- ============================================

DELETE FROM `_模板_需求`
WHERE `id` BETWEEN 980001 AND 993666;

REPLACE INTO `_模板_需求`
(`注释`, `id`, `消耗金币`, `是否消耗物品`, `消耗物品`, `客户端显示`)
SELECT
  CONCAT('深渊修仙图鉴激活-', e.`装备名称`) AS `注释`,
  e.`物品模板ID` AS `id`,
  0 AS `消耗金币`,
  0 AS `是否消耗物品`,
  CONCAT(e.`物品模板ID`, ' 1,60002 1000') AS `消耗物品`,
  CONCAT('提交 ', e.`装备名称`, ' x1, 60002 x1000 激活图鉴') AS `客户端显示`
FROM `_深渊装备模板` e
WHERE e.`是否启用` = 1
  AND (
    (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
    OR (e.`装备类型` = 2 AND e.`是否来自秘藏首领` = 1)
  );

-- ============================================
-- 深渊修仙装备图鉴：主表数据
-- 菜单结构：
-- 一级菜单 = 60副本 / 70副本 / 80副本 / 60团 / 70团 / 80团
-- 二级菜单按章节副本顺序排列，怒焰裂谷开始
-- 分页规则：
-- 同一章节按模式拆分页：正传 / 深渊 / 腐化 / 轮回 / 神器
-- 普通装备每页 9 件，神器每页 22 件，满足每页不超过 28 条
-- ============================================

INSERT INTO `_图鉴系统`
(`注释`, `id`, `一级菜单名称`, `一级菜单图标`, `二级菜单名称1`, `二级菜单名称2`, `二级菜单图标`,
 `第几页`, `等级`, `最大等级`, `物品entry`, `套装ID`, `激活需求`, `激活后执行GM命令`, `属性生效模式`, `固定全属性值`)
SELECT
  atlas.`注释`,
  atlas.`顺序ID` AS `id`,
  atlas.`一级菜单名称`,
  atlas.`一级菜单图标`,
  atlas.`二级菜单名称1`,
  atlas.`二级菜单名称2`,
  atlas.`二级菜单图标`,
  atlas.`第几页`,
  atlas.`等级`,
  atlas.`最大等级`,
  atlas.`物品entry`,
  atlas.`套装ID`,
  atlas.`激活需求`,
  atlas.`激活后执行GM命令`,
  atlas.`属性生效模式`,
  atlas.`固定全属性值`
FROM (
  SELECT
    ROW_NUMBER() OVER (
      ORDER BY
        c.`章节ID`,
        CASE
          WHEN e.`装备类型` = 1 AND e.`来源模式` = 1 THEN 1
          WHEN e.`装备类型` = 1 AND e.`来源模式` = 2 THEN 2
          WHEN e.`装备类型` = 1 AND e.`来源模式` = 3 THEN 3
          WHEN e.`装备类型` = 1 AND e.`来源模式` = 4 THEN 4
          ELSE 5
        END,
        e.`物品模板ID`
    ) AS `顺序ID`,
    CONCAT(
      CASE
        WHEN c.`章节类型` = 1 AND c.`章节ID` BETWEEN 1 AND 20 THEN '60副本'
        WHEN c.`章节类型` = 1 AND c.`章节ID` BETWEEN 21 AND 36 THEN '70副本'
        WHEN c.`章节类型` = 1 THEN '80副本'
        WHEN c.`章节类型` = 2 AND c.`章节ID` BETWEEN 53 AND 57 THEN '60团'
        WHEN c.`章节类型` = 2 AND c.`章节ID` BETWEEN 58 AND 66 THEN '70团'
        ELSE '80团'
      END,
      '·第', LPAD(c.`章节ID`, 2, '0'), '章·', c.`章节名称`,
      '·',
      CASE
        WHEN e.`装备类型` = 1 AND e.`来源模式` = 1 THEN '正传'
        WHEN e.`装备类型` = 1 AND e.`来源模式` = 2 THEN '深渊'
        WHEN e.`装备类型` = 1 AND e.`来源模式` = 3 THEN '腐化'
        WHEN e.`装备类型` = 1 AND e.`来源模式` = 4 THEN '轮回'
        ELSE '神器'
      END,
      '·',
      e.`装备名称`,
      CASE
        WHEN e.`套装ID` <> 0 AND (
          (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
          OR (e.`装备类型` = 2 AND e.`来源模式` = 5)
        ) THEN CONCAT('·套装组', e.`套装ID`)
        ELSE ''
      END
    ) AS `注释`,
    e.`物品模板ID` AS `物品entry`,
    CASE
      WHEN c.`章节类型` = 1 AND c.`章节ID` BETWEEN 1 AND 20 THEN '60副本'
      WHEN c.`章节类型` = 1 AND c.`章节ID` BETWEEN 21 AND 36 THEN '70副本'
      WHEN c.`章节类型` = 1 THEN '80副本'
      WHEN c.`章节类型` = 2 AND c.`章节ID` BETWEEN 53 AND 57 THEN '60团'
      WHEN c.`章节类型` = 2 AND c.`章节ID` BETWEEN 58 AND 66 THEN '70团'
      ELSE '80团'
    END AS `一级菜单名称`,
    '' AS `一级菜单图标`,
    CASE
      WHEN c.`章节类型` = 1 AND c.`章节ID` BETWEEN 1 AND 20 THEN '60副本'
      WHEN c.`章节类型` = 1 AND c.`章节ID` BETWEEN 21 AND 36 THEN '70副本'
      WHEN c.`章节类型` = 1 THEN '80副本'
      WHEN c.`章节类型` = 2 AND c.`章节ID` BETWEEN 53 AND 57 THEN '60团'
      WHEN c.`章节类型` = 2 AND c.`章节ID` BETWEEN 58 AND 66 THEN '70团'
      ELSE '80团'
    END AS `二级菜单名称1`,
    c.`章节名称` AS `二级菜单名称2`,
    '' AS `二级菜单图标`,
    c.`章节ID` * 10 + CASE
      WHEN e.`装备类型` = 1 AND e.`来源模式` = 1 THEN 1
      WHEN e.`装备类型` = 1 AND e.`来源模式` = 2 THEN 2
      WHEN e.`装备类型` = 1 AND e.`来源模式` = 3 THEN 3
      WHEN e.`装备类型` = 1 AND e.`来源模式` = 4 THEN 4
      ELSE 5
    END AS `第几页`,
    1 AS `等级`,
    1 AS `最大等级`,
    CASE
      WHEN e.`套装ID` <> 0 AND (
        (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
        OR (e.`装备类型` = 2 AND e.`来源模式` = 5)
      ) THEN e.`套装ID`
      ELSE 0
    END AS `套装ID`,
    e.`物品模板ID` AS `激活需求`,
    '' AS `激活后执行GM命令`,
    1 AS `属性生效模式`,
    0 AS `固定全属性值`
  FROM `_深渊装备模板` e
  JOIN `_深渊章节配置` c ON c.`章节ID` = e.`来源章节`
  WHERE e.`是否启用` = 1
    AND c.`是否启用` = 1
    AND (
      (e.`装备类型` = 1 AND e.`来源模式` BETWEEN 1 AND 4)
      OR (e.`装备类型` = 2 AND e.`是否来自秘藏首领` = 1)
    )
) atlas
ORDER BY
  atlas.`顺序ID`;

SET FOREIGN_KEY_CHECKS = 1;

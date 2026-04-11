-- ============================================
-- 对接映射表：用于后续接 item_template / quest_template / creature_template
-- ============================================

DROP TABLE IF EXISTS `_深渊物品模板对接`;
CREATE TABLE `_深渊物品模板对接` (
  `物品ID` int unsigned NOT NULL COMMENT '目标 item_template.entry',
  `物品名称` varchar(64) NOT NULL DEFAULT '' COMMENT '物品名称',
  `对接类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=章节遗物 2=阶段神器 3=终极神器 4=普通底材 5=唯一装备',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '所属幕',
  `章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '所属章节',
  `建议物品品质` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '建议 item_template.Quality',
  `建议物品分类` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '建议 item_template.class',
  `建议物品子类` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '建议 item_template.subclass',
  `建议装备槽位` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '建议 item_template.InventoryType',
  `是否唯一` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '是否唯一',
  `是否背包激活` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0否 1是',
  `预留显示ID` int unsigned NOT NULL DEFAULT 0 COMMENT '建议 displayid 预留',
  `对接状态` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0待建 1已建 2已校验',
  `说明` varchar(255) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`物品ID`),
  KEY `索引_对接类型` (`对接类型`),
  KEY `索引_章节ID` (`章节ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊物品模板对接';

DELETE FROM `_深渊物品模板对接` WHERE `物品ID` BETWEEN 950001 AND 993666 OR `物品ID` BETWEEN 960001 AND 960006 OR `物品ID` = 970001;
INSERT INTO `_深渊物品模板对接`
(`物品ID`, `物品名称`, `对接类型`, `幕ID`, `章节ID`, `建议物品品质`, `建议物品分类`, `建议物品子类`, `建议装备槽位`, `是否唯一`, `是否背包激活`, `预留显示ID`, `对接状态`, `说明`)
SELECT
  `物品ID`,
  `名称`,
  CASE `类型` WHEN 1 THEN 1 WHEN 2 THEN 2 WHEN 3 THEN 3 ELSE 0 END,
  `幕ID`,
  `关联章节ID`,
  CASE `类型` WHEN 1 THEN 4 WHEN 2 THEN 5 WHEN 3 THEN 5 ELSE 3 END,
  15,
  0,
  0,
  1,
  1,
  0,
  0,
  '来自 _深渊遗物配置，后续需写入 item_template'
FROM `_深渊遗物配置`;

-- 遗物 / 神器 / 终极神器：按名称主题锁定显示模型，避免整批遗物共用同一图标
UPDATE `_深渊物品模板对接`
SET `预留显示ID` = CASE `物品ID`
  WHEN 950001 THEN 59323
  WHEN 950002 THEN 64245
  WHEN 950003 THEN 64441
  WHEN 950004 THEN 42176
  WHEN 950005 THEN 53843
  WHEN 950006 THEN 48009
  WHEN 950007 THEN 59328
  WHEN 950008 THEN 6338
  WHEN 950009 THEN 45211
  WHEN 950010 THEN 54740
  WHEN 950011 THEN 64441
  WHEN 950012 THEN 43431
  WHEN 950013 THEN 59320
  WHEN 950014 THEN 53843
  WHEN 950015 THEN 61788
  WHEN 950016 THEN 64256
  WHEN 950017 THEN 61414
  WHEN 950018 THEN 54740
  WHEN 950019 THEN 59323
  WHEN 950020 THEN 61788
  WHEN 950021 THEN 61409
  WHEN 950022 THEN 64237
  WHEN 950023 THEN 42176
  WHEN 950024 THEN 60705
  WHEN 950025 THEN 64245
  WHEN 950026 THEN 59328
  WHEN 950027 THEN 64247
  WHEN 950028 THEN 60705
  WHEN 950029 THEN 64631
  WHEN 950030 THEN 58824
  WHEN 950031 THEN 43431
  WHEN 950032 THEN 64441
  WHEN 950033 THEN 64441
  WHEN 950034 THEN 54740
  WHEN 950035 THEN 64264
  WHEN 950036 THEN 60704
  WHEN 950037 THEN 32515
  WHEN 950038 THEN 59328
  WHEN 950039 THEN 54740
  WHEN 950040 THEN 54844
  WHEN 950041 THEN 64251
  WHEN 950042 THEN 54062
  WHEN 950043 THEN 34132
  WHEN 950044 THEN 59320
  WHEN 950045 THEN 59328
  WHEN 950046 THEN 51001
  WHEN 950047 THEN 53843
  WHEN 950048 THEN 6338
  WHEN 950049 THEN 61788
  WHEN 950050 THEN 54614
  WHEN 950051 THEN 64251
  WHEN 950052 THEN 61791
  WHEN 950053 THEN 59323
  WHEN 950054 THEN 46981
  WHEN 950055 THEN 54844
  WHEN 950056 THEN 64923
  WHEN 950057 THEN 61413
  WHEN 950058 THEN 46981
  WHEN 950059 THEN 34132
  WHEN 950060 THEN 64247
  WHEN 950061 THEN 26374
  WHEN 950062 THEN 61791
  WHEN 950063 THEN 61414
  WHEN 950064 THEN 61791
  WHEN 950065 THEN 46817
  WHEN 950066 THEN 60704
  WHEN 950067 THEN 64247
  WHEN 950068 THEN 53843
  WHEN 950069 THEN 58824
  WHEN 950070 THEN 53843
  WHEN 950071 THEN 45211
  WHEN 950072 THEN 26374
  WHEN 950073 THEN 61788
  WHEN 950074 THEN 45160
  WHEN 960001 THEN 61788
  WHEN 960002 THEN 61788
  WHEN 960003 THEN 64247
  WHEN 960004 THEN 45211
  WHEN 960005 THEN 61788
  WHEN 960006 THEN 54062
  WHEN 970001 THEN 42185
  ELSE `预留显示ID`
END
WHERE (`物品ID` BETWEEN 950001 AND 950074)
   OR (`物品ID` BETWEEN 960001 AND 960006)
   OR `物品ID` = 970001;

INSERT INTO `_深渊物品模板对接`
(`物品ID`, `物品名称`, `对接类型`, `幕ID`, `章节ID`, `建议物品品质`, `建议物品分类`, `建议物品子类`, `建议装备槽位`, `是否唯一`, `是否背包激活`, `预留显示ID`, `对接状态`, `说明`)
SELECT
  `物品模板ID`,
  `装备名称`,
  CASE `装备类型` WHEN 1 THEN 4 WHEN 2 THEN 5 ELSE 0 END,
  `幕ID`,
  `来源章节`,
  CASE `装备类型` WHEN 1 THEN 4 ELSE 5 END,
  CASE
    WHEN `部位掩码` = 1 THEN 2
    ELSE 4
  END,
  CASE
    WHEN `部位掩码` = 1 THEN CASE `伤害类型`
      WHEN 5 THEN 0
      WHEN 6 THEN 4
      WHEN 7 THEN 15
      WHEN 8 THEN 13
      WHEN 9 THEN 8
      WHEN 10 THEN 1
      WHEN 11 THEN 5
      WHEN 12 THEN 6
      WHEN 13 THEN 10
      WHEN 14 THEN 2
      WHEN 15 THEN 3
      WHEN 16 THEN 18
      WHEN 17 THEN 19
      WHEN 2 THEN 10
      WHEN 3 THEN 4
      WHEN 4 THEN 10
      ELSE 7
    END
    WHEN `部位掩码` IN (64, 128, 256, 512) THEN 0
    ELSE CASE `护甲类型`
      WHEN 1 THEN 1
      WHEN 2 THEN 2
      WHEN 3 THEN 3
      WHEN 4 THEN 4
      ELSE 0
    END
  END,
  CASE
    WHEN `部位掩码` = 1 THEN CASE `伤害类型`
      WHEN 9 THEN 17
      WHEN 10 THEN 17
      WHEN 11 THEN 17
      WHEN 12 THEN 17
      WHEN 13 THEN 17
      WHEN 14 THEN 15
      WHEN 15 THEN 26
      WHEN 16 THEN 26
      WHEN 17 THEN 26
      ELSE 13
    END
    WHEN 2 THEN 1
    WHEN 4 THEN 5
    WHEN 16 THEN 6
    WHEN 32 THEN 8
    WHEN 64 THEN 11
    WHEN 128 THEN 12
    WHEN 256 THEN 16
    WHEN 512 THEN 23
    ELSE 0
  END,
  CASE `装备类型` WHEN 2 THEN 1 ELSE 0 END,
  0,
  0,
  0,
  '来自 _深渊装备模板，后续需转换到 item_template 的 class/subclass/inventoryType'
FROM `_深渊装备模板`;

-- 所有装备：按幕主题 + 部位锁定基础外观，秘藏与四模式底材统一复用
UPDATE `_深渊物品模板对接` d
JOIN `_深渊装备模板` e ON e.`物品模板ID` = d.`物品ID`
JOIN (
  SELECT 1 AS `幕ID`, 1 AS `部位掩码`, 62984 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 512 AS `部位掩码`, 58973 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 2 AS `部位掩码`, 28143 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 4 AS `部位掩码`, 64854 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 16 AS `部位掩码`, 31682 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 32 AS `部位掩码`, 59094 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 64 AS `部位掩码`, 63958 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 128 AS `部位掩码`, 26374 AS `显示ID`
  UNION ALL
  SELECT 1 AS `幕ID`, 256 AS `部位掩码`, 59553 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 1 AS `部位掩码`, 60617 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 512 AS `部位掩码`, 60705 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 2 AS `部位掩码`, 62035 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 4 AS `部位掩码`, 61849 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 16 AS `部位掩码`, 62138 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 32 AS `部位掩码`, 62108 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 64 AS `部位掩码`, 52632 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 128 AS `部位掩码`, 48008 AS `显示ID`
  UNION ALL
  SELECT 2 AS `幕ID`, 256 AS `部位掩码`, 61260 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 1 AS `部位掩码`, 52784 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 512 AS `部位掩码`, 59328 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 2 AS `部位掩码`, 65040 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 4 AS `部位掩码`, 64703 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 16 AS `部位掩码`, 62008 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 32 AS `部位掩码`, 62110 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 64 AS `部位掩码`, 64227 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 128 AS `部位掩码`, 64261 AS `显示ID`
  UNION ALL
  SELECT 3 AS `幕ID`, 256 AS `部位掩码`, 59040 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 1 AS `部位掩码`, 64756 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 512 AS `部位掩码`, 53843 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 2 AS `部位掩码`, 44683 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 4 AS `部位掩码`, 51520 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 16 AS `部位掩码`, 43522 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 32 AS `部位掩码`, 65259 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 64 AS `部位掩码`, 33864 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 128 AS `部位掩码`, 61413 AS `显示ID`
  UNION ALL
  SELECT 4 AS `幕ID`, 256 AS `部位掩码`, 58999 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 1 AS `部位掩码`, 64153 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 512 AS `部位掩码`, 61791 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 2 AS `部位掩码`, 57531 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 4 AS `部位掩码`, 59766 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 16 AS `部位掩码`, 59700 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 32 AS `部位掩码`, 60568 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 64 AS `部位掩码`, 63960 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 128 AS `部位掩码`, 64631 AS `显示ID`
  UNION ALL
  SELECT 5 AS `幕ID`, 256 AS `部位掩码`, 59034 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 1 AS `部位掩码`, 64997 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 512 AS `部位掩码`, 48894 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 2 AS `部位掩码`, 33822 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 4 AS `部位掩码`, 45017 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 16 AS `部位掩码`, 61962 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 32 AS `部位掩码`, 64789 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 64 AS `部位掩码`, 64176 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 128 AS `部位掩码`, 59316 AS `显示ID`
  UNION ALL
  SELECT 6 AS `幕ID`, 256 AS `部位掩码`, 64648 AS `显示ID`
) m ON m.`幕ID` = e.`幕ID` AND m.`部位掩码` = e.`部位掩码`
SET d.`预留显示ID` = m.`显示ID`
WHERE e.`装备类型` IN (1, 2);

-- 秘藏武器 / 法器：按具体武器类型覆盖显示模型，避免所有武器共用同一外观导致错模或问号
UPDATE `_深渊物品模板对接` d
JOIN `_深渊装备模板` e ON e.`物品模板ID` = d.`物品ID`
SET d.`预留显示ID` = CASE
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 1 THEN 64536   -- 单手剑
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 5 THEN 64480   -- 单手斧
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 6 THEN 64313   -- 单手锤
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 7 THEN 64997   -- 匕首
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 8 THEN 40181   -- 拳套
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 9 THEN 64397   -- 双手剑
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 10 THEN 64879  -- 双手斧
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 11 THEN 64394  -- 双手锤
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 12 THEN 64554  -- 长柄武器
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 13 THEN 64334  -- 法杖
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 14 THEN 64356  -- 弓
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 15 THEN 64366  -- 枪械
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 16 THEN 64371  -- 弩
  WHEN e.`部位掩码` = 1 AND e.`伤害类型` = 17 THEN 64995  -- 魔杖
  WHEN e.`部位掩码` = 512 THEN 64441                      -- 法器/副手
  ELSE d.`预留显示ID`
END
WHERE e.`装备类型` = 2
  AND e.`是否来自秘藏首领` = 1
  AND (e.`部位掩码` = 1 OR e.`部位掩码` = 512);

DROP TABLE IF EXISTS `_深渊任务模板对接`;
CREATE TABLE `_深渊任务模板对接` (
  `任务ID` int unsigned NOT NULL COMMENT '目标 quest_template.ID',
  `章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '关联章节',
  `任务类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=起始任务 2=完成任务 3=隐藏任务 4=引导任务',
  `任务标题` varchar(96) NOT NULL DEFAULT '' COMMENT '建议任务名',
  `上一任务ID` int unsigned NOT NULL DEFAULT 0 COMMENT '前置任务',
  `下一任务ID` int unsigned NOT NULL DEFAULT 0 COMMENT '后续任务',
  `建议任务等级` smallint unsigned NOT NULL DEFAULT 0 COMMENT '建议任务等级',
  `建议最小等级` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '建议最小接取等级',
  `建议任务分类` smallint NOT NULL DEFAULT 0 COMMENT 'QuestSortID/章节分类建议',
  `任务说明` varchar(255) NOT NULL DEFAULT '' COMMENT '备注',
  `对接状态` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0待建 1已建 2已校验',
  PRIMARY KEY (`任务ID`),
  KEY `索引_章节ID` (`章节ID`),
  KEY `索引_任务类型` (`任务类型`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊任务模板对接';

DELETE FROM `_深渊任务模板对接` WHERE `任务ID` BETWEEN 700001 AND 740074;
INSERT INTO `_深渊任务模板对接`
(`任务ID`, `章节ID`, `任务类型`, `任务标题`, `上一任务ID`, `下一任务ID`, `建议任务等级`, `建议最小等级`, `建议任务分类`, `任务说明`, `对接状态`)
SELECT
  `起始任务ID`,
  `章节ID`,
  1,
  CONCAT('第', LPAD(`章节ID`, 2, '0'), '章·起始'),
  CASE WHEN `前置章节ID` = 0 THEN 0 ELSE 730000 + `前置章节ID` END,
  `完成任务ID`,
  `需求修仙等级`,
  LEAST(`需求修仙等级`, 80),
  0,
  '章节起始引导任务',
  0
FROM `_深渊章节配置`;

INSERT INTO `_深渊任务模板对接`
(`任务ID`, `章节ID`, `任务类型`, `任务标题`, `上一任务ID`, `下一任务ID`, `建议任务等级`, `建议最小等级`, `建议任务分类`, `任务说明`, `对接状态`)
SELECT
  `完成任务ID`,
  `章节ID`,
  2,
  CONCAT('第', LPAD(`章节ID`, 2, '0'), '章·官方'),
  `起始任务ID`,
  720000 + `章节ID`,
  `需求修仙等级`,
  LEAST(`需求修仙等级`, 80),
  0,
  '官方副本锚点首领任务，完成后解锁正传模式',
  0
FROM `_深渊章节配置`;

INSERT INTO `_深渊任务模板对接`
(`任务ID`, `章节ID`, `任务类型`, `任务标题`, `上一任务ID`, `下一任务ID`, `建议任务等级`, `建议最小等级`, `建议任务分类`, `任务说明`, `对接状态`)
SELECT
  720000 + `章节ID`,
  `章节ID`,
  3,
  CONCAT('第', LPAD(`章节ID`, 2, '0'), '章·正传'),
  `完成任务ID`,
  730000 + `章节ID`,
  `需求修仙等级`,
  LEAST(`需求修仙等级`, 80),
  0,
  '正传模式锚点首领任务，完成后解锁深渊模式',
  0
FROM `_深渊章节配置`;

INSERT INTO `_深渊任务模板对接`
(`任务ID`, `章节ID`, `任务类型`, `任务标题`, `上一任务ID`, `下一任务ID`, `建议任务等级`, `建议最小等级`, `建议任务分类`, `任务说明`, `对接状态`)
SELECT
  730000 + `章节ID`,
  `章节ID`,
  4,
  CONCAT('第', LPAD(`章节ID`, 2, '0'), '章·深渊'),
  720000 + `章节ID`,
  740000 + `章节ID`,
  `需求修仙等级`,
  LEAST(`需求修仙等级`, 80),
  0,
  '深渊模式锚点首领任务，完成后解锁腐化模式',
  0
FROM `_深渊章节配置`;

INSERT INTO `_深渊任务模板对接`
(`任务ID`, `章节ID`, `任务类型`, `任务标题`, `上一任务ID`, `下一任务ID`, `建议任务等级`, `建议最小等级`, `建议任务分类`, `任务说明`, `对接状态`)
SELECT
  740000 + `章节ID`,
  `章节ID`,
  5,
  CONCAT('第', LPAD(`章节ID`, 2, '0'), '章·腐化'),
  730000 + `章节ID`,
  CASE WHEN `章节ID` >= 74 THEN 0 ELSE 700001 + `章节ID` END,
  `需求修仙等级`,
  LEAST(`需求修仙等级`, 80),
  0,
  '腐化模式锚点首领任务，完成后解锁轮回模式并开启下一副本任务',
  0
FROM `_深渊章节配置`;


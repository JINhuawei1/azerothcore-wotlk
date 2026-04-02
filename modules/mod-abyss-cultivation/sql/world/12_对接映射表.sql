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

DELETE FROM `_深渊物品模板对接` WHERE `物品ID` BETWEEN 950001 AND 992054 OR `物品ID` BETWEEN 960001 AND 960006 OR `物品ID` = 970001;
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
  2,
  `护甲类型`,
  CASE `部位掩码`
    WHEN 1 THEN 13
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

-- 专属装备：按名称锁定首版显示模型，避免唯一装备共用同一占位外观
UPDATE `_深渊物品模板对接`
SET `预留显示ID` = CASE `物品ID`
  WHEN 980001 THEN 62984
  WHEN 980002 THEN 61404
  WHEN 980003 THEN 64225
  WHEN 980004 THEN 53198
  WHEN 980005 THEN 43431
  WHEN 980006 THEN 64441
  WHEN 980007 THEN 60675
  WHEN 980008 THEN 64749
  WHEN 980009 THEN 64247
  WHEN 980010 THEN 44357
  WHEN 980011 THEN 64693
  WHEN 980012 THEN 64256
  ELSE `预留显示ID`
END
WHERE `物品ID` IN (980001, 980002, 980003, 980004, 980005, 980006, 980007, 980008, 980009, 980010, 980011, 980012);

-- 普通底材：按幕主题和部位锁定基础外观，避免整批底材共用单一占位模型
UPDATE `_深渊物品模板对接`
SET `预留显示ID` = CASE `物品ID`
  WHEN 990001 THEN 62984
  WHEN 990002 THEN 58973
  WHEN 990003 THEN 28143
  WHEN 990004 THEN 64854
  WHEN 990005 THEN 31682
  WHEN 990006 THEN 59094
  WHEN 990007 THEN 63958
  WHEN 990008 THEN 26374
  WHEN 990009 THEN 59553
  WHEN 990010 THEN 60617
  WHEN 990011 THEN 60705
  WHEN 990012 THEN 62035
  WHEN 990013 THEN 61849
  WHEN 990014 THEN 62138
  WHEN 990015 THEN 62108
  WHEN 990016 THEN 52632
  WHEN 990017 THEN 48008
  WHEN 990018 THEN 61260
  WHEN 990019 THEN 52784
  WHEN 990020 THEN 59328
  WHEN 990021 THEN 65040
  WHEN 990022 THEN 64703
  WHEN 990023 THEN 62008
  WHEN 990024 THEN 62110
  WHEN 990025 THEN 64227
  WHEN 990026 THEN 64261
  WHEN 990027 THEN 59040
  WHEN 990028 THEN 64756
  WHEN 990029 THEN 53843
  WHEN 990030 THEN 44683
  WHEN 990031 THEN 51520
  WHEN 990032 THEN 43522
  WHEN 990033 THEN 65259
  WHEN 990034 THEN 33864
  WHEN 990035 THEN 61413
  WHEN 990036 THEN 58999
  WHEN 990037 THEN 64153
  WHEN 990038 THEN 61791
  WHEN 990039 THEN 57531
  WHEN 990040 THEN 59766
  WHEN 990041 THEN 59700
  WHEN 990042 THEN 60568
  WHEN 990043 THEN 63960
  WHEN 990044 THEN 64631
  WHEN 990045 THEN 59034
  WHEN 990046 THEN 64997
  WHEN 990047 THEN 48894
  WHEN 990048 THEN 33822
  WHEN 990049 THEN 45017
  WHEN 990050 THEN 61962
  WHEN 990051 THEN 64789
  WHEN 990052 THEN 64176
  WHEN 990053 THEN 59316
  WHEN 990054 THEN 64648
  ELSE `预留显示ID`
END
WHERE `物品ID` BETWEEN 990001 AND 990054;

-- 腐化 / 轮回底材沿用对应正传底材的首版外观映射，确保新增套装可直接显示
UPDATE `_深渊物品模板对接` d
JOIN `_深渊物品模板对接` s ON s.`物品ID` = d.`物品ID` - 1000
SET d.`预留显示ID` = s.`预留显示ID`
WHERE d.`物品ID` BETWEEN 991001 AND 991054
  AND s.`物品ID` BETWEEN 990001 AND 990054;

UPDATE `_深渊物品模板对接` d
JOIN `_深渊物品模板对接` s ON s.`物品ID` = d.`物品ID` - 2000
SET d.`预留显示ID` = s.`预留显示ID`
WHERE d.`物品ID` BETWEEN 992001 AND 992054
  AND s.`物品ID` BETWEEN 990001 AND 990054;

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


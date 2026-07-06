-- 仙门 T10 职业装备补齐（共用已制作的通用套装 ID）
-- 说明：
-- 1. 不新增、不替换套装效果；每个阶段继续使用已有通用 8 件套 itemset。
-- 2. 每个阶段新增 10 个职业 × 8 件装备，绑定同一个阶段通用 itemset。
-- 3. 装备数值属性按阶段写入；外观/职业限制/甲类型参考神圣 T10 与同甲散件。
-- 4. itemset_dbc 只有 17 个 ItemID 槽，无法容纳 80 件职业装备；套装效果计数由 item_template.itemset 生效。

DELETE FROM `item_template`
WHERE (`entry` BETWEEN 95220 AND 95299)
   OR (`entry` BETWEEN 95420 AND 95499)
   OR (`entry` BETWEEN 95620 AND 95699)
   OR (`entry` BETWEEN 95820 AND 95899)
   OR (`entry` BETWEEN 96020 AND 96099)
   OR (`entry` BETWEEN 96220 AND 96299)
   OR (`entry` BETWEEN 96420 AND 96499)
   OR (`entry` BETWEEN 96620 AND 96699)
   OR (`entry` BETWEEN 96820 AND 96899)
   OR (`entry` BETWEEN 97020 AND 97099)
   OR (`entry` BETWEEN 97220 AND 97299)
   OR (`entry` BETWEEN 97420 AND 97499)
   OR (`entry` BETWEEN 97620 AND 97699)
   OR (`entry` BETWEEN 97820 AND 97899)
   OR (`entry` BETWEEN 98020 AND 98099)
   OR (`entry` BETWEEN 98220 AND 98299)
   OR (`entry` BETWEEN 98420 AND 98499);

DROP TEMPORARY TABLE IF EXISTS `_xianmen_t10_stage`;
CREATE TEMPORARY TABLE `_xianmen_t10_stage`
(
    `base_entry` INT UNSIGNED NOT NULL,
    `itemset` INT UNSIGNED NOT NULL,
    `stage_name` VARCHAR(32) NOT NULL,
    `item_level` INT UNSIGNED NOT NULL,
    `stat_value` DECIMAL(65,0) NOT NULL,
    PRIMARY KEY (`base_entry`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4;

INSERT INTO `_xianmen_t10_stage` (`base_entry`, `itemset`, `stage_name`, `item_level`, `stat_value`) VALUES
(95200, 384101, '1万仙装', 1000, 10000),
(95400, 386011, '10万仙装', 1001, 100000),
(95600, 386021, '100万仙装', 1002, 1000000),
(95800, 386031, '1000万仙装', 1003, 10000000),
(96000, 386041, '1亿仙装', 1004, 100000000),
(96200, 386051, '10亿仙装', 1005, 1000000000),
(96400, 386061, '100亿仙装', 1006, 10000000000),
(96600, 386071, '1000亿仙装', 1007, 100000000000),
(96800, 386081, '1兆仙装', 1008, 1000000000000),
(97000, 386091, '10兆仙装', 1009, 10000000000000),
(97200, 386101, '100兆仙装', 1010, 100000000000000),
(97400, 386111, '1000兆仙装', 1011, 1000000000000000),
(97600, 386121, '1京仙装', 1012, 10000000000000000),
(97800, 386131, '10京仙装', 1013, 100000000000000000),
(98000, 386141, '100京仙装', 1014, 1000000000000000000),
(98200, 386151, '1000京仙装', 1015, 10000000000000000000),
(98400, 386161, '1垓仙装', 1016, 100000000000000000000);

DROP TEMPORARY TABLE IF EXISTS `_xianmen_t10_class_slot`;
CREATE TEMPORARY TABLE `_xianmen_t10_class_slot`
(
    `class_index` TINYINT UNSIGNED NOT NULL,
    `slot_index` TINYINT UNSIGNED NOT NULL,
    `class_name` VARCHAR(16) NOT NULL,
    `class_mask` INT NOT NULL,
    `source_entry` INT UNSIGNED NOT NULL,
    `slot_name` VARCHAR(32) NOT NULL,
    PRIMARY KEY (`class_index`, `slot_index`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4;

INSERT INTO `_xianmen_t10_class_slot`
(`class_index`, `slot_index`, `class_name`, `class_mask`, `source_entry`, `slot_name`) VALUES
-- 法师
(0, 0, '法师', 128, 51158, '血法罩帽'),
(0, 1, '法师', 128, 51155, '血法肩垫'),
(0, 2, '法师', 128, 51156, '血法长袍'),
(0, 3, '法师', 128, 50997, '欧塞斯束环'),
(0, 4, '法师', 128, 51157, '血法护腿'),
(0, 5, '法师', 128, 50062, '天灾科学家长靴'),
(0, 6, '法师', 128, 49994, '亡语者护腕'),
(0, 7, '法师', 128, 51159, '血法手套'),
-- 术士
(1, 0, '术士', 256, 51208, '黑巫兜帽'),
(1, 1, '术士', 256, 51205, '黑巫肩垫'),
(1, 2, '术士', 256, 51206, '黑巫法袍'),
(1, 3, '术士', 256, 51862, '灼烧束带'),
(1, 4, '术士', 256, 51207, '黑巫护腿'),
(1, 5, '术士', 256, 51850, '陈尸长靴'),
(1, 6, '术士', 256, 51872, '药浸护腕'),
(1, 7, '术士', 256, 51209, '黑巫手套'),
-- 牧师
(2, 0, '牧师', 16, 51178, '血色侍僧罩帽'),
(2, 1, '牧师', 16, 51175, '血色侍僧肩垫'),
(2, 2, '牧师', 16, 51176, '血色侍僧法袍'),
(2, 3, '牧师', 16, 51930, '恩医束带'),
(2, 4, '牧师', 16, 51177, '血色侍僧护腿'),
(2, 5, '牧师', 16, 51899, '冰冠尖塔便鞋'),
(2, 6, '牧师', 16, 51918, '黑暗祝福护腕'),
(2, 7, '牧师', 16, 51179, '血色侍僧手套'),
-- 德鲁伊
(3, 0, '德鲁伊', 1024, 51137, '树纹头盔'),
(3, 1, '德鲁伊', 1024, 51135, '树纹肩甲'),
(3, 2, '德鲁伊', 1024, 51139, '树纹长袍'),
(3, 3, '德鲁伊', 1024, 50994, '石化藤条腰带'),
(3, 4, '德鲁伊', 1024, 51136, '树纹腿甲'),
(3, 5, '德鲁伊', 1024, 50009, '异常生长之靴'),
(3, 6, '德鲁伊', 1024, 50417, '无尽梦境护腕'),
(3, 7, '德鲁伊', 1024, 51138, '树纹护手'),
-- 盗贼
(4, 0, '盗贼', 8, 51187, '影刃头盔'),
(4, 1, '盗贼', 8, 51185, '影刃护肩'),
(4, 2, '盗贼', 8, 51189, '影刃甲胄'),
(4, 3, '盗贼', 8, 50995, '复仇腰索'),
(4, 4, '盗贼', 8, 51186, '影刃腿甲'),
(4, 5, '盗贼', 8, 49950, '结霜皮靴'),
(4, 6, '盗贼', 8, 50333, '托斯克腕甲'),
(4, 7, '盗贼', 8, 51188, '影刃护手'),
-- 猎人
(5, 0, '猎人', 4, 51153, '安卡哈猎血头盔'),
(5, 1, '猎人', 4, 51151, '安卡哈猎血护肩'),
(5, 2, '猎人', 4, 51150, '安卡哈猎血外套'),
(5, 3, '猎人', 4, 50413, '尼鲁巴尔束带'),
(5, 4, '猎人', 4, 51152, '安卡哈猎血腿甲'),
(5, 5, '猎人', 4, 51818, '龙翼战靴'),
(5, 6, '猎人', 4, 50000, '天灾猎手臂铠'),
(5, 7, '猎人', 4, 51154, '安卡哈猎血护手'),
-- 萨满
(6, 0, '萨满', 64, 51192, '霜巫头饰'),
(6, 1, '萨满', 64, 51194, '霜巫护肩'),
(6, 2, '萨满', 64, 51190, '霜巫外套'),
(6, 3, '萨满', 64, 51919, '亡语信徒腰带'),
(6, 4, '萨满', 64, 51193, '霜巫腿甲'),
(6, 5, '萨满', 64, 51891, '塔德隆长靴'),
(6, 6, '萨满', 64, 51914, '冰冠城防护腕'),
(6, 7, '萨满', 64, 51191, '霜巫手甲'),
-- 战士
(7, 0, '战士', 1, 51212, '伊米亚之王头盔'),
(7, 1, '战士', 1, 51210, '伊米亚之王肩甲'),
(7, 2, '战士', 1, 51214, '伊米亚之王战甲'),
(7, 3, '战士', 1, 51831, '伊米亚铁索护带'),
(7, 4, '战士', 1, 51211, '伊米亚之王腿甲'),
(7, 5, '战士', 1, 51915, '龙骨漆彩长靴'),
(7, 6, '战士', 1, 51832, '针林护腕'),
(7, 7, '战士', 1, 51213, '伊米亚之王护手'),
-- 死亡骑士
(8, 0, '死亡骑士', 32, 51127, '天灾领主战盔'),
(8, 1, '死亡骑士', 32, 51125, '天灾领主肩铠'),
(8, 2, '死亡骑士', 32, 51129, '天灾领主战甲'),
(8, 3, '死亡骑士', 32, 50187, '冷魂锁链'),
(8, 4, '死亡骑士', 32, 51126, '天灾领主腿铠'),
(8, 5, '死亡骑士', 32, 50190, '狞笑重靴'),
(8, 6, '死亡骑士', 32, 51901, '石像鬼护腕'),
(8, 7, '死亡骑士', 32, 51128, '天灾领主护手'),
-- 圣骑士
(9, 0, '圣骑士', 2, 51162, '光誓战盔'),
(9, 1, '圣骑士', 2, 51160, '光誓肩甲'),
(9, 2, '圣骑士', 2, 51164, '光誓战甲'),
(9, 3, '圣骑士', 2, 50010, '公正之怒腰带'),
(9, 4, '圣骑士', 2, 51161, '光誓护腿'),
(9, 5, '圣骑士', 2, 49905, '生命的守护'),
(9, 6, '圣骑士', 2, 50002, '白熊之爪护腕'),
(9, 7, '圣骑士', 2, 51163, '光誓护手');

DROP TEMPORARY TABLE IF EXISTS `_xianmen_t10_items`;
CREATE TEMPORARY TABLE `_xianmen_t10_items` LIKE `item_template`;
ALTER TABLE `_xianmen_t10_items` DROP PRIMARY KEY;
ALTER TABLE `_xianmen_t10_items`
    ADD COLUMN `_new_entry` INT UNSIGNED NOT NULL,
    ADD COLUMN `_new_name` VARCHAR(255) NOT NULL,
    ADD COLUMN `_new_itemset` INT UNSIGNED NOT NULL,
    ADD COLUMN `_new_class` INT NOT NULL,
    ADD COLUMN `_new_description` VARCHAR(255) NOT NULL,
    ADD COLUMN `_template_entry` INT UNSIGNED NOT NULL,
    ADD COLUMN `_stage_item_level` INT UNSIGNED NOT NULL,
    ADD COLUMN `_stage_stat_value` DECIMAL(65,0) NOT NULL;

INSERT INTO `_xianmen_t10_items`
SELECT t10_template.*,
       stage.`base_entry` + 20 + class_slot.`class_index` * 8 + class_slot.`slot_index` AS `_new_entry`,
       CONCAT(stage.`stage_name`, '·', class_slot.`class_name`, '·', class_slot.`slot_name`) AS `_new_name`,
       stage.`itemset` AS `_new_itemset`,
       class_slot.`class_mask` AS `_new_class`,
       CONCAT(stage.`stage_name`, class_slot.`class_name`, '八件套部件：', class_slot.`slot_index` + 1, '/8。') AS `_new_description`,
       class_slot.`source_entry` AS `_template_entry`,
       stage.`item_level` AS `_stage_item_level`,
       stage.`stat_value` AS `_stage_stat_value`
FROM `_xianmen_t10_stage` stage
JOIN `_xianmen_t10_class_slot` class_slot
JOIN `item_template` t10_template ON t10_template.`entry` = class_slot.`source_entry`;

UPDATE `_xianmen_t10_items` new_item
JOIN `item_template` t10_template ON t10_template.`entry` = new_item.`_template_entry`
SET new_item.`entry` = new_item.`_new_entry`,
    new_item.`class` = t10_template.`class`,
    new_item.`subclass` = t10_template.`subclass`,
    new_item.`SoundOverrideSubclass` = t10_template.`SoundOverrideSubclass`,
    new_item.`name` = new_item.`_new_name`,
    new_item.`displayid` = t10_template.`displayid`,
    new_item.`Quality` = 5,
    new_item.`Flags` = 0,
    new_item.`BuyCount` = 1,
    new_item.`BuyPrice` = 0,
    new_item.`SellPrice` = 0,
    new_item.`InventoryType` = t10_template.`InventoryType`,
    new_item.`AllowableClass` = new_item.`_new_class`,
    new_item.`AllowableRace` = t10_template.`AllowableRace`,
    new_item.`ItemLevel` = new_item.`_stage_item_level`,
    new_item.`RequiredLevel` = 1,
    new_item.`maxcount` = 0,
    new_item.`stackable` = 1,
    new_item.`StatsCount` = 10,
    new_item.`stat_type1` = 4,
    new_item.`stat_value1` = new_item.`_stage_stat_value`,
    new_item.`stat_type2` = 3,
    new_item.`stat_value2` = new_item.`_stage_stat_value`,
    new_item.`stat_type3` = 5,
    new_item.`stat_value3` = new_item.`_stage_stat_value`,
    new_item.`stat_type4` = 6,
    new_item.`stat_value4` = new_item.`_stage_stat_value`,
    new_item.`stat_type5` = 7,
    new_item.`stat_value5` = new_item.`_stage_stat_value`,
    new_item.`stat_type6` = 31,
    new_item.`stat_value6` = new_item.`_stage_stat_value`,
    new_item.`stat_type7` = 36,
    new_item.`stat_value7` = new_item.`_stage_stat_value`,
    new_item.`stat_type8` = 32,
    new_item.`stat_value8` = new_item.`_stage_stat_value`,
    new_item.`stat_type9` = 38,
    new_item.`stat_value9` = new_item.`_stage_stat_value`,
    new_item.`stat_type10` = 45,
    new_item.`stat_value10` = new_item.`_stage_stat_value`,
    new_item.`dmg_min1` = 0,
    new_item.`dmg_max1` = 0,
    new_item.`dmg_type1` = 0,
    new_item.`armor` = new_item.`_stage_stat_value`,
    new_item.`delay` = 1000,
    new_item.`RangedModRange` = 0,
    new_item.`spellid_1` = 0,
    new_item.`spelltrigger_1` = 0,
    new_item.`spellcharges_1` = 0,
    new_item.`spellppmRate_1` = 0,
    new_item.`spellcooldown_1` = -1,
    new_item.`spellcategory_1` = 0,
    new_item.`spellcategorycooldown_1` = -1,
    new_item.`spellid_2` = 0,
    new_item.`spelltrigger_2` = 0,
    new_item.`spellcharges_2` = 0,
    new_item.`spellppmRate_2` = 0,
    new_item.`spellcooldown_2` = -1,
    new_item.`spellcategory_2` = 0,
    new_item.`spellcategorycooldown_2` = -1,
    new_item.`spellid_3` = 0,
    new_item.`spelltrigger_3` = 0,
    new_item.`spellcharges_3` = 0,
    new_item.`spellppmRate_3` = 0,
    new_item.`spellcooldown_3` = -1,
    new_item.`spellcategory_3` = 0,
    new_item.`spellcategorycooldown_3` = -1,
    new_item.`spellid_4` = 0,
    new_item.`spelltrigger_4` = 0,
    new_item.`spellcharges_4` = 0,
    new_item.`spellppmRate_4` = 0,
    new_item.`spellcooldown_4` = -1,
    new_item.`spellcategory_4` = 0,
    new_item.`spellcategorycooldown_4` = -1,
    new_item.`spellid_5` = 0,
    new_item.`spelltrigger_5` = 0,
    new_item.`spellcharges_5` = 0,
    new_item.`spellppmRate_5` = 0,
    new_item.`spellcooldown_5` = -1,
    new_item.`spellcategory_5` = 0,
    new_item.`spellcategorycooldown_5` = -1,
    new_item.`bonding` = 1,
    new_item.`description` = new_item.`_new_description`,
    new_item.`Material` = t10_template.`Material`,
    new_item.`sheath` = t10_template.`sheath`,
    new_item.`itemset` = new_item.`_new_itemset`,
    new_item.`MaxDurability` = 0,
    new_item.`RequiredDisenchantSkill` = -1,
    new_item.`VerifiedBuild` = -1;

ALTER TABLE `_xianmen_t10_items`
    DROP COLUMN `_new_entry`,
    DROP COLUMN `_new_name`,
    DROP COLUMN `_new_itemset`,
    DROP COLUMN `_new_class`,
    DROP COLUMN `_new_description`,
    DROP COLUMN `_template_entry`,
    DROP COLUMN `_stage_item_level`,
    DROP COLUMN `_stage_stat_value`;

INSERT INTO `item_template`
SELECT * FROM `_xianmen_t10_items`
ORDER BY `entry`;

-- 新 T10 职业装备通过 item_template.itemset 绑定已有套装 ID。
-- 自定义 ItemSet.dbc 不绑定具体物品 ID，避免 item_set_names 与 DBC 物品槽不一致。
UPDATE `itemset`
SET `ItemId1` = 0, `ItemId2` = 0, `ItemId3` = 0, `ItemId4` = 0, `ItemId5` = 0,
    `ItemId6` = 0, `ItemId7` = 0, `ItemId8` = 0, `ItemId9` = 0, `ItemId10` = 0,
    `ItemId11` = 0, `ItemId12` = 0, `ItemId13` = 0, `ItemId14` = 0, `ItemId15` = 0,
    `ItemId16` = 0, `ItemId17` = 0
WHERE (`ID` BETWEEN 384101 AND 384103)
   OR (`ID` BETWEEN 386011 AND 386163);

UPDATE `itemset_dbc`
SET `ItemID_1` = 0, `ItemID_2` = 0, `ItemID_3` = 0, `ItemID_4` = 0, `ItemID_5` = 0,
    `ItemID_6` = 0, `ItemID_7` = 0, `ItemID_8` = 0, `ItemID_9` = 0, `ItemID_10` = 0,
    `ItemID_11` = 0, `ItemID_12` = 0, `ItemID_13` = 0, `ItemID_14` = 0, `ItemID_15` = 0,
    `ItemID_16` = 0, `ItemID_17` = 0
WHERE (`ID` BETWEEN 384101 AND 384103)
   OR (`ID` BETWEEN 386011 AND 386163);

DELETE FROM `item_set_names`
WHERE `entry` BETWEEN 95200 AND 98499;

-- 自定义仙门装备不使用耐久，避免 item_instance.durability 持久化越界。
UPDATE `item_template`
SET `MaxDurability` = 0
WHERE `entry` BETWEEN 95100 AND 98499
  AND `MaxDurability` <> 0;

-- 古达克仙门试炼：旧 95200-95218 装备已废弃，5 个 Boss 改为掉落最新 1 万随机池（T10 十职业装备 + 仙器 + 仙饰）。
DELETE FROM `creature_loot_template`
WHERE (`Entry` IN (800001, 800002, 800003, 800004, 800005, 800011, 800012, 800013, 800014, 800015, 800016, 800017, 800018)
       AND `Item` IN (95200, 95201, 95202, 95203, 95204, 95205, 95206, 95207,
                      95208, 95209, 95210, 95211, 95212, 95213, 95214, 95215,
                      95216, 95217, 95218, 95100, 95101, 95102, 95103, 95104,
                      95105, 95106, 95107, 95108, 95109))
   OR (`Entry` IN (800001, 800002, 800003, 800004, 800005, 800011, 800012, 800013, 800014, 800015, 800016, 800017, 800018)
       AND `Reference` = 952200);

DELETE FROM `reference_loot_template`
WHERE `Entry` = 952200;

INSERT INTO `reference_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
SELECT
    952200,
    it.`entry`,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    CONCAT('仙门试炼1万随机池 - ', it.`name`)
FROM `item_template` it
WHERE it.`entry` BETWEEN 95220 AND 95299
   OR it.`entry` BETWEEN 95100 AND 95109
   OR it.`entry` BETWEEN 95208 AND 95212
ORDER BY it.`entry`;

INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(800001, 0, 952200, 100.0, 0, 1, 0, 3, 3, '古达克仙门试炼Boss1随机1万T10职业装备'),
(800002, 0, 952200, 100.0, 0, 1, 0, 3, 3, '古达克仙门试炼Boss2随机1万T10职业装备'),
(800003, 0, 952200, 100.0, 0, 1, 0, 3, 3, '古达克仙门试炼Boss3随机1万T10职业装备'),
(800004, 0, 952200, 100.0, 0, 1, 0, 3, 3, '古达克仙门试炼Boss4随机1万T10职业装备'),
(800005, 0, 952200, 100.0, 0, 1, 0, 3, 3, '古达克仙门试炼Boss5随机1万T10职业装备');

-- 古达克仙门试炼小怪：旧装备掉落清理后，统一掉落灵气石 x2。
DELETE FROM `creature_loot_template`
WHERE `Entry` IN (800011, 800012, 800013, 800014, 800015, 800016, 800017, 800018)
  AND (
       `Item` BETWEEN 95100 AND 98499
    OR `Item` = 62001
    OR `Reference` = 952200
  );

INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(800011, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800012, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800013, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800014, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800015, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800016, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800017, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2'),
(800018, 62001, 0, 100.0, 0, 1, 0, 2, 2, '古达克仙门试炼小怪必掉灵气石x2');

DROP TEMPORARY TABLE IF EXISTS `_xianmen_t10_items`;
DROP TEMPORARY TABLE IF EXISTS `_xianmen_t10_class_slot`;
DROP TEMPORARY TABLE IF EXISTS `_xianmen_t10_stage`;

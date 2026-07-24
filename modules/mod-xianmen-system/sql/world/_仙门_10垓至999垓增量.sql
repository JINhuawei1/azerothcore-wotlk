SET NAMES utf8mb4;

-- Database-first extension. Existing 1万-1垓 rows are never deleted or replaced.
-- Audited free ranges:
-- items 98500-99099, itemsets 387341-387391, spells 386701-386916,
-- requirement/reward templates 952536-952823.

START TRANSACTION;

DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_items`;
CREATE TEMPORARY TABLE `_xianqi_new_items` AS SELECT * FROM `item_template` WHERE 1 = 0;

-- Clone the authoritative 1垓 item rows into one new tier at a time.
INSERT INTO `_xianqi_new_items` SELECT * FROM `item_template` WHERE `entry` BETWEEN 98300 AND 98499;
UPDATE `_xianqi_new_items` SET
  `entry` = `entry` + 200,
  `name` = REPLACE(REPLACE(`name`, '1垓', '10垓'), '垓极', '寂灭'),
  `description` = REPLACE(REPLACE(`description`, '1垓', '10垓'), '垓极', '寂灭'),
  `ItemLevel` = `ItemLevel` + 1,
  `itemset` = IF(`itemset` = 0, 0, `itemset` + 20),
  `stat_value1` = IF(`stat_value1` = 0, 0, 1000000000000000000000),
  `stat_value2` = IF(`stat_value2` = 0, 0, 1000000000000000000000),
  `stat_value3` = IF(`stat_value3` = 0, 0, 1000000000000000000000),
  `stat_value4` = IF(`stat_value4` = 0, 0, 1000000000000000000000),
  `stat_value5` = IF(`stat_value5` = 0, 0, 1000000000000000000000),
  `stat_value6` = IF(`stat_value6` = 0, 0, 1000000000000000000000),
  `stat_value7` = IF(`stat_value7` = 0, 0, 1000000000000000000000),
  `stat_value8` = IF(`stat_value8` = 0, 0, 1000000000000000000000),
  `stat_value9` = IF(`stat_value9` = 0, 0, 1000000000000000000000),
  `stat_value10` = IF(`stat_value10` = 0, 0, 1000000000000000000000),
  `armor` = IF(`armor` = 0, 0, 1000000000000000000000);
INSERT INTO `item_template` SELECT * FROM `_xianqi_new_items`;
DELETE FROM `_xianqi_new_items`;

INSERT INTO `_xianqi_new_items` SELECT * FROM `item_template` WHERE `entry` BETWEEN 98300 AND 98499;
UPDATE `_xianqi_new_items` SET
  `entry` = `entry` + 400,
  `name` = REPLACE(REPLACE(`name`, '1垓', '100垓'), '垓极', '彼岸'),
  `description` = REPLACE(REPLACE(`description`, '1垓', '100垓'), '垓极', '彼岸'),
  `ItemLevel` = `ItemLevel` + 2,
  `itemset` = IF(`itemset` = 0, 0, `itemset` + 40),
  `stat_value1` = IF(`stat_value1` = 0, 0, 10000000000000000000000),
  `stat_value2` = IF(`stat_value2` = 0, 0, 10000000000000000000000),
  `stat_value3` = IF(`stat_value3` = 0, 0, 10000000000000000000000),
  `stat_value4` = IF(`stat_value4` = 0, 0, 10000000000000000000000),
  `stat_value5` = IF(`stat_value5` = 0, 0, 10000000000000000000000),
  `stat_value6` = IF(`stat_value6` = 0, 0, 10000000000000000000000),
  `stat_value7` = IF(`stat_value7` = 0, 0, 10000000000000000000000),
  `stat_value8` = IF(`stat_value8` = 0, 0, 10000000000000000000000),
  `stat_value9` = IF(`stat_value9` = 0, 0, 10000000000000000000000),
  `stat_value10` = IF(`stat_value10` = 0, 0, 10000000000000000000000),
  `armor` = IF(`armor` = 0, 0, 10000000000000000000000);
INSERT INTO `item_template` SELECT * FROM `_xianqi_new_items`;
DELETE FROM `_xianqi_new_items`;

INSERT INTO `_xianqi_new_items` SELECT * FROM `item_template` WHERE `entry` BETWEEN 98300 AND 98499;
UPDATE `_xianqi_new_items` SET
  `entry` = `entry` + 600,
  `name` = REPLACE(REPLACE(`name`, '1垓', '999垓'), '垓极', '归一'),
  `description` = REPLACE(REPLACE(`description`, '1垓', '999垓'), '垓极', '归一'),
  `ItemLevel` = `ItemLevel` + 3,
  `itemset` = IF(`itemset` = 0, 0, `itemset` + 60),
  `stat_value1` = IF(`stat_value1` = 0, 0, 99900000000000000000000),
  `stat_value2` = IF(`stat_value2` = 0, 0, 99900000000000000000000),
  `stat_value3` = IF(`stat_value3` = 0, 0, 99900000000000000000000),
  `stat_value4` = IF(`stat_value4` = 0, 0, 99900000000000000000000),
  `stat_value5` = IF(`stat_value5` = 0, 0, 99900000000000000000000),
  `stat_value6` = IF(`stat_value6` = 0, 0, 99900000000000000000000),
  `stat_value7` = IF(`stat_value7` = 0, 0, 99900000000000000000000),
  `stat_value8` = IF(`stat_value8` = 0, 0, 99900000000000000000000),
  `stat_value9` = IF(`stat_value9` = 0, 0, 99900000000000000000000),
  `stat_value10` = IF(`stat_value10` = 0, 0, 99900000000000000000000),
  `armor` = IF(`armor` = 0, 0, 99900000000000000000000);
INSERT INTO `item_template` SELECT * FROM `_xianqi_new_items`;
DROP TEMPORARY TABLE `_xianqi_new_items`;

-- Clone the authoritative 1垓 set rows and retarget items/spells.
DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_itemsets`;
CREATE TEMPORARY TABLE `_xianqi_new_itemsets` AS SELECT * FROM `itemset` WHERE 1 = 0;

INSERT INTO `_xianqi_new_itemsets` SELECT * FROM `itemset` WHERE `ID` BETWEEN 387321 AND 387331;
UPDATE `_xianqi_new_itemsets` SET
  `ID` = `ID` + 20, `Name5` = REPLACE(`Name5`, '1垓', '10垓'),
  `ItemId1` = IF(`ItemId1` = 0, 0, `ItemId1` + 200), `ItemId2` = IF(`ItemId2` = 0, 0, `ItemId2` + 200),
  `ItemId3` = IF(`ItemId3` = 0, 0, `ItemId3` + 200), `ItemId4` = IF(`ItemId4` = 0, 0, `ItemId4` + 200),
  `ItemId5` = IF(`ItemId5` = 0, 0, `ItemId5` + 200), `ItemId6` = IF(`ItemId6` = 0, 0, `ItemId6` + 200),
  `ItemId7` = IF(`ItemId7` = 0, 0, `ItemId7` + 200), `ItemId8` = IF(`ItemId8` = 0, 0, `ItemId8` + 200),
  `ItemId9` = IF(`ItemId9` = 0, 0, `ItemId9` + 200), `ItemId10` = IF(`ItemId10` = 0, 0, `ItemId10` + 200),
  `ItemId11` = IF(`ItemId11` = 0, 0, `ItemId11` + 200), `ItemId12` = IF(`ItemId12` = 0, 0, `ItemId12` + 200),
  `ItemId13` = IF(`ItemId13` = 0, 0, `ItemId13` + 200), `ItemId14` = IF(`ItemId14` = 0, 0, `ItemId14` + 200),
  `ItemId15` = IF(`ItemId15` = 0, 0, `ItemId15` + 200), `ItemId16` = IF(`ItemId16` = 0, 0, `ItemId16` + 200),
  `ItemId17` = IF(`ItemId17` = 0, 0, `ItemId17` + 200),
  `SetBonus1` = IF(`SetBonus1` = 0, 0, `SetBonus1` + 100), `SetBonus2` = IF(`SetBonus2` = 0, 0, `SetBonus2` + 100),
  `SetBonus3` = IF(`SetBonus3` = 0, 0, `SetBonus3` + 100), `SetBonus4` = IF(`SetBonus4` = 0, 0, `SetBonus4` + 100),
  `SetBonus5` = IF(`SetBonus5` = 0, 0, `SetBonus5` + 100), `SetBonus6` = IF(`SetBonus6` = 0, 0, `SetBonus6` + 100),
  `SetBonus7` = IF(`SetBonus7` = 0, 0, `SetBonus7` + 100), `SetBonus8` = IF(`SetBonus8` = 0, 0, `SetBonus8` + 100);
INSERT INTO `itemset` SELECT * FROM `_xianqi_new_itemsets`;
DELETE FROM `_xianqi_new_itemsets`;

INSERT INTO `_xianqi_new_itemsets` SELECT * FROM `itemset` WHERE `ID` BETWEEN 387321 AND 387331;
UPDATE `_xianqi_new_itemsets` SET
  `ID` = `ID` + 40, `Name5` = REPLACE(`Name5`, '1垓', '100垓'),
  `ItemId1` = IF(`ItemId1` = 0, 0, `ItemId1` + 400), `ItemId2` = IF(`ItemId2` = 0, 0, `ItemId2` + 400),
  `ItemId3` = IF(`ItemId3` = 0, 0, `ItemId3` + 400), `ItemId4` = IF(`ItemId4` = 0, 0, `ItemId4` + 400),
  `ItemId5` = IF(`ItemId5` = 0, 0, `ItemId5` + 400), `ItemId6` = IF(`ItemId6` = 0, 0, `ItemId6` + 400),
  `ItemId7` = IF(`ItemId7` = 0, 0, `ItemId7` + 400), `ItemId8` = IF(`ItemId8` = 0, 0, `ItemId8` + 400),
  `ItemId9` = IF(`ItemId9` = 0, 0, `ItemId9` + 400), `ItemId10` = IF(`ItemId10` = 0, 0, `ItemId10` + 400),
  `ItemId11` = IF(`ItemId11` = 0, 0, `ItemId11` + 400), `ItemId12` = IF(`ItemId12` = 0, 0, `ItemId12` + 400),
  `ItemId13` = IF(`ItemId13` = 0, 0, `ItemId13` + 400), `ItemId14` = IF(`ItemId14` = 0, 0, `ItemId14` + 400),
  `ItemId15` = IF(`ItemId15` = 0, 0, `ItemId15` + 400), `ItemId16` = IF(`ItemId16` = 0, 0, `ItemId16` + 400),
  `ItemId17` = IF(`ItemId17` = 0, 0, `ItemId17` + 400),
  `SetBonus1` = IF(`SetBonus1` = 0, 0, `SetBonus1` + 200), `SetBonus2` = IF(`SetBonus2` = 0, 0, `SetBonus2` + 200),
  `SetBonus3` = IF(`SetBonus3` = 0, 0, `SetBonus3` + 200), `SetBonus4` = IF(`SetBonus4` = 0, 0, `SetBonus4` + 200),
  `SetBonus5` = IF(`SetBonus5` = 0, 0, `SetBonus5` + 200), `SetBonus6` = IF(`SetBonus6` = 0, 0, `SetBonus6` + 200),
  `SetBonus7` = IF(`SetBonus7` = 0, 0, `SetBonus7` + 200), `SetBonus8` = IF(`SetBonus8` = 0, 0, `SetBonus8` + 200);
INSERT INTO `itemset` SELECT * FROM `_xianqi_new_itemsets`;
DELETE FROM `_xianqi_new_itemsets`;

INSERT INTO `_xianqi_new_itemsets` SELECT * FROM `itemset` WHERE `ID` BETWEEN 387321 AND 387331;
UPDATE `_xianqi_new_itemsets` SET
  `ID` = `ID` + 60, `Name5` = REPLACE(`Name5`, '1垓', '999垓'),
  `ItemId1` = IF(`ItemId1` = 0, 0, `ItemId1` + 600), `ItemId2` = IF(`ItemId2` = 0, 0, `ItemId2` + 600),
  `ItemId3` = IF(`ItemId3` = 0, 0, `ItemId3` + 600), `ItemId4` = IF(`ItemId4` = 0, 0, `ItemId4` + 600),
  `ItemId5` = IF(`ItemId5` = 0, 0, `ItemId5` + 600), `ItemId6` = IF(`ItemId6` = 0, 0, `ItemId6` + 600),
  `ItemId7` = IF(`ItemId7` = 0, 0, `ItemId7` + 600), `ItemId8` = IF(`ItemId8` = 0, 0, `ItemId8` + 600),
  `ItemId9` = IF(`ItemId9` = 0, 0, `ItemId9` + 600), `ItemId10` = IF(`ItemId10` = 0, 0, `ItemId10` + 600),
  `ItemId11` = IF(`ItemId11` = 0, 0, `ItemId11` + 600), `ItemId12` = IF(`ItemId12` = 0, 0, `ItemId12` + 600),
  `ItemId13` = IF(`ItemId13` = 0, 0, `ItemId13` + 600), `ItemId14` = IF(`ItemId14` = 0, 0, `ItemId14` + 600),
  `ItemId15` = IF(`ItemId15` = 0, 0, `ItemId15` + 600), `ItemId16` = IF(`ItemId16` = 0, 0, `ItemId16` + 600),
  `ItemId17` = IF(`ItemId17` = 0, 0, `ItemId17` + 600),
  `SetBonus1` = IF(`SetBonus1` = 0, 0, `SetBonus1` + 300), `SetBonus2` = IF(`SetBonus2` = 0, 0, `SetBonus2` + 300),
  `SetBonus3` = IF(`SetBonus3` = 0, 0, `SetBonus3` + 300), `SetBonus4` = IF(`SetBonus4` = 0, 0, `SetBonus4` + 300),
  `SetBonus5` = IF(`SetBonus5` = 0, 0, `SetBonus5` + 300), `SetBonus6` = IF(`SetBonus6` = 0, 0, `SetBonus6` + 300),
  `SetBonus7` = IF(`SetBonus7` = 0, 0, `SetBonus7` + 300), `SetBonus8` = IF(`SetBonus8` = 0, 0, `SetBonus8` + 300);
INSERT INTO `itemset` SELECT * FROM `_xianqi_new_itemsets`;
DROP TEMPORARY TABLE `_xianqi_new_itemsets`;

DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_itemsets_dbc`;
CREATE TEMPORARY TABLE `_xianqi_new_itemsets_dbc` AS SELECT * FROM `itemset_dbc` WHERE 1 = 0;

INSERT INTO `_xianqi_new_itemsets_dbc` SELECT * FROM `itemset_dbc` WHERE `ID` BETWEEN 387321 AND 387331;
UPDATE `_xianqi_new_itemsets_dbc` SET
  `ID` = `ID` + 20, `Name_Lang_deDE` = REPLACE(`Name_Lang_deDE`, '1垓', '10垓'),
  `ItemID_1` = IF(`ItemID_1` = 0, 0, `ItemID_1` + 200), `ItemID_2` = IF(`ItemID_2` = 0, 0, `ItemID_2` + 200),
  `ItemID_3` = IF(`ItemID_3` = 0, 0, `ItemID_3` + 200), `ItemID_4` = IF(`ItemID_4` = 0, 0, `ItemID_4` + 200),
  `ItemID_5` = IF(`ItemID_5` = 0, 0, `ItemID_5` + 200), `ItemID_6` = IF(`ItemID_6` = 0, 0, `ItemID_6` + 200),
  `ItemID_7` = IF(`ItemID_7` = 0, 0, `ItemID_7` + 200), `ItemID_8` = IF(`ItemID_8` = 0, 0, `ItemID_8` + 200),
  `ItemID_9` = IF(`ItemID_9` = 0, 0, `ItemID_9` + 200), `ItemID_10` = IF(`ItemID_10` = 0, 0, `ItemID_10` + 200),
  `ItemID_11` = IF(`ItemID_11` = 0, 0, `ItemID_11` + 200), `ItemID_12` = IF(`ItemID_12` = 0, 0, `ItemID_12` + 200),
  `ItemID_13` = IF(`ItemID_13` = 0, 0, `ItemID_13` + 200), `ItemID_14` = IF(`ItemID_14` = 0, 0, `ItemID_14` + 200),
  `ItemID_15` = IF(`ItemID_15` = 0, 0, `ItemID_15` + 200), `ItemID_16` = IF(`ItemID_16` = 0, 0, `ItemID_16` + 200),
  `ItemID_17` = IF(`ItemID_17` = 0, 0, `ItemID_17` + 200),
  `SetSpellID_1` = IF(`SetSpellID_1` = 0, 0, `SetSpellID_1` + 100), `SetSpellID_2` = IF(`SetSpellID_2` = 0, 0, `SetSpellID_2` + 100),
  `SetSpellID_3` = IF(`SetSpellID_3` = 0, 0, `SetSpellID_3` + 100), `SetSpellID_4` = IF(`SetSpellID_4` = 0, 0, `SetSpellID_4` + 100),
  `SetSpellID_5` = IF(`SetSpellID_5` = 0, 0, `SetSpellID_5` + 100), `SetSpellID_6` = IF(`SetSpellID_6` = 0, 0, `SetSpellID_6` + 100),
  `SetSpellID_7` = IF(`SetSpellID_7` = 0, 0, `SetSpellID_7` + 100), `SetSpellID_8` = IF(`SetSpellID_8` = 0, 0, `SetSpellID_8` + 100);
INSERT INTO `itemset_dbc` SELECT * FROM `_xianqi_new_itemsets_dbc`;
DELETE FROM `_xianqi_new_itemsets_dbc`;

INSERT INTO `_xianqi_new_itemsets_dbc` SELECT * FROM `itemset_dbc` WHERE `ID` BETWEEN 387321 AND 387331;
UPDATE `_xianqi_new_itemsets_dbc` SET
  `ID` = `ID` + 40, `Name_Lang_deDE` = REPLACE(`Name_Lang_deDE`, '1垓', '100垓'),
  `ItemID_1` = IF(`ItemID_1` = 0, 0, `ItemID_1` + 400), `ItemID_2` = IF(`ItemID_2` = 0, 0, `ItemID_2` + 400),
  `ItemID_3` = IF(`ItemID_3` = 0, 0, `ItemID_3` + 400), `ItemID_4` = IF(`ItemID_4` = 0, 0, `ItemID_4` + 400),
  `ItemID_5` = IF(`ItemID_5` = 0, 0, `ItemID_5` + 400), `ItemID_6` = IF(`ItemID_6` = 0, 0, `ItemID_6` + 400),
  `ItemID_7` = IF(`ItemID_7` = 0, 0, `ItemID_7` + 400), `ItemID_8` = IF(`ItemID_8` = 0, 0, `ItemID_8` + 400),
  `ItemID_9` = IF(`ItemID_9` = 0, 0, `ItemID_9` + 400), `ItemID_10` = IF(`ItemID_10` = 0, 0, `ItemID_10` + 400),
  `ItemID_11` = IF(`ItemID_11` = 0, 0, `ItemID_11` + 400), `ItemID_12` = IF(`ItemID_12` = 0, 0, `ItemID_12` + 400),
  `ItemID_13` = IF(`ItemID_13` = 0, 0, `ItemID_13` + 400), `ItemID_14` = IF(`ItemID_14` = 0, 0, `ItemID_14` + 400),
  `ItemID_15` = IF(`ItemID_15` = 0, 0, `ItemID_15` + 400), `ItemID_16` = IF(`ItemID_16` = 0, 0, `ItemID_16` + 400),
  `ItemID_17` = IF(`ItemID_17` = 0, 0, `ItemID_17` + 400),
  `SetSpellID_1` = IF(`SetSpellID_1` = 0, 0, `SetSpellID_1` + 200), `SetSpellID_2` = IF(`SetSpellID_2` = 0, 0, `SetSpellID_2` + 200),
  `SetSpellID_3` = IF(`SetSpellID_3` = 0, 0, `SetSpellID_3` + 200), `SetSpellID_4` = IF(`SetSpellID_4` = 0, 0, `SetSpellID_4` + 200),
  `SetSpellID_5` = IF(`SetSpellID_5` = 0, 0, `SetSpellID_5` + 200), `SetSpellID_6` = IF(`SetSpellID_6` = 0, 0, `SetSpellID_6` + 200),
  `SetSpellID_7` = IF(`SetSpellID_7` = 0, 0, `SetSpellID_7` + 200), `SetSpellID_8` = IF(`SetSpellID_8` = 0, 0, `SetSpellID_8` + 200);
INSERT INTO `itemset_dbc` SELECT * FROM `_xianqi_new_itemsets_dbc`;
DELETE FROM `_xianqi_new_itemsets_dbc`;

INSERT INTO `_xianqi_new_itemsets_dbc` SELECT * FROM `itemset_dbc` WHERE `ID` BETWEEN 387321 AND 387331;
UPDATE `_xianqi_new_itemsets_dbc` SET
  `ID` = `ID` + 60, `Name_Lang_deDE` = REPLACE(`Name_Lang_deDE`, '1垓', '999垓'),
  `ItemID_1` = IF(`ItemID_1` = 0, 0, `ItemID_1` + 600), `ItemID_2` = IF(`ItemID_2` = 0, 0, `ItemID_2` + 600),
  `ItemID_3` = IF(`ItemID_3` = 0, 0, `ItemID_3` + 600), `ItemID_4` = IF(`ItemID_4` = 0, 0, `ItemID_4` + 600),
  `ItemID_5` = IF(`ItemID_5` = 0, 0, `ItemID_5` + 600), `ItemID_6` = IF(`ItemID_6` = 0, 0, `ItemID_6` + 600),
  `ItemID_7` = IF(`ItemID_7` = 0, 0, `ItemID_7` + 600), `ItemID_8` = IF(`ItemID_8` = 0, 0, `ItemID_8` + 600),
  `ItemID_9` = IF(`ItemID_9` = 0, 0, `ItemID_9` + 600), `ItemID_10` = IF(`ItemID_10` = 0, 0, `ItemID_10` + 600),
  `ItemID_11` = IF(`ItemID_11` = 0, 0, `ItemID_11` + 600), `ItemID_12` = IF(`ItemID_12` = 0, 0, `ItemID_12` + 600),
  `ItemID_13` = IF(`ItemID_13` = 0, 0, `ItemID_13` + 600), `ItemID_14` = IF(`ItemID_14` = 0, 0, `ItemID_14` + 600),
  `ItemID_15` = IF(`ItemID_15` = 0, 0, `ItemID_15` + 600), `ItemID_16` = IF(`ItemID_16` = 0, 0, `ItemID_16` + 600),
  `ItemID_17` = IF(`ItemID_17` = 0, 0, `ItemID_17` + 600),
  `SetSpellID_1` = IF(`SetSpellID_1` = 0, 0, `SetSpellID_1` + 300), `SetSpellID_2` = IF(`SetSpellID_2` = 0, 0, `SetSpellID_2` + 300),
  `SetSpellID_3` = IF(`SetSpellID_3` = 0, 0, `SetSpellID_3` + 300), `SetSpellID_4` = IF(`SetSpellID_4` = 0, 0, `SetSpellID_4` + 300),
  `SetSpellID_5` = IF(`SetSpellID_5` = 0, 0, `SetSpellID_5` + 300), `SetSpellID_6` = IF(`SetSpellID_6` = 0, 0, `SetSpellID_6` + 300),
  `SetSpellID_7` = IF(`SetSpellID_7` = 0, 0, `SetSpellID_7` + 300), `SetSpellID_8` = IF(`SetSpellID_8` = 0, 0, `SetSpellID_8` + 300);
INSERT INTO `itemset_dbc` SELECT * FROM `_xianqi_new_itemsets_dbc`;
DROP TEMPORARY TABLE `_xianqi_new_itemsets_dbc`;

-- Keep the server-side set-name mirror and artifact registration in sync.
DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_set_names`;
CREATE TEMPORARY TABLE `_xianqi_new_set_names` AS SELECT * FROM `item_set_names` WHERE 1 = 0;
INSERT INTO `_xianqi_new_set_names` SELECT * FROM `item_set_names` WHERE `entry` BETWEEN 98300 AND 98499;
UPDATE `_xianqi_new_set_names` SET `entry` = `entry` + 200, `name` = REPLACE(REPLACE(`name`, '1垓', '10垓'), '垓极', '寂灭');
INSERT INTO `item_set_names` SELECT * FROM `_xianqi_new_set_names`;
DELETE FROM `_xianqi_new_set_names`;
INSERT INTO `_xianqi_new_set_names` SELECT * FROM `item_set_names` WHERE `entry` BETWEEN 98300 AND 98499;
UPDATE `_xianqi_new_set_names` SET `entry` = `entry` + 400, `name` = REPLACE(REPLACE(`name`, '1垓', '100垓'), '垓极', '彼岸');
INSERT INTO `item_set_names` SELECT * FROM `_xianqi_new_set_names`;
DELETE FROM `_xianqi_new_set_names`;
INSERT INTO `_xianqi_new_set_names` SELECT * FROM `item_set_names` WHERE `entry` BETWEEN 98300 AND 98499;
UPDATE `_xianqi_new_set_names` SET `entry` = `entry` + 600, `name` = REPLACE(REPLACE(`name`, '1垓', '999垓'), '垓极', '归一');
INSERT INTO `item_set_names` SELECT * FROM `_xianqi_new_set_names`;
DROP TEMPORARY TABLE `_xianqi_new_set_names`;

DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_artifacts`;
CREATE TEMPORARY TABLE `_xianqi_new_artifacts` AS SELECT * FROM `_仙门_仙器物品` WHERE 1 = 0;
INSERT INTO `_xianqi_new_artifacts` SELECT * FROM `_仙门_仙器物品` WHERE `物品ID` BETWEEN 98300 AND 98309;
UPDATE `_xianqi_new_artifacts` SET `物品ID` = `物品ID` + 200, `备注` = REPLACE(`备注`, '1垓', '10垓');
INSERT INTO `_仙门_仙器物品` SELECT * FROM `_xianqi_new_artifacts`;
DELETE FROM `_xianqi_new_artifacts`;
INSERT INTO `_xianqi_new_artifacts` SELECT * FROM `_仙门_仙器物品` WHERE `物品ID` BETWEEN 98300 AND 98309;
UPDATE `_xianqi_new_artifacts` SET `物品ID` = `物品ID` + 400, `备注` = REPLACE(`备注`, '1垓', '100垓');
INSERT INTO `_仙门_仙器物品` SELECT * FROM `_xianqi_new_artifacts`;
DELETE FROM `_xianqi_new_artifacts`;
INSERT INTO `_xianqi_new_artifacts` SELECT * FROM `_仙门_仙器物品` WHERE `物品ID` BETWEEN 98300 AND 98309;
UPDATE `_xianqi_new_artifacts` SET `物品ID` = `物品ID` + 600, `备注` = REPLACE(`备注`, '1垓', '999垓');
INSERT INTO `_仙门_仙器物品` SELECT * FROM `_xianqi_new_artifacts`;
DROP TEMPORARY TABLE `_xianqi_new_artifacts`;

-- Build only the three new synthesis segments from the rows now present in the database.
DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_stages`;
CREATE TEMPORARY TABLE `_xianqi_new_stages` (
  `stage_index` tinyint UNSIGNED NOT NULL,
  `upgrade_level` tinyint UNSIGNED NOT NULL,
  `artifact_base` int UNSIGNED NOT NULL,
  `gear_base` int UNSIGNED NOT NULL,
  `source_min` int UNSIGNED NOT NULL,
  `source_max` int UNSIGNED NOT NULL,
  PRIMARY KEY (`stage_index`)
) ENGINE=MEMORY;
INSERT INTO `_xianqi_new_stages` VALUES
  (16, 17, 98300, 98400, 98300, 98499),
  (17, 18, 98500, 98600, 98500, 98699),
  (18, 19, 98700, 98800, 98700, 98899);

DROP TEMPORARY TABLE IF EXISTS `_xianqi_new_recipes`;
CREATE TEMPORARY TABLE `_xianqi_new_recipes` AS
SELECT
  951000 + stage.`stage_index` * 96 +
  CASE
    WHEN source_item.`entry` BETWEEN stage.`artifact_base` AND stage.`artifact_base` + 9
      THEN source_item.`entry` - stage.`artifact_base`
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 20 AND 99
      THEN 10 + source_item.`entry` - stage.`gear_base` - 20
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 8 AND 12
      THEN 90 + source_item.`entry` - stage.`gear_base` - 8
    WHEN source_item.`entry` - stage.`gear_base` = 17 THEN 95
  END AS `template_id`,
  stage.`upgrade_level`,
  CASE
    WHEN source_item.`entry` < stage.`gear_base` THEN 0
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 76 AND 83 THEN 1
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 20 AND 27 THEN 2
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 36 AND 43 THEN 3
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 52 AND 59 THEN 4
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 28 AND 35 THEN 5
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 60 AND 67 THEN 6
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 68 AND 75 THEN 7
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 44 AND 51 THEN 8
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 92 AND 99 THEN 9
    WHEN source_item.`entry` - stage.`gear_base` BETWEEN 84 AND 91 THEN 10
    ELSE 0
  END AS `class_type`,
  source_item.`entry` AS `source_entry`,
  target_item.`entry` AS `target_entry`
FROM `_xianqi_new_stages` stage
JOIN `item_template` source_item ON source_item.`entry` BETWEEN stage.`source_min` AND stage.`source_max`
JOIN `item_template` target_item ON target_item.`entry` = source_item.`entry` + 200;

INSERT INTO `_模板_需求` (`注释`, `id`, `需要人物等级`, `是否消耗物品`, `消耗物品`, `客户端显示`)
SELECT CONCAT('仙门装备合成需求：', source_item.`name`, ' → ', target_item.`name`), recipe.`template_id`, '1', 0,
       CONCAT(recipe.`source_entry`, ' 2,62001 500,62002 100'),
       CONCAT('消耗 ', source_item.`name`, ' x2、灵气石 x500、突破石 x100')
FROM `_xianqi_new_recipes` recipe
JOIN `item_template` source_item ON source_item.`entry` = recipe.`source_entry`
JOIN `item_template` target_item ON target_item.`entry` = recipe.`target_entry`
ORDER BY recipe.`template_id`;

INSERT INTO `_模板_奖励` (`id`, `注释`, `几率`, `奖励物品`, `客户端显示`)
SELECT recipe.`template_id`, CONCAT('仙门装备合成奖励：', target_item.`name`), 100,
       CONCAT(recipe.`target_entry`, ' 1'), CONCAT('获得 ', target_item.`name`)
FROM `_xianqi_new_recipes` recipe
JOIN `item_template` target_item ON target_item.`entry` = recipe.`target_entry`
ORDER BY recipe.`template_id`;

INSERT INTO `_物品合成`
(`物品id`, `升级等级`, `职业类型`, `需求id`, `升级成功奖励id`, `成功几率`, `合成几率物品id`, `合成几率提升`, `失败是否摧毁`, `解锁穿戴等级`)
SELECT `source_entry`, `upgrade_level`, `class_type`, `template_id`, `template_id`, 100, 0, 0, 0, `upgrade_level` + 1
FROM `_xianqi_new_recipes`
ORDER BY `template_id`;

-- Add only new wear-control rows. Existing slot permissions stay untouched.
INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT x.`物品ID`, COALESCE(it.`InventoryType`, 0), x.`仙器槽位ID`, 0,
       FLOOR((x.`物品ID` - 95100) / 200) + 1, 2,
       CONCAT('仙器专属：', COALESCE(NULLIF(x.`备注`, ''), it.`name`, CAST(x.`物品ID` AS CHAR)))
FROM `_仙门_仙器物品` x
LEFT JOIN `item_template` it ON it.`entry` = x.`物品ID`
WHERE x.`物品ID` BETWEEN 98500 AND 99099 AND x.`仙器槽位ID` BETWEEN 1 AND 10;

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`,
  CASE it.`InventoryType`
    WHEN 1 THEN 11 WHEN 2 THEN 12 WHEN 3 THEN 13 WHEN 4 THEN 14 WHEN 5 THEN 15 WHEN 20 THEN 15
    WHEN 6 THEN 16 WHEN 7 THEN 17 WHEN 8 THEN 18 WHEN 9 THEN 19 WHEN 10 THEN 20
    WHEN 16 THEN 25 WHEN 15 THEN 28 WHEN 25 THEN 28 WHEN 26 THEN 28 WHEN 28 THEN 28 WHEN 19 THEN 29
  END,
  0, FLOOR((it.`entry` - 95100) / 200) + 1, 2, CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
WHERE it.`entry` BETWEEN 98500 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` IN (1, 2, 3, 4, 5, 20, 6, 7, 8, 9, 10, 16, 15, 25, 26, 28, 19);

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, FLOOR((it.`entry` - 95100) / 200) + 1, 2,
       CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
JOIN (SELECT 21 AS `slot_id` UNION ALL SELECT 22) slots
WHERE it.`entry` BETWEEN 98500 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` = 11;

INSERT INTO `_穿戴控制表`
(`物品id`, `穿戴位置`, `槽位位置`, `穿戴需求`, `穿戴等级`, `限制类型`, `注释`)
SELECT it.`entry`, it.`InventoryType`, slots.`slot_id`, 0, FLOOR((it.`entry` - 95100) / 200) + 1, 2,
       CONCAT('仙器系统扩展专属：', it.`name`)
FROM `item_template` it
JOIN (SELECT 23 AS `slot_id` UNION ALL SELECT 24) slots
WHERE it.`entry` BETWEEN 98500 AND 99099
  AND NOT EXISTS (SELECT 1 FROM `_仙门_仙器物品` x WHERE x.`物品ID` = it.`entry`)
  AND it.`InventoryType` = 12;

-- Last generated template id must be 952823 (stage 18, sequence 95).
DROP TEMPORARY TABLE `_xianqi_new_recipes`;
DROP TEMPORARY TABLE `_xianqi_new_stages`;
COMMIT;

-- DB update 2026_03_19_00 -> 2026_03_19_01
--
UPDATE `item_template`
SET `spellid_1` = 0,
    `spelltrigger_1` = 0,
    `spellcharges_1` = 0,
    `spellppmRate_1` = 0,
    `spellcooldown_1` = -1,
    `spellcategory_1` = 0,
    `spellcategorycooldown_1` = -1
WHERE `entry` = 900012
  AND `spellid_1` = 6649;

UPDATE `quest_template`
SET `RewardChoiceItemID3` = 20810
WHERE `ID` IN (8507, 8731)
  AND `RewardChoiceItemID3` = 20809;

UPDATE `quest_template`
SET `RewardItem2` = 20810
WHERE `ID` IN (8498, 8535, 8537, 8739)
  AND `RewardItem2` = 20809;

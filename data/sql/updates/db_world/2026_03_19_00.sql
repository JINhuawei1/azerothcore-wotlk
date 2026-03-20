-- DB update 2025_05_04_04 -> 2026_03_19_00
--
DELETE FROM `creature_template_model` WHERE `CreatureID` = 60003;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(60003, 0, 30721, 1.3, 1, NULL);

UPDATE `creature_template_model`
SET `Probability` = 1
WHERE `CreatureID` IN (60008, 60009, 60022, 60023, 60024, 60025, 60026, 60027, 60029, 60030, 60031)
  AND `Idx` = 0
  AND `Probability` = 0;

UPDATE `creature_template_model`
SET `CreatureDisplayID` = 30363,
    `Probability` = 1
WHERE `CreatureID` = 60028
  AND `Idx` = 0;

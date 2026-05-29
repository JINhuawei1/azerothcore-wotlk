-- DB update 2026_03_19_03 -> 2026_05_25_00
--
UPDATE `creature_template`
SET `AIName` = ''
WHERE `entry` IN (23845, 23852, 23853, 23854, 23855)
  AND `AIName` = 'SmartAI';

DELETE FROM `conditions`
WHERE `SourceTypeOrReferenceId` = 22
  AND `SourceGroup` = 12
  AND `SourceEntry` = 19271
  AND `SourceId` = 0
  AND `ElseGroup` = 0
  AND `ConditionTypeOrReference` = 12
  AND `ConditionTarget` = 1
  AND `ConditionValue1` = 86;

UPDATE `smart_scripts`
SET `link` = 0
WHERE `entryorguid` = 19271
  AND `source_type` = 0
  AND `id` = 10
  AND `link` = 11;

DELETE FROM `smart_scripts`
WHERE `entryorguid` = 19271
  AND `source_type` = 0
  AND `id` = 11;

DELETE FROM `spell_script_names`
WHERE `spell_id` = 371004
  AND `ScriptName` = 'spell_cultivation_yuanying';

INSERT INTO `creature_addon` (`guid`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`) VALUES
(82897, 827200, 0, 0, 1, 0, 0, '')
ON DUPLICATE KEY UPDATE `path_id` = VALUES(`path_id`);

DELETE FROM `gameobject_addon` WHERE `guid` = 65588;
DELETE FROM `game_event_gameobject` WHERE `guid` = 65588;
DELETE FROM `pool_gameobject` WHERE `guid` = 65588;
DELETE FROM `gameobject` WHERE `guid` = 65588 AND `id` = 192517;

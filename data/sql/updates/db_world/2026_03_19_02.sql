-- DB update 2026_03_19_01 -> 2026_03_19_02
--
UPDATE `item_template`
SET `ScriptName` = ''
WHERE `entry` = 700010
  AND `ScriptName` = 'teleport_system_gossip_item';

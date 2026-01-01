-- =====================================================
-- 技能综合大师模块 - NPC定义
-- =====================================================
--
-- 使用说明：
-- 1. 本模块支持任何绑定了 ScriptName='npc_skill_master' 的NPC
-- 2. NPC必须设置 npcflag = 17 (GOSSIP + TRAINER)
-- 3. 你可以创建多个不同Entry的NPC，只要绑定相同的脚本即可
--
-- =====================================================

-- 删除已存在的NPC (默认示例NPC)
DELETE FROM `creature_template` WHERE `entry` = 60002;

-- 创建技能综合大师NPC (示例)
-- 你可以复制此模板创建不同外观的技能大师NPC
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(60002, 0, 0, 0, 0, 0, '技能综合大师', '职业技能|武器技能|骑术', 'Train', 0, 80, 80, 2, 35, 17, 1, 1.14286, 1, 1, 20, 1.5, 0, 0, 1, 2000, 2000, 1, 1, 1, 2, 2048, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 10, 1, 1, 1, 0, 0, 1, 0, 0, 2, 'npc_skill_master', NULL);

-- =====================================================
-- 关键字段说明：
-- npcflag = 17 表示:
--   UNIT_NPC_FLAG_GOSSIP (1) - 允许对话菜单
--   UNIT_NPC_FLAG_TRAINER (16) - 允许训练师功能
--   1 + 16 = 17
--
-- ScriptName = 'npc_skill_master' - 绑定到技能大师脚本
-- =====================================================

-- 删除已存在的模型数据
DELETE FROM `creature_template_model` WHERE `CreatureID` = 60002;

-- 添加NPC模型 (可自定义外观)
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(60002, 0, 3343, 1, 1, NULL);

-- 删除已存在的附加数据
DELETE FROM `creature_template_addon` WHERE `entry` = 60002;

-- 添加NPC附加数据
INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`) VALUES
(60002, 0, 0, 0, 1, 0, 0, NULL);

-- =====================================================
-- 如何将现有NPC改为技能大师：
-- =====================================================
-- UPDATE `creature_template` SET
--     `npcflag` = `npcflag` | 17,
--     `ScriptName` = 'npc_skill_master'
-- WHERE `entry` = YOUR_NPC_ENTRY;
-- =====================================================

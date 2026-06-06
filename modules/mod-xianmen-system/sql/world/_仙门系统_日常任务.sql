-- ============================================
-- 仙门系统 - 原生日常任务 NPC / 任务 / 奖励物品
-- 号段:
--   NPC: 383000
--   日常奖励: 62001 x1000, 62002 x50
--   贡献令物品: 383001
--   天劫目标: 390101-390200, 每个日常对应同序号天劫, 击杀 2 次
--   日常任务池: 383100 (每日随机激活 20 个)
--   日常任务: 383101-383200
-- ============================================

SET NAMES utf8mb4;

SET @XIANMEN_DAILY_NPC := 383000;
SET @XIANMEN_DAILY_REWARD_ITEM := 383001;
SET @XIANMEN_DAILY_DIRECT_REWARD_ITEM_1 := 62001;
SET @XIANMEN_DAILY_DIRECT_REWARD_AMOUNT_1 := 1000;
SET @XIANMEN_DAILY_DIRECT_REWARD_ITEM_2 := 62002;
SET @XIANMEN_DAILY_DIRECT_REWARD_AMOUNT_2 := 50;
SET @XIANMEN_DAILY_QUEST_BASE := 383100;
SET @XIANMEN_DAILY_POOL := 383100;
SET @XIANMEN_DAILY_ACTIVE_LIMIT := 20;
SET @XIANMEN_DAILY_REWARD_CONTRIBUTION := 100;
SET @XIANMEN_DAILY_TARGET_BASE := 390100;
SET @XIANMEN_DAILY_TARGET_KILL_COUNT := 2;

DROP TEMPORARY TABLE IF EXISTS `_tmp_xianmen_daily_numbers`;
CREATE TEMPORARY TABLE `_tmp_xianmen_daily_numbers` (
  `n` int UNSIGNED NOT NULL,
  PRIMARY KEY (`n`) USING BTREE
) ENGINE = MEMORY;

INSERT INTO `_tmp_xianmen_daily_numbers` (`n`)
SELECT d0.`n` + d1.`n` * 10 + 1 AS `n`
FROM
  (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
   UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d0
CROSS JOIN
  (SELECT 0 AS `n` UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
   UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d1
ORDER BY `n`;

DELETE FROM `creature_queststarter`
WHERE `id` = @XIANMEN_DAILY_NPC
  AND `quest` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `creature_questender`
WHERE `id` = @XIANMEN_DAILY_NPC
  AND `quest` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `pool_quest`
WHERE `entry` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100)
   OR `pool_entry` = @XIANMEN_DAILY_POOL;

DELETE FROM `pool_template`
WHERE `entry` = @XIANMEN_DAILY_POOL;

DELETE FROM `quest_offer_reward`
WHERE `ID` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `quest_request_items`
WHERE `ID` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `quest_details`
WHERE `ID` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `quest_template_addon`
WHERE `ID` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `quest_template`
WHERE `ID` BETWEEN (@XIANMEN_DAILY_QUEST_BASE + 1) AND (@XIANMEN_DAILY_QUEST_BASE + 100);

DELETE FROM `creature_template_model`
WHERE `CreatureID` = @XIANMEN_DAILY_NPC;

DELETE FROM `creature_template`
WHERE `entry` = @XIANMEN_DAILY_NPC;

DELETE FROM `item_template`
WHERE `entry` = @XIANMEN_DAILY_REWARD_ITEM;

CREATE TABLE IF NOT EXISTS `_物品_使用获得`  (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL COMMENT '物品描述',
  `entry` mediumint UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品ID',
  `物品使用奖励` int NOT NULL DEFAULT 0 COMMENT '物品使用奖励（对应_模板_奖励.id）',
  `GM命令` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL COMMENT '执行GM命令,格式 .add 25',
  `消耗物品` tinyint NOT NULL DEFAULT 1 COMMENT '是否消耗物品(1=是,0=否)',
  PRIMARY KEY (`entry`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Item Use Reward System' ROW_FORMAT = FIXED;

DELETE FROM `_物品_使用获得`
WHERE `entry` = @XIANMEN_DAILY_REWARD_ITEM;

INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
 `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`,
 `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `StatsCount`,
 `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,
 `bonding`, `description`, `Material`, `VerifiedBuild`)
VALUES
(@XIANMEN_DAILY_REWARD_ITEM, 0, 0, -1, '宗门贡献令', 6418, 3, 0, 0,
 1, 0, 0, 0, -1, -1,
 80, 1, 0, 1000, 0,
 25347, 0, 0, 0, -1, 0, -1,
 1, CONCAT('使用后获得 ', @XIANMEN_DAILY_REWARD_CONTRIBUTION, ' 点宗门贡献。'), 4, 12340);

INSERT INTO `_物品_使用获得`
(`注释`, `entry`, `物品使用奖励`, `GM命令`, `消耗物品`)
VALUES
(CONCAT('宗门贡献令：使用后增加 ', @XIANMEN_DAILY_REWARD_CONTRIBUTION, ' 点宗门贡献'),
 @XIANMEN_DAILY_REWARD_ITEM, 0, CONCAT('.仙门 加贡献 ', @XIANMEN_DAILY_REWARD_CONTRIBUTION), 1);

INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `IconName`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`,
 `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`,
 `type`, `type_flags`, `lootid`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`,
 `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RegenHealth`,
 `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(@XIANMEN_DAILY_NPC, '仙门日常使者', '宗门日常委托', 'Quest', 80, 80, 2, 35, 2,
 1, 1.14286, 20, 1.1, 0, 0, 1,
 2000, 2000, 1, 1, 1, 2, 2048, 0,
 7, 0, 0, 0, 0, '', 0, 1,
 100, 1, 1, 1, 1,
 0, 0, 2, '', 12340);

INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(@XIANMEN_DAILY_NPC, 0, 3343, 1.0, 1.0, 12340);

INSERT INTO `quest_template`
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `QuestInfoID`, `SuggestedGroupNum`,
 `RequiredNpcOrGo1`, `RequiredNpcOrGo2`, `RequiredNpcOrGo3`, `RequiredNpcOrGo4`,
 `RequiredNpcOrGoCount1`, `RequiredNpcOrGoCount2`, `RequiredNpcOrGoCount3`, `RequiredNpcOrGoCount4`,
 `RewardItem1`, `RewardAmount1`, `RewardItem2`, `RewardAmount2`, `Flags`,
 `ObjectiveText1`, `ObjectiveText2`, `ObjectiveText3`, `ObjectiveText4`,
 `LogTitle`, `LogDescription`, `QuestDescription`, `QuestCompletionLog`, `VerifiedBuild`)
SELECT
  @XIANMEN_DAILY_QUEST_BASE + n.`n`,
  2,
  80,
  1,
  0,
  0,
  0,
  @XIANMEN_DAILY_TARGET_BASE + n.`n`,
  0,
  0,
  0,
  @XIANMEN_DAILY_TARGET_KILL_COUNT,
  0,
  0,
  0,
  @XIANMEN_DAILY_DIRECT_REWARD_ITEM_1,
  @XIANMEN_DAILY_DIRECT_REWARD_AMOUNT_1,
  @XIANMEN_DAILY_DIRECT_REWARD_ITEM_2,
  @XIANMEN_DAILY_DIRECT_REWARD_AMOUNT_2,
  4096,
  CONCAT('击败天劫·第 ', n.`n`, ' 重'),
  '',
  '',
  '',
  CONCAT('宗门日常·',
    CASE ((n.`n` - 1) % 10) + 1
      WHEN 1 THEN '巡山问道'
      WHEN 2 THEN '清修打坐'
      WHEN 3 THEN '灵气采撷'
      WHEN 4 THEN '护送香火'
      WHEN 5 THEN '清剿邪祟'
      WHEN 6 THEN '抄录典籍'
      WHEN 7 THEN '炼丹备料'
      WHEN 8 THEN '剑阵演练'
      WHEN 9 THEN '符箓校验'
      ELSE '秘境巡查'
    END,
    '（', LPAD(n.`n`, 3, '0'), '）'),
  '完成宗门日常委托，击败指定天劫 2 次，领取灵气石与突破石。',
  CONCAT('今日宗门事务繁多，请击败第 ', n.`n`, ' 重天劫 2 次。完成后可获得灵气石与突破石。'),
  '返回仙门日常使者领取灵气石与突破石。',
  12340
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `quest_template_addon`
(`ID`, `SpecialFlags`)
SELECT
  @XIANMEN_DAILY_QUEST_BASE + n.`n`,
  1
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `quest_details`
(`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `VerifiedBuild`)
SELECT
  @XIANMEN_DAILY_QUEST_BASE + n.`n`,
  1, 0, 0, 0,
  0, 0, 0, 0,
  12340
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `quest_request_items`
(`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`, `VerifiedBuild`)
SELECT
  @XIANMEN_DAILY_QUEST_BASE + n.`n`,
  1,
  1,
  '宗门日常已经完成，可以领取今日灵气石与突破石。',
  12340
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `quest_offer_reward`
(`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `RewardText`, `VerifiedBuild`)
SELECT
  @XIANMEN_DAILY_QUEST_BASE + n.`n`,
  1, 0, 0, 0,
  0, 0, 0, 0,
  '今日宗门事务已记入功簿。收下这些灵气石与突破石，继续精进修行。',
  12340
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `creature_queststarter`
(`id`, `quest`)
SELECT
  @XIANMEN_DAILY_NPC,
  @XIANMEN_DAILY_QUEST_BASE + n.`n`
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `creature_questender`
(`id`, `quest`)
SELECT
  @XIANMEN_DAILY_NPC,
  @XIANMEN_DAILY_QUEST_BASE + n.`n`
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

INSERT INTO `pool_template`
(`entry`, `max_limit`, `description`)
VALUES
(@XIANMEN_DAILY_POOL, @XIANMEN_DAILY_ACTIVE_LIMIT, '仙门日常任务池：100 个模板每日随机显示 20 个');

INSERT INTO `pool_quest`
(`entry`, `pool_entry`, `description`)
SELECT
  @XIANMEN_DAILY_QUEST_BASE + n.`n`,
  @XIANMEN_DAILY_POOL,
  CONCAT('仙门日常任务池成员 ', LPAD(n.`n`, 3, '0'))
FROM `_tmp_xianmen_daily_numbers` n
ORDER BY n.`n`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_xianmen_daily_numbers`;

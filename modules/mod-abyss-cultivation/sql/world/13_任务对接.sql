-- ============================================
-- 首版 quest_template 对接：由 _深渊任务模板对接 批量生成
-- 说明：先落最小可用任务链，后续再补奖励、目标、阵营限制等细节
-- ============================================

DELETE FROM `quest_offer_reward` WHERE `ID` BETWEEN 700001 AND 740074;
DELETE FROM `quest_request_items` WHERE `ID` BETWEEN 700001 AND 740074;
DELETE FROM `quest_details` WHERE `ID` BETWEEN 700001 AND 740074;
DELETE FROM `quest_template_addon` WHERE `ID` BETWEEN 700001 AND 740074;
DELETE FROM `quest_template` WHERE `ID` BETWEEN 700001 AND 740074;

REPLACE INTO `quest_template`
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `QuestInfoID`, `SuggestedGroupNum`, `TimeAllowed`, `AllowableRaces`,
 `RequiredFactionId1`, `RequiredFactionId2`, `RequiredFactionValue1`, `RequiredFactionValue2`,
 `RewardNextQuest`, `RewardXPDifficulty`, `RewardMoney`, `RewardMoneyDifficulty`, `RewardDisplaySpell`, `RewardSpell`, `RewardHonor`, `RewardKillHonor`,
 `StartItem`, `Flags`, `RewardTitle`, `RequiredPlayerKills`, `RewardTalents`, `RewardArenaPoints`,
 `LogTitle`, `LogDescription`, `QuestDescription`, `AreaDescription`, `QuestCompletionLog`)
SELECT
  q.`任务ID`,
  2,
  q.`建议任务等级`,
  q.`建议最小等级`,
  q.`建议任务分类`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  q.`任务标题`,
  CONCAT('深渊章节任务·', c.`章节名称`),
  CONCAT(
    CASE q.`任务类型`
      WHEN 1 THEN '起始引导'
      WHEN 2 THEN '官方通关'
      WHEN 3 THEN '正传通关'
      WHEN 4 THEN '深渊通关'
      WHEN 5 THEN '腐化通关'
      ELSE '推进'
    END,
    '【', c.`章节名称`, '】'
  ),
  c.`章节名称`,
  CONCAT('完成任务：', q.`任务标题`)
FROM `_深渊任务模板对接` q
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
WHERE q.`任务ID` BETWEEN 700001 AND 740074;

REPLACE INTO `quest_template_addon`
(`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`,
 `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`,
 `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`,
 `ProvidedItemCount`, `SpecialFlags`)
SELECT
  `任务ID`,
  0,
  0,
  0,
  `上一任务ID`,
  `下一任务ID`,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
FROM `_深渊任务模板对接`
WHERE `任务ID` BETWEEN 700001 AND 740074;

REPLACE INTO `quest_details`
(`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`)
SELECT
  `任务ID`,
  1,
  0,
  0,
  0,
  0,
  0,
  0,
  0
FROM `_深渊任务模板对接`
WHERE `任务ID` BETWEEN 700001 AND 740074;

REPLACE INTO `quest_request_items`
(`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`)
SELECT
  q.`任务ID`,
  1,
  1,
  CONCAT(
    CASE q.`任务类型`
      WHEN 1 THEN '起始引导已完成'
      WHEN 2 THEN '官方首领已击败'
      WHEN 3 THEN '正传锚点已击败'
      WHEN 4 THEN '深渊锚点已击败'
      WHEN 5 THEN '腐化锚点已击败'
      ELSE '已推进'
    END,
    '【', c.`章节名称`, '】'
  )
FROM `_深渊任务模板对接` q
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
WHERE q.`任务ID` BETWEEN 700001 AND 740074;

REPLACE INTO `quest_offer_reward`
(`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `RewardText`)
SELECT
  q.`任务ID`,
  1,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  CONCAT(
    CASE q.`任务类型`
      WHEN 1 THEN '章节引导已完成：'
      WHEN 2 THEN '正传模式已解锁：'
      WHEN 3 THEN '深渊模式已解锁：'
      WHEN 4 THEN '腐化模式已解锁：'
      WHEN 5 THEN '轮回模式已解锁：'
      ELSE '深渊流程已推进：'
    END,
    c.`章节名称`
  )
FROM `_深渊任务模板对接` q
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
WHERE q.`任务ID` BETWEEN 700001 AND 740074;

-- 官方锚点首领名称直接同步世界库，避免手填名称与 creature_template 不一致
UPDATE `_深渊首领配置` ab
JOIN `creature_template` ct ON ct.`entry` = ab.`首领入口`
SET ab.`首领名称` = ct.`name`
WHERE ab.`首领类型` = 1
  AND ct.`name` IS NOT NULL
  AND ct.`name` <> '';

-- 起始任务：要求击败章节锚点/关键首领，不再自动完成
UPDATE `quest_template` qt
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qt.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
LEFT JOIN `_深渊首领配置` ab ON ab.`首领入口` = c.`锚点首领入口`
LEFT JOIN `_深渊首领配置` fb ON fb.`首领入口` = c.`最终首领入口`
SET
  qt.`Flags` = 0,
  qt.`RequiredNpcOrGo1` = CASE
    WHEN c.`触发类型` = 1 AND c.`锚点首领入口` <> 0 THEN c.`锚点首领入口`
    WHEN c.`最终首领入口` <> 0 THEN c.`最终首领入口`
    WHEN c.`锚点首领入口` <> 0 THEN c.`锚点首领入口`
    ELSE 0
  END,
  qt.`RequiredNpcOrGoCount1` = CASE
    WHEN c.`触发类型` = 1 AND c.`锚点首领入口` <> 0 THEN 1
    WHEN c.`最终首领入口` <> 0 THEN 1
    WHEN c.`锚点首领入口` <> 0 THEN 1
    ELSE 0
  END,
  qt.`QuestDescription` = CASE
    WHEN c.`触发类型` = 1 AND c.`锚点首领入口` <> 0 THEN CONCAT('进入深渊章节【', c.`章节名称`, '】并击败锚点首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】。')
    WHEN c.`最终首领入口` <> 0 THEN CONCAT('进入深渊章节【', c.`章节名称`, '】并击败关键首领【', COALESCE(NULLIF(fb.`首领名称`, ''), CONCAT('entry:', c.`最终首领入口`)), '】。')
    WHEN c.`锚点首领入口` <> 0 THEN CONCAT('进入深渊章节【', c.`章节名称`, '】并击败关键首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】。')
    ELSE CONCAT('进入深渊章节【', c.`章节名称`, '】并完成本章起始试炼。')
  END,
  qt.`QuestCompletionLog` = CONCAT('已完成章节【', c.`章节名称`, '】的起始试炼。'),
  qt.`ObjectiveText1` = CASE
    WHEN c.`触发类型` = 1 AND c.`锚点首领入口` <> 0 THEN CONCAT('击败锚点首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】，开启【', c.`章节名称`, '】')
    WHEN c.`最终首领入口` <> 0 THEN CONCAT('击败关键首领【', COALESCE(NULLIF(fb.`首领名称`, ''), CONCAT('entry:', c.`最终首领入口`)), '】，开启【', c.`章节名称`, '】')
    WHEN c.`锚点首领入口` <> 0 THEN CONCAT('击败关键首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】，开启【', c.`章节名称`, '】')
    ELSE CONCAT('完成章节【', c.`章节名称`, '】起始试炼')
  END
WHERE q.`任务类型` = 1
  AND q.`任务ID` BETWEEN 700001 AND 740074;

-- 正传 / 深渊 / 腐化任务：事件型任务，由模块 C++ 在对应模式击杀对应首领时完成
UPDATE `quest_template` qt
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qt.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
LEFT JOIN `_深渊首领配置` ab ON ab.`首领入口` = c.`锚点首领入口`
LEFT JOIN `_深渊首领配置` db ON db.`首领入口` = c.`深渊首领入口`
SET
  qt.`Flags` = 0,
  qt.`ObjectiveText1` = CASE
    WHEN q.`任务类型` = 2 THEN CONCAT('在官方副本【', c.`章节名称`, '】中击败锚点首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】')
    WHEN q.`任务类型` = 3 THEN CONCAT('在正传模式【', c.`章节名称`, '】中击败首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】')
    WHEN q.`任务类型` = 4 THEN CONCAT('在深渊模式【', c.`章节名称`, '】中击败深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】')
    WHEN q.`任务类型` = 5 THEN CONCAT('在腐化模式【', c.`章节名称`, '】中击败深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】')
    ELSE qt.`ObjectiveText1`
  END,
  qt.`QuestDescription` = CASE
    WHEN q.`任务类型` = 2 THEN CONCAT('在官方副本【', c.`章节名称`, '】中击败锚点首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】，完成后即可解锁正传模式。')
    WHEN q.`任务类型` = 3 THEN CONCAT('在正传模式中推进【', c.`章节名称`, '】，击败首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】后即可解锁深渊模式。')
    WHEN q.`任务类型` = 4 THEN CONCAT('在深渊模式中推进【', c.`章节名称`, '】，击败深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】后即可解锁腐化模式。')
    WHEN q.`任务类型` = 5 THEN CONCAT('在腐化模式中推进【', c.`章节名称`, '】，击败深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】后即可解锁轮回模式，并开启下一副本任务。')
    ELSE qt.`QuestDescription`
  END,
  qt.`QuestCompletionLog` = CASE
    WHEN q.`任务类型` = 2 THEN CONCAT('已完成【', c.`章节名称`, '】官方通关试炼。')
    WHEN q.`任务类型` = 3 THEN CONCAT('已完成【', c.`章节名称`, '】正传试炼。')
    WHEN q.`任务类型` = 4 THEN CONCAT('已完成【', c.`章节名称`, '】深渊试炼。')
    WHEN q.`任务类型` = 5 THEN CONCAT('已完成【', c.`章节名称`, '】腐化试炼。')
    ELSE qt.`QuestCompletionLog`
  END,
  qt.`RequiredNpcOrGo1` = CASE
    WHEN q.`任务类型` = 2 THEN c.`锚点首领入口`
    WHEN q.`任务类型` IN (3, 4, 5) THEN c.`深渊首领入口`
    ELSE 0
  END,
  qt.`RequiredNpcOrGoCount1` = 1
WHERE q.`任务类型` IN (2, 3, 4, 5)
  AND q.`任务ID` BETWEEN 700001 AND 740074;

-- 变身/换阶段首领章节：任务目标清零，接取后可直接提交
UPDATE `quest_template` qt
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qt.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
SET
  qt.`RequiredNpcOrGo1` = 0,
  qt.`RequiredNpcOrGo2` = 0,
  qt.`RequiredNpcOrGo3` = 0,
  qt.`RequiredNpcOrGo4` = 0,
  qt.`RequiredNpcOrGoCount1` = 0,
  qt.`RequiredNpcOrGoCount2` = 0,
  qt.`RequiredNpcOrGoCount3` = 0,
  qt.`RequiredNpcOrGoCount4` = 0,
  qt.`ObjectiveText1` = '',
  qt.`ObjectiveText2` = '',
  qt.`ObjectiveText3` = '',
  qt.`ObjectiveText4` = '',
  qt.`QuestDescription` = CONCAT('【', c.`章节名称`, '】任务已调整为接取后可直接提交。'),
  qt.`QuestCompletionLog` = CONCAT('返回提交【', c.`章节名称`, '】任务。')
WHERE c.`章节ID` IN (42, 52, 64, 66, 69)
  AND q.`任务类型` IN (1, 2, 3, 4, 5)
  AND q.`任务ID` BETWEEN 700001 AND 740074;

-- 腐化任务：完成后发放章节遗物与底材奖励
UPDATE `quest_template` qt
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qt.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
SET
  qt.`RewardItem1` = c.`遗物物品ID`,
  qt.`RewardAmount1` = CASE WHEN c.`遗物物品ID` <> 0 THEN 1 ELSE 0 END,
  qt.`RewardChoiceItemID1` = COALESCE((SELECT i.`物品ID` FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4 ORDER BY i.`物品ID` LIMIT 1), 0),
  qt.`RewardChoiceItemQuantity1` = CASE WHEN EXISTS (SELECT 1 FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4) THEN 1 ELSE 0 END,
  qt.`RewardChoiceItemID2` = COALESCE((SELECT i.`物品ID` FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4 ORDER BY i.`物品ID` LIMIT 1 OFFSET 1), 0),
  qt.`RewardChoiceItemQuantity2` = CASE WHEN (SELECT COUNT(*) FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4) >= 2 THEN 1 ELSE 0 END,
  qt.`RewardChoiceItemID3` = COALESCE((SELECT i.`物品ID` FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4 ORDER BY i.`物品ID` LIMIT 1 OFFSET 2), 0),
  qt.`RewardChoiceItemQuantity3` = CASE WHEN (SELECT COUNT(*) FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4) >= 3 THEN 1 ELSE 0 END,
  qt.`RewardChoiceItemID4` = COALESCE((SELECT i.`物品ID` FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4 ORDER BY i.`物品ID` LIMIT 1 OFFSET 3), 0),
  qt.`RewardChoiceItemQuantity4` = CASE WHEN (SELECT COUNT(*) FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4) >= 4 THEN 1 ELSE 0 END,
  qt.`RewardChoiceItemID5` = COALESCE((SELECT i.`物品ID` FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4 ORDER BY i.`物品ID` LIMIT 1 OFFSET 4), 0),
  qt.`RewardChoiceItemQuantity5` = CASE WHEN (SELECT COUNT(*) FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4) >= 5 THEN 1 ELSE 0 END,
  qt.`RewardChoiceItemID6` = COALESCE((SELECT i.`物品ID` FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4 ORDER BY i.`物品ID` LIMIT 1 OFFSET 5), 0),
  qt.`RewardChoiceItemQuantity6` = CASE WHEN (SELECT COUNT(*) FROM `_深渊物品模板对接` i WHERE i.`章节ID` = c.`章节ID` AND i.`对接类型` = 4) >= 6 THEN 1 ELSE 0 END,
  qt.`RewardMoneyDifficulty` = LEAST(GREATEST(c.`需求修仙等级`, 1), 80)
WHERE q.`任务类型` = 5
  AND q.`任务ID` BETWEEN 700001 AND 740074;

-- 起始任务：补引导文本与任务响应内容
UPDATE `quest_request_items` qr
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qr.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
LEFT JOIN `_深渊首领配置` ab ON ab.`首领入口` = c.`锚点首领入口`
LEFT JOIN `_深渊首领配置` fb ON fb.`首领入口` = c.`最终首领入口`
SET
  qr.`CompletionText` = CONCAT(
    '踏入【', c.`章节名称`, '】后，击败',
    CASE
      WHEN c.`触发类型` = 1 AND c.`锚点首领入口` <> 0 THEN CONCAT('锚点首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】')
      WHEN c.`最终首领入口` <> 0 THEN CONCAT('关键首领【', COALESCE(NULLIF(fb.`首领名称`, ''), CONCAT('entry:', c.`最终首领入口`)), '】')
      WHEN c.`锚点首领入口` <> 0 THEN CONCAT('关键首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】')
      ELSE '目标首领'
    END,
    '后，深渊试炼即正式开始。'
  )
WHERE q.`任务类型` = 1
  AND q.`任务ID` BETWEEN 700001 AND 740074;

UPDATE `quest_offer_reward` qo
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qo.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
SET
  qo.`RewardText` = CONCAT(
    '已完成官方通关任务，【', c.`章节名称`, '】的正传模式现已开放。'
  )
WHERE q.`任务类型` = 1
  AND q.`任务ID` BETWEEN 700001 AND 730074;

-- 正传 / 深渊 / 腐化：补模式解锁与阶段反馈
UPDATE `quest_request_items` qr
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qr.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
LEFT JOIN `_深渊章节配置` n ON n.`章节ID` = c.`章节ID` + 1
LEFT JOIN `_深渊遗物配置` r ON r.`物品ID` = c.`遗物物品ID`
LEFT JOIN `_深渊首领配置` ab ON ab.`首领入口` = c.`锚点首领入口`
LEFT JOIN `_深渊首领配置` db ON db.`首领入口` = c.`深渊首领入口`
SET
  qr.`CompletionText` = CONCAT(
    CASE q.`任务类型`
      WHEN 2 THEN CONCAT('你已击破【', c.`章节名称`, '】的官方锚点【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】，正传模式即将开启。')
      WHEN 3 THEN CONCAT('你已击破【', c.`章节名称`, '】的正传首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】，深渊模式即将开启。')
      WHEN 4 THEN CONCAT('你已击破【', c.`章节名称`, '】的深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】，腐化模式即将开启。')
      WHEN 5 THEN CONCAT('你已击破【', c.`章节名称`, '】的腐化首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】，轮回模式即将开启。')
      ELSE CONCAT('你已完成【', c.`章节名称`, '】。')
    END,
    CASE WHEN q.`任务类型` = 5 AND r.`名称` IS NOT NULL AND r.`名称` <> '' THEN CONCAT(' 将获得章节遗物【', r.`名称`, '】。') ELSE '' END,
    CASE WHEN q.`任务类型` = 5 THEN ' 你还可从本章底材池中选择一件奖励。' ELSE '' END,
    CASE WHEN q.`任务类型` = 5 AND n.`章节ID` IS NOT NULL THEN CONCAT(' 下一章为【', n.`章节名称`, '】。') ELSE '' END
  )
WHERE q.`任务类型` IN (2, 3, 4, 5)
  AND q.`任务ID` BETWEEN 700001 AND 740074;

UPDATE `quest_offer_reward` qo
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qo.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
LEFT JOIN `_深渊章节配置` n ON n.`章节ID` = c.`章节ID` + 1
LEFT JOIN `_深渊遗物配置` r ON r.`物品ID` = c.`遗物物品ID`
LEFT JOIN `_深渊首领配置` ab ON ab.`首领入口` = c.`锚点首领入口`
LEFT JOIN `_深渊首领配置` db ON db.`首领入口` = c.`深渊首领入口`
SET
  qo.`RewardText` = CONCAT(
    CASE q.`任务类型`
      WHEN 2 THEN CONCAT('你已完成锚点首领【', COALESCE(NULLIF(ab.`首领名称`, ''), CONCAT('entry:', c.`锚点首领入口`)), '】的官方试炼，【', c.`章节名称`, '】正传模式已解锁。')
      WHEN 3 THEN CONCAT('你已完成首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】的正传试炼，【', c.`章节名称`, '】深渊模式已解锁。')
      WHEN 4 THEN CONCAT('你已完成深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】的深渊试炼，【', c.`章节名称`, '】腐化模式已解锁。')
      WHEN 5 THEN CONCAT('你已完成深渊首领【', COALESCE(NULLIF(db.`首领名称`, ''), CONCAT('entry:', c.`深渊首领入口`)), '】的腐化试炼，【', c.`章节名称`, '】轮回模式已解锁。')
      ELSE CONCAT('【', c.`章节名称`, '】流程已推进。')
    END,
    CASE WHEN q.`任务类型` = 5 AND r.`名称` IS NOT NULL AND r.`名称` <> '' THEN CONCAT(' 你获得了章节遗物【', r.`名称`, '】。') ELSE '' END,
    CASE WHEN q.`任务类型` = 5 THEN ' 同时可自选一件章节底材作为额外奖励。' ELSE '' END,
    CASE WHEN q.`任务类型` = 5 AND n.`章节ID` IS NOT NULL THEN CONCAT(' 下一章：', n.`章节名称`, '。') ELSE '' END
  )
WHERE q.`任务类型` IN (2, 3, 4, 5)
  AND q.`任务ID` BETWEEN 700001 AND 740074;

-- 腐化任务：补首批基础奖励，便于首版联调时有明显回报
UPDATE `quest_template` qt
JOIN `_深渊任务模板对接` q ON q.`任务ID` = qt.`ID`
JOIN `_深渊章节配置` c ON c.`章节ID` = q.`章节ID`
SET
  qt.`RewardMoney` = q.`建议任务等级` * 1000,
  qt.`RewardXPDifficulty` = q.`建议任务等级`,
  qt.`RewardHonor` = CASE WHEN q.`建议任务等级` >= 80 THEN 20 ELSE 0 END
WHERE q.`任务类型` = 5
  AND q.`任务ID` BETWEEN 700001 AND 740074;

UPDATE `_深渊任务模板对接`
SET `对接状态` = 1
WHERE `任务ID` BETWEEN 700001 AND 740074;


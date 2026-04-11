-- ============================================
-- 深渊遗物 / 神器技能模板定义 (89101-89181)
-- 来源: 09_遗物与神器.sql，共 81 条
-- 说明:
-- 1. _wydbc_spell 提供 Spell.dbc 自定义技能占位与客户端说明
-- 2. item_template.spellid_1 直接绑定技能，便于物品提示与统一查询
-- 3. 系统型遗物暂保留展示技能，不并入普通 spell 驱动：
--    89135 禁狱零匙 / 89142 紫狱印典 / 89158 幕星假面 / 89170 观察者棱眼
-- ============================================

DELETE FROM `_wydbc_spell` WHERE `ID` BETWEEN 89101 AND 89181;
DELETE FROM `spell_dbc` WHERE `ID` BETWEEN 89101 AND 89181;

INSERT INTO `_wydbc_spell`
(
  `ID`, `Attributes`, `RangeIndex`, `DurationIndex`, `Effect_1`, `ImplicitTargetA_1`, `EffectAura_1`,
  `SpellVisualID_1`, `SpellIconID`, `ActiveIconID`,
  `Name_Lang_deDE`,
  `Description_Lang_deDE`,
  `AuraDescription_Lang_deDE`,
  `SchoolMask`
)
SELECT
  CASE
    WHEN `物品ID` BETWEEN 950001 AND 950074 THEN 89100 + (`物品ID` - 950000)
    WHEN `物品ID` BETWEEN 960001 AND 960006 THEN 89174 + (`物品ID` - 960000)
    WHEN `物品ID` = 970001 THEN 89181
  END AS `ID`,
  64 AS `Attributes`,          -- SPELL_ATTR0_PASSIVE
  1 AS `RangeIndex`,           -- 自身
  21 AS `DurationIndex`,       -- 永久
  6 AS `Effect_1`,             -- APPLY_AURA
  1 AS `ImplicitTargetA_1`,    -- 自身
  4 AS `EffectAura_1`,         -- SPELL_AURA_DUMMY
  0 AS `SpellVisualID_1`,
  0 AS `SpellIconID`,
  0 AS `ActiveIconID`,
  `名称` AS `Name_Lang_deDE`,
  `完整描述` AS `Description_Lang_deDE`,
  `完整描述` AS `AuraDescription_Lang_deDE`,
  1 AS `SchoolMask`
FROM `_深渊遗物配置`
WHERE (`物品ID` BETWEEN 950001 AND 950074)
   OR (`物品ID` BETWEEN 960001 AND 960006)
   OR `物品ID` = 970001
ORDER BY `物品ID`;

-- 已实现的遗物效果切换为 SCRIPT_EFFECT，由深渊模块统一 CastSpell 驱动
UPDATE `_wydbc_spell`
SET
  `Effect_1` = CASE `ID`
    WHEN 89101 THEN 77
    WHEN 89102 THEN 77
    WHEN 89103 THEN 77
    WHEN 89104 THEN 77
    WHEN 89110 THEN 77
    WHEN 89112 THEN 77
    WHEN 89113 THEN 77
    WHEN 89105 THEN 77
    WHEN 89106 THEN 77
    WHEN 89107 THEN 77
    WHEN 89108 THEN 77
    WHEN 89109 THEN 77
    WHEN 89111 THEN 77
    WHEN 89114 THEN 77
    WHEN 89115 THEN 77
    WHEN 89116 THEN 77
    WHEN 89117 THEN 77
    WHEN 89118 THEN 77
    WHEN 89119 THEN 77
    WHEN 89120 THEN 77
    WHEN 89121 THEN 77
    WHEN 89122 THEN 77
    WHEN 89123 THEN 77
    WHEN 89124 THEN 77
    WHEN 89125 THEN 77
    WHEN 89126 THEN 77
    WHEN 89127 THEN 77
    WHEN 89128 THEN 77
    WHEN 89129 THEN 77
    WHEN 89130 THEN 77
    WHEN 89131 THEN 77
    WHEN 89132 THEN 77
    WHEN 89133 THEN 77
    WHEN 89134 THEN 77
    WHEN 89136 THEN 77
    WHEN 89137 THEN 77
    WHEN 89138 THEN 77
    WHEN 89139 THEN 77
    WHEN 89140 THEN 77
    WHEN 89141 THEN 77
    WHEN 89143 THEN 77
    WHEN 89144 THEN 77
    WHEN 89145 THEN 77
    WHEN 89146 THEN 77
    WHEN 89147 THEN 77
    WHEN 89148 THEN 77
    WHEN 89149 THEN 77
    WHEN 89150 THEN 77
    WHEN 89151 THEN 77
    WHEN 89152 THEN 77
    WHEN 89153 THEN 77
    WHEN 89154 THEN 77
    WHEN 89155 THEN 77
    WHEN 89156 THEN 77
    WHEN 89157 THEN 77
    WHEN 89159 THEN 77
    WHEN 89160 THEN 77
    WHEN 89161 THEN 77
    WHEN 89162 THEN 77
    WHEN 89163 THEN 77
    WHEN 89164 THEN 77
    WHEN 89165 THEN 77
    WHEN 89166 THEN 77
    WHEN 89167 THEN 77
    WHEN 89168 THEN 77
    WHEN 89169 THEN 77
    WHEN 89171 THEN 77
    WHEN 89172 THEN 77
    WHEN 89173 THEN 77
    WHEN 89174 THEN 77
    WHEN 89175 THEN 77
    WHEN 89176 THEN 77
    WHEN 89177 THEN 77
    WHEN 89178 THEN 77
    WHEN 89179 THEN 77
    WHEN 89180 THEN 77
    WHEN 89181 THEN 77
    ELSE `Effect_1`
  END,
  `EffectAura_1` = CASE `ID`
    WHEN 89101 THEN 0
    WHEN 89102 THEN 0
    WHEN 89103 THEN 0
    WHEN 89104 THEN 0
    WHEN 89110 THEN 0
    WHEN 89112 THEN 0
    WHEN 89113 THEN 0
    WHEN 89105 THEN 0
    WHEN 89106 THEN 0
    WHEN 89107 THEN 0
    WHEN 89108 THEN 0
    WHEN 89109 THEN 0
    WHEN 89111 THEN 0
    WHEN 89114 THEN 0
    WHEN 89115 THEN 0
    WHEN 89116 THEN 0
    WHEN 89117 THEN 0
    WHEN 89118 THEN 0
    WHEN 89119 THEN 0
    WHEN 89120 THEN 0
    WHEN 89121 THEN 0
    WHEN 89122 THEN 0
    WHEN 89123 THEN 0
    WHEN 89124 THEN 0
    WHEN 89125 THEN 0
    WHEN 89126 THEN 0
    WHEN 89127 THEN 0
    WHEN 89128 THEN 0
    WHEN 89129 THEN 0
    WHEN 89130 THEN 0
    WHEN 89131 THEN 0
    WHEN 89132 THEN 0
    WHEN 89133 THEN 0
    WHEN 89134 THEN 0
    WHEN 89136 THEN 0
    WHEN 89137 THEN 0
    WHEN 89138 THEN 0
    WHEN 89139 THEN 0
    WHEN 89140 THEN 0
    WHEN 89141 THEN 0
    WHEN 89143 THEN 0
    WHEN 89144 THEN 0
    WHEN 89145 THEN 0
    WHEN 89146 THEN 0
    WHEN 89147 THEN 0
    WHEN 89148 THEN 0
    WHEN 89149 THEN 0
    WHEN 89150 THEN 0
    WHEN 89151 THEN 0
    WHEN 89152 THEN 0
    WHEN 89153 THEN 0
    WHEN 89154 THEN 0
    WHEN 89155 THEN 0
    WHEN 89156 THEN 0
    WHEN 89157 THEN 0
    WHEN 89159 THEN 0
    WHEN 89160 THEN 0
    WHEN 89161 THEN 0
    WHEN 89162 THEN 0
    WHEN 89163 THEN 0
    WHEN 89164 THEN 0
    WHEN 89165 THEN 0
    WHEN 89166 THEN 0
    WHEN 89167 THEN 0
    WHEN 89168 THEN 0
    WHEN 89169 THEN 0
    WHEN 89171 THEN 0
    WHEN 89172 THEN 0
    WHEN 89173 THEN 0
    WHEN 89174 THEN 0
    WHEN 89175 THEN 0
    WHEN 89176 THEN 0
    WHEN 89177 THEN 0
    WHEN 89178 THEN 0
    WHEN 89179 THEN 0
    WHEN 89180 THEN 0
    WHEN 89181 THEN 0
    ELSE `EffectAura_1`
  END
WHERE `ID` BETWEEN 89101 AND 89181;

-- 对真正由模块 CastSpell 驱动的脚本法术，使用与 890xx 相同的可施放结构，
-- 不再保留 PASSIVE 属性，否则会出现“模块调用了 CastSpell，但技能不正常落地”的问题。
UPDATE `_wydbc_spell`
SET
  `Attributes` = 0,
  `AttributesEx` = 1024,
  `AttributesEx2` = 0,
  `AttributesEx3` = 1048576,
  `AttributesEx4` = 64,
  `DurationIndex` = 0
WHERE `ID` BETWEEN 89101 AND 89181
  AND `Effect_1` = 77;

-- 系统型遗物在测试模式下也改为可主动释放的脚本法术，
-- 这样 89101-89181 全部都可以直接手动施放测试，不再混有被动展示技能。
UPDATE `_wydbc_spell`
SET
  `Effect_1` = 77,
  `EffectAura_1` = 0,
  `Attributes` = 0,
  `AttributesEx` = 1024,
  `AttributesEx2` = 0,
  `AttributesEx3` = 1048576,
  `AttributesEx4` = 64,
  `DurationIndex` = 0
WHERE `ID` IN (89135, 89142, 89158, 89170);

-- 托管遗物技能不应要求装备类型。
-- 这里必须是 -1，若被标准化成 0，会触发
-- HasItemFitToSpellRequirements: Not handled spell requirement for item class 0
UPDATE `_wydbc_spell`
SET
  `EquippedItemClass` = -1,
  `EquippedItemSubclass` = 0,
  `EquippedItemInvTypes` = 0
WHERE `ID` BETWEEN 89101 AND 89181;

-- 参照官方 116 寒冰箭：中文内容写入 deDE 列，zhCN 留空
UPDATE `_wydbc_spell`
SET
  `Name_Lang_deDE` = IFNULL(NULLIF(`Name_Lang_deDE`, ''), `Name_Lang_zhCN`),
  `Description_Lang_deDE` = IFNULL(NULLIF(`Description_Lang_deDE`, ''), `Description_Lang_zhCN`),
  `AuraDescription_Lang_deDE` = IFNULL(NULLIF(`AuraDescription_Lang_deDE`, ''), `AuraDescription_Lang_zhCN`),
  `Name_Lang_zhCN` = '',
  `Description_Lang_zhCN` = '',
  `AuraDescription_Lang_zhCN` = '',
  `Name_Lang_Mask` = 16712190,
  `Description_Lang_Mask` = 16712190,
  `AuraDescription_Lang_Mask` = 16712188
WHERE `ID` BETWEEN 89101 AND 89181;

DELETE FROM `spell_script_names` WHERE `spell_id` BETWEEN 89101 AND 89181;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(89101, 'spell_abyss_managed_relic'),
(89102, 'spell_abyss_managed_relic'),
(89103, 'spell_abyss_managed_relic'),
(89104, 'spell_abyss_managed_relic'),
(89110, 'spell_abyss_managed_relic'),
(89112, 'spell_abyss_managed_relic'),
(89113, 'spell_abyss_managed_relic'),
(89105, 'spell_abyss_managed_relic'),
(89106, 'spell_abyss_managed_relic'),
(89107, 'spell_abyss_managed_relic'),
(89108, 'spell_abyss_managed_relic'),
(89109, 'spell_abyss_managed_relic'),
(89111, 'spell_abyss_managed_relic'),
(89114, 'spell_abyss_managed_relic'),
(89115, 'spell_abyss_managed_relic'),
(89116, 'spell_abyss_managed_relic'),
(89117, 'spell_abyss_managed_relic'),
(89118, 'spell_abyss_managed_relic'),
(89119, 'spell_abyss_managed_relic'),
(89120, 'spell_abyss_managed_relic'),
(89121, 'spell_abyss_managed_relic'),
(89122, 'spell_abyss_managed_relic'),
(89123, 'spell_abyss_managed_relic'),
(89124, 'spell_abyss_managed_relic'),
(89125, 'spell_abyss_managed_relic'),
(89126, 'spell_abyss_managed_relic'),
(89127, 'spell_abyss_managed_relic'),
(89128, 'spell_abyss_managed_relic'),
(89129, 'spell_abyss_managed_relic'),
(89130, 'spell_abyss_managed_relic'),
(89131, 'spell_abyss_managed_relic'),
(89132, 'spell_abyss_managed_relic'),
(89133, 'spell_abyss_managed_relic'),
(89134, 'spell_abyss_managed_relic'),
(89136, 'spell_abyss_managed_relic'),
(89137, 'spell_abyss_managed_relic'),
(89138, 'spell_abyss_managed_relic'),
(89139, 'spell_abyss_managed_relic'),
(89140, 'spell_abyss_managed_relic'),
(89141, 'spell_abyss_managed_relic'),
(89143, 'spell_abyss_managed_relic'),
(89144, 'spell_abyss_managed_relic'),
(89145, 'spell_abyss_managed_relic'),
(89146, 'spell_abyss_managed_relic'),
(89147, 'spell_abyss_managed_relic'),
(89148, 'spell_abyss_managed_relic'),
(89149, 'spell_abyss_managed_relic'),
(89150, 'spell_abyss_managed_relic'),
(89151, 'spell_abyss_managed_relic'),
(89152, 'spell_abyss_managed_relic'),
(89153, 'spell_abyss_managed_relic'),
(89154, 'spell_abyss_managed_relic'),
(89155, 'spell_abyss_managed_relic'),
(89156, 'spell_abyss_managed_relic'),
(89157, 'spell_abyss_managed_relic'),
(89159, 'spell_abyss_managed_relic'),
(89160, 'spell_abyss_managed_relic'),
(89161, 'spell_abyss_managed_relic'),
(89162, 'spell_abyss_managed_relic'),
(89163, 'spell_abyss_managed_relic'),
(89164, 'spell_abyss_managed_relic'),
(89165, 'spell_abyss_managed_relic'),
(89166, 'spell_abyss_managed_relic'),
(89167, 'spell_abyss_managed_relic'),
(89168, 'spell_abyss_managed_relic'),
(89169, 'spell_abyss_managed_relic'),
(89171, 'spell_abyss_managed_relic'),
(89172, 'spell_abyss_managed_relic'),
(89173, 'spell_abyss_managed_relic'),
(89174, 'spell_abyss_managed_relic'),
(89175, 'spell_abyss_managed_relic'),
(89176, 'spell_abyss_managed_relic'),
(89177, 'spell_abyss_managed_relic'),
(89178, 'spell_abyss_managed_relic'),
(89179, 'spell_abyss_managed_relic'),
(89180, 'spell_abyss_managed_relic'),
(89181, 'spell_abyss_managed_relic');

-- 图标适配：
-- 1. 普通战斗型遗物尽量复用 18_装备特效法术.sql 已验证过的图标分组
-- 2. 系统型遗物也补展示图标，方便测试期识别
UPDATE `_wydbc_spell`
SET
  `SpellIconID` = CASE
    WHEN `ID` IN (89101, 89109, 89119, 89172, 89174) THEN 2725
    WHEN `ID` IN (89110, 89115, 89153, 89160) THEN 37
    WHEN `ID` IN (89116, 89136, 89168) THEN 12
    WHEN `ID` IN (89102, 89125, 89126, 89178) THEN 1541
    WHEN `ID` IN (89113, 89118, 89134, 89139, 89156, 89167) THEN 10
    WHEN `ID` IN (89103, 89127, 89132, 89133, 89138, 89146, 89155, 89169) THEN 122
    WHEN `ID` IN (89111, 89112, 89131, 89144, 89152, 89158, 89164, 89177) THEN 123
    WHEN `ID` IN (89114, 89135, 89140, 89157, 89162, 89170, 89176, 89180, 89181) THEN 173
    WHEN `ID` IN (89120, 89122, 89128, 89150) THEN 143
    WHEN `ID` IN (89107, 89145) THEN 220
    WHEN `ID` IN (89104, 89108, 89117, 89121, 89123, 89137, 89143, 89154, 89159, 89163) THEN 457
    WHEN `ID` IN (89129, 89148) THEN 1986
    WHEN `ID` IN (89130, 89142, 89151) THEN 212
    WHEN `ID` IN (89105, 89124, 89141, 89161, 89173) THEN 143
    WHEN `ID` IN (89149, 89165) THEN 10
    WHEN `ID` IN (89171, 89179) THEN 3027
    WHEN `ID` = 89166 THEN 3027
    WHEN `ID` = 89175 THEN 11
    ELSE `SpellIconID`
  END,
  `ActiveIconID` = CASE
    WHEN `ID` IN (89101, 89109, 89119, 89172, 89174) THEN 2725
    WHEN `ID` IN (89110, 89115, 89153, 89160) THEN 37
    WHEN `ID` IN (89116, 89136, 89168) THEN 12
    WHEN `ID` IN (89102, 89125, 89126, 89178) THEN 1541
    WHEN `ID` IN (89113, 89118, 89134, 89139, 89156, 89167) THEN 10
    WHEN `ID` IN (89103, 89127, 89132, 89133, 89138, 89146, 89155, 89169) THEN 122
    WHEN `ID` IN (89111, 89112, 89131, 89144, 89152, 89158, 89164, 89177) THEN 123
    WHEN `ID` IN (89114, 89135, 89140, 89157, 89162, 89170, 89176, 89180, 89181) THEN 173
    WHEN `ID` IN (89120, 89122, 89128, 89150) THEN 143
    WHEN `ID` IN (89107, 89145) THEN 220
    WHEN `ID` IN (89104, 89108, 89117, 89121, 89123, 89137, 89143, 89154, 89159, 89163) THEN 457
    WHEN `ID` IN (89129, 89148) THEN 1986
    WHEN `ID` IN (89130, 89142, 89151) THEN 212
    WHEN `ID` IN (89105, 89124, 89141, 89161, 89173) THEN 143
    WHEN `ID` IN (89149, 89165) THEN 10
    WHEN `ID` IN (89171, 89179) THEN 3027
    WHEN `ID` = 89166 THEN 3027
    WHEN `ID` = 89175 THEN 11
    ELSE `ActiveIconID`
  END
WHERE `ID` BETWEEN 89101 AND 89181;

-- 第二轮图标精修：
-- 1. 修正 89106 / 89147 先前漏配图标的问题
-- 2. 客户端主视觉在下一段统一切换为 9951xx 自定义 SpellVisual 组合
UPDATE `_wydbc_spell`
SET
  `SpellIconID` = CASE `ID`
    WHEN 89106 THEN 212
    WHEN 89147 THEN 3027
    ELSE `SpellIconID`
  END,
  `ActiveIconID` = CASE `ID`
    WHEN 89106 THEN 212
    WHEN 89147 THEN 3027
    ELSE `ActiveIconID`
  END
WHERE `ID` BETWEEN 89101 AND 89181;

-- 第三轮客户端特效重做：
-- 1. 89101-89181 统一映射到 995101-995181，自定义 SpellVisualID 与法术 ID 一一对应
-- 2. 具体官方 Kit 组合由 agent-harness\patch_toolchain\wanjian\scripts\rebuild_wanjian_dbc.py 写入 SpellVisual.dbc
-- 3. 服务端侧只注册这些自定义 ID，避免旧的单法术视觉再次覆盖回来
UPDATE `_wydbc_spell`
SET
  `SpellVisualID_1` = 995100 + (`ID` - 89100),
  `SpellVisualID_2` = 0
WHERE `ID` BETWEEN 89101 AND 89181;

DELETE FROM `spellvisual_dbc` WHERE `ID` BETWEEN 995101 AND 995181;
INSERT INTO `spellvisual_dbc` (`ID`, `HasMissile`, `MissileModel`)
SELECT 995100 + (`ID` - 89100), 0, 0
FROM `_wydbc_spell`
WHERE `ID` BETWEEN 89101 AND 89181;

-- 目标统一修复：
-- 1. 大多数 891xx 专属特效本质上是“朝当前敌对目标释放”，此前默认自施会导致视觉落在玩家身上
-- 2. 仅系统型 / 纯自 Buff / 神器触发型保留自施
UPDATE `_wydbc_spell`
SET
  `RangeIndex` = CASE
    WHEN `ID` IN (89135, 89142, 89149, 89157, 89158, 89165, 89170, 89175, 89176, 89177, 89178, 89179, 89180, 89181) THEN 1
    ELSE 35
  END,
  `ImplicitTargetA_1` = CASE
    WHEN `ID` IN (89135, 89142, 89149, 89157, 89158, 89165, 89170, 89175, 89176, 89177, 89178, 89179, 89180, 89181) THEN 1
    ELSE 6
  END,
  `ImplicitTargetB_1` = 0
WHERE `ID` BETWEEN 89101 AND 89181;

-- 空字段标准化：避免 DBC 导出时因 NULL 报错
UPDATE _wydbc_spell SET
  `Category` = IFNULL(`Category`, 0),
  `DispelType` = IFNULL(`DispelType`, 0),
  `Mechanic` = IFNULL(`Mechanic`, 0),
  `Attributes` = IFNULL(`Attributes`, 0),
  `AttributesEx` = IFNULL(`AttributesEx`, 0),
  `AttributesEx2` = IFNULL(`AttributesEx2`, 0),
  `AttributesEx3` = IFNULL(`AttributesEx3`, 0),
  `AttributesEx4` = IFNULL(`AttributesEx4`, 0),
  `AttributesEx5` = IFNULL(`AttributesEx5`, 0),
  `AttributesEx6` = IFNULL(`AttributesEx6`, 0),
  `AttributesEx7` = IFNULL(`AttributesEx7`, 0),
  `ShapeshiftMask` = IFNULL(`ShapeshiftMask`, 0),
  `unk_320_2` = IFNULL(`unk_320_2`, 0),
  `ShapeshiftExclude` = IFNULL(`ShapeshiftExclude`, 0),
  `unk_320_3` = IFNULL(`unk_320_3`, 0),
  `Targets` = IFNULL(`Targets`, 0),
  `TargetCreatureType` = IFNULL(`TargetCreatureType`, 0),
  `RequiresSpellFocus` = IFNULL(`RequiresSpellFocus`, 0),
  `FacingCasterFlags` = IFNULL(`FacingCasterFlags`, 0),
  `CasterAuraState` = IFNULL(`CasterAuraState`, 0),
  `TargetAuraState` = IFNULL(`TargetAuraState`, 0),
  `ExcludeCasterAuraState` = IFNULL(`ExcludeCasterAuraState`, 0),
  `ExcludeTargetAuraState` = IFNULL(`ExcludeTargetAuraState`, 0),
  `CasterAuraSpell` = IFNULL(`CasterAuraSpell`, 0),
  `TargetAuraSpell` = IFNULL(`TargetAuraSpell`, 0),
  `ExcludeCasterAuraSpell` = IFNULL(`ExcludeCasterAuraSpell`, 0),
  `ExcludeTargetAuraSpell` = IFNULL(`ExcludeTargetAuraSpell`, 0),
  `CastingTimeIndex` = IFNULL(`CastingTimeIndex`, 0),
  `RecoveryTime` = IFNULL(`RecoveryTime`, 0),
  `CategoryRecoveryTime` = IFNULL(`CategoryRecoveryTime`, 0),
  `InterruptFlags` = IFNULL(`InterruptFlags`, 0),
  `AuraInterruptFlags` = IFNULL(`AuraInterruptFlags`, 0),
  `ChannelInterruptFlags` = IFNULL(`ChannelInterruptFlags`, 0),
  `ProcTypeMask` = IFNULL(`ProcTypeMask`, 0),
  `ProcChance` = IFNULL(`ProcChance`, 0),
  `ProcCharges` = IFNULL(`ProcCharges`, 0),
  `MaxLevel` = IFNULL(`MaxLevel`, 0),
  `BaseLevel` = IFNULL(`BaseLevel`, 0),
  `SpellLevel` = IFNULL(`SpellLevel`, 0),
  `DurationIndex` = IFNULL(`DurationIndex`, 0),
  `PowerType` = IFNULL(`PowerType`, 0),
  `ManaCost` = IFNULL(`ManaCost`, 0),
  `ManaCostPerLevel` = IFNULL(`ManaCostPerLevel`, 0),
  `ManaPerSecond` = IFNULL(`ManaPerSecond`, 0),
  `ManaPerSecondPerLevel` = IFNULL(`ManaPerSecondPerLevel`, 0),
  `RangeIndex` = IFNULL(`RangeIndex`, 0),
  `Speed` = IFNULL(`Speed`, 0),
  `ModalNextSpell` = IFNULL(`ModalNextSpell`, 0),
  `CumulativeAura` = IFNULL(`CumulativeAura`, 0),
  `Totem_1` = IFNULL(`Totem_1`, 0),
  `Totem_2` = IFNULL(`Totem_2`, 0),
  `Reagent_1` = IFNULL(`Reagent_1`, 0),
  `Reagent_2` = IFNULL(`Reagent_2`, 0),
  `Reagent_3` = IFNULL(`Reagent_3`, 0),
  `Reagent_4` = IFNULL(`Reagent_4`, 0),
  `Reagent_5` = IFNULL(`Reagent_5`, 0),
  `Reagent_6` = IFNULL(`Reagent_6`, 0),
  `Reagent_7` = IFNULL(`Reagent_7`, 0),
  `Reagent_8` = IFNULL(`Reagent_8`, 0),
  `ReagentCount_1` = IFNULL(`ReagentCount_1`, 0),
  `ReagentCount_2` = IFNULL(`ReagentCount_2`, 0),
  `ReagentCount_3` = IFNULL(`ReagentCount_3`, 0),
  `ReagentCount_4` = IFNULL(`ReagentCount_4`, 0),
  `ReagentCount_5` = IFNULL(`ReagentCount_5`, 0),
  `ReagentCount_6` = IFNULL(`ReagentCount_6`, 0),
  `ReagentCount_7` = IFNULL(`ReagentCount_7`, 0),
  `ReagentCount_8` = IFNULL(`ReagentCount_8`, 0),
  `EquippedItemClass` = IFNULL(`EquippedItemClass`, 0),
  `EquippedItemSubclass` = IFNULL(`EquippedItemSubclass`, 0),
  `EquippedItemInvTypes` = IFNULL(`EquippedItemInvTypes`, 0),
  `Effect_1` = IFNULL(`Effect_1`, 0),
  `Effect_2` = IFNULL(`Effect_2`, 0),
  `Effect_3` = IFNULL(`Effect_3`, 0),
  `EffectDieSides_1` = IFNULL(`EffectDieSides_1`, 0),
  `EffectDieSides_2` = IFNULL(`EffectDieSides_2`, 0),
  `EffectDieSides_3` = IFNULL(`EffectDieSides_3`, 0),
  `EffectRealPointsPerLevel_1` = IFNULL(`EffectRealPointsPerLevel_1`, 0),
  `EffectRealPointsPerLevel_2` = IFNULL(`EffectRealPointsPerLevel_2`, 0),
  `EffectRealPointsPerLevel_3` = IFNULL(`EffectRealPointsPerLevel_3`, 0),
  `EffectBasePoints_1` = IFNULL(`EffectBasePoints_1`, 0),
  `EffectBasePoints_2` = IFNULL(`EffectBasePoints_2`, 0),
  `EffectBasePoints_3` = IFNULL(`EffectBasePoints_3`, 0),
  `EffectMechanic_1` = IFNULL(`EffectMechanic_1`, 0),
  `EffectMechanic_2` = IFNULL(`EffectMechanic_2`, 0),
  `EffectMechanic_3` = IFNULL(`EffectMechanic_3`, 0),
  `ImplicitTargetA_1` = IFNULL(`ImplicitTargetA_1`, 0),
  `ImplicitTargetA_2` = IFNULL(`ImplicitTargetA_2`, 0),
  `ImplicitTargetA_3` = IFNULL(`ImplicitTargetA_3`, 0),
  `ImplicitTargetB_1` = IFNULL(`ImplicitTargetB_1`, 0),
  `ImplicitTargetB_2` = IFNULL(`ImplicitTargetB_2`, 0),
  `ImplicitTargetB_3` = IFNULL(`ImplicitTargetB_3`, 0),
  `EffectRadiusIndex_1` = IFNULL(`EffectRadiusIndex_1`, 0),
  `EffectRadiusIndex_2` = IFNULL(`EffectRadiusIndex_2`, 0),
  `EffectRadiusIndex_3` = IFNULL(`EffectRadiusIndex_3`, 0),
  `EffectAura_1` = IFNULL(`EffectAura_1`, 0),
  `EffectAura_2` = IFNULL(`EffectAura_2`, 0),
  `EffectAura_3` = IFNULL(`EffectAura_3`, 0),
  `EffectAuraPeriod_1` = IFNULL(`EffectAuraPeriod_1`, 0),
  `EffectAuraPeriod_2` = IFNULL(`EffectAuraPeriod_2`, 0),
  `EffectAuraPeriod_3` = IFNULL(`EffectAuraPeriod_3`, 0),
  `EffectMultipleValue_1` = IFNULL(`EffectMultipleValue_1`, 0),
  `EffectMultipleValue_2` = IFNULL(`EffectMultipleValue_2`, 0),
  `EffectMultipleValue_3` = IFNULL(`EffectMultipleValue_3`, 0),
  `EffectChainTargets_1` = IFNULL(`EffectChainTargets_1`, 0),
  `EffectChainTargets_2` = IFNULL(`EffectChainTargets_2`, 0),
  `EffectChainTargets_3` = IFNULL(`EffectChainTargets_3`, 0),
  `EffectItemType_1` = IFNULL(`EffectItemType_1`, 0),
  `EffectItemType_2` = IFNULL(`EffectItemType_2`, 0),
  `EffectItemType_3` = IFNULL(`EffectItemType_3`, 0),
  `EffectMiscValue_1` = IFNULL(`EffectMiscValue_1`, 0),
  `EffectMiscValue_2` = IFNULL(`EffectMiscValue_2`, 0),
  `EffectMiscValue_3` = IFNULL(`EffectMiscValue_3`, 0),
  `EffectMiscValueB_1` = IFNULL(`EffectMiscValueB_1`, 0),
  `EffectMiscValueB_2` = IFNULL(`EffectMiscValueB_2`, 0),
  `EffectMiscValueB_3` = IFNULL(`EffectMiscValueB_3`, 0),
  `EffectTriggerSpell_1` = IFNULL(`EffectTriggerSpell_1`, 0),
  `EffectTriggerSpell_2` = IFNULL(`EffectTriggerSpell_2`, 0),
  `EffectTriggerSpell_3` = IFNULL(`EffectTriggerSpell_3`, 0),
  `EffectPointsPerCombo_1` = IFNULL(`EffectPointsPerCombo_1`, 0),
  `EffectPointsPerCombo_2` = IFNULL(`EffectPointsPerCombo_2`, 0),
  `EffectPointsPerCombo_3` = IFNULL(`EffectPointsPerCombo_3`, 0),
  `EffectSpellClassMaskA_1` = IFNULL(`EffectSpellClassMaskA_1`, 0),
  `EffectSpellClassMaskA_2` = IFNULL(`EffectSpellClassMaskA_2`, 0),
  `EffectSpellClassMaskA_3` = IFNULL(`EffectSpellClassMaskA_3`, 0),
  `EffectSpellClassMaskB_1` = IFNULL(`EffectSpellClassMaskB_1`, 0),
  `EffectSpellClassMaskB_2` = IFNULL(`EffectSpellClassMaskB_2`, 0),
  `EffectSpellClassMaskB_3` = IFNULL(`EffectSpellClassMaskB_3`, 0),
  `EffectSpellClassMaskC_1` = IFNULL(`EffectSpellClassMaskC_1`, 0),
  `EffectSpellClassMaskC_2` = IFNULL(`EffectSpellClassMaskC_2`, 0),
  `EffectSpellClassMaskC_3` = IFNULL(`EffectSpellClassMaskC_3`, 0),
  `SpellVisualID_1` = IFNULL(`SpellVisualID_1`, 0),
  `SpellVisualID_2` = IFNULL(`SpellVisualID_2`, 0),
  `SpellIconID` = IFNULL(`SpellIconID`, 0),
  `ActiveIconID` = IFNULL(`ActiveIconID`, 0),
  `SpellPriority` = IFNULL(`SpellPriority`, 0),
  `Name_Lang_enUS` = IFNULL(`Name_Lang_enUS`, ''),
  `Name_Lang_enGB` = IFNULL(`Name_Lang_enGB`, ''),
  `Name_Lang_koKR` = IFNULL(`Name_Lang_koKR`, ''),
  `Name_Lang_frFR` = IFNULL(`Name_Lang_frFR`, ''),
  `Name_Lang_deDE` = IFNULL(`Name_Lang_deDE`, ''),
  `Name_Lang_enCN` = IFNULL(`Name_Lang_enCN`, ''),
  `Name_Lang_zhCN` = IFNULL(`Name_Lang_zhCN`, ''),
  `Name_Lang_enTW` = IFNULL(`Name_Lang_enTW`, ''),
  `Name_Lang_zhTW` = IFNULL(`Name_Lang_zhTW`, ''),
  `Name_Lang_esES` = IFNULL(`Name_Lang_esES`, ''),
  `Name_Lang_esMX` = IFNULL(`Name_Lang_esMX`, ''),
  `Name_Lang_ruRU` = IFNULL(`Name_Lang_ruRU`, ''),
  `Name_Lang_ptPT` = IFNULL(`Name_Lang_ptPT`, ''),
  `Name_Lang_ptBR` = IFNULL(`Name_Lang_ptBR`, ''),
  `Name_Lang_itIT` = IFNULL(`Name_Lang_itIT`, ''),
  `Name_Lang_Unk` = IFNULL(`Name_Lang_Unk`, ''),
  `Name_Lang_Mask` = IFNULL(`Name_Lang_Mask`, 0),
  `NameSubtext_Lang_enUS` = IFNULL(`NameSubtext_Lang_enUS`, ''),
  `NameSubtext_Lang_enGB` = IFNULL(`NameSubtext_Lang_enGB`, ''),
  `NameSubtext_Lang_koKR` = IFNULL(`NameSubtext_Lang_koKR`, ''),
  `NameSubtext_Lang_frFR` = IFNULL(`NameSubtext_Lang_frFR`, ''),
  `NameSubtext_Lang_deDE` = IFNULL(`NameSubtext_Lang_deDE`, ''),
  `NameSubtext_Lang_enCN` = IFNULL(`NameSubtext_Lang_enCN`, ''),
  `NameSubtext_Lang_zhCN` = IFNULL(`NameSubtext_Lang_zhCN`, ''),
  `NameSubtext_Lang_enTW` = IFNULL(`NameSubtext_Lang_enTW`, ''),
  `NameSubtext_Lang_zhTW` = IFNULL(`NameSubtext_Lang_zhTW`, ''),
  `NameSubtext_Lang_esES` = IFNULL(`NameSubtext_Lang_esES`, ''),
  `NameSubtext_Lang_esMX` = IFNULL(`NameSubtext_Lang_esMX`, ''),
  `NameSubtext_Lang_ruRU` = IFNULL(`NameSubtext_Lang_ruRU`, ''),
  `NameSubtext_Lang_ptPT` = IFNULL(`NameSubtext_Lang_ptPT`, ''),
  `NameSubtext_Lang_ptBR` = IFNULL(`NameSubtext_Lang_ptBR`, ''),
  `NameSubtext_Lang_itIT` = IFNULL(`NameSubtext_Lang_itIT`, ''),
  `NameSubtext_Lang_Unk` = IFNULL(`NameSubtext_Lang_Unk`, ''),
  `NameSubtext_Lang_Mask` = IFNULL(`NameSubtext_Lang_Mask`, 0),
  `Description_Lang_enUS` = IFNULL(`Description_Lang_enUS`, ''),
  `Description_Lang_enGB` = IFNULL(`Description_Lang_enGB`, ''),
  `Description_Lang_koKR` = IFNULL(`Description_Lang_koKR`, ''),
  `Description_Lang_frFR` = IFNULL(`Description_Lang_frFR`, ''),
  `Description_Lang_deDE` = IFNULL(`Description_Lang_deDE`, ''),
  `Description_Lang_enCN` = IFNULL(`Description_Lang_enCN`, ''),
  `Description_Lang_zhCN` = IFNULL(`Description_Lang_zhCN`, ''),
  `Description_Lang_enTW` = IFNULL(`Description_Lang_enTW`, ''),
  `Description_Lang_zhTW` = IFNULL(`Description_Lang_zhTW`, ''),
  `Description_Lang_esES` = IFNULL(`Description_Lang_esES`, ''),
  `Description_Lang_esMX` = IFNULL(`Description_Lang_esMX`, ''),
  `Description_Lang_ruRU` = IFNULL(`Description_Lang_ruRU`, ''),
  `Description_Lang_ptPT` = IFNULL(`Description_Lang_ptPT`, ''),
  `Description_Lang_ptBR` = IFNULL(`Description_Lang_ptBR`, ''),
  `Description_Lang_itIT` = IFNULL(`Description_Lang_itIT`, ''),
  `Description_Lang_Unk` = IFNULL(`Description_Lang_Unk`, ''),
  `Description_Lang_Mask` = IFNULL(`Description_Lang_Mask`, 0),
  `AuraDescription_Lang_enUS` = IFNULL(`AuraDescription_Lang_enUS`, ''),
  `AuraDescription_Lang_enGB` = IFNULL(`AuraDescription_Lang_enGB`, ''),
  `AuraDescription_Lang_koKR` = IFNULL(`AuraDescription_Lang_koKR`, ''),
  `AuraDescription_Lang_frFR` = IFNULL(`AuraDescription_Lang_frFR`, ''),
  `AuraDescription_Lang_deDE` = IFNULL(`AuraDescription_Lang_deDE`, ''),
  `AuraDescription_Lang_enCN` = IFNULL(`AuraDescription_Lang_enCN`, ''),
  `AuraDescription_Lang_zhCN` = IFNULL(`AuraDescription_Lang_zhCN`, ''),
  `AuraDescription_Lang_enTW` = IFNULL(`AuraDescription_Lang_enTW`, ''),
  `AuraDescription_Lang_zhTW` = IFNULL(`AuraDescription_Lang_zhTW`, ''),
  `AuraDescription_Lang_esES` = IFNULL(`AuraDescription_Lang_esES`, ''),
  `AuraDescription_Lang_esMX` = IFNULL(`AuraDescription_Lang_esMX`, ''),
  `AuraDescription_Lang_ruRU` = IFNULL(`AuraDescription_Lang_ruRU`, ''),
  `AuraDescription_Lang_ptPT` = IFNULL(`AuraDescription_Lang_ptPT`, ''),
  `AuraDescription_Lang_ptBR` = IFNULL(`AuraDescription_Lang_ptBR`, ''),
  `AuraDescription_Lang_itIT` = IFNULL(`AuraDescription_Lang_itIT`, ''),
  `AuraDescription_Lang_Unk` = IFNULL(`AuraDescription_Lang_Unk`, ''),
  `AuraDescription_Lang_Mask` = IFNULL(`AuraDescription_Lang_Mask`, 0),
  `ManaCostPct` = IFNULL(`ManaCostPct`, 0),
  `StartRecoveryCategory` = IFNULL(`StartRecoveryCategory`, 0),
  `StartRecoveryTime` = IFNULL(`StartRecoveryTime`, 0),
  `MaxTargetLevel` = IFNULL(`MaxTargetLevel`, 0),
  `SpellClassSet` = IFNULL(`SpellClassSet`, 0),
  `SpellClassMask_1` = IFNULL(`SpellClassMask_1`, 0),
  `SpellClassMask_2` = IFNULL(`SpellClassMask_2`, 0),
  `SpellClassMask_3` = IFNULL(`SpellClassMask_3`, 0),
  `MaxTargets` = IFNULL(`MaxTargets`, 0),
  `DefenseType` = IFNULL(`DefenseType`, 0),
  `PreventionType` = IFNULL(`PreventionType`, 0),
  `StanceBarOrder` = IFNULL(`StanceBarOrder`, 0),
  `EffectChainAmplitude_1` = IFNULL(`EffectChainAmplitude_1`, 0),
  `EffectChainAmplitude_2` = IFNULL(`EffectChainAmplitude_2`, 0),
  `EffectChainAmplitude_3` = IFNULL(`EffectChainAmplitude_3`, 0),
  `MinFactionID` = IFNULL(`MinFactionID`, 0),
  `MinReputation` = IFNULL(`MinReputation`, 0),
  `RequiredAuraVision` = IFNULL(`RequiredAuraVision`, 0),
  `RequiredTotemCategoryID_1` = IFNULL(`RequiredTotemCategoryID_1`, 0),
  `RequiredTotemCategoryID_2` = IFNULL(`RequiredTotemCategoryID_2`, 0),
  `RequiredAreasID` = IFNULL(`RequiredAreasID`, 0),
  `SchoolMask` = IFNULL(`SchoolMask`, 0),
  `RuneCostID` = IFNULL(`RuneCostID`, 0),
  `SpellMissileID` = IFNULL(`SpellMissileID`, 0),
  `PowerDisplayID` = IFNULL(`PowerDisplayID`, 0),
  `EffectBonusMultiplier_1` = IFNULL(`EffectBonusMultiplier_1`, 0),
  `EffectBonusMultiplier_2` = IFNULL(`EffectBonusMultiplier_2`, 0),
  `EffectBonusMultiplier_3` = IFNULL(`EffectBonusMultiplier_3`, 0),
  `SpellDescriptionVariableID` = IFNULL(`SpellDescriptionVariableID`, 0),
  `SpellDifficultyID` = IFNULL(`SpellDifficultyID`, 0)
WHERE ID BETWEEN 89101 AND 89181;

-- 遗物 / 神器槽位加成展示 spell：只用于物品 tooltip 展示，不参与真实触发
DELETE FROM `_wydbc_spell` WHERE `ID` BETWEEN 89601 AND 89604;
DELETE FROM `spell_dbc` WHERE `ID` BETWEEN 89601 AND 89604;

INSERT INTO `_wydbc_spell`
(
  `ID`, `Attributes`, `RangeIndex`, `DurationIndex`, `Effect_1`, `ImplicitTargetA_1`, `EffectAura_1`,
  `SpellVisualID_1`, `SpellIconID`, `ActiveIconID`,
  `Name_Lang_deDE`,
  `Description_Lang_deDE`,
  `AuraDescription_Lang_deDE`,
  `SchoolMask`
)
VALUES
(89601, 64, 1, 21, 6, 1, 4, 0, 0, 0, '章节遗物主槽加成', '章节遗物主槽加成：全属性+2.0%，生命+2.0%，护甲+1.5%，攻击+1.6%，法术+1.8%。', '章节遗物主槽加成：全属性+2.0%，生命+2.0%，护甲+1.5%，攻击+1.6%，法术+1.8%。', 1),
(89602, 64, 1, 21, 6, 1, 4, 0, 0, 0, '章节遗物副槽加成', '章节遗物副槽加成：全属性+1.0%，生命+1.0%，护甲+0.8%，攻击+0.8%，法术+0.9%。', '章节遗物副槽加成：全属性+1.0%，生命+1.0%，护甲+0.8%，攻击+0.8%，法术+0.9%。', 1),
(89603, 64, 1, 21, 6, 1, 4, 0, 0, 0, '阶段神器槽加成', '阶段神器槽加成：全属性+5.0%，生命+5.0%，护甲+3.8%，攻击+4.0%，法术+4.5%。', '阶段神器槽加成：全属性+5.0%，生命+5.0%，护甲+3.8%，攻击+4.0%，法术+4.5%。', 1),
(89604, 64, 1, 21, 6, 1, 4, 0, 0, 0, '终极神器槽加成', '终极神器槽加成：全属性+10.0%，生命+10.0%，护甲+7.5%，攻击+8.0%，法术+9.0%。', '终极神器槽加成：全属性+10.0%，生命+10.0%，护甲+7.5%，攻击+8.0%，法术+9.0%。', 1);

REPLACE INTO `spell_dbc`
(`ID`, `Attributes`, `RangeIndex`, `DurationIndex`, `Effect_1`, `ImplicitTargetA_1`, `EffectAura_1`, `SpellVisualID_1`, `SpellIconID`, `ActiveIconID`, `Name_Lang_deDE`, `Description_Lang_deDE`, `AuraDescription_Lang_deDE`, `SchoolMask`)
SELECT
  `ID`, `Attributes`, `RangeIndex`, `DurationIndex`, `Effect_1`, `ImplicitTargetA_1`, `EffectAura_1`, `SpellVisualID_1`, `SpellIconID`, `ActiveIconID`, `Name_Lang_deDE`, `Description_Lang_deDE`, `AuraDescription_Lang_deDE`, `SchoolMask`
FROM `_wydbc_spell`
WHERE `ID` BETWEEN 89601 AND 89604;


-- 注意：
-- 深渊遗物 / 神器的真实触发时机由 mod-abyss-cultivation 运行时逻辑负责，
-- 不应再通过 item_template.spellid_1 做装备触发绑定，否则会出现“装备时先触发一次”的错误表现。
UPDATE `item_template`
SET
  `spellid_1` = 0,
  `spelltrigger_1` = 0,
  `spellid_2` = 0,
  `spelltrigger_2` = 0,
  `spellid_3` = 0,
  `spelltrigger_3` = 0,
  `spellcharges_1` = 0,
  `spellcharges_2` = 0,
  `spellcharges_3` = 0,
  `spellppmRate_1` = 0,
  `spellppmRate_2` = 0,
  `spellppmRate_3` = 0,
  `spellcooldown_1` = -1,
  `spellcooldown_2` = -1,
  `spellcooldown_3` = -1,
  `spellcategory_1` = 0,
  `spellcategory_2` = 0,
  `spellcategory_3` = 0,
  `spellcategorycooldown_1` = -1,
  `spellcategorycooldown_2` = -1,
  `spellcategorycooldown_3` = -1
WHERE (`entry` BETWEEN 950001 AND 950074)
   OR (`entry` BETWEEN 960001 AND 960006)
   OR `entry` = 970001;

-- 遗物 / 神器 tooltip：直接显示 Spell.dbc 描述，插件顶部面板不再重复拼接
UPDATE `item_template`
SET `description` = ''
WHERE (`entry` BETWEEN 950001 AND 950074)
   OR (`entry` BETWEEN 960001 AND 960006)
   OR `entry` = 970001;

UPDATE `item_template`
SET
  `spellid_1` = 89601,
  `spelltrigger_1` = 1,
  `spellcharges_1` = 0,
  `spellppmRate_1` = 0,
  `spellcooldown_1` = -1,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = -1,
  `spellid_2` = 89602,
  `spelltrigger_2` = 1,
  `spellcharges_2` = 0,
  `spellppmRate_2` = 0,
  `spellcooldown_2` = -1,
  `spellcategory_2` = 0,
  `spellcategorycooldown_2` = -1,
  `spellid_3` = 89100 + (`entry` - 950000),
  `spelltrigger_3` = 1,
  `spellcharges_3` = 0,
  `spellppmRate_3` = 0,
  `spellcooldown_3` = -1,
  `spellcategory_3` = 0,
  `spellcategorycooldown_3` = -1
WHERE `entry` BETWEEN 950001 AND 950074;

UPDATE `item_template`
SET
  `spellid_1` = 89603,
  `spelltrigger_1` = 1,
  `spellcharges_1` = 0,
  `spellppmRate_1` = 0,
  `spellcooldown_1` = -1,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = -1,
  `spellid_2` = 89174 + (`entry` - 960000),
  `spelltrigger_2` = 1,
  `spellcharges_2` = 0,
  `spellppmRate_2` = 0,
  `spellcooldown_2` = -1,
  `spellcategory_2` = 0,
  `spellcategorycooldown_2` = -1
WHERE `entry` BETWEEN 960001 AND 960006;

UPDATE `item_template`
SET
  `spellid_1` = 89604,
  `spelltrigger_1` = 1,
  `spellcharges_1` = 0,
  `spellppmRate_1` = 0,
  `spellcooldown_1` = -1,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = -1,
  `spellid_2` = 89181,
  `spelltrigger_2` = 1,
  `spellcharges_2` = 0,
  `spellppmRate_2` = 0,
  `spellcooldown_2` = -1,
  `spellcategory_2` = 0,
  `spellcategorycooldown_2` = -1
WHERE `entry` = 970001;


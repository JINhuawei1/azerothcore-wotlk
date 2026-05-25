-- ============================================
-- 套装配置表：由C++模拟套装效果，不依赖客户端DBC
-- 套装ID编号规则: 幕ID * 100 + 来源模式 * 10
-- ============================================

DROP TABLE IF EXISTS `_深渊套装配置`;
CREATE TABLE `_深渊套装配置` (
  `套装ID` int unsigned NOT NULL COMMENT '内部套装ID',
  `套装名称` varchar(64) NOT NULL DEFAULT '' COMMENT '套装名称',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '幕ID',
  `来源模式` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=正传 2=深渊 3=腐化 4=轮回 5=神器',
  `2件效果描述` varchar(255) NOT NULL DEFAULT '' COMMENT '穿2件触发效果',
  `2件全属性加成` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件全属性加值',
  `2件暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件暴击等级加值',
  `2件急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件急速等级加值',
  `2件攻击强度` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件攻击强度加值',
  `2件法术强度` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件法术强度加值',
  `4件效果描述` varchar(255) NOT NULL DEFAULT '' COMMENT '穿4件触发效果',
  `4件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件全伤害加成%',
  `4件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件最大生命值加成%',
  `4件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件额外暴击等级',
  `4件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件额外急速等级',
  `4件特殊效果` varchar(128) NOT NULL DEFAULT '' COMMENT '4件特殊效果脚本标识',
  `6件效果描述` varchar(255) NOT NULL DEFAULT '' COMMENT '穿6件触发效果',
  `6件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件全伤害加成%',
  `6件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件最大生命值加成%',
  `6件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件额外暴击等级',
  `6件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件额外急速等级',
  `6件特殊效果` varchar(128) NOT NULL DEFAULT '' COMMENT '6件特殊效果脚本标识',
  `8件效果描述` varchar(255) NOT NULL DEFAULT '' COMMENT '穿8件触发效果',
  `8件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件全伤害加成%',
  `8件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件最大生命值加成%',
  `8件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件额外暴击等级',
  `8件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件额外急速等级',
  `8件特殊效果` varchar(128) NOT NULL DEFAULT '' COMMENT '8件特殊效果脚本标识',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`套装ID`),
  KEY `索引_幕模式` (`幕ID`, `来源模式`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊套装配置(C++模拟)';

ALTER TABLE `_深渊套装配置`
  MODIFY COLUMN `2件全属性加成` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件全属性加值',
  MODIFY COLUMN `2件暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件暴击等级加值',
  MODIFY COLUMN `2件急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件急速等级加值',
  MODIFY COLUMN `2件攻击强度` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件攻击强度加值',
  MODIFY COLUMN `2件法术强度` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '2件法术强度加值',
  MODIFY COLUMN `4件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件全伤害加成%',
  MODIFY COLUMN `4件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件最大生命值加成%',
  MODIFY COLUMN `4件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件额外暴击等级',
  MODIFY COLUMN `4件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '4件额外急速等级',
  MODIFY COLUMN `6件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件全伤害加成%',
  MODIFY COLUMN `6件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件最大生命值加成%',
  MODIFY COLUMN `6件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件额外暴击等级',
  MODIFY COLUMN `6件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '6件额外急速等级',
  MODIFY COLUMN `8件伤害加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件全伤害加成%',
  MODIFY COLUMN `8件生命值加成百分比` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件最大生命值加成%',
  MODIFY COLUMN `8件额外暴击等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件额外暴击等级',
  MODIFY COLUMN `8件额外急速等级` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '8件额外急速等级';

DELETE FROM `_深渊套装配置` WHERE `套装ID` BETWEEN 110 AND 650;
INSERT INTO `_深渊套装配置`
(`套装ID`, `套装名称`, `幕ID`, `来源模式`,
 `2件效果描述`, `2件全属性加成`, `2件暴击等级`, `2件急速等级`, `2件攻击强度`, `2件法术强度`,
 `4件效果描述`, `4件伤害加成百分比`, `4件生命值加成百分比`, `4件额外暴击等级`, `4件额外急速等级`, `4件特殊效果`,
 `是否启用`) VALUES
-- 第一幕
(110, '灰烬试炼', 1, 1,
 '全属性+10，暴击等级+14', 10, 14, 0, 0, 0,
 '全伤害+4%，近战/法术暴击后额外造成一次35%伤害回响', 4, 0, 0, 0, '套装_灰烬回响', 1),
(120, '渊焰试炼', 1, 2,
 '全属性+12，暴击等级+18，急速等级+8', 12, 18, 8, 0, 0,
 '全伤害+5%，近战/法术暴击后额外造成一次强化回响', 5, 0, 8, 8, '套装_灰烬回响', 1),
(130, '腐焰浸染', 1, 3,
 '全属性+14，暴击等级+22，急速等级+10', 14, 22, 10, 0, 0,
 '全伤害+6%，击杀敌人恢复3%生命值和法力值', 6, 0, 0, 0, '套装_腐焰噬魂', 1),
(140, '轮焰涅槃', 1, 4,
 '全属性+18，暴击等级+28，急速等级+14', 18, 28, 14, 0, 0,
 '全伤害+8%，生命值+3%，暴击时12%概率触发双倍暴击', 8, 3, 0, 0, '套装_轮焰双爆', 1),
-- 第二幕
(210, '远征荣耀', 2, 1,
 '全属性+14，攻击强度+28，法术强度+20', 14, 0, 0, 28, 20,
 '全伤害+5%，连续攻击同一目标叠加伤害(最高3层各+2%)', 5, 0, 0, 0, '套装_远征聚焦', 1),
(220, '渊征战歌', 2, 2,
 '全属性+18，攻击强度+35，法术强度+24', 18, 0, 0, 35, 24,
 '全伤害+7%，连续攻击同一目标时更快叠加压制层数', 7, 0, 8, 8, '套装_远征聚焦', 1),
(230, '血誓枷锁', 2, 3,
 '全属性+20，攻击强度+40，法术强度+30', 20, 0, 0, 40, 30,
 '全伤害+8%，受到致命伤害时有25%概率免死并恢复15%生命值(75秒冷却)', 8, 0, 0, 0, '套装_血誓不灭', 1),
(240, '征魂轮转', 2, 4,
 '全属性+25，攻击强度+52，法术强度+38', 25, 0, 0, 52, 38,
 '全伤害+10%，生命值+5%，每12秒获得一次爆发：下一个技能伤害提高60%', 10, 5, 0, 0, '套装_征魂爆发', 1),
-- 第三幕
(310, '寒夜深渊', 3, 1,
 '全属性+20，暴击等级+24，急速等级+18', 20, 24, 18, 0, 0,
 '全伤害+6%，攻击附带冰霜效果使目标减速25%持续3秒', 6, 0, 0, 0, '套装_寒夜冻结', 1),
(320, '渊霜凝界', 3, 2,
 '全属性+24，暴击等级+30，急速等级+22', 24, 30, 22, 0, 0,
 '全伤害+8%，攻击附带更强冰霜压制并提高冻结触发率', 8, 0, 10, 10, '套装_寒夜冻结', 1),
(330, '冥寒侵蚀', 3, 3,
 '全属性+28，暴击等级+38，急速等级+26', 28, 38, 26, 0, 0,
 '全伤害+10%，冰冻目标受到的伤害额外+10%', 10, 0, 0, 0, '套装_冥寒碎冰', 1),
(340, '玄霜极寒', 3, 4,
 '全属性+34，暴击等级+46，急速等级+32', 34, 46, 32, 0, 0,
 '全伤害+12%，生命值+6%，冰霜攻击有15%概率冻结目标2秒', 12, 6, 0, 0, '套装_玄霜封印', 1),
-- 第四幕
(410, '古神低语', 4, 1,
 '全属性+24，攻击强度+42，法术强度+35', 24, 0, 0, 42, 35,
 '全伤害+7%，暴击后获得古神之力：全属性+30持续8秒', 7, 0, 0, 0, '套装_古神觉醒', 1),
(420, '渊蚀低语', 4, 2,
 '全属性+30，攻击强度+52，法术强度+42', 30, 0, 0, 52, 42,
 '全伤害+10%，暴击后更容易触发古神之力，并附带少量暴击/急速', 10, 0, 10, 8, '套装_古神觉醒', 1),
(430, '渎神残响', 4, 3,
 '全属性+36，攻击强度+60，法术强度+48', 36, 0, 0, 60, 48,
 '全伤害+11%，伤害的2%转化为护盾(最高为最大生命值12%)', 11, 0, 0, 0, '套装_渎神护盾', 1),
(440, '神蚀吞天', 4, 4,
 '全属性+44，攻击强度+78，法术强度+64', 44, 0, 0, 78, 64,
 '全伤害+14%，生命值+8%，每次暴击吸收目标1%当前生命值', 14, 8, 0, 0, '套装_神蚀吸收', 1),
-- 第五幕
(510, '余烬残火', 5, 1,
 '全属性+32，暴击等级+40，急速等级+28', 32, 40, 28, 0, 0,
 '全伤害+8%，击杀敌人后8秒内攻击速度+12%', 8, 0, 0, 0, '套装_余烬狂暴', 1),
(520, '渊烬残火', 5, 2,
 '全属性+38，暴击等级+48，急速等级+34', 38, 48, 34, 0, 0,
 '全伤害+12%，击杀敌人后更快进入狂暴状态并追加暴击/急速', 12, 0, 12, 12, '套装_余烬狂暴', 1),
(530, '劫烬焚世', 5, 3,
 '全属性+44，暴击等级+58，急速等级+40', 44, 58, 40, 0, 0,
 '全伤害+13%，受到伤害时有8%概率反弹140%伤害给攻击者', 13, 0, 0, 0, '套装_劫烬反噬', 1),
(540, '烬世轮回', 5, 4,
 '全属性+55，暴击等级+70，急速等级+50', 55, 70, 50, 0, 0,
 '全伤害+16%，生命值+10%，战斗中每6秒全属性永久+1(离开战斗重置)', 16, 10, 0, 0, '套装_烬世叠加', 1),
-- 第六幕
(610, '终焉审判', 6, 1,
 '全属性+40，暴击等级+50，攻击强度+55，法术强度+46', 40, 50, 0, 55, 46,
 '全伤害+10%，每36秒触发一次终焉审判：对周围所有敌人造成260%攻击力伤害', 10, 0, 0, 0, '套装_终焉天罚', 1),
(620, '渊灭审判', 6, 2,
 '全属性+50，暴击等级+62，攻击强度+70，法术强度+58', 50, 62, 0, 70, 58,
 '全伤害+14%，终焉审判触发更频繁，并追加生命/暴击收益', 14, 4, 14, 0, '套装_终焉天罚', 1),
(630, '灭界降临', 6, 3,
 '全属性+56，暴击等级+72，攻击强度+82，法术强度+68', 56, 72, 0, 82, 68,
 '全伤害+16%，穿戴4件后解锁灭界形态：持续10秒全属性提高60%(150秒冷却)', 16, 0, 0, 0, '套装_灭界形态', 1),
(640, '归墟终焉', 6, 4,
 '全属性+70，暴击等级+88，攻击强度+108，法术强度+88', 70, 88, 0, 108, 88,
 '全伤害+19%，生命值+12%，死亡时自动复活并进入6秒统御爆发(300秒冷却)', 19, 12, 0, 0, '套装_归墟不灭', 1),
-- 神器套装：独立于轮回套装的秘藏专用套装线
(150, '焚界秘藏', 1, 5,
 '全属性+24，暴击等级+36，急速等级+18', 24, 36, 18, 0, 0,
 '全伤害+10%，生命值+4%，攻击/施法时有概率触发焚界天爆，额外造成一次火焰追击并回复少量主资源', 10, 4, 12, 12, '套装_焚界天爆', 1),
(250, '征魂秘藏', 2, 5,
 '全属性+30，攻击强度+60，法术强度+44', 30, 0, 0, 60, 44,
 '全伤害+12%，生命值+6%，每10秒触发征魂统御，对当前目标追加一次统御爆裂并回复少量主资源', 12, 6, 12, 12, '套装_征魂统御', 1),
(350, '永霜秘藏', 3, 5,
 '全属性+38，暴击等级+52，急速等级+38', 38, 52, 38, 0, 0,
 '全伤害+14%，生命值+8%，冰霜攻击较高概率触发永冻裁决，造成冻结爆裂', 14, 8, 14, 14, '套装_永冻裁决', 1),
(450, '渊神秘藏', 4, 5,
 '全属性+48，攻击强度+90，法术强度+72', 48, 0, 0, 90, 72,
 '全伤害+17%，生命值+10%，暴击时触发渊神吞界，对目标造成暗影吞噬并回复生命/资源', 17, 10, 16, 16, '套装_渊神吞界', 1),
(550, '劫烬秘藏', 5, 5,
 '全属性+60，暴击等级+78，急速等级+56', 60, 78, 56, 0, 0,
 '全伤害+20%，生命值+12%，战斗中每5秒叠加1层烬灭永燃，层数上限与成长速度小幅提高', 20, 12, 18, 18, '套装_烬灭永燃', 1),
(650, '归墟秘藏', 6, 5,
 '全属性+78，暴击等级+96，攻击强度+120', 78, 96, 0, 120, 0,
 '全伤害+22%，生命值+14%，死亡时触发归墟主宰复苏，并进入较长时间的统御爆发(240秒冷却)', 22, 14, 20, 20, '套装_归墟主宰', 1);

-- 6件 / 8件效果：基于当前4件套主题继续强化，作为完整套装阶段
UPDATE `_深渊套装配置`
SET
  `6件伤害加成百分比` = LEAST(`4件伤害加成百分比` + CASE `来源模式` WHEN 5 THEN 6 WHEN 4 THEN 5 WHEN 3 THEN 4 WHEN 2 THEN 3 ELSE 2 END, 32),
  `6件生命值加成百分比` = LEAST(`4件生命值加成百分比` + CASE WHEN `4件生命值加成百分比` > 0 THEN 3 ELSE 2 END, 20),
  `6件额外暴击等级` = `4件额外暴击等级` + CASE `来源模式` WHEN 5 THEN 18 WHEN 4 THEN 16 WHEN 3 THEN 14 ELSE 12 END,
  `6件额外急速等级` = `4件额外急速等级` + CASE `来源模式` WHEN 5 THEN 18 WHEN 4 THEN 16 WHEN 3 THEN 14 ELSE 12 END,
  `6件特殊效果` = `4件特殊效果`,
  `6件效果描述` = CONCAT(
    '在4件效果基础上，',
    '全伤害+',
    LEAST(`4件伤害加成百分比` + CASE `来源模式` WHEN 5 THEN 6 WHEN 4 THEN 5 WHEN 3 THEN 4 WHEN 2 THEN 3 ELSE 2 END, 32),
    '%',
    CASE
      WHEN LEAST(`4件生命值加成百分比` + CASE WHEN `4件生命值加成百分比` > 0 THEN 3 ELSE 2 END, 20) > 0
        THEN CONCAT('，生命值+', LEAST(`4件生命值加成百分比` + CASE WHEN `4件生命值加成百分比` > 0 THEN 3 ELSE 2 END, 20), '%')
      ELSE ''
    END,
    '，暴击等级+',
    `4件额外暴击等级` + CASE `来源模式` WHEN 5 THEN 18 WHEN 4 THEN 16 WHEN 3 THEN 14 ELSE 12 END,
    '，急速等级+',
    `4件额外急速等级` + CASE `来源模式` WHEN 5 THEN 18 WHEN 4 THEN 16 WHEN 3 THEN 14 ELSE 12 END,
    CASE WHEN `4件特殊效果` <> '' THEN CONCAT('，并强化效果【', `4件特殊效果`, '】') ELSE '' END
  ),
  `8件伤害加成百分比` = LEAST(`4件伤害加成百分比` + CASE `来源模式` WHEN 5 THEN 12 WHEN 4 THEN 10 WHEN 3 THEN 8 WHEN 2 THEN 6 ELSE 4 END, 40),
  `8件生命值加成百分比` = LEAST(`4件生命值加成百分比` + CASE WHEN `4件生命值加成百分比` > 0 THEN 6 ELSE 4 END, 28),
  `8件额外暴击等级` = `4件额外暴击等级` + CASE `来源模式` WHEN 5 THEN 36 WHEN 4 THEN 32 WHEN 3 THEN 28 ELSE 24 END,
  `8件额外急速等级` = `4件额外急速等级` + CASE `来源模式` WHEN 5 THEN 36 WHEN 4 THEN 32 WHEN 3 THEN 28 ELSE 24 END,
  `8件特殊效果` = `4件特殊效果`,
  `8件效果描述` = CONCAT(
    '在6件效果基础上，',
    '全伤害+',
    LEAST(`4件伤害加成百分比` + CASE `来源模式` WHEN 5 THEN 12 WHEN 4 THEN 10 WHEN 3 THEN 8 WHEN 2 THEN 6 ELSE 4 END, 40),
    '%',
    CASE
      WHEN LEAST(`4件生命值加成百分比` + CASE WHEN `4件生命值加成百分比` > 0 THEN 6 ELSE 4 END, 28) > 0
        THEN CONCAT('，生命值+', LEAST(`4件生命值加成百分比` + CASE WHEN `4件生命值加成百分比` > 0 THEN 6 ELSE 4 END, 28), '%')
      ELSE ''
    END,
    '，暴击等级+',
    `4件额外暴击等级` + CASE `来源模式` WHEN 5 THEN 36 WHEN 4 THEN 32 WHEN 3 THEN 28 ELSE 24 END,
    '，急速等级+',
    `4件额外急速等级` + CASE `来源模式` WHEN 5 THEN 36 WHEN 4 THEN 32 WHEN 3 THEN 28 ELSE 24 END,
    CASE WHEN `4件特殊效果` <> '' THEN CONCAT('，并完全解放效果【', `4件特殊效果`, '】') ELSE '' END
  )
WHERE `套装ID` BETWEEN 110 AND 650;

-- 原生套装 Spell 链接：
-- ItemSet 只展示 2/4/6/8 四条可见说明，4/6/8 的额外属性与脚本标记
-- 通过 spell_linked_spell 在 aura 应用/移除时自动联动 helper spell，
-- 避免客户端 tooltip 出现重复的空白“(4) 套装：”/“(6) 套装：”行。
DELETE FROM `spell_linked_spell`
WHERE `comment` LIKE '深渊套装原生化%';

INSERT INTO `spell_linked_spell` (`spell_trigger`, `spell_effect`, `type`, `comment`)
SELECT
  CASE
    WHEN `来源模式` = 5 THEN 89201 + ((24 + (`幕ID` - 1)) * 7) + 1
    ELSE 89201 + (((`幕ID` - 1) * 4 + (`来源模式` - 1)) * 7) + 1
  END AS `spell_trigger`,
  CASE
    WHEN `来源模式` = 5 THEN 89201 + ((24 + (`幕ID` - 1)) * 7) + 2
    ELSE 89201 + (((`幕ID` - 1) * 4 + (`来源模式` - 1)) * 7) + 2
  END AS `spell_effect`,
  2 AS `type`,
  CONCAT('深渊套装原生化 4件 helper: ', `套装名称`)
FROM `_深渊套装配置`
WHERE `是否启用` = 1

UNION ALL

SELECT
  CASE
    WHEN `来源模式` = 5 THEN 89201 + ((24 + (`幕ID` - 1)) * 7) + 3
    ELSE 89201 + (((`幕ID` - 1) * 4 + (`来源模式` - 1)) * 7) + 3
  END AS `spell_trigger`,
  CASE
    WHEN `来源模式` = 5 THEN 89201 + ((24 + (`幕ID` - 1)) * 7) + 4
    ELSE 89201 + (((`幕ID` - 1) * 4 + (`来源模式` - 1)) * 7) + 4
  END AS `spell_effect`,
  2 AS `type`,
  CONCAT('深渊套装原生化 6件 helper: ', `套装名称`)
FROM `_深渊套装配置`
WHERE `是否启用` = 1

UNION ALL

SELECT
  CASE
    WHEN `来源模式` = 5 THEN 89201 + ((24 + (`幕ID` - 1)) * 7) + 5
    ELSE 89201 + (((`幕ID` - 1) * 4 + (`来源模式` - 1)) * 7) + 5
  END AS `spell_trigger`,
  CASE
    WHEN `来源模式` = 5 THEN 89201 + ((24 + (`幕ID` - 1)) * 7) + 6
    ELSE 89201 + (((`幕ID` - 1) * 4 + (`来源模式` - 1)) * 7) + 6
  END AS `spell_effect`,
  2 AS `type`,
  CONCAT('深渊套装原生化 8件 helper: ', `套装名称`)
FROM `_深渊套装配置`
WHERE `是否启用` = 1;


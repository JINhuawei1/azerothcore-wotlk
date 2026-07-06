-- ============================================
-- 仙门系统 - 世界库配置
-- ============================================

CREATE TABLE IF NOT EXISTS `_仙门_门派` (
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '有效门派ID',
  `门派名称` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `门派描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `修为上限` smallint UNSIGNED NOT NULL DEFAULT 100 COMMENT '门派修为等级上限，0使用配置默认值',
  `加入需求ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '需求模板ID，0表示无要求',
  PRIMARY KEY (`门派ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门门派配置' ROW_FORMAT = DYNAMIC;

SET @xianmen_add_desc_column = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_门派' AND COLUMN_NAME = '门派描述') = 0,
  'ALTER TABLE `_仙门_门派` ADD COLUMN `门派描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '''' AFTER `门派名称`',
  'SELECT 1'
);
PREPARE xianmen_add_desc_column_stmt FROM @xianmen_add_desc_column;
EXECUTE xianmen_add_desc_column_stmt;
DEALLOCATE PREPARE xianmen_add_desc_column_stmt;

SET @xianmen_add_max_level_column = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_门派' AND COLUMN_NAME = '修为上限') = 0,
  'ALTER TABLE `_仙门_门派` ADD COLUMN `修为上限` smallint UNSIGNED NOT NULL DEFAULT 100 COMMENT ''门派修为等级上限，0使用配置默认值'' AFTER `门派描述`',
  'SELECT 1'
);
PREPARE xianmen_add_max_level_column_stmt FROM @xianmen_add_max_level_column;
EXECUTE xianmen_add_max_level_column_stmt;
DEALLOCATE PREPARE xianmen_add_max_level_column_stmt;

DELETE FROM `_仙门_门派` WHERE `门派ID` BETWEEN 1 AND 5;
INSERT INTO `_仙门_门派` (`门派ID`, `门派名称`, `门派描述`, `修为上限`, `加入需求ID`) VALUES
(1, '太虚剑宗', '剑修流派，主打物理爆发，被动触发剑气与流血；核心属性为攻击强度、暴击、切割。', 100, 0),
(2, '九霄符箓', '符法流派，主打法术爆发，被动触发多重与连锁效果；核心属性为法术强度、急速、魔次。', 100, 0),
(3, '不灭体殿', '炼体流派，主打坦克、反伤与超高耐久；核心属性为耐力、护甲、超大生命。', 100, 0),
(4, '御灵魂宗', '驭魂流派，主打武魂召唤，随门派修为提升多武魂与继承能力；核心玩法为召唤物强化、武魂继承。', 100, 0);

CREATE TABLE IF NOT EXISTS `_仙门_升级需求` (
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=全门派默认，其余为指定门派覆盖',
  `目标等级` smallint UNSIGNED NOT NULL DEFAULT 0 COMMENT '升级到该修为等级',
  `历史贡献需求` bigint UNSIGNED NOT NULL DEFAULT 0 COMMENT '达到该累计历史贡献后可升级',
  `需求模板ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联 _模板_需求.id，0表示无额外需求',
  PRIMARY KEY (`门派ID`, `目标等级`) USING BTREE,
  KEY `idx_仙门升级需求_目标等级` (`目标等级`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门修为升级需求配置' ROW_FORMAT = DYNAMIC;

DELETE FROM `_仙门_升级需求` WHERE `门派ID` = 0 AND `目标等级` BETWEEN 1 AND 1000;
INSERT INTO `_仙门_升级需求` (`门派ID`, `目标等级`, `历史贡献需求`, `需求模板ID`)
SELECT
  0,
  levels.`目标等级`,
  CASE
    WHEN levels.`目标等级` <= 1 THEN 0
    ELSE ((levels.`目标等级` - 1) * 100) + (((levels.`目标等级` - 1) * (levels.`目标等级` - 2)) / 2 * 10)
  END,
  0
FROM (
  WITH RECURSIVE xianmen_levels(`目标等级`) AS (
    SELECT 1
    UNION ALL
    SELECT `目标等级` + 1 FROM xianmen_levels WHERE `目标等级` < 1000
  )
  SELECT `目标等级` FROM xianmen_levels
) levels;

CREATE TABLE IF NOT EXISTS `_仙门_技能` (
  `技能ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '建议 382001-382999',
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `序号` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '每派 1-10',
  `技能名称` varchar(60) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `技能子类` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '1属性 2触发 3系统集成',
  `法术ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '隐藏 aura/proc spellId，DBC 未落地时可先为规划ID',
  `基础数值` decimal(20,4) NOT NULL DEFAULT 0 COMMENT '按技能语义解释',
  `每10级成长` decimal(20,4) NOT NULL DEFAULT 0 COMMENT '每 10 修为等级成长',
  `触发参数` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `技能描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`技能ID`) USING BTREE,
  KEY `idx_仙门技能_门派` (`门派ID`, `序号`)
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门技能配置' ROW_FORMAT = DYNAMIC;

DELETE FROM `_仙门_技能` WHERE `技能ID` BETWEEN 382001 AND 382999;
INSERT INTO `_仙门_技能`
(`技能ID`, `门派ID`, `序号`, `技能名称`, `技能子类`, `法术ID`, `基础数值`, `每10级成长`, `触发参数`, `技能描述`) VALUES
(382001, 1, 1, '剑心通明', 1, 382001, 20.0000, 5.0000, '常驻', '攻击强度与暴击提升'),
(382002, 1, 2, '利刃无锋', 1, 382002, 30.0000, 5.0000, '常驻', '暴击伤害与命中提升'),
(382003, 1, 3, '凌厉', 1, 382003, 10.0000, 2.0000, '常驻', '急速与近战攻速提升'),
(382004, 1, 4, '破甲剑芒', 1, 382004, 25.0000, 2.0000, '常驻', '护甲穿透与高护甲目标增伤'),
(382005, 1, 5, '剑气环绕', 2, 382005, 150.0000, 20.0000, '攻击 PPM 4', '激发剑气造成物理伤害'),
(382006, 1, 6, '嗜血剑意', 3, 382006, 30.0000, 0.0000, '攻击 30%', '命中附加切割流血，挂钩切割系统'),
(382007, 1, 7, '万剑护体', 2, 382007, 80.0000, 10.0000, '攻击 PPM 2', '召唤飞剑连击'),
(382008, 1, 8, '剑罡护身', 2, 382008, 100.0000, 15.0000, '受击 20%', '弹反剑气'),
(382009, 1, 9, '剑神之威', 1, 382009, 30.0000, 5.0000, '常驻', '物理总输出提升，对 Boss 额外增伤'),
(382010, 1, 10, '剑意通神', 1, 382010, 10.0000, 2.0000, '常驻', '全属性提升与暴击率上限突破'),

(382021, 2, 1, '符道精通', 1, 382021, 20.0000, 5.0000, '常驻', '法术强度与急速提升'),
(382022, 2, 2, '灵法贯通', 1, 382022, 5.0000, 1.0000, '常驻', '法术暴击与法术穿透提升'),
(382023, 2, 3, '法力无穷', 1, 382023, 30.0000, 5.0000, '常驻', '法力与法力回复提升'),
(382024, 2, 4, '通天符道', 1, 382024, 30.0000, 5.0000, '常驻', '法术暴击伤害与 AOE 范围提升'),
(382025, 2, 5, '烈焰缠身', 2, 382025, 150.0000, 15.0000, '命中 30%', '引爆火符 AOE'),
(382026, 2, 6, '魔次共鸣', 3, 382026, 50.0000, 5.0000, '命中 PPM 4', '触发魔次多重伤害，挂钩魔次系统'),
(382027, 2, 7, '连环天雷', 2, 382027, 120.0000, 15.0000, '命中 PPM 3', '连锁落雷最多 5 目标'),
(382028, 2, 8, '冰封符', 2, 382028, 20.0000, 0.0000, '命中 20%', '冰冻减速或定身'),
(382029, 2, 9, '灵盾护体', 2, 382029, 25.0000, 0.0000, '受击 25%', '生成法术护盾'),
(382030, 2, 10, '符箓大成', 1, 382030, 30.0000, 5.0000, '常驻', '法术总输出提升'),

(382031, 3, 1, '金刚不坏', 1, 382031, 25.0000, 5.0000, '常驻', '耐力、护甲与减伤提升'),
(382032, 3, 2, '厚土之躯', 1, 382032, 30.0000, 5.0000, '常驻', '生命与生命回复提升'),
(382033, 3, 3, '钢筋铁骨', 1, 382033, 10.0000, 1.0000, '常驻', '格挡与招架提升'),
(382034, 3, 4, '山岳之力', 1, 382034, 1.0000, 0.5000, '常驻', '按最大生命转化为攻击强度'),
(382035, 3, 5, '荆棘反伤', 2, 382035, 30.0000, 5.0000, '受击触发', '反弹受到伤害'),
(382036, 3, 6, '血气加身', 2, 382036, 1.0000, 0.2000, '攻击触发', '按最大生命附加额外伤害'),
(382037, 3, 7, '怒火反击', 2, 382037, 0.5000, 0.1000, '受击 25%', '按最大生命反击 AOE'),
(382038, 3, 8, '不动如山', 2, 382038, 50.0000, 0.0000, '受击 20%', '短时高额减伤或免控'),
(382039, 3, 9, '不灭金身', 2, 382039, 25.0000, 0.0000, '濒死触发', '血锁或护盾，被动护命'),
(382040, 3, 10, '万劫不灭体', 1, 382040, 30.0000, 5.0000, '常驻', '最大生命与反伤上限提升'),

(382041, 4, 1, '通灵之术', 3, 382041, 5.0000, 0.5000, '常驻', '武魂属性继承提升'),
(382042, 4, 2, '群灵附身', 3, 382042, 10.0000, 1.0000, '修为曲线', '按修为提升可同时召唤的武魂数量'),
(382043, 4, 3, '魂力亲和', 1, 382043, 50.0000, 10.0000, 'Boss 击杀', '获得魂力提升'),
(382044, 4, 4, '灵兽共鸣', 1, 382044, 5.0000, 1.0000, '常驻', '每个武魂提升主人全属性'),
(382045, 4, 5, '御魂之威', 1, 382045, 30.0000, 5.0000, '常驻', '武魂技能伤害与魂环效果提升'),
(382046, 4, 6, '魂环精通', 1, 382046, 8.0000, 1.0000, '常驻', '武魂魂环等级上限与效果提升'),
(382047, 4, 7, '魂体血脉', 1, 382047, 15.0000, 1.0000, '常驻', '武魂生命与护甲提升'),
(382048, 4, 8, '灵魂链接', 2, 382048, 5.0000, 0.0000, '武魂攻击触发', '武魂攻击时为主人回血或加增益'),
(382049, 4, 9, '噬魂之力', 2, 382049, 50.0000, 0.0000, '武魂击杀触发', '返还魂力并提升武魂暴击'),
(382050, 4, 10, '万魂之主', 3, 382050, 10.0000, 1.0000, '常驻', '武魂继承上限与额外召唤位提升');

CREATE TABLE IF NOT EXISTS `_仙门_仙器槽位` (
  `槽位ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位名称` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `槽位类型` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '1通用扩展槽 2仙器槽',
  `解锁需求ID` int UNSIGNED NOT NULL DEFAULT 0,
  `部位` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '扩展槽部位=官方装备槽+1:1头2颈3肩4衬衣5胸6腰7腿8脚9腕10手11戒指1 12戒指2 13饰品1 14饰品2 15披风16主手17副手18远程19战袍,仙器槽=0',
  PRIMARY KEY (`槽位ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门仙器与扩展装备槽位配置' ROW_FORMAT = DYNAMIC;

SET @xianqi_add_body_part_column = IF(
  (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_仙门_仙器槽位' AND COLUMN_NAME = '部位') = 0,
  'ALTER TABLE `_仙门_仙器槽位` ADD COLUMN `部位` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT ''扩展槽部位=官方装备槽+1:1头2颈3肩4衬衣5胸6腰7腿8脚9腕10手11戒指1 12戒指2 13饰品1 14饰品2 15披风16主手17副手18远程19战袍,仙器槽=0'' AFTER `解锁需求ID`',
  'SELECT 1'
);
PREPARE xianqi_add_body_part_column_stmt FROM @xianqi_add_body_part_column;
EXECUTE xianqi_add_body_part_column_stmt;
DEALLOCATE PREPARE xianqi_add_body_part_column_stmt;

DELETE FROM `_仙门_仙器槽位` WHERE `槽位ID` BETWEEN 1 AND 29;
INSERT INTO `_仙门_仙器槽位` (`槽位ID`, `槽位名称`, `槽位类型`, `解锁需求ID`, `部位`) VALUES
(1, '仙器槽一', 2, 0, 0), (2, '仙器槽二', 2, 0, 0), (3, '仙器槽三', 2, 0, 0), (4, '仙器槽四', 2, 0, 0), (5, '仙器槽五', 2, 0, 0),
(6, '仙器槽六', 2, 0, 0), (7, '仙器槽七', 2, 0, 0), (8, '仙器槽八', 2, 0, 0), (9, '仙器槽九', 2, 0, 0), (10, '仙器槽十', 2, 0, 0),
(11, '头部扩展槽', 1, 0, 1), (12, '颈部扩展槽', 1, 0, 2), (13, '肩部扩展槽', 1, 0, 3), (14, '衬衣扩展槽', 1, 0, 4), (15, '胸部扩展槽', 1, 0, 5),
(16, '腰带扩展槽', 1, 0, 6), (17, '腿部扩展槽', 1, 0, 7), (18, '脚部扩展槽', 1, 0, 8), (19, '护腕扩展槽', 1, 0, 9), (20, '手套扩展槽', 1, 0, 10),
(21, '戒指扩展槽1', 1, 0, 11), (22, '戒指扩展槽2', 1, 0, 12), (23, '饰品扩展槽1', 1, 0, 13), (24, '饰品扩展槽2', 1, 0, 14), (25, '披风扩展槽', 1, 0, 15),
(26, '主手扩展槽', 1, 0, 16), (27, '副手扩展槽', 1, 0, 17), (28, '远程扩展槽', 1, 0, 18), (29, '战袍扩展槽', 1, 0, 19);

CREATE TABLE IF NOT EXISTS `_仙门_日常` (
  `模板ID` int UNSIGNED NOT NULL DEFAULT 0,
  `模板名称` varchar(80) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `模板类型` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '1讨伐 2狩猎 3采集 4上缴 5副本 6试炼 7论道 8历练 9护法 10悬赏',
  `目标参数` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `默认数量` int UNSIGNED NOT NULL DEFAULT 1,
  `难度系数` float NOT NULL DEFAULT 1,
  `默认奖励档` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '1低 2中 3高',
  PRIMARY KEY (`模板ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门日常配置' ROW_FORMAT = DYNAMIC;

DELETE FROM `_仙门_日常` WHERE `模板ID` BETWEEN 96001 AND 96200;
INSERT INTO `_仙门_日常` (`模板ID`, `模板名称`, `模板类型`, `目标参数`, `默认数量`, `难度系数`, `默认奖励档`) VALUES
(96001, '斩妖除魔', 1, 'Boss entry', 3, 1.0, 2),
(96003, '清剿邪修', 2, 'zone/creature family', 100, 0.5, 1),
(96005, '采灵草', 3, 'item entry', 30, 0.5, 1),
(96006, '凿灵矿', 3, 'item entry', 30, 0.5, 1),
(96007, '门派供奉', 4, 'item/currency', 1, 0.8, 2),
(96009, '荡涤秘境', 5, 'instance/boss', 1, 1.5, 3),
(96010, '登天试炼', 6, 'challenge floor', 10, 1.0, 2),
(96011, '论剑斗法', 7, 'pvp kill', 5, 1.0, 2),
(96012, '山河历练', 8, 'area/coord', 3, 0.5, 1),
(96015, '缉拿要犯', 10, 'rare entry', 1, 1.3, 3);

CREATE TABLE IF NOT EXISTS `_仙门_奖励` (
  `奖励类型` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '1碎片 2贡献 3货币 4材料 5材料箱 6稀有材料',
  `奖励名称` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `低档数量` int UNSIGNED NOT NULL DEFAULT 0,
  `中档数量` int UNSIGNED NOT NULL DEFAULT 0,
  `高档数量` int UNSIGNED NOT NULL DEFAULT 0,
  `最高概率` float NOT NULL DEFAULT 100,
  `物品ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'item_template.entry，0表示不发物品',
  PRIMARY KEY (`奖励类型`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门日常奖励配置' ROW_FORMAT = DYNAMIC;

DELETE FROM `_仙门_奖励` WHERE `奖励类型` BETWEEN 1 AND 6;
INSERT INTO `_仙门_奖励` (`奖励类型`, `奖励名称`, `低档数量`, `中档数量`, `高档数量`, `最高概率`, `物品ID`) VALUES
(1, '合成碎片', 10, 80, 200, 100, 0),
(2, '门派贡献点', 50, 120, 300, 100, 0),
(3, '货币', 5, 15, 40, 100, 0),
(4, '门派材料/修为经验', 1, 3, 8, 100, 0),
(5, '普通材料箱', 1, 1, 2, 100, 0),
(6, '稀有材料', 0, 0, 1, 5, 0);

DROP TABLE IF EXISTS `_仙门_合成配方`;
DROP TABLE IF EXISTS `_仙门_丹方`;
DROP TABLE IF EXISTS `_仙门_丹药效果`;

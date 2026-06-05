-- ============================================
-- 仙门系统 - 世界库配置
-- ============================================

CREATE TABLE IF NOT EXISTS `_仙门_门派` (
  `门派ID` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '1-5',
  `门派名称` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `门派描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
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

DELETE FROM `_仙门_门派` WHERE `门派ID` BETWEEN 1 AND 5;
INSERT INTO `_仙门_门派` (`门派ID`, `门派名称`, `门派描述`, `加入需求ID`) VALUES
(1, '太虚剑宗', '剑修流派，主打物理爆发，被动触发剑气与流血；核心属性为攻击强度、暴击、切割。', 0),
(2, '万象丹鼎', '丹道流派，主打采集与炼丹，通过属性丹永久累积属性；核心玩法为采集产出、炼丹、永久属性提升。', 0),
(3, '九霄符箓', '符法流派，主打法术爆发，被动触发多重与连锁效果；核心属性为法术强度、急速、魔次。', 0),
(4, '不灭体殿', '炼体流派，主打坦克、反伤与超高耐久；核心属性为耐力、护甲、超大生命。', 0),
(5, '御灵魂宗', '驭魂流派，主打武魂召唤，随门派修为提升多武魂与继承能力；核心玩法为召唤物强化、武魂继承。', 0);

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
(382004, 1, 4, '破甲剑芒', 1, 382004, 0.0000, 0.0000, '常驻', '护甲穿透与高护甲目标增伤'),
(382005, 1, 5, '剑气环绕', 2, 382005, 150.0000, 20.0000, '攻击 PPM 4', '激发剑气造成物理伤害'),
(382006, 1, 6, '嗜血剑意', 3, 382006, 30.0000, 0.0000, '攻击 30%', '命中附加切割流血，挂钩切割系统'),
(382007, 1, 7, '万剑护体', 2, 382007, 80.0000, 10.0000, '攻击 PPM 2', '召唤飞剑连击'),
(382008, 1, 8, '剑罡护身', 2, 382008, 100.0000, 15.0000, '受击 20%', '弹反剑气'),
(382009, 1, 9, '剑神之威', 1, 382009, 30.0000, 5.0000, '常驻', '物理总输出提升，对 Boss 额外增伤'),
(382010, 1, 10, '剑意通神', 1, 382010, 10.0000, 2.0000, '常驻', '全属性提升与暴击率上限突破'),

(382011, 2, 1, '灵农之手', 1, 382011, 50.0000, 10.0000, '常驻', '采药、采矿、剥皮产出与速度提升'),
(382012, 2, 2, '丹道精通', 1, 382012, 20.0000, 3.0000, '常驻', '炼丹成功率与产出提升'),
(382013, 2, 3, '药力充盈', 1, 382013, 20.0000, 4.0000, '常驻', '吃丹获得属性提升'),
(382014, 2, 4, '固元培本', 1, 382014, 0.0000, 0.0000, '常驻', '丹药属性累积上限提升'),
(382015, 2, 5, '点石成金', 1, 382015, 0.0000, 0.0000, '采集触发', '采集额外掉落稀有炼丹材料'),
(382016, 2, 6, '火候纯青', 1, 382016, 0.0000, 0.0000, '炼丹触发', '炼丹暴击双倍产出'),
(382017, 2, 7, '妙手回春', 3, 382017, 0.0000, 0.0000, '战斗持续', '自动回血，挂钩回血系统'),
(382018, 2, 8, '百毒不侵', 2, 382018, 0.0000, 0.0000, '受击触发', '净化自身 debuff 并提升抗性'),
(382019, 2, 9, '丹圣之体', 1, 382019, 30.0000, 5.0000, '常驻', '生命值与全属性提升'),
(382020, 2, 10, '仙丹妙药', 1, 382020, 0.0000, 0.0000, '吃丹触发', '吃丹时回血并获得临时全属性增益'),

(382021, 3, 1, '符道精通', 1, 382021, 20.0000, 5.0000, '常驻', '法术强度与急速提升'),
(382022, 3, 2, '灵法贯通', 1, 382022, 5.0000, 1.0000, '常驻', '法术暴击与法术穿透提升'),
(382023, 3, 3, '法力无穷', 1, 382023, 30.0000, 5.0000, '常驻', '法力与法力回复提升'),
(382024, 3, 4, '通天符道', 1, 382024, 30.0000, 5.0000, '常驻', '法术暴击伤害与 AOE 范围提升'),
(382025, 3, 5, '烈焰缠身', 2, 382025, 150.0000, 15.0000, '命中 30%', '引爆火符 AOE'),
(382026, 3, 6, '魔次共鸣', 3, 382026, 0.0000, 0.0000, '命中触发', '触发魔次多重伤害，挂钩魔次系统'),
(382027, 3, 7, '连环天雷', 2, 382027, 120.0000, 15.0000, '命中 PPM 3', '连锁落雷最多 5 目标'),
(382028, 3, 8, '冰封符', 2, 382028, 20.0000, 0.0000, '命中 20%', '冰冻减速或定身'),
(382029, 3, 9, '灵盾护体', 2, 382029, 25.0000, 0.0000, '受击 25%', '生成法术护盾'),
(382030, 3, 10, '符箓大成', 1, 382030, 30.0000, 5.0000, '常驻', '法术总输出提升'),

(382031, 4, 1, '金刚不坏', 1, 382031, 25.0000, 5.0000, '常驻', '耐力、护甲与减伤提升'),
(382032, 4, 2, '厚土之躯', 1, 382032, 30.0000, 5.0000, '常驻', '生命与生命回复提升'),
(382033, 4, 3, '钢筋铁骨', 1, 382033, 0.0000, 0.0000, '常驻', '格挡与招架提升'),
(382034, 4, 4, '山岳之力', 1, 382034, 0.0000, 0.0000, '常驻', '按最大生命转化为攻击强度'),
(382035, 4, 5, '荆棘反伤', 2, 382035, 30.0000, 5.0000, '受击触发', '反弹受到伤害'),
(382036, 4, 6, '血气加身', 2, 382036, 1.0000, 0.2000, '攻击触发', '按最大生命附加额外伤害'),
(382037, 4, 7, '怒火反击', 2, 382037, 0.5000, 0.1000, '受击 25%', '按最大生命反击 AOE'),
(382038, 4, 8, '不动如山', 2, 382038, 50.0000, 0.0000, '受击 20%', '短时高额减伤或免控'),
(382039, 4, 9, '不灭金身', 2, 382039, 0.0000, 0.0000, '濒死触发', '血锁或护盾，被动护命'),
(382040, 4, 10, '万劫不灭体', 1, 382040, 30.0000, 5.0000, '常驻', '最大生命与反伤上限提升'),

(382041, 5, 1, '通灵之术', 3, 382041, 0.0000, 0.0000, '常驻', '武魂属性继承提升'),
(382042, 5, 2, '群灵附身', 3, 382042, 0.0000, 0.0000, '修为曲线', '按修为提升可同时召唤的武魂数量'),
(382043, 5, 3, '魂力亲和', 1, 382043, 50.0000, 10.0000, 'Boss 击杀', '获得魂力提升'),
(382044, 5, 4, '灵兽共鸣', 1, 382044, 5.0000, 1.0000, '常驻', '每个武魂提升主人全属性'),
(382045, 5, 5, '御魂之威', 1, 382045, 30.0000, 5.0000, '常驻', '武魂技能伤害与魂环效果提升'),
(382046, 5, 6, '魂环精通', 1, 382046, 0.0000, 0.0000, '常驻', '武魂魂环等级上限与效果提升'),
(382047, 5, 7, '魂体血脉', 1, 382047, 0.0000, 0.0000, '常驻', '武魂生命与护甲提升'),
(382048, 5, 8, '灵魂链接', 2, 382048, 0.0000, 0.0000, '武魂攻击触发', '武魂攻击时为主人回血或加增益'),
(382049, 5, 9, '噬魂之力', 2, 382049, 0.0000, 0.0000, '武魂击杀触发', '返还魂力并提升武魂暴击'),
(382050, 5, 10, '万魂之主', 3, 382050, 0.0000, 0.0000, '常驻', '武魂继承上限与额外召唤位提升');

CREATE TABLE IF NOT EXISTS `_仙门_仙器槽位` (
  `槽位ID` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `槽位名称` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `槽位类型` tinyint UNSIGNED NOT NULL DEFAULT 1 COMMENT '1通用扩展槽 2仙器槽',
  `解锁需求ID` int UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`槽位ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门仙器与扩展装备槽位配置' ROW_FORMAT = DYNAMIC;

DELETE FROM `_仙门_仙器槽位` WHERE `槽位ID` BETWEEN 1 AND 23;
INSERT INTO `_仙门_仙器槽位` (`槽位ID`, `槽位名称`, `槽位类型`, `解锁需求ID`) VALUES
(1, '仙器槽一', 2, 0), (2, '仙器槽二', 2, 0), (3, '仙器槽三', 2, 0), (4, '仙器槽四', 2, 0), (5, '仙器槽五', 2, 0),
(6, '仙器槽六', 2, 0), (7, '仙器槽七', 2, 0), (8, '仙器槽八', 2, 0), (9, '仙器槽九', 2, 0), (10, '仙器槽十', 2, 0),
(11, '扩展槽一', 1, 0), (12, '扩展槽二', 1, 0), (13, '扩展槽三', 1, 0), (14, '扩展槽四', 1, 0), (15, '扩展槽五', 1, 0),
(16, '扩展槽六', 1, 0), (17, '扩展槽七', 1, 0), (18, '扩展槽八', 1, 0), (19, '扩展槽九', 1, 0), (20, '扩展槽十', 1, 0),
(21, '扩展槽十一', 1, 0), (22, '扩展槽十二', 1, 0), (23, '扩展槽十三', 1, 0);

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
(4, '炼丹材料/修为经验', 1, 3, 8, 100, 0),
(5, '普通材料箱', 1, 1, 2, 100, 0),
(6, '稀有材料', 0, 0, 1, 5, 0);

CREATE TABLE IF NOT EXISTS `_仙门_合成配方` (
  `配方ID` int UNSIGNED NOT NULL DEFAULT 0,
  `注释` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `源物品ID` int UNSIGNED NOT NULL DEFAULT 0,
  `源物品数量` int UNSIGNED NOT NULL DEFAULT 5,
  `产出物品ID` int UNSIGNED NOT NULL DEFAULT 0,
  `成功率` float UNSIGNED NOT NULL DEFAULT 100,
  `需求模板ID` int UNSIGNED NOT NULL DEFAULT 0,
  `鉴定等级编号` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '20=全属性',
  `属性百分比最小` float UNSIGNED NOT NULL DEFAULT 10,
  `属性百分比最大` float UNSIGNED NOT NULL DEFAULT 1000,
  `失败处理` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0扣材料 1保留材料',
  PRIMARY KEY (`配方ID`) USING BTREE,
  KEY `idx_仙门合成_源物品` (`源物品ID`)
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门 N 件相同装备合成配方' ROW_FORMAT = DYNAMIC;

CREATE TABLE IF NOT EXISTS `_仙门_丹方` (
  `丹方ID` int UNSIGNED NOT NULL DEFAULT 0,
  `丹方名称` varchar(80) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `产出丹药ID` int UNSIGNED NOT NULL DEFAULT 0,
  `产出数量` int UNSIGNED NOT NULL DEFAULT 1,
  `需求模板ID` int UNSIGNED NOT NULL DEFAULT 0,
  `解锁修为等级` int UNSIGNED NOT NULL DEFAULT 0,
  `成功率` float UNSIGNED NOT NULL DEFAULT 100,
  `失败处理` tinyint UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`丹方ID`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门丹方配置' ROW_FORMAT = DYNAMIC;

-- 属性类型使用 ItemModType/自定义属性ID：0法力值 1生命值 3敏捷 4力量 5智力 6精神 7耐力 8真实伤害 9切割伤害 10冷却缩减 11技能伤害
-- 12防御 13躲闪 14招架 15格挡 16近战命中 17远程命中 18法术命中 19近战暴击 20远程暴击 21法术暴击 22近战被命中 23远程被命中 24法术被命中 25近战被暴击 26远程被暴击 27法术被暴击
-- 28近战急速 29远程急速 30法术急速 31命中 32暴击 33被命中 34被暴击 35韧性 36急速 37精准 38攻击强度 39远程攻击强度 43法力回复 44护甲穿透 45法术强度 46生命回复 47法术穿透 48格挡值
CREATE TABLE IF NOT EXISTS `_仙门_丹药效果` (
  `丹药ID` int UNSIGNED NOT NULL DEFAULT 0,
  `属性类型` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ItemModType/自定义属性ID，逻辑以数值为准',
  `每颗数值` decimal(39,0) NOT NULL DEFAULT 0,
  `单项上限` decimal(39,0) NOT NULL DEFAULT 0 COMMENT '0=不上限',
  PRIMARY KEY (`丹药ID`, `属性类型`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '仙门丹药永久属性配置' ROW_FORMAT = DYNAMIC;

-- ============================================
-- 修仙深渊：万劫轮回 - 世界库精简结构
-- 模块：mod-abyss-cultivation
-- 设计原则：首版优先减少表数量，尽量按“章节 / 首领 / 装备 / 遗物 / 局内效果”五大块收敛
-- ============================================

DROP TABLE IF EXISTS `_深渊首领别名`;

DROP TABLE IF EXISTS `_深渊章节配置`;
CREATE TABLE `_深渊章节配置` (
  `章节ID` smallint unsigned NOT NULL COMMENT '章节ID 1-74',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '幕ID 1-6',
  `章节名称` varchar(64) NOT NULL DEFAULT '' COMMENT '章节名称',
  `副本地图ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '副本地图ID',
  `章节类型` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '1=5人本 2=团队本',
  `需求修仙等级` smallint unsigned NOT NULL DEFAULT 0 COMMENT '修仙门槛',
  `前置章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '前置章节',
  `起始任务ID` int unsigned NOT NULL DEFAULT 0 COMMENT '起始任务ID',
  `完成任务ID` int unsigned NOT NULL DEFAULT 0 COMMENT '完成任务ID',
  `遗物物品ID` int unsigned NOT NULL DEFAULT 0 COMMENT '章节遗物物品ID',
  `掉落组ID` int unsigned NOT NULL DEFAULT 0 COMMENT '掉落组ID',
  `锚点首领入口` int unsigned NOT NULL DEFAULT 0 COMMENT '锚点首领 creature_template entry',
  `最终首领入口` int unsigned NOT NULL DEFAULT 0 COMMENT '章节最终官方首领 entry',
  `触发类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=击杀锚点 2=击杀多个关键首领 3=事件结束',
  `关键首领击杀掩码` bigint unsigned NOT NULL DEFAULT 0 COMMENT '关键首领击杀mask',
  `首通召唤规则` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '首通召唤规则',
  `重复刷召唤规则` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '重复刷召唤规则',
  `深渊首领入口` int unsigned NOT NULL DEFAULT 0 COMMENT '深渊首领入口',
  `深渊召唤地图ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '深渊召唤地图ID',
  `深渊召唤坐标X` float NOT NULL DEFAULT 0 COMMENT '深渊召唤坐标X',
  `深渊召唤坐标Y` float NOT NULL DEFAULT 0 COMMENT '深渊召唤坐标Y',
  `深渊召唤坐标Z` float NOT NULL DEFAULT 0 COMMENT '深渊召唤坐标Z',
  `深渊召唤朝向` float NOT NULL DEFAULT 0 COMMENT '深渊召唤朝向',
  `秘藏首领入口` int unsigned NOT NULL DEFAULT 0 COMMENT '秘藏首领入口',
  `秘藏基础概率` float NOT NULL DEFAULT 0 COMMENT '秘藏首领基础召唤概率',
  `秘藏保底次数` smallint unsigned NOT NULL DEFAULT 0 COMMENT '秘藏首领保底次数',
  `偏向部位掩码` int unsigned NOT NULL DEFAULT 0 COMMENT '偏向部位mask',
  `公共底材权重包` varchar(64) NOT NULL DEFAULT '' COMMENT '公共底材权重包',
  `正传基础掉率` float NOT NULL DEFAULT 0 COMMENT '正传基础掉率',
  `深渊基础掉率` float NOT NULL DEFAULT 0 COMMENT '深渊基础掉率',
  `腐化基础掉率` float NOT NULL DEFAULT 0 COMMENT '腐化基础掉率',
  `轮回基础掉率` float NOT NULL DEFAULT 0 COMMENT '轮回基础掉率',
  `隐藏房奖励加成` float NOT NULL DEFAULT 0 COMMENT '隐藏房奖励加成',
  `秘藏首领奖励加成` float NOT NULL DEFAULT 0 COMMENT '秘藏首领奖励加成',
  `深渊装保底次数` smallint unsigned NOT NULL DEFAULT 0 COMMENT '深渊装保底次数',
  `是否解锁速刷` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '是否解锁速刷',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`章节ID`),
  KEY `索引_幕ID` (`幕ID`),
  KEY `索引_副本地图ID` (`副本地图ID`),
  KEY `索引_锚点首领入口` (`锚点首领入口`),
  KEY `索引_深渊首领入口` (`深渊首领入口`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊章节主配置';

DROP TABLE IF EXISTS `_深渊首领配置`;
CREATE TABLE `_深渊首领配置` (
  `首领入口` int unsigned NOT NULL COMMENT 'creature_template entry',
  `首领名称` varchar(64) NOT NULL DEFAULT '' COMMENT '首领名称',
  `首领类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=锚点首领 2=深渊首领 3=秘藏首领',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '所属幕',
  `章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '所属章节',
  `生命倍率` float NOT NULL DEFAULT 1 COMMENT '生命倍率',
  `伤害倍率` float NOT NULL DEFAULT 1 COMMENT '伤害倍率',
  `正传模式倍率` float NOT NULL DEFAULT 1 COMMENT '正传模式倍率',
  `深渊模式倍率` float NOT NULL DEFAULT 1 COMMENT '深渊模式倍率',
  `腐化模式倍率` float NOT NULL DEFAULT 1 COMMENT '腐化模式倍率',
  `轮回模式倍率` float NOT NULL DEFAULT 1 COMMENT '轮回模式倍率',
  `阶段1血量阈值` tinyint unsigned NOT NULL DEFAULT 100 COMMENT '阶段1开始血量阈值',
  `阶段1技能组` varchar(64) NOT NULL DEFAULT '' COMMENT '阶段1技能组',
  `阶段2血量阈值` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '阶段2开始血量阈值',
  `阶段2技能组` varchar(64) NOT NULL DEFAULT '' COMMENT '阶段2技能组',
  `阶段3血量阈值` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '阶段3开始血量阈值',
  `阶段3技能组` varchar(64) NOT NULL DEFAULT '' COMMENT '阶段3技能组',
  `场地效果` varchar(64) NOT NULL DEFAULT '' COMMENT '场地效果',
  `掉落包ID` int unsigned NOT NULL DEFAULT 0 COMMENT '掉落包ID',
  `出场文本` varchar(255) NOT NULL DEFAULT '' COMMENT '出场文本',
  `死亡文本` varchar(255) NOT NULL DEFAULT '' COMMENT '死亡文本',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`首领入口`),
  KEY `索引_首领类型` (`首领类型`),
  KEY `索引_章节ID` (`章节ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊首领主配置';

DROP TABLE IF EXISTS `_深渊遗物配置`;
CREATE TABLE `_深渊遗物配置` (
  `物品ID` int unsigned NOT NULL COMMENT '遗物或神器物品ID',
  `名称` varchar(64) NOT NULL DEFAULT '' COMMENT '名称',
  `类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=章节遗物 2=阶段神器 3=终极神器',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '所属幕',
  `关联章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '关联章节',
  `激活槽位` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=主遗物 2-6=副遗物 7=阶段神器 8=终极神器',
  `激活规则` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=背包激活 2=界面激活 3=任务后自动激活',
  `互斥组` int unsigned NOT NULL DEFAULT 0 COMMENT '互斥组',
  `特效家族` varchar(32) NOT NULL DEFAULT '' COMMENT '特效家族',
  `推荐槽位` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '推荐槽位',
  `脚本组` varchar(64) NOT NULL DEFAULT '' COMMENT '脚本组',
  `副槽缩放` float NOT NULL DEFAULT 0 COMMENT '副槽缩放',
  `合成材料文本` text COMMENT '神器合成材料文本',
  `简述` varchar(255) NOT NULL DEFAULT '' COMMENT '简述',
  `完整描述` text COMMENT '完整描述',
  `权重_敏捷` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物敏捷权重',
  `权重_力量` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物力量权重',
  `权重_智力` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物智力权重',
  `权重_精神` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物精神权重',
  `权重_耐力` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物耐力权重',
  `权重_命中等级` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物命中等级权重',
  `权重_暴击等级` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物暴击等级权重',
  `权重_急速等级` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物急速等级权重',
  `权重_攻击强度` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物攻击强度权重',
  `权重_法术强度` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '遗物法术强度权重',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`物品ID`),
  KEY `索引_类型` (`类型`),
  KEY `索引_关联章节ID` (`关联章节ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊遗物与神器配置';

DROP TABLE IF EXISTS `_深渊装备模板`;
CREATE TABLE `_深渊装备模板` (
  `模板ID` int unsigned NOT NULL AUTO_INCREMENT,
  `物品模板ID` int unsigned NOT NULL DEFAULT 0 COMMENT '关联item_template.entry',
  `装备名称` varchar(64) NOT NULL DEFAULT '' COMMENT '装备名称',
  `装备类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=普通底材 2=唯一装备',
  `来源章节` smallint unsigned NOT NULL DEFAULT 0 COMMENT '来源章节',
  `来源模式` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=正传 2=深渊 3=腐化 4=轮回 5=神器套装',
  `部位掩码` int unsigned NOT NULL DEFAULT 0 COMMENT '部位mask',
  `幕ID` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '所属幕',
  `基础装等` smallint unsigned NOT NULL DEFAULT 0 COMMENT '基础装等',
  `护甲类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0无 1布 2皮 3锁 4板',
  `伤害类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0通用 1物理 2法系 3坦克 4治疗',
  `主属性预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '主属性预算最小值',
  `主属性预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '主属性预算最大值',
  `次属性预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '次属性预算最小值',
  `次属性预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '次属性预算最大值',
  `特效预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '特效预算最小值',
  `特效预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '特效预算最大值',
  `固定词缀组` varchar(64) NOT NULL DEFAULT '' COMMENT '固定词缀组',
  `固定特效ID` int unsigned NOT NULL DEFAULT 0 COMMENT '固定特效ID',
  `是否来自秘藏首领` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '是否来自秘藏首领',
  `是否需要碎片` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '是否需要碎片合成',
  `风味文本` varchar(255) NOT NULL DEFAULT '' COMMENT '风味文本',
  `套装ID` int unsigned NOT NULL DEFAULT 0 COMMENT '所属套装ID, 0=无套装',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`模板ID`),
  KEY `索引_物品模板ID` (`物品模板ID`),
  KEY `索引_来源章节` (`来源章节`),
  KEY `索引_装备类型` (`装备类型`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊装备主模板';

ALTER TABLE `_深渊装备模板`
  MODIFY COLUMN `主属性预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '主属性预算最小值',
  MODIFY COLUMN `主属性预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '主属性预算最大值',
  MODIFY COLUMN `次属性预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '次属性预算最小值',
  MODIFY COLUMN `次属性预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '次属性预算最大值',
  MODIFY COLUMN `特效预算最小值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '特效预算最小值',
  MODIFY COLUMN `特效预算最大值` decimal(39,0) unsigned NOT NULL DEFAULT 0 COMMENT '特效预算最大值';

DROP TABLE IF EXISTS `_深渊词缀模板`;
CREATE TABLE `_深渊词缀模板` (
  `词缀ID` int unsigned NOT NULL COMMENT '词缀ID',
  `词缀名称` varchar(64) NOT NULL DEFAULT '' COMMENT '词缀名称',
  `词缀类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=前缀 2=后缀 3=章节词缀',
  `词缀组` int unsigned NOT NULL DEFAULT 0 COMMENT '词缀组',
  `最小章节` smallint unsigned NOT NULL DEFAULT 0 COMMENT '最小章节',
  `最大章节` smallint unsigned NOT NULL DEFAULT 0 COMMENT '最大章节',
  `部位掩码` int unsigned NOT NULL DEFAULT 0 COMMENT '允许部位',
  `最低品质` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '最低品质',
  `数值公式` varchar(128) NOT NULL DEFAULT '' COMMENT '数值公式',
  `脚本组` varchar(64) NOT NULL DEFAULT '' COMMENT '脚本组',
  `描述` varchar(255) NOT NULL DEFAULT '' COMMENT '描述',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`词缀ID`),
  KEY `索引_词缀类型` (`词缀类型`),
  KEY `索引_章节范围` (`最小章节`, `最大章节`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊词缀模板';

DROP TABLE IF EXISTS `_深渊特效模板`;
CREATE TABLE `_深渊特效模板` (
  `特效ID` int unsigned NOT NULL COMMENT '特效ID',
  `特效名称` varchar(64) NOT NULL DEFAULT '' COMMENT '特效名称',
  `触发类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '触发类型',
  `触发参数1` int unsigned NOT NULL DEFAULT 0 COMMENT '触发参数1',
  `触发参数2` int unsigned NOT NULL DEFAULT 0 COMMENT '触发参数2',
  `冷却毫秒` int unsigned NOT NULL DEFAULT 0 COMMENT '内置冷却毫秒',
  `每分钟触发率` float NOT NULL DEFAULT 0 COMMENT '每分钟触发率',
  `特效家族` varchar(32) NOT NULL DEFAULT '' COMMENT '特效家族',
  `数值公式` varchar(128) NOT NULL DEFAULT '' COMMENT '数值公式',
  `允许部位掩码` int unsigned NOT NULL DEFAULT 0 COMMENT '允许部位',
  `脚本组` varchar(64) NOT NULL DEFAULT '' COMMENT '脚本组',
  `描述` varchar(255) NOT NULL DEFAULT '' COMMENT '描述',
  `是否启用` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '是否启用',
  PRIMARY KEY (`特效ID`),
  KEY `索引_特效家族` (`特效家族`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='深渊特效模板';

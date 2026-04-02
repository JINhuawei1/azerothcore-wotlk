-- ============================================
-- 修仙深渊：万劫轮回 - 角色库精简结构
-- 模块：mod-abyss-cultivation
-- 设计原则：当前收敛到“主数据 / 收藏 / 局内状态”三张核心表
-- ============================================

DROP TABLE IF EXISTS `_玩家深渊主数据`;
CREATE TABLE `_玩家深渊主数据` (
  `角色ID` int unsigned NOT NULL COMMENT '角色GUID',
  `当前章节` smallint unsigned NOT NULL DEFAULT 0 COMMENT '当前推进章节',
  `历史最高章节` smallint unsigned NOT NULL DEFAULT 0 COMMENT '历史最高章节',
  `当前修仙门槛等级` smallint unsigned NOT NULL DEFAULT 0 COMMENT '当前修仙门槛等级',
  `最高腐化层` smallint unsigned NOT NULL DEFAULT 0 COMMENT '最高腐化层',
  `已解锁模式掩码` int unsigned NOT NULL DEFAULT 0 COMMENT '已解锁模式mask',
  `剧情状态` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0未开启 1进行中 2已完成',
  `主遗物` int unsigned NOT NULL DEFAULT 0 COMMENT '主遗物',
  `副遗物1` int unsigned NOT NULL DEFAULT 0 COMMENT '副遗物1',
  `副遗物2` int unsigned NOT NULL DEFAULT 0 COMMENT '副遗物2',
  `副遗物3` int unsigned NOT NULL DEFAULT 0 COMMENT '副遗物3',
  `副遗物4` int unsigned NOT NULL DEFAULT 0 COMMENT '副遗物4',
  `副遗物5` int unsigned NOT NULL DEFAULT 0 COMMENT '副遗物5',
  `阶段神器` int unsigned NOT NULL DEFAULT 0 COMMENT '阶段神器',
  `终极神器` int unsigned NOT NULL DEFAULT 0 COMMENT '终极神器',
  `预设编号` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '预设编号',
  `当前保底章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '当前保底章节',
  `深渊装失败次数` smallint unsigned NOT NULL DEFAULT 0 COMMENT '深渊装保底计数',
  `秘藏首领失败次数` smallint unsigned NOT NULL DEFAULT 0 COMMENT '秘藏首领保底计数',
  `最后更新时间` int unsigned NOT NULL DEFAULT 0 COMMENT '最后更新时间',
  PRIMARY KEY (`角色ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家深渊主数据';

DROP TABLE IF EXISTS `_玩家深渊收藏`;
CREATE TABLE `_玩家深渊收藏` (
  `角色ID` int unsigned NOT NULL COMMENT '角色GUID',
  `收藏类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=遗物 2=神器',
  `物品ID` int unsigned NOT NULL DEFAULT 0 COMMENT '收藏物品ID',
  `关联章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '关联章节',
  `等级` smallint unsigned NOT NULL DEFAULT 0 COMMENT '等级或强化阶段',
  `觉醒层数` smallint unsigned NOT NULL DEFAULT 0 COMMENT '觉醒层数',
  `来源类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1=章节奖励 2=合成 3=活动',
  `解锁时间` int unsigned NOT NULL DEFAULT 0 COMMENT '解锁时间',
  PRIMARY KEY (`角色ID`, `收藏类型`, `物品ID`),
  KEY `索引_收藏类型` (`收藏类型`),
  KEY `索引_物品ID` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家深渊收藏';

DROP TABLE IF EXISTS `_玩家深渊章节模式`;
CREATE TABLE `_玩家深渊章节模式` (
  `角色ID` int unsigned NOT NULL COMMENT '角色GUID',
  `章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '章节ID',
  `已解锁模式掩码` int unsigned NOT NULL DEFAULT 0 COMMENT '按章节独立记录已解锁模式',
  `最后更新时间` int unsigned NOT NULL DEFAULT 0 COMMENT '最后更新时间',
  PRIMARY KEY (`角色ID`, `章节ID`),
  KEY `索引_章节ID` (`章节ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家深渊章节模式';

DROP TABLE IF EXISTS `_玩家深渊局内状态`;
CREATE TABLE `_玩家深渊局内状态` (
  `角色ID` int unsigned NOT NULL COMMENT '角色GUID',
  `当前副本地图ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '当前副本地图',
  `当前章节ID` smallint unsigned NOT NULL DEFAULT 0 COMMENT '当前章节',
  `模式类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '难度模式',
  `腐化层` smallint unsigned NOT NULL DEFAULT 0 COMMENT '腐化层',
  `本局主遗物` int unsigned NOT NULL DEFAULT 0 COMMENT '本局主遗物',
  `本局副遗物1` int unsigned NOT NULL DEFAULT 0 COMMENT '本局副遗物1',
  `本局副遗物2` int unsigned NOT NULL DEFAULT 0 COMMENT '本局副遗物2',
  `本局副遗物3` int unsigned NOT NULL DEFAULT 0 COMMENT '本局副遗物3',
  `本局副遗物4` int unsigned NOT NULL DEFAULT 0 COMMENT '本局副遗物4',
  `本局副遗物5` int unsigned NOT NULL DEFAULT 0 COMMENT '本局副遗物5',
  `锚点首领击杀掩码` bigint unsigned NOT NULL DEFAULT 0 COMMENT '锚点首领击杀mask',
  `是否已召唤深渊首领` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '是否已召唤深渊首领',
  `是否已召唤秘藏首领` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '是否已召唤秘藏首领',
  `开局时间` int unsigned NOT NULL DEFAULT 0 COMMENT '开局时间',
  PRIMARY KEY (`角色ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家深渊局内状态';

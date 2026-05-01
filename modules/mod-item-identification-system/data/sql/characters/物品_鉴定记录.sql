-- 物品鉴定记录表
-- 用于记录物品的鉴定状态和鉴定获得的属性

DROP TABLE IF EXISTS `物品_鉴定记录`;
CREATE TABLE `物品_鉴定记录` (
  `记录ID` int unsigned NOT NULL AUTO_INCREMENT COMMENT '自增主键',
  `玩家GUID` int unsigned NOT NULL COMMENT '玩家GUID',
  `物品GUID` int unsigned NOT NULL COMMENT '物品GUID',
  `物品ID` int unsigned NOT NULL COMMENT '物品Entry ID',
  `鉴定模板ID` int unsigned NOT NULL COMMENT '使用的鉴定模板ID',

  -- 鉴定获得的效果记录
  `是否获得成长` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得成长属性',
  `成长组ID` int unsigned NOT NULL DEFAULT '0' COMMENT '成长属性组ID',
  
  `是否获得强化` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得强化属性',
  `强化组ID` int unsigned NOT NULL DEFAULT '0' COMMENT '强化属性组ID',
  
  `是否获得基础属性` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得基础随机属性',
  `基础属性数量` int unsigned NOT NULL DEFAULT '0' COMMENT '获得的基础属性数量',
  `基础属性组ID` int unsigned NOT NULL DEFAULT '0' COMMENT '基础属性组ID',
  `基础属性详情` varchar(500) NOT NULL DEFAULT '' COMMENT '基础属性详情（格式：类型 值,类型 值, 例如：3 10,5 15,）',
  
  `是否获得追加属性` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得追加随机属性',
  `追加属性数量` int unsigned NOT NULL DEFAULT '0' COMMENT '获得的追加属性数量',
  `追加属性组ID列表` varchar(255) NOT NULL DEFAULT '' COMMENT '追加属性组ID（逗号分隔）',
  
  `是否获得符文凹槽` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得符文凹槽',
  `符文凹槽数量` int unsigned NOT NULL DEFAULT '0' COMMENT '获得的符文凹槽数量',
  
  `是否获得技能` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得技能效果',
  `技能组ID列表` varchar(255) NOT NULL DEFAULT '' COMMENT '技能组ID（逗号分隔）',
  
  `是否获得魔次` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得技能魔次',
  `魔次数量` int unsigned NOT NULL DEFAULT '0' COMMENT '获得的魔次数量',
  `魔次组ID列表` varchar(255) NOT NULL DEFAULT '' COMMENT '魔次组ID（逗号分隔）',
  
  `是否获得套装` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得套装属性',
  `套装组ID` int unsigned NOT NULL DEFAULT '0' COMMENT '套装组ID',
  `套装ID` int unsigned NOT NULL DEFAULT '0' COMMENT '具体套装ID',
  
  -- 其他信息
  `消耗金币` bigint unsigned NOT NULL DEFAULT '0' COMMENT '鉴定消耗的金币（铜币）',
  `成功率` int unsigned NOT NULL DEFAULT '100' COMMENT '鉴定时的成功率',
  
  PRIMARY KEY (`记录ID`),
  UNIQUE KEY `idx_物品GUID` (`物品GUID`),
  KEY `idx_玩家GUID` (`玩家GUID`),
  KEY `idx_物品ID` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='物品鉴定记录表';

-- 武魂系统 - 角色库数据

CREATE TABLE IF NOT EXISTS `_玩家武魂数据` (
  `角色id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家GUID',
  `武魂ID` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '当前武魂定义ID',
  `已激活` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=未激活，1=已激活武魂分身',
  `已召唤` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=未召唤，1=玩家希望武魂保持召唤并跟随',
  `魂力` bigint UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前可用魂力',
  `魂环1等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第1魂环等级',
  `魂环2等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第2魂环等级',
  `魂环3等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第3魂环等级',
  `魂环4等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第4魂环等级',
  `魂环5等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第5魂环等级',
  `魂环6等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第6魂环等级',
  `魂环7等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第7魂环等级',
  `魂环8等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第8魂环等级',
  `魂环9等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '第9魂环等级',
  PRIMARY KEY (`角色id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家武魂主数据';

CREATE TABLE IF NOT EXISTS `_玩家武魂技能` (
  `角色id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家GUID',
  `技能ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '对应 _武魂系统_技能.技能ID',
  `等级` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '技能等级',
  `槽位` tinyint UNSIGNED NULL DEFAULT NULL COMMENT 'NULL=未装配，0-8 对应9个技能槽',
  PRIMARY KEY (`角色id`, `技能ID`),
  KEY `idx_wuhun_skill_slot` (`角色id`, `槽位`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家武魂技能状态';

CREATE TABLE IF NOT EXISTS `_玩家武魂装备` (
  `角色id` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家GUID',
  `槽位` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '对应 EQUIPMENT_SLOT_*',
  `解锁时间` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix时间戳，0表示未解锁',
  `物品ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT 'item_template entry',
  `物品GUID` bigint UNSIGNED NOT NULL DEFAULT 0 COMMENT 'item_instance guid',
  PRIMARY KEY (`角色id`, `槽位`),
  KEY `idx_wuhun_equip_unlock_time` (`解锁时间`),
  KEY `idx_wuhun_item_guid` (`物品GUID`),
  KEY `idx_wuhun_item_entry` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='玩家武魂装备状态';

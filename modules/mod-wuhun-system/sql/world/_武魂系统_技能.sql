-- 武魂系统 - 技能配置

CREATE TABLE IF NOT EXISTS `_武魂系统_技能` (
  `技能ID` int UNSIGNED NOT NULL,
  `名称` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `技能SpellID` int UNSIGNED NOT NULL DEFAULT 0,
  `冷却毫秒` int UNSIGNED NOT NULL DEFAULT 5000,
  `学习消耗` int UNSIGNED NOT NULL DEFAULT 100,
  `升级消耗` int UNSIGNED NOT NULL DEFAULT 100,
  `技能等级上限` int UNSIGNED NOT NULL DEFAULT 10,
  `目标类型` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0敌方目标 1武魂自身 2主人',
  `低血量阈值` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=不限制，1-100=目标血量百分比',
  `施法距离` float NOT NULL DEFAULT 30,
  `每级伤害加成` float NOT NULL DEFAULT 1 COMMENT '每级技能伤害加成百分比，1=1%',
  `描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`技能ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='武魂系统技能配置';

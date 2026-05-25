-- 武魂系统 - 模板配置

CREATE TABLE IF NOT EXISTS `_武魂系统_模板` (
  `模板ID` int UNSIGNED NOT NULL COMMENT '武魂模板ID',
  `名称` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `生物模板` int UNSIGNED NOT NULL DEFAULT 930001 COMMENT '召唤出的 creature_template.entry',
  `模型缩放` float NOT NULL DEFAULT 1,
  `默认模板` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '1=默认武魂模板',
  `激活魂力消耗` decimal(39,0) UNSIGNED NOT NULL DEFAULT 1000 COMMENT '激活武魂分身所需魂力',
  `进化魂力消耗` decimal(39,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '进化到该模板所需魂力，模板1为0',
  `副本Boss魂力` decimal(39,0) UNSIGNED NOT NULL DEFAULT 1,
  `世界Boss魂力` decimal(39,0) UNSIGNED NOT NULL DEFAULT 1,
  `魂环等级上限` int UNSIGNED NOT NULL DEFAULT 100,
  `最大继承百分比` int UNSIGNED NOT NULL DEFAULT 900 COMMENT '900=900%',
  `目标同步间隔毫秒` int UNSIGNED NOT NULL DEFAULT 250,
  `属性刷新间隔毫秒` int UNSIGNED NOT NULL DEFAULT 3000,
  `跟随距离` float NOT NULL DEFAULT 2.5,
  `超距召回距离` float NOT NULL DEFAULT 55,
  `自动协助半径` float NOT NULL DEFAULT 30,
  `描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `备注` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`模板ID`),
  KEY `idx_wuhun_template_default` (`默认模板`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='武魂系统模板配置';

ALTER TABLE `_武魂系统_模板`
  MODIFY COLUMN `激活魂力消耗` decimal(39,0) UNSIGNED NOT NULL DEFAULT 1000 COMMENT '激活武魂分身所需魂力',
  MODIFY COLUMN `进化魂力消耗` decimal(39,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '进化到该模板所需魂力，模板1为0',
  MODIFY COLUMN `副本Boss魂力` decimal(39,0) UNSIGNED NOT NULL DEFAULT 1,
  MODIFY COLUMN `世界Boss魂力` decimal(39,0) UNSIGNED NOT NULL DEFAULT 1;

DELETE FROM `_武魂系统_模板` WHERE `模板ID` BETWEEN 1 AND 10;
INSERT INTO `_武魂系统_模板`
(`模板ID`, `名称`, `生物模板`, `模型缩放`, `默认模板`, `激活魂力消耗`, `进化魂力消耗`, `副本Boss魂力`, `世界Boss魂力`, `魂环等级上限`, `最大继承百分比`, `目标同步间隔毫秒`, `属性刷新间隔毫秒`, `跟随距离`, `超距召回距离`, `自动协助半径`, `描述`, `备注`) VALUES
(1, '一阶武魂', 930001, 1.00, 1, 1000, 0,    1,  1, 100, 900, 250, 3000, 2.5, 55, 30, '一阶武魂分身，击杀Boss获得1点魂力', ''),
(2, '二阶武魂', 930001, 1.05, 0, 1000, 1000, 2,  2, 100, 900, 250, 3000, 2.5, 55, 30, '二阶武魂分身，击杀Boss获得2点魂力', ''),
(3, '三阶武魂', 930001, 1.10, 0, 1000, 2000, 3,  3, 100, 900, 250, 3000, 2.5, 55, 30, '三阶武魂分身，击杀Boss获得3点魂力', ''),
(4, '四阶武魂', 930001, 1.15, 0, 1000, 3000, 4,  4, 100, 900, 250, 3000, 2.5, 55, 30, '四阶武魂分身，击杀Boss获得4点魂力', ''),
(5, '五阶武魂', 930001, 1.20, 0, 1000, 4000, 5,  5, 100, 900, 250, 3000, 2.5, 55, 30, '五阶武魂分身，击杀Boss获得5点魂力', ''),
(6, '六阶武魂', 930001, 1.25, 0, 1000, 5000, 6,  6, 100, 900, 250, 3000, 2.5, 55, 30, '六阶武魂分身，击杀Boss获得6点魂力', ''),
(7, '七阶武魂', 930001, 1.30, 0, 1000, 6000, 7,  7, 100, 900, 250, 3000, 2.5, 55, 30, '七阶武魂分身，击杀Boss获得7点魂力', ''),
(8, '八阶武魂', 930001, 1.35, 0, 1000, 7000, 8,  8, 100, 900, 250, 3000, 2.5, 55, 30, '八阶武魂分身，击杀Boss获得8点魂力', ''),
(9, '九阶武魂', 930001, 1.40, 0, 1000, 8000, 9,  9, 100, 900, 250, 3000, 2.5, 55, 30, '九阶武魂分身，击杀Boss获得9点魂力', ''),
(10, '十阶武魂', 930001, 1.45, 0, 1000, 9000, 10, 10, 100, 900, 250, 3000, 2.5, 55, 30, '十阶武魂分身，击杀Boss获得10点魂力', '');

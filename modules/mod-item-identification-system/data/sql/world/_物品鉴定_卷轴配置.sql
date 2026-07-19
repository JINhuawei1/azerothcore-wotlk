CREATE TABLE IF NOT EXISTS `_物品鉴定_卷轴配置` (
  `卷轴物品ID` INT UNSIGNED NOT NULL COMMENT 'item_template.entry；表中存在记录即启用',
  `卷轴类型` TINYINT UNSIGNED NOT NULL COMMENT '1=鉴定倍率卷轴，2=清理卷轴',
  `倍率` DECIMAL(65,0) UNSIGNED NOT NULL DEFAULT 0 COMMENT '鉴定倍率卷轴覆盖写入的倍率；清理卷轴填0',
  `鉴定组ID` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '鉴定倍率卷轴绑定的鉴定组；清理卷轴填0',
  PRIMARY KEY (`卷轴物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='物品鉴定系统卷轴配置';

INSERT INTO `_物品鉴定_卷轴配置` (`卷轴物品ID`, `卷轴类型`, `倍率`, `鉴定组ID`) VALUES
(999100, 1, 100, 1),
(999101, 1, 1000, 1),
(999102, 2, 0, 0)
ON DUPLICATE KEY UPDATE
  `卷轴类型` = VALUES(`卷轴类型`),
  `倍率` = VALUES(`倍率`),
  `鉴定组ID` = VALUES(`鉴定组ID`);

-- 卷轴物品模板使用测试附魔法术47147作为物品目标光标载体。
-- 该法术不限制装备类别、子类和部位；脚本会拦截实际施法并自行消耗卷轴，因此charges保持0。
-- UPDATE `item_template` SET `spellid_1` = 47147, `spelltrigger_1` = 0, `spellcharges_1` = 0
-- WHERE `entry` IN (999100, 999101, 999102);

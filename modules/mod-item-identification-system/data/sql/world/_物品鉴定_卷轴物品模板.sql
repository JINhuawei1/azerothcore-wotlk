-- 基于原版“附魔护腕 - 初级生命”卷轴（38679）克隆物品外观，使用无限制物品目标法术47147显示目标光标。
-- 卷轴消耗由鉴定系统脚本处理，因此 spellcharges_1 必须为0，避免核心重复消耗。

DROP TEMPORARY TABLE IF EXISTS `_tmp_物品鉴定_卷轴模板`;
CREATE TEMPORARY TABLE `_tmp_物品鉴定_卷轴模板` LIKE `item_template`;

INSERT INTO `_tmp_物品鉴定_卷轴模板`
SELECT * FROM `item_template` WHERE `entry` = 38679;

UPDATE `_tmp_物品鉴定_卷轴模板` SET
  `entry` = 999100,
  `name` = '鉴定卷轴·百倍',
  `Quality` = 4,
  `displayid` = 811,
  `BuyPrice` = 0,
  `SellPrice` = 0,
  `bonding` = 0,
  `description` = '对一件武器或护甲使用。未鉴定装备按鉴定组1进行鉴定；已有鉴定结果保持不变；最终倍率覆盖为100倍。',
  `spellid_1` = 47147,
  `spelltrigger_1` = 0,
  `spellcharges_1` = 0,
  `spellcooldown_1` = 0,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = 0,
  `ScriptName` = '',
  `VerifiedBuild` = 12340;

REPLACE INTO `item_template`
SELECT * FROM `_tmp_物品鉴定_卷轴模板`;

TRUNCATE TABLE `_tmp_物品鉴定_卷轴模板`;
INSERT INTO `_tmp_物品鉴定_卷轴模板`
SELECT * FROM `item_template` WHERE `entry` = 38679;

UPDATE `_tmp_物品鉴定_卷轴模板` SET
  `entry` = 999101,
  `name` = '鉴定卷轴·千倍',
  `Quality` = 5,
  `displayid` = 811,
  `BuyPrice` = 0,
  `SellPrice` = 0,
  `bonding` = 0,
  `description` = '对一件武器或护甲使用。未鉴定装备按鉴定组1进行鉴定；已有鉴定结果保持不变；最终倍率覆盖为1000倍。',
  `spellid_1` = 47147,
  `spelltrigger_1` = 0,
  `spellcharges_1` = 0,
  `spellcooldown_1` = 0,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = 0,
  `ScriptName` = '',
  `VerifiedBuild` = 12340;

REPLACE INTO `item_template`
SELECT * FROM `_tmp_物品鉴定_卷轴模板`;

TRUNCATE TABLE `_tmp_物品鉴定_卷轴模板`;
INSERT INTO `_tmp_物品鉴定_卷轴模板`
SELECT * FROM `item_template` WHERE `entry` = 38679;

UPDATE `_tmp_物品鉴定_卷轴模板` SET
  `entry` = 999102,
  `name` = '鉴定清理卷轴',
  `Quality` = 3,
  `displayid` = 811,
  `BuyPrice` = 0,
  `SellPrice` = 0,
  `bonding` = 0,
  `description` = '清除一件武器或护甲的全部自定义鉴定结果、成长、强化、符文、技能、套装和倍率，使其恢复为未鉴定状态。',
  `spellid_1` = 47147,
  `spelltrigger_1` = 0,
  `spellcharges_1` = 0,
  `spellcooldown_1` = 0,
  `spellcategory_1` = 0,
  `spellcategorycooldown_1` = 0,
  `ScriptName` = '',
  `VerifiedBuild` = 12340;

REPLACE INTO `item_template`
SELECT * FROM `_tmp_物品鉴定_卷轴模板`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_物品鉴定_卷轴模板`;

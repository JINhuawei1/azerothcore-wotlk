-- 转身系统扩展：101-500级
-- Boss 397101-397500 / 宝石 996101-996500 / 需求模板 931101-931500
-- 不含刷新坐标（不刷入游戏世界）

-- 数字序列
DROP TEMPORARY TABLE IF EXISTS `_tmp_reinc_ext`;
CREATE TEMPORARY TABLE `_tmp_reinc_ext` (`lvl` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_reinc_ext` (`lvl`)
SELECT a.N + b.N*10 + c.N*100 + 1 AS seq
FROM (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) c
HAVING seq BETWEEN 101 AND 500;

-- 清理
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 397101 AND 397500;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 397101 AND 397500;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 397101 AND 397500;
DELETE FROM `_模板_需求` WHERE `id` BETWEEN 931101 AND 931500;
DELETE FROM `item_template` WHERE `entry` BETWEEN 996101 AND 996500;

-- 转身系统配置表
INSERT IGNORE INTO `_转身系统` (`转身等级`, `模板_需求`, `奖励属性`, `奖励天赋`)
SELECT `lvl`, 931000 + `lvl`, 5.0, 10 FROM `_tmp_reinc_ext`;

-- 宝石物品
INSERT INTO `item_template`
(`entry`,`class`,`subclass`,`SoundOverrideSubclass`,`name`,`displayid`,`Quality`,`Flags`,`FlagsExtra`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,`AllowableClass`,`AllowableRace`,`ItemLevel`,`stackable`,`description`,`Material`,`BagFamily`,`VerifiedBuild`)
SELECT 996000+`lvl`,3,0,-1,CONCAT('转身宝石',`lvl`),54315,5,0,0,1,0,0,0,-1,-1,80,999999999,
  CONCAT('|cFF33FF66转身第',`lvl`,'转材料|r'),1,0,12340
FROM `_tmp_reinc_ext`;

-- 需求模板
INSERT INTO `_模板_需求` (`注释`,`id`,`是否消耗物品`,`消耗物品`,`客户端显示`)
SELECT CONCAT('转身系统-',LPAD(`lvl`,3,'0')),931000+`lvl`,1,
  CONCAT(996000+`lvl`,' 5'),CONCAT('消耗转身宝石',`lvl`,' x5 完成第',`lvl`,'转')
FROM `_tmp_reinc_ext`;

-- Boss模板（不刷新，仅定义）
INSERT INTO `creature_template`
(`entry`,`name`,`subname`,`minlevel`,`maxlevel`,`exp`,`faction`,`scale`,`rank`,`dmgschool`,
 `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,
 `unit_class`,`type`,`lootid`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
 `ExperienceModifier`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,
 `detection_range`,`HoverHeight`,`RegenHealth`,`VerifiedBuild`)
SELECT
  397000+`lvl`,
  CONCAT('|cFFCC0033转身Boss',`lvl`,'|r'),
  '转身宝石守护者',
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  2,14,1,2,0,
  63.6+((`lvl`-1) DIV 5)*6,
  2000,2000,1,1,1,7,
  397000+`lvl`,
  864+((`lvl`-1) DIV 5)*144,
  1,
  CASE
    WHEN `lvl`<=150 THEN 3.5 WHEN `lvl`<=200 THEN 3.8 WHEN `lvl`<=250 THEN 4.2
    WHEN `lvl`<=300 THEN 4.6 WHEN `lvl`<=350 THEN 5.0 WHEN `lvl`<=400 THEN 5.5
    WHEN `lvl`<=450 THEN 6.0 ELSE 6.5
  END,
  1,1,1.14286,1,1,60,1,1,12340
FROM `_tmp_reinc_ext`;

-- Boss模型（80个循环）
DROP TEMPORARY TABLE IF EXISTS `_tmp_reinc_mdl`;
CREATE TEMPORARY TABLE `_tmp_reinc_mdl` (`idx` int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY, `did` int unsigned NOT NULL) ENGINE=MEMORY;
INSERT INTO `_tmp_reinc_mdl` (`did`) VALUES
(30721),(30320),(30303),(30275),(30206),(30168),(30153),(30110),(30060),(29955),
(29905),(29840),(29756),(29694),(29654),(29612),(29524),(29462),(29399),(29309),
(29268),(29183),(29117),(29032),(28988),(28923),(28860),(28787),(28729),(28641),
(28576),(28510),(28447),(28373),(28308),(28249),(28174),(28112),(28040),(27975),
(27917),(27856),(27783),(27721),(27660),(27600),(27536),(27471),(27406),(27344),
(27280),(27219),(27156),(27093),(27030),(26966),(26903),(26840),(26777),(26714),
(26651),(26588),(26525),(26462),(26399),(26336),(26273),(26210),(26147),(26084),
(26021),(25958),(25895),(25832),(25769),(25706),(25643),(25580),(25517),(25454);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 397000+l.`lvl`,0,m.`did`,3.0,1,12340
FROM `_tmp_reinc_ext` l INNER JOIN `_tmp_reinc_mdl` m ON m.`idx`=((l.`lvl`-101) MOD 80)+1;

-- Boss掉落
INSERT INTO `creature_loot_template` (`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT 397000+`lvl`,996000+`lvl`,0,100,0,1,0,1,1,CONCAT('转身Boss #',LPAD(`lvl`,3,'0'))
FROM `_tmp_reinc_ext`;

-- 清理临时表
DROP TEMPORARY TABLE IF EXISTS `_tmp_reinc_mdl`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_reinc_ext`;

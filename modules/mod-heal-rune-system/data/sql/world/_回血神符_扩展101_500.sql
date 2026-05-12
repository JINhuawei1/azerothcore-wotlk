-- 回血神符系统扩展：101-500级
-- Boss 395301-395700 / 物品(神符碎片) 994301-994700 / 需求模板 933001-933400
-- 不含刷新坐标

DROP TEMPORARY TABLE IF EXISTS `_tmp_heal_ext`;
CREATE TEMPORARY TABLE `_tmp_heal_ext` (`lvl` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_heal_ext` (`lvl`)
SELECT a.N + b.N*10 + c.N*100 + 1 AS seq
FROM (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) c
HAVING seq BETWEEN 101 AND 500;

-- 清理
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 395301 AND 395700;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 395301 AND 395700;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 395301 AND 395700;
DELETE FROM `_模板_需求` WHERE `id` BETWEEN 933001 AND 933400;
DELETE FROM `item_template` WHERE `entry` BETWEEN 994301 AND 994700;

-- 回血神符配置表扩展（血蓝值延续100级的递增：91-100级每级+100，继续+100/级）
INSERT IGNORE INTO `_回血神符` (`id`,`回血神符名称`,`回血神符描述`,`回血等级`,`需求系统id`,`血蓝设置`,`血蓝值`)
SELECT `lvl`,
  CONCAT('回生神符·', LPAD(`lvl`,3,'0')),
  CONCAT('消耗神符碎片',`lvl`,' x5，激活后每秒恢复 ', 5500 + (`lvl`-100)*100, ' 点血量与法力'),
  `lvl`, 932900+`lvl`, 1, 5500 + (`lvl`-100)*100
FROM `_tmp_heal_ext`;

-- 神符碎片物品 994301-994700
INSERT INTO `item_template`
(`entry`,`class`,`subclass`,`SoundOverrideSubclass`,`name`,`displayid`,`Quality`,`Flags`,`FlagsExtra`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,`AllowableClass`,`AllowableRace`,`ItemLevel`,`stackable`,`description`,`Material`,`BagFamily`,`VerifiedBuild`)
SELECT 994200+`lvl`,3,0,-1,CONCAT('神符碎片',`lvl`),54315,5,0,0,1,0,0,0,-1,-1,80,999999999,
  CONCAT('|cFF33FF66回血神符第',`lvl`,'级材料|r'),1,0,12340
FROM `_tmp_heal_ext`;

-- 需求模板 933001-933400
INSERT INTO `_模板_需求` (`注释`,`id`,`是否消耗物品`,`消耗物品`,`客户端显示`)
SELECT CONCAT('回血神符-',LPAD(`lvl`,3,'0')),932900+`lvl`,1,
  CONCAT(994200+`lvl`,' 5'),CONCAT('消耗神符碎片',`lvl`,' x5 升级到神符 ',`lvl`,' 级')
FROM `_tmp_heal_ext`;

-- Boss模板 395301-395700
INSERT INTO `creature_template`
(`entry`,`name`,`subname`,`minlevel`,`maxlevel`,`exp`,`faction`,`scale`,`rank`,`dmgschool`,
 `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,
 `unit_class`,`type`,`lootid`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
 `ExperienceModifier`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,
 `detection_range`,`HoverHeight`,`RegenHealth`,`VerifiedBuild`)
SELECT
  395200+`lvl`,
  CONCAT('|cFF00CC66神符Boss',`lvl`,'|r'),
  '神符碎片守护者',
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  2,14,1,2,0,
  63.6+((`lvl`-1) DIV 5)*6,
  2000,2000,1,1,1,7,
  395200+`lvl`,
  864+((`lvl`-1) DIV 5)*144,
  1,
  CASE WHEN `lvl`<=150 THEN 3.5 WHEN `lvl`<=200 THEN 3.8 WHEN `lvl`<=250 THEN 4.2
       WHEN `lvl`<=300 THEN 4.6 WHEN `lvl`<=350 THEN 5.0 WHEN `lvl`<=400 THEN 5.5
       WHEN `lvl`<=450 THEN 6.0 ELSE 6.5 END,
  1,1,1.14286,1,1,60,1,1,12340
FROM `_tmp_heal_ext`;

-- Boss模型
DROP TEMPORARY TABLE IF EXISTS `_tmp_heal_mdl`;
CREATE TEMPORARY TABLE `_tmp_heal_mdl` (`idx` int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY, `did` int unsigned NOT NULL) ENGINE=MEMORY;
INSERT INTO `_tmp_heal_mdl` (`did`) VALUES
(30721),(30320),(30303),(30275),(30206),(30168),(30153),(30110),(30060),(29955),
(29905),(29840),(29756),(29694),(29654),(29612),(29524),(29462),(29399),(29309),
(29268),(29183),(29117),(29032),(28988),(28923),(28860),(28787),(28729),(28641),
(28576),(28510),(28447),(28373),(28308),(28249),(28174),(28112),(28040),(27975),
(27917),(27856),(27783),(27721),(27660),(27600),(27536),(27471),(27406),(27344),
(27280),(27219),(27156),(27093),(27030),(26966),(26903),(26840),(26777),(26714),
(26651),(26588),(26525),(26462),(26399),(26336),(26273),(26210),(26147),(26084),
(26021),(25958),(25895),(25832),(25769),(25706),(25643),(25580),(25517),(25454);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 395200+l.`lvl`,0,m.`did`,3.0,1,12340
FROM `_tmp_heal_ext` l INNER JOIN `_tmp_heal_mdl` m ON m.`idx`=((l.`lvl`-101) MOD 80)+1;

-- Boss掉落
INSERT INTO `creature_loot_template` (`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT 395200+`lvl`,994200+`lvl`,0,100,0,1,0,1,1,CONCAT('神符Boss #',LPAD(`lvl`,3,'0'))
FROM `_tmp_heal_ext`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_heal_mdl`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_heal_ext`;

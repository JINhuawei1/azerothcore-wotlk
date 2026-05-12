-- 切割系统扩展：101-500级
-- Boss 398001-398400 / 物品(切割神石) 994701-995100 / 需求模板 932001-932400
-- 不含刷新坐标

DROP TEMPORARY TABLE IF EXISTS `_tmp_cut_ext`;
CREATE TEMPORARY TABLE `_tmp_cut_ext` (`lvl` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_cut_ext` (`lvl`)
SELECT a.N + b.N*10 + c.N*100 + 1 AS seq
FROM (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) c
HAVING seq BETWEEN 101 AND 500;

-- 清理
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 398001 AND 398400;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 398001 AND 398400;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 398001 AND 398400;
DELETE FROM `_模板_需求` WHERE `id` BETWEEN 932001 AND 932400;
DELETE FROM `item_template` WHERE `entry` BETWEEN 994701 AND 995100;

-- 切割系统配置表扩展（延续100级的递增：91-100级每级+1000，继续+1000/级）
INSERT IGNORE INTO `_切割系统` (`切割等级`,`需求系统id`,`伤害类型`,`切割伤害`,`触发几率`)
SELECT `lvl`, 931900+`lvl`, 1, 55000 + (`lvl`-100)*1000, 100
FROM `_tmp_cut_ext`;

-- 切割神石物品
INSERT INTO `item_template`
(`entry`,`class`,`subclass`,`SoundOverrideSubclass`,`name`,`displayid`,`Quality`,`Flags`,`FlagsExtra`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,`AllowableClass`,`AllowableRace`,`ItemLevel`,`stackable`,`description`,`Material`,`BagFamily`,`VerifiedBuild`)
SELECT 994600+`lvl`,3,0,-1,CONCAT('切割神石',`lvl`),54315,5,0,0,1,0,0,0,-1,-1,80,999999999,
  CONCAT('|cFFFF6633切割第',`lvl`,'级材料|r'),1,0,12340
FROM `_tmp_cut_ext`;

-- 需求模板
INSERT INTO `_模板_需求` (`注释`,`id`,`是否消耗物品`,`消耗物品`,`客户端显示`)
SELECT CONCAT('切割系统-',LPAD(`lvl`,3,'0')),931900+`lvl`,1,
  CONCAT(994600+`lvl`,' 5'),CONCAT('消耗切割神石',`lvl`,' x5 升级到切割 ',`lvl`,' 级')
FROM `_tmp_cut_ext`;

-- Boss模板
INSERT INTO `creature_template`
(`entry`,`name`,`subname`,`minlevel`,`maxlevel`,`exp`,`faction`,`scale`,`rank`,`dmgschool`,
 `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,
 `unit_class`,`type`,`lootid`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
 `ExperienceModifier`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,
 `detection_range`,`HoverHeight`,`RegenHealth`,`VerifiedBuild`)
SELECT
  397900+`lvl`,
  CONCAT('|cFFFF3300切割Boss',`lvl`,'|r'),
  '切割神石守护者',
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  2,14,1,2,0,
  63.6+((`lvl`-1) DIV 5)*6,
  2000,2000,1,1,1,7,
  397900+`lvl`,
  864+((`lvl`-1) DIV 5)*144,
  1,
  CASE WHEN `lvl`<=150 THEN 3.5 WHEN `lvl`<=200 THEN 3.8 WHEN `lvl`<=250 THEN 4.2
       WHEN `lvl`<=300 THEN 4.6 WHEN `lvl`<=350 THEN 5.0 WHEN `lvl`<=400 THEN 5.5
       WHEN `lvl`<=450 THEN 6.0 ELSE 6.5 END,
  1,1,1.14286,1,1,60,1,1,12340
FROM `_tmp_cut_ext`;

-- Boss模型
DROP TEMPORARY TABLE IF EXISTS `_tmp_cut_mdl`;
CREATE TEMPORARY TABLE `_tmp_cut_mdl` (`idx` int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY, `did` int unsigned NOT NULL) ENGINE=MEMORY;
INSERT INTO `_tmp_cut_mdl` (`did`) VALUES
(24960),(24891),(24818),(24756),(24690),(24623),(24558),(24492),(24427),(24361),
(24296),(24230),(24165),(24099),(24034),(23968),(23903),(23837),(23772),(23706),
(23641),(23575),(23510),(23444),(23379),(23313),(23248),(23182),(23117),(23051),
(22986),(22920),(22855),(22789),(22724),(22658),(22593),(22527),(22462),(22396),
(22331),(22265),(22200),(22134),(22069),(22003),(21938),(21872),(21807),(21741),
(21676),(21610),(21545),(21479),(21414),(21348),(21283),(21217),(21152),(21086),
(21021),(20955),(20890),(20824),(20759),(20693),(20628),(20562),(20497),(20431),
(20366),(20300),(20235),(20169),(20104),(20038),(19973),(19907),(19842),(19776);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 397900+l.`lvl`,0,m.`did`,3.0,1,12340
FROM `_tmp_cut_ext` l INNER JOIN `_tmp_cut_mdl` m ON m.`idx`=((l.`lvl`-101) MOD 80)+1;

-- Boss掉落
INSERT INTO `creature_loot_template` (`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT 397900+`lvl`,994600+`lvl`,0,100,0,1,0,1,1,CONCAT('切割Boss #',LPAD(`lvl`,3,'0'))
FROM `_tmp_cut_ext`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_cut_mdl`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_cut_ext`;

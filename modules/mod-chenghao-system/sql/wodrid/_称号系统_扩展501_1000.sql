-- 称号系统扩展：501-1000级
-- Boss 430501-431000 / 物品(称号碎片) 62601-63100 / 需求模板 91501-92000
-- 不含刷新坐标；挑战幻境会复用原世界 430001-430100 的坐标动态生成幻境层 Boss。

DROP TEMPORARY TABLE IF EXISTS `_tmp_title_ext`;
CREATE TEMPORARY TABLE `_tmp_title_ext` (`lvl` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_title_ext` (`lvl`)
SELECT a.N + b.N*10 + c.N*100 + 501 AS seq
FROM (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) c
HAVING seq BETWEEN 501 AND 1000;

DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 430501 AND 431000;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 430501 AND 431000;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 430501 AND 431000;
DELETE FROM `_模板_需求` WHERE `id` BETWEEN 91501 AND 92000;
DELETE FROM `item_template` WHERE `entry` BETWEEN 62601 AND 63100;

INSERT INTO `item_template`
(`entry`,`class`,`subclass`,`SoundOverrideSubclass`,`name`,`displayid`,`Quality`,`Flags`,`FlagsExtra`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,`AllowableClass`,`AllowableRace`,`ItemLevel`,`stackable`,`description`,`Material`,`BagFamily`,`VerifiedBuild`)
SELECT 62100+`lvl`,12,0,-1,CONCAT('称号碎片·第',`lvl`,'阶'),6948,0,32768,0,1,0,0,0,-1,-1,`lvl`,20,
  CONCAT('用于激活第',`lvl`,'阶称号，由特定Boss掉落'),0,0,12340
FROM `_tmp_title_ext`;

INSERT INTO `_模板_需求` (`注释`,`id`,`是否消耗物品`,`消耗物品`,`客户端显示`)
SELECT CONCAT('称号',`lvl`,'阶激活'),91000+`lvl`,0,
  CONCAT(62100+`lvl`,' 5'),CONCAT('提交 称号碎片·第',`lvl`,'阶 x5')
FROM `_tmp_title_ext`;

INSERT IGNORE INTO `_称号系统` (`id`,`称号等级`,`需求系统id`,`激活光环技能id`,`称号描述`)
SELECT `lvl`,`lvl`,91000+`lvl`,372000+`lvl`,
  CONCAT('第',`lvl`,'阶称号 | 减伤',(`lvl` DIV 5),'% | 生命+',`lvl`*100)
FROM `_tmp_title_ext`;

INSERT INTO `creature_template`
(`entry`,`name`,`subname`,`minlevel`,`maxlevel`,`exp`,`faction`,`scale`,`rank`,`dmgschool`,
 `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,
 `unit_class`,`type`,`lootid`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
 `ExperienceModifier`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,
 `detection_range`,`HoverHeight`,`RegenHealth`,`VerifiedBuild`)
SELECT
  430000+`lvl`,
  CONCAT('|cFFFFCC00称号Boss',`lvl`,'|r'),
  '称号碎片守护者',
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  LEAST(83,((`lvl`-1) DIV 10+1)*8),
  2,14,1,3,0,
  (63.6+((`lvl`-1) DIV 5)*6)*2,
  2000,2000,1,1,1,7,
  430000+`lvl`,
  (864+((`lvl`-1) DIV 5)*144)*2,
  1,
  CASE WHEN `lvl`<=550 THEN 7.0 WHEN `lvl`<=600 THEN 7.5 WHEN `lvl`<=650 THEN 8.0
       WHEN `lvl`<=700 THEN 8.5 WHEN `lvl`<=750 THEN 9.0 WHEN `lvl`<=800 THEN 9.5
       WHEN `lvl`<=850 THEN 10.0 WHEN `lvl`<=900 THEN 10.5 WHEN `lvl`<=950 THEN 11.0 ELSE 12.0 END,
  1,1,1.14286,1,1,60,1,1,12340
FROM `_tmp_title_ext`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_title_mdl`;
CREATE TEMPORARY TABLE `_tmp_title_mdl` (`idx` int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY, `did` int unsigned NOT NULL) ENGINE=MEMORY;
INSERT INTO `_tmp_title_mdl` (`did`) VALUES
(11121),(16033),(21135),(20127),(8570),(27035),(28875),(28611),(26752),(22711),
(29268),(29615),(28548),(28977),(28817),(31119),(30362),(31093),(28641),(30721);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 430000+l.`lvl`,0,m.`did`,3.0,1,12340
FROM `_tmp_title_ext` l INNER JOIN `_tmp_title_mdl` m ON m.`idx`=FLOOR((l.`lvl`-501) / 25)+1;

INSERT INTO `creature_loot_template` (`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT 430000+`lvl`,62100+`lvl`,0,100,0,1,0,1,1,CONCAT('称号Boss #',LPAD(`lvl`,3,'0'))
FROM `_tmp_title_ext`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_title_mdl`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_title_ext`;

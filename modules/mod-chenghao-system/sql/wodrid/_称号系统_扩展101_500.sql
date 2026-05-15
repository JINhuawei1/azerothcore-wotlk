-- 称号系统扩展：101-500级
-- Boss 430101-430500 / 物品(称号碎片) 62201-62600 / 需求模板 91101-91500
-- 不含刷新坐标

DROP TEMPORARY TABLE IF EXISTS `_tmp_title_ext`;
CREATE TEMPORARY TABLE `_tmp_title_ext` (`lvl` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_title_ext` (`lvl`)
SELECT a.N + b.N*10 + c.N*100 + 1 AS seq
FROM (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) c
HAVING seq BETWEEN 101 AND 500;

-- 清理
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 430101 AND 430500;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 430101 AND 430500;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 430101 AND 430500;
DELETE FROM `_模板_需求` WHERE `id` BETWEEN 91101 AND 91500;
DELETE FROM `item_template` WHERE `entry` BETWEEN 62201 AND 62600;

-- 称号碎片物品
INSERT INTO `item_template`
(`entry`,`class`,`subclass`,`SoundOverrideSubclass`,`name`,`displayid`,`Quality`,`Flags`,`FlagsExtra`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,`AllowableClass`,`AllowableRace`,`ItemLevel`,`stackable`,`description`,`Material`,`BagFamily`,`VerifiedBuild`)
SELECT 62100+`lvl`,12,0,-1,CONCAT('称号碎片·第',`lvl`,'阶'),6948,0,32768,0,1,0,0,0,-1,-1,`lvl`,20,
  CONCAT('用于激活第',`lvl`,'阶称号，由特定Boss掉落'),0,0,12340
FROM `_tmp_title_ext`;

-- 需求模板
INSERT INTO `_模板_需求` (`注释`,`id`,`是否消耗物品`,`消耗物品`,`客户端显示`)
SELECT CONCAT('称号',`lvl`,'阶激活'),91000+`lvl`,0,
  CONCAT(62100+`lvl`,' 5'),CONCAT('提交 称号碎片·第',`lvl`,'阶 x5')
FROM `_tmp_title_ext`;

-- 称号系统配置表扩展（延续规律：每5级减伤+1%，每级生命+100）
INSERT IGNORE INTO `_称号系统` (`id`,`称号等级`,`需求系统id`,`激活光环技能id`,`称号描述`)
SELECT `lvl`,`lvl`,91000+`lvl`,372000+`lvl`,
  CONCAT('第',`lvl`,'阶称号 | 减伤',(`lvl` DIV 5),'% | 生命+',`lvl`*100)
FROM `_tmp_title_ext`;

-- Boss模板
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
  CASE WHEN `lvl`<=150 THEN 3.5 WHEN `lvl`<=200 THEN 3.8 WHEN `lvl`<=250 THEN 4.2
       WHEN `lvl`<=300 THEN 4.6 WHEN `lvl`<=350 THEN 5.0 WHEN `lvl`<=400 THEN 5.5
       WHEN `lvl`<=450 THEN 6.0 ELSE 6.5 END,
  1,1,1.14286,1,1,60,1,1,12340
FROM `_tmp_title_ext`;

-- Boss模型
DROP TEMPORARY TABLE IF EXISTS `_tmp_title_mdl`;
CREATE TEMPORARY TABLE `_tmp_title_mdl` (`idx` int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY, `did` int unsigned NOT NULL) ENGINE=MEMORY;
INSERT INTO `_tmp_title_mdl` (`did`) VALUES
(11121),(16033),(21135),(20127),(8570),(27035),(28875),(28611),(26752),(22711),
(29268),(29615),(28548),(28977),(28817),(31119),(30362),(31093),(28641),(30721);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 430000+l.`lvl`,0,m.`did`,3.0,1,12340
FROM `_tmp_title_ext` l INNER JOIN `_tmp_title_mdl` m ON m.`idx`=FLOOR((l.`lvl`-101) / 20)+1;

-- Boss掉落
INSERT INTO `creature_loot_template` (`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT 430000+`lvl`,62100+`lvl`,0,100,0,1,0,1,1,CONCAT('称号Boss #',LPAD(`lvl`,3,'0'))
FROM `_tmp_title_ext`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_title_mdl`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_title_ext`;

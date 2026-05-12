-- ============================================
-- 修仙系统 - 连续召唤Boss (390101-390200)
-- 玩家击杀1号Boss后5秒自动召唤下一个，连续到100号
-- 10套战斗脚本循环使用：Boss N 使用脚本 ((N-1) MOD 10) + 1
-- ============================================

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 390101 AND 390200;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 390101 AND 390200;

-- 数字序列
DROP TEMPORARY TABLE IF EXISTS `_tmp_cult_boss`;
CREATE TEMPORARY TABLE `_tmp_cult_boss` (`lvl` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_cult_boss` (`lvl`)
SELECT a.N + b.N*10 + 1 AS seq
FROM (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a
CROSS JOIN (SELECT 0 AS N UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b
HAVING seq BETWEEN 1 AND 100;

-- Boss模板（ScriptName指向C++脚本，10套循环）
INSERT INTO `creature_template`
(`entry`,`name`,`subname`,`minlevel`,`maxlevel`,`exp`,`faction`,`scale`,`rank`,`dmgschool`,
 `DamageModifier`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,
 `unit_class`,`type`,`lootid`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,
 `ExperienceModifier`,`speed_walk`,`speed_run`,`speed_swim`,`speed_flight`,
 `detection_range`,`HoverHeight`,`RegenHealth`,`mechanic_immune_mask`,
 `flags_extra`,`ScriptName`,`VerifiedBuild`)
SELECT
  390100+`lvl`,
  CONCAT('|cFFFF6600天劫·第',`lvl`,'重|r'),
  CONCAT('修仙试炼 ',`lvl`,'/100'),
  LEAST(83, 60 + (`lvl` DIV 5)),
  LEAST(83, 60 + (`lvl` DIV 5)),
  2, 14, 1.0 + `lvl`*0.005, 2, 0,
  -- DamageModifier递增
  50 + `lvl` * 2,
  2000, 2000, 1, 1, 1, 7, 0,
  -- HealthModifier递增
  500 + `lvl` * 50,
  1,
  -- ArmorModifier递增
  1.5 + `lvl` * 0.03,
  1, 1, 1.14286, 1, 1, 60, 1, 0,
  -- 免疫控制（世界Boss标准）
  617299803,
  2,
  CONCAT('npc_cultivation_chain_boss_', ((`lvl`-1) MOD 10) + 1),
  12340
FROM `_tmp_cult_boss`;

-- Boss模型（20个模型循环使用）
DROP TEMPORARY TABLE IF EXISTS `_tmp_cult_mdl`;
CREATE TEMPORARY TABLE `_tmp_cult_mdl` (`idx` int unsigned NOT NULL AUTO_INCREMENT PRIMARY KEY, `did` int unsigned NOT NULL) ENGINE=MEMORY;
INSERT INTO `_tmp_cult_mdl` (`did`) VALUES
(31182),(30614),(30303),(29932),(29524),(28988),(28510),(27975),(27471),(26966),
(26462),(25958),(25454),(24960),(24427),(23903),(23379),(22855),(22331),(21807);

INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
SELECT 390100+l.`lvl`, 0, m.`did`, 1.0 + l.`lvl`*0.005, 1, 12340
FROM `_tmp_cult_boss` l INNER JOIN `_tmp_cult_mdl` m ON m.`idx` = ((l.`lvl`-1) MOD 20) + 1;

DROP TEMPORARY TABLE IF EXISTS `_tmp_cult_mdl`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_cult_boss`;

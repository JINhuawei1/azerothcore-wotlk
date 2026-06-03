-- ============================================================
-- 挑战幻境系统 - World 数据库
-- 玩家不隔离，只按“当前幻境等级/生物层”隔离生物。
-- 文件名按需求保留：_挑战幻境.sql
-- ============================================================

CREATE TABLE IF NOT EXISTS `_挑战幻境等级` (
  `等级` int unsigned NOT NULL COMMENT '幻境等级。0 保留给原世界，业务等级从 1 开始，可扩展到 9999999',
  `名称` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示名称',
  `需求物品` int unsigned NOT NULL DEFAULT 0 COMMENT '进入需求物品 entry，0=无需求',
  `需求数量` int unsigned NOT NULL DEFAULT 0 COMMENT '进入需求物品数量',
  `生物组` int unsigned NOT NULL DEFAULT 0 COMMENT '关联 _挑战幻境生物.生物组，0=默认使用本等级',
  `持续秒数` int unsigned NOT NULL DEFAULT 0 COMMENT '0=不自动限时',
  `备注` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  PRIMARY KEY (`等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='挑战幻境-等级配置';

CREATE TABLE IF NOT EXISTS `_挑战幻境生物` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `生物组` int unsigned NOT NULL DEFAULT 0 COMMENT '刷怪组。通常等于幻境等级，也可多等级共用',
  `生物Entry` int unsigned NOT NULL COMMENT 'creature_template.entry',
  `地图` smallint unsigned NOT NULL DEFAULT 0 COMMENT 'map id',
  `区域` int unsigned NOT NULL DEFAULT 0 COMMENT 'zone/area id，0=不限',
  `坐标X` float NOT NULL DEFAULT 0,
  `坐标Y` float NOT NULL DEFAULT 0,
  `坐标Z` float NOT NULL DEFAULT 0,
  `朝向O` float NOT NULL DEFAULT 0,
  `数量` smallint unsigned NOT NULL DEFAULT 1 COMMENT '同点刷出数量',
  `刷新秒数` int unsigned NOT NULL DEFAULT 300,
  `游荡距离` float NOT NULL DEFAULT 0,
  `备注` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `idx_组_地图` (`生物组`, `地图`),
  KEY `idx_生物Entry` (`生物Entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='挑战幻境-动态生物模板';

CREATE TABLE IF NOT EXISTS `_挑战幻境替换` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `等级` int unsigned NOT NULL COMMENT '玩家当前幻境等级',
  `地图` smallint unsigned NOT NULL DEFAULT 0 COMMENT 'map id，0=不限',
  `区域` int unsigned NOT NULL DEFAULT 0 COMMENT 'zone/area id，0=不限',
  `原生物Entry` int unsigned NOT NULL COMMENT '原世界看到的生物，比如狼',
  `幻境生物Entry` int unsigned NOT NULL COMMENT '幻境中看到的替换生物，比如狗',
  `备注` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `uk_等级_地图_区域_原生物` (`等级`, `地图`, `区域`, `原生物Entry`),
  KEY `idx_幻境生物Entry` (`幻境生物Entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='挑战幻境-原世界生物替换规则';

-- ============================================================
-- 坐标数据：按 5 套系统补齐挑战幻境 1-9 层
--
-- 原世界保留每套系统 1-100：
--   400001-400100 神符守卫
--   410001-410100 裂锋守卫
--   420001-420100 魔次守卫
--   430001-430100 称号守卫
--   440001-440100 转身守卫
--
-- 挑战幻境按 100 为一组：
--   幻境1：x00101-x00200
--   幻境2：x00201-x00300
--   ...
--   幻境9：x00901-x01000
-- ============================================================

DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_level`;
CREATE TEMPORARY TABLE `_tmp_mirage_level` (`等级` int unsigned NOT NULL PRIMARY KEY) ENGINE=MEMORY;
INSERT INTO `_tmp_mirage_level` (`等级`)
SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5
UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9;

DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_system`;
CREATE TEMPORARY TABLE `_tmp_mirage_system` (
  `系统名` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `基数` int unsigned NOT NULL,
  `原世界GUID基数` int unsigned NOT NULL,
  PRIMARY KEY (`基数`)
) ENGINE=MEMORY;
INSERT INTO `_tmp_mirage_system` (`系统名`, `基数`, `原世界GUID基数`) VALUES
('神符守卫', 400000, 5450000),
('裂锋守卫', 410000, 5450100),
('魔次守卫', 420000, 5450200),
('称号守卫', 430000, 5450300),
('转身守卫', 440000, 5450400);

DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_pos`;
CREATE TEMPORARY TABLE `_tmp_mirage_pos` (
  `基数` int unsigned NOT NULL,
  `序号` int unsigned NOT NULL,
  `地图` smallint unsigned NOT NULL DEFAULT 0,
  `区域` int unsigned NOT NULL DEFAULT 0,
  `坐标X` float NOT NULL DEFAULT 0,
  `坐标Y` float NOT NULL DEFAULT 0,
  `坐标Z` float NOT NULL DEFAULT 0,
  `朝向O` float NOT NULL DEFAULT 0,
  `刷新秒数` int unsigned NOT NULL DEFAULT 300,
  `游荡距离` float NOT NULL DEFAULT 0,
  `来源备注` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  PRIMARY KEY (`基数`, `序号`)
) ENGINE=MEMORY;

-- 每套守卫使用自己的原世界 001-100 坐标作为幻境基准。
-- 例如：430001、430101、430201...430901 都使用称号守卫 001 的位置；
--       410001、410101、410201...410901 都使用裂锋守卫 001 的位置。
INSERT IGNORE INTO `_tmp_mirage_pos`
  (`基数`, `序号`, `地图`, `区域`, `坐标X`, `坐标Y`, `坐标Z`, `朝向O`, `刷新秒数`, `游荡距离`, `来源备注`)
SELECT
  FLOOR(c.`id1` / 10000) * 10000,
  c.`id1` - (FLOOR(c.`id1` / 10000) * 10000),
  c.`map`,
  c.`areaId`,
  c.`position_x`,
  c.`position_y`,
  c.`position_z`,
  c.`orientation`,
  c.`spawntimesecs`,
  c.`wander_distance`,
  c.`Comment`
FROM `creature` c
WHERE c.`id1` BETWEEN 400001 AND 400100
   OR c.`id1` BETWEEN 410001 AND 410100
   OR c.`id1` BETWEEN 420001 AND 420100
   OR c.`id1` BETWEEN 430001 AND 430100
   OR c.`id1` BETWEEN 440001 AND 440100
ORDER BY c.`id1`, c.`guid`;

INSERT IGNORE INTO `_tmp_mirage_pos`
  (`基数`, `序号`, `地图`, `区域`, `坐标X`, `坐标Y`, `坐标Z`, `朝向O`, `刷新秒数`, `游荡距离`, `来源备注`)
SELECT
  FLOOR(`生物Entry` / 10000) * 10000,
  `生物Entry` - (FLOOR(`生物Entry` / 10000) * 10000) - 100,
  `地图`,
  `区域`,
  `坐标X`,
  `坐标Y`,
  `坐标Z`,
  `朝向O`,
  `刷新秒数`,
  `游荡距离`,
  `备注`
FROM `_挑战幻境生物`
WHERE `生物组` = 1
  AND (
    `生物Entry` BETWEEN 400101 AND 400200
    OR `生物Entry` BETWEEN 410101 AND 410200
    OR `生物Entry` BETWEEN 420101 AND 420200
    OR `生物Entry` BETWEEN 430101 AND 430200
    OR `生物Entry` BETWEEN 440101 AND 440200
  )
ORDER BY `生物Entry`, `ID`;

-- 修正第 92 号五套守卫坐标，避免后续重跑 SQL 时从旧坐标生成幻境数据。
UPDATE `_tmp_mirage_pos`
SET `地图` = 571, `区域` = 0, `坐标X` = 7856.8, `坐标Y` = -1396.82, `坐标Z` = 1534.06,
    `朝向O` = 3.22622, `刷新秒数` = 300, `游荡距离` = 0, `来源备注` = '挑战幻境坐标修正：神符守卫 Entry=400092 坐标序号=92'
WHERE `基数` = 400000 AND `序号` = 92;

UPDATE `_tmp_mirage_pos`
SET `地图` = 571, `区域` = 0, `坐标X` = 7825.08, `坐标Y` = -1452.63, `坐标Z` = 1534.56,
    `朝向O` = 1.03497, `刷新秒数` = 300, `游荡距离` = 0, `来源备注` = '挑战幻境坐标修正：裂锋守卫 Entry=410092 坐标序号=92'
WHERE `基数` = 410000 AND `序号` = 92;

UPDATE `_tmp_mirage_pos`
SET `地图` = 571, `区域` = 0, `坐标X` = 7872.44, `坐标Y` = -1466.4, `坐标Z` = 1534.56,
    `朝向O` = 1.71436, `刷新秒数` = 300, `游荡距离` = 0, `来源备注` = '挑战幻境坐标修正：魔次守卫 Entry=420092 坐标序号=92'
WHERE `基数` = 420000 AND `序号` = 92;

UPDATE `_tmp_mirage_pos`
SET `地图` = 571, `区域` = 0, `坐标X` = 7917.28, `坐标Y` = -1433.06, `坐标Z` = 1535.26,
    `朝向O` = 2.64977, `刷新秒数` = 300, `游荡距离` = 0, `来源备注` = '挑战幻境坐标修正：称号守卫 Entry=430092 坐标序号=92'
WHERE `基数` = 430000 AND `序号` = 92;

UPDATE `_tmp_mirage_pos`
SET `地图` = 571, `区域` = 0, `坐标X` = 7928.34, `坐标Y` = -1390.1, `坐标Z` = 1535.26,
    `朝向O` = 3.14064, `刷新秒数` = 300, `游荡距离` = 0, `来源备注` = '挑战幻境坐标修正：转身守卫 Entry=440092 坐标序号=92'
WHERE `基数` = 440000 AND `序号` = 92;

-- 清理旧版手写测试数据和旧编号幻境数据。
DELETE FROM `_挑战幻境等级` WHERE `备注` LIKE '测试配置：使用称号Boss%';
DELETE FROM `_挑战幻境生物` WHERE `备注` LIKE '测试：称号Boss%';
DELETE FROM `_挑战幻境替换` WHERE `备注` LIKE '测试：%称号Boss%';

DELETE FROM `_挑战幻境等级`
WHERE `等级` BETWEEN 1 AND 9
  AND (`名称` LIKE '挑战幻境%' OR `备注` LIKE '%Boss坐标同步%' OR `备注` LIKE '挑战幻境统一坐标同步%');

DELETE FROM `_挑战幻境生物`
WHERE `生物组` BETWEEN 1 AND 9
  AND (
    `生物Entry` BETWEEN 396101 AND 397000
    OR `生物Entry` BETWEEN 400101 AND 401000
    OR `生物Entry` BETWEEN 410101 AND 411000
    OR `生物Entry` BETWEEN 420101 AND 421000
    OR `生物Entry` BETWEEN 430101 AND 431000
    OR `生物Entry` BETWEEN 440101 AND 441000
    OR `备注` LIKE '%Boss坐标同步%'
    OR `备注` LIKE '挑战幻境统一坐标同步%'
  );

INSERT INTO `_挑战幻境等级`
  (`等级`, `名称`, `需求物品`, `需求数量`, `生物组`, `持续秒数`, `备注`)
SELECT
  l.`等级`,
  CONCAT('挑战幻境', l.`等级`) AS `名称`,
  0 AS `需求物品`,
  0 AS `需求数量`,
  l.`等级` AS `生物组`,
  0 AS `持续秒数`,
  CONCAT('挑战幻境统一坐标同步：幻境', l.`等级`, ' 使用五套Boss第', l.`等级` * 100 + 1, '-', l.`等级` * 100 + 100, '阶') AS `备注`
FROM `_tmp_mirage_level` l
ON DUPLICATE KEY UPDATE
  `名称` = VALUES(`名称`),
  `需求物品` = VALUES(`需求物品`),
  `需求数量` = VALUES(`需求数量`),
  `生物组` = VALUES(`生物组`),
  `持续秒数` = VALUES(`持续秒数`),
  `备注` = VALUES(`备注`);

INSERT INTO `_挑战幻境生物`
  (`生物组`, `生物Entry`, `地图`, `区域`, `坐标X`, `坐标Y`, `坐标Z`, `朝向O`, `数量`, `刷新秒数`, `游荡距离`, `备注`)
SELECT
  l.`等级` AS `生物组`,
  s.`基数` + l.`等级` * 100 + p.`序号` AS `生物Entry`,
  p.`地图`,
  p.`区域`,
  p.`坐标X`,
  p.`坐标Y`,
  p.`坐标Z`,
  p.`朝向O`,
  1 AS `数量`,
  p.`刷新秒数`,
  p.`游荡距离`,
  CONCAT('挑战幻境统一坐标同步：', s.`系统名`, ' 幻境', l.`等级`, ' Entry=', s.`基数` + l.`等级` * 100 + p.`序号`, ' 坐标序号=', p.`序号`) AS `备注`
FROM `_tmp_mirage_level` l
CROSS JOIN `_tmp_mirage_system` s
INNER JOIN `_tmp_mirage_pos` p ON p.`基数` = s.`基数`
WHERE p.`序号` BETWEEN 1 AND 100
ORDER BY l.`等级`, s.`基数`, p.`序号`;

-- 原世界固定刷新 001-100；幻境层只动态显示 101-1000。
DELETE FROM `creature`
WHERE `id1` BETWEEN 395001 AND 397000
   OR `id2` BETWEEN 395001 AND 397000
   OR `id3` BETWEEN 395001 AND 397000
   OR `id1` BETWEEN 400001 AND 400100
   OR `id1` BETWEEN 410001 AND 410100
   OR `id1` BETWEEN 420001 AND 420100
   OR `id1` BETWEEN 430001 AND 430100
   OR `id1` BETWEEN 440001 AND 440100
   OR `guid` BETWEEN 5450001 AND 5450500;

DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 395001 AND 397000;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 395001 AND 397000;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 395001 AND 397000;

INSERT INTO `creature`
  (`guid`, `id1`, `id2`, `id3`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
   `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`,
   `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
   `ScriptName`, `VerifiedBuild`, `CreateObject`, `Comment`)
SELECT
  s.`原世界GUID基数` + p.`序号`,
  s.`基数` + p.`序号`,
  0,
  0,
  p.`地图`,
  0,
  p.`区域`,
  1,
  1,
  0,
  p.`坐标X`,
  p.`坐标Y`,
  p.`坐标Z`,
  p.`朝向O`,
  p.`刷新秒数`,
  p.`游荡距离`,
  0,
  1,
  0,
  0,
  0,
  0,
  0,
  '',
  12340,
  0,
  CONCAT('挑战幻境原世界坐标同步：', s.`系统名`, ' Entry=', s.`基数` + p.`序号`, ' 坐标序号=', p.`序号`)
FROM `_tmp_mirage_system` s
INNER JOIN `_tmp_mirage_pos` p ON p.`基数` = s.`基数`
WHERE p.`序号` BETWEEN 1 AND 100
ORDER BY s.`基数`, p.`序号`;

-- 五套守卫统一绑定挑战幻境 Boss 战斗脚本。
UPDATE `creature_template`
SET `AIName` = '',
    `ScriptName` = CASE
        WHEN `entry` = 400001 THEN 'npc_challenge_mirage_guardian'
        WHEN `entry` BETWEEN 400001 AND 401000 THEN 'npc_challenge_mirage_rune_guardian'
        WHEN `entry` BETWEEN 410001 AND 411000 THEN 'npc_challenge_mirage_cleaver_guardian'
        WHEN `entry` BETWEEN 420001 AND 421000 THEN 'npc_challenge_mirage_magic_guardian'
        WHEN `entry` BETWEEN 430001 AND 431000 THEN 'npc_challenge_mirage_title_guardian'
        WHEN `entry` BETWEEN 440001 AND 441000 THEN 'npc_challenge_mirage_rebirth_guardian'
        ELSE `ScriptName`
    END
WHERE `entry` BETWEEN 400001 AND 401000
   OR `entry` BETWEEN 410001 AND 411000
   OR `entry` BETWEEN 420001 AND 421000
   OR `entry` BETWEEN 430001 AND 431000
   OR `entry` BETWEEN 440001 AND 441000;

-- 挑战幻境 101-1000 阶 Boss 最终血量：101 阶 300 亿起步，每阶 +10 亿。
-- creature_template 只写反算后的基础 HealthModifier，最终会再乘精英血量倍率和 _属性调整_生物.血量百分比。
UPDATE `creature_template` ct
INNER JOIN `creature_classlevelstats` cls
  ON cls.`level` = ct.`maxlevel`
 AND cls.`class` = ct.`unit_class`
SET ct.`HealthModifier` = CEIL((
    30000000000 +
    (
      CASE
        WHEN ct.`entry` BETWEEN 400101 AND 401000 THEN ct.`entry` - 400000
        WHEN ct.`entry` BETWEEN 410101 AND 411000 THEN ct.`entry` - 410000
        WHEN ct.`entry` BETWEEN 420101 AND 421000 THEN ct.`entry` - 420000
        WHEN ct.`entry` BETWEEN 430101 AND 431000 THEN ct.`entry` - 430000
        WHEN ct.`entry` BETWEEN 440101 AND 441000 THEN ct.`entry` - 440000
        ELSE 101
      END - 101
    ) * 1000000000
  ) / (
  GREATEST(
    CASE ct.`exp`
      WHEN 0 THEN cls.`basehp0`
      WHEN 1 THEN cls.`basehp1`
      ELSE cls.`basehp2`
    END,
    1
  )
  * CASE ct.`rank`
      WHEN 1 THEN 3
      WHEN 2 THEN 5
      WHEN 3 THEN 5
      WHEN 4 THEN 3
      ELSE 1
    END
  * (ROUND(100 + 900 * POW((
      CASE
        WHEN ct.`entry` BETWEEN 400101 AND 401000 THEN ct.`entry` - 400000
        WHEN ct.`entry` BETWEEN 410101 AND 411000 THEN ct.`entry` - 410000
        WHEN ct.`entry` BETWEEN 420101 AND 421000 THEN ct.`entry` - 420000
        WHEN ct.`entry` BETWEEN 430101 AND 431000 THEN ct.`entry` - 430000
        WHEN ct.`entry` BETWEEN 440101 AND 441000 THEN ct.`entry` - 440000
        ELSE 101
      END
    ) / 101.0, 1.35), 2) / 100.0)
  )
)
WHERE ct.`entry` BETWEEN 400101 AND 401000
   OR ct.`entry` BETWEEN 410101 AND 411000
   OR ct.`entry` BETWEEN 420101 AND 421000
   OR ct.`entry` BETWEEN 430101 AND 431000
   OR ct.`entry` BETWEEN 440101 AND 441000;

-- 复用旧版 5 套守卫独立模型；每套 001-100 的模型循环应用到 001-1000，显示大小统一 1 倍。
REPLACE INTO `creature_template_model`
  (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT
  s.`基数` + l.`层` * 100 + p.`序号`,
  0,
  CASE s.`基数`
    WHEN 400000 THEN ELT(p.`序号`,
      10709,10701,10626,10452,10443,10432,10374,10356,10355,10354,10346,10054,9750,9591,9562,9534,9531,9530,9529,9491,9448,9444,9418,9372,9135,9013,8900,8870,8834,8574,8550,8471,8390,8389,8129,7975,7892,7856,7848,7847,7844,7840,7819,7807,7569,7509,7349,7336,7232,7049,7043,6889,6818,6800,6763,6743,6695,6693,6692,6380,6212,6116,6113,6085,6082,6068,6041,5927,5782,5781,5774,5773,5772,5747,5561,5430,5286,5243,5229,5047,5026,4982,4979,4978,4973,4943,4937,4920,4914,4913,4912,4910,4874,4762,4629,4597,4596,4595,4593,4592)
    WHEN 410000 THEN ELT(p.`序号`,
      4585,4458,4212,4156,4026,3898,3616,3589,3535,3341,3320,3267,3212,3208,3186,3030,2879,2850,2714,2713,2703,2702,2687,2597,2582,2549,2541,2537,2491,2355,2346,2296,2174,2168,2076,1994,1973,1961,1933,1921,1912,1817,1549,1534,1306,1305,1204,1194,1162,1104,1103,1092,1091,1078,1065,1043,1019,1018,1012,1011,985,982,965,963,955,931,913,904,831,830,821,788,780,774,720,706,682,652,631,625,610,548,543,540,536,525,522,519,511,507,500,497,491,441,418,391,388,383,368,360)
    WHEN 420000 THEN ELT(p.`序号`,
      27006,26285,275,152,24564,16174,29268,28019,24191,21601,8570,31005,29614,29267,29240,18698,31761,31165,31119,30893,30881,30858,30857,30856,30790,29815,29615,28977,28817,28638,27153,26752,21793,19274,15945,15787,30865,30318,28611,25337,22256,22209,21899,21831,21830,18527,16590,16309,16033,15432,11380,32179,31577,31089,29524,29185,29176,29175,29174,29082,29041,28875,28831,28787,28777,28743,28651,28641,28548,28488,28381,28344,28324,27421,27108,27082,27039,27035,26967,26935,25501,24213,16582,16279,16155,16154,16153,16137,16110,16064,16035,15940,15931,15928,15295,31093,30993,29860,29816,29073)
    WHEN 430000 THEN ELT(p.`序号`,
      26193,26087,25656,24106,23685,23136,16214,11402,1206,28818,28248,28239,28230,27504,26286,25680,21180,20862,20810,20771,20770,20769,20768,20767,20766,20765,20764,20763,20762,20761,20590,20044,19824,19816,19681,18070,17625,17445,16406,16176,16170,16167,14528,14526,14525,14523,14497,14315,14313,14272,14257,14255,12819,12342,12336,12073,11640,11570,11566,11564,11562,11532,11511,11510,11453,11422,11414,11413,11412,11347,11331,11319,11316,11293,11262,11261,11257,11181,11179,11142,11140,11106,11099,11096,11092,11084,11012,10983,10921,10920,10911,10904,10889,10850,10819,10807,10802,10800,10792,10771)
    WHEN 440000 THEN ELT(p.`序号`,
      10709,10701,10626,10452,10443,10432,10374,10356,10355,10354,10346,10054,9750,9591,9562,9534,9531,9530,9529,9491,9448,9444,9418,9372,9135,9013,8900,8870,8834,8574,8550,8471,8390,8389,8129,7975,7892,7856,7848,7847,7844,7840,7819,7807,7569,7509,7349,7336,7232,7049,7043,6889,6818,6800,6763,6743,6695,6693,6692,6380,6212,6116,6113,6085,6082,6068,6041,5927,5782,5781,5774,5773,5772,5747,5561,5430,5286,5243,5229,5047,5026,4982,4979,4978,4973,4943,4937,4920,4914,4913,4912,4910,4874,4762,4629,4597,4596,4595,4593,4592)
  END,
  1.0,
  1,
  12340
FROM `_tmp_mirage_system` s
INNER JOIN `_tmp_mirage_pos` p ON p.`基数` = s.`基数`
CROSS JOIN (
  SELECT 0 AS `层`
  UNION ALL SELECT `等级` FROM `_tmp_mirage_level`
) l
WHERE p.`序号` BETWEEN 1 AND 100
ORDER BY s.`基数`, l.`层`, p.`序号`;

-- ============================================================
-- Boss 难度：挑战幻境 101-1000 阶
--
-- 依据当前库内玩家成长来源做保守设计：
--   1. 参考 _幻境生物属性：1000 级为 25 倍血量、1000 血条，但攻击端偏弱。
--   2. 难度按 Entry 实际阶位 101-1000 逐级递增，不再按幻境层整百分段。
--   3. 幻境 1 覆盖 101-200，幻境 2 覆盖 201-300，依此类推，但同一幻境内每级也有差异。
--   4. 血条约为实际阶位 * 30，101 阶约 3030 血条，1000 阶 30000 血条。
--   5. Boss 伤害 101 阶从 20 亿起步，每级 +1 亿，1000 阶为 919 亿。
-- ============================================================

ALTER TABLE `_属性调整_生物`
  MODIFY COLUMN `物理攻击最小值` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0,
  MODIFY COLUMN `物理攻击最大值` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0,
  MODIFY COLUMN `真实伤害值` decimal(65,0) UNSIGNED NOT NULL DEFAULT 0;

DELETE FROM `_属性调整_生物`
WHERE `生物id` BETWEEN 400101 AND 401000
   OR `生物id` BETWEEN 410101 AND 411000
   OR `生物id` BETWEEN 420101 AND 421000
   OR `生物id` BETWEEN 430101 AND 431000
   OR `生物id` BETWEEN 440101 AND 441000;

INSERT INTO `_属性调整_生物`
  (`id`, `生物id`, `注释`, `组`, `组内随机几率`, `等级`,
   `移动速度`, `移动速度百分比`, `攻击间隔`, `攻击间隔百分比`,
   `血量值`, `血量百分比`, `魔法值`, `魔法百分比`,
   `护甲值`, `护甲百分比`, `抗性值`, `抗性百分比`,
   `物理攻击最小值`, `物理攻击最大值`, `物理攻击百分比`,
   `魔法攻击最小值`, `魔法攻击最大值`, `魔法攻击百分比`,
   `DOT攻击最小值`, `DOT攻击最大值`, `DOT攻击百分比`,
   `治疗最小值`, `治疗最大值`, `治疗百分比`,
   `真实伤害值`, `物理受伤百分比`, `魔法受伤百分比`, `被攻击掉血上限`,
   `物理攻击切割伤害值`, `物理攻击切割伤害百分比`,
   `魔法攻击切割伤害值`, `魔法攻击切割伤害百分比`,
   `DOT攻击切割伤害值`, `DOT攻击切割伤害百分比`,
   `是否可以被切割`, `离开原位置重置距离`,
   `随机移动距离_多个随机逗号隔开`, `刷新后自动移动到坐标`,
   `_自定义Ai_组_多个随机逗号分开`, `_物品_鉴定_组`,
   `官方掉落ID_多个逗号隔开`, `官方掉落ID_多个随机逗号隔开`, `是否加载原掉落`,
   `模型id`, `主手模型id_多个随机逗号隔开`, `副手模型id_多个随机逗号隔开`, `远程模型id_多个随机逗号隔开`,
   `模型大小倍率`, `生物类型`, `击杀奖励`, `击杀奖励几率`,
   `击杀队长奖励`, `击杀队长奖励几率`, `击杀队伍奖励`, `击杀队伍奖励几率`,
   `击杀弹窗`, `击杀公告`, `刷新公告`, `血条数量`, `生物描述`)
SELECT
  s.`基数` + l.`等级` * 100 + p.`序号` AS `id`,
  s.`基数` + l.`等级` * 100 + p.`序号` AS `生物id`,
  CONCAT('挑战幻境Boss难度：', s.`系统名`, ' 幻境', l.`等级`,
         ' Entry阶位', l.`等级` * 100 + p.`序号`,
         ' 难度阶位', l.`等级` * 100 + p.`序号`) AS `注释`,
  0 AS `组`,
  1 AS `组内随机几率`,
  0 AS `等级`,
  0 AS `移动速度`,
  100 AS `移动速度百分比`,
  0 AS `攻击间隔`,
  100 AS `攻击间隔百分比`,
  0 AS `血量值`,
  ROUND(100 + 900 * POW((l.`等级` * 100 + p.`序号`) / 101.0, 1.35), 2) AS `血量百分比`,
  0 AS `魔法值`,
  100 AS `魔法百分比`,
  0 AS `护甲值`,
  100 + (l.`等级` * 100 + p.`序号`) * 2.5 AS `护甲百分比`,
  0 AS `抗性值`,
  100 + (l.`等级` * 100 + p.`序号`) * 2 AS `抗性百分比`,
  FLOOR((2000000000 + ((l.`等级` * 100 + p.`序号`) - 101) * 100000000) * 0.5) AS `物理攻击最小值`,
  2000000000 + ((l.`等级` * 100 + p.`序号`) - 101) * 100000000 AS `物理攻击最大值`,
  100 AS `物理攻击百分比`,
  0 AS `魔法攻击最小值`,
  0 AS `魔法攻击最大值`,
  100 AS `魔法攻击百分比`,
  0 AS `DOT攻击最小值`,
  0 AS `DOT攻击最大值`,
  100 AS `DOT攻击百分比`,
  0 AS `治疗最小值`,
  0 AS `治疗最大值`,
  100 AS `治疗百分比`,
  2000000000 + ((l.`等级` * 100 + p.`序号`) - 101) * 100000000 AS `真实伤害值`,
  GREATEST(10, 60 - FLOOR((l.`等级` * 100 + p.`序号`) / 20)) AS `物理受伤百分比`,
  GREATEST(10, 60 - FLOOR((l.`等级` * 100 + p.`序号`) / 20)) AS `魔法受伤百分比`,
  0 AS `被攻击掉血上限`,
  0 AS `物理攻击切割伤害值`,
  0 AS `物理攻击切割伤害百分比`,
  0 AS `魔法攻击切割伤害值`,
  0 AS `魔法攻击切割伤害百分比`,
  0 AS `DOT攻击切割伤害值`,
  0 AS `DOT攻击切割伤害百分比`,
  1 AS `是否可以被切割`,
  120 AS `离开原位置重置距离`,
  '' AS `随机移动距离_多个随机逗号隔开`,
  '' AS `刷新后自动移动到坐标`,
  '' AS `_自定义Ai_组_多个随机逗号分开`,
  0 AS `_物品_鉴定_组`,
  '' AS `官方掉落ID_多个逗号隔开`,
  '' AS `官方掉落ID_多个随机逗号隔开`,
  '是' AS `是否加载原掉落`,
  NULL AS `模型id`,
  NULL AS `主手模型id_多个随机逗号隔开`,
  NULL AS `副手模型id_多个随机逗号隔开`,
  NULL AS `远程模型id_多个随机逗号隔开`,
  1 AS `模型大小倍率`,
  0 AS `生物类型`,
  0 AS `击杀奖励`,
  100 AS `击杀奖励几率`,
  0 AS `击杀队长奖励`,
  100 AS `击杀队长奖励几率`,
  0 AS `击杀队伍奖励`,
  100 AS `击杀队伍奖励几率`,
  0 AS `击杀弹窗`,
  0 AS `击杀公告`,
  0 AS `刷新公告`,
  CAST((l.`等级` * 100 + p.`序号`) * 30 AS CHAR) AS `血条数量`,
  '' AS `生物描述`
FROM `_tmp_mirage_level` l
CROSS JOIN `_tmp_mirage_system` s
INNER JOIN `_tmp_mirage_pos` p ON p.`基数` = s.`基数`
WHERE p.`序号` BETWEEN 1 AND 100
ORDER BY l.`等级`, s.`基数`, p.`序号`;

DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_pos`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_system`;
DROP TEMPORARY TABLE IF EXISTS `_tmp_mirage_level`;

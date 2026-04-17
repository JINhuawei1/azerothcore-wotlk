-- Boss400 统一刷新坐标
-- 最终真源：统一管理 395001-395300 与 396001-396100 的最终刷点，避免旧文件覆盖
-- 说明：称号Boss先创建刷新点，再用最终坐标重排修正到设计位置

-- ===== 世界Boss395 最终坐标 =====
-- 世界Boss395 坐标重排（敌对怪刷点版）
-- 每个区域 4 个坐标来自对应等级带的真实敌对怪刷点。
-- 其中前 2 个点用于神符Boss与切割Boss。

UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2525.38, `position_y` = 491.596, `position_z` = 34.489, `orientation` = 0.912, `Comment` = '神符Boss Lv8 提瑞斯法林地·西部 #395001' WHERE `id1` = 395001;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2324.44, `position_y` = 622.718, `position_z` = 33.868, `orientation` = 3.246, `Comment` = '切割Boss Lv8 提瑞斯法林地·西部 #395101' WHERE `id1` = 395101;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2802.35, `position_y` = 880.452, `position_z` = 111.925, `orientation` = 4.276, `Comment` = '神符Boss Lv8 提瑞斯法林地·东部 #395002' WHERE `id1` = 395002;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2847.75, `position_y` = 844.061, `position_z` = 112.27, `orientation` = 4.591, `Comment` = '切割Boss Lv8 提瑞斯法林地·东部 #395102' WHERE `id1` = 395102;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1653.52, `position_y` = 17.382, `position_z` = -10.19, `orientation` = 0.06, `Comment` = '神符Boss Lv8 莫高雷·西部 #395003' WHERE `id1` = 395003;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1684.95, `position_y` = 43.283, `position_z` = -5.898, `orientation` = 1.618, `Comment` = '切割Boss Lv8 莫高雷·西部 #395103' WHERE `id1` = 395103;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1411.58, `position_y` = -282.708, `position_z` = -12.997, `orientation` = 1.536, `Comment` = '神符Boss Lv8 莫高雷·东部 #395004' WHERE `id1` = 395004;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1308.68, `position_y` = -216.623, `position_z` = 14.772, `orientation` = 3.394, `Comment` = '切割Boss Lv8 莫高雷·东部 #395104' WHERE `id1` = 395104;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5585.36, `position_y` = 679.937, `position_z` = 385.03, `orientation` = 2.502, `Comment` = '神符Boss Lv8 丹莫罗·东部 #395005' WHERE `id1` = 395005;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5585.96, `position_y` = 552.711, `position_z` = 388.179, `orientation` = 2.78, `Comment` = '切割Boss Lv8 丹莫罗·东部 #395105' WHERE `id1` = 395105;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5585.36, `position_y` = 679.937, `position_z` = 385.03, `orientation` = 2.502, `Comment` = '神符Boss Lv8 丹莫罗·西部 #395006' WHERE `id1` = 395006;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5592.17, `position_y` = 418.139, `position_z` = 380.449, `orientation` = 1.468, `Comment` = '切割Boss Lv8 丹莫罗·西部 #395106' WHERE `id1` = 395106;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 10108.3, `position_y` = 1101.98, `position_z` = 1323.5, `orientation` = 3.415, `Comment` = '神符Boss Lv8 泰达希尔·南部 #395007' WHERE `id1` = 395007;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 10285, `position_y` = 1426.41, `position_z` = 1341.08, `orientation` = 4.407, `Comment` = '切割Boss Lv8 泰达希尔·南部 #395107' WHERE `id1` = 395107;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9844.43, `position_y` = 1555.69, `position_z` = 1291.02, `orientation` = 2.217, `Comment` = '神符Boss Lv8 泰达希尔·北部 #395008' WHERE `id1` = 395008;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9810.28, `position_y` = 1570.64, `position_z` = 1295.44, `orientation` = 2.513, `Comment` = '切割Boss Lv8 泰达希尔·北部 #395108' WHERE `id1` = 395108;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9066.4, `position_y` = -548.124, `position_z` = 58.433, `orientation` = 4.033, `Comment` = '神符Boss Lv8 艾尔文森林 #395009' WHERE `id1` = 395009;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9040.33, `position_y` = -554.065, `position_z` = 55.671, `orientation` = 1.098, `Comment` = '切割Boss Lv8 艾尔文森林 #395109' WHERE `id1` = 395109;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1055.75, `position_y` = -4751.77, `position_z` = 16.222, `orientation` = 4.434, `Comment` = '神符Boss Lv8 杜隆塔尔 #395010' WHERE `id1` = 395010;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1436.69, `position_y` = -4785.35, `position_z` = 7.738, `orientation` = 4.945, `Comment` = '切割Boss Lv8 杜隆塔尔 #395110' WHERE `id1` = 395110;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -4884.38, `position_y` = -3501.98, `position_z` = 291.612, `orientation` = 1.329, `Comment` = '神符Boss Lv16 洛克莫丹·南部 #395011' WHERE `id1` = 395011;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -4983.4, `position_y` = -3515.61, `position_z` = 301.391, `orientation` = 5.654, `Comment` = '切割Boss Lv16 洛克莫丹·南部 #395111' WHERE `id1` = 395111;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5484.62, `position_y` = -2742.06, `position_z` = 364.417, `orientation` = 5.209, `Comment` = '神符Boss Lv16 洛克莫丹·北部 #395012' WHERE `id1` = 395012;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5442.9, `position_y` = -2788.44, `position_z` = 363.673, `orientation` = 6.101, `Comment` = '切割Boss Lv16 洛克莫丹·北部 #395112' WHERE `id1` = 395112;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6640.83, `position_y` = -44.569, `position_z` = 35.752, `orientation` = 4.183, `Comment` = '神符Boss Lv16 黑海岸·南部 #395013' WHERE `id1` = 395013;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6621.26, `position_y` = -131.381, `position_z` = 35.445, `orientation` = 4.544, `Comment` = '切割Boss Lv16 黑海岸·南部 #395113' WHERE `id1` = 395113;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6034.99, `position_y` = 383.143, `position_z` = 24.728, `orientation` = 4.723, `Comment` = '神符Boss Lv16 黑海岸·北部 #395014' WHERE `id1` = 395014;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6118.29, `position_y` = 402.146, `position_z` = 29.372, `orientation` = 4.799, `Comment` = '切割Boss Lv16 黑海岸·北部 #395114' WHERE `id1` = 395114;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1353.13, `position_y` = 719.534, `position_z` = 35.532, `orientation` = 0.257, `Comment` = '神符Boss Lv16 银松森林·北部 #395015' WHERE `id1` = 395015;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1360.21, `position_y` = 798.544, `position_z` = 44.727, `orientation` = 0.931, `Comment` = '切割Boss Lv16 银松森林·北部 #395115' WHERE `id1` = 395115;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1273.19, `position_y` = 262.87, `position_z` = -6.451, `orientation` = 3.683, `Comment` = '神符Boss Lv16 银松森林·南部 #395016' WHERE `id1` = 395016;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1288.64, `position_y` = 54.777, `position_z` = -11.831, `orientation` = 4.643, `Comment` = '切割Boss Lv16 银松森林·南部 #395116' WHERE `id1` = 395116;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6848.06, `position_y` = -6343.11, `position_z` = 31.098, `orientation` = 4.493, `Comment` = '神符Boss Lv16 幽魂之地·北部 #395017' WHERE `id1` = 395017;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 7062.77, `position_y` = -6229.69, `position_z` = 22.697, `orientation` = 3.472, `Comment` = '切割Boss Lv16 幽魂之地·北部 #395117' WHERE `id1` = 395117;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6716.23, `position_y` = -7351.89, `position_z` = 53.553, `orientation` = 2.348, `Comment` = '神符Boss Lv16 幽魂之地·南部 #395018' WHERE `id1` = 395018;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6807.39, `position_y` = -7385.57, `position_z` = 47.533, `orientation` = 5.445, `Comment` = '切割Boss Lv16 幽魂之地·南部 #395118' WHERE `id1` = 395118;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10483.3, `position_y` = 1283.47, `position_z` = 56.808, `orientation` = 1.221, `Comment` = '神符Boss Lv16 西部荒野 #395019' WHERE `id1` = 395019;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10381.3, `position_y` = 1276.99, `position_z` = 44.11, `orientation` = 1.379, `Comment` = '切割Boss Lv16 西部荒野 #395119' WHERE `id1` = 395119;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -444.449, `position_y` = -3011.4, `position_z` = 91.806, `orientation` = 2.256, `Comment` = '神符Boss Lv16 贫瘠之地 #395020' WHERE `id1` = 395020;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -649.59, `position_y` = -3317.87, `position_z` = 94.198, `orientation` = 1.809, `Comment` = '切割Boss Lv16 贫瘠之地 #395120' WHERE `id1` = 395120;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3396.71, `position_y` = -2324.7, `position_z` = 52.274, `orientation` = 5.861, `Comment` = '神符Boss Lv24 湿地·北部 #395021' WHERE `id1` = 395021;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3379.35, `position_y` = -2221.07, `position_z` = 52.683, `orientation` = 5.618, `Comment` = '切割Boss Lv24 湿地·北部 #395121' WHERE `id1` = 395121;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3414.23, `position_y` = -3118.49, `position_z` = 23.201, `orientation` = 0.917, `Comment` = '神符Boss Lv24 湿地·南部 #395022' WHERE `id1` = 395022;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3367.2, `position_y` = -3062.04, `position_z` = 22.352, `orientation` = 2.291, `Comment` = '切割Boss Lv24 湿地·南部 #395122' WHERE `id1` = 395122;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1289.44, `position_y` = 14.195, `position_z` = -0.689, `orientation` = 5.602, `Comment` = '神符Boss Lv24 石爪山脉·北部 #395023' WHERE `id1` = 395023;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1406.81, `position_y` = 19.024, `position_z` = 15.487, `orientation` = 5.593, `Comment` = '切割Boss Lv24 石爪山脉·北部 #395123' WHERE `id1` = 395123;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1186.86, `position_y` = -470.364, `position_z` = 14.149, `orientation` = 3.203, `Comment` = '神符Boss Lv24 石爪山脉·南部 #395024' WHERE `id1` = 395024;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1153.44, `position_y` = -505.265, `position_z` = 7.247, `orientation` = 1.232, `Comment` = '切割Boss Lv24 石爪山脉·南部 #395124' WHERE `id1` = 395124;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 81.746, `position_y` = 130.012, `position_z` = 49.664, `orientation` = 4.441, `Comment` = '神符Boss Lv24 希尔斯布莱德丘陵·北部 #395025' WHERE `id1` = 395025;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 77.244, `position_y` = 48.856, `position_z` = 55.023, `orientation` = 4.712, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵·北部 #395125' WHERE `id1` = 395125;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 254.979, `position_y` = -850.563, `position_z` = 146.853, `orientation` = 1.142, `Comment` = '神符Boss Lv24 希尔斯布莱德丘陵·南部 #395026' WHERE `id1` = 395026;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 222.754, `position_y` = -859.775, `position_z` = 148.023, `orientation` = 3.251, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵·南部 #395126' WHERE `id1` = 395126;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -712.139, `position_y` = 2121.84, `position_z` = 102.528, `orientation` = 4.66, `Comment` = '神符Boss Lv24 凄凉之地·西部 #395027' WHERE `id1` = 395027;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1549.89, `position_y` = 1783.93, `position_z` = 61.478, `orientation` = 1.599, `Comment` = '切割Boss Lv24 凄凉之地·西部 #395127' WHERE `id1` = 395127;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -745.368, `position_y` = 2041.19, `position_z` = 92.356, `orientation` = 1.384, `Comment` = '神符Boss Lv24 凄凉之地·东部 #395028' WHERE `id1` = 395028;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -767.39, `position_y` = 1946.49, `position_z` = 90.206, `orientation` = 4.022, `Comment` = '切割Boss Lv24 凄凉之地·东部 #395128' WHERE `id1` = 395128;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9290.65, `position_y` = -2479.95, `position_z` = 41.899, `orientation` = 5.056, `Comment` = '神符Boss Lv24 赤脊山 #395029' WHERE `id1` = 395029;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9235, `position_y` = -2376.05, `position_z` = 92.17, `orientation` = 5.952, `Comment` = '切割Boss Lv24 赤脊山 #395129' WHERE `id1` = 395129;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2290.65, `position_y` = -1915.09, `position_z` = 66.918, `orientation` = 0.464, `Comment` = '神符Boss Lv24 灰谷 #395030' WHERE `id1` = 395030;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2285.93, `position_y` = -1857.27, `position_z` = 68.708, `orientation` = 2.977, `Comment` = '切割Boss Lv24 灰谷 #395130' WHERE `id1` = 395130;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10553.4, `position_y` = -3515.56, `position_z` = 23.121, `orientation` = 1.304, `Comment` = '神符Boss Lv32 悲伤沼泽·南部 #395031' WHERE `id1` = 395031;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10624.8, `position_y` = -3519.29, `position_z` = 21.928, `orientation` = 1.507, `Comment` = '切割Boss Lv32 悲伤沼泽·南部 #395131' WHERE `id1` = 395131;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10557.8, `position_y` = -2551.18, `position_z` = 19.69, `orientation` = 1.788, `Comment` = '神符Boss Lv32 悲伤沼泽·北部 #395032' WHERE `id1` = 395032;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10463.9, `position_y` = -2844.54, `position_z` = 17.655, `orientation` = 5.56, `Comment` = '切割Boss Lv32 悲伤沼泽·北部 #395132' WHERE `id1` = 395132;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4980.21, `position_y` = -923.934, `position_z` = -4.977, `orientation` = 2.096, `Comment` = '神符Boss Lv32 千针石林·北部 #395033' WHERE `id1` = 395033;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4958.01, `position_y` = -883.172, `position_z` = -5.829, `orientation` = 1.395, `Comment` = '切割Boss Lv32 千针石林·北部 #395133' WHERE `id1` = 395133;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5510.73, `position_y` = -1589.29, `position_z` = 28.071, `orientation` = 1.466, `Comment` = '神符Boss Lv32 千针石林·南部 #395034' WHERE `id1` = 395034;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5560.51, `position_y` = -1634.4, `position_z` = 22.097, `orientation` = 4.507, `Comment` = '切割Boss Lv32 千针石林·南部 #395134' WHERE `id1` = 395134;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 648.909, `position_y` = -647.39, `position_z` = 155.706, `orientation` = 2.443, `Comment` = '神符Boss Lv32 奥特兰克山脉·东部 #395035' WHERE `id1` = 395035;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 382.563, `position_y` = -583.427, `position_z` = 159.461, `orientation` = 2.533, `Comment` = '切割Boss Lv32 奥特兰克山脉·东部 #395135' WHERE `id1` = 395135;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 225.94, `position_y` = -318.775, `position_z` = 155.309, `orientation` = 4.108, `Comment` = '神符Boss Lv32 奥特兰克山脉·西部 #395036' WHERE `id1` = 395036;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 19.99, `position_y` = -570.185, `position_z` = 146.782, `orientation` = 5.178, `Comment` = '切割Boss Lv32 奥特兰克山脉·西部 #395136' WHERE `id1` = 395136;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11635.7, `position_y` = 639.049, `position_z` = 51.237, `orientation` = 5.094, `Comment` = '神符Boss Lv32 荆棘谷北部·北部 #395037' WHERE `id1` = 395037;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11624, `position_y` = 684.091, `position_z` = 49.139, `orientation` = 5.116, `Comment` = '切割Boss Lv32 荆棘谷北部·北部 #395137' WHERE `id1` = 395137;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11066.1, `position_y` = -539.748, `position_z` = 32.998, `orientation` = 2.294, `Comment` = '神符Boss Lv32 荆棘谷北部·南部 #395038' WHERE `id1` = 395038;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11054, `position_y` = -489.126, `position_z` = 30.443, `orientation` = 4.869, `Comment` = '切割Boss Lv32 荆棘谷北部·南部 #395138' WHERE `id1` = 395138;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11513.3, `position_y` = 15.66, `position_z` = 14.289, `orientation` = 5.424, `Comment` = '神符Boss Lv32 暮色森林 #395039' WHERE `id1` = 395039;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11613.7, `position_y` = 14.477, `position_z` = 14.615, `orientation` = 5.474, `Comment` = '切割Boss Lv32 暮色森林 #395139' WHERE `id1` = 395139;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3413.97, `position_y` = -2150.87, `position_z` = 91.792, `orientation` = 1.476, `Comment` = '神符Boss Lv32 南贫瘠之地 #395040' WHERE `id1` = 395040;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -2551.34, `position_y` = -2848.82, `position_z` = 65.224, `orientation` = 6.063, `Comment` = '切割Boss Lv32 南贫瘠之地 #395140' WHERE `id1` = 395140;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6171.8, `position_y` = -2990.86, `position_z` = 228.591, `orientation` = 2.351, `Comment` = '神符Boss Lv40 荒芜之地·东部 #395041' WHERE `id1` = 395041;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6203.15, `position_y` = -3037.36, `position_z` = 220.408, `orientation` = 4.458, `Comment` = '切割Boss Lv40 荒芜之地·东部 #395141' WHERE `id1` = 395141;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6745.66, `position_y` = -2862.6, `position_z` = 244.763, `orientation` = 2.178, `Comment` = '神符Boss Lv40 荒芜之地·西部 #395042' WHERE `id1` = 395042;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6704.56, `position_y` = -2928.19, `position_z` = 241.744, `orientation` = 0.429, `Comment` = '切割Boss Lv40 荒芜之地·西部 #395142' WHERE `id1` = 395142;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7720.54, `position_y` = -3311.62, `position_z` = 70.223, `orientation` = 1.092, `Comment` = '神符Boss Lv40 塔纳利斯·西部 #395043' WHERE `id1` = 395043;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7710.35, `position_y` = -3344.4, `position_z` = 60.413, `orientation` = 4.418, `Comment` = '切割Boss Lv40 塔纳利斯·西部 #395143' WHERE `id1` = 395143;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7231.87, `position_y` = -3614.13, `position_z` = 10.482, `orientation` = 2.931, `Comment` = '神符Boss Lv40 塔纳利斯·东部 #395044' WHERE `id1` = 395044;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7313.72, `position_y` = -3747.19, `position_z` = 10.408, `orientation` = 4.432, `Comment` = '切割Boss Lv40 塔纳利斯·东部 #395144' WHERE `id1` = 395144;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6601, `position_y` = -1302.1, `position_z` = 208.843, `orientation` = 0.264, `Comment` = '神符Boss Lv40 灼热峡谷·南部 #395045' WHERE `id1` = 395045;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6771.64, `position_y` = -1153.67, `position_z` = 243.399, `orientation` = 2.025, `Comment` = '切割Boss Lv40 灼热峡谷·南部 #395145' WHERE `id1` = 395145;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6782.29, `position_y` = -1048.12, `position_z` = 240.75, `orientation` = 1.281, `Comment` = '神符Boss Lv40 灼热峡谷·北部 #395046' WHERE `id1` = 395046;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6685.1, `position_y` = -879.374, `position_z` = 257.649, `orientation` = 3.958, `Comment` = '切割Boss Lv40 灼热峡谷·北部 #395146' WHERE `id1` = 395146;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 362.008, `position_y` = -3336.98, `position_z` = 119.604, `orientation` = 3.036, `Comment` = '神符Boss Lv40 辛特兰·南部 #395047' WHERE `id1` = 395047;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 430.95, `position_y` = -3325.61, `position_z` = 121.042, `orientation` = 0.871, `Comment` = '切割Boss Lv40 辛特兰·南部 #395147' WHERE `id1` = 395147;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 140.824, `position_y` = -2991, `position_z` = 123.67, `orientation` = 3.09, `Comment` = '神符Boss Lv40 辛特兰·北部 #395048' WHERE `id1` = 395048;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 205.745, `position_y` = -2969.61, `position_z` = 112.068, `orientation` = 1.253, `Comment` = '切割Boss Lv40 辛特兰·北部 #395148' WHERE `id1` = 395148;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -1666.14, `position_y` = -2672.47, `position_z` = 41.878, `orientation` = 2.589, `Comment` = '神符Boss Lv40 阿拉希高地 #395049' WHERE `id1` = 395049;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -1753.44, `position_y` = -2680.2, `position_z` = 40.616, `orientation` = 0.094, `Comment` = '切割Boss Lv40 阿拉希高地 #395149' WHERE `id1` = 395149;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3561.63, `position_y` = -3366.68, `position_z` = 39.602, `orientation` = 1.807, `Comment` = '神符Boss Lv40 尘泥沼泽 #395050' WHERE `id1` = 395050;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3614.84, `position_y` = -3311.35, `position_z` = 30.084, `orientation` = 2.81, `Comment` = '切割Boss Lv40 尘泥沼泽 #395150' WHERE `id1` = 395150;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10576.1, `position_y` = -3553.9, `position_z` = 22.067, `orientation` = 4.553, `Comment` = '神符Boss Lv48 诅咒之地·南部 #395051' WHERE `id1` = 395051;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10469.2, `position_y` = -3559.67, `position_z` = 17.499, `orientation` = 0.151, `Comment` = '切割Boss Lv48 诅咒之地·南部 #395151' WHERE `id1` = 395151;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10910.1, `position_y` = -2690.75, `position_z` = 8.589, `orientation` = 4.119, `Comment` = '神符Boss Lv48 诅咒之地·北部 #395052' WHERE `id1` = 395052;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10836.9, `position_y` = -2720.07, `position_z` = 7.719, `orientation` = 2.758, `Comment` = '切割Boss Lv48 诅咒之地·北部 #395152' WHERE `id1` = 395152;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3016.63, `position_y` = -4111.79, `position_z` = 100.656, `orientation` = 0.191, `Comment` = '神符Boss Lv48 艾萨拉·北部 #395053' WHERE `id1` = 395053;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3051.2, `position_y` = -4181.58, `position_z` = 97.516, `orientation` = 0.968, `Comment` = '切割Boss Lv48 艾萨拉·北部 #395153' WHERE `id1` = 395153;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2572.77, `position_y` = -4992.83, `position_z` = 123.364, `orientation` = 4.704, `Comment` = '神符Boss Lv48 艾萨拉·南部 #395054' WHERE `id1` = 395054;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2576.61, `position_y` = -4908.51, `position_z` = 138.077, `orientation` = 5.263, `Comment` = '切割Boss Lv48 艾萨拉·南部 #395154' WHERE `id1` = 395154;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7381.68, `position_y` = 1248.51, `position_z` = -1.357, `orientation` = 5.875, `Comment` = '神符Boss Lv48 安戈洛环形山·西部 #395055' WHERE `id1` = 395055;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7443.37, `position_y` = 1198.55, `position_z` = 2.306, `orientation` = 1.421, `Comment` = '切割Boss Lv48 安戈洛环形山·西部 #395155' WHERE `id1` = 395155;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6445.49, `position_y` = 657.568, `position_z` = 4.913, `orientation` = 1.427, `Comment` = '神符Boss Lv48 安戈洛环形山·东部 #395056' WHERE `id1` = 395056;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6553.32, `position_y` = 674.139, `position_z` = 3.375, `orientation` = 1.736, `Comment` = '切割Boss Lv48 安戈洛环形山·东部 #395156' WHERE `id1` = 395156;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3073.22, `position_y` = -4678.43, `position_z` = 98.494, `orientation` = 1.361, `Comment` = '神符Boss Lv48 东瘟疫之地·南部 #395057' WHERE `id1` = 395057;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3077.72, `position_y` = -4512.63, `position_z` = 114.544, `orientation` = 0.564, `Comment` = '切割Boss Lv48 东瘟疫之地·南部 #395157' WHERE `id1` = 395157;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3199.58, `position_y` = -4239.13, `position_z` = 94.576, `orientation` = 2.214, `Comment` = '神符Boss Lv48 东瘟疫之地·北部 #395058' WHERE `id1` = 395058;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3264.68, `position_y` = -4324.32, `position_z` = 102.904, `orientation` = 1.248, `Comment` = '切割Boss Lv48 东瘟疫之地·北部 #395158' WHERE `id1` = 395158;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -12544.7, `position_y` = -713.784, `position_z` = 38.807, `orientation` = 1.411, `Comment` = '神符Boss Lv48 荆棘谷 #395059' WHERE `id1` = 395059;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -12445.5, `position_y` = -747.227, `position_z` = 36.881, `orientation` = 6.152, `Comment` = '切割Boss Lv48 荆棘谷 #395159' WHERE `id1` = 395159;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4585.96, `position_y` = 1816.36, `position_z` = 91.764, `orientation` = 3.283, `Comment` = '神符Boss Lv48 菲拉斯 #395060' WHERE `id1` = 395060;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4556.43, `position_y` = 1746.99, `position_z` = 96.212, `orientation` = 4.207, `Comment` = '切割Boss Lv48 菲拉斯 #395160' WHERE `id1` = 395160;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1755.85, `position_y` = -1177.83, `position_z` = 59.55, `orientation` = 5.835, `Comment` = '神符Boss Lv56 西瘟疫之地·北部 #395061' WHERE `id1` = 395061;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1899.09, `position_y` = -1539.69, `position_z` = 59.758, `orientation` = 5.896, `Comment` = '切割Boss Lv56 西瘟疫之地·北部 #395161' WHERE `id1` = 395161;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1514.06, `position_y` = -1868.82, `position_z` = 59.227, `orientation` = 4.437, `Comment` = '神符Boss Lv56 西瘟疫之地·南部 #395062' WHERE `id1` = 395062;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1490.67, `position_y` = -1816.92, `position_z` = 60.518, `orientation` = 6.167, `Comment` = '切割Boss Lv56 西瘟疫之地·南部 #395162' WHERE `id1` = 395162;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6542.59, `position_y` = -4115.34, `position_z` = 664.449, `orientation` = 3.392, `Comment` = '神符Boss Lv56 冬泉谷·东部 #395063' WHERE `id1` = 395063;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6446.73, `position_y` = -4083.28, `position_z` = 662.119, `orientation` = 3.954, `Comment` = '切割Boss Lv56 冬泉谷·东部 #395163' WHERE `id1` = 395163;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6471.87, `position_y` = -4076.27, `position_z` = 658.521, `orientation` = 1.06, `Comment` = '神符Boss Lv56 冬泉谷·西部 #395064' WHERE `id1` = 395064;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6353.48, `position_y` = -3945.67, `position_z` = 685.443, `orientation` = 1.96, `Comment` = '切割Boss Lv56 冬泉谷·西部 #395164' WHERE `id1` = 395164;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7101.46, `position_y` = 1507.94, `position_z` = 6.89, `orientation` = 0.698, `Comment` = '神符Boss Lv56 希利苏斯·南部 #395065' WHERE `id1` = 395065;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6882.43, `position_y` = 1376.58, `position_z` = 2.911, `orientation` = 4.812, `Comment` = '切割Boss Lv56 希利苏斯·南部 #395165' WHERE `id1` = 395165;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7368.96, `position_y` = 1670.6, `position_z` = -92.121, `orientation` = 1.649, `Comment` = '神符Boss Lv56 希利苏斯·北部 #395066' WHERE `id1` = 395066;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7478.41, `position_y` = 1836.37, `position_z` = -49.474, `orientation` = 5.815, `Comment` = '切割Boss Lv56 希利苏斯·北部 #395166' WHERE `id1` = 395166;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2894.67, `position_y` = 3388, `position_z` = -16.005, `orientation` = 2.734, `Comment` = '神符Boss Lv56 影月谷·西部 #395067' WHERE `id1` = 395067;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2645.27, `position_y` = 3175.82, `position_z` = 6.407, `orientation` = 4.004, `Comment` = '切割Boss Lv56 影月谷·西部 #395167' WHERE `id1` = 395167;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2948.36, `position_y` = 3343.35, `position_z` = 2.306, `orientation` = 0.279, `Comment` = '神符Boss Lv56 影月谷·东部 #395068' WHERE `id1` = 395068;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2928.81, `position_y` = 3372.29, `position_z` = 0.062, `orientation` = 1.536, `Comment` = '切割Boss Lv56 影月谷·东部 #395168' WHERE `id1` = 395168;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -8098.66, `position_y` = -1509.88, `position_z` = 133.178, `orientation` = 0.489, `Comment` = '神符Boss Lv56 燃烧平原 #395069' WHERE `id1` = 395069;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -8156.33, `position_y` = -1693.23, `position_z` = 136.154, `orientation` = 4.73, `Comment` = '切割Boss Lv56 燃烧平原 #395169' WHERE `id1` = 395169;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3627.89, `position_y` = -1133.12, `position_z` = 210.807, `orientation` = 5.253, `Comment` = '神符Boss Lv56 费伍德森林 #395070' WHERE `id1` = 395070;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 4518.08, `position_y` = -665.083, `position_z` = 260.707, `orientation` = 4.867, `Comment` = '切割Boss Lv56 费伍德森林 #395170' WHERE `id1` = 395170;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2446.82, `position_y` = 5315.01, `position_z` = -3.98, `orientation` = 1.669, `Comment` = '神符Boss Lv64 泰罗卡森林·西部 #395071' WHERE `id1` = 395071;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2350.35, `position_y` = 5349.53, `position_z` = -2.773, `orientation` = 6.052, `Comment` = '切割Boss Lv64 泰罗卡森林·西部 #395171' WHERE `id1` = 395171;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1770.09, `position_y` = 4883.38, `position_z` = 5.497, `orientation` = 0.644, `Comment` = '神符Boss Lv64 泰罗卡森林·东部 #395072' WHERE `id1` = 395072;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1835.23, `position_y` = 4874.81, `position_z` = 2.686, `orientation` = 0.068, `Comment` = '切割Boss Lv64 泰罗卡森林·东部 #395172' WHERE `id1` = 395172;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1849.52, `position_y` = 7550.64, `position_z` = -6.968, `orientation` = 5.2, `Comment` = '神符Boss Lv64 纳格兰·西部 #395073' WHERE `id1` = 395073;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1579.16, `position_y` = 7637.03, `position_z` = -10.002, `orientation` = 3.797, `Comment` = '切割Boss Lv64 纳格兰·西部 #395173' WHERE `id1` = 395173;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -813.486, `position_y` = 8053.47, `position_z` = 46.049, `orientation` = 0.722, `Comment` = '神符Boss Lv64 纳格兰·东部 #395074' WHERE `id1` = 395074;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -771.043, `position_y` = 8077.06, `position_z` = 47.556, `orientation` = 0.325, `Comment` = '切割Boss Lv64 纳格兰·东部 #395174' WHERE `id1` = 395174;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 2564.4, `position_y` = 6551.85, `position_z` = 0.851, `orientation` = 3.193, `Comment` = '神符Boss Lv64 刀锋山·北部 #395075' WHERE `id1` = 395075;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 2591.2, `position_y` = 6480.95, `position_z` = 2.768, `orientation` = 1.867, `Comment` = '切割Boss Lv64 刀锋山·北部 #395175' WHERE `id1` = 395175;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3474.05, `position_y` = 5434.67, `position_z` = 141.33, `orientation` = 3.857, `Comment` = '神符Boss Lv64 刀锋山·南部 #395076' WHERE `id1` = 395076;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3430.73, `position_y` = 5446.9, `position_z` = 144.081, `orientation` = 1.204, `Comment` = '切割Boss Lv64 刀锋山·南部 #395176' WHERE `id1` = 395176;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3900.08, `position_y` = 3991.32, `position_z` = 119.809, `orientation` = 4.005, `Comment` = '神符Boss Lv64 虚空风暴·东部 #395077' WHERE `id1` = 395077;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3948.76, `position_y` = 3908.72, `position_z` = 177.454, `orientation` = 5.411, `Comment` = '切割Boss Lv64 虚空风暴·东部 #395177' WHERE `id1` = 395177;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3083.82, `position_y` = 3822.36, `position_z` = 142.924, `orientation` = 1.077, `Comment` = '神符Boss Lv64 虚空风暴·西部 #395078' WHERE `id1` = 395078;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3206.03, `position_y` = 3620.3, `position_z` = 129.265, `orientation` = 4.927, `Comment` = '切割Boss Lv64 虚空风暴·西部 #395178' WHERE `id1` = 395178;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -350.175, `position_y` = 4987.3, `position_z` = 50.463, `orientation` = 2.396, `Comment` = '神符Boss Lv64 地狱火半岛 #395079' WHERE `id1` = 395079;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -258.481, `position_y` = 5029.74, `position_z` = 62.464, `orientation` = 4.466, `Comment` = '切割Boss Lv64 地狱火半岛 #395179' WHERE `id1` = 395179;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 643.713, `position_y` = 6378.93, `position_z` = 18.552, `orientation` = 4.211, `Comment` = '神符Boss Lv64 赞加沼泽 #395080' WHERE `id1` = 395080;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 649.101, `position_y` = 6541.54, `position_z` = -7.269, `orientation` = 3.431, `Comment` = '切割Boss Lv64 赞加沼泽 #395180' WHERE `id1` = 395180;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3668.98, `position_y` = -845.706, `position_z` = 165.135, `orientation` = 0.247, `Comment` = '神符Boss Lv72 龙骨荒野·东部 #395081' WHERE `id1` = 395081;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3732.99, `position_y` = -848.598, `position_z` = 164.945, `orientation` = 1.25, `Comment` = '切割Boss Lv72 龙骨荒野·东部 #395181' WHERE `id1` = 395181;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2907.92, `position_y` = -999.159, `position_z` = 19.14, `orientation` = 1.145, `Comment` = '神符Boss Lv72 龙骨荒野·西部 #395082' WHERE `id1` = 395082;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2888.33, `position_y` = -731.726, `position_z` = 76.909, `orientation` = 3.365, `Comment` = '切割Boss Lv72 龙骨荒野·西部 #395182' WHERE `id1` = 395182;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4040.46, `position_y` = -4350.68, `position_z` = 261.357, `orientation` = 1.676, `Comment` = '神符Boss Lv72 灰熊丘陵·南部 #395083' WHERE `id1` = 395083;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4075.72, `position_y` = -4044.92, `position_z` = 174.3, `orientation` = 3.557, `Comment` = '切割Boss Lv72 灰熊丘陵·南部 #395183' WHERE `id1` = 395183;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3747.4, `position_y` = -2945.51, `position_z` = 229.381, `orientation` = 3.262, `Comment` = '神符Boss Lv72 灰熊丘陵·北部 #395084' WHERE `id1` = 395084;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3245.02, `position_y` = -3020.31, `position_z` = 152.492, `orientation` = 4.628, `Comment` = '切割Boss Lv72 灰熊丘陵·北部 #395184' WHERE `id1` = 395184;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6025.73, `position_y` = 5062.74, `position_z` = -123.534, `orientation` = 0.251, `Comment` = '神符Boss Lv72 索拉查盆地北·南部 #395085' WHERE `id1` = 395085;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6125.46, `position_y` = 5177.13, `position_z` = -124.501, `orientation` = 4.671, `Comment` = '切割Boss Lv72 索拉查盆地北·南部 #395185' WHERE `id1` = 395185;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6282.09, `position_y` = 5848.88, `position_z` = 48.278, `orientation` = 1.3, `Comment` = '神符Boss Lv72 索拉查盆地北·北部 #395086' WHERE `id1` = 395086;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6215.66, `position_y` = 5749.88, `position_z` = -6.098, `orientation` = 3.892, `Comment` = '切割Boss Lv72 索拉查盆地北·北部 #395186' WHERE `id1` = 395186;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2738.3, `position_y` = -1941.73, `position_z` = 4.121, `orientation` = 3.782, `Comment` = '神符Boss Lv72 龙骨荒野南·西部 #395087' WHERE `id1` = 395087;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2905.01, `position_y` = -2184.91, `position_z` = 48.529, `orientation` = 5.202, `Comment` = '切割Boss Lv72 龙骨荒野南·西部 #395187' WHERE `id1` = 395187;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2832.84, `position_y` = -1959.32, `position_z` = 12.279, `orientation` = 1.347, `Comment` = '神符Boss Lv72 龙骨荒野南·东部 #395088' WHERE `id1` = 395088;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2703.98, `position_y` = -1973.19, `position_z` = 6.677, `orientation` = -1.099, `Comment` = '切割Boss Lv72 龙骨荒野南·东部 #395188' WHERE `id1` = 395188;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2353.59, `position_y` = -4663.92, `position_z` = 231.967, `orientation` = 4.785, `Comment` = '神符Boss Lv72 嚎风峡湾 #395089' WHERE `id1` = 395089;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2262.95, `position_y` = -4857.27, `position_z` = 238.638, `orientation` = 4.584, `Comment` = '切割Boss Lv72 嚎风峡湾 #395189' WHERE `id1` = 395189;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3037.13, `position_y` = 5283.68, `position_z` = 58.254, `orientation` = 2.5, `Comment` = '神符Boss Lv72 北风苔原 #395090' WHERE `id1` = 395090;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2959.89, `position_y` = 5314.98, `position_z` = 62.954, `orientation` = 0.685, `Comment` = '切割Boss Lv72 北风苔原 #395190' WHERE `id1` = 395190;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 8259.18, `position_y` = -2570.82, `position_z` = 1149.71, `orientation` = 1.567, `Comment` = '神符Boss Lv80 风暴峭壁·东部 #395091' WHERE `id1` = 395091;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 8063.88, `position_y` = -2615.24, `position_z` = 1137.9, `orientation` = 2.879, `Comment` = '切割Boss Lv80 风暴峭壁·东部 #395191' WHERE `id1` = 395191;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7211.42, `position_y` = -2111.94, `position_z` = 772.317, `orientation` = 2.532, `Comment` = '神符Boss Lv80 风暴峭壁·西部 #395092' WHERE `id1` = 395092;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7134.93, `position_y` = -2112.19, `position_z` = 761.723, `orientation` = 0.788, `Comment` = '切割Boss Lv80 风暴峭壁·西部 #395192' WHERE `id1` = 395192;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5889.11, `position_y` = 4096.36, `position_z` = -87.374, `orientation` = 1.526, `Comment` = '神符Boss Lv80 索拉查盆地·南部 #395093' WHERE `id1` = 395093;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5875.97, `position_y` = 4065.41, `position_z` = -87.41, `orientation` = 3.318, `Comment` = '切割Boss Lv80 索拉查盆地·南部 #395193' WHERE `id1` = 395193;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5658.21, `position_y` = 5086.45, `position_z` = -134.274, `orientation` = 0.118, `Comment` = '神符Boss Lv80 索拉查盆地·北部 #395094' WHERE `id1` = 395094;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5682.29, `position_y` = 4995.28, `position_z` = -135.518, `orientation` = 0.923, `Comment` = '切割Boss Lv80 索拉查盆地·北部 #395194' WHERE `id1` = 395194;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4952.88, `position_y` = 2650.97, `position_z` = 357.348, `orientation` = 2.331, `Comment` = '神符Boss Lv80 冬拥湖·南部 #395095' WHERE `id1` = 395095;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5120.63, `position_y` = 2482.88, `position_z` = 357.282, `orientation` = 3.158, `Comment` = '切割Boss Lv80 冬拥湖·南部 #395195' WHERE `id1` = 395195;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4822.67, `position_y` = 3180.47, `position_z` = 352.591, `orientation` = 3.389, `Comment` = '神符Boss Lv80 冬拥湖·北部 #395096' WHERE `id1` = 395096;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4864.81, `position_y` = 2609.88, `position_z` = 356.589, `orientation` = 0.179, `Comment` = '切割Boss Lv80 冬拥湖·北部 #395196' WHERE `id1` = 395196;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6575.6, `position_y` = 465.094, `position_z` = 407.446, `orientation` = 2.693, `Comment` = '神符Boss Lv80 水晶之歌森林·东部 #395097' WHERE `id1` = 395097;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6518.07, `position_y` = 616.422, `position_z` = 410.18, `orientation` = 2.132, `Comment` = '切割Boss Lv80 水晶之歌森林·东部 #395197' WHERE `id1` = 395197;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6769.5, `position_y` = -48.979, `position_z` = 755.978, `orientation` = 0.141, `Comment` = '神符Boss Lv80 水晶之歌森林·西部 #395098' WHERE `id1` = 395098;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7009.05, `position_y` = 544.408, `position_z` = 610.468, `orientation` = 3.236, `Comment` = '切割Boss Lv80 水晶之歌森林·西部 #395198' WHERE `id1` = 395198;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5706.9, `position_y` = -2781.17, `position_z` = 274.488, `orientation` = 3.284, `Comment` = '神符Boss Lv80 祖达克 #395099' WHERE `id1` = 395099;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5650.63, `position_y` = -2719.79, `position_z` = 276.695, `orientation` = 3.142, `Comment` = '切割Boss Lv80 祖达克 #395199' WHERE `id1` = 395199;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7876.96, `position_y` = 2289.61, `position_z` = 383.972, `orientation` = 3.653, `Comment` = '神符Boss Lv80 冰冠冰川 #395100' WHERE `id1` = 395100;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7896.62, `position_y` = 2365.76, `position_z` = 398.889, `orientation` = 0.087, `Comment` = '切割Boss Lv80 冰冠冰川 #395200' WHERE `id1` = 395200;

-- ===== 魔次Boss 最终坐标 =====
-- 魔次Boss 坐标重排（敌对怪刷点版）
-- 每个区域 4 个坐标来自对应等级带的真实敌对怪刷点。
-- 第 3 个点用于魔次Boss。

UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2514.16, `position_y` = 602.187, `position_z` = 28.324, `orientation` = 4.595, `Comment` = '魔次Boss Lv8 提瑞斯法林地·西部 #395201' WHERE `id1` = 395201;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2750.74, `position_y` = 887.705, `position_z` = 113.151, `orientation` = 0.549, `Comment` = '魔次Boss Lv8 提瑞斯法林地·东部 #395202' WHERE `id1` = 395202;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1682.8, `position_y` = 115.039, `position_z` = -8.591, `orientation` = 2.022, `Comment` = '魔次Boss Lv8 莫高雷·西部 #395203' WHERE `id1` = 395203;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1571.22, `position_y` = -79.094, `position_z` = 15.68, `orientation` = 3.388, `Comment` = '魔次Boss Lv8 莫高雷·东部 #395204' WHERE `id1` = 395204;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5521.19, `position_y` = 551.901, `position_z` = 391.715, `orientation` = 2.063, `Comment` = '魔次Boss Lv8 丹莫罗·东部 #395205' WHERE `id1` = 395205;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6122.96, `position_y` = 79.165, `position_z` = 416.991, `orientation` = 6.149, `Comment` = '魔次Boss Lv8 丹莫罗·西部 #395206' WHERE `id1` = 395206;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9982.86, `position_y` = 751.405, `position_z` = 1323.95, `orientation` = 5.332, `Comment` = '魔次Boss Lv8 泰达希尔·南部 #395207' WHERE `id1` = 395207;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9863.73, `position_y` = 1580.98, `position_z` = 1287.42, `orientation` = 0.392, `Comment` = '魔次Boss Lv8 泰达希尔·北部 #395208' WHERE `id1` = 395208;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9088.73, `position_y` = -573.684, `position_z` = 62.581, `orientation` = 5.082, `Comment` = '魔次Boss Lv8 艾尔文森林 #395209' WHERE `id1` = 395209;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1131.94, `position_y` = -4673.4, `position_z` = 17.754, `orientation` = 2.273, `Comment` = '魔次Boss Lv8 杜隆塔尔 #395210' WHERE `id1` = 395210;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -4820.94, `position_y` = -3455.15, `position_z` = 290.966, `orientation` = 2.934, `Comment` = '魔次Boss Lv16 洛克莫丹·南部 #395211' WHERE `id1` = 395211;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5536.03, `position_y` = -2781.71, `position_z` = 364.148, `orientation` = 3.339, `Comment` = '魔次Boss Lv16 洛克莫丹·北部 #395212' WHERE `id1` = 395212;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6631.41, `position_y` = 2.836, `position_z` = 35.422, `orientation` = 0.273, `Comment` = '魔次Boss Lv16 黑海岸·南部 #395213' WHERE `id1` = 395213;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6116.13, `position_y` = 585.874, `position_z` = -4.142, `orientation` = 0.078, `Comment` = '魔次Boss Lv16 黑海岸·北部 #395214' WHERE `id1` = 395214;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1489.73, `position_y` = 653.891, `position_z` = 45.698, `orientation` = 0.193, `Comment` = '魔次Boss Lv16 银松森林·北部 #395215' WHERE `id1` = 395215;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1314.07, `position_y` = 313.413, `position_z` = -12.103, `orientation` = 1.431, `Comment` = '魔次Boss Lv16 银松森林·南部 #395216' WHERE `id1` = 395216;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6922.79, `position_y` = -6203.27, `position_z` = 25.943, `orientation` = 4.655, `Comment` = '魔次Boss Lv16 幽魂之地·北部 #395217' WHERE `id1` = 395217;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6843.57, `position_y` = -7409.95, `position_z` = 46.438, `orientation` = 1.763, `Comment` = '魔次Boss Lv16 幽魂之地·南部 #395218' WHERE `id1` = 395218;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10319.7, `position_y` = 1229.1, `position_z` = 37.869, `orientation` = 5.393, `Comment` = '魔次Boss Lv16 西部荒野 #395219' WHERE `id1` = 395219;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -731.679, `position_y` = -3240.54, `position_z` = 94.14, `orientation` = 6.079, `Comment` = '魔次Boss Lv16 贫瘠之地 #395220' WHERE `id1` = 395220;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3340.36, `position_y` = -2302.65, `position_z` = 52.149, `orientation` = 0.028, `Comment` = '魔次Boss Lv24 湿地·北部 #395221' WHERE `id1` = 395221;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3389.06, `position_y` = -3189.21, `position_z` = 24.23, `orientation` = 0.777, `Comment` = '魔次Boss Lv24 湿地·南部 #395222' WHERE `id1` = 395222;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1349.5, `position_y` = 80.668, `position_z` = 11.991, `orientation` = 5.839, `Comment` = '魔次Boss Lv24 石爪山脉·北部 #395223' WHERE `id1` = 395223;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1277.61, `position_y` = -527.06, `position_z` = 25.623, `orientation` = 4.467, `Comment` = '魔次Boss Lv24 石爪山脉·南部 #395224' WHERE `id1` = 395224;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 210.3, `position_y` = -8.497, `position_z` = 73.037, `orientation` = 2.486, `Comment` = '魔次Boss Lv24 希尔斯布莱德丘陵·北部 #395225' WHERE `id1` = 395225;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 267.085, `position_y` = -798.087, `position_z` = 139.064, `orientation` = 4.508, `Comment` = '魔次Boss Lv24 希尔斯布莱德丘陵·南部 #395226' WHERE `id1` = 395226;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1678.97, `position_y` = 2301.93, `position_z` = 81.079, `orientation` = 1.249, `Comment` = '魔次Boss Lv24 凄凉之地·西部 #395227' WHERE `id1` = 395227;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -715.471, `position_y` = 2184.16, `position_z` = 100.246, `orientation` = 3.143, `Comment` = '魔次Boss Lv24 凄凉之地·东部 #395228' WHERE `id1` = 395228;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9491.05, `position_y` = -2290.11, `position_z` = 74.458, `orientation` = 1.166, `Comment` = '魔次Boss Lv24 赤脊山 #395229' WHERE `id1` = 395229;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2232.12, `position_y` = -1880.19, `position_z` = 69.821, `orientation` = 4.969, `Comment` = '魔次Boss Lv24 灰谷 #395230' WHERE `id1` = 395230;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10477, `position_y` = -3452.45, `position_z` = 19.825, `orientation` = 2.929, `Comment` = '魔次Boss Lv32 悲伤沼泽·南部 #395231' WHERE `id1` = 395231;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10602.2, `position_y` = -2555.93, `position_z` = 24.076, `orientation` = 3.657, `Comment` = '魔次Boss Lv32 悲伤沼泽·北部 #395232' WHERE `id1` = 395232;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5054.03, `position_y` = -973.837, `position_z` = -4.963, `orientation` = 3.08, `Comment` = '魔次Boss Lv32 千针石林·北部 #395233' WHERE `id1` = 395233;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5579.14, `position_y` = -1577.71, `position_z` = 10.667, `orientation` = 6.17, `Comment` = '魔次Boss Lv32 千针石林·南部 #395234' WHERE `id1` = 395234;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 604.904, `position_y` = -709.128, `position_z` = 151.235, `orientation` = 4.328, `Comment` = '魔次Boss Lv32 奥特兰克山脉·东部 #395235' WHERE `id1` = 395235;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -112.202, `position_y` = -253.167, `position_z` = 141.937, `orientation` = 4.765, `Comment` = '魔次Boss Lv32 奥特兰克山脉·西部 #395236' WHERE `id1` = 395236;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11650.2, `position_y` = 616.899, `position_z` = 50.304, `orientation` = 1.674, `Comment` = '魔次Boss Lv32 荆棘谷北部·北部 #395237' WHERE `id1` = 395237;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11001.4, `position_y` = -536.417, `position_z` = 33.437, `orientation` = 0.536, `Comment` = '魔次Boss Lv32 荆棘谷北部·南部 #395238' WHERE `id1` = 395238;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11476.9, `position_y` = -81.814, `position_z` = 32.738, `orientation` = 0.736, `Comment` = '魔次Boss Lv32 暮色森林 #395239' WHERE `id1` = 395239;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3482.8, `position_y` = -1912.19, `position_z` = 95.614, `orientation` = 4.71, `Comment` = '魔次Boss Lv32 南贫瘠之地 #395240' WHERE `id1` = 395240;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6154.87, `position_y` = -3090.72, `position_z` = 227.512, `orientation` = 1.274, `Comment` = '魔次Boss Lv40 荒芜之地·东部 #395241' WHERE `id1` = 395241;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6745.11, `position_y` = -2659.32, `position_z` = 241.814, `orientation` = 0.216, `Comment` = '魔次Boss Lv40 荒芜之地·西部 #395242' WHERE `id1` = 395242;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7649.63, `position_y` = -3320.29, `position_z` = 65.333, `orientation` = 3.708, `Comment` = '魔次Boss Lv40 塔纳利斯·西部 #395243' WHERE `id1` = 395243;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7363.61, `position_y` = -3866.43, `position_z` = 10.989, `orientation` = 2.205, `Comment` = '魔次Boss Lv40 塔纳利斯·东部 #395244' WHERE `id1` = 395244;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6756.99, `position_y` = -1212.31, `position_z` = 245.279, `orientation` = 1.874, `Comment` = '魔次Boss Lv40 灼热峡谷·南部 #395245' WHERE `id1` = 395245;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6708.54, `position_y` = -1109.27, `position_z` = 244.616, `orientation` = 1.105, `Comment` = '魔次Boss Lv40 灼热峡谷·北部 #395246' WHERE `id1` = 395246;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 281.402, `position_y` = -3172.74, `position_z` = 121.237, `orientation` = 1.854, `Comment` = '魔次Boss Lv40 辛特兰·南部 #395247' WHERE `id1` = 395247;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 197.521, `position_y` = -3052.44, `position_z` = 131.686, `orientation` = 4.543, `Comment` = '魔次Boss Lv40 辛特兰·北部 #395248' WHERE `id1` = 395248;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -1548.48, `position_y` = -2413.47, `position_z` = 76.365, `orientation` = 4.441, `Comment` = '魔次Boss Lv40 阿拉希高地 #395249' WHERE `id1` = 395249;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3541.37, `position_y` = -3189.01, `position_z` = 42.707, `orientation` = 5.828, `Comment` = '魔次Boss Lv40 尘泥沼泽 #395250' WHERE `id1` = 395250;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10592.8, `position_y` = -3582.49, `position_z` = 21.967, `orientation` = 0.578, `Comment` = '魔次Boss Lv48 诅咒之地·南部 #395251' WHERE `id1` = 395251;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11034.3, `position_y` = -2804.99, `position_z` = 8.674, `orientation` = 1.112, `Comment` = '魔次Boss Lv48 诅咒之地·北部 #395252' WHERE `id1` = 395252;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3082.4, `position_y` = -4176.7, `position_z` = 98.649, `orientation` = 2.994, `Comment` = '魔次Boss Lv48 艾萨拉·北部 #395253' WHERE `id1` = 395253;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2649.96, `position_y` = -4993.95, `position_z` = 127.458, `orientation` = 1.391, `Comment` = '魔次Boss Lv48 艾萨拉·南部 #395254' WHERE `id1` = 395254;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7413.93, `position_y` = 1180.05, `position_z` = 3.208, `orientation` = 5.18, `Comment` = '魔次Boss Lv48 安戈洛环形山·西部 #395255' WHERE `id1` = 395255;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6347.69, `position_y` = 779.847, `position_z` = 1.01, `orientation` = 4.915, `Comment` = '魔次Boss Lv48 安戈洛环形山·东部 #395256' WHERE `id1` = 395256;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3260.5, `position_y` = -4420.22, `position_z` = 110.113, `orientation` = 4.312, `Comment` = '魔次Boss Lv48 东瘟疫之地·南部 #395257' WHERE `id1` = 395257;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3153.64, `position_y` = -4102.27, `position_z` = 99.049, `orientation` = 1.549, `Comment` = '魔次Boss Lv48 东瘟疫之地·北部 #395258' WHERE `id1` = 395258;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -12386.6, `position_y` = -891.133, `position_z` = 47.185, `orientation` = 2.481, `Comment` = '魔次Boss Lv48 荆棘谷 #395259' WHERE `id1` = 395259;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4556.88, `position_y` = 1860.01, `position_z` = 90.125, `orientation` = 2.609, `Comment` = '魔次Boss Lv48 菲拉斯 #395260' WHERE `id1` = 395260;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2406.53, `position_y` = -1586.18, `position_z` = 112.145, `orientation` = 2.443, `Comment` = '魔次Boss Lv56 西瘟疫之地·北部 #395261' WHERE `id1` = 395261;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1512.2, `position_y` = -1833.83, `position_z` = 61.744, `orientation` = 1.034, `Comment` = '魔次Boss Lv56 西瘟疫之地·南部 #395262' WHERE `id1` = 395262;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6407.23, `position_y` = -4003.54, `position_z` = 667.126, `orientation` = 4.805, `Comment` = '魔次Boss Lv56 冬泉谷·东部 #395263' WHERE `id1` = 395263;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6322.67, `position_y` = -3927.75, `position_z` = 696.217, `orientation` = 6.097, `Comment` = '魔次Boss Lv56 冬泉谷·西部 #395264' WHERE `id1` = 395264;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7172.7, `position_y` = 1448.99, `position_z` = 6.199, `orientation` = 0.554, `Comment` = '魔次Boss Lv56 希利苏斯·南部 #395265' WHERE `id1` = 395265;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7572.96, `position_y` = 1829.32, `position_z` = -48.473, `orientation` = 4.838, `Comment` = '魔次Boss Lv56 希利苏斯·北部 #395266' WHERE `id1` = 395266;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2992.71, `position_y` = 3433.33, `position_z` = 0.14, `orientation` = 4.658, `Comment` = '魔次Boss Lv56 影月谷·西部 #395267' WHERE `id1` = 395267;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2948.97, `position_y` = 3386.08, `position_z` = 0.148, `orientation` = 0.052, `Comment` = '魔次Boss Lv56 影月谷·东部 #395268' WHERE `id1` = 395268;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -8139.96, `position_y` = -1585.32, `position_z` = 133.966, `orientation` = 3.913, `Comment` = '魔次Boss Lv56 燃烧平原 #395269' WHERE `id1` = 395269;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3628.7, `position_y` = -1174.01, `position_z` = 211.205, `orientation` = 4.287, `Comment` = '魔次Boss Lv56 费伍德森林 #395270' WHERE `id1` = 395270;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2416.48, `position_y` = 5221.11, `position_z` = -4.927, `orientation` = 6.054, `Comment` = '魔次Boss Lv64 泰罗卡森林·西部 #395271' WHERE `id1` = 395271;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1710.07, `position_y` = 5001.1, `position_z` = 2.693, `orientation` = 2.351, `Comment` = '魔次Boss Lv64 泰罗卡森林·东部 #395272' WHERE `id1` = 395272;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1874.88, `position_y` = 7553.25, `position_z` = -6.591, `orientation` = 6.139, `Comment` = '魔次Boss Lv64 纳格兰·西部 #395273' WHERE `id1` = 395273;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -769.287, `position_y` = 8153.52, `position_z` = 49.548, `orientation` = 4.316, `Comment` = '魔次Boss Lv64 纳格兰·东部 #395274' WHERE `id1` = 395274;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 2717.49, `position_y` = 6424, `position_z` = 2.376, `orientation` = 4.869, `Comment` = '魔次Boss Lv64 刀锋山·北部 #395275' WHERE `id1` = 395275;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3405.74, `position_y` = 5402.54, `position_z` = 147.397, `orientation` = 1.728, `Comment` = '魔次Boss Lv64 刀锋山·南部 #395276' WHERE `id1` = 395276;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3957.45, `position_y` = 4013.2, `position_z` = 115.489, `orientation` = 2.72, `Comment` = '魔次Boss Lv64 虚空风暴·东部 #395277' WHERE `id1` = 395277;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 2882.65, `position_y` = 3567.13, `position_z` = 160.882, `orientation` = 5.55, `Comment` = '魔次Boss Lv64 虚空风暴·西部 #395278' WHERE `id1` = 395278;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -227.063, `position_y` = 5074.04, `position_z` = 78.356, `orientation` = 5.11, `Comment` = '魔次Boss Lv64 地狱火半岛 #395279' WHERE `id1` = 395279;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 717.637, `position_y` = 6584.08, `position_z` = 5.385, `orientation` = 4.702, `Comment` = '魔次Boss Lv64 赞加沼泽 #395280' WHERE `id1` = 395280;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3611.61, `position_y` = -792.073, `position_z` = 164.401, `orientation` = 0.611, `Comment` = '魔次Boss Lv72 龙骨荒野·东部 #395281' WHERE `id1` = 395281;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2805.5, `position_y` = -736.527, `position_z` = 33.496, `orientation` = 2.153, `Comment` = '魔次Boss Lv72 龙骨荒野·西部 #395282' WHERE `id1` = 395282;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4004.09, `position_y` = -4021.7, `position_z` = 173.994, `orientation` = 5.62, `Comment` = '魔次Boss Lv72 灰熊丘陵·南部 #395283' WHERE `id1` = 395283;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3645.9, `position_y` = -3265.6, `position_z` = 242.412, `orientation` = 3.805, `Comment` = '魔次Boss Lv72 灰熊丘陵·北部 #395284' WHERE `id1` = 395284;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6009.18, `position_y` = 4956.37, `position_z` = -107.458, `orientation` = 3.704, `Comment` = '魔次Boss Lv72 索拉查盆地北·南部 #395285' WHERE `id1` = 395285;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6243.43, `position_y` = 5917.52, `position_z` = 57.753, `orientation` = 1.886, `Comment` = '魔次Boss Lv72 索拉查盆地北·北部 #395286' WHERE `id1` = 395286;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2757.06, `position_y` = -2410.56, `position_z` = 39.558, `orientation` = 4.206, `Comment` = '魔次Boss Lv72 龙骨荒野南·西部 #395287' WHERE `id1` = 395287;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2953.84, `position_y` = -2170.85, `position_z` = 57.875, `orientation` = 5.612, `Comment` = '魔次Boss Lv72 龙骨荒野南·东部 #395288' WHERE `id1` = 395288;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2412.77, `position_y` = -4848.57, `position_z` = 252.057, `orientation` = 1.471, `Comment` = '魔次Boss Lv72 嚎风峡湾 #395289' WHERE `id1` = 395289;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2984.39, `position_y` = 5359.3, `position_z` = 64.135, `orientation` = 4.704, `Comment` = '魔次Boss Lv72 北风苔原 #395290' WHERE `id1` = 395290;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 8306.36, `position_y` = -2524.83, `position_z` = 1152.22, `orientation` = 2.292, `Comment` = '魔次Boss Lv80 风暴峭壁·东部 #395291' WHERE `id1` = 395291;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7329.02, `position_y` = -2096.36, `position_z` = 773.121, `orientation` = 4.29, `Comment` = '魔次Boss Lv80 风暴峭壁·西部 #395292' WHERE `id1` = 395292;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5835.12, `position_y` = 4044.62, `position_z` = -86.979, `orientation` = 2.965, `Comment` = '魔次Boss Lv80 索拉查盆地·南部 #395293' WHERE `id1` = 395293;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5695.33, `position_y` = 5172.03, `position_z` = -134.428, `orientation` = 0.484, `Comment` = '魔次Boss Lv80 索拉查盆地·北部 #395294' WHERE `id1` = 395294;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5069.81, `position_y` = 2393.82, `position_z` = 358.459, `orientation` = 2.707, `Comment` = '魔次Boss Lv80 冬拥湖·南部 #395295' WHERE `id1` = 395295;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5143.38, `position_y` = 3184.41, `position_z` = 362.846, `orientation` = 3.03, `Comment` = '魔次Boss Lv80 冬拥湖·北部 #395296' WHERE `id1` = 395296;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6409.11, `position_y` = 249.111, `position_z` = 396.498, `orientation` = 2.913, `Comment` = '魔次Boss Lv80 水晶之歌森林·东部 #395297' WHERE `id1` = 395297;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6809.66, `position_y` = -0.105, `position_z` = 758.342, `orientation` = 0.792, `Comment` = '魔次Boss Lv80 水晶之歌森林·西部 #395298' WHERE `id1` = 395298;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5579.99, `position_y` = -2852.14, `position_z` = 274.446, `orientation` = 3.244, `Comment` = '魔次Boss Lv80 祖达克 #395299' WHERE `id1` = 395299;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7850.92, `position_y` = 2318.47, `position_z` = 382.82, `orientation` = 2.066, `Comment` = '魔次Boss Lv80 冰冠冰川 #395300' WHERE `id1` = 395300;

-- ===== 称号Boss 基础刷新行 =====
-- 称号Boss刷新点 (guid 5400001-5400100)
-- 规则: 20个区域, 每区5只同模型同等级Boss
-- 等级按区域匹配: 8/16/24/32/40/48/56/64/72/80

DELETE FROM `creature` WHERE `guid` BETWEEN 5400001 AND 5400100;

-- ========== 等级8 (Boss 1-10) ==========
-- 区域1: 艾尔文森林 (map 0) - Boss 396001-396005 炎魔
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400001,396001,0,0,0,1,1,0,-9693.8,-287.4,59.1,0,600,0,0,0),
(5400002,396002,0,0,0,1,1,0,-9743,-361,53.9,0,600,0,0,0),
(5400003,396003,0,0,0,1,1,0,-9553,-728,99.3,0,600,0,0,0),
(5400004,396004,0,0,0,1,1,0,-9259,-21,73.2,0,600,0,0,0),
(5400005,396005,0,0,0,1,1,0,-9718,-85,36.1,0,600,0,0,0);

-- 区域2: 杜隆塔尔 (map 1) - Boss 396006-396010 骨龙
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400006,396006,1,0,0,1,1,0,1179.6,-4249.5,23.3,0,600,0,0,0),
(5400007,396007,1,0,0,1,1,0,1360.5,-4484.8,27.8,0,600,0,0,0),
(5400008,396008,1,0,0,1,1,0,1046.8,-4103.9,18.2,0,600,0,0,0),
(5400009,396009,1,0,0,1,1,0,1374.8,-4356.5,26.4,0,600,0,0,0),
(5400010,396010,1,0,0,1,1,0,861.8,-4177.5,18.0,0,600,0,0,0);

-- ========== 等级16 (Boss 11-20) ==========
-- 区域3: 西部荒野 (map 0) - Boss 396011-396015 暗影恶魔
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400011,396011,0,0,0,1,1,0,-9952.6,1540.5,45.2,0,600,0,0,0),
(5400012,396012,0,0,0,1,1,0,-10469.8,1345.2,45.8,0,600,0,0,0),
(5400013,396013,0,0,0,1,1,0,-9968.5,1113.8,39.7,0,600,0,0,0),
(5400014,396014,0,0,0,1,1,0,-9847,1027.7,33.3,0,600,0,0,0),
(5400015,396015,0,0,0,1,1,0,-10169.2,1278.1,37.2,0,600,0,0,0);

-- 区域4: 贫瘠之地 (map 1) - Boss 396016-396020 深渊领主
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400016,396016,1,0,0,1,1,0,-91,-3160.3,92.8,0,600,0,0,0),
(5400017,396017,1,0,0,1,1,0,-360.9,-2510.4,96.4,0,600,0,0,0),
(5400018,396018,1,0,0,1,1,0,-102.8,-3131.3,92.2,0,600,0,0,0),
(5400019,396019,1,0,0,1,1,0,47.6,-2910.1,92.2,0,600,0,0,0),
(5400020,396020,1,0,0,1,1,0,54.4,-2850.4,95.9,0,600,0,0,0);

-- ========== 等级24 (Boss 21-30) ==========
-- 区域5: 赤脊山 (map 0) - Boss 396021-396025 黑龙女王
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400021,396021,0,0,0,1,1,0,-9390,-2410.7,45.9,0,600,0,0,0),
(5400022,396022,0,0,0,1,1,0,-9360.3,-2523.2,15.9,0,600,0,0,0),
(5400023,396023,0,0,0,1,1,0,-9726,-2274,62.7,0,600,0,0,0),
(5400024,396024,0,0,0,1,1,0,-9164,-2442.9,110.9,0,600,0,0,0),
(5400025,396025,0,0,0,1,1,0,-9425.8,-2119.2,66.5,0,600,0,0,0);

-- 区域6: 灰谷 (map 1) - Boss 396026-396030 黑曜石龙
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400026,396026,1,0,0,1,1,0,2021.4,-2122,110.7,0,600,0,0,0),
(5400027,396027,1,0,0,1,1,0,2009.3,-2345.1,89.7,0,600,0,0,0),
(5400028,396028,1,0,0,1,1,0,1582.2,-1811.2,126.2,0,600,0,0,0),
(5400029,396029,1,0,0,1,1,0,1846.5,-2004.2,109.5,0,600,0,0,0),
(5400030,396030,1,0,0,1,1,0,1979.4,-2139.1,98.7,0,600,0,0,0);

-- ========== 等级32 (Boss 31-40) ==========
-- 区域7: 暮色森林 (map 0) - Boss 396031-396035 烈焰巨兽
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400031,396031,0,0,0,1,1,0,-11179.5,-112.9,7.8,0,600,0,0,0),
(5400032,396032,0,0,0,1,1,0,-11135.5,-159.1,10.7,0,600,0,0,0),
(5400033,396033,0,0,0,1,1,0,-10837.1,-573.6,36.7,0,600,0,0,0),
(5400034,396034,0,0,0,1,1,0,-10875.3,-290.3,37.5,0,600,0,0,0),
(5400035,396035,0,0,0,1,1,0,-11020.6,-154,15.3,0,600,0,0,0);

-- 区域8: 南贫瘠之地 (map 1) - Boss 396036-396040 魔能机甲
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400036,396036,1,0,0,1,1,0,-2181.2,-2477.4,95.9,0,600,0,0,0),
(5400037,396037,1,0,0,1,1,0,-2538.9,-2042.1,94.2,0,600,0,0,0),
(5400038,396038,1,0,0,1,1,0,-2472.9,-2041.6,92.4,0,600,0,0,0),
(5400039,396039,1,0,0,1,1,0,-2448.3,-2063.5,96.7,0,600,0,0,0),
(5400040,396040,1,0,0,1,1,0,-2253.3,-2241.2,93.1,0,600,0,0,0);

-- ========== 等级40 (Boss 41-50) ==========
-- 区域9: 阿拉希高地 (map 0) - Boss 396041-396045 蓝龙之王
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400041,396041,0,0,0,1,1,0,-1275.8,-2538.7,21.6,0,600,0,0,0),
(5400042,396042,0,0,0,1,1,0,-1208.5,-2343.7,60.4,0,600,0,0,0),
(5400043,396043,0,0,0,1,1,0,-1879.2,-2852.2,64.1,0,600,0,0,0),
(5400044,396044,0,0,0,1,1,0,-1234.5,-2560.2,23.7,0,600,0,0,0),
(5400045,396045,0,0,0,1,1,0,-1205,-2532.1,23,0,600,0,0,0);

-- 区域10: 尘泥沼泽 (map 1) - Boss 396046-396050 远古恶魔
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400046,396046,1,0,0,1,1,0,-3934.1,-2869.7,51.4,0,600,0,0,0),
(5400047,396047,1,0,0,1,1,0,-3984.8,-2910.7,36.5,0,600,0,0,0),
(5400048,396048,1,0,0,1,1,0,-3615.3,-2834.5,35.6,0,600,0,0,0),
(5400049,396049,1,0,0,1,1,0,-3663.5,-3020.2,39.6,0,600,0,0,0),
(5400050,396050,1,0,0,1,1,0,-3626.2,-3246.6,30.8,0,600,0,0,0);

-- ========== 等级48 (Boss 51-60) ==========
-- 区域11: 荆棘谷 (map 0) - Boss 396051-396055 虫族领主
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400051,396051,0,0,0,1,1,0,-12704.8,-478.6,30.4,0,600,0,0,0),
(5400052,396052,0,0,0,1,1,0,-12555,-453,27.6,0,600,0,0,0),
(5400053,396053,0,0,0,1,1,0,-12659.7,-485.7,29.7,0,600,0,0,0),
(5400054,396054,0,0,0,1,1,0,-12573,-555.7,36.7,0,600,0,0,0),
(5400055,396055,0,0,0,1,1,0,-12595.2,-659,40.4,0,600,0,0,0);

-- 区域12: 菲拉斯 (map 1) - Boss 396056-396060 恶魔之王
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400056,396056,1,0,0,1,1,0,-4543.2,1572.1,102.7,0,600,0,0,0),
(5400057,396057,1,0,0,1,1,0,-4939.8,1197.6,60.7,0,600,0,0,0),
(5400058,396058,1,0,0,1,1,0,-5151.7,1514.3,47.4,0,600,0,0,0),
(5400059,396059,1,0,0,1,1,0,-4689.3,1465.7,95.9,0,600,0,0,0),
(5400060,396060,1,0,0,1,1,0,-4890.8,1243.3,77.5,0,600,0,0,0);

-- ========== 等级56 (Boss 61-70) ==========
-- 区域13: 燃烧平原 (map 0) - Boss 396061-396065 萨隆邪铁
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400061,396061,0,0,0,1,1,0,-7726.1,-1432.6,140.9,0,600,0,0,0),
(5400062,396062,0,0,0,1,1,0,-7720.4,-1488.8,138.7,0,600,0,0,0),
(5400063,396063,0,0,0,1,1,0,-7821.9,-1898.2,135.2,0,600,0,0,0),
(5400064,396064,0,0,0,1,1,0,-7869.2,-1766.4,121.6,0,600,0,0,0),
(5400065,396065,0,0,0,1,1,0,-8092.7,-1889.3,137.4,0,600,0,0,0);

-- 区域14: 费伍德森林 (map 1) - Boss 396066-396070 风暴泰坦
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400066,396066,1,0,0,1,1,0,4177.3,-1235.1,343.3,0,600,0,0,0),
(5400067,396067,1,0,0,1,1,0,4156.7,-917.5,266.1,0,600,0,0,0),
(5400068,396068,1,0,0,1,1,0,4180.1,-1081.6,298.7,0,600,0,0,0),
(5400069,396069,1,0,0,1,1,0,4018.5,-749.8,286.8,0,600,0,0,0),
(5400070,396070,1,0,0,1,1,0,4065.7,-741.4,282.9,0,600,0,0,0);

-- ========== 等级64 (Boss 71-80) ==========
-- 区域15: 地狱火半岛 (map 530) - Boss 396071-396075 远古之神
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400071,396071,530,0,0,1,1,0,-319.7,4517.1,51.4,0,600,0,0,0),
(5400072,396072,530,0,0,1,1,0,-481,4348.9,37.1,0,600,0,0,0),
(5400073,396073,530,0,0,1,1,0,62,4217.8,79.3,0,600,0,0,0),
(5400074,396074,530,0,0,1,1,0,-396.8,4373.7,54,0,600,0,0,0),
(5400075,396075,530,0,0,1,1,0,173.8,4364.6,120.2,0,600,0,0,0);

-- 区域16: 赞加沼泽 (map 530) - Boss 396076-396080 骨骸领主
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400076,396076,530,0,0,1,1,0,281.3,6094.3,180.1,0,600,0,0,0),
(5400077,396077,530,0,0,1,1,0,143.9,5544.9,17.5,0,600,0,0,0),
(5400078,396078,530,0,0,1,1,0,-84.4,5989.4,18.1,0,600,0,0,0),
(5400079,396079,530,0,0,1,1,0,257.2,6020.7,62.4,0,600,0,0,0),
(5400080,396080,530,0,0,1,1,0,252.3,6030.8,131.7,0,600,0,0,0);

-- ========== 等级72 (Boss 81-90) ==========
-- 区域17: 嚎风峡湾 (map 571) - Boss 396081-396085 冰霜巨龙
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400081,396081,571,0,0,1,1,0,1618.8,-4922.7,138,0,600,0,0,0),
(5400082,396082,571,0,0,1,1,0,1526.6,-5139.1,183.8,0,600,0,0,0),
(5400083,396083,571,0,0,1,1,0,2195.1,-5115.1,236.6,0,600,0,0,0),
(5400084,396084,571,0,0,1,1,0,1507.8,-5034.9,119.2,0,600,0,0,0),
(5400085,396085,571,0,0,1,1,0,1988.5,-5019.9,213.2,0,600,0,0,0);

-- 区域18: 北风苔原 (map 571) - Boss 396086-396090 鲜血女王
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400086,396086,571,0,0,1,1,0,3130.8,5022.5,28.1,0,600,0,0,0),
(5400087,396087,571,0,0,1,1,0,3100.5,5344.9,55.2,0,600,0,0,0),
(5400088,396088,571,0,0,1,1,0,2860.1,5041.1,26.6,0,600,0,0,0),
(5400089,396089,571,0,0,1,1,0,3142.2,5315.5,53.2,0,600,0,0,0),
(5400090,396090,571,0,0,1,1,0,3256.2,4978.8,31,0,600,0,0,0);

-- ========== 等级80 (Boss 91-100) ==========
-- 区域19: 祖达克 (map 571) - Boss 396091-396095 星界守望
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400091,396091,571,0,0,1,1,0,5611.6,-2896.9,274.6,0,600,0,0,0),
(5400092,396092,571,0,0,1,1,0,5502.9,-2944.9,276.7,0,600,0,0,0),
(5400093,396093,571,0,0,1,1,0,5568.1,-2932.5,277.9,0,600,0,0,0),
(5400094,396094,571,0,0,1,1,0,5551.7,-2917.8,277.8,0,600,0,0,0),
(5400095,396095,571,0,0,1,1,0,5325.7,-3373.5,297.6,0,600,0,0,0);

-- 区域20: 冰冠冰川 (map 571) - Boss 396096-396100 巫妖王
INSERT INTO `creature` (`guid`,`id1`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`MovementType`,`VerifiedBuild`) VALUES
(5400096,396096,571,0,0,1,1,0,7518,2069.7,530,0,600,0,0,0),
(5400097,396097,571,0,0,1,1,0,7273.9,1830.7,587.2,0,600,0,0,0),
(5400098,396098,571,0,0,1,1,0,7128.5,2066.2,622,0,600,0,0,0),
(5400099,396099,571,0,0,1,1,0,7518.6,1908.6,511.8,0,600,0,0,0),
(5400100,396100,571,0,0,1,1,0,7515.8,1958.7,502.3,0,600,0,0,0);

-- 完成: 100个称号Boss, 20个区域, 每区5只同模型同等级Boss

-- ===== 称号Boss 最终坐标 =====
-- 称号Boss396 坐标重排（敌对怪刷点版）
-- 每个区域 4 个坐标来自对应等级带的真实敌对怪刷点。
-- 第 4 个点用于称号Boss。

UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2388.28, `position_y` = 538.51, `position_z` = 39.032, `orientation` = 2.269, `Comment` = '称号Boss Lv8 提瑞斯法林地·西部 #396001' WHERE `id1` = 396001;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2827.17, `position_y` = 809.824, `position_z` = 113.697, `orientation` = 2.423, `Comment` = '称号Boss Lv8 提瑞斯法林地·东部 #396002' WHERE `id1` = 396002;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1618.61, `position_y` = 56.288, `position_z` = -13.219, `orientation` = 3.21, `Comment` = '称号Boss Lv8 莫高雷·西部 #396003' WHERE `id1` = 396003;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1485.13, `position_y` = 119.767, `position_z` = 0.113, `orientation` = -1.013, `Comment` = '称号Boss Lv8 莫高雷·东部 #396004' WHERE `id1` = 396004;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5515.93, `position_y` = 513.092, `position_z` = 387.936, `orientation` = 5.581, `Comment` = '称号Boss Lv8 丹莫罗·东部 #396005' WHERE `id1` = 396005;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5777, `position_y` = 202.862, `position_z` = 372.136, `orientation` = 5.019, `Comment` = '称号Boss Lv8 丹莫罗·西部 #396006' WHERE `id1` = 396006;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 10546.8, `position_y` = 1482.62, `position_z` = 1323.2, `orientation` = 3.501, `Comment` = '称号Boss Lv8 泰达希尔·南部 #396007' WHERE `id1` = 396007;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9769.95, `position_y` = 1611.7, `position_z` = 1285.49, `orientation` = 3.7, `Comment` = '称号Boss Lv8 泰达希尔·北部 #396008' WHERE `id1` = 396008;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9116.58, `position_y` = -568.454, `position_z` = 59.157, `orientation` = 3.952, `Comment` = '称号Boss Lv8 艾尔文森林 #396009' WHERE `id1` = 396009;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1130.52, `position_y` = -4572.23, `position_z` = 17.237, `orientation` = 3.078, `Comment` = '称号Boss Lv8 杜隆塔尔 #396010' WHERE `id1` = 396010;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -4916.53, `position_y` = -3629.65, `position_z` = 300.909, `orientation` = 1.594, `Comment` = '称号Boss Lv16 洛克莫丹·南部 #396011' WHERE `id1` = 396011;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5531.22, `position_y` = -2725.84, `position_z` = 366.841, `orientation` = 4.447, `Comment` = '称号Boss Lv16 洛克莫丹·北部 #396012' WHERE `id1` = 396012;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6419.79, `position_y` = 51.041, `position_z` = 33.182, `orientation` = 2.382, `Comment` = '称号Boss Lv16 黑海岸·南部 #396013' WHERE `id1` = 396013;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6106.17, `position_y` = 543.677, `position_z` = 2.593, `orientation` = 1.953, `Comment` = '称号Boss Lv16 黑海岸·北部 #396014' WHERE `id1` = 396014;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1545.62, `position_y` = 512.725, `position_z` = 48.221, `orientation` = 4.837, `Comment` = '称号Boss Lv16 银松森林·北部 #396015' WHERE `id1` = 396015;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1282.79, `position_y` = 183.962, `position_z` = -8.175, `orientation` = 0.192, `Comment` = '称号Boss Lv16 银松森林·南部 #396016' WHERE `id1` = 396016;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6843.48, `position_y` = -6281.33, `position_z` = 26.38, `orientation` = 3.214, `Comment` = '称号Boss Lv16 幽魂之地·北部 #396017' WHERE `id1` = 396017;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6950.91, `position_y` = -7479.96, `position_z` = 47.835, `orientation` = 0.749, `Comment` = '称号Boss Lv16 幽魂之地·南部 #396018' WHERE `id1` = 396018;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10328.2, `position_y` = 1120.76, `position_z` = 36.766, `orientation` = 3.205, `Comment` = '称号Boss Lv16 西部荒野 #396019' WHERE `id1` = 396019;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -654.986, `position_y` = -3051.04, `position_z` = 91.918, `orientation` = 1.257, `Comment` = '称号Boss Lv16 贫瘠之地 #396020' WHERE `id1` = 396020;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3345.93, `position_y` = -2141.74, `position_z` = 43.992, `orientation` = 3.645, `Comment` = '称号Boss Lv24 湿地·北部 #396021' WHERE `id1` = 396021;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3310.97, `position_y` = -3061.34, `position_z` = 21.852, `orientation` = 2.404, `Comment` = '称号Boss Lv24 湿地·南部 #396022' WHERE `id1` = 396022;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1454.69, `position_y` = 78.883, `position_z` = 18.395, `orientation` = 1.523, `Comment` = '称号Boss Lv24 石爪山脉·北部 #396023' WHERE `id1` = 396023;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1260.14, `position_y` = -483.923, `position_z` = 13.877, `orientation` = 3.387, `Comment` = '称号Boss Lv24 石爪山脉·南部 #396024' WHERE `id1` = 396024;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 292.679, `position_y` = 82.218, `position_z` = 49.298, `orientation` = 1.345, `Comment` = '称号Boss Lv24 希尔斯布莱德丘陵·北部 #396025' WHERE `id1` = 396025;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 209.679, `position_y` = -668.532, `position_z` = 109.377, `orientation` = 5.765, `Comment` = '称号Boss Lv24 希尔斯布莱德丘陵·南部 #396026' WHERE `id1` = 396026;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1750.99, `position_y` = 2581.59, `position_z` = 87.304, `orientation` = 1.55, `Comment` = '称号Boss Lv24 凄凉之地·西部 #396027' WHERE `id1` = 396027;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -697.307, `position_y` = 2063.26, `position_z` = 99.062, `orientation` = 2.266, `Comment` = '称号Boss Lv24 凄凉之地·东部 #396028' WHERE `id1` = 396028;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -9491.3, `position_y` = -2173.03, `position_z` = 89.904, `orientation` = 3.23, `Comment` = '称号Boss Lv24 赤脊山 #396029' WHERE `id1` = 396029;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2240, `position_y` = -1842.7, `position_z` = 81.7, `orientation` = 5.3, `Comment` = '称号Boss Lv24 灰谷 #396030' WHERE `id1` = 396030;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10576.1, `position_y` = -3553.9, `position_z` = 22.067, `orientation` = 4.553, `Comment` = '称号Boss Lv32 悲伤沼泽·南部 #396031' WHERE `id1` = 396031;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10528.5, `position_y` = -2541.92, `position_z` = 19.845, `orientation` = 2.566, `Comment` = '称号Boss Lv32 悲伤沼泽·北部 #396032' WHERE `id1` = 396032;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4887.97, `position_y` = -897.821, `position_z` = -4.888, `orientation` = 1.298, `Comment` = '称号Boss Lv32 千针石林·北部 #396033' WHERE `id1` = 396033;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5585.7, `position_y` = -1624.01, `position_z` = 15.571, `orientation` = 3.483, `Comment` = '称号Boss Lv32 千针石林·南部 #396034' WHERE `id1` = 396034;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 475.207, `position_y` = -581.735, `position_z` = 175.288, `orientation` = 3.235, `Comment` = '称号Boss Lv32 奥特兰克山脉·东部 #396035' WHERE `id1` = 396035;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -47.926, `position_y` = -178.853, `position_z` = 132.206, `orientation` = 1.567, `Comment` = '称号Boss Lv32 奥特兰克山脉·西部 #396036' WHERE `id1` = 396036;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11607.5, `position_y` = 645.048, `position_z` = 59.682, `orientation` = 4.111, `Comment` = '称号Boss Lv32 荆棘谷北部·北部 #396037' WHERE `id1` = 396037;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11101.4, `position_y` = -535.784, `position_z` = 33.308, `orientation` = 5.388, `Comment` = '称号Boss Lv32 荆棘谷北部·南部 #396038' WHERE `id1` = 396038;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11685, `position_y` = -54.297, `position_z` = 17.589, `orientation` = 5.667, `Comment` = '称号Boss Lv32 暮色森林 #396039' WHERE `id1` = 396039;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3513.15, `position_y` = -1891.2, `position_z` = 96.058, `orientation` = 0.054, `Comment` = '称号Boss Lv32 南贫瘠之地 #396040' WHERE `id1` = 396040;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6086.38, `position_y` = -3214.84, `position_z` = 262.956, `orientation` = 2.078, `Comment` = '称号Boss Lv40 荒芜之地·东部 #396041' WHERE `id1` = 396041;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6607.43, `position_y` = -2911.96, `position_z` = 245.138, `orientation` = 3.497, `Comment` = '称号Boss Lv40 荒芜之地·西部 #396042' WHERE `id1` = 396042;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7652.69, `position_y` = -3379.84, `position_z` = 57.169, `orientation` = 5.131, `Comment` = '称号Boss Lv40 塔纳利斯·西部 #396043' WHERE `id1` = 396043;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7326.04, `position_y` = -3958.94, `position_z` = 9.967, `orientation` = 0.267, `Comment` = '称号Boss Lv40 塔纳利斯·东部 #396044' WHERE `id1` = 396044;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6715.64, `position_y` = -1252.52, `position_z` = 242.069, `orientation` = 0.21, `Comment` = '称号Boss Lv40 灼热峡谷·南部 #396045' WHERE `id1` = 396045;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6765.86, `position_y` = -950.991, `position_z` = 243.282, `orientation` = 5.992, `Comment` = '称号Boss Lv40 灼热峡谷·北部 #396046' WHERE `id1` = 396046;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 352.247, `position_y` = -3097.83, `position_z` = 128.786, `orientation` = 1.899, `Comment` = '称号Boss Lv40 辛特兰·南部 #396047' WHERE `id1` = 396047;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 293.016, `position_y` = -2981.72, `position_z` = 113.933, `orientation` = 5.709, `Comment` = '称号Boss Lv40 辛特兰·北部 #396048' WHERE `id1` = 396048;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -1634.47, `position_y` = -2345.83, `position_z` = 76.797, `orientation` = 2.761, `Comment` = '称号Boss Lv40 阿拉希高地 #396049' WHERE `id1` = 396049;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -3485.04, `position_y` = -3171.71, `position_z` = 28.924, `orientation` = 5.767, `Comment` = '称号Boss Lv40 尘泥沼泽 #396050' WHERE `id1` = 396050;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10540.9, `position_y` = -3585.5, `position_z` = 22.622, `orientation` = 4.838, `Comment` = '称号Boss Lv48 诅咒之地·南部 #396051' WHERE `id1` = 396051;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10986.6, `position_y` = -2840.5, `position_z` = 11.794, `orientation` = 1.639, `Comment` = '称号Boss Lv48 诅咒之地·北部 #396052' WHERE `id1` = 396052;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3114.76, `position_y` = -4145.25, `position_z` = 103.617, `orientation` = 0.29, `Comment` = '称号Boss Lv48 艾萨拉·北部 #396053' WHERE `id1` = 396053;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2605.03, `position_y` = -4837.45, `position_z` = 138.905, `orientation` = 4.091, `Comment` = '称号Boss Lv48 艾萨拉·南部 #396054' WHERE `id1` = 396054;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7431.69, `position_y` = 1313.03, `position_z` = 2.257, `orientation` = 0.112, `Comment` = '称号Boss Lv48 安戈洛环形山·西部 #396055' WHERE `id1` = 396055;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6644.8, `position_y` = 709.394, `position_z` = 6.49, `orientation` = 0.127, `Comment` = '称号Boss Lv48 安戈洛环形山·东部 #396056' WHERE `id1` = 396056;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3023.69, `position_y` = -4654.56, `position_z` = 104.213, `orientation` = 1.868, `Comment` = '称号Boss Lv48 东瘟疫之地·南部 #396057' WHERE `id1` = 396057;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3157.8, `position_y` = -4177.61, `position_z` = 95.805, `orientation` = 4.017, `Comment` = '称号Boss Lv48 东瘟疫之地·北部 #396058' WHERE `id1` = 396058;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -12573, `position_y` = -555.679, `position_z` = 36.701, `orientation` = 5.71, `Comment` = '称号Boss Lv48 荆棘谷 #396059' WHERE `id1` = 396059;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -4491.4, `position_y` = 1807.56, `position_z` = 106.929, `orientation` = 3.402, `Comment` = '称号Boss Lv48 菲拉斯 #396060' WHERE `id1` = 396060;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1742.91, `position_y` = -951.218, `position_z` = 73.254, `orientation` = 3.633, `Comment` = '称号Boss Lv56 西瘟疫之地·北部 #396061' WHERE `id1` = 396061;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1544.94, `position_y` = -1875.5, `position_z` = 58.394, `orientation` = 1.219, `Comment` = '称号Boss Lv56 西瘟疫之地·南部 #396062' WHERE `id1` = 396062;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6636.51, `position_y` = -3922.2, `position_z` = 678.54, `orientation` = 5.506, `Comment` = '称号Boss Lv56 冬泉谷·东部 #396063' WHERE `id1` = 396063;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6213.32, `position_y` = -4171.97, `position_z` = 725.299, `orientation` = 5.246, `Comment` = '称号Boss Lv56 冬泉谷·西部 #396064' WHERE `id1` = 396064;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7001.89, `position_y` = 1317.44, `position_z` = 3.354, `orientation` = 0.088, `Comment` = '称号Boss Lv56 希利苏斯·南部 #396065' WHERE `id1` = 396065;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7329.62, `position_y` = 1837.43, `position_z` = -88.839, `orientation` = 0.569, `Comment` = '称号Boss Lv56 希利苏斯·北部 #396066' WHERE `id1` = 396066;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2970.8, `position_y` = 3428.27, `position_z` = 0.37, `orientation` = 1.151, `Comment` = '称号Boss Lv56 影月谷·西部 #396067' WHERE `id1` = 396067;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2924.21, `position_y` = 3412.8, `position_z` = 1.893, `orientation` = 5.652, `Comment` = '称号Boss Lv56 影月谷·东部 #396068' WHERE `id1` = 396068;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -8116.27, `position_y` = -1418.61, `position_z` = 131.726, `orientation` = 2.072, `Comment` = '称号Boss Lv56 燃烧平原 #396069' WHERE `id1` = 396069;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3609.52, `position_y` = -1156.05, `position_z` = 215.7, `orientation` = 2.95, `Comment` = '称号Boss Lv56 费伍德森林 #396070' WHERE `id1` = 396070;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2250.49, `position_y` = 5015.35, `position_z` = -3.083, `orientation` = 1.048, `Comment` = '称号Boss Lv64 泰罗卡森林·西部 #396071' WHERE `id1` = 396071;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2012.18, `position_y` = 4973.63, `position_z` = 28.036, `orientation` = 0.074, `Comment` = '称号Boss Lv64 泰罗卡森林·东部 #396072' WHERE `id1` = 396072;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1888.17, `position_y` = 7512.92, `position_z` = -6.368, `orientation` = 3.308, `Comment` = '称号Boss Lv64 纳格兰·西部 #396073' WHERE `id1` = 396073;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -978.598, `position_y` = 7958.43, `position_z` = 24.763, `orientation` = 3.179, `Comment` = '称号Boss Lv64 纳格兰·东部 #396074' WHERE `id1` = 396074;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 2692.1, `position_y` = 6579.48, `position_z` = 21.63, `orientation` = 2.583, `Comment` = '称号Boss Lv64 刀锋山·北部 #396075' WHERE `id1` = 396075;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3395.1, `position_y` = 5445.12, `position_z` = 146.032, `orientation` = 2.833, `Comment` = '称号Boss Lv64 刀锋山·南部 #396076' WHERE `id1` = 396076;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3879.9, `position_y` = 4050.85, `position_z` = 120.936, `orientation` = 2.276, `Comment` = '称号Boss Lv64 虚空风暴·东部 #396077' WHERE `id1` = 396077;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3187.02, `position_y` = 3812.87, `position_z` = 140.167, `orientation` = 3.687, `Comment` = '称号Boss Lv64 虚空风暴·西部 #396078' WHERE `id1` = 396078;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -176.153, `position_y` = 4943.58, `position_z` = 57.469, `orientation` = 1.07, `Comment` = '称号Boss Lv64 地狱火半岛 #396079' WHERE `id1` = 396079;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 643.179, `position_y` = 6273.02, `position_z` = 21.95, `orientation` = 1.362, `Comment` = '称号Boss Lv64 赞加沼泽 #396080' WHERE `id1` = 396080;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3674.94, `position_y` = -820.554, `position_z` = 164.761, `orientation` = 3.368, `Comment` = '称号Boss Lv72 龙骨荒野·东部 #396081' WHERE `id1` = 396081;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3115.57, `position_y` = -865.589, `position_z` = 42.009, `orientation` = 3.455, `Comment` = '称号Boss Lv72 龙骨荒野·西部 #396082' WHERE `id1` = 396082;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4170.88, `position_y` = -4041.09, `position_z` = 165.364, `orientation` = 3.776, `Comment` = '称号Boss Lv72 灰熊丘陵·南部 #396083' WHERE `id1` = 396083;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3373.01, `position_y` = -3008.71, `position_z` = 192.651, `orientation` = 1.303, `Comment` = '称号Boss Lv72 灰熊丘陵·北部 #396084' WHERE `id1` = 396084;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6432.62, `position_y` = 4951.51, `position_z` = -40.535, `orientation` = 2.214, `Comment` = '称号Boss Lv72 索拉查盆地北·南部 #396085' WHERE `id1` = 396085;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6119.07, `position_y` = 5865.8, `position_z` = 54.498, `orientation` = 5.559, `Comment` = '称号Boss Lv72 索拉查盆地北·北部 #396086' WHERE `id1` = 396086;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2703.98, `position_y` = -1973.19, `position_z` = 6.677, `orientation` = -1.099, `Comment` = '称号Boss Lv72 龙骨荒野南·西部 #396087' WHERE `id1` = 396087;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2823.46, `position_y` = -1939, `position_z` = 10.76, `orientation` = 1.481, `Comment` = '称号Boss Lv72 龙骨荒野南·东部 #396088' WHERE `id1` = 396088;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2230.14, `position_y` = -4929.9, `position_z` = 244.343, `orientation` = 5.658, `Comment` = '称号Boss Lv72 嚎风峡湾 #396089' WHERE `id1` = 396089;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3100.4, `position_y` = 5342.54, `position_z` = 55.638, `orientation` = -2.977, `Comment` = '称号Boss Lv72 北风苔原 #396090' WHERE `id1` = 396090;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 8292.88, `position_y` = -2572.04, `position_z` = 1146.46, `orientation` = 1.325, `Comment` = '称号Boss Lv80 风暴峭壁·东部 #396091' WHERE `id1` = 396091;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7242.79, `position_y` = -2266.94, `position_z` = 756.768, `orientation` = -1.671, `Comment` = '称号Boss Lv80 风暴峭壁·西部 #396092' WHERE `id1` = 396092;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5802.63, `position_y` = 4049.33, `position_z` = -82.737, `orientation` = 0.383, `Comment` = '称号Boss Lv80 索拉查盆地·南部 #396093' WHERE `id1` = 396093;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5814.6, `position_y` = 5253.06, `position_z` = -94.873, `orientation` = 2.887, `Comment` = '称号Boss Lv80 索拉查盆地·北部 #396094' WHERE `id1` = 396094;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5129.76, `position_y` = 2545.37, `position_z` = 366.021, `orientation` = 4.421, `Comment` = '称号Boss Lv80 冬拥湖·南部 #396095' WHERE `id1` = 396095;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5120.63, `position_y` = 2482.88, `position_z` = 357.282, `orientation` = 3.158, `Comment` = '称号Boss Lv80 冬拥湖·北部 #396096' WHERE `id1` = 396096;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6606.58, `position_y` = 489.339, `position_z` = 398.316, `orientation` = 5.604, `Comment` = '称号Boss Lv80 水晶之歌森林·东部 #396097' WHERE `id1` = 396097;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6723.19, `position_y` = -48.113, `position_z` = 749.283, `orientation` = 2.976, `Comment` = '称号Boss Lv80 水晶之歌森林·西部 #396098' WHERE `id1` = 396098;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5632.96, `position_y` = -2859.65, `position_z` = 274.382, `orientation` = 3.653, `Comment` = '称号Boss Lv80 祖达克 #396099' WHERE `id1` = 396099;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7903.32, `position_y` = 2252.6, `position_z` = 382.979, `orientation` = 6.231, `Comment` = '称号Boss Lv80 冰冠冰川 #396100' WHERE `id1` = 396100;

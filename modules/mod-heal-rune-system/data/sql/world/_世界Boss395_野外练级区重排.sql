-- 世界Boss395 野外练级区刷新重排
-- 规则: 40 个区域，每区 5 只 Boss，共 200 只
-- 回血Boss(395001-395100) 20个区域，切割Boss(395101-395200) 20个区域
-- 回血与切割区域互不重叠，且不与称号Boss(396001-396100)区域重复

UPDATE `creature` SET `equipment_id` = 0 WHERE `id1` BETWEEN 395001 AND 395200;

-- ========================================
-- 回血Boss (395001-395100)
-- ========================================

-- 等级8 - 提瑞斯法林地 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1920.1, `position_y` = 643.7, `position_z` = 44.4, `orientation` = 0, `Comment` = '回血Boss Lv8 提瑞斯法林地 #395001' WHERE `id1` = 395001;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2242.4, `position_y` = 599.4, `position_z` = 33.3, `orientation` = 0, `Comment` = '回血Boss Lv8 提瑞斯法林地 #395002' WHERE `id1` = 395002;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2053.8, `position_y` = 948.8, `position_z` = 37.3, `orientation` = 0, `Comment` = '回血Boss Lv8 提瑞斯法林地 #395003' WHERE `id1` = 395003;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2049.1, `position_y` = 647.2, `position_z` = 36.9, `orientation` = 0, `Comment` = '回血Boss Lv8 提瑞斯法林地 #395004' WHERE `id1` = 395004;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2321.8, `position_y` = 582.7, `position_z` = 24.8, `orientation` = 0, `Comment` = '回血Boss Lv8 提瑞斯法林地 #395005' WHERE `id1` = 395005;

-- 等级8 - 莫高雷 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -2180.9, `position_y` = -120.8, `position_z` = -5.6, `orientation` = 0, `Comment` = '回血Boss Lv8 莫高雷 #395006' WHERE `id1` = 395006;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -2178.9, `position_y` = 13.4, `position_z` = 23.9, `orientation` = 0, `Comment` = '回血Boss Lv8 莫高雷 #395007' WHERE `id1` = 395007;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1579.2, `position_y` = -295.6, `position_z` = -27.4, `orientation` = 0, `Comment` = '回血Boss Lv8 莫高雷 #395008' WHERE `id1` = 395008;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1516, `position_y` = -255.5, `position_z` = -11.5, `orientation` = 0, `Comment` = '回血Boss Lv8 莫高雷 #395009' WHERE `id1` = 395009;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -2055.3, `position_y` = -94.6, `position_z` = -7.3, `orientation` = 0, `Comment` = '回血Boss Lv8 莫高雷 #395010' WHERE `id1` = 395010;

-- 等级16 - 洛克莫丹 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5034.8, `position_y` = -2889, `position_z` = 337.4, `orientation` = 0, `Comment` = '回血Boss Lv16 洛克莫丹 #395011' WHERE `id1` = 395011;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5420, `position_y` = -2818.2, `position_z` = 356.9, `orientation` = 0, `Comment` = '回血Boss Lv16 洛克莫丹 #395012' WHERE `id1` = 395012;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5305.8, `position_y` = -2970.6, `position_z` = 346.6, `orientation` = 0, `Comment` = '回血Boss Lv16 洛克莫丹 #395013' WHERE `id1` = 395013;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5274.5, `position_y` = -2924.1, `position_z` = 349.5, `orientation` = 0, `Comment` = '回血Boss Lv16 洛克莫丹 #395014' WHERE `id1` = 395014;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5391.7, `position_y` = -2974.9, `position_z` = 325.9, `orientation` = 0, `Comment` = '回血Boss Lv16 洛克莫丹 #395015' WHERE `id1` = 395015;

-- 等级16 - 黑海岸 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6321, `position_y` = 223.2, `position_z` = 36.8, `orientation` = 0, `Comment` = '回血Boss Lv16 黑海岸 #395016' WHERE `id1` = 395016;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6384.2, `position_y` = 620.6, `position_z` = -5.5, `orientation` = 0, `Comment` = '回血Boss Lv16 黑海岸 #395017' WHERE `id1` = 395017;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6394.7, `position_y` = 649.8, `position_z` = -15.8, `orientation` = 0, `Comment` = '回血Boss Lv16 黑海岸 #395018' WHERE `id1` = 395018;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6508, `position_y` = 528.1, `position_z` = -5.7, `orientation` = 0, `Comment` = '回血Boss Lv16 黑海岸 #395019' WHERE `id1` = 395019;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6595.4, `position_y` = 578.5, `position_z` = -10.2, `orientation` = 0, `Comment` = '回血Boss Lv16 黑海岸 #395020' WHERE `id1` = 395020;

-- 等级24 - 湿地 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3113, `position_y` = -2260, `position_z` = 10.7, `orientation` = 0, `Comment` = '回血Boss Lv24 湿地 #395021' WHERE `id1` = 395021;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3372, `position_y` = -2264.6, `position_z` = 53.6, `orientation` = 0, `Comment` = '回血Boss Lv24 湿地 #395022' WHERE `id1` = 395022;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3446.1, `position_y` = -2759.4, `position_z` = 6.5, `orientation` = 0, `Comment` = '回血Boss Lv24 湿地 #395023' WHERE `id1` = 395023;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3450.6, `position_y` = -2350.7, `position_z` = 51.9, `orientation` = 0, `Comment` = '回血Boss Lv24 湿地 #395024' WHERE `id1` = 395024;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -3419.4, `position_y` = -2436.7, `position_z` = 52, `orientation` = 0, `Comment` = '回血Boss Lv24 湿地 #395025' WHERE `id1` = 395025;

-- 等级24 - 石爪山脉 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 989, `position_y` = -4.8, `position_z` = 23.4, `orientation` = 0, `Comment` = '回血Boss Lv24 石爪山脉 #395026' WHERE `id1` = 395026;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1082.7, `position_y` = -48.7, `position_z` = 4.5, `orientation` = 0, `Comment` = '回血Boss Lv24 石爪山脉 #395027' WHERE `id1` = 395027;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1118.4, `position_y` = -38, `position_z` = 0.9, `orientation` = 0, `Comment` = '回血Boss Lv24 石爪山脉 #395028' WHERE `id1` = 395028;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 1152.3, `position_y` = -118.3, `position_z` = -0.7, `orientation` = 0, `Comment` = '回血Boss Lv24 石爪山脉 #395029' WHERE `id1` = 395029;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 866.6, `position_y` = 292.5, `position_z` = 22.7, `orientation` = 0, `Comment` = '回血Boss Lv24 石爪山脉 #395030' WHERE `id1` = 395030;

-- 等级32 - 悲伤沼泽 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10387.6, `position_y` = -2642.3, `position_z` = 22, `orientation` = 0, `Comment` = '回血Boss Lv32 悲伤沼泽 #395031' WHERE `id1` = 395031;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10127.5, `position_y` = -2839.6, `position_z` = 22.3, `orientation` = 0, `Comment` = '回血Boss Lv32 悲伤沼泽 #395032' WHERE `id1` = 395032;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10437.7, `position_y` = -2627, `position_z` = 23, `orientation` = 0, `Comment` = '回血Boss Lv32 悲伤沼泽 #395033' WHERE `id1` = 395033;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10196.1, `position_y` = -3082.6, `position_z` = 23.2, `orientation` = 0, `Comment` = '回血Boss Lv32 悲伤沼泽 #395034' WHERE `id1` = 395034;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -10429.1, `position_y` = -2676.9, `position_z` = 22.4, `orientation` = 0, `Comment` = '回血Boss Lv32 悲伤沼泽 #395035' WHERE `id1` = 395035;

-- 等级32 - 千针石林 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5183.8, `position_y` = -1170.2, `position_z` = 45.2, `orientation` = 0, `Comment` = '回血Boss Lv32 千针石林 #395036' WHERE `id1` = 395036;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5075.5, `position_y` = -952.7, `position_z` = -4.9, `orientation` = 0, `Comment` = '回血Boss Lv32 千针石林 #395037' WHERE `id1` = 395037;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5124.7, `position_y` = -1036.4, `position_z` = -5.5, `orientation` = 0, `Comment` = '回血Boss Lv32 千针石林 #395038' WHERE `id1` = 395038;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5011.3, `position_y` = -842.1, `position_z` = -5.3, `orientation` = 0, `Comment` = '回血Boss Lv32 千针石林 #395039' WHERE `id1` = 395039;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -5026.7, `position_y` = -949.2, `position_z` = 61.9, `orientation` = 0, `Comment` = '回血Boss Lv32 千针石林 #395040' WHERE `id1` = 395040;

-- 等级40 - 荒芜之地 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6102, `position_y` = -3038.4, `position_z` = 248.9, `orientation` = 0, `Comment` = '回血Boss Lv40 荒芜之地 #395041' WHERE `id1` = 395041;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6273.1, `position_y` = -2976.3, `position_z` = 223.7, `orientation` = 0, `Comment` = '回血Boss Lv40 荒芜之地 #395042' WHERE `id1` = 395042;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6339, `position_y` = -3118.4, `position_z` = 293.1, `orientation` = 0, `Comment` = '回血Boss Lv40 荒芜之地 #395043' WHERE `id1` = 395043;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6112.9, `position_y` = -2996.5, `position_z` = 399.3, `orientation` = 0, `Comment` = '回血Boss Lv40 荒芜之地 #395044' WHERE `id1` = 395044;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6121.7, `position_y` = -2941.8, `position_z` = 207.9, `orientation` = 0, `Comment` = '回血Boss Lv40 荒芜之地 #395045' WHERE `id1` = 395045;

-- 等级40 - 塔纳利斯 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7120.8, `position_y` = -3774.2, `position_z` = 9, `orientation` = 0, `Comment` = '回血Boss Lv40 塔纳利斯 #395046' WHERE `id1` = 395046;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7116.3, `position_y` = -3788.1, `position_z` = 8.6, `orientation` = 0, `Comment` = '回血Boss Lv40 塔纳利斯 #395047' WHERE `id1` = 395047;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7384.6, `position_y` = -3254.7, `position_z` = 12.3, `orientation` = 0, `Comment` = '回血Boss Lv40 塔纳利斯 #395048' WHERE `id1` = 395048;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7122.4, `position_y` = -3770.6, `position_z` = 9.4, `orientation` = 0, `Comment` = '回血Boss Lv40 塔纳利斯 #395049' WHERE `id1` = 395049;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7121.4, `position_y` = -3479.1, `position_z` = 9.1, `orientation` = 0, `Comment` = '回血Boss Lv40 塔纳利斯 #395050' WHERE `id1` = 395050;

-- 等级48 - 诅咒之地 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11363.2, `position_y` = -3051.7, `position_z` = -4.2, `orientation` = 0, `Comment` = '回血Boss Lv48 诅咒之地 #395051' WHERE `id1` = 395051;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11436.9, `position_y` = -2949.2, `position_z` = 4.5, `orientation` = 0, `Comment` = '回血Boss Lv48 诅咒之地 #395052' WHERE `id1` = 395052;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11137.2, `position_y` = -2713.4, `position_z` = 11.6, `orientation` = 0, `Comment` = '回血Boss Lv48 诅咒之地 #395053' WHERE `id1` = 395053;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11460.1, `position_y` = -2957.5, `position_z` = 10.3, `orientation` = 0, `Comment` = '回血Boss Lv48 诅咒之地 #395054' WHERE `id1` = 395054;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11155.3, `position_y` = -2731, `position_z` = 14.1, `orientation` = 0, `Comment` = '回血Boss Lv48 诅咒之地 #395055' WHERE `id1` = 395055;

-- 等级48 - 艾萨拉 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3188.1, `position_y` = -4311.8, `position_z` = 104.6, `orientation` = 0, `Comment` = '回血Boss Lv48 艾萨拉 #395056' WHERE `id1` = 395056;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3263, `position_y` = -4437.7, `position_z` = 101.8, `orientation` = 0, `Comment` = '回血Boss Lv48 艾萨拉 #395057' WHERE `id1` = 395057;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2976.2, `position_y` = -4182.5, `position_z` = 100.5, `orientation` = 0, `Comment` = '回血Boss Lv48 艾萨拉 #395058' WHERE `id1` = 395058;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 3287.7, `position_y` = -4321.4, `position_z` = 131.2, `orientation` = 0, `Comment` = '回血Boss Lv48 艾萨拉 #395059' WHERE `id1` = 395059;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 2950.9, `position_y` = -4152.9, `position_z` = 100.2, `orientation` = 0, `Comment` = '回血Boss Lv48 艾萨拉 #395060' WHERE `id1` = 395060;

-- 等级56 - 西瘟疫之地 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1922.2, `position_y` = -1548.5, `position_z` = 61, `orientation` = 0, `Comment` = '回血Boss Lv56 西瘟疫之地 #395061' WHERE `id1` = 395061;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1930.6, `position_y` = -1714.8, `position_z` = 61.7, `orientation` = 0, `Comment` = '回血Boss Lv56 西瘟疫之地 #395062' WHERE `id1` = 395062;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1788.8, `position_y` = -1361.3, `position_z` = 63.6, `orientation` = 0, `Comment` = '回血Boss Lv56 西瘟疫之地 #395063' WHERE `id1` = 395063;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2156.5, `position_y` = -1743.1, `position_z` = 59.7, `orientation` = 0, `Comment` = '回血Boss Lv56 西瘟疫之地 #395064' WHERE `id1` = 395064;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1729.2, `position_y` = -1172.2, `position_z` = 59.4, `orientation` = 0, `Comment` = '回血Boss Lv56 西瘟疫之地 #395065' WHERE `id1` = 395065;

-- 等级56 - 冬泉谷 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6509.6, `position_y` = -3083.7, `position_z` = 594.2, `orientation` = 0, `Comment` = '回血Boss Lv56 冬泉谷 #395066' WHERE `id1` = 395066;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6509.1, `position_y` = -3475.4, `position_z` = 630.5, `orientation` = 0, `Comment` = '回血Boss Lv56 冬泉谷 #395067' WHERE `id1` = 395067;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6415.4, `position_y` = -3116.1, `position_z` = 582.1, `orientation` = 0, `Comment` = '回血Boss Lv56 冬泉谷 #395068' WHERE `id1` = 395068;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6484.9, `position_y` = -3166, `position_z` = 570.2, `orientation` = 0, `Comment` = '回血Boss Lv56 冬泉谷 #395069' WHERE `id1` = 395069;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 6877.9, `position_y` = -3355.3, `position_z` = 724.2, `orientation` = 0, `Comment` = '回血Boss Lv56 冬泉谷 #395070' WHERE `id1` = 395070;

-- 等级64 - 泰罗卡森林 (map 530)
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2262.7, `position_y` = 4715.8, `position_z` = -1.2, `orientation` = 0, `Comment` = '回血Boss Lv64 泰罗卡森林 #395071' WHERE `id1` = 395071;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2644.7, `position_y` = 4449.7, `position_z` = 36.2, `orientation` = 0, `Comment` = '回血Boss Lv64 泰罗卡森林 #395072' WHERE `id1` = 395072;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2562.4, `position_y` = 4387, `position_z` = 34.6, `orientation` = 0, `Comment` = '回血Boss Lv64 泰罗卡森林 #395073' WHERE `id1` = 395073;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2458.6, `position_y` = 4883.9, `position_z` = 34.6, `orientation` = 0, `Comment` = '回血Boss Lv64 泰罗卡森林 #395074' WHERE `id1` = 395074;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2605, `position_y` = 4440.7, `position_z` = 36.2, `orientation` = 0, `Comment` = '回血Boss Lv64 泰罗卡森林 #395075' WHERE `id1` = 395075;

-- 等级64 - 纳格兰 (map 530)
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1204.4, `position_y` = 8178.3, `position_z` = -6.3, `orientation` = 0, `Comment` = '回血Boss Lv64 纳格兰 #395076' WHERE `id1` = 395076;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1520, `position_y` = 7818.5, `position_z` = -102.3, `orientation` = 0, `Comment` = '回血Boss Lv64 纳格兰 #395077' WHERE `id1` = 395077;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1112, `position_y` = 7769.8, `position_z` = 17.9, `orientation` = 0, `Comment` = '回血Boss Lv64 纳格兰 #395078' WHERE `id1` = 395078;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1128.1, `position_y` = 8033.5, `position_z` = -81.5, `orientation` = 0, `Comment` = '回血Boss Lv64 纳格兰 #395079' WHERE `id1` = 395079;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -1558.5, `position_y` = 7638.6, `position_z` = -7.7, `orientation` = 0, `Comment` = '回血Boss Lv64 纳格兰 #395080' WHERE `id1` = 395080;

-- 等级72 - 龙骨荒野 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3707, `position_y` = -539, `position_z` = 292.4, `orientation` = 0, `Comment` = '回血Boss Lv72 龙骨荒野 #395081' WHERE `id1` = 395081;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3485.6, `position_y` = -1197.2, `position_z` = 113.5, `orientation` = 0, `Comment` = '回血Boss Lv72 龙骨荒野 #395082' WHERE `id1` = 395082;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3465.7, `position_y` = -1154.1, `position_z` = 113.1, `orientation` = 0, `Comment` = '回血Boss Lv72 龙骨荒野 #395083' WHERE `id1` = 395083;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3925.2, `position_y` = -861.4, `position_z` = 121.9, `orientation` = 0, `Comment` = '回血Boss Lv72 龙骨荒野 #395084' WHERE `id1` = 395084;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3577.1, `position_y` = -1154.7, `position_z` = 88.9, `orientation` = 0, `Comment` = '回血Boss Lv72 龙骨荒野 #395085' WHERE `id1` = 395085;

-- 等级72 - 灰熊丘陵 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3777.8, `position_y` = -3111.9, `position_z` = 279.2, `orientation` = 0, `Comment` = '回血Boss Lv72 灰熊丘陵 #395086' WHERE `id1` = 395086;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4322.9, `position_y` = -3693.6, `position_z` = 263.9, `orientation` = 0, `Comment` = '回血Boss Lv72 灰熊丘陵 #395087' WHERE `id1` = 395087;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4034.4, `position_y` = -3471, `position_z` = 272.2, `orientation` = 0, `Comment` = '回血Boss Lv72 灰熊丘陵 #395088' WHERE `id1` = 395088;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4213.7, `position_y` = -3655.9, `position_z` = 255.1, `orientation` = 0, `Comment` = '回血Boss Lv72 灰熊丘陵 #395089' WHERE `id1` = 395089;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4143.8, `position_y` = -3790.8, `position_z` = 194.8, `orientation` = 0, `Comment` = '回血Boss Lv72 灰熊丘陵 #395090' WHERE `id1` = 395090;

-- 等级80 - 风暴峭壁 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7594.4, `position_y` = -2281.3, `position_z` = 940.1, `orientation` = 0, `Comment` = '回血Boss Lv80 风暴峭壁 #395091' WHERE `id1` = 395091;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7384.3, `position_y` = -2517.3, `position_z` = 750, `orientation` = 0, `Comment` = '回血Boss Lv80 风暴峭壁 #395092' WHERE `id1` = 395092;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7311.8, `position_y` = -2530.1, `position_z` = 749.5, `orientation` = 0, `Comment` = '回血Boss Lv80 风暴峭壁 #395093' WHERE `id1` = 395093;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7309.5, `position_y` = -2613, `position_z` = 814.9, `orientation` = 0, `Comment` = '回血Boss Lv80 风暴峭壁 #395094' WHERE `id1` = 395094;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 7304, `position_y` = -2519.6, `position_z` = 750.3, `orientation` = 0, `Comment` = '回血Boss Lv80 风暴峭壁 #395095' WHERE `id1` = 395095;

-- 等级80 - 索拉查盆地 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5826.2, `position_y` = 4610.8, `position_z` = -134.2, `orientation` = 0, `Comment` = '回血Boss Lv80 索拉查盆地 #395096' WHERE `id1` = 395096;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5619.1, `position_y` = 4702.1, `position_z` = -136.5, `orientation` = 0, `Comment` = '回血Boss Lv80 索拉查盆地 #395097' WHERE `id1` = 395097;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5780.6, `position_y` = 4504.5, `position_z` = -133.4, `orientation` = 0, `Comment` = '回血Boss Lv80 索拉查盆地 #395098' WHERE `id1` = 395098;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5503.4, `position_y` = 4885, `position_z` = -198.3, `orientation` = 0, `Comment` = '回血Boss Lv80 索拉查盆地 #395099' WHERE `id1` = 395099;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5616.6, `position_y` = 4284.5, `position_z` = -103.4, `orientation` = 0, `Comment` = '回血Boss Lv80 索拉查盆地 #395100' WHERE `id1` = 395100;

-- ========================================
-- 切割Boss (395101-395200)
-- ========================================

-- 等级8 - 丹莫罗 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5826.3, `position_y` = -376.7, `position_z` = 376.3, `orientation` = 0, `Comment` = '切割Boss Lv8 丹莫罗 #395101' WHERE `id1` = 395101;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5310.4, `position_y` = 135.8, `position_z` = 388.7, `orientation` = 0, `Comment` = '切割Boss Lv8 丹莫罗 #395102' WHERE `id1` = 395102;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5432.3, `position_y` = -134.6, `position_z` = 350.6, `orientation` = 0, `Comment` = '切割Boss Lv8 丹莫罗 #395103' WHERE `id1` = 395103;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5532.2, `position_y` = -296.4, `position_z` = 358.1, `orientation` = 0, `Comment` = '切割Boss Lv8 丹莫罗 #395104' WHERE `id1` = 395104;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -5669.4, `position_y` = -256.4, `position_z` = 368.2, `orientation` = 0, `Comment` = '切割Boss Lv8 丹莫罗 #395105' WHERE `id1` = 395105;

-- 等级8 - 泰达希尔 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 10134.3, `position_y` = 1086.4, `position_z` = 1327.6, `orientation` = 0, `Comment` = '切割Boss Lv8 泰达希尔 #395106' WHERE `id1` = 395106;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9695.8, `position_y` = 1240.8, `position_z` = 1281.9, `orientation` = 0, `Comment` = '切割Boss Lv8 泰达希尔 #395107' WHERE `id1` = 395107;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9850.4, `position_y` = 818.6, `position_z` = 1308.1, `orientation` = 0, `Comment` = '切割Boss Lv8 泰达希尔 #395108' WHERE `id1` = 395108;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 10084, `position_y` = 1119.2, `position_z` = 1326.5, `orientation` = 0, `Comment` = '切割Boss Lv8 泰达希尔 #395109' WHERE `id1` = 395109;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = 9604.1, `position_y` = 1269.7, `position_z` = 1287.3, `orientation` = 0, `Comment` = '切割Boss Lv8 泰达希尔 #395110' WHERE `id1` = 395110;

-- 等级16 - 银松森林 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 479.5, `position_y` = 954, `position_z` = 128.9, `orientation` = 0, `Comment` = '切割Boss Lv16 银松森林 #395111' WHERE `id1` = 395111;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1014, `position_y` = 682.2, `position_z` = 65.1, `orientation` = 0, `Comment` = '切割Boss Lv16 银松森林 #395112' WHERE `id1` = 395112;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 482.4, `position_y` = 984.1, `position_z` = 128.7, `orientation` = 0, `Comment` = '切割Boss Lv16 银松森林 #395113' WHERE `id1` = 395113;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 1071.9, `position_y` = 644.7, `position_z` = 50.6, `orientation` = 0, `Comment` = '切割Boss Lv16 银松森林 #395114' WHERE `id1` = 395114;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 995.3, `position_y` = 696.3, `position_z` = 70, `orientation` = 0, `Comment` = '切割Boss Lv16 银松森林 #395115' WHERE `id1` = 395115;

-- 等级16 - 幽魂之地 (map 530)
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 7256.3, `position_y` = -6915.9, `position_z` = 49, `orientation` = 0, `Comment` = '切割Boss Lv16 幽魂之地 #395116' WHERE `id1` = 395116;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6897.1, `position_y` = -6971.6, `position_z` = 47.2, `orientation` = 0, `Comment` = '切割Boss Lv16 幽魂之地 #395117' WHERE `id1` = 395117;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 7113.6, `position_y` = -6924.3, `position_z` = 47.5, `orientation` = 0, `Comment` = '切割Boss Lv16 幽魂之地 #395118' WHERE `id1` = 395118;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 7157.5, `position_y` = -6611.4, `position_z` = 60.7, `orientation` = 0, `Comment` = '切割Boss Lv16 幽魂之地 #395119' WHERE `id1` = 395119;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 6933.8, `position_y` = -6715.6, `position_z` = 23.8, `orientation` = 0, `Comment` = '切割Boss Lv16 幽魂之地 #395120' WHERE `id1` = 395120;

-- 等级24 - 希尔斯布莱德丘陵 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 3.9, `position_y` = -338.2, `position_z` = 131.3, `orientation` = 0, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵 #395121' WHERE `id1` = 395121;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -413.8, `position_y` = 53.4, `position_z` = 54.3, `orientation` = 0, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵 #395122' WHERE `id1` = 395122;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 62.3, `position_y` = -228.4, `position_z` = 129.7, `orientation` = 0, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵 #395123' WHERE `id1` = 395123;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -81.9, `position_y` = -82.5, `position_z` = 136.9, `orientation` = 0, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵 #395124' WHERE `id1` = 395124;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -314.4, `position_y` = -531.5, `position_z` = 57, `orientation` = 0, `Comment` = '切割Boss Lv24 希尔斯布莱德丘陵 #395125' WHERE `id1` = 395125;

-- 等级24 - 凄凉之地 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1468, `position_y` = 2824.8, `position_z` = 92.5, `orientation` = 0, `Comment` = '切割Boss Lv24 凄凉之地 #395126' WHERE `id1` = 395126;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1408.5, `position_y` = 2709.6, `position_z` = 93.5, `orientation` = 0, `Comment` = '切割Boss Lv24 凄凉之地 #395127' WHERE `id1` = 395127;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1221.9, `position_y` = 2917.2, `position_z` = 87.2, `orientation` = 0, `Comment` = '切割Boss Lv24 凄凉之地 #395128' WHERE `id1` = 395128;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1710.1, `position_y` = 2577.5, `position_z` = 117.6, `orientation` = 0, `Comment` = '切割Boss Lv24 凄凉之地 #395129' WHERE `id1` = 395129;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -1458.9, `position_y` = 2797.6, `position_z` = 93.8, `orientation` = 0, `Comment` = '切割Boss Lv24 凄凉之地 #395130' WHERE `id1` = 395130;

-- 等级32 - 奥特兰克山脉 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 843.6, `position_y` = -541.1, `position_z` = 139.9, `orientation` = 0, `Comment` = '切割Boss Lv32 奥特兰克山脉 #395131' WHERE `id1` = 395131;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 447.5, `position_y` = -644.7, `position_z` = 167.2, `orientation` = 0, `Comment` = '切割Boss Lv32 奥特兰克山脉 #395132' WHERE `id1` = 395132;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 235, `position_y` = -359.1, `position_z` = 161.4, `orientation` = 0, `Comment` = '切割Boss Lv32 奥特兰克山脉 #395133' WHERE `id1` = 395133;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 355.4, `position_y` = -590, `position_z` = 154, `orientation` = 0, `Comment` = '切割Boss Lv32 奥特兰克山脉 #395134' WHERE `id1` = 395134;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 888.1, `position_y` = -121.8, `position_z` = 38.5, `orientation` = 0, `Comment` = '切割Boss Lv32 奥特兰克山脉 #395135' WHERE `id1` = 395135;

-- 等级32 - 荆棘谷北部 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11513.4, `position_y` = -53.4, `position_z` = 13.7, `orientation` = 0, `Comment` = '切割Boss Lv32 荆棘谷北部 #395136' WHERE `id1` = 395136;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11457.6, `position_y` = 378, `position_z` = 77.6, `orientation` = 0, `Comment` = '切割Boss Lv32 荆棘谷北部 #395137' WHERE `id1` = 395137;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11764.4, `position_y` = 16.8, `position_z` = 22.6, `orientation` = 0, `Comment` = '切割Boss Lv32 荆棘谷北部 #395138' WHERE `id1` = 395138;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11658.1, `position_y` = 561.9, `position_z` = 50.8, `orientation` = 0, `Comment` = '切割Boss Lv32 荆棘谷北部 #395139' WHERE `id1` = 395139;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -11460.3, `position_y` = 385.8, `position_z` = 77.5, `orientation` = 0, `Comment` = '切割Boss Lv32 荆棘谷北部 #395140' WHERE `id1` = 395140;

-- 等级40 - 灼热峡谷 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6641, `position_y` = -861.6, `position_z` = 244.3, `orientation` = 0, `Comment` = '切割Boss Lv40 灼热峡谷 #395141' WHERE `id1` = 395141;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6639.9, `position_y` = -805.5, `position_z` = 244.7, `orientation` = 0, `Comment` = '切割Boss Lv40 灼热峡谷 #395142' WHERE `id1` = 395142;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6685.1, `position_y` = -879.4, `position_z` = 257.6, `orientation` = 0, `Comment` = '切割Boss Lv40 灼热峡谷 #395143' WHERE `id1` = 395143;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6454.2, `position_y` = -937.5, `position_z` = 334.5, `orientation` = 0, `Comment` = '切割Boss Lv40 灼热峡谷 #395144' WHERE `id1` = 395144;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = -6531.1, `position_y` = -1297, `position_z` = 201, `orientation` = 0, `Comment` = '切割Boss Lv40 灼热峡谷 #395145' WHERE `id1` = 395145;

-- 等级40 - 辛特兰 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 414.1, `position_y` = -3716.3, `position_z` = 130, `orientation` = 0, `Comment` = '切割Boss Lv40 辛特兰 #395146' WHERE `id1` = 395146;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 276.8, `position_y` = -3585.1, `position_z` = 123.1, `orientation` = 0, `Comment` = '切割Boss Lv40 辛特兰 #395147' WHERE `id1` = 395147;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 332.5, `position_y` = -3335.4, `position_z` = 115.8, `orientation` = 0, `Comment` = '切割Boss Lv40 辛特兰 #395148' WHERE `id1` = 395148;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 376.1, `position_y` = -3170.1, `position_z` = 146.4, `orientation` = 0, `Comment` = '切割Boss Lv40 辛特兰 #395149' WHERE `id1` = 395149;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 451.2, `position_y` = -3348.2, `position_z` = 119.7, `orientation` = 0, `Comment` = '切割Boss Lv40 辛特兰 #395150' WHERE `id1` = 395150;

-- 等级48 - 安戈洛环形山 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6513.5, `position_y` = 827.7, `position_z` = 13, `orientation` = 0, `Comment` = '切割Boss Lv48 安戈洛环形山 #395151' WHERE `id1` = 395151;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6685.5, `position_y` = 805.1, `position_z` = 14.5, `orientation` = 0, `Comment` = '切割Boss Lv48 安戈洛环形山 #395152' WHERE `id1` = 395152;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6804.9, `position_y` = 820.4, `position_z` = 51.3, `orientation` = 0, `Comment` = '切割Boss Lv48 安戈洛环形山 #395153' WHERE `id1` = 395153;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6593.8, `position_y` = 887.4, `position_z` = -43.7, `orientation` = 0, `Comment` = '切割Boss Lv48 安戈洛环形山 #395154' WHERE `id1` = 395154;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6939.1, `position_y` = 1114.2, `position_z` = 0.9, `orientation` = 0, `Comment` = '切割Boss Lv48 安戈洛环形山 #395155' WHERE `id1` = 395155;

-- 等级48 - 东瘟疫之地 (map 0)
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2510, `position_y` = -4160.9, `position_z` = 47.1, `orientation` = 0, `Comment` = '切割Boss Lv48 东瘟疫之地 #395156' WHERE `id1` = 395156;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2548.8, `position_y` = -4449.4, `position_z` = 78.5, `orientation` = 0, `Comment` = '切割Boss Lv48 东瘟疫之地 #395157' WHERE `id1` = 395157;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2410.8, `position_y` = -4165.4, `position_z` = 75.5, `orientation` = 0, `Comment` = '切割Boss Lv48 东瘟疫之地 #395158' WHERE `id1` = 395158;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2567.7, `position_y` = -4599.9, `position_z` = 81, `orientation` = 0, `Comment` = '切割Boss Lv48 东瘟疫之地 #395159' WHERE `id1` = 395159;
UPDATE `creature` SET `map` = 0, `zoneId` = 0, `areaId` = 0, `position_x` = 2989.2, `position_y` = -4184, `position_z` = 95, `orientation` = 0, `Comment` = '切割Boss Lv48 东瘟疫之地 #395160' WHERE `id1` = 395160;

-- 等级56 - 希利苏斯 (map 1)
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6894.5, `position_y` = 1788.8, `position_z` = 5.4, `orientation` = 0, `Comment` = '切割Boss Lv56 希利苏斯 #395161' WHERE `id1` = 395161;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -6985.5, `position_y` = 1748.2, `position_z` = 2.1, `orientation` = 0, `Comment` = '切割Boss Lv56 希利苏斯 #395162' WHERE `id1` = 395162;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7323.1, `position_y` = 1729, `position_z` = -94.1, `orientation` = 0, `Comment` = '切割Boss Lv56 希利苏斯 #395163' WHERE `id1` = 395163;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7381.5, `position_y` = 1805.7, `position_z` = -32.4, `orientation` = 0, `Comment` = '切割Boss Lv56 希利苏斯 #395164' WHERE `id1` = 395164;
UPDATE `creature` SET `map` = 1, `zoneId` = 0, `areaId` = 0, `position_x` = -7329.6, `position_y` = 1837.4, `position_z` = -88.8, `orientation` = 0, `Comment` = '切割Boss Lv56 希利苏斯 #395165' WHERE `id1` = 395165;

-- 等级56 - 影月谷 (map 530)
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -2907.6, `position_y` = 2603, `position_z` = 93.8, `orientation` = 0, `Comment` = '切割Boss Lv56 影月谷 #395166' WHERE `id1` = 395166;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -3123, `position_y` = 2643, `position_z` = 62.4, `orientation` = 0, `Comment` = '切割Boss Lv56 影月谷 #395167' WHERE `id1` = 395167;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -3409.1, `position_y` = 2585.8, `position_z` = 57.6, `orientation` = 0, `Comment` = '切割Boss Lv56 影月谷 #395168' WHERE `id1` = 395168;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -3316.2, `position_y` = 2455.4, `position_z` = 51.7, `orientation` = 0, `Comment` = '切割Boss Lv56 影月谷 #395169' WHERE `id1` = 395169;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = -3016.4, `position_y` = 2549.7, `position_z` = 79.1, `orientation` = 0, `Comment` = '切割Boss Lv56 影月谷 #395170' WHERE `id1` = 395170;

-- 等级64 - 刀锋山 (map 530)
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3282.4, `position_y` = 5831.6, `position_z` = -3.5, `orientation` = 0, `Comment` = '切割Boss Lv64 刀锋山 #395171' WHERE `id1` = 395171;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3053.4, `position_y` = 5664, `position_z` = 143.4, `orientation` = 0, `Comment` = '切割Boss Lv64 刀锋山 #395172' WHERE `id1` = 395172;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3285.2, `position_y` = 5708.7, `position_z` = -5.4, `orientation` = 0, `Comment` = '切割Boss Lv64 刀锋山 #395173' WHERE `id1` = 395173;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3022.6, `position_y` = 5963.6, `position_z` = 130.8, `orientation` = 0, `Comment` = '切割Boss Lv64 刀锋山 #395174' WHERE `id1` = 395174;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3154.8, `position_y` = 6252.1, `position_z` = 124.9, `orientation` = 0, `Comment` = '切割Boss Lv64 刀锋山 #395175' WHERE `id1` = 395175;

-- 等级64 - 虚空风暴 (map 530)
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3590.3, `position_y` = 3544, `position_z` = 120, `orientation` = 0, `Comment` = '切割Boss Lv64 虚空风暴 #395176' WHERE `id1` = 395176;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3569.7, `position_y` = 3655.3, `position_z` = 129.8, `orientation` = 0, `Comment` = '切割Boss Lv64 虚空风暴 #395177' WHERE `id1` = 395177;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 4142.8, `position_y` = 3231.2, `position_z` = 192.1, `orientation` = 0, `Comment` = '切割Boss Lv64 虚空风暴 #395178' WHERE `id1` = 395178;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3987.4, `position_y` = 3785.5, `position_z` = 124.3, `orientation` = 0, `Comment` = '切割Boss Lv64 虚空风暴 #395179' WHERE `id1` = 395179;
UPDATE `creature` SET `map` = 530, `zoneId` = 0, `areaId` = 0, `position_x` = 3562.9, `position_y` = 3536.7, `position_z` = 128.1, `orientation` = 0, `Comment` = '切割Boss Lv64 虚空风暴 #395180' WHERE `id1` = 395180;

-- 等级72 - 索拉查盆地北 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5633, `position_y` = 5048.1, `position_z` = -134.3, `orientation` = 0, `Comment` = '切割Boss Lv72 索拉查盆地北 #395181' WHERE `id1` = 395181;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5932.6, `position_y` = 5589, `position_z` = -73, `orientation` = 0, `Comment` = '切割Boss Lv72 索拉查盆地北 #395182' WHERE `id1` = 395182;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6209.3, `position_y` = 5533.1, `position_z` = -47.3, `orientation` = 0, `Comment` = '切割Boss Lv72 索拉查盆地北 #395183' WHERE `id1` = 395183;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 6146.4, `position_y` = 5014.9, `position_z` = -98, `orientation` = 0, `Comment` = '切割Boss Lv72 索拉查盆地北 #395184' WHERE `id1` = 395184;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5904.2, `position_y` = 5639.4, `position_z` = -69.9, `orientation` = 0, `Comment` = '切割Boss Lv72 索拉查盆地北 #395185' WHERE `id1` = 395185;

-- 等级72 - 龙骨荒野南 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2994.2, `position_y` = -1843.6, `position_z` = 59.8, `orientation` = 0, `Comment` = '切割Boss Lv72 龙骨荒野南 #395186' WHERE `id1` = 395186;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2724.9, `position_y` = -1864.6, `position_z` = 19.7, `orientation` = 0, `Comment` = '切割Boss Lv72 龙骨荒野南 #395187' WHERE `id1` = 395187;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2846.8, `position_y` = -1714, `position_z` = 17.6, `orientation` = 0, `Comment` = '切割Boss Lv72 龙骨荒野南 #395188' WHERE `id1` = 395188;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 2813.6, `position_y` = -1637.9, `position_z` = 15.7, `orientation` = 0, `Comment` = '切割Boss Lv72 龙骨荒野南 #395189' WHERE `id1` = 395189;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 3307.8, `position_y` = -2003.8, `position_z` = 96.7, `orientation` = 0, `Comment` = '切割Boss Lv72 龙骨荒野南 #395190' WHERE `id1` = 395190;

-- 等级80 - 冬拥湖 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5129.8, `position_y` = 2545.4, `position_z` = 366, `orientation` = 0, `Comment` = '切割Boss Lv80 冬拥湖 #395191' WHERE `id1` = 395191;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5336, `position_y` = 2566.2, `position_z` = 396.2, `orientation` = 0, `Comment` = '切割Boss Lv80 冬拥湖 #395192' WHERE `id1` = 395192;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5440.3, `position_y` = 2870.6, `position_z` = 418.8, `orientation` = 0, `Comment` = '切割Boss Lv80 冬拥湖 #395193' WHERE `id1` = 395193;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5081.1, `position_y` = 2586.9, `position_z` = 364, `orientation` = 0, `Comment` = '切割Boss Lv80 冬拥湖 #395194' WHERE `id1` = 395194;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 4949, `position_y` = 2937.8, `position_z` = 550.5, `orientation` = 0, `Comment` = '切割Boss Lv80 冬拥湖 #395195' WHERE `id1` = 395195;

-- 等级80 - 水晶之歌森林 (map 571)
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5778.7, `position_y` = 351.7, `position_z` = 178.4, `orientation` = 0, `Comment` = '切割Boss Lv80 水晶之歌森林 #395196' WHERE `id1` = 395196;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5774, `position_y` = 746, `position_z` = 641.5, `orientation` = 0, `Comment` = '切割Boss Lv80 水晶之歌森林 #395197' WHERE `id1` = 395197;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5844.3, `position_y` = 610.2, `position_z` = 620.4, `orientation` = 0, `Comment` = '切割Boss Lv80 水晶之歌森林 #395198' WHERE `id1` = 395198;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5888.7, `position_y` = 507.9, `position_z` = 641.7, `orientation` = 0, `Comment` = '切割Boss Lv80 水晶之歌森林 #395199' WHERE `id1` = 395199;
UPDATE `creature` SET `map` = 571, `zoneId` = 0, `areaId` = 0, `position_x` = 5817.4, `position_y` = 417.1, `position_z` = 657.7, `orientation` = 0, `Comment` = '切割Boss Lv80 水晶之歌森林 #395200' WHERE `id1` = 395200;

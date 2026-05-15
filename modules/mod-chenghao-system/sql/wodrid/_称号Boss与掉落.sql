-- 称号系统100个专属Boss (430001-430100) + 掉落表
-- 难度: 参照回血Boss公式 * 2倍, 全部rank=3世界Boss
-- 等级: 10档(每档10个Boss), 8/16/24/32/40/48/56/64/72/80级
-- 模型: 每个Boss独立模型(由_称号Boss独立模型.sql覆盖)

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 430001 AND 430100;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 430001 AND 430100;
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 430001 AND 430100;

-- 使用存储过程批量生成
DROP PROCEDURE IF EXISTS `_tmp_gen_chenghao_boss`;
DELIMITER $$
CREATE PROCEDURE `_tmp_gen_chenghao_boss`()
BEGIN
  DECLARE i INT DEFAULT 1;
  DECLARE v_entry INT;
  DECLARE v_frag INT;
  DECLARE v_disp INT;
  DECLARE v_theme VARCHAR(20);
  DECLARE v_name VARCHAR(100);
  DECLARE v_hmod FLOAT;
  DECLARE v_dmod FLOAT;
  DECLARE v_amod FLOAT;
  DECLARE v_rank INT;
  DECLARE v_immune INT UNSIGNED;
  DECLARE v_flags INT;
  DECLARE v_tidx INT;
  DECLARE v_level INT;

  WHILE i <= 100 DO
    SET v_entry = 430000 + i;
    SET v_frag = 62100 + i;
    SET v_tidx = FLOOR((i - 1) / 5);

    -- 等级: 10个档位(每10个Boss一档), 从8级到80级, 每档+8级
    SET v_level = (FLOOR((i - 1) / 10) + 1) * 8;

    -- 20个主题外观(每5个Boss换一个模型)
    SET v_disp = ELT(v_tidx + 1,
      11121, 16033, 21135, 20127, 8570,       -- 1-25阶: 炎魔/骨龙/暗影恶魔/深渊领主/黑龙女王
      27035, 28875, 28611, 26752, 22711,       -- 26-50阶: 黑曜石龙/烈焰巨兽/魔能机甲/蓝龙之王/远古恶魔
      29268, 29615, 28548, 28977, 28817,       -- 51-75阶: 虫族领主/恶魔之王/萨隆邪铁/风暴泰坦/远古之神
      31119, 30362, 31093, 28641, 30721        -- 76-100阶: 骨骸领主/冰霜巨龙/鲜血女王/星界守望/巫妖王
    );
    SET v_theme = ELT(v_tidx + 1,
      '炎魔', '骨龙', '暗影恶魔', '深渊领主', '黑龙女王',
      '黑曜石龙', '烈焰巨兽', '魔能机甲', '蓝龙之王', '远古恶魔',
      '虫族领主', '恶魔之王', '萨隆邪铁', '风暴泰坦', '远古之神',
      '骨骸领主', '冰霜巨龙', '鲜血女王', '星界守望', '巫妖王'
    );
    SET v_name = CONCAT('称号守卫·', LPAD(i, 3, '0'));

    -- 难度: 参照回血Boss公式 * 2倍
    -- 回血Boss: HP=(60+g*12)*6, DMG=(4+g*0.45)*6, ARM=2.5+g*0.15
    -- 称号Boss: HP=(60+g*12)*12, DMG=(4+g*0.45)*12, ARM=2.5+g*0.15
    SET v_hmod = (60 + (v_tidx + 1) * 12) * 12;
    SET v_dmod = ROUND((4.0 + (v_tidx + 1) * 0.45) * 12, 1);
    SET v_amod = ROUND(2.5 + (v_tidx + 1) * 0.15, 2);

    -- Rank: 全部世界Boss级别 (rank=3)
    SET v_rank = 3;

    -- 控制免疫递增
    IF i <= 30 THEN SET v_immune = 0;
    ELSEIF i <= 60 THEN SET v_immune = 8388624;
    ELSEIF i <= 90 THEN SET v_immune = 617299839;
    ELSE SET v_immune = 2147483647;
    END IF;

    SET v_flags = IF(i > 50, 32768, 0);

    INSERT INTO `creature_template`
      (`entry`,`name`,`subname`,`minlevel`,`maxlevel`,`faction`,`unit_class`,`rank`,
       `mingold`,`maxgold`,`HealthModifier`,`DamageModifier`,`ArmorModifier`,
       `mechanic_immune_mask`,`unit_flags`,`detection_range`,`scale`,`lootid`,`type`,`AIName`)
    VALUES
      (v_entry, v_name, '称号守护者', v_level, v_level, 14, 1, v_rank,
       i * 50, i * 100, v_hmod, v_dmod, v_amod,
       v_immune, v_flags, 60, 3.0, v_entry, 10, '');

    -- 模型表 (AzerothCore用独立表)
    INSERT INTO `creature_template_model`
      (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
    VALUES
      (v_entry, 0, v_disp, 3.0, 1.0, 0);

    -- 掉落表: 100%掉对应阶碎片
    INSERT INTO `creature_loot_template`
      (`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
    VALUES
      (v_entry, v_frag, 0, 100, 0, 1, 0, 1, 1, CONCAT('称号碎片·第', i, '阶'));

    SET i = i + 1;
  END WHILE;
END$$
DELIMITER ;

CALL `_tmp_gen_chenghao_boss`();
DROP PROCEDURE IF EXISTS `_tmp_gen_chenghao_boss`;

-- ===== 内嵌最终独立模型 =====
-- 称号Boss 300独立模型更新 (430001-430100)
-- 每个Boss一个独特模型，不重复
UPDATE `creature_template_model` SET `DisplayScale` = 3.0
 WHERE `CreatureID` BETWEEN 430001 AND 430100 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 26193
 WHERE `CreatureID` = 430001 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 26087
 WHERE `CreatureID` = 430002 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 25656
 WHERE `CreatureID` = 430003 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 24106
 WHERE `CreatureID` = 430004 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 23685
 WHERE `CreatureID` = 430005 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 23136
 WHERE `CreatureID` = 430006 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16214
 WHERE `CreatureID` = 430007 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11402
 WHERE `CreatureID` = 430008 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1206
 WHERE `CreatureID` = 430009 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28818
 WHERE `CreatureID` = 430010 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28248
 WHERE `CreatureID` = 430011 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28239
 WHERE `CreatureID` = 430012 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 28230
 WHERE `CreatureID` = 430013 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 27504
 WHERE `CreatureID` = 430014 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 26286
 WHERE `CreatureID` = 430015 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 25680
 WHERE `CreatureID` = 430016 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 21180
 WHERE `CreatureID` = 430017 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20862
 WHERE `CreatureID` = 430018 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20810
 WHERE `CreatureID` = 430019 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20771
 WHERE `CreatureID` = 430020 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20770
 WHERE `CreatureID` = 430021 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20769
 WHERE `CreatureID` = 430022 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20768
 WHERE `CreatureID` = 430023 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20767
 WHERE `CreatureID` = 430024 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20766
 WHERE `CreatureID` = 430025 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20765
 WHERE `CreatureID` = 430026 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20764
 WHERE `CreatureID` = 430027 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20763
 WHERE `CreatureID` = 430028 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20762
 WHERE `CreatureID` = 430029 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20761
 WHERE `CreatureID` = 430030 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20590
 WHERE `CreatureID` = 430031 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 20044
 WHERE `CreatureID` = 430032 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 19824
 WHERE `CreatureID` = 430033 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 19816
 WHERE `CreatureID` = 430034 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 19681
 WHERE `CreatureID` = 430035 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 18070
 WHERE `CreatureID` = 430036 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 17625
 WHERE `CreatureID` = 430037 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 17445
 WHERE `CreatureID` = 430038 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16406
 WHERE `CreatureID` = 430039 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16176
 WHERE `CreatureID` = 430040 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16170
 WHERE `CreatureID` = 430041 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 16167
 WHERE `CreatureID` = 430042 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14528
 WHERE `CreatureID` = 430043 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14526
 WHERE `CreatureID` = 430044 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14525
 WHERE `CreatureID` = 430045 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14523
 WHERE `CreatureID` = 430046 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14497
 WHERE `CreatureID` = 430047 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14315
 WHERE `CreatureID` = 430048 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14313
 WHERE `CreatureID` = 430049 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14272
 WHERE `CreatureID` = 430050 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14257
 WHERE `CreatureID` = 430051 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 14255
 WHERE `CreatureID` = 430052 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12819
 WHERE `CreatureID` = 430053 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12342
 WHERE `CreatureID` = 430054 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12336
 WHERE `CreatureID` = 430055 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 12073
 WHERE `CreatureID` = 430056 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11640
 WHERE `CreatureID` = 430057 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11570
 WHERE `CreatureID` = 430058 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11566
 WHERE `CreatureID` = 430059 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11564
 WHERE `CreatureID` = 430060 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11562
 WHERE `CreatureID` = 430061 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11532
 WHERE `CreatureID` = 430062 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11511
 WHERE `CreatureID` = 430063 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11510
 WHERE `CreatureID` = 430064 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11453
 WHERE `CreatureID` = 430065 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11422
 WHERE `CreatureID` = 430066 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11414
 WHERE `CreatureID` = 430067 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11413
 WHERE `CreatureID` = 430068 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11412
 WHERE `CreatureID` = 430069 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11347
 WHERE `CreatureID` = 430070 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11331
 WHERE `CreatureID` = 430071 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11319
 WHERE `CreatureID` = 430072 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11316
 WHERE `CreatureID` = 430073 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11293
 WHERE `CreatureID` = 430074 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11262
 WHERE `CreatureID` = 430075 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11261
 WHERE `CreatureID` = 430076 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11257
 WHERE `CreatureID` = 430077 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11181
 WHERE `CreatureID` = 430078 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11179
 WHERE `CreatureID` = 430079 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11142
 WHERE `CreatureID` = 430080 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11140
 WHERE `CreatureID` = 430081 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11106
 WHERE `CreatureID` = 430082 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11099
 WHERE `CreatureID` = 430083 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11096
 WHERE `CreatureID` = 430084 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11092
 WHERE `CreatureID` = 430085 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11084
 WHERE `CreatureID` = 430086 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 11012
 WHERE `CreatureID` = 430087 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10983
 WHERE `CreatureID` = 430088 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10921
 WHERE `CreatureID` = 430089 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10920
 WHERE `CreatureID` = 430090 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10911
 WHERE `CreatureID` = 430091 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10904
 WHERE `CreatureID` = 430092 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10889
 WHERE `CreatureID` = 430093 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10850
 WHERE `CreatureID` = 430094 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10819
 WHERE `CreatureID` = 430095 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10807
 WHERE `CreatureID` = 430096 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10802
 WHERE `CreatureID` = 430097 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10800
 WHERE `CreatureID` = 430098 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10792
 WHERE `CreatureID` = 430099 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 10771
 WHERE `CreatureID` = 430100 AND `Idx` = 0;

-- 统一为称号Boss名称增加颜色，重复导入时不会叠加颜色码
UPDATE `creature_template`
SET `name` = CONCAT('|cFFCC0033', REPLACE(REPLACE(`name`, '|cFFCC0033', ''), '|r', ''), '|r')
WHERE `entry` BETWEEN 430001 AND 430100;

-- 完成: 100个称号Boss(430001-430100) + 模型 + 掉落

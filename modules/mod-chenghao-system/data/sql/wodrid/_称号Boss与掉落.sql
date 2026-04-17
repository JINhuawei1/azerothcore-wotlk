-- 称号系统100个专属Boss (396001-396100) + 掉落表
-- 难度: 参照回血Boss公式 * 2倍, 全部rank=3世界Boss
-- 等级: 10档(每档10个Boss), 8/16/24/32/40/48/56/64/72/80级
-- 模型: 每个Boss独立模型(由_称号Boss独立模型.sql覆盖)

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 396001 AND 396100;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 396001 AND 396100;
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 396001 AND 396100;

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
    SET v_entry = 396000 + i;
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
    SET v_name = CONCAT(v_theme, '·第', i, '阶');

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

-- 完成: 100个称号Boss(396001-396100) + 模型 + 掉落

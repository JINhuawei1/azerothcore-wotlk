-- ============================================================
-- 宣传奖励系统 - 系统配置表 (world 数据库)
-- 对接 mod-reward-template 的 `_模板_奖励` 表 与 mod-redemption-code 的 `_奖励_兑换码` 表
-- ============================================================

DROP TABLE IF EXISTS `_宣传奖励系统`;
CREATE TABLE `_宣传奖励系统` (
  `id`           int UNSIGNED NOT NULL DEFAULT 1 COMMENT '配置ID,固定为1。表中无此行 = 模块禁用',
  `注释`         varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `武器entry`    int UNSIGNED NOT NULL DEFAULT 997001 COMMENT '宣传神器首级 item_template entry。实际持有判定支持 997001-998000',
  `初始全属性值` int NOT NULL DEFAULT 1999 COMMENT '宣传1天时的全属性数值',
  `每日增量`     int NOT NULL DEFAULT 1000 COMMENT '每多1天宣传增加的全属性',
  `对接奖励组`   int UNSIGNED NOT NULL DEFAULT 9001 COMMENT '生成兑换码时写入 _奖励_兑换码.组 (mod-redemption-code 用于同组限领)',
  `对接需求ID`   int UNSIGNED NOT NULL DEFAULT 0 COMMENT '生成兑换码时写入 _奖励_兑换码.需求 (mod-requirement-template ID,0=无需求)',
  `对接奖励ID`   int UNSIGNED NOT NULL DEFAULT 0 COMMENT '生成宣传CDK时写入 _奖励_兑换码.奖励。宣传模块兑换时会按玩家天数映射到 _模板_奖励 100-1099',
  `领取公告`     int UNSIGNED NOT NULL DEFAULT 27 COMMENT '兑换码 _奖励_兑换码.领取公告 字段',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '宣传奖励系统配置' ROW_FORMAT = DYNAMIC;

INSERT INTO `_宣传奖励系统`
  (`id`, `注释`, `武器entry`, `初始全属性值`, `每日增量`,
   `对接奖励组`, `对接需求ID`, `对接奖励ID`, `领取公告`)
VALUES
  (1, '默认配置:通用宣传CDK,兑换时升级宣传神器997001-998000,奖励显示ID=100-1099',
      997001, 1999, 1000, 9001, 0, 100, 27);

-- ============================================================
-- 提示: 宣传CDK是通用码,不直接固定某一把武器。
-- 玩家每兑换一次宣传组CDK,模块会按该角色宣传天数升级:
--   第1次: 发宣传神器1
--   第2次: 回收宣传神器1,发宣传神器2
--   ...
-- `_宣传神器物品模板.sql` 会同步写入 _模板_奖励 100-1099 作为兑换结果显示:
--   100 -> 宣传神器1(997001)
--   101 -> 宣传神器2(997002)
--   ...
--   1099 -> 宣传神器1000(998000)
-- ============================================================

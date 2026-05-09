-- ============================================================
-- 宣传奖励系统 - 玩家累计天数表 (characters 数据库)
-- 玩家是否领取过武器、有几把武器,由 mod-redemption-code + _模板_奖励 决定
-- 本表只管"宣传天数",GM 用 .宣传奖励 发放 命令累加(1 CDK = 1 天)
-- ============================================================

DROP TABLE IF EXISTS `_宣传奖励系统玩家`;
CREATE TABLE `_宣传奖励系统玩家` (
  `玩家GUID` int UNSIGNED NOT NULL COMMENT 'characters.guid',
  `宣传天数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '累计宣传天数(永久绑定玩家,1 CDK = 1 天)',
  PRIMARY KEY (`玩家GUID`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '宣传奖励-玩家累计天数';

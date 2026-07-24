-- 十二生肖角色数据表
-- 冷却时间不落库，运行时按控制表读取；本表只保存玩家成长与命宫状态。

CREATE TABLE IF NOT EXISTS `_十二生肖玩家` (
    `玩家GUID` INT UNSIGNED NOT NULL COMMENT 'characters.guid',
    `生肖ID` TINYINT UNSIGNED NOT NULL,
    `已解锁` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `星级` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `修为` DECIMAL(65,0) UNSIGNED NOT NULL DEFAULT 0,
    `命宫槽位` TINYINT UNSIGNED NOT NULL DEFAULT 255 COMMENT '255=未装备',
    `主属性选择` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`玩家GUID`, `生肖ID`),
    KEY `idx_zodiac_player` (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='十二生肖玩家成长与命宫状态';

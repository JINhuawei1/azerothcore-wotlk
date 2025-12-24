-- 飞升系统角色数据表（紧凑格式）
-- 存储玩家的飞升装备数据和槽位解锁状态，一行代表一个角色

DROP TABLE IF EXISTS `_飞升系统_数据`;
CREATE TABLE IF NOT EXISTS `_飞升系统_数据` (
    `玩家GUID` INT UNSIGNED NOT NULL COMMENT '玩家角色GUID',
    `已解锁槽位` VARCHAR(64) DEFAULT '' COMMENT '已解锁的槽位列表，逗号分隔，如: 0,1,2,3',
    `装备数据` TEXT COMMENT '装备数据，格式: 槽位:物品ID:物品GUID,槽位:物品ID:物品GUID,...',
    PRIMARY KEY (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='飞升系统角色数据';

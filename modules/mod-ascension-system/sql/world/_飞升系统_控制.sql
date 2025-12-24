-- 飞升系统控制表
-- 用于配置每个槽位的解锁条件和属性倍率

DROP TABLE IF EXISTS `_飞升系统_控制`;
CREATE TABLE IF NOT EXISTS `_飞升系统_控制` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `槽位` TINYINT UNSIGNED NOT NULL COMMENT '0-17对应18个装备槽位',
    `槽位名称` VARCHAR(32) DEFAULT '' COMMENT '槽位中文名称',
    `启用` TINYINT UNSIGNED DEFAULT 1 COMMENT '是否启用此槽位 0=禁用 1=启用',
    `解锁需求` INT UNSIGNED DEFAULT 0 COMMENT '需求模板ID，0表示无需求直接解锁',
    `属性倍率` FLOAT DEFAULT 1.0 COMMENT '此槽位的属性倍率',
    `备注` VARCHAR(255) DEFAULT '',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_slot` (`槽位`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='飞升系统槽位控制表';

-- 初始化18个槽位数据
INSERT INTO `_飞升系统_控制` (`槽位`, `槽位名称`, `启用`, `解锁需求`, `属性倍率`, `备注`) VALUES
(0, '头部', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_HEAD'),
(1, '颈部', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_NECK'),
(2, '肩部', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_SHOULDERS'),
(3, '衬衣', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_BODY'),
(4, '胸甲', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_CHEST'),
(5, '腰带', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_WAIST'),
(6, '腿部', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_LEGS'),
(7, '脚部', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_FEET'),
(8, '手腕', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_WRISTS'),
(9, '手套', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_HANDS'),
(10, '戒指1', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_FINGER1'),
(11, '戒指2', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_FINGER2'),
(12, '饰品1', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_TRINKET1'),
(13, '饰品2', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_TRINKET2'),
(14, '披风', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_BACK'),
(15, '主手', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_MAINHAND'),
(16, '副手', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_OFFHAND'),
(17, '远程', 1, 0, 1.0, '对应 EQUIPMENT_SLOT_RANGED');

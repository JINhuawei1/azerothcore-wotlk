ALTER TABLE `_切割系统`
    MODIFY COLUMN `切割伤害` DECIMAL(30,4) NOT NULL DEFAULT '0.0000' COMMENT '固定值直接生效；百分比按目标当前血量计算，最大100';

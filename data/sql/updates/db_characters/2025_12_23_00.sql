-- 扩展玩家金币上限到40万金（4,000,000,000 铜币）
-- 将 money 字段从 int unsigned 改为 bigint unsigned
ALTER TABLE `characters` MODIFY COLUMN `money` bigint unsigned NOT NULL DEFAULT '0';

-- 幻境倍率 int256 化：INT UNSIGNED(上限42.9亿) → decimal(65,0)(上限≈1e65)
-- 支持 1亿~100亿倍及以上,消除溢出。旧值自动兼容(小数值在大列里无损)。
ALTER TABLE `待鉴定物品标记`
  MODIFY `幻境倍率` decimal(65,0) unsigned NOT NULL DEFAULT 1 COMMENT '拾取时的幻境倍率(int256)';

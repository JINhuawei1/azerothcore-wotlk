-- 优化待鉴定列表与批量鉴定查询：
-- WHERE `玩家GUID` = ? ORDER BY `物品GUID` 可直接走复合索引。
ALTER TABLE `待鉴定物品标记`
  ADD KEY `idx_玩家GUID_物品GUID` (`玩家GUID`, `物品GUID`);

-- ============================================
-- 首版 creature_loot_template 对接：由 _深渊自定义首领对接 / _深渊物品模板对接 批量生成
-- 说明：改为“引用池 + 概率”结构，后续继续细化时更容易扩展
-- ============================================

DELETE FROM `creature_loot_template`
WHERE (`Entry` BETWEEN 910001 AND 910074)
   OR (`Entry` BETWEEN 919001 AND 919012);

DELETE FROM `reference_loot_template`
WHERE (`Entry` BETWEEN 191001 AND 191074)
   OR (`Entry` BETWEEN 192001 AND 192074)
   OR (`Entry` BETWEEN 193001 AND 193074)
   OR (`Entry` BETWEEN 194001 AND 194074);

-- 自定义深渊 / 秘藏首领奖励统一改由模块 C++ 运行时代码直接发放，
-- 不再向 reference_loot_template / creature_loot_template 生成引用池，
-- 这样可以避免共享入口、章节复用与 lootid 清零后的无用告警。

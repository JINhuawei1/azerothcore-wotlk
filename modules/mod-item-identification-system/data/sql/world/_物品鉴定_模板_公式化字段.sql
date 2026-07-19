-- 鉴定模板公式化字段:每级增量 + 显式属性计算模式 + 公式类型
-- 取代"用数值大小(<=300)猜模式"的旧逻辑;所有系数进数据库,C++ 不硬编码
ALTER TABLE `_物品鉴定_模板`
  ADD COLUMN `基础每级增量` double NOT NULL DEFAULT 0 COMMENT '幻境等级每+1,基础属性百分比增加量(线性斜率)' AFTER `基础最大属性值`,
  ADD COLUMN `追加每级增量` double NOT NULL DEFAULT 0 COMMENT '幻境等级每+1,追加属性百分比增加量(线性斜率)' AFTER `追加属性最大值`,
  ADD COLUMN `属性计算模式` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0=绝对值(数值即属性点),1=官方百分比(数值即百分比,可超300)' AFTER `追加每级增量`,
  ADD COLUMN `公式类型` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0=线性(基础+等级*增量),1=指数(预留,暂未实现)' AFTER `属性计算模式`;

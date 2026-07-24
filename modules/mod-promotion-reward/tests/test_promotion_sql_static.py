from pathlib import Path


MODULE_ROOT = Path(__file__).resolve().parents[1]
WORLD_SQL = MODULE_ROOT / "sql" / "world" / "20260724_宣传审核配置.sql"
CHARACTERS_SQL = MODULE_ROOT / "sql" / "characters" / "20260724_宣传审核流水.sql"


def _read_sql(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_promotion_sql_files_exist_and_are_idempotent():
    world_sql = _read_sql(WORLD_SQL)
    characters_sql = _read_sql(CHARACTERS_SQL)
    sql = f"{world_sql}\n{characters_sql}"

    assert "CREATE TABLE IF NOT EXISTS" in sql
    assert "ON DUPLICATE KEY UPDATE" in world_sql
    assert "DROP TABLE" not in sql.upper()


def test_world_task_table_contains_configurable_fields():
    sql = _read_sql(WORLD_SQL)
    assert "`_宣传审核任务`" in sql
    for field in (
        "任务ID",
        "任务名称",
        "启用",
        "奖励模式",
        "奖励组",
        "需求ID",
        "奖励ID",
        "物品entry",
        "物品数量",
        "账号每日上限",
        "IP每日上限",
        "任务每日上限",
        "关键词",
        "连续无效封号次数",
        "创建时间",
        "更新时间",
    ):
        assert f"`{field}`" in sql


def test_characters_tables_and_audit_fields_exist():
    sql = _read_sql(CHARACTERS_SQL)
    for table in (
        "_宣传提交记录",
        "_宣传奖励流水",
        "_宣传兑换流水",
        "_宣传账号统计",
        "_宣传IP统计",
        "_宣传审核日志",
    ):
        assert f"`{table}`" in sql

    for field in (
        "提交ID",
        "任务ID",
        "账号ID",
        "角色GUID",
        "来源IP",
        "图片存储引用",
        "论坛链接",
        "内容哈希",
        "预检状态",
        "审核状态",
        "审核人账号ID",
        "审核理由",
        "发放模式",
        "request_id",
        "CDK",
        "奖励ID",
        "物品entry",
        "物品GUID",
        "发放状态",
        "兑换角色GUID",
        "兑换时间",
        "兑换前宣传天数",
        "兑换后宣传天数",
        "旧宣传物品GUID",
        "新宣传物品GUID",
        "新增奖励GUID",
        "资源快照",
        "回滚错误",
        "统计日期",
        "今日发放次数",
        "连续无效次数",
        "封禁状态",
        "IP地址",
        "审核人名称",
        "操作时间",
    ):
        assert f"`{field}`" in sql


def test_reward_request_and_submission_uniqueness_are_declared():
    sql = _read_sql(CHARACTERS_SQL)
    assert "UNIQUE KEY `uk_宣传奖励流水_提交ID` (`提交ID`)" in sql
    assert "UNIQUE KEY `uk_宣传奖励流水_request_id` (`request_id`)" in sql
    assert "KEY `idx_宣传奖励流水_CDK` (`CDK`)" in sql

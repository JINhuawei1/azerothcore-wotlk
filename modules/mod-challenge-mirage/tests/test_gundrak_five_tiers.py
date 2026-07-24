from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[3]
SQL_PATH = REPO_ROOT / "modules" / "mod-challenge-mirage" / "sql" / "world" / "_\u6311\u6218\u5e7b\u5883_\u53e4\u8fbe\u514b\u4ed9\u95e8\u8bd5\u70bc_\u4e94\u9636\u6269\u5c55.sql"
BASE_SQL_PATH = REPO_ROOT / "modules" / "mod-challenge-mirage" / "sql" / "world" / "_\u6311\u6218\u5e7b\u5883_\u53e4\u8fbe\u514b\u4ed9\u95e8\u8bd5\u70bc.sql"
AI_PATH = REPO_ROOT / "modules" / "mod-challenge-mirage" / "src" / "ChallengeMirageDungeonAI.cpp"
XIANMEN_PATH = REPO_ROOT / "modules" / "mod-xianmen-system" / "src" / "mod_xianmen_system.cpp"


class GundrakFiveTiersTest(unittest.TestCase):
    def test_xianmen_creature_absolute_stats_are_reduced_by_1000000(self):
        sql = BASE_SQL_PATH.read_text(encoding="utf-8-sig")
        attribute_insert = sql.split("INSERT INTO `_\u5c5e\u6027\u8c03\u6574_\u751f\u7269`", 1)[1]
        attribute_insert = attribute_insert.split("FROM `_tmp_mirage_gundrak_template` t;", 1)[0]

        divisor = "END / CAST(1000000 AS DECIMAL(65,0))"
        self.assertEqual(attribute_insert.count(divisor), 9)

    def test_sql_defines_four_database_driven_tiers(self):
        sql = SQL_PATH.read_text(encoding="utf-8-sig")

        expected_rows = [
            "(10002, 100, '\u4e8c\u7ea7', 2, 100000, 962200, 96100, 96299, 20, 5)",
            "(10003, 200, '\u4e09\u7ea7', 4, 1000000000, 970200, 96900, 97099, 40, 10)",
            "(10004, 300, '\u56db\u7ea7', 8, 10000000000000, 978200, 97700, 97899, 80, 20)",
            "(10005, 400, '\u4e94\u7ea7', 16, 100000000000000000, 986200, 98500, 98699, 160, 40)",
        ]
        for row in expected_rows:
            self.assertIn(row, sql)

        for token in (
            "START TRANSACTION",
            "COMMIT",
            "`InventoryType` <> 16",
            "`reference_loot_template`",
            "`creature_loot_template`",
            "`creature_template_model`",
            "`_\u5c5e\u6027\u8c03\u6574_\u751f\u7269`",
            "`_\u6311\u6218\u5e7b\u5883\u751f\u7269`",
            "`_\u6311\u6218\u5e7b\u5883\u7b49\u7ea7`",
        ):
            self.assertIn(token, sql)

    def test_ai_summons_and_minion_spells_follow_the_active_tier(self):
        source = AI_PATH.read_text(encoding="utf-8-sig")

        self.assertIn("GetGundrakTierEntry", source)
        self.assertIn("GetGundrakBaseEntry", source)
        self.assertIn("SummonMirageAdd(me, GetGundrakTierEntry(me->GetEntry(), entry)", source)
        self.assertIn("switch (GetGundrakBaseEntry(me->GetEntry()))", source)

    def test_boss_and_minion_contributions_double_by_tier(self):
        source = XIANMEN_PATH.read_text(encoding="utf-8-sig")

        self.assertIn("{ 10, 20, 40, 80, 160 }", source)
        self.assertIn("{ 5, 10, 20, 40, 80 }", source)
        self.assertIn("GetXianmenTrialTierIndex", source)


if __name__ == "__main__":
    unittest.main()

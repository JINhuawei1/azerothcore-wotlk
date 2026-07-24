from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[3]
SQL_PATHS = (
    REPO_ROOT / "modules" / "mod-synthesis-system" / "sql" / "world" / "_\u4ed9\u95e8\u88c5\u5907\u5408\u6210.sql",
    REPO_ROOT / "modules" / "mod-xianmen-system" / "sql" / "world" / "_\u4ed9\u95e8_10\u5793\u81f3999\u5793\u589e\u91cf.sql",
)


class XianmenSynthesisMaterialsTest(unittest.TestCase):
    def test_all_recipe_sources_use_reduced_material_costs(self):
        for path in SQL_PATHS:
            with self.subTest(path=path):
                sql = path.read_text(encoding="utf-8-sig")

                self.assertEqual(sql.count("' 2,62001 500,62002 100'"), 1)
                self.assertEqual(sql.count("' x2\u3001\u7075\u6c14\u77f3 x500\u3001\u7a81\u7834\u77f3 x100'"), 1)
                self.assertNotIn("' 3,62001 10000,62002 5000'", sql)
                self.assertNotIn("' x3\u3001\u7075\u6c14\u77f3 x10000\u3001\u7a81\u7834\u77f3 x5000'", sql)


if __name__ == "__main__":
    unittest.main()

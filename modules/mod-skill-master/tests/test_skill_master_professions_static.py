from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
MODULE = ROOT / "modules" / "mod-skill-master"
HEADER = MODULE / "src" / "SkillMaster.h"
SOURCE = MODULE / "src" / "SkillMaster.cpp"
WORLD_SQL = MODULE / "sql" / "world" / "技能综合大师.sql"


class SkillMasterProfessionStaticTest(unittest.TestCase):
    def test_main_menu_opens_fourteen_profession_submenu(self):
        header = HEADER.read_text(encoding="utf-8")
        source = SOURCE.read_text(encoding="utf-8")

        self.assertIn("SKILL_MASTER_MENU_PROFESSIONS", header)
        self.assertIn("SKILL_MASTER_MENU_PROFESSION_BASE", header)
        self.assertIn("void ShowProfessionMenu(Player* player, Creature* creature);", header)
        self.assertIn("case SKILL_MASTER_MENU_PROFESSIONS:", source)
        self.assertIn("ShowProfessionMenu(player, creature);", source)
        self.assertIn("|t 专业技能", source)
        self.assertIn("professionTrainerDefinitions.size()", source)
        self.assertIn("SKILL_MASTER_MENU_PROFESSION_BASE + index", source)

    def test_profession_definitions_cover_all_fourteen_professions(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("professionTrainerDefinitions", source)
        self.assertIn("primaryProfessionSkillIds", source)
        start = source.index("professionTrainerDefinitions")
        end = source.index("primaryProfessionSkillIds", start)
        definitions = source[start:end]

        expected = {
            "SKILL_MINING": ("采矿", "1681, 18747, 26912"),
            "SKILL_ENCHANTING": ("附魔", "1317, 18753, 26906"),
            "SKILL_ALCHEMY": ("炼金", "1215, 16588, 26903"),
            "SKILL_JEWELCRAFTING": ("珠宝", "15501, 18751, 26915"),
            "SKILL_BLACKSMITHING": ("锻造", "514, 16583, 26564, 5164, 7231"),
            "SKILL_ENGINEERING": ("工程", "1676, 17634, 25277, 8126, 7406, 24868"),
            "SKILL_TAILORING": ("裁缝", "1103, 18749, 26914, 4578"),
            "SKILL_FIRST_AID": ("急救", "2326, 18990, 23734"),
            "SKILL_COOKING": ("烹饪", "1355, 18987, 26905"),
            "SKILL_HERBALISM": ("草药", "812, 18748, 26910"),
            "SKILL_LEATHERWORKING": ("制皮", "1385, 18754, 26911, 7870, 7866, 7868"),
            "SKILL_SKINNING": ("剥皮", "1292, 18755, 26913"),
            "SKILL_FISHING": ("钓鱼", "1651, 18911, 26909"),
            "SKILL_INSCRIPTION": ("铭文", "26916"),
        }
        for skill, (name, trainer_ids) in expected.items():
            with self.subTest(skill=skill):
                self.assertIn(skill, definitions)
                self.assertIn(f'"{name}"', definitions)
                self.assertIn(trainer_ids, definitions)

        self.assertEqual(definitions.count("ProfessionTrainerDefinition{"), 14)

    def test_profession_menu_recalculates_eleven_primary_profession_slots(self):
        source = SOURCE.read_text(encoding="utf-8")

        self.assertIn("SKILL_MASTER_PRIMARY_PROFESSION_LIMIT = 11", source)
        self.assertIn("void SkillMasterCreatureScript::RefreshPrimaryProfessionPoints", source)
        self.assertIn("player->SetFreePrimaryProfessions(freePoints);", source)
        self.assertIn("RefreshPrimaryProfessionPoints(player);", source)

        for skill in (
            "SKILL_ALCHEMY",
            "SKILL_BLACKSMITHING",
            "SKILL_ENCHANTING",
            "SKILL_ENGINEERING",
            "SKILL_HERBALISM",
            "SKILL_INSCRIPTION",
            "SKILL_JEWELCRAFTING",
            "SKILL_LEATHERWORKING",
            "SKILL_MINING",
            "SKILL_SKINNING",
            "SKILL_TAILORING",
        ):
            self.assertIn(skill, source)

        self.assertNotIn("class SkillMasterServerScript", source)

    def test_world_sql_attaches_all_profession_trainer_templates(self):
        sql = WORLD_SQL.read_text(encoding="utf-8")

        self.assertIn("职业技能|专业技能|武器技能|骑术", sql)
        for reference in range(201001, 201043):
            self.assertIn(f"(60002, -{reference}, 0, 0, 0, 0, 0)", sql)
        for reference in range(202001, 202010):
            self.assertIn(f"(60002, -{reference}, 0, 0, 0, 0, 0)", sql)

    def test_world_sql_preserves_configured_creature_model_and_scale(self):
        sql = WORLD_SQL.read_text(encoding="utf-8")

        self.assertNotIn("DELETE FROM `creature_template` WHERE `entry` = 60002", sql)
        self.assertIn("ON DUPLICATE KEY UPDATE", sql)
        self.assertIn("20, 1, 0, 0, 1, 2000, 2000", sql)
        self.assertNotIn("20, 1.5, 0, 0, 1, 2000, 2000", sql)
        self.assertNotIn("DELETE FROM `creature_template_model` WHERE `CreatureID` = 60002", sql)
        self.assertIn("INSERT IGNORE INTO `creature_template_model`", sql)
        self.assertIn("(60002, 0, 30721, 1.3, 1.3, 12340);", sql)
        self.assertNotIn("(60002, 0, 3343, 1, 1, NULL);", sql)

    def test_world_sql_keeps_creature_icon_name_empty(self):
        sql = WORLD_SQL.read_text(encoding="utf-8")

        self.assertIn("'职业技能|专业技能|武器技能|骑术', '', 0, 80, 80", sql)
        self.assertNotIn("'职业技能|专业技能|武器技能|骑术', 'Train', 0, 80, 80", sql)
        self.assertIn("`IconName` = ''", sql)


if __name__ == "__main__":
    unittest.main()

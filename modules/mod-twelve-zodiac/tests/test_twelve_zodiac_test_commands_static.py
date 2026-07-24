import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "modules" / "mod-twelve-zodiac" / "src" / "TwelveZodiac.cpp"
UNIT_SOURCE = ROOT / "src" / "server" / "game" / "Entities" / "Unit" / "Unit.cpp"
UNIT_SCRIPT_HEADER = ROOT / "src" / "server" / "game" / "Scripting" / "ScriptDefines" / "UnitScript.h"
UNIT_SCRIPT_SOURCE = ROOT / "src" / "server" / "game" / "Scripting" / "ScriptDefines" / "UnitScript.cpp"
SCRIPT_MGR_HEADER = ROOT / "src" / "server" / "game" / "Scripting" / "ScriptMgr.h"
MAGIC_HIT_SOURCE = ROOT / "modules" / "mod-magic-hit-system" / "src" / "MagicHitScripts.cpp"


class TwelveZodiacTestCommandsStaticTest(unittest.TestCase):
    def test_single_command_profession_learning_and_forgetting(self):
        text = SOURCE.read_text(encoding="utf-8")

        self.assertIn('{ "测试学习", HandleTestLearn,', text)
        self.assertIn('{ "测试遗忘", HandleTestForget,', text)
        self.assertIn("ZODIAC_TEST_SKILL_IDS", text)
        self.assertIn("ZODIAC_TEST_FIRST_RANK_SPELL_IDS", text)
        self.assertIn("LearnAllProfessionRecipes(player, skillId)", text)
        self.assertIn("GetSkillLineAbilitiesBySkillLine(skillId)", text)
        self.assertIn("player->SetSkill(skillId, skillStep ? skillStep : 1, 450, 450)", text)
        self.assertIn("player->removeSpell(firstRankSpellId, SPEC_MASK_ALL, false)", text)
        self.assertIn("player->SetSkill(skillId, 0, 0, 0)", text)
        self.assertIn("SyncProfessionUnlocks(player, false)", text)
        self.assertNotIn("ResetPlayerForTesting(player)", text)

        for skill_id in (186, 333, 171, 755, 164, 202, 197, 129, 185, 182, 165, 393, 356, 773):
            self.assertIn(str(skill_id), text)

        for spell_id in (2575, 3908, 7620, 4036, 8613, 2259, 2550, 7411, 3273, 2018, 2108, 25229, 2366, 45357):
            self.assertIn(str(spell_id), text)

    def test_interface_command_pushes_open_request_through_hermes(self):
        text = SOURCE.read_text(encoding="utf-8")

        self.assertIn('constexpr char const* ZODIAC_OPEN_ADDON_PREFIX = "ZODIACOPEN";', text)
        self.assertIn('{ "界面", HandleOpenUi,', text)
        self.assertIn("static bool HandleOpenUi(ChatHandler* handler", text)
        self.assertIn(
            'HermesBridge_SendAddonMessage(player, ZODIAC_OPEN_ADDON_PREFIX, "OPEN_UI")',
            text,
        )
        self.assertIn("十二生肖UI通信尚未就绪", text)

    def test_unlock_state_tracks_current_profession_level_both_directions(self):
        text = SOURCE.read_text(encoding="utf-8")
        start = text.index("void TwelveZodiacManager::SyncProfessionUnlocks")
        end = text.index("void TwelveZodiacManager::OnSkillUpdate", start)
        sync_block = text[start:end]
        update_end = text.index("bool TwelveZodiacManager::TryTriggerDamage", end)
        update_block = text[end:update_end]

        self.assertIn(
            "bool const shouldUnlock = player->GetSkillValue(control.professionSkillId) >= control.requiredSkill;",
            sync_block,
        )
        self.assertIn("if (progress.unlocked == shouldUnlock)", sync_block)
        self.assertIn("progress.unlocked = shouldUnlock;", sync_block)
        self.assertIn("progress.slot = ZODIAC_INACTIVE_SLOT;", sync_block)
        self.assertIn("runtime->nextDamageReady[id] = 0;", sync_block)
        self.assertIn("runtime->nextHealReady[id] = 0;", sync_block)
        self.assertIn("runtime->nextDeathSaveReady[id] = 0;", sync_block)
        self.assertIn("专业低于", sync_block)

        self.assertIn("control.professionSkillId == skillId", update_block)
        self.assertNotIn("std::max(value, newValue) >= control.requiredSkill", update_block)

    def test_server_builds_database_driven_five_slot_recommendations(self):
        text = SOURCE.read_text(encoding="utf-8")

        self.assertIn("struct ZodiacCombinationRating", text)
        self.assertIn("EvaluateCombination", text)
        self.assertIn("FindBestCombinations", text)
        self.assertIn("damageRecommendation", text)
        self.assertIn("healRecommendation", text)
        self.assertIn("survivalRecommendation", text)
        self.assertIn("deathSaveCount", text)
        self.assertIn('payload << "GUIDE|"', text)

    def test_guide_payload_includes_full_totals_for_every_recommendation(self):
        text = SOURCE.read_text(encoding="utf-8")

        for recommendation in (
            "damageRecommendation",
            "healRecommendation",
            "survivalRecommendation",
        ):
            self.assertIn(f"ToAddonNumber({recommendation}.damageScore)", text)
            self.assertIn(f"ToAddonNumber({recommendation}.healScore)", text)
            self.assertIn(f"{recommendation}.deathSaveCount", text)

    def test_temporary_zodiac_damage_debug_logging_is_removed_without_removing_hooks(self):
        module_text = SOURCE.read_text(encoding="utf-8")
        unit_text = UNIT_SOURCE.read_text(encoding="utf-8")

        for marker in (
            "[十二生肖-伤害调试]",
            "[十二生肖-UnitScript入口]",
            "[十二生肖-核心伤害入口]",
            "ZODIAC_DAMAGE_DEBUG_INTERVAL_MS",
            "nextDamageDebugLog",
            "nextDamageEarlyReturnDebugLog",
            "nextUnitScriptDamageDebugLog",
            "nextZodiacCoreDamageDebugLog",
            "logEarlyReturn",
            "debugTrace",
            "zodiacCoreDebugTrace",
        ):
            self.assertNotIn(marker, module_text + unit_text)

        self.assertIn("sScriptMgr->OnBeforeDamageWithContext", unit_text)
        self.assertIn("sScriptMgr->OnDamage(attacker", unit_text)
        self.assertIn("sScriptMgr->OnDamageWithContext", unit_text)
        self.assertIn("s_twelveZodiacManager.ApplyDamage", module_text)
        self.assertIn("SendZodiacDamagePayload", module_text)

    def test_world_config_load_is_delayed_and_uses_async_database_query(self):
        text = SOURCE.read_text(encoding="utf-8")
        world_start = text.index("class TwelveZodiacWorldScript")
        world_end = text.index("class TwelveZodiacPlayerScript", world_start)
        world_block = text[world_start:world_end]

        self.assertIn("constexpr uint32 ZODIAC_DEFERRED_LOAD_DELAY_MS = 3000;", text)
        self.assertIn("WORLDHOOK_ON_AFTER_CONFIG_LOAD", world_block)
        self.assertIn("WORLDHOOK_ON_STARTUP", world_block)
        self.assertIn("WORLDHOOK_ON_UPDATE", world_block)
        self.assertIn("void OnStartup() override", world_block)
        self.assertIn("void OnUpdate(uint32 diff) override", world_block)
        self.assertIn("_queryProcessor.ProcessReadyCallbacks();", world_block)
        self.assertIn("WorldDatabase.AsyncQuery(ZODIAC_CONTROL_SELECT_SQL)", world_block)
        self.assertIn("s_twelveZodiacManager.ApplyControls(std::move(result));", world_block)
        self.assertIn("QueryCallbackProcessor _queryProcessor;", world_block)
        self.assertNotIn("s_twelveZodiacManager.LoadControls();", world_block)
        self.assertIn("void ScheduleReload()", world_block)
        self.assertIn("s_twelveZodiacWorldScript->ScheduleReload();", text)
        self.assertNotIn("s_twelveZodiacManager.LoadControls();", text)
        self.assertNotIn("WorldDatabase.Query(ZODIAC_CONTROL_SELECT_SQL)", text)
        self.assertIn('LOG_INFO("server.loading", "→十二生肖系统√', text)
        self.assertIn('LOG_ERROR("server.loading", "→十二生肖系统×', text)

    def test_online_players_are_refreshed_in_small_batches_after_async_reload(self):
        text = SOURCE.read_text(encoding="utf-8")
        world_start = text.index("class TwelveZodiacWorldScript")
        world_end = text.index("class TwelveZodiacPlayerScript", world_start)
        world_block = text[world_start:world_end]

        self.assertIn("constexpr uint32 ZODIAC_PLAYER_REFRESH_BATCH_SIZE = 5;", text)
        self.assertIn("void QueueOnlinePlayerRefresh();", text)
        self.assertIn("void ProcessPendingPlayerRefresh(uint32 maxPlayers);", text)
        self.assertIn("QueueOnlinePlayerRefresh();", text)
        self.assertIn("ObjectAccessor::FindConnectedPlayer(guid)", text)
        self.assertIn(
            "s_twelveZodiacManager.ProcessPendingPlayerRefresh(ZODIAC_PLAYER_REFRESH_BATCH_SIZE);",
            world_block,
        )
        self.assertNotIn("std::vector<Player*> onlinePlayers", text)

    def test_zodiac_damage_runs_before_legacy_absorption_and_magic_hit_stays_final(self):
        module_text = SOURCE.read_text(encoding="utf-8")
        unit_text = UNIT_SOURCE.read_text(encoding="utf-8")
        unit_script_header = UNIT_SCRIPT_HEADER.read_text(encoding="utf-8")
        unit_script_source = UNIT_SCRIPT_SOURCE.read_text(encoding="utf-8")
        script_mgr_header = SCRIPT_MGR_HEADER.read_text(encoding="utf-8")
        magic_hit_text = MAGIC_HIT_SOURCE.read_text(encoding="utf-8")

        self.assertIn("UNITHOOK_ON_BEFORE_DAMAGE_WITH_CONTEXT", unit_script_header)
        self.assertIn("virtual void OnBeforeDamageWithContext", unit_script_header)
        self.assertIn("void ScriptMgr::OnBeforeDamageWithContext", unit_script_source)
        self.assertIn("void OnBeforeDamageWithContext", script_mgr_header)

        before_index = unit_text.index("sScriptMgr->OnBeforeDamageWithContext")
        legacy_index = unit_text.index("sScriptMgr->OnDamage(attacker")
        final_index = unit_text.index("sScriptMgr->OnDamageWithContext")
        self.assertLess(before_index, legacy_index)
        self.assertLess(legacy_index, final_index)

        zodiac_start = module_text.index("class TwelveZodiacUnitScript")
        zodiac_end = module_text.index("class TwelveZodiacCommandScript", zodiac_start)
        zodiac_block = module_text[zodiac_start:zodiac_end]
        self.assertIn("UNITHOOK_ON_BEFORE_DAMAGE_WITH_CONTEXT", zodiac_block)
        self.assertIn("void OnBeforeDamageWithContext", zodiac_block)
        self.assertNotIn("UNITHOOK_ON_DAMAGE_WITH_CONTEXT", zodiac_block)

        self.assertIn("void OnDamageWithContext", magic_hit_text)
        self.assertNotIn("void OnBeforeDamageWithContext", magic_hit_text)

    def test_zodiac_damage_formula_is_sent_over_hermes_before_legacy_absorption(self):
        text = SOURCE.read_text(encoding="utf-8")
        apply_start = text.index("void TwelveZodiacManager::ApplyDamage")
        apply_end = text.index("void TwelveZodiacManager::ApplyHeal", apply_start)
        apply_block = text[apply_start:apply_end]

        self.assertIn('constexpr char const* ZODIAC_DAMAGE_ADDON_PREFIX = "ZODIACDMG";', text)
        self.assertIn("void SendZodiacDamagePayload", text)
        self.assertIn("HermesBridge_SendAddonMessage(player, ZODIAC_DAMAGE_ADDON_PREFIX", text)
        self.assertIn('payload << "HIT|"', text)
        self.assertIn("originalDamage", apply_block)
        self.assertIn("damage", apply_block)
        self.assertIn("effectiveMultiplierPercent", apply_block)
        self.assertIn("triggerApplied", apply_block)
        self.assertIn("afterTriad != afterIndividual", apply_block)
        self.assertIn("fullSet", apply_block)
        self.assertIn("spellInfo ? spellInfo->Id : 0", text)
        self.assertIn("triggerApplied, triadApplied, fullSet, spellInfo", apply_block)
        self.assertIn("SendZodiacDamagePayload", apply_block)

    def test_zodiac_multiplier_applies_to_every_player_damage_context(self):
        text = SOURCE.read_text(encoding="utf-8")
        script_start = text.index("class TwelveZodiacUnitScript")
        script_end = text.index("class TwelveZodiacCommandScript", script_start)
        script_block = text[script_start:script_end]
        hook_start = script_block.index("void OnBeforeDamageWithContext")
        hook_block = script_block[hook_start:]

        self.assertIn("UNITHOOK_ON_BEFORE_DAMAGE_WITH_CONTEXT", script_block)
        self.assertIn("s_twelveZodiacManager.ApplyDamage(player, victim, damage, damageType, spellInfo)", hook_block)
        self.assertNotIn("damageType == DIRECT_DAMAGE", hook_block)
        self.assertNotIn("damageType == SPELL_DIRECT_DAMAGE", hook_block)
        self.assertNotIn("spellInfo == nullptr", hook_block)


if __name__ == "__main__":
    unittest.main()

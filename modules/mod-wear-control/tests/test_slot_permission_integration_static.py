from pathlib import Path
import unittest


MODULES = Path(__file__).resolve().parents[2]
WEAR_HEADER = MODULES / "mod-wear-control" / "src" / "WearControl.h"
WEAR_SOURCE = MODULES / "mod-wear-control" / "src" / "WearControl.cpp"
WEAR_SQL = MODULES / "mod-wear-control" / "sql" / "characters" / "_穿戴等级权限.sql"
SYNTHESIS_SOURCE = MODULES / "mod-synthesis-system" / "src" / "mod_synthesis_system.cpp"
HERMES_SOURCE = MODULES / "mod-hermes-bridge" / "src" / "HermesBridgeHandler.cpp"


class SlotPermissionIntegrationStaticTest(unittest.TestCase):
    def test_permissions_are_keyed_by_system_and_slot(self):
        header = WEAR_HEADER.read_text(encoding="utf-8-sig")
        source = WEAR_SOURCE.read_text(encoding="utf-8-sig")
        schema = WEAR_SQL.read_text(encoding="utf-8-sig")

        self.assertIn("GetPlayerWearLevel(Player* player, uint8 limitType, uint8 slotPosition)", header)
        self.assertIn("UnlockPlayerWearLevel(Player* player, uint8 limitType, uint8 slotPosition, uint32 wearLevel", header)
        self.assertIn("`限制类型`, `槽位位置`, `穿戴等级`", source)
        self.assertIn("PRIMARY KEY (`玩家GUID`, `限制类型`, `槽位位置`)", schema)

    def test_synthesis_unlocks_slots_from_the_reward_item(self):
        source = SYNTHESIS_SOURCE.read_text(encoding="utf-8-sig")

        self.assertIn("entry.unlockItemId", source)
        self.assertIn("WearControl::GetItemWearSlots", source)
        self.assertIn("WearControl::UnlockPlayerWearLevel(player, limitType, slotPosition", source)

    def test_hermes_synthesis_uses_the_same_slot_unlock_path(self):
        source = HERMES_SOURCE.read_text(encoding="utf-8-sig")

        self.assertIn("`解锁穿戴等级`", source)
        self.assertIn("entry.UnlockWearLevel", source)
        self.assertIn("WearControl::GetItemWearSlots", source)
        self.assertIn("WearControl::UnlockPlayerWearLevel(&player, limitType, slotPosition", source)


if __name__ == "__main__":
    unittest.main()

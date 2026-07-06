from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "AscensionSystem.cpp"


class AscensionLogStaticTest(unittest.TestCase):
    def test_addon_command_logs_are_removed(self):
        source = SOURCE.read_text(encoding="utf-8-sig")

        self.assertNotIn("AscensionSystemUI addon command", source)


if __name__ == "__main__":
    unittest.main()

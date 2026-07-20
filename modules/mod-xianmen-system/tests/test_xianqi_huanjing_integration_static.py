from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "XianmenArtifactSlots.cpp"
BUILD = ROOT / "CMakeLists.txt"


class XianqiHuanJingIntegrationStaticTest(unittest.TestCase):
    def test_xianqi_slots_apply_and_remove_identification_attributes(self):
        source = SOURCE.read_text(encoding="utf-8-sig")
        build = BUILD.read_text(encoding="utf-8-sig")

        self.assertIn('include_directories(${CMAKE_SOURCE_DIR}/modules/mod-huanjinxitong/src)', build)
        self.assertIn('#if __has_include("HuanJingSystem.h")', source)
        self.assertEqual(source.count("sHuanJingSystem->ApplyHuanJingEnhancement("), 1)
        self.assertEqual(source.count("sHuanJingSystem->RemoveHuanJingEnhancement("), 3)


if __name__ == "__main__":
    unittest.main()

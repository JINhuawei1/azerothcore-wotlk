from __future__ import annotations

from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent
ITEM_SQL_PATH = BASE_DIR / "远古战袍衬衫_物品模板.sql"
QUEST_SQL_PATH = BASE_DIR / "远古战袍衬衫_任务框架.sql"

TABARD_START = 73001
TABARD_END = 74000
SHIRT_START = 74001
SHIRT_END = 75000
SHIRT_GEM_START = 75001
SHIRT_GEM_END = 76000

TABARD_QUEST_START = 70001
TABARD_QUEST_END = 71000
SHIRT_QUEST_START = 71001
SHIRT_QUEST_END = 72000

TABARD_NPC_ENTRY = 72001
SHIRT_NPC_ENTRY = 72002

TABARD_DISPLAY_ID = 20621
SHIRT_DISPLAY_ID = 3265
SHIRT_GEM_DISPLAY_ID = 6663
NPC_DISPLAY_ID = 1287

TABARD_REQUIRED_ITEM_ID = 60003
TABARD_REQUIRED_ITEM_COUNT = 5000
SHIRT_REQUIRED_ITEM_COUNT = 500

TABARD_STATS = [4, 3, 7, 5, 6, 31, 32, 36, 44, 47]
SHIRT_STATS = [4, 3, 7, 5, 6, 46, 43, 8, 9]


def q(value: str) -> str:
    return "'" + value.replace("\\", "\\\\").replace("'", "''") + "'"


def item_level_from_entry(entry: int, start: int) -> int:
    return entry - start + 1


def build_item_row(
    entry: int,
    start: int,
    name_prefix: str,
    inventory_type: int,
    display_id: int,
    stat_types: list[int],
) -> str:
    level = item_level_from_entry(entry, start)
    stat_pairs: list[str] = []

    for stat_type in stat_types:
        if stat_type in (43, 46):
            value = level * 5_000_000
        else:
            value = level * 10_000
        stat_pairs.append(f"{stat_type}, {value}")

    while len(stat_pairs) < 10:
        stat_pairs.append("0, 0")

    name = f"{name_prefix}{level}级"
    description = (
        f"{name}. 属性按等级成长: 基础属性与特殊伤害每级+10000, "
        f"生命/法力回复每级+5000000."
    )

    values = [
        str(entry),
        "4",
        "0",
        q(name),
        str(display_id),
        "5",
        "0",
        "0",
        "1",
        "0",
        "0",
        str(inventory_type),
        "-1",
        "-1",
        str(level),
        "1",
        "0",
        "1",
        str(len(stat_types)),
        ", ".join(stat_pairs),
        "1",
        q(description),
        "NULL",
    ]

    return "(" + ", ".join(values) + ")"


def build_shirt_gem_row(entry: int) -> str:
    level = item_level_from_entry(entry, SHIRT_GEM_START)
    name = f"衬衫宝石{level}"
    description = f"{name}. 用于远古衬衫{level}级任务, 每次任务需求10000个."

    values = [
        str(entry),
        "3",
        "7",
        q(name),
        str(SHIRT_GEM_DISPLAY_ID),
        "3",
        "0",
        "0",
        "1",
        "0",
        "0",
        "0",
        "-1",
        "-1",
        str(level),
        "1",
        "0",
        "20000",
        "0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0, 0",
        "0",
        q(description),
        "NULL",
    ]

    return "(" + ", ".join(values) + ")"


def generate_item_sql() -> str:
    header = """-- 自动生成文件, 请勿手改.
-- 生成脚本: modules/物品数据/generate_ancient_items.py

DELETE FROM `item_template` WHERE `entry` BETWEEN 73001 AND 76000;

REPLACE INTO `item_template`
(`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
 `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`,
 `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `StatsCount`,
 `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`,
 `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`, `stat_type6`, `stat_value6`,
 `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`,
 `stat_type10`, `stat_value10`, `bonding`, `description`, `VerifiedBuild`)
VALUES
"""

    rows: list[str] = []

    for entry in range(TABARD_START, TABARD_END + 1):
        rows.append(
            build_item_row(
                entry=entry,
                start=TABARD_START,
                name_prefix="远古战袍",
                inventory_type=19,
                display_id=TABARD_DISPLAY_ID,
                stat_types=TABARD_STATS,
            )
        )

    for entry in range(SHIRT_START, SHIRT_END + 1):
        rows.append(
            build_item_row(
                entry=entry,
                start=SHIRT_START,
                name_prefix="远古衬衫",
                inventory_type=4,
                display_id=SHIRT_DISPLAY_ID,
                stat_types=SHIRT_STATS,
            )
        )

    for entry in range(SHIRT_GEM_START, SHIRT_GEM_END + 1):
        rows.append(build_shirt_gem_row(entry))

    return header + ",\n".join(rows) + ";\n"


def build_creature_template_rows() -> str:
    rows = [
        (
            TABARD_NPC_ENTRY,
            "远古战袍使者",
            "战袍任务发布与交付",
        ),
        (
            SHIRT_NPC_ENTRY,
            "远古衬衫使者",
            "衬衫任务发布与交付",
        ),
    ]

    values = []
    for entry, name, subname in rows:
        values.append(
            "("
            + ", ".join(
                [
                    str(entry),
                    q(name),
                    q(subname),
                    q("Speak"),
                    "80",
                    "80",
                    "0",
                    "35",
                    "2",
                    "1",
                    "1.14286",
                    "1",
                    "1",
                    "20",
                    "1",
                    "0",
                    "0",
                    "1",
                    "2000",
                    "2000",
                    "1",
                    "1",
                    "1",
                    "6",
                    "1",
                    "1",
                    "1",
                    "1",
                    "1",
                    "NULL",
                ]
            )
            + ")"
        )

    return """REPLACE INTO `creature_template`
(`entry`, `name`, `subname`, `IconName`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`,
 `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
 `unit_class`, `type`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`,
 `ExperienceModifier`, `VerifiedBuild`)
VALUES
""" + ",\n".join(values) + ";\n"


def build_creature_model_rows() -> str:
    return f"""REPLACE INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
({TABARD_NPC_ENTRY}, 0, {NPC_DISPLAY_ID}, 1, 1, NULL),
({SHIRT_NPC_ENTRY}, 0, {NPC_DISPLAY_ID}, 1, 1, NULL);
"""


def build_creature_addon_rows() -> str:
    return f"""REPLACE INTO `creature_template_addon`
(`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
VALUES
({TABARD_NPC_ENTRY}, 0, 0, 0, 1, 0, 0, NULL),
({SHIRT_NPC_ENTRY}, 0, 0, 0, 1, 0, 0, NULL);
"""


def quest_title(prefix: str, level: int) -> str:
    return f"{prefix}{level}级任务"


def reward_item_entry(prefix: str, level: int) -> int:
    if prefix == "远古战袍":
        return TABARD_START + level - 1
    return SHIRT_START + level - 1


def required_item_entry(prefix: str, level: int) -> int:
    if prefix == "远古战袍":
        return TABARD_REQUIRED_ITEM_ID
    return SHIRT_GEM_START + level - 1


def required_item_count(prefix: str, level: int) -> int:
    if prefix == "远古战袍":
        return TABARD_REQUIRED_ITEM_COUNT
    return SHIRT_REQUIRED_ITEM_COUNT


def build_quest_template_row(
    quest_id: int,
    level: int,
    item_prefix: str,
    reward_next: int,
) -> str:
    reward_item = reward_item_entry(item_prefix, level)
    required_item = required_item_entry(item_prefix, level)
    required_count = required_item_count(item_prefix, level)
    title = quest_title(item_prefix, level)
    if item_prefix == "远古战袍":
        log_desc = f"收集 {required_count} 个物品60003, 领取{item_prefix}{level}级."
        quest_desc = (
            f"提交 {required_count} 个物品60003后, "
            f"可领取 {item_prefix}{level}级."
        )
    else:
        log_desc = f"收集 {required_count} 个衬衫宝石{level}, 领取{item_prefix}{level}级."
        quest_desc = (
            f"提交 {required_count} 个衬衫宝石{level}后, "
            f"可领取 {item_prefix}{level}级."
        )
    completion = f"完成任务: {title}"

    return (
        "("
        + ", ".join(
            [
                str(quest_id),
                "2",
                "1",
                "1",
                str(reward_next),
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                str(reward_item),
                "1",
                q(title),
                q(log_desc),
                q(quest_desc),
                q(item_prefix),
                q(completion),
                str(required_item),
                str(required_count),
                "NULL",
            ]
        )
        + ")"
    )


def build_quest_addon_row(quest_id: int, prev_id: int, next_id: int) -> str:
    return f"({quest_id}, 0, 0, 0, {prev_id}, {next_id}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)"


def build_quest_details_row(quest_id: int) -> str:
    return f"({quest_id}, 1, 0, 0, 0, 0, 0, 0, 0)"


def build_request_items_row(quest_id: int, item_prefix: str, level: int) -> str:
    required_count = required_item_count(item_prefix, level)
    if item_prefix == "远古战袍":
        text = f"你已经准备好 {required_count} 个物品60003, 可以领取{item_prefix}{level}级."
    else:
        text = f"你已经准备好 {required_count} 个衬衫宝石{level}, 可以领取{item_prefix}{level}级."
    return f"({quest_id}, 1, 1, {q(text)}, NULL)"


def build_offer_reward_row(quest_id: int, item_prefix: str, level: int) -> str:
    text = f"做得不错. 这是你的{item_prefix}{level}级."
    return f"({quest_id}, 1, 0, 0, 0, 0, 0, 0, 0, {q(text)}, NULL)"


def build_creature_quest_row(entry: int, quest_id: int) -> str:
    return f"({entry}, {quest_id})"


def generate_quest_sql() -> str:
    header = f"""-- 自动生成文件, 请勿手改.
-- 生成脚本: modules/物品数据/generate_ancient_items.py

DELETE FROM `creature_queststarter` WHERE `quest` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};
DELETE FROM `creature_questender` WHERE `quest` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};
DELETE FROM `quest_offer_reward` WHERE `ID` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};
DELETE FROM `quest_request_items` WHERE `ID` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};
DELETE FROM `quest_details` WHERE `ID` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};
DELETE FROM `quest_template_addon` WHERE `ID` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};
DELETE FROM `quest_template` WHERE `ID` BETWEEN {TABARD_QUEST_START} AND {SHIRT_QUEST_END};

DELETE FROM `creature_template_model` WHERE `CreatureID` IN ({TABARD_NPC_ENTRY}, {SHIRT_NPC_ENTRY});
DELETE FROM `creature_template_addon` WHERE `entry` IN ({TABARD_NPC_ENTRY}, {SHIRT_NPC_ENTRY});
DELETE FROM `creature_template` WHERE `entry` IN ({TABARD_NPC_ENTRY}, {SHIRT_NPC_ENTRY});

"""

    sections = [
        build_creature_template_rows(),
        build_creature_model_rows(),
        build_creature_addon_rows(),
    ]

    quest_rows: list[str] = []
    addon_rows: list[str] = []
    detail_rows: list[str] = []
    request_rows: list[str] = []
    reward_rows: list[str] = []
    starter_rows: list[str] = []
    ender_rows: list[str] = []

    for prefix, start, end, npc_entry in (
        ("远古战袍", TABARD_QUEST_START, TABARD_QUEST_END, TABARD_NPC_ENTRY),
        ("远古衬衫", SHIRT_QUEST_START, SHIRT_QUEST_END, SHIRT_NPC_ENTRY),
    ):
        for quest_id in range(start, end + 1):
            level = quest_id - start + 1
            prev_id = quest_id - 1 if quest_id > start else 0
            next_id = quest_id + 1 if quest_id < end else 0

            quest_rows.append(build_quest_template_row(quest_id, level, prefix, next_id))
            addon_rows.append(build_quest_addon_row(quest_id, prev_id, next_id))
            detail_rows.append(build_quest_details_row(quest_id))
            request_rows.append(build_request_items_row(quest_id, prefix, level))
            reward_rows.append(build_offer_reward_row(quest_id, prefix, level))
            starter_rows.append(build_creature_quest_row(npc_entry, quest_id))
            ender_rows.append(build_creature_quest_row(npc_entry, quest_id))

    sections.append(
        """REPLACE INTO `quest_template`
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `RewardNextQuest`, `RewardXPDifficulty`,
 `RewardMoney`, `RewardMoneyDifficulty`, `RewardDisplaySpell`, `RewardSpell`, `RewardHonor`,
 `RewardKillHonor`, `Flags`, `RewardItem1`, `RewardAmount1`, `LogTitle`, `LogDescription`,
 `QuestDescription`, `AreaDescription`, `QuestCompletionLog`, `RequiredItemId1`, `RequiredItemCount1`,
 `VerifiedBuild`)
VALUES
"""
        + ",\n".join(quest_rows)
        + ";\n"
    )

    sections.append(
        """REPLACE INTO `quest_template_addon`
(`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`,
 `ExclusiveGroup`, `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`,
 `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`,
 `ProvidedItemCount`, `SpecialFlags`)
VALUES
"""
        + ",\n".join(addon_rows)
        + ";\n"
    )

    sections.append(
        """REPLACE INTO `quest_details`
(`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`)
VALUES
"""
        + ",\n".join(detail_rows)
        + ";\n"
    )

    sections.append(
        """REPLACE INTO `quest_request_items`
(`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`, `VerifiedBuild`)
VALUES
"""
        + ",\n".join(request_rows)
        + ";\n"
    )

    sections.append(
        """REPLACE INTO `quest_offer_reward`
(`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `RewardText`, `VerifiedBuild`)
VALUES
"""
        + ",\n".join(reward_rows)
        + ";\n"
    )

    sections.append(
        "REPLACE INTO `creature_queststarter` (`id`, `quest`) VALUES\n"
        + ",\n".join(starter_rows)
        + ";\n"
    )

    sections.append(
        "REPLACE INTO `creature_questender` (`id`, `quest`) VALUES\n"
        + ",\n".join(ender_rows)
        + ";\n"
    )

    return header + "\n".join(sections)


def main() -> None:
    item_sql = generate_item_sql()
    quest_sql = generate_quest_sql()

    ITEM_SQL_PATH.write_text(item_sql, encoding="utf-8", newline="\n")
    QUEST_SQL_PATH.write_text(quest_sql, encoding="utf-8", newline="\n")

    print(f"已生成: {ITEM_SQL_PATH}")
    print(f"已生成: {QUEST_SQL_PATH}")
    print("战袍数量: 1000")
    print("衬衫数量: 1000")
    print("衬衫宝石数量: 1000")
    print("任务数量: 2000")
    print("NPC模板数量: 2")


if __name__ == "__main__":
    main()

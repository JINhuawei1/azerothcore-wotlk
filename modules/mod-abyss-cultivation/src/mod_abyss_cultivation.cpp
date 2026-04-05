#include "ScriptMgr.h"

#include "Chat.h"
#include "Configuration/Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Logging/Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "AllCreatureScript.h"
#include "ScriptedCreature.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <initializer_list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
constexpr char ABYSS_ADDON_PREFIX[] = "ABYSS_UI";
constexpr size_t ABYSS_MAX_ADDON_PAYLOAD = 220;

bool IsModuleEnabled()
{
    return sConfigMgr->GetOption<bool>("AbyssCultivation.Enable", false);
}

bool IsDebugEnabled()
{
    return sConfigMgr->GetOption<bool>("AbyssCultivation.Debug", false);
}

bool IsAutoBeginOnMapEnterEnabled()
{
    return sConfigMgr->GetOption<bool>("AbyssCultivation.AutoBeginOnMapEnter", false);
}

uint32 GetNow()
{
    return static_cast<uint32>(std::time(nullptr));
}

float RollPercentage()
{
    static bool seeded = false;
    if (!seeded)
    {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }

    return static_cast<float>(std::rand() % 10000) / 100.0f;
}

uint32 GetRandomSeed()
{
    RollPercentage();
    return static_cast<uint32>(std::rand());
}

uint32 RollWeight(uint32 totalWeight)
{
    if (totalWeight == 0)
        return 0;

    RollPercentage();
    return static_cast<uint32>(std::rand()) % totalWeight;
}

char const* GetTaskTypeName(uint8 taskType)
{
    switch (taskType)
    {
        case 1: return "start";
        case 2: return "official";
        case 3: return "story";
        case 4: return "abyss";
        case 5: return "corruption";
        default: return "unknown";
    }
}

char const* GetBossTypeName(uint8 bossType)
{
    switch (bossType)
    {
        case 2: return "abyss";
        case 3: return "cache";
        default: return "unknown";
    }
}

char const* GetItemDockingTypeName(uint8 dockingType)
{
    switch (dockingType)
    {
        case 1: return "chapter_relic";
        case 2: return "phase_artifact";
        case 3: return "ultimate_artifact";
        case 4: return "material";
        case 5: return "unique_equip";
        default: return "unknown";
    }
}

char const* GetQuestStatusName(QuestStatus status)
{
    switch (status)
    {
        case QUEST_STATUS_NONE: return "none";
        case QUEST_STATUS_COMPLETE: return "complete";
        case QUEST_STATUS_INCOMPLETE: return "incomplete";
        case QUEST_STATUS_FAILED: return "failed";
        case QUEST_STATUS_REWARDED: return "rewarded";
        default: return "unknown";
    }
}

char const* GetModeTypeName(uint8 modeType)
{
    switch (modeType)
    {
        case 1: return "story";
        case 2: return "abyss";
        case 3: return "corruption";
        case 4: return "reincarnation";
        default: return "unknown";
    }
}

uint32 GetModeTypeMask(uint8 modeType)
{
    if (modeType == 0 || modeType > 31)
        return 0;

    return 1u << (modeType - 1);
}

bool IsAdvancedModeType(uint8 modeType)
{
    return modeType >= 2 && modeType <= 4;
}

enum AbyssRelicSlot : uint8
{
    ABYSS_RELIC_SLOT_MAIN = 1,
    ABYSS_RELIC_SLOT_SUB_1 = 2,
    ABYSS_RELIC_SLOT_SUB_2 = 3,
    ABYSS_RELIC_SLOT_SUB_3 = 4,
    ABYSS_RELIC_SLOT_SUB_4 = 5,
    ABYSS_RELIC_SLOT_SUB_5 = 6,
    ABYSS_RELIC_SLOT_PHASE = 7,
    ABYSS_RELIC_SLOT_ULTIMATE = 8
};

char const* GetRelicSlotName(uint8 slot)
{
    switch (slot)
    {
        case ABYSS_RELIC_SLOT_MAIN: return "主遗物";
        case ABYSS_RELIC_SLOT_SUB_1: return "副遗物1";
        case ABYSS_RELIC_SLOT_SUB_2: return "副遗物2";
        case ABYSS_RELIC_SLOT_SUB_3: return "副遗物3";
        case ABYSS_RELIC_SLOT_SUB_4: return "副遗物4";
        case ABYSS_RELIC_SLOT_SUB_5: return "副遗物5";
        case ABYSS_RELIC_SLOT_PHASE: return "阶段神器";
        case ABYSS_RELIC_SLOT_ULTIMATE: return "终极神器";
        default: return "未知";
    }
}

std::string NormalizeCommandToken(std::string token)
{
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });

    return token;
}

bool IsCurrentScopeToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token == "current" || token == "当前" || token == "当前章";
}

bool IsNextScopeToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token == "next" || token == "下一" || token == "下一章";
}

bool IsAllScopeToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token == "all" || token == "全部";
}

bool IsRelicStateActionToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token.empty() || token == "state" || token == "状态";
}

bool IsRelicListActionToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token == "list" || token == "列表";
}

bool IsRelicSetActionToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token == "set" || token == "设置";
}

bool IsRelicClearActionToken(std::string token)
{
    token = NormalizeCommandToken(std::move(token));
    return token == "clear" || token == "清空";
}

bool TryParseRelicSlotToken(std::string token, uint8& slot)
{
    if (token.empty())
        return false;

    token = NormalizeCommandToken(std::move(token));

    if (token == "1" || token == "main" || token == "primary" || token == "主" || token == "主遗物")
    {
        slot = ABYSS_RELIC_SLOT_MAIN;
        return true;
    }

    if (token == "2" || token == "sub1" || token == "secondary1" || token == "副1" || token == "副遗物1")
    {
        slot = ABYSS_RELIC_SLOT_SUB_1;
        return true;
    }

    if (token == "3" || token == "sub2" || token == "secondary2" || token == "副2" || token == "副遗物2")
    {
        slot = ABYSS_RELIC_SLOT_SUB_2;
        return true;
    }

    if (token == "4" || token == "sub3" || token == "secondary3" || token == "副3" || token == "副遗物3")
    {
        slot = ABYSS_RELIC_SLOT_SUB_3;
        return true;
    }

    if (token == "5" || token == "sub4" || token == "secondary4" || token == "副4" || token == "副遗物4")
    {
        slot = ABYSS_RELIC_SLOT_SUB_4;
        return true;
    }

    if (token == "6" || token == "sub5" || token == "secondary5" || token == "副5" || token == "副遗物5")
    {
        slot = ABYSS_RELIC_SLOT_SUB_5;
        return true;
    }

    if (token == "7" || token == "phase" || token == "artifact" || token == "阶段" || token == "阶段神器")
    {
        slot = ABYSS_RELIC_SLOT_PHASE;
        return true;
    }

    if (token == "8" || token == "ultimate" || token == "终极" || token == "终极神器")
    {
        slot = ABYSS_RELIC_SLOT_ULTIMATE;
        return true;
    }

    return false;
}

float ClampRelicScale(float scale)
{
    if (scale <= 0.0f)
        return 0.5f;

    return std::max(0.1f, std::min(scale, 1.0f));
}

uint32 ScaleUIntValue(uint32 value, float scale)
{
    if (value == 0 || scale <= 0.0f)
        return 0;

    return std::max<uint32>(1u, static_cast<uint32>(std::lround(static_cast<double>(value) * scale)));
}

int32 ScaleIntValue(int32 value, float scale)
{
    if (value == 0 || scale <= 0.0f)
        return 0;

    return static_cast<int32>(std::lround(static_cast<double>(value) * scale));
}

std::string EscapeSqlString(std::string const& value)
{
    std::string result;
    result.reserve(value.size() + 8);

    for (char ch : value)
    {
        switch (ch)
        {
            case '\\':
                result += "\\\\";
                break;
            case '\'':
                result += "''";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\n':
                result += "\\n";
                break;
            default:
                result += ch;
                break;
        }
    }

    return result;
}

std::string SanitizeAddonText(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '|' || ch == '^' || ch == '~' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = '/';
    }

    return value;
}

std::string BuildAddonIconPath(char const* inventoryIcon)
{
    if (!inventoryIcon || !*inventoryIcon)
        return "Interface\\Icons\\INV_Misc_QuestionMark";

    std::string icon(inventoryIcon);
    if (icon.find("Interface\\") == 0 || icon.find("interface\\") == 0)
        return icon;

    return "Interface\\Icons\\" + icon;
}

std::string GetItemIconPathForAddon(uint32 itemId, uint32 fallbackDisplayId = 0)
{
    uint32 displayId = fallbackDisplayId;

    if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId))
        displayId = itemTemplate->DisplayInfoID;

    if (displayId == 0)
        return "Interface\\Icons\\INV_Misc_QuestionMark";

    if (ItemDisplayInfoEntry const* displayInfo = sItemDisplayInfoStore.LookupEntry(displayId))
        return BuildAddonIconPath(displayInfo->inventoryIcon);

    return "Interface\\Icons\\INV_Misc_QuestionMark";
}

std::vector<std::string> SplitCsvTokens(std::string const& value)
{
    std::vector<std::string> result;
    std::string token;
    std::istringstream stream(value);
    while (std::getline(stream, token, ','))
    {
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch)
        {
            return std::isspace(ch) != 0;
        }), token.end());

        if (!token.empty())
            result.push_back(token);
    }

    return result;
}

char const* const kLoadChaptersSql =
    "SELECT * FROM `_\xE6\xB7\xB1\xE6\xB8\x8A\xE7\xAB\xA0\xE8\x8A\x82\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE7\xAB\xA0\xE8\x8A\x82ID`";

char const* const kLoadPlayerSql =
    "SELECT `当前章节`, `历史最高章节`, `当前修仙门槛等级`, `最高腐化层`, `已解锁模式掩码`, `剧情状态`, "
    "`主遗物`, `副遗物1`, `副遗物2`, `副遗物3`, `副遗物4`, `副遗物5`, `阶段神器`, `终极神器`, "
    "`预设编号`, `当前保底章节ID`, `深渊装失败次数`, `秘藏首领失败次数`, `最后更新时间` "
    "FROM `_玩家深渊主数据` WHERE `角色ID` = {}";

char const* const kInsertPlayerSql =
    "INSERT IGNORE INTO `_\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE4\xB8\xBB\xE6\x95\xB0\xE6\x8D\xAE` (`\xE8\xA7\x92\xE8\x89\xB2ID`) VALUES ({})";

char const* const kSavePlayerSql =
    "REPLACE INTO `_玩家深渊主数据` "
    "(`角色ID`, `当前章节`, `历史最高章节`, `当前修仙门槛等级`, `最高腐化层`, `已解锁模式掩码`, `剧情状态`, "
    "`主遗物`, `副遗物1`, `副遗物2`, `副遗物3`, `副遗物4`, `副遗物5`, `阶段神器`, `终极神器`, "
    "`预设编号`, `当前保底章节ID`, `深渊装失败次数`, `秘藏首领失败次数`, `最后更新时间`) "
    "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})";

char const* const kDeletePlayerSql =
    "DELETE FROM `_\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE4\xB8\xBB\xE6\x95\xB0\xE6\x8D\xAE` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

char const* const kLoadPlayerChapterModeSql =
    "SELECT `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE5\xB7\xB2\xE8\xA7\xA3\xE9\x94\x81\xE6\xA8\xA1\xE5\xBC\x8F\xE6\x8E\xA9\xE7\xA0\x81` FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE7\xAB\xA0\xE8\x8A\x82\xE6\xA8\xA1\xE5\xBC\x8F` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {} ORDER BY `\xE7\xAB\xA0\xE8\x8A\x82ID`";

char const* const kSavePlayerChapterModeSql =
    "REPLACE INTO `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE7\xAB\xA0\xE8\x8A\x82\xE6\xA8\xA1\xE5\xBC\x8F` (`\xE8\xA7\x92\xE8\x89\xB2ID`, `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE5\xB7\xB2\xE8\xA7\xA3\xE9\x94\x81\xE6\xA8\xA1\xE5\xBC\x8F\xE6\x8E\xA9\xE7\xA0\x81`, `\xE6\x9C\x80\xE5\x90\x8E\xE6\x9B\xB4\xE6\x96\xB0\xE6\x97\xB6\xE9\x97\xB4`) VALUES ({}, {}, {}, {})";

char const* const kDeletePlayerChapterModeSql =
    "DELETE FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE7\xAB\xA0\xE8\x8A\x82\xE6\xA8\xA1\xE5\xBC\x8F` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

char const* const kLoadPlayerRunStateSql =
    "SELECT `角色ID`, `当前副本地图ID`, `当前章节ID`, `模式类型`, `腐化层`, `本局主遗物`, `本局副遗物1`, `本局副遗物2`, "
    "`本局副遗物3`, `本局副遗物4`, `本局副遗物5`, `锚点首领击杀掩码`, `是否已召唤深渊首领`, `是否已召唤秘藏首领`, `开局时间` "
    "FROM `_玩家深渊局内状态` WHERE `角色ID` = {}";

char const* const kSavePlayerRunStateSql =
    "REPLACE INTO `_玩家深渊局内状态` "
    "(`角色ID`, `当前副本地图ID`, `当前章节ID`, `模式类型`, `腐化层`, `本局主遗物`, `本局副遗物1`, `本局副遗物2`, `本局副遗物3`, `本局副遗物4`, `本局副遗物5`, `锚点首领击杀掩码`, `是否已召唤深渊首领`, `是否已召唤秘藏首领`, `开局时间`) "
    "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})";

char const* const kDeletePlayerRunStateSql =
    "DELETE FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE5\xB1\x80\xE5\x86\x85\xE7\x8A\xB6\xE6\x80\x81` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

char const* const kDeletePlayerCollectionSql =
    "DELETE FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE6\x94\xB6\xE8\x97\x8F` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {}";

char const* const kLoadPlayerCollectionSql =
    "SELECT `\xE6\x94\xB6\xE8\x97\x8F\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE7\x89\xA9\xE5\x93\x81ID`, `\xE5\x85\xB3\xE8\x81\x94\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE7\xAD\x89\xE7\xBA\xA7`, `\xE8\xA7\x89\xE9\x86\x92\xE5\xB1\x82\xE6\x95\xB0`, `\xE6\x9D\xA5\xE6\xBA\x90\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE8\xA7\xA3\xE9\x94\x81\xE6\x97\xB6\xE9\x97\xB4` FROM `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE6\x94\xB6\xE8\x97\x8F` WHERE `\xE8\xA7\x92\xE8\x89\xB2ID` = {} ORDER BY `\xE6\x94\xB6\xE8\x97\x8F\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE5\x85\xB3\xE8\x81\x94\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE7\x89\xA9\xE5\x93\x81ID`";

char const* const kInsertPlayerRelicCollectionSql =
    "INSERT IGNORE INTO `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE6\x94\xB6\xE8\x97\x8F` VALUES ({}, 1, {}, {}, 1, 0, 1, {})";

char const* const kInsertPlayerCollectionSql =
    "INSERT IGNORE INTO `\x5F\xE7\x8E\xA9\xE5\xAE\xB6\xE6\xB7\xB1\xE6\xB8\x8A\xE6\x94\xB6\xE8\x97\x8F` VALUES ({}, {}, {}, {}, {}, {}, {}, {})";

char const* const kLoadTaskDockingsSql =
    "SELECT `\xE4\xBB\xBB\xE5\x8A\xA1ID`, `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE4\xBB\xBB\xE5\x8A\xA1\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE4\xBB\xBB\xE5\x8A\xA1\xE6\xA0\x87\xE9\xA2\x98`, `\xE4\xB8\x8A\xE4\xB8\x80\xE4\xBB\xBB\xE5\x8A\xA1ID`, `\xE4\xB8\x8B\xE4\xB8\x80\xE4\xBB\xBB\xE5\x8A\xA1ID`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE4\xBB\xBB\xE5\x8A\xA1\xE7\xAD\x89\xE7\xBA\xA7`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE6\x9C\x80\xE5\xB0\x8F\xE7\xAD\x89\xE7\xBA\xA7`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE4\xBB\xBB\xE5\x8A\xA1\xE5\x88\x86\xE7\xB1\xBB`, `\xE4\xBB\xBB\xE5\x8A\xA1\xE8\xAF\xB4\xE6\x98\x8E`, `\xE5\xAF\xB9\xE6\x8E\xA5\xE7\x8A\xB6\xE6\x80\x81` FROM `_\xE6\xB7\xB1\xE6\xB8\x8A\xE4\xBB\xBB\xE5\x8A\xA1\xE6\xA8\xA1\xE6\x9D\xBF\xE5\xAF\xB9\xE6\x8E\xA5` ORDER BY `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE4\xBB\xBB\xE5\x8A\xA1\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE4\xBB\xBB\xE5\x8A\xA1ID`";

char const* const kLoadBossDockingsSql =
    "SELECT `\xE9\xA6\x96\xE9\xA2\x86\xE5\x85\xA5\xE5\x8F\xA3`, `\xE9\xA6\x96\xE9\xA2\x86\xE5\x90\x8D\xE7\xA7\xB0`, `\xE9\xA6\x96\xE9\xA2\x86\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE5\xB9\x95ID`, `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE7\xAD\x89\xE7\xBA\xA7`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE9\x98\xB5\xE8\x90\xA5`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE6\xA8\xA1\xE5\x9E\x8B\xE7\xBB\x84`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE8\x84\x9A\xE6\x9C\xAC\xE5\x90\x8D`, `\xE6\x98\xAF\xE5\x90\xA6\xE5\x86\x99\xE5\x85\xA5\xE7\x94\x9F\xE7\x89\xA9\xE6\xA8\xA1\xE6\x9D\xBF`, `\xE6\x98\xAF\xE5\x90\xA6\xE5\x86\x99\xE5\x85\xA5\xE6\x8E\x89\xE8\x90\xBD\xE6\xA8\xA1\xE6\x9D\xBF`, `\xE5\xAF\xB9\xE6\x8E\xA5\xE7\x8A\xB6\xE6\x80\x81`, `\xE8\xAF\xB4\xE6\x98\x8E` FROM `_\xE6\xB7\xB1\xE6\xB8\x8A\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\xE9\xA6\x96\xE9\xA2\x86\xE5\xAF\xB9\xE6\x8E\xA5` ORDER BY `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE9\xA6\x96\xE9\xA2\x86\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE9\xA6\x96\xE9\xA2\x86\xE5\x85\xA5\xE5\x8F\xA3`";

char const* const kLoadItemDockingsSql =
    "SELECT `\xE7\x89\xA9\xE5\x93\x81ID`, `\xE7\x89\xA9\xE5\x93\x81\xE5\x90\x8D\xE7\xA7\xB0`, `\xE5\xAF\xB9\xE6\x8E\xA5\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE5\xB9\x95ID`, `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE7\x89\xA9\xE5\x93\x81\xE5\x93\x81\xE8\xB4\xA8`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE7\x89\xA9\xE5\x93\x81\xE5\x88\x86\xE7\xB1\xBB`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE7\x89\xA9\xE5\x93\x81\xE5\xAD\x90\xE7\xB1\xBB`, `\xE5\xBB\xBA\xE8\xAE\xAE\xE8\xA3\x85\xE5\xA4\x87\xE6\xA7\xBD\xE4\xBD\x8D`, `\xE6\x98\xAF\xE5\x90\xA6\xE5\x94\xAF\xE4\xB8\x80`, `\xE6\x98\xAF\xE5\x90\xA6\xE8\x83\x8C\xE5\x8C\x85\xE6\xBF\x80\xE6\xB4\xBB`, `\xE9\xA2\x84\xE7\x95\x99\xE6\x98\xBE\xE7\xA4\xBAID`, `\xE5\xAF\xB9\xE6\x8E\xA5\xE7\x8A\xB6\xE6\x80\x81`, `\xE8\xAF\xB4\xE6\x98\x8E` FROM `_\xE6\xB7\xB1\xE6\xB8\x8A\xE7\x89\xA9\xE5\x93\x81\xE6\xA8\xA1\xE6\x9D\xBF\xE5\xAF\xB9\xE6\x8E\xA5` ORDER BY `\xE7\xAB\xA0\xE8\x8A\x82ID`, `\xE5\xAF\xB9\xE6\x8E\xA5\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE7\x89\xA9\xE5\x93\x81ID`";

char const* const kLoadRelicsSql =
    "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE9\x81\x97\xE7\x89\xA9\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE7\x89\xA9\xE5\x93\x81ID`";

char const* const kLoadBossConfigsSql =
    "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE9\xA6\x96\xE9\xA2\x86\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE9\xA6\x96\xE9\xA2\x86\xE5\x85\xA5\xE5\x8F\xA3`";

char const* const kLoadEquipmentTemplatesSql =
    "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE8\xA3\x85\xE5\xA4\x87\xE6\xA8\xA1\xE6\x9D\xBF` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE6\x9D\xA5\xE6\xBA\x90\xE7\xAB\xA0\xE8\x8A\x82`, `\xE6\x9D\xA5\xE6\xBA\x90\xE6\xA8\xA1\xE5\xBC\x8F`, `\xE7\x89\xA9\xE5\x93\x81\xE6\xA8\xA1\xE6\x9D\xBFID`";

char const* const kLoadAffixTemplatesSql =
    "SELECT `\xE8\xAF\x8D\xE7\xBC\x80ID`, `\xE8\xAF\x8D\xE7\xBC\x80\xE5\x90\x8D\xE7\xA7\xB0`, `\xE8\xAF\x8D\xE7\xBC\x80\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE8\xAF\x8D\xE7\xBC\x80\xE7\xBB\x84`, `\xE6\x9C\x80\xE5\xB0\x8F\xE7\xAB\xA0\xE8\x8A\x82`, `\xE6\x9C\x80\xE5\xA4\xA7\xE7\xAB\xA0\xE8\x8A\x82`, `\xE9\x83\xA8\xE4\xBD\x8D\xE6\x8E\xA9\xE7\xA0\x81`, `\xE6\x9C\x80\xE4\xBD\x8E\xE5\x93\x81\xE8\xB4\xA8`, `\xE6\x95\xB0\xE5\x80\xBC\xE5\x85\xAC\xE5\xBC\x8F`, `\xE8\x84\x9A\xE6\x9C\xAC\xE7\xBB\x84`, `\xE6\x8F\x8F\xE8\xBF\xB0`, `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE8\xAF\x8D\xE7\xBC\x80\xE6\xA8\xA1\xE6\x9D\xBF` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE8\xAF\x8D\xE7\xBC\x80ID`";

char const* const kLoadSpecialEffectTemplatesSql =
    "SELECT `\xE7\x89\xB9\xE6\x95\x88ID`, `\xE7\x89\xB9\xE6\x95\x88\xE5\x90\x8D\xE7\xA7\xB0`, `\xE8\xA7\xA6\xE5\x8F\x91\xE7\xB1\xBB\xE5\x9E\x8B`, `\xE8\xA7\xA6\xE5\x8F\x91\xE5\x8F\x82\xE6\x95\xB0" "1`, `\xE8\xA7\xA6\xE5\x8F\x91\xE5\x8F\x82\xE6\x95\xB0" "2`, `\xE5\x86\xB7\xE5\x8D\xB4\xE6\xAF\xAB\xE7\xA7\x92`, `\xE6\xAF\x8F\xE5\x88\x86\xE9\x92\x9F\xE8\xA7\xA6\xE5\x8F\x91\xE7\x8E\x87`, `\xE7\x89\xB9\xE6\x95\x88\xE5\xAE\xB6\xE6\x97\x8F`, `\xE6\x95\xB0\xE5\x80\xBC\xE5\x85\xAC\xE5\xBC\x8F`, `\xE5\x85\x81\xE8\xAE\xB8\xE9\x83\xA8\xE4\xBD\x8D\xE6\x8E\xA9\xE7\xA0\x81`, `\xE8\x84\x9A\xE6\x9C\xAC\xE7\xBB\x84`, `\xE6\x8F\x8F\xE8\xBF\xB0`, `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE7\x89\xB9\xE6\x95\x88\xE6\xA8\xA1\xE6\x9D\xBF` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE7\x89\xB9\xE6\x95\x88ID`";

// _深渊套装配置
char const* const kLoadSetBonusConfigsSql =
    "SELECT * FROM `\x5F\xE6\xB7\xB1\xE6\xB8\x8A\xE5\xA5\x97\xE8\xA3\x85\xE9\x85\x8D\xE7\xBD\xAE` WHERE `\xE6\x98\xAF\xE5\x90\xA6\xE5\x90\xAF\xE7\x94\xA8` = 1 ORDER BY `\xE5\xA5\x97\xE8\xA3\x85ID`";

struct AbyssChapterConfig
{
    uint16 chapterId = 0;
    uint8 actId = 0;
    std::string chapterName;
    uint16 mapId = 0;
    uint8 chapterType = 0;
    uint16 requiredCultivationLevel = 0;
    uint16 prerequisiteChapterId = 0;
    uint32 startQuestId = 0;
    uint32 completeQuestId = 0;
    uint32 relicItemId = 0;
    uint32 lootGroupId = 0;
    uint32 anchorBossEntry = 0;
    uint32 finalBossEntry = 0;
    uint8 triggerType = 0;
    uint64 requiredBossKillMask = 0;
    uint8 firstClearSummonRule = 0;
    uint8 repeatSummonRule = 0;
    uint32 abyssBossEntry = 0;
    uint16 abyssSummonMapId = 0;
    float abyssSummonX = 0.0f;
    float abyssSummonY = 0.0f;
    float abyssSummonZ = 0.0f;
    float abyssSummonO = 0.0f;
    uint32 cacheBossEntry = 0;
    float cacheBossBaseChance = 0.0f;
    uint16 cacheBossPityCount = 0;
    uint32 preferredSlotMask = 0;
    std::string commonBaseMaterialPackage;
    float storyDropRate = 0.0f;
    float abyssDropRate = 0.0f;
    float corruptionDropRate = 0.0f;
    float reincarnationDropRate = 0.0f;
    float hiddenRoomBonus = 0.0f;
    float cacheBossRewardBonus = 0.0f;
    uint16 abyssGearPityCount = 0;
    bool quickFarmUnlocked = false;
    bool enabled = false;
};

struct AbyssRelicConfig
{
    uint32 itemId = 0;
    std::string name;
    uint8 relicType = 0;
    uint8 actId = 0;
    uint16 relatedChapterId = 0;
    uint8 activeSlot = 0;
    uint8 activeRule = 0;
    uint32 exclusiveGroup = 0;
    std::string effectFamily;
    uint8 recommendedSlot = 0;
    std::string scriptGroup;
    float subSlotScale = 0.0f;
    std::string briefDescription;
    std::string fullDescription;
    uint8 agilityWeight = 0;
    uint8 strengthWeight = 0;
    uint8 intellectWeight = 0;
    uint8 spiritWeight = 0;
    uint8 staminaWeight = 0;
    uint8 hitRatingWeight = 0;
    uint8 critRatingWeight = 0;
    uint8 hasteRatingWeight = 0;
    uint8 attackPowerWeight = 0;
    uint8 spellPowerWeight = 0;
};

struct AbyssBossConfig
{
    uint32 bossEntry = 0;
    std::string bossName;
    uint8 bossType = 0;
    uint8 actId = 0;
    uint16 chapterId = 0;
    float healthModifier = 1.0f;
    float damageModifier = 1.0f;
    float storyModifier = 1.0f;
    float abyssModifier = 1.0f;
    float corruptionModifier = 1.0f;
    float reincarnationModifier = 1.0f;
    uint8 phase1HealthPct = 100;
    std::string phase1SkillGroup;
    uint8 phase2HealthPct = 0;
    std::string phase2SkillGroup;
    uint8 phase3HealthPct = 0;
    std::string phase3SkillGroup;
    std::string areaEffect;
    uint32 lootPackageId = 0;
    std::string introText;
    std::string deathText;
};

struct AbyssEquipmentTemplate
{
    uint32 templateId = 0;
    uint32 itemId = 0;
    std::string itemName;
    uint8 equipmentType = 0;
    uint16 sourceChapter = 0;
    uint8 sourceMode = 0;
    uint32 slotMask = 0;
    uint8 actId = 0;
    uint16 baseItemLevel = 0;
    uint8 armorType = 0;
    uint8 damageType = 0;
    uint16 minPrimaryBudget = 0;
    uint16 maxPrimaryBudget = 0;
    uint16 minSecondaryBudget = 0;
    uint16 maxSecondaryBudget = 0;
    uint16 minEffectBudget = 0;
    uint16 maxEffectBudget = 0;
    std::string fixedAffixGroup;
    uint32 fixedEffectId = 0;
    bool fromCacheBoss = false;
    bool requiresFragments = false;
    std::string flavorText;
    uint32 setId = 0;
    bool enabled = false;
};

struct AbyssAffixTemplate
{
    uint32 affixId = 0;
    std::string affixName;
    uint8 affixType = 0;
    uint32 affixGroup = 0;
    uint16 minChapter = 0;
    uint16 maxChapter = 0;
    uint32 slotMask = 0;
    uint8 minQuality = 0;
    std::string formula;
    std::string scriptGroup;
    std::string description;
};

struct AbyssSpecialEffectTemplate
{
    uint32 effectId = 0;
    std::string effectName;
    uint8 triggerType = 0;
    uint32 triggerParam1 = 0;
    uint32 triggerParam2 = 0;
    uint32 cooldownMs = 0;
    float ppmRate = 0.0f;
    std::string effectFamily;
    std::string formula;
    uint32 allowedSlotMask = 0;
    std::string scriptGroup;
    std::string description;
};

// 套装配置
struct AbyssSetBonusConfig
{
    uint32 setId = 0;
    std::string setName;
    uint8 actId = 0;
    uint8 sourceMode = 0;
    // 2件效果
    uint16 twoPieceAllStat = 0;
    uint16 twoPieceCrit = 0;
    uint16 twoPieceHaste = 0;
    uint16 twoPieceAP = 0;
    uint16 twoPieceSP = 0;
    std::string twoPieceDesc;
    // 4件效果
    uint8  fourPieceDmgPct = 0;
    uint8  fourPieceHpPct = 0;
    uint16 fourPieceCrit = 0;
    uint16 fourPieceHaste = 0;
    std::string fourPieceSpecialEffect;
    std::string fourPieceDesc;
    bool enabled = true;
};

// 玩家当前活跃套装buff状态
struct PlayerSetBonusState
{
    uint32 setId = 0;
    uint8 pieceCount = 0;
    bool hasTwoPieceBonus = false;
    bool hasFourPieceBonus = false;
};

struct AbyssTaskDocking
{
    uint32 questId = 0;
    uint16 chapterId = 0;
    uint8 taskType = 0;
    std::string title;
    uint32 previousQuestId = 0;
    uint32 nextQuestId = 0;
    uint16 suggestedQuestLevel = 0;
    uint8 suggestedMinLevel = 0;
    int16 suggestedQuestSort = 0;
    std::string note;
    uint8 dockingStatus = 0;
};

struct AbyssBossDocking
{
    uint32 bossEntry = 0;
    std::string bossName;
    uint8 bossType = 0;
    uint8 actId = 0;
    uint16 chapterId = 0;
    uint8 suggestedLevel = 0;
    uint16 suggestedFaction = 0;
    uint32 suggestedModelGroup = 0;
    std::string suggestedScriptName;
    bool shouldWriteCreatureTemplate = false;
    bool shouldWriteLootTemplate = false;
    uint8 dockingStatus = 0;
    std::string note;
};

struct AbyssItemDocking
{
    uint32 itemId = 0;
    std::string itemName;
    uint8 dockingType = 0;
    uint8 actId = 0;
    uint16 chapterId = 0;
    uint8 suggestedQuality = 0;
    uint8 suggestedClass = 0;
    uint8 suggestedSubClass = 0;
    uint8 suggestedInventoryType = 0;
    bool uniqueItem = false;
    bool bagActivated = false;
    uint32 reservedDisplayId = 0;
    uint8 dockingStatus = 0;
    std::string note;
};

struct PlayerAbyssData
{
    uint16 currentChapter = 0;
    uint16 highestChapter = 0;
    uint16 currentCultivationThreshold = 0;
    uint16 highestCorruptionTier = 0;
    uint32 unlockedModeMask = 0;
    uint8 storyState = 0;
    uint32 mainRelic = 0;
    uint32 subRelic1 = 0;
    uint32 subRelic2 = 0;
    uint32 subRelic3 = 0;
    uint32 subRelic4 = 0;
    uint32 subRelic5 = 0;
    uint32 phaseArtifact = 0;
    uint32 ultimateArtifact = 0;
    uint8 presetIndex = 0;
    uint16 pityChapterId = 0;
    uint16 abyssGearFailCount = 0;
    uint16 cacheBossFailCount = 0;
    uint32 lastUpdated = 0;
    bool hasDatabaseRow = false;
};

struct PlayerAbyssChapterModeUnlock
{
    uint16 chapterId = 0;
    uint32 unlockedModeMask = 0;
    uint32 lastUpdated = 0;
};

struct PlayerAbyssRunState
{
    uint16 currentMapId = 0;
    uint16 currentChapterId = 0;
    uint8 modeType = 0;
    uint16 corruptionTier = 0;
    uint32 runMainRelic = 0;
    uint32 runSubRelic1 = 0;
    uint32 runSubRelic2 = 0;
    uint32 runSubRelic3 = 0;
    uint32 runSubRelic4 = 0;
    uint32 runSubRelic5 = 0;
    uint64 anchorBossKillMask = 0;
    bool abyssBossSummoned = false;
    bool cacheBossSummoned = false;
    uint32 startTime = 0;
    bool hasDatabaseRow = false;
};

struct PlayerAbyssCollectionEntry
{
    uint8 collectionType = 0;
    uint32 itemId = 0;
    uint16 relatedChapterId = 0;
    uint16 level = 0;
    uint16 awakenLevel = 0;
    uint8 sourceType = 0;
    uint32 unlockTime = 0;
};

struct PlayerAbyssProcState
{
    uint32 killCounter = 0;
    uint32 bloodKillCounter = 0;
    uint32 poisonCastCounter = 0;
    uint32 bloodFlameComboCounter = 0;
    uint32 starfallCounter = 0;
    uint32 tideCastCounter = 0;
    uint32 bloomCastCounter = 0;
    uint32 lastSpellId = 0;
    uint32 lastComboSpellId = 0;
    uint32 lastSpellCastTime = 0;
    uint32 lastReplayTime = 0;
    uint32 lastLowHealthProcTime = 0;
    uint32 lastChainLightningTime = 0;
    uint32 lastFireRingTime = 0;
    uint32 lastSoulProcTime = 0;
    uint32 lastPoisonBurstTime = 0;
    uint32 lastBloodFlameTime = 0;
    uint32 lastFeatherVolleyTime = 0;
    uint32 lastStarfallTime = 0;
    uint32 lastObserverChoiceTime = 0;
    uint32 lastTideBurstTime = 0;
    uint32 lastBloomBurstTime = 0;
    uint32 lastMirrorEchoTime = 0;
    uint32 lastSwarmBurstTime = 0;
    uint32 lastBattleBannerTime = 0;
    uint32 lastRedJadeTime = 0;
    uint32 lastDreamBurstTime = 0;
    uint32 lastPrisonChoiceTime = 0;
    uint32 lastMatrixEchoTime = 0;
    uint32 lastHolyVerdictTime = 0;
    uint32 lastDominionTime = 0;
    uint32 matrixCastCounter = 0;
    uint32 dominionCounter = 0;
    float lastPromptX = 0.0f;
    float lastPromptY = 0.0f;
    float lastPromptZ = 0.0f;
    float lastPromptO = 0.0f;
    bool hasPromptPosition = false;
    uint8 beastMode = 0;
    bool abyssModePromptShown = false;
    bool replayingSpell = false;
};

struct AbyssRewardResult
{
    uint32 itemId = 0;
    std::string itemName;
    uint8 dockingType = 0;
    uint8 quality = 0;
    uint8 sourceMode = 0;
    uint16 baseItemLevel = 0;
    uint32 bossEntry = 0;
    uint32 rewardTime = 0;
    std::string prefixName;
    std::string suffixName;
    std::string chapterAffixName;
    std::string specialEffectName;
    std::string rewardSummary;
    bool awarded = false;
    bool inventoryStored = false;
    bool countedAsAbyssGear = false;
    bool countedAsHighQuality = false;
    bool countedAsChapterEffect = false;
};

struct AbyssRewardCandidate
{
    uint32 itemId = 0;
    std::string itemName;
    uint8 dockingType = 0;
    uint8 quality = 0;
    uint8 inventoryType = 0;
    bool uniqueItem = false;
    bool bagActivated = false;
    uint8 sourceMode = 0;
    uint16 baseItemLevel = 0;
    uint32 slotMask = 0;
};

struct AbyssChapterAccessState
{
    bool prerequisiteMet = false;
    bool levelMet = false;
    bool readyToEnter = false;
    std::string reason;
};

void SendAbyssModePrompt(Player* player, AbyssChapterConfig const& chapter, PlayerAbyssData const& data, uint32 modeMask);

class AbyssCultivationMgr
{
public:
    static AbyssCultivationMgr* instance()
    {
        static AbyssCultivationMgr instance;
        return &instance;
    }

    void ClearWorldData()
    {
        _chapterConfigs.clear();
        _relicConfigs.clear();
        _bossConfigs.clear();
        _equipmentTemplates.clear();
        _setBonusConfigs.clear();
        _affixTemplates.clear();
        _specialEffectTemplates.clear();
        _taskDockings.clear();
        _bossDockings.clear();
        _itemDockings.clear();
        _worldDataLoaded = false;
    }

    void LoadChapterConfigs()
    {
        uint32 oldMSTime = getMSTime();
        _chapterConfigs.clear();

        QueryResult result = WorldDatabase.Query(kLoadChaptersSql);
        if (!result)
        {
            _worldDataLoaded = true;
            LOG_WARN("module", "mod-abyss-cultivation: no chapter rows found in custom world table");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssChapterConfig config;
            config.chapterId = fields[0].Get<uint16>();
            config.actId = fields[1].Get<uint8>();
            config.chapterName = fields[2].Get<std::string>();
            config.mapId = fields[3].Get<uint16>();
            config.chapterType = fields[4].Get<uint8>();
            config.requiredCultivationLevel = fields[5].Get<uint16>();
            config.prerequisiteChapterId = fields[6].Get<uint16>();
            config.startQuestId = fields[7].Get<uint32>();
            config.completeQuestId = fields[8].Get<uint32>();
            config.relicItemId = fields[9].Get<uint32>();
            config.lootGroupId = fields[10].Get<uint32>();
            config.anchorBossEntry = fields[11].Get<uint32>();
            config.finalBossEntry = fields[12].Get<uint32>();
            config.triggerType = fields[13].Get<uint8>();
            config.requiredBossKillMask = fields[14].Get<uint64>();
            config.firstClearSummonRule = fields[15].Get<uint8>();
            config.repeatSummonRule = fields[16].Get<uint8>();
            config.abyssBossEntry = fields[17].Get<uint32>();
            config.abyssSummonMapId = fields[18].Get<uint16>();
            config.abyssSummonX = fields[19].Get<float>();
            config.abyssSummonY = fields[20].Get<float>();
            config.abyssSummonZ = fields[21].Get<float>();
            config.abyssSummonO = fields[22].Get<float>();
            config.cacheBossEntry = fields[23].Get<uint32>();
            config.cacheBossBaseChance = fields[24].Get<float>();
            config.cacheBossPityCount = fields[25].Get<uint16>();
            config.preferredSlotMask = fields[26].Get<uint32>();
            config.commonBaseMaterialPackage = fields[27].Get<std::string>();
            config.storyDropRate = fields[28].Get<float>();
            config.abyssDropRate = fields[29].Get<float>();
            config.corruptionDropRate = fields[30].Get<float>();
            config.reincarnationDropRate = fields[31].Get<float>();
            config.hiddenRoomBonus = fields[32].Get<float>();
            config.cacheBossRewardBonus = fields[33].Get<float>();
            config.abyssGearPityCount = fields[34].Get<uint16>();
            config.quickFarmUnlocked = fields[35].Get<bool>();
            config.enabled = fields[36].Get<bool>();

            _chapterConfigs[config.chapterId] = config;
            ++count;
        } while (result->NextRow());

        _worldDataLoaded = true;
        uint32 warningCount = 0;
        for (auto const& pair : _chapterConfigs)
        {
            AbyssChapterConfig const& config = pair.second;

            if (config.prerequisiteChapterId != 0 && !GetChapterConfig(config.prerequisiteChapterId))
            {
                ++warningCount;
                LOG_WARN("module", "mod-abyss-cultivation: chapter {} prerequisite chapter {} is missing from cache",
                    config.chapterId, config.prerequisiteChapterId);
            }

            if (config.startQuestId == 0 || config.completeQuestId == 0)
            {
                ++warningCount;
                LOG_WARN("module", "mod-abyss-cultivation: chapter {} has incomplete quest mapping startQuest={} completeQuest={}",
                    config.chapterId, config.startQuestId, config.completeQuestId);
            }
        }

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} chapters in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));

        if (warningCount > 0)
        {
            LOG_WARN("module", "mod-abyss-cultivation: chapter cache validation finished with {} warning(s)",
                warningCount);
        }
    }

    void LoadTaskDockings()
    {
        uint32 oldMSTime = getMSTime();
        _taskDockings.clear();

        QueryResult result = WorldDatabase.Query(kLoadTaskDockingsSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no task docking rows found in custom world table");
            return;
        }

        uint32 count = 0;
        uint32 warningCount = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssTaskDocking docking;
            docking.questId = fields[0].Get<uint32>();
            docking.chapterId = fields[1].Get<uint16>();
            docking.taskType = fields[2].Get<uint8>();
            docking.title = fields[3].Get<std::string>();
            docking.previousQuestId = fields[4].Get<uint32>();
            docking.nextQuestId = fields[5].Get<uint32>();
            docking.suggestedQuestLevel = fields[6].Get<uint16>();
            docking.suggestedMinLevel = fields[7].Get<uint8>();
            docking.suggestedQuestSort = fields[8].Get<int16>();
            docking.note = fields[9].Get<std::string>();
            docking.dockingStatus = fields[10].Get<uint8>();

            if (!GetChapterConfig(docking.chapterId))
            {
                ++warningCount;
                LOG_WARN("module", "mod-abyss-cultivation: task docking quest {} points to missing chapter {}",
                    docking.questId, docking.chapterId);
            }

            _taskDockings[docking.questId] = docking;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} task dockings in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));

        if (warningCount > 0)
        {
            LOG_WARN("module", "mod-abyss-cultivation: task docking validation finished with {} warning(s)",
                warningCount);
        }
    }

    void LoadBossDockings()
    {
        uint32 oldMSTime = getMSTime();
        _bossDockings.clear();

        QueryResult result = WorldDatabase.Query(kLoadBossDockingsSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no custom boss docking rows found in custom world table");
            return;
        }

        uint32 count = 0;
        uint32 warningCount = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssBossDocking docking;
            docking.bossEntry = fields[0].Get<uint32>();
            docking.bossName = fields[1].Get<std::string>();
            docking.bossType = fields[2].Get<uint8>();
            docking.actId = fields[3].Get<uint8>();
            docking.chapterId = fields[4].Get<uint16>();
            docking.suggestedLevel = fields[5].Get<uint8>();
            docking.suggestedFaction = fields[6].Get<uint16>();
            docking.suggestedModelGroup = fields[7].Get<uint32>();
            docking.suggestedScriptName = fields[8].Get<std::string>();
            docking.shouldWriteCreatureTemplate = fields[9].Get<bool>();
            docking.shouldWriteLootTemplate = fields[10].Get<bool>();
            docking.dockingStatus = fields[11].Get<uint8>();
            docking.note = fields[12].Get<std::string>();

            if (docking.chapterId != 0 && !GetChapterConfig(docking.chapterId))
            {
                ++warningCount;
                LOG_WARN("module", "mod-abyss-cultivation: boss docking entry {} points to missing chapter {}",
                    docking.bossEntry, docking.chapterId);
            }

            _bossDockings[docking.bossEntry] = docking;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} custom boss dockings in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));

        if (warningCount > 0)
        {
            LOG_WARN("module", "mod-abyss-cultivation: boss docking validation finished with {} warning(s)",
                warningCount);
        }
    }

    void LoadItemDockings()
    {
        uint32 oldMSTime = getMSTime();
        _itemDockings.clear();

        QueryResult result = WorldDatabase.Query(kLoadItemDockingsSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no item docking rows found in custom world table");
            return;
        }

        uint32 count = 0;
        uint32 warningCount = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssItemDocking docking;
            docking.itemId = fields[0].Get<uint32>();
            docking.itemName = fields[1].Get<std::string>();
            docking.dockingType = fields[2].Get<uint8>();
            docking.actId = fields[3].Get<uint8>();
            docking.chapterId = fields[4].Get<uint16>();
            docking.suggestedQuality = fields[5].Get<uint8>();
            docking.suggestedClass = fields[6].Get<uint8>();
            docking.suggestedSubClass = fields[7].Get<uint8>();
            docking.suggestedInventoryType = fields[8].Get<uint8>();
            docking.uniqueItem = fields[9].Get<bool>();
            docking.bagActivated = fields[10].Get<bool>();
            docking.reservedDisplayId = fields[11].Get<uint32>();
            docking.dockingStatus = fields[12].Get<uint8>();
            docking.note = fields[13].Get<std::string>();

            if (docking.chapterId != 0 && !GetChapterConfig(docking.chapterId))
            {
                ++warningCount;
                LOG_WARN("module", "mod-abyss-cultivation: item docking item {} points to missing chapter {}",
                    docking.itemId, docking.chapterId);
            }

            _itemDockings[docking.itemId] = docking;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} item dockings in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));

        if (warningCount > 0)
        {
            LOG_WARN("module", "mod-abyss-cultivation: item docking validation finished with {} warning(s)",
                warningCount);
        }
    }

    void LoadRelicConfigs()
    {
        uint32 oldMSTime = getMSTime();
        _relicConfigs.clear();

        QueryResult result = WorldDatabase.Query(kLoadRelicsSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no relic config rows found in custom world table");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssRelicConfig config;
            config.itemId = fields[0].Get<uint32>();
            config.name = fields[1].Get<std::string>();
            config.relicType = fields[2].Get<uint8>();
            config.actId = fields[3].Get<uint8>();
            config.relatedChapterId = fields[4].Get<uint16>();
            config.activeSlot = fields[5].Get<uint8>();
            config.activeRule = fields[6].Get<uint8>();
            config.exclusiveGroup = fields[7].Get<uint32>();
            config.effectFamily = fields[8].Get<std::string>();
            config.recommendedSlot = fields[9].Get<uint8>();
            config.scriptGroup = fields[10].Get<std::string>();
            config.subSlotScale = fields[11].Get<float>();
            config.briefDescription = fields[13].Get<std::string>();
            config.fullDescription = fields[14].Get<std::string>();
            config.agilityWeight = fields[15].Get<uint8>();
            config.strengthWeight = fields[16].Get<uint8>();
            config.intellectWeight = fields[17].Get<uint8>();
            config.spiritWeight = fields[18].Get<uint8>();
            config.staminaWeight = fields[19].Get<uint8>();
            config.hitRatingWeight = fields[20].Get<uint8>();
            config.critRatingWeight = fields[21].Get<uint8>();
            config.hasteRatingWeight = fields[22].Get<uint8>();
            config.attackPowerWeight = fields[23].Get<uint8>();
            config.spellPowerWeight = fields[24].Get<uint8>();

            _relicConfigs[config.itemId] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} relic configs in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));
    }

    void LoadBossConfigs()
    {
        uint32 oldMSTime = getMSTime();
        _bossConfigs.clear();

        QueryResult result = WorldDatabase.Query(kLoadBossConfigsSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no boss config rows found in custom world table");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssBossConfig config;
            config.bossEntry = fields[0].Get<uint32>();
            config.bossName = fields[1].Get<std::string>();
            config.bossType = fields[2].Get<uint8>();
            config.actId = fields[3].Get<uint8>();
            config.chapterId = fields[4].Get<uint16>();
            config.healthModifier = fields[5].Get<float>();
            config.damageModifier = fields[6].Get<float>();
            config.storyModifier = fields[7].Get<float>();
            config.abyssModifier = fields[8].Get<float>();
            config.corruptionModifier = fields[9].Get<float>();
            config.reincarnationModifier = fields[10].Get<float>();
            config.phase1HealthPct = fields[11].Get<uint8>();
            config.phase1SkillGroup = fields[12].Get<std::string>();
            config.phase2HealthPct = fields[13].Get<uint8>();
            config.phase2SkillGroup = fields[14].Get<std::string>();
            config.phase3HealthPct = fields[15].Get<uint8>();
            config.phase3SkillGroup = fields[16].Get<std::string>();
            config.areaEffect = fields[17].Get<std::string>();
            config.lootPackageId = fields[18].Get<uint32>();
            config.introText = fields[19].Get<std::string>();
            config.deathText = fields[20].Get<std::string>();

            _bossConfigs[config.bossEntry] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} boss configs in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));
    }

    void LoadEquipmentTemplates()
    {
        uint32 oldMSTime = getMSTime();
        _equipmentTemplates.clear();

        QueryResult result = WorldDatabase.Query(kLoadEquipmentTemplatesSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no equipment template rows found in custom world table");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssEquipmentTemplate config;
            config.templateId = fields[0].Get<uint32>();
            config.itemId = fields[1].Get<uint32>();
            config.itemName = fields[2].Get<std::string>();
            config.equipmentType = fields[3].Get<uint8>();
            config.sourceChapter = fields[4].Get<uint16>();
            config.sourceMode = fields[5].Get<uint8>();
            config.slotMask = fields[6].Get<uint32>();
            config.actId = fields[7].Get<uint8>();
            config.baseItemLevel = fields[8].Get<uint16>();
            config.armorType = fields[9].Get<uint8>();
            config.damageType = fields[10].Get<uint8>();
            config.minPrimaryBudget = fields[11].Get<uint16>();
            config.maxPrimaryBudget = fields[12].Get<uint16>();
            config.minSecondaryBudget = fields[13].Get<uint16>();
            config.maxSecondaryBudget = fields[14].Get<uint16>();
            config.minEffectBudget = fields[15].Get<uint16>();
            config.maxEffectBudget = fields[16].Get<uint16>();
            config.fixedAffixGroup = fields[17].Get<std::string>();
            config.fixedEffectId = fields[18].Get<uint32>();
            config.fromCacheBoss = fields[19].Get<bool>();
            config.requiresFragments = fields[20].Get<bool>();
            config.flavorText = fields[21].Get<std::string>();
            config.setId = fields[22].Get<uint32>();
            config.enabled = fields[23].Get<bool>();

            _equipmentTemplates[config.itemId] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} equipment templates in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));
    }

    void LoadSetBonusConfigs()
    {
        uint32 oldMSTime = getMSTime();
        _setBonusConfigs.clear();

        QueryResult result = WorldDatabase.Query(kLoadSetBonusConfigsSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no set bonus config rows found");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssSetBonusConfig config;
            config.setId            = fields[0].Get<uint32>();
            config.setName          = fields[1].Get<std::string>();
            config.actId            = fields[2].Get<uint8>();
            config.sourceMode       = fields[3].Get<uint8>();
            config.twoPieceDesc     = fields[4].Get<std::string>();
            config.twoPieceAllStat  = fields[5].Get<uint16>();
            config.twoPieceCrit     = fields[6].Get<uint16>();
            config.twoPieceHaste    = fields[7].Get<uint16>();
            config.twoPieceAP       = fields[8].Get<uint16>();
            config.twoPieceSP       = fields[9].Get<uint16>();
            config.fourPieceDesc    = fields[10].Get<std::string>();
            config.fourPieceDmgPct  = fields[11].Get<uint8>();
            config.fourPieceHpPct   = fields[12].Get<uint8>();
            config.fourPieceCrit    = fields[13].Get<uint16>();
            config.fourPieceHaste   = fields[14].Get<uint16>();
            config.fourPieceSpecialEffect = fields[15].Get<std::string>();
            config.enabled          = fields[16].Get<bool>();

            _setBonusConfigs[config.setId] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} set bonus configs in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));
    }

    void LoadAffixTemplates()
    {
        uint32 oldMSTime = getMSTime();
        _affixTemplates.clear();

        QueryResult result = WorldDatabase.Query(kLoadAffixTemplatesSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no affix template rows found in custom world table");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssAffixTemplate config;
            config.affixId = fields[0].Get<uint32>();
            config.affixName = fields[1].Get<std::string>();
            config.affixType = fields[2].Get<uint8>();
            config.affixGroup = fields[3].Get<uint32>();
            config.minChapter = fields[4].Get<uint16>();
            config.maxChapter = fields[5].Get<uint16>();
            config.slotMask = fields[6].Get<uint32>();
            config.minQuality = fields[7].Get<uint8>();
            config.formula = fields[8].Get<std::string>();
            config.scriptGroup = fields[9].Get<std::string>();
            config.description = fields[10].Get<std::string>();

            _affixTemplates[config.affixId] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} affix templates in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));
    }

    void LoadSpecialEffectTemplates()
    {
        uint32 oldMSTime = getMSTime();
        _specialEffectTemplates.clear();

        QueryResult result = WorldDatabase.Query(kLoadSpecialEffectTemplatesSql);
        if (!result)
        {
            LOG_WARN("module", "mod-abyss-cultivation: no special effect template rows found in custom world table");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            AbyssSpecialEffectTemplate config;
            config.effectId = fields[0].Get<uint32>();
            config.effectName = fields[1].Get<std::string>();
            config.triggerType = fields[2].Get<uint8>();
            config.triggerParam1 = fields[3].Get<uint32>();
            config.triggerParam2 = fields[4].Get<uint32>();
            config.cooldownMs = fields[5].Get<uint32>();
            config.ppmRate = fields[6].Get<float>();
            config.effectFamily = fields[7].Get<std::string>();
            config.formula = fields[8].Get<std::string>();
            config.allowedSlotMask = fields[9].Get<uint32>();
            config.scriptGroup = fields[10].Get<std::string>();
            config.description = fields[11].Get<std::string>();

            _specialEffectTemplates[config.effectId] = config;
            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-abyss-cultivation: loaded {} special effect templates in {} ms",
            count, GetMSTimeDiffToNow(oldMSTime));
    }

    void LoadPlayerData(Player* player, bool createIfMissing)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        PlayerAbyssData data;

        QueryResult result = CharacterDatabase.Query(kLoadPlayerSql, guid);
        if (!result)
        {
            if (createIfMissing)
            {
                CharacterDatabase.Execute(kInsertPlayerSql, guid);
                data.lastUpdated = GetNow();
                data.hasDatabaseRow = true;
            }

            _playerData[guid] = data;

            if (IsDebugEnabled())
            {
                LOG_DEBUG("module", "mod-abyss-cultivation: initialized player cache for {} ({})",
                    player->GetName(), guid);
            }

            return;
        }

        Field* fields = result->Fetch();
        data.currentChapter = fields[0].Get<uint16>();
        data.highestChapter = fields[1].Get<uint16>();
        data.currentCultivationThreshold = fields[2].Get<uint16>();
        data.highestCorruptionTier = fields[3].Get<uint16>();
        data.unlockedModeMask = fields[4].Get<uint32>();
        data.storyState = fields[5].Get<uint8>();
        data.mainRelic = fields[6].Get<uint32>();
        data.subRelic1 = fields[7].Get<uint32>();
        data.subRelic2 = fields[8].Get<uint32>();
        data.subRelic3 = fields[9].Get<uint32>();
        data.subRelic4 = fields[10].Get<uint32>();
        data.subRelic5 = fields[11].Get<uint32>();
        data.phaseArtifact = fields[12].Get<uint32>();
        data.ultimateArtifact = fields[13].Get<uint32>();
        data.presetIndex = fields[14].Get<uint8>();
        data.pityChapterId = fields[15].Get<uint16>();
        data.abyssGearFailCount = fields[16].Get<uint16>();
        data.cacheBossFailCount = fields[17].Get<uint16>();
        data.lastUpdated = fields[18].Get<uint32>();
        data.hasDatabaseRow = true;

        _playerData[guid] = data;

        if (IsDebugEnabled())
        {
            LOG_DEBUG("module", "mod-abyss-cultivation: loaded player {} ({}) currentChapter={} highestChapter={}",
                player->GetName(), guid, data.currentChapter, data.highestChapter);
        }
    }

    void LoadPlayerCollections(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        std::vector<PlayerAbyssCollectionEntry> entries;

        QueryResult result = CharacterDatabase.Query(kLoadPlayerCollectionSql, guid);
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                PlayerAbyssCollectionEntry entry;
                entry.collectionType = fields[0].Get<uint8>();
                entry.itemId = fields[1].Get<uint32>();
                entry.relatedChapterId = fields[2].Get<uint16>();
                entry.level = fields[3].Get<uint16>();
                entry.awakenLevel = fields[4].Get<uint16>();
                entry.sourceType = fields[5].Get<uint8>();
                entry.unlockTime = fields[6].Get<uint32>();
                entries.push_back(entry);
            } while (result->NextRow());
        }

        std::sort(entries.begin(), entries.end(), [](PlayerAbyssCollectionEntry const& left, PlayerAbyssCollectionEntry const& right)
        {
            if (left.collectionType != right.collectionType)
                return left.collectionType < right.collectionType;
            if (left.relatedChapterId != right.relatedChapterId)
                return left.relatedChapterId < right.relatedChapterId;
            return left.itemId < right.itemId;
        });

        _playerCollections[guid] = std::move(entries);
    }

    void LoadPlayerChapterModeUnlocks(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        std::unordered_map<uint16, PlayerAbyssChapterModeUnlock> unlocks;

        QueryResult result = CharacterDatabase.Query(kLoadPlayerChapterModeSql, guid);
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                PlayerAbyssChapterModeUnlock entry;
                entry.chapterId = fields[0].Get<uint16>();
                entry.unlockedModeMask = fields[1].Get<uint32>();
                entry.lastUpdated = GetNow();
                unlocks[entry.chapterId] = entry;
            } while (result->NextRow());
        }

        _playerChapterModeUnlocks[guid] = std::move(unlocks);
    }

    void EnsurePlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end() || !itr->second.hasDatabaseRow)
            LoadPlayerData(player, true);

        if (_playerChapterModeUnlocks.find(guid) == _playerChapterModeUnlocks.end())
            LoadPlayerChapterModeUnlocks(player);
    }

    void SavePlayerData(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return;

        PlayerAbyssData& data = itr->second;
        data.lastUpdated = GetNow();
        data.hasDatabaseRow = true;

        CharacterDatabase.Execute(
            kSavePlayerSql,
            guid,
            data.currentChapter,
            data.highestChapter,
            data.currentCultivationThreshold,
            data.highestCorruptionTier,
            data.unlockedModeMask,
            data.storyState,
            data.mainRelic,
            data.subRelic1,
            data.subRelic2,
            data.subRelic3,
            data.subRelic4,
            data.subRelic5,
            data.phaseArtifact,
            data.ultimateArtifact,
            data.presetIndex,
            data.pityChapterId,
            data.abyssGearFailCount,
            data.cacheBossFailCount,
            data.lastUpdated);

        if (IsDebugEnabled())
        {
            LOG_DEBUG("module", "mod-abyss-cultivation: saved player {} ({})",
                player->GetName(), guid);
        }
    }

    uint32 GetStoredChapterModeMask(uint32 guid, uint16 chapterId) const
    {
        auto playerItr = _playerChapterModeUnlocks.find(guid);
        if (playerItr == _playerChapterModeUnlocks.end())
            return 0;

        auto chapterItr = playerItr->second.find(chapterId);
        if (chapterItr == playerItr->second.end())
            return 0;

        return chapterItr->second.unlockedModeMask;
    }

    void SavePlayerChapterModeUnlock(Player* player, uint16 chapterId)
    {
        if (!player || chapterId == 0)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto playerItr = _playerChapterModeUnlocks.find(guid);
        if (playerItr == _playerChapterModeUnlocks.end())
            return;

        auto chapterItr = playerItr->second.find(chapterId);
        if (chapterItr == playerItr->second.end())
            return;

        chapterItr->second.lastUpdated = GetNow();
        CharacterDatabase.Execute(
            kSavePlayerChapterModeSql,
            guid,
            chapterItr->second.chapterId,
            chapterItr->second.unlockedModeMask,
            chapterItr->second.lastUpdated);
    }

    uint32 GetChapterUnlockedModeMask(Player* player, PlayerAbyssData const& data, AbyssChapterConfig const& chapter) const
    {
        if (!player)
            return 0;

        return GetStoredChapterModeMask(player->GetGUID().GetCounter(), chapter.chapterId);
    }

    bool IsChapterModeUnlocked(Player* player, PlayerAbyssData const& data, AbyssChapterConfig const& chapter, uint8 modeType) const
    {
        uint32 requiredModeMask = GetModeTypeMask(modeType);
        if (requiredModeMask == 0)
            return false;

        return (GetChapterUnlockedModeMask(player, data, chapter) & requiredModeMask) != 0;
    }

    bool UnlockChapterMode(Player* player, uint16 chapterId, uint8 modeType)
    {
        if (!player || chapterId == 0 || modeType == 0 || modeType > 4)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto& chapterUnlocks = _playerChapterModeUnlocks[guid];
        PlayerAbyssChapterModeUnlock& entry = chapterUnlocks[chapterId];
        entry.chapterId = chapterId;

        uint32 grantMask = 0;
        for (uint8 currentMode = 1; currentMode <= modeType; ++currentMode)
            grantMask |= GetModeTypeMask(currentMode);

        if ((entry.unlockedModeMask & grantMask) == grantMask)
            return false;

        entry.unlockedModeMask |= grantMask;
        SavePlayerChapterModeUnlock(player, chapterId);
        return true;
    }

    void LoadPlayerRunState(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        QueryResult result = CharacterDatabase.Query(kLoadPlayerRunStateSql, guid);
        if (!result)
        {
            _playerRunStates.erase(guid);
            return;
        }

        Field* fields = result->Fetch();
        PlayerAbyssRunState state;
        state.currentMapId = fields[1].Get<uint16>();
        state.currentChapterId = fields[2].Get<uint16>();
        state.modeType = fields[3].Get<uint8>();
        state.corruptionTier = fields[4].Get<uint16>();
        state.runMainRelic = fields[5].Get<uint32>();
        state.runSubRelic1 = fields[6].Get<uint32>();
        state.runSubRelic2 = fields[7].Get<uint32>();
        state.runSubRelic3 = fields[8].Get<uint32>();
        state.runSubRelic4 = fields[9].Get<uint32>();
        state.runSubRelic5 = fields[10].Get<uint32>();
        state.anchorBossKillMask = fields[11].Get<uint64>();
        state.abyssBossSummoned = fields[12].Get<bool>();
        state.cacheBossSummoned = fields[13].Get<bool>();
        state.startTime = fields[14].Get<uint32>();
        state.hasDatabaseRow = true;
        _playerRunStates[guid] = state;
    }

    void SavePlayerRunState(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerRunStates.find(guid);
        if (itr == _playerRunStates.end())
            return;

        PlayerAbyssRunState& state = itr->second;
        state.hasDatabaseRow = true;
        CharacterDatabase.Execute(
            kSavePlayerRunStateSql,
            guid,
            state.currentMapId,
            state.currentChapterId,
            state.modeType,
            state.corruptionTier,
            state.runMainRelic,
            state.runSubRelic1,
            state.runSubRelic2,
            state.runSubRelic3,
            state.runSubRelic4,
            state.runSubRelic5,
            state.anchorBossKillMask,
            state.abyssBossSummoned,
            state.cacheBossSummoned,
            state.startTime);
    }

    void ClearPlayerRunState(uint32 guid)
    {
        _playerRunStates.erase(guid);
    }

    void ClearPlayerCollections(uint32 guid)
    {
        _playerCollections.erase(guid);
    }

    void ClearPlayerChapterModeUnlocks(uint32 guid)
    {
        _playerChapterModeUnlocks.erase(guid);
    }

    void DeletePlayerRunState(uint32 guid)
    {
        ClearPlayerRunState(guid);
        CharacterDatabase.Execute(kDeletePlayerRunStateSql, guid);
    }

    void ClearPlayerData(uint32 guid)
    {
        _playerData.erase(guid);
        ClearPlayerCollections(guid);
        ClearPlayerChapterModeUnlocks(guid);
    }

    void DeletePlayerData(uint32 guid)
    {
        ClearPlayerData(guid);
        ClearPlayerRunState(guid);
        ClearPlayerProcState(guid);
        CharacterDatabase.Execute(kDeletePlayerSql, guid);
        CharacterDatabase.Execute(kDeletePlayerChapterModeSql, guid);
        CharacterDatabase.Execute(kDeletePlayerRunStateSql, guid);
        CharacterDatabase.Execute(kDeletePlayerCollectionSql, guid);
    }

    bool HasCachedPlayerData(uint32 guid) const
    {
        return _playerData.find(guid) != _playerData.end();
    }

    std::vector<PlayerAbyssCollectionEntry> const& GetPlayerCollections(uint32 guid) const
    {
        static std::vector<PlayerAbyssCollectionEntry> const emptyEntries;

        auto itr = _playerCollections.find(guid);
        if (itr != _playerCollections.end())
            return itr->second;

        return emptyEntries;
    }

    bool HasLoadedWorldData() const
    {
        return _worldDataLoaded;
    }

    uint32 GetChapterCount() const
    {
        return static_cast<uint32>(_chapterConfigs.size());
    }

    uint32 GetTaskDockingCount() const
    {
        return static_cast<uint32>(_taskDockings.size());
    }

    uint32 GetBossDockingCount() const
    {
        return static_cast<uint32>(_bossDockings.size());
    }

    uint32 GetItemDockingCount() const
    {
        return static_cast<uint32>(_itemDockings.size());
    }

    uint32 GetRelicConfigCount() const
    {
        return static_cast<uint32>(_relicConfigs.size());
    }

    std::unordered_map<uint32, AbyssRelicConfig> const& GetAllRelicConfigs() const
    {
        return _relicConfigs;
    }

    uint32 GetBossConfigCount() const
    {
        return static_cast<uint32>(_bossConfigs.size());
    }

    uint32 GetEquipmentTemplateCount() const
    {
        return static_cast<uint32>(_equipmentTemplates.size());
    }

    std::unordered_map<uint32, AbyssEquipmentTemplate> const& GetAllEquipmentTemplates() const
    {
        return _equipmentTemplates;
    }

    std::unordered_map<uint32, AbyssSetBonusConfig> const& GetSetBonusConfigs() const
    {
        return _setBonusConfigs;
    }

    uint32 GetAffixTemplateCount() const
    {
        return static_cast<uint32>(_affixTemplates.size());
    }

    uint32 GetSpecialEffectTemplateCount() const
    {
        return static_cast<uint32>(_specialEffectTemplates.size());
    }

    std::unordered_map<uint16, AbyssChapterConfig> const& GetAllChapterConfigs() const
    {
        return _chapterConfigs;
    }

    AbyssChapterConfig const* GetChapterConfig(uint16 chapterId) const
    {
        auto itr = _chapterConfigs.find(chapterId);
        if (itr != _chapterConfigs.end())
            return &itr->second;

        return nullptr;
    }

    AbyssChapterConfig const* GetChapterConfigByMapId(uint32 mapId) const
    {
        for (auto const& pair : _chapterConfigs)
        {
            if (pair.second.mapId == mapId)
                return &pair.second;
        }

        return nullptr;
    }

    AbyssChapterConfig const* GetNextChapterConfig(uint16 chapterId) const
    {
        AbyssChapterConfig const* result = nullptr;
        uint16 bestChapterId = 0;

        for (auto const& pair : _chapterConfigs)
        {
            uint16 candidateChapterId = pair.first;
            if (candidateChapterId <= chapterId)
                continue;

            if (!result || candidateChapterId < bestChapterId)
            {
                result = &pair.second;
                bestChapterId = candidateChapterId;
            }
        }

        return result;
    }

    PlayerAbyssData const* GetPlayerData(uint32 guid) const
    {
        auto itr = _playerData.find(guid);
        if (itr != _playerData.end())
            return &itr->second;

        return nullptr;
    }

    PlayerAbyssRunState const* GetPlayerRunState(uint32 guid) const
    {
        auto itr = _playerRunStates.find(guid);
        if (itr != _playerRunStates.end())
            return &itr->second;

        return nullptr;
    }

    AbyssRelicConfig const* GetRelicConfig(uint32 itemId) const
    {
        auto itr = _relicConfigs.find(itemId);
        if (itr != _relicConfigs.end())
            return &itr->second;

        return nullptr;
    }

    float GetConfiguredSubRelicScale(uint32 itemId) const
    {
        if (AbyssRelicConfig const* relic = GetRelicConfig(itemId))
            return ClampRelicScale(relic->subSlotScale);

        return 0.5f;
    }

    bool IsRelicCompatibleWithSlot(AbyssRelicConfig const& config, uint8 slot) const
    {
        switch (slot)
        {
            case ABYSS_RELIC_SLOT_MAIN:
            case ABYSS_RELIC_SLOT_SUB_1:
            case ABYSS_RELIC_SLOT_SUB_2:
            case ABYSS_RELIC_SLOT_SUB_3:
            case ABYSS_RELIC_SLOT_SUB_4:
            case ABYSS_RELIC_SLOT_SUB_5:
                return config.relicType == 1;
            case ABYSS_RELIC_SLOT_PHASE:
                return config.relicType == 2;
            case ABYSS_RELIC_SLOT_ULTIMATE:
                return config.relicType == 3;
            default:
                return false;
        }
    }

    bool NormalizePlayerActiveRelics(PlayerAbyssData& data) const
    {
        bool changed = false;
        std::vector<std::pair<uint8, uint32*>> slots =
        {
            { ABYSS_RELIC_SLOT_MAIN, &data.mainRelic },
            { ABYSS_RELIC_SLOT_SUB_1, &data.subRelic1 },
            { ABYSS_RELIC_SLOT_SUB_2, &data.subRelic2 },
            { ABYSS_RELIC_SLOT_SUB_3, &data.subRelic3 },
            { ABYSS_RELIC_SLOT_SUB_4, &data.subRelic4 },
            { ABYSS_RELIC_SLOT_SUB_5, &data.subRelic5 },
            { ABYSS_RELIC_SLOT_PHASE, &data.phaseArtifact },
            { ABYSS_RELIC_SLOT_ULTIMATE, &data.ultimateArtifact }
        };

        for (auto const& slotEntry : slots)
        {
            uint32* slotValue = slotEntry.second;
            if (!slotValue || *slotValue == 0)
                continue;

            AbyssRelicConfig const* relic = GetRelicConfig(*slotValue);
            if (!relic || !IsRelicCompatibleWithSlot(*relic, slotEntry.first))
            {
                *slotValue = 0;
                changed = true;
            }
        }

        std::unordered_map<uint32, uint8> seenItems;
        std::unordered_map<uint32, uint8> seenExclusiveGroups;
        for (auto const& slotEntry : slots)
        {
            uint32* slotValue = slotEntry.second;
            if (!slotValue || *slotValue == 0)
                continue;

            if (seenItems.find(*slotValue) != seenItems.end())
            {
                *slotValue = 0;
                changed = true;
                continue;
            }

            seenItems[*slotValue] = slotEntry.first;

            if (AbyssRelicConfig const* relic = GetRelicConfig(*slotValue))
            {
                if (relic->exclusiveGroup != 0)
                {
                    if (seenExclusiveGroups.find(relic->exclusiveGroup) != seenExclusiveGroups.end())
                    {
                        *slotValue = 0;
                        changed = true;
                        continue;
                    }

                    seenExclusiveGroups[relic->exclusiveGroup] = slotEntry.first;
                }
            }
        }

        return changed;
    }

    uint8 GetActiveRelicSlot(PlayerAbyssData const& data, uint32 itemId) const
    {
        if (itemId == 0)
            return 0;
        if (data.mainRelic == itemId)
            return ABYSS_RELIC_SLOT_MAIN;
        if (data.subRelic1 == itemId)
            return ABYSS_RELIC_SLOT_SUB_1;
        if (data.subRelic2 == itemId)
            return ABYSS_RELIC_SLOT_SUB_2;
        if (data.subRelic3 == itemId)
            return ABYSS_RELIC_SLOT_SUB_3;
        if (data.subRelic4 == itemId)
            return ABYSS_RELIC_SLOT_SUB_4;
        if (data.subRelic5 == itemId)
            return ABYSS_RELIC_SLOT_SUB_5;
        if (data.phaseArtifact == itemId)
            return ABYSS_RELIC_SLOT_PHASE;
        if (data.ultimateArtifact == itemId)
            return ABYSS_RELIC_SLOT_ULTIMATE;
        return 0;
    }

    bool PlayerOwnsCollectedRelic(uint32 guid, uint32 itemId) const
    {
        if (itemId == 0)
            return false;

        auto itr = _playerCollections.find(guid);
        if (itr == _playerCollections.end())
            return false;

        for (PlayerAbyssCollectionEntry const& entry : itr->second)
            if (entry.itemId == itemId)
                return true;

        return false;
    }

    AbyssBossConfig const* GetBossConfig(uint32 bossEntry) const
    {
        auto itr = _bossConfigs.find(bossEntry);
        if (itr != _bossConfigs.end())
            return &itr->second;

        return nullptr;
    }

    AbyssEquipmentTemplate const* GetEquipmentTemplate(uint32 itemId) const
    {
        auto itr = _equipmentTemplates.find(itemId);
        if (itr != _equipmentTemplates.end())
            return &itr->second;

        return nullptr;
    }

    AbyssAffixTemplate const* GetAffixTemplate(uint32 affixId) const
    {
        auto itr = _affixTemplates.find(affixId);
        if (itr != _affixTemplates.end())
            return &itr->second;

        return nullptr;
    }

    AbyssSpecialEffectTemplate const* GetSpecialEffectTemplate(uint32 effectId) const
    {
        auto itr = _specialEffectTemplates.find(effectId);
        if (itr != _specialEffectTemplates.end())
            return &itr->second;

        return nullptr;
    }

    std::vector<AbyssAffixTemplate const*> GetAffixCandidates(AbyssChapterConfig const& chapter, AbyssRewardCandidate const& candidate, uint8 affixType, uint8 qualityId, std::vector<std::string> const& fixedNames) const
    {
        std::vector<AbyssAffixTemplate const*> result;
        for (auto const& pair : _affixTemplates)
        {
            AbyssAffixTemplate const& affix = pair.second;
            if (affix.affixType != affixType)
                continue;
            if (chapter.chapterId < affix.minChapter || chapter.chapterId > affix.maxChapter)
                continue;
            if (qualityId < affix.minQuality)
                continue;
            if (affix.slotMask != 0 && candidate.slotMask != 0 && (affix.slotMask & candidate.slotMask) == 0)
                continue;
            if (!fixedNames.empty() && std::find(fixedNames.begin(), fixedNames.end(), affix.affixName) == fixedNames.end())
                continue;

            result.push_back(&affix);
        }

        return result;
    }

    AbyssAffixTemplate const* RollAffixFromPool(std::vector<AbyssAffixTemplate const*> const& pool) const
    {
        if (pool.empty())
            return nullptr;
        if (pool.size() == 1)
            return pool.front();

        return pool[RollWeight(static_cast<uint32>(pool.size())) % pool.size()];
    }

    AbyssSpecialEffectTemplate const* RollSpecialEffectTemplate(AbyssChapterConfig const& chapter, AbyssRewardCandidate const& candidate, uint8 qualityId) const
    {
        if (AbyssEquipmentTemplate const* equipment = GetEquipmentTemplate(candidate.itemId))
        {
            if (equipment->fixedEffectId != 0)
                if (AbyssSpecialEffectTemplate const* fixedEffect = GetSpecialEffectTemplate(equipment->fixedEffectId))
                    return fixedEffect;
        }

        if (qualityId < 3)
            return nullptr;

        std::vector<AbyssSpecialEffectTemplate const*> pool;
        for (auto const& pair : _specialEffectTemplates)
        {
            AbyssSpecialEffectTemplate const& effect = pair.second;
            if (effect.allowedSlotMask != 0 && candidate.slotMask != 0 && (effect.allowedSlotMask & candidate.slotMask) == 0)
                continue;

            pool.push_back(&effect);
        }

        if (!pool.empty())
            return pool[RollWeight(static_cast<uint32>(pool.size())) % pool.size()];

        return nullptr;
    }

    void RollRewardSummary(AbyssChapterConfig const& chapter, AbyssRewardCandidate const& candidate, AbyssRewardResult& result) const
    {
        std::vector<std::string> fixedNames;
        if (AbyssEquipmentTemplate const* equipment = GetEquipmentTemplate(candidate.itemId))
            fixedNames = SplitCsvTokens(equipment->fixedAffixGroup);

        if (AbyssAffixTemplate const* prefix = RollAffixFromPool(GetAffixCandidates(chapter, candidate, 1, candidate.quality, fixedNames)))
            result.prefixName = prefix->affixName;

        if (AbyssAffixTemplate const* suffix = RollAffixFromPool(GetAffixCandidates(chapter, candidate, 2, candidate.quality, fixedNames)))
            result.suffixName = suffix->affixName;

        if (candidate.quality >= 3)
            if (AbyssAffixTemplate const* chapterAffix = RollAffixFromPool(GetAffixCandidates(chapter, candidate, 3, candidate.quality, fixedNames)))
                result.chapterAffixName = chapterAffix->affixName;

        if (AbyssSpecialEffectTemplate const* effect = RollSpecialEffectTemplate(chapter, candidate, candidate.quality))
            result.specialEffectName = effect->effectName;

        std::vector<std::string> parts;
        if (!result.prefixName.empty())
            parts.push_back("前缀:" + result.prefixName);
        if (!result.suffixName.empty())
            parts.push_back("后缀:" + result.suffixName);
        if (!result.chapterAffixName.empty())
            parts.push_back("章节词缀:" + result.chapterAffixName);
        if (!result.specialEffectName.empty())
            parts.push_back("特效:" + result.specialEffectName);

        if (parts.empty())
            result.rewardSummary = "基础掉落";
        else
        {
            std::ostringstream summary;
            for (size_t index = 0; index < parts.size(); ++index)
            {
                if (index > 0)
                    summary << " / ";
                summary << parts[index];
            }

            result.rewardSummary = summary.str();
        }
    }

    AbyssBossDocking const* GetBossDocking(uint32 bossEntry) const
    {
        auto itr = _bossDockings.find(bossEntry);
        if (itr != _bossDockings.end())
            return &itr->second;

        return nullptr;
    }

    std::string GetBossDisplayName(uint32 bossEntry) const
    {
        if (bossEntry == 0)
            return "";

        if (AbyssBossConfig const* bossConfig = GetBossConfig(bossEntry))
            if (!bossConfig->bossName.empty())
                return bossConfig->bossName;

        if (AbyssBossDocking const* docking = GetBossDocking(bossEntry))
            if (!docking->bossName.empty())
                return docking->bossName;

        if (CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(bossEntry))
            if (!creatureTemplate->Name.empty())
                return creatureTemplate->Name;

        return "";
    }

    std::vector<AbyssTaskDocking const*> GetTaskDockingsForChapter(uint16 chapterId) const
    {
        std::vector<AbyssTaskDocking const*> result;
        for (auto const& pair : _taskDockings)
        {
            if (pair.second.chapterId == chapterId)
                result.push_back(&pair.second);
        }

        std::sort(result.begin(), result.end(), [](AbyssTaskDocking const* left, AbyssTaskDocking const* right)
        {
            if (left->taskType != right->taskType)
                return left->taskType < right->taskType;
            return left->questId < right->questId;
        });

        return result;
    }

    AbyssTaskDocking const* GetTaskDocking(uint32 questId) const
    {
        auto itr = _taskDockings.find(questId);
        if (itr != _taskDockings.end())
            return &itr->second;

        return nullptr;
    }

    AbyssTaskDocking const* GetTaskDockingForChapter(uint16 chapterId, uint8 taskType) const
    {
        for (auto const& pair : _taskDockings)
        {
            if (pair.second.chapterId == chapterId && pair.second.taskType == taskType)
                return &pair.second;
        }

        return nullptr;
    }

    std::vector<AbyssBossDocking const*> GetBossDockingsForChapter(uint16 chapterId) const
    {
        std::vector<AbyssBossDocking const*> result;
        for (auto const& pair : _bossDockings)
        {
            if (pair.second.chapterId == chapterId)
                result.push_back(&pair.second);
        }

        std::sort(result.begin(), result.end(), [](AbyssBossDocking const* left, AbyssBossDocking const* right)
        {
            if (left->bossType != right->bossType)
                return left->bossType < right->bossType;
            return left->bossEntry < right->bossEntry;
        });

        return result;
    }

    std::vector<AbyssItemDocking const*> GetItemDockingsForChapter(uint16 chapterId) const
    {
        std::vector<AbyssItemDocking const*> result;
        for (auto const& pair : _itemDockings)
        {
            if (pair.second.chapterId == chapterId)
                result.push_back(&pair.second);
        }

        std::sort(result.begin(), result.end(), [](AbyssItemDocking const* left, AbyssItemDocking const* right)
        {
            if (left->dockingType != right->dockingType)
                return left->dockingType < right->dockingType;
            return left->itemId < right->itemId;
        });

        return result;
    }

    float GetChapterDropRate(AbyssChapterConfig const& chapter, uint8 modeType) const
    {
        switch (modeType)
        {
            case 2: return chapter.abyssDropRate;
            case 3: return chapter.corruptionDropRate;
            case 4: return chapter.reincarnationDropRate;
            default: return chapter.storyDropRate;
        }
    }

    std::vector<AbyssRewardCandidate> GetRewardCandidatesForBoss(AbyssChapterConfig const& chapter, uint32 bossEntry, uint8 modeType = 1) const
    {
        std::vector<AbyssRewardCandidate> candidates;

        auto appendCandidate = [&](uint32 itemId, std::string const& itemName, uint8 dockingType, bool uniqueItem, bool bagActivated, uint8 sourceMode, uint16 baseItemLevel, uint32 slotMask)
        {
            if (itemId == 0)
                return;

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate)
                return;

            auto existingItr = std::find_if(candidates.begin(), candidates.end(), [&](AbyssRewardCandidate const& candidate)
            {
                return candidate.itemId == itemId;
            });
            if (existingItr != candidates.end())
                return;

            AbyssRewardCandidate candidate;
            candidate.itemId = itemId;
            candidate.itemName = itemName;
            candidate.dockingType = dockingType;
            candidate.quality = itemTemplate->Quality;
            candidate.inventoryType = itemTemplate->InventoryType;
            candidate.uniqueItem = uniqueItem;
            candidate.bagActivated = bagActivated;
            candidate.sourceMode = sourceMode;
            candidate.baseItemLevel = baseItemLevel == 0 ? static_cast<uint16>(itemTemplate->ItemLevel) : baseItemLevel;
            candidate.slotMask = slotMask;
            candidates.push_back(candidate);
        };

        bool isCacheBoss = bossEntry == chapter.cacheBossEntry;
        bool isAbyssBoss = bossEntry == chapter.abyssBossEntry;
        bool isAnchorOrFinal = bossEntry == chapter.anchorBossEntry || bossEntry == chapter.finalBossEntry;

        uint16 bestSourceChapter = 0;
        for (auto const& pair : _equipmentTemplates)
        {
            AbyssEquipmentTemplate const& equipment = pair.second;
            if (!equipment.enabled || equipment.actId != chapter.actId || equipment.sourceChapter > chapter.chapterId)
                continue;

            bool matches = false;
            if (isCacheBoss)
                matches = equipment.fromCacheBoss;
            else if (isAbyssBoss)
                matches = !equipment.fromCacheBoss;
            else if (isAnchorOrFinal)
                matches = equipment.equipmentType == 1 && equipment.sourceMode == 1;

            if (!matches)
                continue;

            if (bestSourceChapter < equipment.sourceChapter)
                bestSourceChapter = equipment.sourceChapter;
        }

        for (auto const& pair : _equipmentTemplates)
        {
            AbyssEquipmentTemplate const& equipment = pair.second;
            if (!equipment.enabled || equipment.actId != chapter.actId || equipment.sourceChapter != bestSourceChapter)
                continue;

            bool matches = false;
            if (isCacheBoss)
            {
                matches = equipment.fromCacheBoss && (equipment.sourceMode == 1 || equipment.sourceMode <= std::max<uint8>(modeType, 1u));
            }
            else if (isAbyssBoss)
            {
                matches = !equipment.fromCacheBoss && (equipment.sourceMode == 1 || equipment.sourceMode <= std::max<uint8>(modeType, 1u));
            }
            else if (isAnchorOrFinal)
            {
                matches = equipment.equipmentType == 1 && equipment.sourceMode == 1;
            }

            if (!matches)
                continue;

            appendCandidate(
                equipment.itemId,
                equipment.itemName,
                equipment.equipmentType == 2 ? 5u : 4u,
                equipment.equipmentType == 2,
                false,
                equipment.sourceMode,
                equipment.baseItemLevel,
                equipment.slotMask);
        }

        std::vector<AbyssItemDocking const*> itemDockings = GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
        {
            if (!docking)
                continue;

            if (docking->dockingType == 1 || docking->dockingType == 2 || docking->dockingType == 3)
                continue;

            if (isCacheBoss)
            {
                if (docking->dockingType == 5)
                    appendCandidate(docking->itemId, docking->itemName, docking->dockingType, docking->uniqueItem, docking->bagActivated, 0, 0, 0);
                continue;
            }

            if (isAbyssBoss)
            {
                if (docking->dockingType == 5 || docking->dockingType == 4)
                    appendCandidate(docking->itemId, docking->itemName, docking->dockingType, docking->uniqueItem, docking->bagActivated, 0, 0, 0);
                continue;
            }

            if (isAnchorOrFinal && docking->dockingType == 4)
                appendCandidate(docking->itemId, docking->itemName, docking->dockingType, docking->uniqueItem, docking->bagActivated, 0, 0, 0);
        }

        std::sort(candidates.begin(), candidates.end(), [](AbyssRewardCandidate const& left, AbyssRewardCandidate const& right)
        {
            if (left.quality != right.quality)
                return left.quality > right.quality;
            return left.itemId < right.itemId;
        });

        return candidates;
    }

    AbyssRewardCandidate const* RollRewardItemCandidate(std::vector<AbyssRewardCandidate> const& candidates, AbyssChapterConfig const& chapter, bool preferHighQuality, bool preferUnique, uint8 modeType, uint16 corruptionTier) const
    {
        if (candidates.empty())
            return nullptr;

        uint32 totalWeight = 0;
        std::vector<uint32> weights;
        weights.reserve(candidates.size());

        for (AbyssRewardCandidate const& candidate : candidates)
        {
            uint32 weight = 10 + static_cast<uint32>(candidate.quality) * 5;
            if (preferHighQuality && candidate.quality >= 4)
                weight *= 3;
            if (preferUnique && candidate.uniqueItem)
                weight *= 4;
            if (!preferUnique && candidate.dockingType == 4)
                weight += 5;
            if (modeType >= 3 && candidate.quality >= 4)
                weight *= 2;
            if (chapter.preferredSlotMask != 0 && candidate.slotMask != 0 && (candidate.slotMask & chapter.preferredSlotMask) != 0)
                weight *= 2;
            if (chapter.cacheBossRewardBonus > 0.0f && preferUnique && candidate.uniqueItem)
                weight = static_cast<uint32>(std::max<float>(1.0f, static_cast<float>(weight) * (1.0f + chapter.cacheBossRewardBonus)));
            if (candidate.baseItemLevel > 0)
                weight += std::max<uint32>(candidate.baseItemLevel / 30u, 1u);

            weights.push_back(weight);
            totalWeight += weight;
        }

        uint32 ticket = RollWeight(totalWeight);
        uint32 cursor = 0;
        for (size_t index = 0; index < candidates.size(); ++index)
        {
            cursor += weights[index];
            if (ticket < cursor)
                return &candidates[index];
        }

        return &candidates.back();
    }

    AbyssRewardResult GrantBossReward(Player* player, AbyssChapterConfig const& chapter, PlayerAbyssData& data, PlayerAbyssRunState const& runState, uint32 bossEntry)
    {
        AbyssRewardResult result;
        if (!player)
            return result;

        std::vector<AbyssRewardCandidate> candidates = GetRewardCandidatesForBoss(chapter, bossEntry, runState.modeType);
        if (candidates.empty())
            return result;

        bool preferUnique = bossEntry == chapter.cacheBossEntry;
        bool preferHighQuality = runState.modeType >= 3;

        if (bossEntry != chapter.cacheBossEntry)
        {
            float dropChance = GetChapterDropRate(chapter, runState.modeType);
            if (bossEntry == chapter.finalBossEntry)
                dropChance += chapter.hiddenRoomBonus * 100.0f;
            if (RollPercentage() >= dropChance)
            {
                if (bossEntry == chapter.abyssBossEntry)
                    ++data.abyssGearFailCount;
                return result;
            }
        }

        if (bossEntry == chapter.abyssBossEntry && chapter.abyssGearPityCount > 0 &&
            static_cast<uint32>(data.abyssGearFailCount + 1) >= chapter.abyssGearPityCount)
        {
            preferUnique = true;
        }

        AbyssRewardCandidate const* selected = RollRewardItemCandidate(candidates, chapter, preferHighQuality, preferUnique, runState.modeType, runState.corruptionTier);
        if (!selected)
            return result;

        result.itemId = selected->itemId;
        result.itemName = selected->itemName;
        result.dockingType = selected->dockingType;
        result.quality = selected->quality;
        result.sourceMode = selected->sourceMode;
        result.baseItemLevel = selected->baseItemLevel;
        result.bossEntry = bossEntry;
        result.rewardTime = GetNow();
        RollRewardSummary(chapter, *selected, result);
        result.awarded = true;
        result.inventoryStored = player->AddItem(selected->itemId, 1);
        result.countedAsAbyssGear = result.inventoryStored && (selected->dockingType == 5 || selected->inventoryType != 0);
        result.countedAsHighQuality = result.inventoryStored && selected->quality >= 4;
        result.countedAsChapterEffect = result.inventoryStored && (selected->bagActivated || selected->quality >= 5);

        if (result.countedAsAbyssGear)
            data.abyssGearFailCount = 0;
        else if (bossEntry == chapter.abyssBossEntry)
            ++data.abyssGearFailCount;

        if (player->GetSession())
        {
            ChatHandler handler(player->GetSession());
            handler.PSendSysMessage("Abyss reward {} item={} ({}) type={} quality={} stored={}",
                bossEntry == chapter.cacheBossEntry ? "cache" : (bossEntry == chapter.abyssBossEntry ? "abyss" : "chapter"),
                result.itemId,
                result.itemName,
                static_cast<uint32>(result.dockingType),
                static_cast<uint32>(result.quality),
                result.inventoryStored);
            handler.PSendSysMessage("  rewardMeta sourceMode={} baseIlvl={} preferredSlotMask={}",
                static_cast<uint32>(result.sourceMode),
                result.baseItemLevel,
                chapter.preferredSlotMask);
            handler.PSendSysMessage("  rewardRoll {}", result.rewardSummary);
        }

        return result;
    }

    bool NormalizePlayerData(uint32 guid, PlayerAbyssData& data) const
    {
        bool changed = false;

        if (data.storyState > 2)
        {
            data.storyState = 0;
            changed = true;
        }

        if (data.currentChapter != 0 && !GetChapterConfig(data.currentChapter))
        {
            data.currentChapter = 0;
            changed = true;
        }

        if (data.highestChapter != 0 && !GetChapterConfig(data.highestChapter))
        {
            data.highestChapter = 0;
            changed = true;
        }

        if (data.pityChapterId != 0 && !GetChapterConfig(data.pityChapterId))
        {
            data.pityChapterId = 0;
            changed = true;
        }

        if (data.currentChapter != 0)
        {
            AbyssChapterConfig const* chapterConfig = GetChapterConfig(data.currentChapter);
            if (chapterConfig && data.currentCultivationThreshold != chapterConfig->requiredCultivationLevel)
            {
                data.currentCultivationThreshold = chapterConfig->requiredCultivationLevel;
                changed = true;
            }

            if (data.highestChapter < data.currentChapter)
            {
                data.highestChapter = data.currentChapter;
                changed = true;
            }
        }
        else if (data.currentCultivationThreshold != 0)
        {
            data.currentCultivationThreshold = 0;
            changed = true;
        }

        changed = NormalizePlayerActiveRelics(data) || changed;
        changed = RefreshUnlockedModes(guid, data) || changed;

        return changed;
    }

    bool RefreshUnlockedModes(uint32 guid, PlayerAbyssData& data) const
    {
        uint32 modeMask = GetModeTypeMask(1);
        auto unlockItr = _playerChapterModeUnlocks.find(guid);
        if (unlockItr != _playerChapterModeUnlocks.end())
        {
            for (auto const& pair : unlockItr->second)
                modeMask |= pair.second.unlockedModeMask;
        }

        if (data.unlockedModeMask == modeMask)
            return false;

        data.unlockedModeMask = modeMask;
        return true;
    }

    bool NormalizePlayerRunState(PlayerAbyssRunState& state, PlayerAbyssData const* playerData) const
    {
        bool changed = false;

        if (state.currentChapterId == 0 || !GetChapterConfig(state.currentChapterId))
        {
            state = PlayerAbyssRunState();
            return true;
        }

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(state.currentChapterId);
        if (!chapterConfig)
        {
            state = PlayerAbyssRunState();
            return true;
        }

        if (state.modeType == 0 || state.modeType > 4)
        {
            state.modeType = 1;
            changed = true;
        }

        if (state.currentMapId == 0)
        {
            state.currentMapId = chapterConfig->mapId;
            changed = true;
        }

        if (state.startTime == 0)
        {
            state.startTime = GetNow();
            changed = true;
        }

        if (playerData)
        {
            if (state.runMainRelic == 0 && playerData->mainRelic != 0)
            {
                state.runMainRelic = playerData->mainRelic;
                changed = true;
            }

            if (state.runSubRelic1 == 0 && playerData->subRelic1 != 0)
            {
                state.runSubRelic1 = playerData->subRelic1;
                changed = true;
            }

            if (state.runSubRelic2 == 0 && playerData->subRelic2 != 0)
            {
                state.runSubRelic2 = playerData->subRelic2;
                changed = true;
            }

            if (state.runSubRelic3 == 0 && playerData->subRelic3 != 0)
            {
                state.runSubRelic3 = playerData->subRelic3;
                changed = true;
            }

            if (state.runSubRelic4 == 0 && playerData->subRelic4 != 0)
            {
                state.runSubRelic4 = playerData->subRelic4;
                changed = true;
            }

            if (state.runSubRelic5 == 0 && playerData->subRelic5 != 0)
            {
                state.runSubRelic5 = playerData->subRelic5;
                changed = true;
            }
        }

        return changed;
    }

    bool SetPlayerRelicSlot(Player* player, uint8 slot, uint32 itemId, std::string* failureReason)
    {
        if (!player)
        {
            if (failureReason)
                *failureReason = "no_player";
            return false;
        }

        if (GetPlayerRunState(player->GetGUID().GetCounter()))
        {
            if (failureReason)
                *failureReason = "active_run_locked";
            return false;
        }

        EnsurePlayerData(player);
        LoadPlayerCollections(player);

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
        {
            if (failureReason)
                *failureReason = "player_data_missing";
            return false;
        }

        PlayerAbyssData& data = itr->second;
        uint32* slotValue = nullptr;
        switch (slot)
        {
            case ABYSS_RELIC_SLOT_MAIN:
                slotValue = &data.mainRelic;
                break;
            case ABYSS_RELIC_SLOT_SUB_1:
                slotValue = &data.subRelic1;
                break;
            case ABYSS_RELIC_SLOT_SUB_2:
                slotValue = &data.subRelic2;
                break;
            case ABYSS_RELIC_SLOT_SUB_3:
                slotValue = &data.subRelic3;
                break;
            case ABYSS_RELIC_SLOT_SUB_4:
                slotValue = &data.subRelic4;
                break;
            case ABYSS_RELIC_SLOT_SUB_5:
                slotValue = &data.subRelic5;
                break;
            case ABYSS_RELIC_SLOT_PHASE:
                slotValue = &data.phaseArtifact;
                break;
            case ABYSS_RELIC_SLOT_ULTIMATE:
                slotValue = &data.ultimateArtifact;
                break;
            default:
                break;
        }

        if (!slotValue)
        {
            if (failureReason)
                *failureReason = "invalid_slot";
            return false;
        }

        if (itemId == 0)
        {
            if (*slotValue == 0)
            {
                if (failureReason)
                    *failureReason = "slot_empty";
                return false;
            }

            *slotValue = 0;
            NormalizePlayerData(guid, data);
            SavePlayerData(player);
            RefreshPlayerRuntimeStats(player);
            return true;
        }

        if (!PlayerOwnsCollectedRelic(guid, itemId))
        {
            if (failureReason)
                *failureReason = "relic_not_collected";
            return false;
        }

        AbyssRelicConfig const* relic = GetRelicConfig(itemId);
        if (!relic)
        {
            if (failureReason)
                *failureReason = "relic_not_configured";
            return false;
        }

        if (!IsRelicCompatibleWithSlot(*relic, slot))
        {
            if (failureReason)
                *failureReason = "slot_incompatible";
            return false;
        }

        auto clearIfMatches = [&](uint32& currentValue)
        {
            if (currentValue == itemId)
                currentValue = 0;
        };

        clearIfMatches(data.mainRelic);
        clearIfMatches(data.subRelic1);
        clearIfMatches(data.subRelic2);
        clearIfMatches(data.subRelic3);
        clearIfMatches(data.subRelic4);
        clearIfMatches(data.subRelic5);
        clearIfMatches(data.phaseArtifact);
        clearIfMatches(data.ultimateArtifact);

        auto hasExclusiveConflict = [&](uint32 currentValue) -> bool
        {
            if (currentValue == 0 || relic->exclusiveGroup == 0)
                return false;

            AbyssRelicConfig const* currentRelic = GetRelicConfig(currentValue);
            return currentRelic && currentRelic->exclusiveGroup == relic->exclusiveGroup;
        };

        if (hasExclusiveConflict(data.mainRelic) ||
            hasExclusiveConflict(data.subRelic1) ||
            hasExclusiveConflict(data.subRelic2) ||
            hasExclusiveConflict(data.subRelic3) ||
            hasExclusiveConflict(data.subRelic4) ||
            hasExclusiveConflict(data.subRelic5) ||
            hasExclusiveConflict(data.phaseArtifact) ||
            hasExclusiveConflict(data.ultimateArtifact))
        {
            if (failureReason)
                *failureReason = "exclusive_conflict";
            return false;
        }

        *slotValue = itemId;
        NormalizePlayerData(guid, data);
        SavePlayerData(player);
        RefreshPlayerRuntimeStats(player);
        return true;
    }

    void ClearPlayerProcState(uint32 guid)
    {
        _playerProcStates.erase(guid);
    }

    PlayerAbyssProcState& GetOrCreatePlayerProcState(uint32 guid)
    {
        return _playerProcStates[guid];
    }

    float GetActiveScriptGroupScale(Player* player, std::string const& scriptGroup) const
    {
        if (!player || scriptGroup.empty())
            return 0.0f;

        uint32 guid = player->GetGUID().GetCounter();
        PlayerAbyssRunState const* runState = GetPlayerRunState(guid);
        PlayerAbyssData const* playerData = GetPlayerData(guid);

        float bestScale = 0.0f;
        auto considerRelicGroup = [&](uint32 itemId, float scale)
        {
            if (itemId == 0)
                return;

            AbyssRelicConfig const* relic = GetRelicConfig(itemId);
            if (relic && relic->scriptGroup == scriptGroup)
                bestScale = std::max(bestScale, ClampRelicScale(scale));
        };

        if (runState)
        {
            considerRelicGroup(runState->runMainRelic, 1.0f);
            considerRelicGroup(runState->runSubRelic1, GetConfiguredSubRelicScale(runState->runSubRelic1));
            considerRelicGroup(runState->runSubRelic2, GetConfiguredSubRelicScale(runState->runSubRelic2));
            considerRelicGroup(runState->runSubRelic3, GetConfiguredSubRelicScale(runState->runSubRelic3));
            considerRelicGroup(runState->runSubRelic4, GetConfiguredSubRelicScale(runState->runSubRelic4));
            considerRelicGroup(runState->runSubRelic5, GetConfiguredSubRelicScale(runState->runSubRelic5));
        }

        if (playerData)
        {
            considerRelicGroup(playerData->phaseArtifact, 1.0f);
            considerRelicGroup(playerData->ultimateArtifact, 1.0f);
        }

        return bestScale;
    }

    float GetActiveScriptGroupScale(Player* player, std::initializer_list<char const*> scriptGroups) const
    {
        float bestScale = 0.0f;
        for (char const* scriptGroup : scriptGroups)
            bestScale = std::max(bestScale, GetActiveScriptGroupScale(player, scriptGroup ? std::string(scriptGroup) : std::string()));

        return bestScale;
    }

    bool HasActiveScriptGroup(Player* player, std::string const& scriptGroup) const
    {
        return GetActiveScriptGroupScale(player, scriptGroup) > 0.0f;
    }

    Unit* GetPrimaryCombatTarget(Player* player) const
    {
        if (!player)
            return nullptr;

        if (Unit* victim = player->GetVictim())
            return victim;

        if (Player* selected = player->GetSelectedPlayer())
            return selected;

        return player->SelectNearbyTarget(nullptr, 20.0f);
    }

    void DealConfiguredBurst(Player* player, Unit* target, uint32 minDamage, uint32 maxDamage, SpellSchoolMask schoolMask, float scale = 1.0f) const
    {
        if (!player || !target || !target->IsAlive() || scale <= 0.0f)
            return;

        uint32 scaledMinDamage = ScaleUIntValue(minDamage, scale);
        uint32 scaledMaxDamage = ScaleUIntValue(maxDamage, scale);
        if (scaledMaxDamage < scaledMinDamage)
            scaledMaxDamage = scaledMinDamage;

        uint32 damage = scaledMinDamage;
        if (scaledMaxDamage > scaledMinDamage)
            damage += RollWeight(scaledMaxDamage - scaledMinDamage + 1);

        player->DealDamage(player, target, damage, nullptr, SPELL_DIRECT_DAMAGE, schoolMask);
    }

    void TriggerFireRing(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 180, 260, SPELL_SCHOOL_MASK_FIRE, scale);
        if (Unit* secondaryTarget = player->SelectNearbyTarget(primaryTarget, 12.0f))
            DealConfiguredBurst(player, secondaryTarget, 120, 180, SPELL_SCHOOL_MASK_FIRE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 裂火爆环触发。");
    }

    void TriggerChainLightning(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 140, 220, SPELL_SCHOOL_MASK_NATURE, scale);
        if (Unit* bounce1 = player->SelectNearbyTarget(primaryTarget, 15.0f))
            DealConfiguredBurst(player, bounce1, 100, 160, SPELL_SCHOOL_MASK_NATURE, scale);
        if (Unit* bounce2 = player->SelectNearbyTarget(nullptr, 15.0f))
            if (bounce2 != primaryTarget)
                DealConfiguredBurst(player, bounce2, 80, 120, SPELL_SCHOOL_MASK_NATURE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 连锁雷击触发。");
    }

    void TriggerSoulBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(6)), scale));
        player->ModifyPower(player->getPowerType(), ScaleIntValue(20, scale));
        cooldownTime = GetNow();

        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 90, 140, SPELL_SCHOOL_MASK_SHADOW, scale);

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 魂火追击触发。");
    }

    void TriggerLowHealthRetaliation(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(12)), scale));
        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 锁链反打触发。");
    }

    void TriggerPoisonBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 110, 170, SPELL_SCHOOL_MASK_NATURE, scale);
        if (Unit* splash = player->SelectNearbyTarget(primaryTarget, 10.0f))
            DealConfiguredBurst(player, splash, 70, 110, SPELL_SCHOOL_MASK_NATURE, scale);

        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(3)), scale));
        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 蛇蜕毒爆触发。");
    }

    void TriggerBloodFlameBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 200, 300, SPELL_SCHOOL_MASK_FIRE, scale);
        if (Unit* splash = player->SelectNearbyTarget(primaryTarget, 12.0f))
            DealConfiguredBurst(player, splash, 120, 180, SPELL_SCHOOL_MASK_FIRE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 血焰审判触发。");
    }

    void TriggerFeatherVolley(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 130, 190, SPELL_SCHOOL_MASK_ARCANE, scale);
        if (Unit* extra1 = player->SelectNearbyTarget(primaryTarget, 15.0f))
            DealConfiguredBurst(player, extra1, 90, 140, SPELL_SCHOOL_MASK_ARCANE, scale);
        if (Unit* extra2 = player->SelectNearbyTarget(nullptr, 15.0f))
            if (extra2 != primaryTarget)
                DealConfiguredBurst(player, extra2, 90, 140, SPELL_SCHOOL_MASK_ARCANE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 命羽齐射触发。");
    }

    void TriggerStarfallBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 220, 320, SPELL_SCHOOL_MASK_ARCANE, scale);
        if (Unit* splash = player->SelectNearbyTarget(primaryTarget, 15.0f))
            DealConfiguredBurst(player, splash, 120, 180, SPELL_SCHOOL_MASK_ARCANE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 星界坠临触发。");
    }

    void TriggerTideBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 150, 230, SPELL_SCHOOL_MASK_FROST, scale);
        if (Unit* bounce = player->SelectNearbyTarget(primaryTarget, 14.0f))
            DealConfiguredBurst(player, bounce, 100, 150, SPELL_SCHOOL_MASK_FROST, scale);

        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(4)), scale));
        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 潮汐回流触发。");
    }

    void TriggerBloomBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* target = GetPrimaryCombatTarget(player);
        if (!target)
            return;

        DealConfiguredBurst(player, target, 140, 210, SPELL_SCHOOL_MASK_NATURE, scale);
        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(5)), scale));

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 古树/腐花协战触发。");
    }

    void TriggerMirrorEcho(Player* player, uint32 spellId, uint32& cooldownTime, char const* label, float scale = 1.0f) const
    {
        if (!player || spellId == 0 || scale <= 0.0f)
            return;

        if (scale < 0.999f && RollPercentage() >= (scale * 100.0f))
            return;

        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        if (procState.replayingSpell)
            return;

        Unit* target = GetPrimaryCombatTarget(player);
        procState.replayingSpell = true;
        player->CastSpell(target ? target : player, spellId, true);
        procState.replayingSpell = false;
        cooldownTime = GetNow();

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] {}触发。", label);
    }

    void TriggerSwarmBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 160, 240, SPELL_SCHOOL_MASK_NATURE, scale);
        if (Unit* extra = player->SelectNearbyTarget(primaryTarget, 10.0f))
            DealConfiguredBurst(player, extra, 90, 140, SPELL_SCHOOL_MASK_NATURE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 虫群/孢囊爆裂触发。");
    }

    void TriggerBattleBannerBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* target = GetPrimaryCombatTarget(player);
        if (!target)
            return;

        DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);
        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(3)), scale));
        cooldownTime = GetNow();

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 守望战旌触发。");
    }

    void TriggerRedJadeBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* target = GetPrimaryCombatTarget(player);
        if (!target)
            return;

        DealConfiguredBurst(player, target, 240, 340, SPELL_SCHOOL_MASK_FIRE, scale);
        if (Unit* splash = player->SelectNearbyTarget(target, 18.0f))
            DealConfiguredBurst(player, splash, 140, 200, SPELL_SCHOOL_MASK_FIRE, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 赤玉终焉触发。");
    }

    void TriggerDreamBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 150, 220, SPELL_SCHOOL_MASK_SHADOW, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 梦沼暗爆触发。");
    }

    void TriggerHolyVerdict(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(18)), scale));
        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_NORMAL, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 圣陨终裁触发。");
    }

    void TriggerDominionBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 260, 360, SPELL_SCHOOL_MASK_FROST, scale);

        cooldownTime = GetNow();
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssEffect] 霜王统御触发。");
    }

    bool EnsurePlayerRunLocation(Player* player)
    {
        if (!player)
            return false;

        PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter());
        if (!runState || runState->currentMapId == 0)
            return false;

        if (player->GetMapId() == runState->currentMapId)
            return false;

        uint16 previousMapId = runState->currentMapId;
        EndPlayerRun(player);
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("Abyss run ended because player left chapter map {}.", previousMapId);
        return true;
    }

    void HandlePlayerSpellCast(Player* player, Spell* spell)
    {
        if (!player || !spell)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        if (!GetPlayerRunState(guid))
            return;

        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(guid);
        if (procState.replayingSpell)
            return;

        procState.lastSpellId = spell->GetSpellInfo()->Id;
        procState.lastSpellCastTime = GetNow();

        float chaseBladeScale = GetActiveScriptGroupScale(player, { "遗物_黑潮齿轮", "祝福_追命飞刃" });
        if (chaseBladeScale > 0.0f)
        {
            if (Unit* target = GetPrimaryCombatTarget(player))
                DealConfiguredBurst(player, target, 100, 160, SPELL_SCHOOL_MASK_ARCANE, chaseBladeScale);
        }

        float chainLightningScale = GetActiveScriptGroupScale(player, { "遗物_爆线电枢", "祝福_雷暴连极" });
        if (chainLightningScale > 0.0f)
        {
            if (GetNow() > procState.lastChainLightningTime + 2)
                TriggerChainLightning(player, procState.lastChainLightningTime, chainLightningScale);
        }

        float poisonScale = GetActiveScriptGroupScale(player, "遗物_蛇蜕古胆");
        if (poisonScale > 0.0f)
        {
            ++procState.poisonCastCounter;
            if (procState.poisonCastCounter >= 3 && GetNow() > procState.lastPoisonBurstTime + 3)
            {
                procState.poisonCastCounter = 0;
                TriggerPoisonBurst(player, procState.lastPoisonBurstTime, poisonScale);
            }
        }

        float bloodFlameScale = GetActiveScriptGroupScale(player, { "遗物_血焰圣经", "祝福_日蚀焚城", "特效_血焰审判" });
        if (bloodFlameScale > 0.0f)
        {
            if (procState.lastComboSpellId != spell->GetSpellInfo()->Id)
            {
                procState.lastComboSpellId = spell->GetSpellInfo()->Id;
                ++procState.bloodFlameComboCounter;
            }

            if (procState.bloodFlameComboCounter >= 4 && GetNow() > procState.lastBloodFlameTime + 5)
            {
                procState.bloodFlameComboCounter = 0;
                TriggerBloodFlameBurst(player, procState.lastBloodFlameTime, bloodFlameScale);
            }
        }

        float battlePrayerScale = GetActiveScriptGroupScale(player, "遗物_维库战祷");
        if (battlePrayerScale > 0.0f)
        {
            player->ModifyHealth(ScaleIntValue(static_cast<int32>(player->CountPctFromMaxHealth(2)), battlePrayerScale));
            if (Unit* target = GetPrimaryCombatTarget(player))
                DealConfiguredBurst(player, target, 60, 100, SPELL_SCHOOL_MASK_NORMAL, battlePrayerScale);
        }

        float starfallScale = GetActiveScriptGroupScale(player, "遗物_星界虹膜");
        if (starfallScale > 0.0f)
        {
            ++procState.starfallCounter;
            if (procState.starfallCounter >= 5 && GetNow() > procState.lastStarfallTime + 6)
            {
                procState.starfallCounter = 0;
                TriggerStarfallBurst(player, procState.lastStarfallTime, starfallScale);
            }
        }

        float tideScale = GetActiveScriptGroupScale(player, { "遗物_深海祈眼", "遗物_潮蛇王鳞" });
        if (tideScale > 0.0f)
        {
            ++procState.tideCastCounter;
            if (procState.tideCastCounter >= 5 && GetNow() > procState.lastTideBurstTime + 4)
            {
                procState.tideCastCounter = 0;
                TriggerTideBurst(player, procState.lastTideBurstTime, tideScale);
            }
        }

        float bloomScale = GetActiveScriptGroupScale(player, "遗物_古树孢祖");
        if (bloomScale > 0.0f)
        {
            ++procState.bloomCastCounter;
            if (procState.bloomCastCounter >= 4 && GetNow() > procState.lastBloomBurstTime + 5)
            {
                procState.bloomCastCounter = 0;
                TriggerBloomBurst(player, procState.lastBloomBurstTime, bloomScale);
            }
        }

        float mirrorScale = GetActiveScriptGroupScale(player, { "遗物_伊利影印", "遗物_映像碎镜" });
        if (mirrorScale > 0.0f)
        {
            if (procState.lastSpellId != 0 && GetNow() > procState.lastMirrorEchoTime + 8)
                TriggerMirrorEcho(player, procState.lastSpellId, procState.lastMirrorEchoTime, "影印残像", mirrorScale);
        }

        float swarmScale = GetActiveScriptGroupScale(player, "遗物_虫群主脑");
        if (swarmScale > 0.0f)
        {
            if (GetNow() > procState.lastSwarmBurstTime + 7)
                TriggerSwarmBurst(player, procState.lastSwarmBurstTime, swarmScale);
        }

        float dreamScale = GetActiveScriptGroupScale(player, "遗物_梦沼眼膜");
        if (dreamScale > 0.0f)
        {
            if (GetNow() > procState.lastDreamBurstTime + 8)
                TriggerDreamBurst(player, procState.lastDreamBurstTime, dreamScale);
        }

        float beastScale = GetActiveScriptGroupScale(player, "遗物_神噬断爪");
        if (beastScale > 0.0f)
        {
            procState.beastMode = static_cast<uint8>((procState.beastMode + 1) % 3);
            if (Unit* target = GetPrimaryCombatTarget(player))
            {
                uint32 minDamage = procState.beastMode == 0 ? 90u : (procState.beastMode == 1 ? 110u : 130u);
                uint32 maxDamage = procState.beastMode == 0 ? 140u : (procState.beastMode == 1 ? 170u : 200u);
                DealConfiguredBurst(player, target, minDamage, maxDamage, SPELL_SCHOOL_MASK_NORMAL, beastScale);
            }
        }

        float matrixScale = GetActiveScriptGroupScale(player, { "遗物_虚空矩阵", "祝福_双生元婴" });
        if (matrixScale > 0.0f)
        {
            ++procState.matrixCastCounter;
            if (procState.matrixCastCounter >= 4 && procState.lastSpellId != 0 && GetNow() > procState.lastMatrixEchoTime + 10)
            {
                procState.matrixCastCounter = 0;
                TriggerMirrorEcho(player, procState.lastSpellId, procState.lastMatrixEchoTime, "虚空矩阵复写", matrixScale);
                TriggerMirrorEcho(player, procState.lastSpellId, procState.lastMatrixEchoTime, "虚空矩阵复写", matrixScale);
            }
        }
    }

    void HandlePlayerUpdate(Player* player, uint32 diff)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        if (!GetPlayerRunState(guid))
            return;

        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(guid);
        uint32 now = GetNow();

        float lowHealthScale = GetActiveScriptGroupScale(player, { "遗物_断罪枷锁", "特效_锁链爆裂", "遗物_圣陨判词" });
        if (lowHealthScale > 0.0f &&
            player->HealthBelowPct(35) && now > procState.lastLowHealthProcTime + 15)
        {
            TriggerLowHealthRetaliation(player, procState.lastLowHealthProcTime, lowHealthScale);
        }

        float holyVerdictScale = GetActiveScriptGroupScale(player, { "遗物_圣陨判词", "祝福_断命法旨", "特效_圣陨终裁" });
        if (holyVerdictScale > 0.0f &&
            player->HealthBelowPct(25) && now > procState.lastHolyVerdictTime + 30)
        {
            TriggerHolyVerdict(player, procState.lastHolyVerdictTime, holyVerdictScale);
        }

        float replayScale = GetActiveScriptGroupScale(player, { "遗物_泰坦偏轴", "祝福_逆时回响", "特效_时痕回放" });
        if (replayScale > 0.0f &&
            procState.lastSpellId != 0 && now > procState.lastSpellCastTime + 10 && now > procState.lastReplayTime + 10)
        {
            TriggerMirrorEcho(player, procState.lastSpellId, procState.lastReplayTime, "时痕回放", replayScale);
        }

        float bannerScale = GetActiveScriptGroupScale(player, "遗物_守望战旌");
        if (bannerScale > 0.0f && now > procState.lastBattleBannerTime + 20)
            TriggerBattleBannerBurst(player, procState.lastBattleBannerTime, bannerScale);

        float redJadeScale = GetActiveScriptGroupScale(player, "遗物_赤玉界针");
        if (redJadeScale > 0.0f && now > procState.lastRedJadeTime + 12)
            TriggerRedJadeBurst(player, procState.lastRedJadeTime, redJadeScale);

        float dominionScale = GetActiveScriptGroupScale(player, { "遗物_霜王残印", "祝福_极霜粉碎", "特效_霜王统御" });
        if (dominionScale > 0.0f &&
            procState.dominionCounter >= 6 && now > procState.lastDominionTime + 18)
        {
            procState.dominionCounter = 0;
            TriggerDominionBurst(player, procState.lastDominionTime, dominionScale);
        }

        (void)diff;
    }

    bool TryAutoBeginPlayerRun(Player* player)
    {
        if (!player || !player->IsInWorld() || !_worldDataLoaded)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        if (GetPlayerRunState(guid))
            return false;

        PlayerAbyssData const* playerData = GetPlayerData(guid);
        if (!playerData || playerData->currentChapter == 0)
            return false;

        AbyssChapterConfig const* chapter = GetChapterConfig(playerData->currentChapter);
        if (!chapter || chapter->mapId == 0)
            return false;

        if (player->GetMapId() != chapter->mapId)
            return false;

        if (Map* map = player->GetMap())
        {
            if (!map->IsDungeon() && !map->IsRaid())
                return false;
        }

        std::string failureReason;
        if (!BeginPlayerRun(player, chapter->chapterId, 1, 0, &failureReason))
            return false;

        if (player->GetSession())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Abyss run auto-started for chapter {} ({}) on map {}.",
                chapter->chapterId,
                chapter->chapterName,
                chapter->mapId);
        }

        return true;
    }

private:
    enum RelicRuntimeAttribute : uint8
    {
        RELIC_ATTR_AGILITY = 1,
        RELIC_ATTR_STRENGTH = 2,
        RELIC_ATTR_INTELLECT = 3,
        RELIC_ATTR_SPIRIT = 4,
        RELIC_ATTR_STAMINA = 5,
        RELIC_ATTR_HIT_RATING = 6,
        RELIC_ATTR_CRIT_RATING = 7,
        RELIC_ATTR_HASTE_RATING = 8,
        RELIC_ATTR_ATTACK_POWER = 9,
        RELIC_ATTR_SPELL_POWER = 10
    };

    uint32 GetRelicWeightTotal(AbyssRelicConfig const& relic) const
    {
        return relic.agilityWeight + relic.strengthWeight + relic.intellectWeight + relic.spiritWeight + relic.staminaWeight +
            relic.hitRatingWeight + relic.critRatingWeight + relic.hasteRatingWeight + relic.attackPowerWeight + relic.spellPowerWeight;
    }

    uint32 GetRelicAttributeWeight(AbyssRelicConfig const& relic, RelicRuntimeAttribute attribute) const
    {
        switch (attribute)
        {
            case RELIC_ATTR_AGILITY: return relic.agilityWeight;
            case RELIC_ATTR_STRENGTH: return relic.strengthWeight;
            case RELIC_ATTR_INTELLECT: return relic.intellectWeight;
            case RELIC_ATTR_SPIRIT: return relic.spiritWeight;
            case RELIC_ATTR_STAMINA: return relic.staminaWeight;
            case RELIC_ATTR_HIT_RATING: return relic.hitRatingWeight;
            case RELIC_ATTR_CRIT_RATING: return relic.critRatingWeight;
            case RELIC_ATTR_HASTE_RATING: return relic.hasteRatingWeight;
            case RELIC_ATTR_ATTACK_POWER: return relic.attackPowerWeight;
            case RELIC_ATTR_SPELL_POWER: return relic.spellPowerWeight;
            default: return 0;
        }
    }

    float GetRelicSlotScale(AbyssRelicConfig const& relic, uint8 slot) const
    {
        switch (slot)
        {
            case ABYSS_RELIC_SLOT_MAIN:
                return 1.0f;
            case ABYSS_RELIC_SLOT_SUB_1:
            case ABYSS_RELIC_SLOT_SUB_2:
            case ABYSS_RELIC_SLOT_SUB_3:
            case ABYSS_RELIC_SLOT_SUB_4:
            case ABYSS_RELIC_SLOT_SUB_5:
                return ClampRelicScale(relic.subSlotScale > 0.0f ? relic.subSlotScale : 0.5f);
            case ABYSS_RELIC_SLOT_PHASE:
                return 1.25f;
            case ABYSS_RELIC_SLOT_ULTIMATE:
                return 1.60f;
            default:
                return 0.0f;
        }
    }

    float GetRelicBaseBudget(AbyssRelicConfig const& relic) const
    {
        switch (relic.relicType)
        {
            case 1:
                return 12.0f + static_cast<float>(relic.actId) * 4.0f + static_cast<float>(std::min<uint16>(relic.relatedChapterId, 74)) * 0.50f;
            case 2:
                return 48.0f + static_cast<float>(relic.actId) * 8.0f;
            case 3:
                return 96.0f + static_cast<float>(relic.actId) * 10.0f;
            default:
                return 0.0f;
        }
    }

    float GetRelicModeScale(Player* player) const
    {
        if (!player)
            return 1.0f;

        PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter());
        if (!runState)
            return 1.0f;

        switch (runState->modeType)
        {
            case 2: return 1.10f;
            case 3: return 1.25f;
            case 4: return 1.40f;
            default: return 1.0f;
        }
    }

    int32 GetRelicAttributeContribution(AbyssRelicConfig const& relic, uint8 slot, RelicRuntimeAttribute attribute, float modeScale) const
    {
        uint32 totalWeight = GetRelicWeightTotal(relic);
        uint32 weight = GetRelicAttributeWeight(relic, attribute);
        if (totalWeight == 0 || weight == 0)
            return 0;

        float slotScale = GetRelicSlotScale(relic, slot);
        if (slotScale <= 0.0f)
            return 0;

        float budget = GetRelicBaseBudget(relic) * slotScale * std::max(modeScale, 0.1f);
        float value = budget * (static_cast<float>(weight) / static_cast<float>(totalWeight));
        return static_cast<int32>(std::lround(value));
    }

    int32 GetPlayerRuntimeRelicAttributeBonus(Player* player, RelicRuntimeAttribute attribute) const
    {
        if (!player)
            return 0;

        PlayerAbyssData const* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return 0;

        std::array<std::pair<uint8, uint32>, 8> slots =
        {{
            { ABYSS_RELIC_SLOT_MAIN, data->mainRelic },
            { ABYSS_RELIC_SLOT_SUB_1, data->subRelic1 },
            { ABYSS_RELIC_SLOT_SUB_2, data->subRelic2 },
            { ABYSS_RELIC_SLOT_SUB_3, data->subRelic3 },
            { ABYSS_RELIC_SLOT_SUB_4, data->subRelic4 },
            { ABYSS_RELIC_SLOT_SUB_5, data->subRelic5 },
            { ABYSS_RELIC_SLOT_PHASE, data->phaseArtifact },
            { ABYSS_RELIC_SLOT_ULTIMATE, data->ultimateArtifact }
        }};

        float modeScale = GetRelicModeScale(player);
        int32 total = 0;
        for (auto const& slotEntry : slots)
        {
            if (slotEntry.second == 0)
                continue;

            AbyssRelicConfig const* relic = GetRelicConfig(slotEntry.second);
            if (!relic)
                continue;

            total += GetRelicAttributeContribution(*relic, slotEntry.first, attribute, modeScale);
        }

        return total;
    }

    float GetPlayerRuntimeProgressionBonusPct(Player* player) const
    {
        if (!player)
            return 0.0f;

        PlayerAbyssData const* data = GetPlayerData(player->GetGUID().GetCounter());
        if (!data)
            return 0.0f;

        float bonus = 0.0f;
        if (data->mainRelic != 0)
            bonus += 2.0f;
        if (data->subRelic1 != 0)
            bonus += 2.0f * GetConfiguredSubRelicScale(data->subRelic1);
        if (data->subRelic2 != 0)
            bonus += 2.0f * GetConfiguredSubRelicScale(data->subRelic2);
        if (data->subRelic3 != 0)
            bonus += 2.0f * GetConfiguredSubRelicScale(data->subRelic3);
        if (data->subRelic4 != 0)
            bonus += 2.0f * GetConfiguredSubRelicScale(data->subRelic4);
        if (data->subRelic5 != 0)
            bonus += 2.0f * GetConfiguredSubRelicScale(data->subRelic5);
        if (data->phaseArtifact != 0)
            bonus += 5.0f;
        if (data->ultimateArtifact != 0)
            bonus += 10.0f;
        if (data->highestChapter >= 74)
            bonus += 5.0f;

        return bonus;
    }

    float GetPlayerRuntimeRunBonusPct(Player* player) const
    {
        if (!player)
            return 0.0f;

        PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter());
        if (!runState)
            return 0.0f;

        float bonus = 0.0f;
        switch (runState->modeType)
        {
            case 2:
                bonus += 6.0f;
                break;
            case 3:
                bonus += 12.0f;
                break;
            case 4:
                bonus += 18.0f;
                break;
            default:
                break;
        }

        bonus += std::min<float>(runState->corruptionTier * 1.0f, 25.0f);

        if (runState->runMainRelic != 0)
            bonus += 3.0f;
        if (runState->runSubRelic1 != 0)
            bonus += 3.0f * GetConfiguredSubRelicScale(runState->runSubRelic1);
        if (runState->runSubRelic2 != 0)
            bonus += 3.0f * GetConfiguredSubRelicScale(runState->runSubRelic2);
        if (runState->runSubRelic3 != 0)
            bonus += 3.0f * GetConfiguredSubRelicScale(runState->runSubRelic3);
        if (runState->runSubRelic4 != 0)
            bonus += 3.0f * GetConfiguredSubRelicScale(runState->runSubRelic4);
        if (runState->runSubRelic5 != 0)
            bonus += 3.0f * GetConfiguredSubRelicScale(runState->runSubRelic5);

        return bonus;
    }

public:

    int32 GetPlayerRuntimeRelicStatFlatBonus(Player* player, Stats stat) const
    {
        switch (stat)
        {
            case STAT_AGILITY: return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_AGILITY);
            case STAT_STRENGTH: return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_STRENGTH);
            case STAT_INTELLECT: return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_INTELLECT);
            case STAT_SPIRIT: return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_SPIRIT);
            case STAT_STAMINA: return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_STAMINA);
            default: return 0;
        }
    }

    int32 GetPlayerRuntimeRelicRatingBonus(Player* player, CombatRating cr) const
    {
        switch (cr)
        {
            case CR_HIT_MELEE:
            case CR_HIT_RANGED:
            case CR_HIT_SPELL:
                return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_HIT_RATING);
            case CR_CRIT_MELEE:
            case CR_CRIT_RANGED:
            case CR_CRIT_SPELL:
                return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_CRIT_RATING);
            case CR_HASTE_MELEE:
            case CR_HASTE_RANGED:
            case CR_HASTE_SPELL:
                return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_HASTE_RATING);
            default:
                return 0;
        }
    }

    int32 GetPlayerRuntimeRelicAttackPowerBonus(Player* player) const
    {
        return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_ATTACK_POWER);
    }

    int32 GetPlayerRuntimeRelicSpellPowerBonus(Player* player) const
    {
        return GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_SPELL_POWER);
    }

    float GetPlayerRuntimeStatBonusPct(Player* player) const
    {
        return GetPlayerRuntimeProgressionBonusPct(player) + GetPlayerRuntimeRunBonusPct(player);
    }

    float GetPlayerRuntimeHealthBonusPct(Player* player) const
    {
        return GetPlayerRuntimeProgressionBonusPct(player) + (GetPlayerRuntimeRunBonusPct(player) * 1.15f);
    }

    float GetPlayerRuntimeArmorBonusPct(Player* player) const
    {
        return (GetPlayerRuntimeProgressionBonusPct(player) * 0.75f) + (GetPlayerRuntimeRunBonusPct(player) * 1.25f);
    }

    float GetPlayerRuntimeAttackBonusPct(Player* player) const
    {
        return (GetPlayerRuntimeProgressionBonusPct(player) * 0.8f) + (GetPlayerRuntimeRunBonusPct(player) * 1.35f);
    }

    float GetPlayerRuntimeSpellBonusPct(Player* player) const
    {
        return (GetPlayerRuntimeProgressionBonusPct(player) * 0.9f) + (GetPlayerRuntimeRunBonusPct(player) * 1.25f);
    }

    float GetPlayerRuntimeXpBonusPct(Player* player) const
    {
        if (!player)
            return 0.0f;

        PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter());
        if (!runState)
            return 0.0f;

        float xpBonus = 0.0f;
        switch (runState->modeType)
        {
            case 2:
                xpBonus += 10.0f;
                break;
            case 3:
                xpBonus += 22.0f;
                break;
            case 4:
                xpBonus += 40.0f;
                break;
            default:
                break;
        }

        xpBonus += std::min<float>(runState->corruptionTier * 1.5f, 30.0f);

        return xpBonus;
    }

    void RefreshPlayerRuntimeStats(Player* player) const
    {
        if (!player)
            return;

        player->UpdateAllStats();
        player->UpdateAllRatings();
    }

    // 套装系统：检测玩家穿戴的底材件数并施加/移除buff
    void RefreshPlayerSetBonuses(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();

        // 统计每个套装ID穿了几件
        std::unordered_map<uint32, uint8> setCounts;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            auto itr = _equipmentTemplates.find(item->GetEntry());
            if (itr == _equipmentTemplates.end())
                continue;

            if (itr->second.setId == 0)
                continue;

            setCounts[itr->second.setId]++;
        }

        auto& oldStates = _playerSetBonusStates[guid];

        // 移除不再满足条件的套装buff
        for (auto itr = oldStates.begin(); itr != oldStates.end(); )
        {
            uint32 setId = itr->first;
            PlayerSetBonusState& state = itr->second;
            uint8 newCount = 0;
            auto countItr = setCounts.find(setId);
            if (countItr != setCounts.end())
                newCount = countItr->second;

            bool shouldHaveTwo = newCount >= 2;
            bool shouldHaveFour = newCount >= 4;

            if (state.hasTwoPieceBonus && !shouldHaveTwo)
            {
                RemoveSetBonusAura(player, setId, 2);
                state.hasTwoPieceBonus = false;
            }
            if (state.hasFourPieceBonus && !shouldHaveFour)
            {
                RemoveSetBonusAura(player, setId, 4);
                state.hasFourPieceBonus = false;
            }

            state.pieceCount = newCount;
            if (newCount == 0)
                itr = oldStates.erase(itr);
            else
                ++itr;
        }

        // 添加新满足条件的套装buff
        for (auto& [setId, count] : setCounts)
        {
            auto& state = oldStates[setId];
            state.setId = setId;
            state.pieceCount = count;

            if (count >= 2 && !state.hasTwoPieceBonus)
            {
                ApplySetBonusAura(player, setId, 2);
                state.hasTwoPieceBonus = true;
            }
            if (count >= 4 && !state.hasFourPieceBonus)
            {
                ApplySetBonusAura(player, setId, 4);
                state.hasFourPieceBonus = true;
            }
        }
    }

    void CleanupPlayerSetBonuses(uint32 guid)
    {
        _playerSetBonusStates.erase(guid);
    }

    AbyssSetBonusConfig const* GetSetBonusConfig(uint32 setId) const
    {
        auto itr = _setBonusConfigs.find(setId);
        return itr != _setBonusConfigs.end() ? &itr->second : nullptr;
    }

private:
    // 应用套装buff：通过修改玩家属性实现
    void ApplySetBonusAura(Player* player, uint32 setId, uint8 tier)
    {
        AbyssSetBonusConfig const* config = GetSetBonusConfig(setId);
        if (!config)
            return;

        if (tier == 2)
        {
            // 2件效果：直接加属性
            if (config->twoPieceAllStat > 0)
            {
                player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(config->twoPieceAllStat), true);
                player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(config->twoPieceAllStat), true);
                player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(config->twoPieceAllStat), true);
                player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(config->twoPieceAllStat), true);
                player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(config->twoPieceAllStat), true);
            }
            if (config->twoPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->twoPieceCrit, true);
            if (config->twoPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->twoPieceHaste, true);
            if (config->twoPieceAP > 0)
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(config->twoPieceAP), true);
            if (config->twoPieceSP > 0)
                player->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, float(config->twoPieceSP), true);

            ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[套装] 已激活 %s (2件)：%s|r",
                config->setName.c_str(), config->twoPieceDesc.c_str());
        }
        else if (tier == 4)
        {
            // 4件效果：加属性 + 特殊效果标记
            if (config->fourPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->fourPieceCrit, true);
            if (config->fourPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->fourPieceHaste, true);
            if (config->fourPieceHpPct > 0)
            {
                float bonus = player->GetMaxHealth() * config->fourPieceHpPct / 100.0f;
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, true);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000[套装] 已激活 %s (4件)：%s|r",
                config->setName.c_str(), config->fourPieceDesc.c_str());
        }

        player->UpdateAllStats();
        player->UpdateAllRatings();
    }

    void RemoveSetBonusAura(Player* player, uint32 setId, uint8 tier)
    {
        AbyssSetBonusConfig const* config = GetSetBonusConfig(setId);
        if (!config)
            return;

        if (tier == 2)
        {
            if (config->twoPieceAllStat > 0)
            {
                player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(config->twoPieceAllStat), false);
                player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(config->twoPieceAllStat), false);
                player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(config->twoPieceAllStat), false);
                player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(config->twoPieceAllStat), false);
                player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(config->twoPieceAllStat), false);
            }
            if (config->twoPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->twoPieceCrit, false);
            if (config->twoPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->twoPieceHaste, false);
            if (config->twoPieceAP > 0)
                player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(config->twoPieceAP), false);
            if (config->twoPieceSP > 0)
                player->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, float(config->twoPieceSP), false);

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[套装] 已失去 %s (2件) 效果|r",
                config->setName.c_str());
        }
        else if (tier == 4)
        {
            if (config->fourPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->fourPieceCrit, false);
            if (config->fourPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->fourPieceHaste, false);
            if (config->fourPieceHpPct > 0)
            {
                float bonus = player->GetMaxHealth() * config->fourPieceHpPct / 100.0f;
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, false);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[套装] 已失去 %s (4件) 效果|r",
                config->setName.c_str());
        }

        player->UpdateAllStats();
        player->UpdateAllRatings();
    }

public:
    float GetBossModeModifier(AbyssBossConfig const& config, uint8 modeType) const
    {
        switch (modeType)
        {
            case 2: return config.abyssModifier;
            case 3: return config.corruptionModifier;
            case 4: return config.reincarnationModifier;
            default: return config.storyModifier;
        }
    }

    bool ApplyBossRuntimeTuning(Creature* creature) const
    {
        if (!creature)
            return false;

        AbyssBossConfig const* bossConfig = GetBossConfig(creature->GetEntry());
        if (!bossConfig)
            return false;

        if (!creature->IsSummon())
            return false;

        float modeModifier = bossConfig->storyModifier;
        uint16 corruptionTier = 0;

        if (TempSummon* summon = creature->ToTempSummon())
        {
            if (Unit* summoner = summon->GetSummonerUnit())
            {
                if (Player* player = summoner->ToPlayer())
                {
                    if (PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter()))
                    {
                        if (runState->currentChapterId == bossConfig->chapterId && runState->modeType >= 2)
                        {
                            modeModifier = GetBossModeModifier(*bossConfig, runState->modeType);
                            corruptionTier = runState->corruptionTier;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        float healthScale = std::max(1.0f, bossConfig->healthModifier * modeModifier * (1.0f + static_cast<float>(corruptionTier) * 0.03f));
        float scale = 1.0f + ((healthScale - 1.0f) * 0.10f);
        scale = std::min(std::max(scale, 1.0f), 2.0f);

        uint32 baseHealth = creature->GetMaxHealth();
        if (baseHealth > 0)
        {
            uint32 tunedHealth = static_cast<uint32>(static_cast<float>(baseHealth) * healthScale);
            tunedHealth = std::max<uint32>(tunedHealth, baseHealth);
            creature->SetMaxHealth(tunedHealth);
            creature->SetHealth(tunedHealth);
        }

        creature->SetObjectScale(scale);
        return true;
    }

    uint64 GetTrackedBossMaskBit(AbyssChapterConfig const& chapter, uint32 creatureEntry) const
    {
        if (creatureEntry == 0)
            return 0;

        if (creatureEntry == chapter.anchorBossEntry)
            return 1ULL << 0;

        if (creatureEntry == chapter.finalBossEntry && creatureEntry != chapter.anchorBossEntry)
            return 1ULL << 1;

        if (creatureEntry == chapter.abyssBossEntry)
            return 1ULL << 2;

        if (creatureEntry == chapter.cacheBossEntry)
            return 1ULL << 3;

        return 0;
    }

    bool IsAbyssSummonReady(AbyssChapterConfig const& chapter, PlayerAbyssRunState const& state) const
    {
        if (chapter.abyssBossEntry == 0 || state.abyssBossSummoned)
            return false;

        if (chapter.triggerType == 1)
            return (state.anchorBossKillMask & (1ULL << 0)) != 0;

        if (chapter.requiredBossKillMask != 0)
            return (state.anchorBossKillMask & chapter.requiredBossKillMask) == chapter.requiredBossKillMask;

        return state.anchorBossKillMask != 0;
    }

    bool TrySummonBoss(Player* player, AbyssChapterConfig const& chapter, uint32 bossEntry, bool useConfiguredPosition) const
    {
        if (!player || bossEntry == 0)
            return false;

        float x = player->GetPositionX();
        float y = player->GetPositionY();
        float z = player->GetPositionZ();
        float o = player->GetOrientation();

        if (useConfiguredPosition &&
            chapter.abyssSummonMapId != 0 &&
            player->GetMapId() == chapter.abyssSummonMapId &&
            (chapter.abyssSummonX != 0.0f || chapter.abyssSummonY != 0.0f || chapter.abyssSummonZ != 0.0f))
        {
            x = chapter.abyssSummonX;
            y = chapter.abyssSummonY;
            z = chapter.abyssSummonZ;
            o = chapter.abyssSummonO;
        }
        else
        {
            x += 5.0f * std::cos(o);
            y += 5.0f * std::sin(o);
        }

        Creature* summon = player->SummonCreature(bossEntry, x, y, z, o, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300000);
        if (!summon)
            return false;

        summon->SetInCombatWith(player);
        summon->AddThreat(player, 1000.0f);

        return true;
    }

    bool EnsureCollectionEntry(uint32 guid, uint8 collectionType, uint32 itemId, uint16 relatedChapterId, uint16 level, uint16 awakenLevel, uint8 sourceType, uint32 unlockTime)
    {
        if (itemId == 0)
            return false;

        CharacterDatabase.Execute(kInsertPlayerCollectionSql, guid, collectionType, itemId, relatedChapterId, level, awakenLevel, sourceType, unlockTime);

        std::vector<PlayerAbyssCollectionEntry>& entries = _playerCollections[guid];
        auto existingItr = std::find_if(entries.begin(), entries.end(), [&](PlayerAbyssCollectionEntry const& entry)
        {
            return entry.itemId == itemId;
        });

        if (existingItr != entries.end())
            return false;

        PlayerAbyssCollectionEntry entry;
        entry.collectionType = collectionType;
        entry.itemId = itemId;
        entry.relatedChapterId = relatedChapterId;
        entry.level = level;
        entry.awakenLevel = awakenLevel;
        entry.sourceType = sourceType;
        entry.unlockTime = unlockTime;
        entries.push_back(entry);

        std::sort(entries.begin(), entries.end(), [](PlayerAbyssCollectionEntry const& left, PlayerAbyssCollectionEntry const& right)
        {
            if (left.collectionType != right.collectionType)
                return left.collectionType < right.collectionType;
            if (left.relatedChapterId != right.relatedChapterId)
                return left.relatedChapterId < right.relatedChapterId;
            return left.itemId < right.itemId;
        });

        return true;
    }

    bool CollectRelicFromInventory(Player* player, uint32 itemId, std::string* failureReason)
    {
        if (!player)
        {
            if (failureReason)
                *failureReason = "no_player";
            return false;
        }

        if (itemId == 0)
        {
            if (failureReason)
                *failureReason = "invalid_item";
            return false;
        }

        uint32 guid = player->GetGUID().GetCounter();
        if (_playerCollections.find(guid) == _playerCollections.end())
            LoadPlayerCollections(player);

        if (PlayerOwnsCollectedRelic(guid, itemId))
        {
            if (failureReason)
                *failureReason = "already_collected";
            return true;
        }

        AbyssRelicConfig const* relic = GetRelicConfig(itemId);
        if (!relic)
        {
            if (failureReason)
                *failureReason = "relic_not_configured";
            return false;
        }

        if (!player->HasItemCount(itemId, 1, false))
        {
            if (failureReason)
                *failureReason = "item_not_in_bag";
            return false;
        }

        uint8 collectionType = relic->relicType == 1 ? 1 : 2;
        uint8 sourceType = relic->relicType == 1 ? 1 : 2;
        uint32 now = GetNow();
        bool changed = EnsureCollectionEntry(guid, collectionType, itemId, relic->relatedChapterId, 1, 0, sourceType, now);
        player->DestroyItemCount(itemId, 1, true, false);

        if (changed)
        {
            if (failureReason)
                *failureReason = "ok";
            return true;
        }

        if (failureReason)
            *failureReason = "collect_failed";
        return false;
    }

    bool TryUnlockStageArtifacts(uint32 guid, PlayerAbyssData& data)
    {
        bool changed = false;
        uint32 now = GetNow();

        for (uint8 actId = 1; actId <= 6; ++actId)
        {
            bool hasAllRelics = true;
            uint32 phaseArtifactId = 0;
            for (auto const& pair : _relicConfigs)
            {
                AbyssRelicConfig const& relic = pair.second;
                if (relic.actId != actId)
                    continue;

                if (relic.relicType == 1 && !PlayerOwnsCollectedRelic(guid, relic.itemId))
                {
                    hasAllRelics = false;
                    break;
                }

                if (relic.relicType == 2)
                    phaseArtifactId = relic.itemId;
            }

            if (!hasAllRelics || phaseArtifactId == 0)
                continue;

            if (EnsureCollectionEntry(guid, 2, phaseArtifactId, 0, 1, 0, 2, now))
                changed = true;

            if (data.phaseArtifact == 0)
            {
                data.phaseArtifact = phaseArtifactId;
                changed = true;
            }
        }

        return changed;
    }

    bool TryUnlockUltimateArtifact(uint32 guid, PlayerAbyssData& data)
    {
        if (data.highestChapter < 74)
            return false;

        for (auto const& pair : _relicConfigs)
        {
            AbyssRelicConfig const& relic = pair.second;
            if (relic.relicType == 2 && !PlayerOwnsCollectedRelic(guid, relic.itemId))
                return false;
        }

        for (auto const& pair : _relicConfigs)
        {
            AbyssRelicConfig const& relic = pair.second;
            if (relic.relicType != 3)
                continue;

            uint32 now = GetNow();
            bool changed = EnsureCollectionEntry(guid, 2, relic.itemId, relic.relatedChapterId, 1, 0, 2, now);
            if (data.ultimateArtifact == 0)
            {
                data.ultimateArtifact = relic.itemId;
                changed = true;
            }

            return changed;
        }

        return false;
    }

    void UnlockChapterRelic(uint32 guid, AbyssChapterConfig const& chapter, PlayerAbyssData& data)
    {
        if (chapter.relicItemId == 0)
            return;

        uint32 now = GetNow();
        CharacterDatabase.Execute(kInsertPlayerRelicCollectionSql, guid, chapter.relicItemId, chapter.chapterId, now);
        EnsureCollectionEntry(guid, 1, chapter.relicItemId, chapter.chapterId, 1, 0, 1, now);

        if (data.mainRelic == 0)
            data.mainRelic = chapter.relicItemId;
    }

    bool SyncPlayerProgressFromQuests(Player* player, PlayerAbyssData& data) const
    {
        if (!player)
            return false;

        bool changed = false;
        uint16 inferredCurrentChapter = data.currentChapter;
        uint16 inferredHighestChapter = data.highestChapter;
        uint8 inferredStoryState = data.storyState;

        for (auto const& pair : _taskDockings)
        {
            AbyssTaskDocking const& docking = pair.second;
            QuestStatus questStatus = player->GetQuestStatus(docking.questId);
            bool rewarded = player->GetQuestRewardStatus(docking.questId);
            bool touched = rewarded || questStatus != QUEST_STATUS_NONE;

            if (!touched)
                continue;

            if (inferredCurrentChapter < docking.chapterId)
                inferredCurrentChapter = docking.chapterId;

            if (docking.taskType == 5 && (rewarded || questStatus == QUEST_STATUS_COMPLETE))
            {
                if (inferredHighestChapter < docking.chapterId)
                    inferredHighestChapter = docking.chapterId;

                if (inferredCurrentChapter == docking.chapterId && inferredStoryState < 2)
                    inferredStoryState = 2;
            }
            else if (inferredCurrentChapter == docking.chapterId && inferredStoryState == 0)
            {
                inferredStoryState = 1;
            }
        }

        if (inferredCurrentChapter != 0)
        {
            AbyssTaskDocking const* currentComplete = GetTaskDockingForChapter(inferredCurrentChapter, 5);
            AbyssTaskDocking const* currentStart = GetTaskDockingForChapter(inferredCurrentChapter, 1);

            if (currentComplete)
            {
                QuestStatus status = player->GetQuestStatus(currentComplete->questId);
                bool rewarded = player->GetQuestRewardStatus(currentComplete->questId);
                if (rewarded || status == QUEST_STATUS_COMPLETE)
                    inferredStoryState = 2;
            }

            if (inferredStoryState == 0 && currentStart)
            {
                QuestStatus status = player->GetQuestStatus(currentStart->questId);
                bool rewarded = player->GetQuestRewardStatus(currentStart->questId);
                if (rewarded || status != QUEST_STATUS_NONE)
                    inferredStoryState = 1;
            }
        }

        if (data.currentChapter != inferredCurrentChapter)
        {
            data.currentChapter = inferredCurrentChapter;
            changed = true;
        }

        if (data.highestChapter != inferredHighestChapter)
        {
            data.highestChapter = inferredHighestChapter;
            changed = true;
        }

        if (data.storyState != inferredStoryState)
        {
            data.storyState = inferredStoryState;
            changed = true;
        }

        return changed;
    }

    bool SyncPlayerChapterModeUnlocksFromProgress(Player* player, PlayerAbyssData const& /*data*/)
    {
        if (!player)
            return false;

        bool changed = false;

        for (auto const& pair : _chapterConfigs)
        {
            AbyssChapterConfig const& chapter = pair.second;

            if (chapter.startQuestId != 0)
            {
                QuestStatus startStatus = player->GetQuestStatus(chapter.startQuestId);
                bool startRewarded = player->GetQuestRewardStatus(chapter.startQuestId);
                if (startRewarded || startStatus == QUEST_STATUS_COMPLETE)
                {
                    if (UnlockChapterMode(player, chapter.chapterId, 1))
                        changed = true;
                }
            }

            AbyssTaskDocking const* storyTask = GetTaskDockingForChapter(chapter.chapterId, 2);
            if (storyTask)
            {
                QuestStatus status = player->GetQuestStatus(storyTask->questId);
                bool rewarded = player->GetQuestRewardStatus(storyTask->questId);
                if (rewarded || status == QUEST_STATUS_COMPLETE)
                {
                    if (UnlockChapterMode(player, chapter.chapterId, 2))
                        changed = true;
                }
            }

            AbyssTaskDocking const* abyssTask = GetTaskDockingForChapter(chapter.chapterId, 3);
            if (abyssTask)
            {
                QuestStatus status = player->GetQuestStatus(abyssTask->questId);
                bool rewarded = player->GetQuestRewardStatus(abyssTask->questId);
                if (rewarded || status == QUEST_STATUS_COMPLETE)
                {
                    if (UnlockChapterMode(player, chapter.chapterId, 3))
                        changed = true;
                }
            }

            AbyssTaskDocking const* corruptionTask = GetTaskDockingForChapter(chapter.chapterId, 5);
            if (corruptionTask)
            {
                QuestStatus status = player->GetQuestStatus(corruptionTask->questId);
                bool rewarded = player->GetQuestRewardStatus(corruptionTask->questId);
                if (rewarded || status == QUEST_STATUS_COMPLETE)
                {
                    if (UnlockChapterMode(player, chapter.chapterId, 4))
                        changed = true;
                }
            }
        }

        return changed;
    }

    bool SyncPlayerData(Player* player, bool saveIfChanged)
    {
        if (!player)
            return false;

        if (!_worldDataLoaded)
            return false;

        EnsurePlayerData(player);

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return false;

        bool changed = SyncPlayerProgressFromQuests(player, itr->second);
        changed = SyncPlayerChapterModeUnlocksFromProgress(player, itr->second) || changed;
        changed = TryUnlockStageArtifacts(guid, itr->second) || changed;
        changed = TryUnlockUltimateArtifact(guid, itr->second) || changed;
        changed = NormalizePlayerData(guid, itr->second) || changed;

        auto runItr = _playerRunStates.find(guid);
        if (runItr != _playerRunStates.end())
        {
            bool runChanged = NormalizePlayerRunState(runItr->second, &itr->second);
            if (runItr->second.currentChapterId == 0)
            {
                DeletePlayerRunState(guid);
                ClearPlayerProcState(guid);
                changed = true;
            }
            else if (runChanged)
            {
                changed = true;
                if (saveIfChanged)
                    SavePlayerRunState(player);
            }
        }

        if (changed && saveIfChanged)
            SavePlayerData(player);

        return changed;
    }

    AbyssChapterAccessState EvaluateChapterAccess(Player* player, PlayerAbyssData const& data, AbyssChapterConfig const& chapter) const
    {
        AbyssChapterAccessState result;
        result.prerequisiteMet = chapter.prerequisiteChapterId == 0 || data.highestChapter >= chapter.prerequisiteChapterId;
        result.levelMet = player && player->GetLevel() >= chapter.requiredCultivationLevel;
        result.readyToEnter = result.prerequisiteMet && result.levelMet;

        if (!result.prerequisiteMet)
            result.reason = "missing_prerequisite";
        else if (!result.levelMet)
            result.reason = "level_too_low";
        else
            result.reason = "ready";

        return result;
    }

    bool SetPlayerCurrentChapter(Player* player, uint16 chapterId)
    {
        if (!player)
            return false;

        if (chapterId != 0 && !GetChapterConfig(chapterId))
            return false;

        EnsurePlayerData(player);

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return false;

        PlayerAbyssData& data = itr->second;
        data.currentChapter = chapterId;

        if (chapterId == 0)
        {
            data.currentCultivationThreshold = 0;
            data.storyState = 0;
        }
        else
        {
            AbyssChapterConfig const* chapterConfig = GetChapterConfig(chapterId);
            data.currentCultivationThreshold = chapterConfig ? chapterConfig->requiredCultivationLevel : 0;
        }

        NormalizePlayerData(guid, data);

        auto runItr = _playerRunStates.find(guid);
        if (runItr != _playerRunStates.end() && runItr->second.currentChapterId != chapterId)
        {
            DeletePlayerRunState(guid);
            ClearPlayerProcState(guid);
        }

        SavePlayerData(player);
        RefreshPlayerRuntimeStats(player);
        return true;
    }

    bool TryTeleportPlayerToChapter(Player* player, AbyssChapterConfig const& chapter, std::string* failureReason) const
    {
        if (!player)
        {
            if (failureReason)
                *failureReason = "no_player";
            return false;
        }

        if (chapter.mapId == 0)
        {
            if (failureReason)
                *failureReason = "teleport_unavailable";
            return false;
        }

        if (AreaTriggerTeleport const* trigger = sObjectMgr->GetMapEntranceTrigger(chapter.mapId))
        {
            if (player->TeleportTo(trigger->target_mapId, trigger->target_X, trigger->target_Y, trigger->target_Z, trigger->target_Orientation))
                return true;
        }

        int32 entranceMapId = -1;
        float entranceX = 0.0f;
        float entranceY = 0.0f;
        if (MapEntry const* mapEntry = sMapStore.LookupEntry(chapter.mapId))
        {
            if (mapEntry->GetEntrancePos(entranceMapId, entranceX, entranceY) && entranceMapId >= 0)
            {
                if (player->TeleportTo(static_cast<uint32>(entranceMapId), entranceX, entranceY, player->GetPositionZ(), player->GetOrientation()))
                    return true;
            }
        }

        if (chapter.abyssSummonMapId != 0 &&
            (chapter.abyssSummonX != 0.0f || chapter.abyssSummonY != 0.0f || chapter.abyssSummonZ != 0.0f))
        {
            if (player->TeleportTo(chapter.abyssSummonMapId, chapter.abyssSummonX, chapter.abyssSummonY, chapter.abyssSummonZ, chapter.abyssSummonO))
                return true;
        }

        if (failureReason)
            *failureReason = "teleport_failed";
        return false;
    }

    bool BeginPlayerRunAndTeleport(Player* player, uint16 chapterId, uint8 modeType, uint16 corruptionTier, std::string* failureReason)
    {
        uint8 requestedModeType = modeType;
        if (requestedModeType == 0)
            requestedModeType = 1;

        if (requestedModeType == 1)
        {
            std::string switchFailureReason;
            if (TryEnterTriggeredStoryMode(player, chapterId, &switchFailureReason))
                return true;
        }

        if (requestedModeType >= 2)
        {
            std::string switchFailureReason;
            if (TryEnterTriggeredAbyssMode(player, chapterId, requestedModeType, corruptionTier, &switchFailureReason))
                return true;

            if (!switchFailureReason.empty())
            {
                if (failureReason)
                    *failureReason = switchFailureReason;
                return false;
            }
        }

        if (!BeginPlayerRun(player, chapterId, requestedModeType, corruptionTier, failureReason))
            return false;

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(chapterId);
        if (!chapterConfig)
        {
            EndPlayerRun(player);
            if (failureReason)
                *failureReason = "chapter_not_found";
            return false;
        }

        if (player->GetMapId() == chapterConfig->mapId)
            return true;

        std::string teleportFailureReason;
        if (TryTeleportPlayerToChapter(player, *chapterConfig, &teleportFailureReason))
            return true;

        EndPlayerRun(player);
        if (failureReason)
            *failureReason = teleportFailureReason.empty() ? "enter_rolled_back" : teleportFailureReason;
        return false;
    }

    bool BeginPlayerRun(Player* player, uint16 chapterId, uint8 modeType, uint16 corruptionTier, std::string* failureReason)
    {
        if (!player)
        {
            if (failureReason)
                *failureReason = "no_player";
            return false;
        }

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(chapterId);
        if (!chapterConfig)
        {
            if (failureReason)
                *failureReason = "chapter_not_found";
            return false;
        }

        EnsurePlayerData(player);

        uint32 guid = player->GetGUID().GetCounter();
        auto playerItr = _playerData.find(guid);
        if (playerItr == _playerData.end())
        {
            if (failureReason)
                *failureReason = "player_data_missing";
            return false;
        }

        uint8 requestedMode = modeType == 0 ? 1 : modeType;
        if (!IsChapterModeUnlocked(player, playerItr->second, *chapterConfig, requestedMode))
        {
            if (failureReason)
                *failureReason = "mode_locked";
            return false;
        }

        AbyssChapterAccessState accessState = EvaluateChapterAccess(player, playerItr->second, *chapterConfig);
        if (!accessState.readyToEnter)
        {
            if (failureReason)
                *failureReason = accessState.reason;
            return false;
        }

        PlayerAbyssRunState& state = _playerRunStates[guid];
        state.currentMapId = chapterConfig->mapId;
        state.currentChapterId = chapterId;
        state.modeType = requestedMode;
        state.corruptionTier = corruptionTier;
        state.runMainRelic = playerItr->second.mainRelic;
        state.runSubRelic1 = playerItr->second.subRelic1;
        state.runSubRelic2 = playerItr->second.subRelic2;
        state.runSubRelic3 = playerItr->second.subRelic3;
        state.runSubRelic4 = playerItr->second.subRelic4;
        state.runSubRelic5 = playerItr->second.subRelic5;
        state.anchorBossKillMask = 0;
        state.abyssBossSummoned = false;
        state.cacheBossSummoned = false;
        state.startTime = GetNow();
        state.hasDatabaseRow = true;

        GetOrCreatePlayerProcState(guid).abyssModePromptShown = false;

        NormalizePlayerRunState(state, &playerItr->second);
        SavePlayerRunState(player);
        RefreshPlayerRuntimeStats(player);
        return true;
    }

    bool EndPlayerRun(Player* player)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerRunStates.find(guid);
        if (itr == _playerRunStates.end())
            return false;

        DeletePlayerRunState(guid);
        ClearPlayerProcState(guid);
        RefreshPlayerRuntimeStats(player);
        return true;
    }

    bool TryEnterTriggeredAbyssMode(Player* player, uint16 chapterId, uint8 modeType, uint16 corruptionTier, std::string* failureReason)
    {
        if (failureReason)
            failureReason->clear();

        if (!player)
        {
            if (failureReason)
                *failureReason = "no_player";
            return false;
        }

        if (!_worldDataLoaded || !IsAdvancedModeType(modeType))
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto runItr = _playerRunStates.find(guid);
        if (runItr == _playerRunStates.end())
            return false;

        auto playerItr = _playerData.find(guid);
        if (playerItr == _playerData.end())
        {
            if (failureReason)
                *failureReason = "player_data_missing";
            return false;
        }

        PlayerAbyssRunState& state = runItr->second;
        PlayerAbyssData& data = playerItr->second;

        if (state.currentChapterId == 0 ||
            state.currentChapterId != chapterId ||
            state.modeType != 1)
        {
            return false;
        }

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(state.currentChapterId);
        if (!chapterConfig)
            return false;

        if (state.currentMapId != 0 && player->GetMapId() != state.currentMapId)
            return false;

        if (!IsChapterModeUnlocked(player, data, *chapterConfig, modeType))
        {
            if (failureReason)
                *failureReason = "mode_locked";
            return false;
        }

        if (state.abyssBossSummoned)
        {
            if (failureReason)
                *failureReason = "abyss_already_summoned";
            return false;
        }

        if (!IsAbyssSummonReady(*chapterConfig, state))
        {
            if (failureReason)
                *failureReason = "abyss_not_ready";
            return false;
        }

        uint8 previousModeType = state.modeType;
        uint16 previousCorruptionTier = state.corruptionTier;

        state.modeType = modeType;
        state.corruptionTier = modeType == 3 ? corruptionTier : 0;
        NormalizePlayerRunState(state, &data);

        if (!TrySummonBoss(player, *chapterConfig, chapterConfig->abyssBossEntry, true))
        {
            state.modeType = previousModeType;
            state.corruptionTier = previousCorruptionTier;
            NormalizePlayerRunState(state, &data);
            SavePlayerRunState(player);

            if (failureReason)
                *failureReason = "summon_failed";
            return false;
        }

        state.abyssBossSummoned = true;
        GetOrCreatePlayerProcState(guid).abyssModePromptShown = true;
        SavePlayerRunState(player);
        return true;
    }

    bool TryEnterTriggeredStoryMode(Player* player, uint16 chapterId, std::string* failureReason)
    {
        if (failureReason)
            failureReason->clear();

        if (!player)
        {
            if (failureReason)
                *failureReason = "no_player";
            return false;
        }

        if (!_worldDataLoaded)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto runItr = _playerRunStates.find(guid);
        if (runItr == _playerRunStates.end())
            return false;

        auto playerItr = _playerData.find(guid);
        if (playerItr == _playerData.end())
        {
            if (failureReason)
                *failureReason = "player_data_missing";
            return false;
        }

        PlayerAbyssRunState& state = runItr->second;
        PlayerAbyssData& data = playerItr->second;
        if (state.currentChapterId == 0 || state.currentChapterId != chapterId || state.modeType != 1)
            return false;

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(state.currentChapterId);
        if (!chapterConfig)
            return false;

        if (!IsChapterModeUnlocked(player, data, *chapterConfig, 1))
        {
            if (failureReason)
                *failureReason = "mode_locked";
            return false;
        }

        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(guid);
        float x = player->GetPositionX();
        float y = player->GetPositionY();
        float z = player->GetPositionZ();
        float o = player->GetOrientation();

        if (procState.hasPromptPosition)
        {
            x = procState.lastPromptX;
            y = procState.lastPromptY;
            z = procState.lastPromptZ;
            o = procState.lastPromptO;
        }

        Creature* summon = player->SummonCreature(chapterConfig->anchorBossEntry, x, y, z, o, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300000);
        if (!summon)
        {
            if (failureReason)
                *failureReason = "summon_failed";
            return false;
        }

        summon->SetInCombatWith(player);
        summon->AddThreat(player, 1000.0f);
        procState.abyssModePromptShown = true;
        return true;
    }

    bool HandleCreatureKill(Player* player, Creature* creature)
    {
        if (!player || !creature || !_worldDataLoaded)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 creatureEntry = creature->GetEntry();

        AbyssChapterConfig const* mappedChapterByMap = GetChapterConfigByMapId(player->GetMapId());

        bool hadActiveRun = false;
        auto runItr = _playerRunStates.find(playerGuid);
        if (runItr == _playerRunStates.end())
        {
            AbyssChapterConfig const* chapterByMap = mappedChapterByMap;
            if (!chapterByMap)
                return false;

            uint64 lazyTrackedMaskBit = GetTrackedBossMaskBit(*chapterByMap, creatureEntry);
            bool isLazyBootstrapKill = lazyTrackedMaskBit == (1ULL << 0) || lazyTrackedMaskBit == (1ULL << 1);
            if (!isLazyBootstrapKill)
                return false;

            std::string bootstrapFailureReason;
            if (!BeginPlayerRun(player, chapterByMap->chapterId, 1, 0, &bootstrapFailureReason))
            {
                return false;
            }

            runItr = _playerRunStates.find(playerGuid);
            if (runItr == _playerRunStates.end())
                return false;

        }
        else
        {
            hadActiveRun = true;
        }

        auto playerItr = _playerData.find(playerGuid);
        if (playerItr == _playerData.end())
            return false;

        PlayerAbyssRunState& state = runItr->second;
        PlayerAbyssData& data = playerItr->second;

        if (state.modeType == 0)
            return false;

        if (NormalizePlayerRunState(state, &data))
            SavePlayerRunState(player);

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(state.currentChapterId);
        if (!chapterConfig)
            return false;

        if (state.currentMapId != 0 && player->GetMapId() != state.currentMapId)
            return false;

        uint64 trackedMaskBit = GetTrackedBossMaskBit(*chapterConfig, creatureEntry);
        bool changed = false;
        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(playerGuid);

        if (trackedMaskBit != 0 && (state.anchorBossKillMask & trackedMaskBit) == 0)
        {
            state.anchorBossKillMask |= trackedMaskBit;
            changed = true;
        }

        if (trackedMaskBit == (1ULL << 0) || trackedMaskBit == (1ULL << 1))
        {
            procState.lastPromptX = creature->GetPositionX();
            procState.lastPromptY = creature->GetPositionY();
            procState.lastPromptZ = creature->GetPositionZ();
            procState.lastPromptO = creature->GetOrientation();
            procState.hasPromptPosition = true;
        }

        bool readyForPrompt = IsAbyssSummonReady(*chapterConfig, state);

        if (state.modeType == 1)
        {
            uint32 availableModeMask = GetChapterUnlockedModeMask(player, data, *chapterConfig);
            if (hadActiveRun)
                availableModeMask &= (GetModeTypeMask(2) | GetModeTypeMask(3) | GetModeTypeMask(4));

            if (availableModeMask != 0 &&
                readyForPrompt &&
                !procState.abyssModePromptShown)
            {
                SendAbyssModePrompt(player, *chapterConfig, data, availableModeMask);
                procState.abyssModePromptShown = true;
            }
            else if (trackedMaskBit != 0)
            {
                LOG_INFO("module", "mod-abyss-cultivation: mode prompt suppressed player={} ({}) chapter={} reason=modeMask:{} ready:{} shown:{}",
                    player->GetName(),
                    playerGuid,
                    chapterConfig->chapterId,
                    availableModeMask,
                    readyForPrompt,
                    procState.abyssModePromptShown);
            }

            if (changed)
                SavePlayerRunState(player);

            return changed;
        }

        float fireRingScale = GetActiveScriptGroupScale(player, { "遗物_裂火炭核", "特效_裂火爆环" });
        if (fireRingScale > 0.0f)
        {
            ++procState.killCounter;
            if (procState.killCounter >= 12 && GetNow() > procState.lastFireRingTime + 3)
            {
                procState.killCounter = 0;
                TriggerFireRing(player, procState.lastFireRingTime, fireRingScale);
            }
        }

        float soulScale = GetActiveScriptGroupScale(player, { "遗物_通灵逆契", "祝福_灵魂沸涌", "特效_魂火追击" });
        if (soulScale > 0.0f)
        {
            ++procState.bloodKillCounter;
            if (procState.bloodKillCounter >= 1 && GetNow() > procState.lastSoulProcTime + 1)
            {
                procState.bloodKillCounter = 0;
                TriggerSoulBurst(player, procState.lastSoulProcTime, soulScale);
            }
        }

        float featherScale = GetActiveScriptGroupScale(player, { "遗物_鸦神命羽", "遗物_王陨号角" });
        if (featherScale > 0.0f &&
            GetNow() > procState.lastFeatherVolleyTime + 2)
        {
            TriggerFeatherVolley(player, procState.lastFeatherVolleyTime, featherScale);
        }

        if (HasActiveScriptGroup(player, "遗物_霜王残印"))
            ++procState.dominionCounter;

        if (IsAbyssSummonReady(*chapterConfig, state))
        {
            if (TrySummonBoss(player, *chapterConfig, chapterConfig->abyssBossEntry, true))
            {
                state.abyssBossSummoned = true;
                changed = true;
            }
        }

        bool triggerKill = trackedMaskBit == (1ULL << 0) || trackedMaskBit == (1ULL << 1);
        if (triggerKill && !state.cacheBossSummoned && chapterConfig->cacheBossEntry != 0)
        {
            bool pityReached = chapterConfig->cacheBossPityCount > 0 &&
                static_cast<uint32>(data.cacheBossFailCount + 1) >= chapterConfig->cacheBossPityCount;
            bool rollSuccess = pityReached || RollPercentage() < chapterConfig->cacheBossBaseChance;

            if (rollSuccess && TrySummonBoss(player, *chapterConfig, chapterConfig->cacheBossEntry, false))
            {
                state.cacheBossSummoned = true;
                data.cacheBossFailCount = 0;
                changed = true;

                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("Cache boss summoned for chapter {}: entry={} pity={}", chapterConfig->chapterId, chapterConfig->cacheBossEntry, pityReached);
            }
            else if (!state.cacheBossSummoned)
            {
                ++data.cacheBossFailCount;
                changed = true;
            }
        }

        if (creatureEntry == chapterConfig->cacheBossEntry && data.cacheBossFailCount != 0)
        {
            data.cacheBossFailCount = 0;
            changed = true;
        }

        if (creatureEntry == chapterConfig->abyssBossEntry ||
            creatureEntry == chapterConfig->cacheBossEntry)
        {
            AbyssRewardResult reward = GrantBossReward(player, *chapterConfig, data, state, creatureEntry);
            changed = reward.awarded || changed;
        }

        if (creatureEntry == chapterConfig->abyssBossEntry && data.highestCorruptionTier < state.corruptionTier)
        {
            data.highestCorruptionTier = state.corruptionTier;
            changed = true;
        }

        if (changed)
        {
            SavePlayerData(player);
            SavePlayerRunState(player);
        }

        return changed;
    }

    bool TryRestoreDefaultLootForNonAbyssKill(Player* player, Creature* creature)
    {
        if (!player || !creature || !_worldDataLoaded)
            return false;

        Map* map = player->GetMap();
        if (!map || (!map->IsDungeon() && !map->IsRaid()))
            return false;

        AbyssChapterConfig const* chapterConfig = GetChapterConfigByMapId(player->GetMapId());
        if (!chapterConfig)
            return false;

        PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter());
        if (runState &&
            runState->modeType >= 2 &&
            (runState->currentMapId == 0 || runState->currentMapId == player->GetMapId()))
        {
            return false;
        }

        if (!creature->isDead())
            return false;

        if (creature->GetLootRecipient())
            return false;

        if (!creature->loot.empty() && !creature->loot.isLooted())
            return false;

        CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
        if (!creatureTemplate)
            return false;

        if (creatureTemplate->lootid == 0 &&
            creatureTemplate->mingold == 0 &&
            creatureTemplate->maxgold == 0)
        {
            return false;
        }

        creature->loot.clear();
        creature->SetLootRecipient(player, player->GetGroup() != nullptr);
        creature->loot.loot_type = LOOT_CORPSE;

        if (creatureTemplate->lootid != 0)
            creature->loot.FillLoot(creatureTemplate->lootid, LootTemplates_Creature, player, false, false, creature->GetLootMode(), creature);

        if (creature->GetLootMode())
            creature->loot.generateMoneyLoot(creatureTemplate->mingold, creatureTemplate->maxgold);

        if (creature->loot.isLooted())
        {
            creature->SetLootRecipient(nullptr);
            return false;
        }

        creature->SetDynamicFlag(UNIT_DYNFLAG_LOOTABLE);

        return true;
    }

    bool HandleQuestCompletion(Player* player, Quest const* quest)
    {
        if (!player || !quest || !_worldDataLoaded)
            return false;

        AbyssTaskDocking const* docking = GetTaskDocking(quest->GetQuestId());
        if (!docking)
            return false;

        EnsurePlayerData(player);

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerData.find(guid);
        if (itr == _playerData.end())
            return false;

        PlayerAbyssData& data = itr->second;
        bool changed = false;
        uint8 completedRunMode = 0;

        auto runItr = _playerRunStates.find(guid);
        if (runItr != _playerRunStates.end() && runItr->second.currentChapterId == docking->chapterId)
            completedRunMode = runItr->second.modeType;

        if (data.currentChapter != docking->chapterId)
        {
            data.currentChapter = docking->chapterId;
            changed = true;
        }

        if (data.currentCultivationThreshold != 0 || docking->chapterId != 0)
        {
            AbyssChapterConfig const* chapterConfig = GetChapterConfig(docking->chapterId);
            uint16 threshold = chapterConfig ? chapterConfig->requiredCultivationLevel : 0;
            if (data.currentCultivationThreshold != threshold)
            {
                data.currentCultivationThreshold = threshold;
                changed = true;
            }
        }

        if (docking->taskType == 1)
        {
            if (data.storyState != 1)
            {
                data.storyState = 1;
                changed = true;
            }
        }
        else if (docking->taskType == 5)
        {
            if (data.highestChapter < docking->chapterId)
            {
                data.highestChapter = docking->chapterId;
                changed = true;
            }

            if (data.storyState != 2)
            {
                data.storyState = 2;
                changed = true;
            }

            if (AbyssChapterConfig const* chapterConfig = GetChapterConfig(docking->chapterId))
            {
                uint32 previousMainRelic = data.mainRelic;
                uint32 previousPhaseArtifact = data.phaseArtifact;
                uint32 previousUltimateArtifact = data.ultimateArtifact;
                UnlockChapterRelic(guid, *chapterConfig, data);
                if (data.mainRelic != previousMainRelic)
                    changed = true;
                changed = TryUnlockStageArtifacts(guid, data) || changed;
                changed = TryUnlockUltimateArtifact(guid, data) || changed;
                if (data.phaseArtifact != previousPhaseArtifact || data.ultimateArtifact != previousUltimateArtifact)
                    changed = true;
            }
        }

        uint8 unlockedModeType = 0;
        if (docking->taskType == 1)
        {
            changed = changed;
        }
        else if (docking->taskType == 2)
        {
            if (UnlockChapterMode(player, docking->chapterId, 1))
            {
                changed = true;
                unlockedModeType = 1;
            }
        }
        else if (docking->taskType == 3)
        {
            if (UnlockChapterMode(player, docking->chapterId, 2))
            {
                changed = true;
                unlockedModeType = 2;
            }
        }
        else if (docking->taskType == 4)
        {
            if (UnlockChapterMode(player, docking->chapterId, 3))
            {
                changed = true;
                unlockedModeType = 3;
            }
        }
        else if (docking->taskType == 5)
        {
            if (UnlockChapterMode(player, docking->chapterId, 4))
            {
                changed = true;
                unlockedModeType = 4;
            }
        }

        changed = NormalizePlayerData(guid, data) || changed;

        if (changed)
        {
            SavePlayerData(player);
            RefreshPlayerRuntimeStats(player);

            if (IsDebugEnabled())
            {
                LOG_DEBUG("module", "mod-abyss-cultivation: player {} ({}) completed quest {} mapped to chapter {} type={} currentChapter={} highestChapter={} storyState={}",
                    player->GetName(),
                    guid,
                    quest->GetQuestId(),
                    docking->chapterId,
                    static_cast<uint32>(docking->taskType),
                    data.currentChapter,
                    data.highestChapter,
                    static_cast<uint32>(data.storyState));
            }

            if (docking->taskType == 5)
            {
                if (runItr != _playerRunStates.end() && runItr->second.currentChapterId == docking->chapterId)
                {
                    DeletePlayerRunState(guid);
                    ClearPlayerProcState(guid);
                }

                AbyssChapterConfig const* nextChapter = GetNextChapterConfig(docking->chapterId);
                if (nextChapter && player->GetSession())
                {
                    AbyssChapterAccessState accessState = EvaluateChapterAccess(player, data, *nextChapter);
                    ChatHandler chatHandler(player->GetSession());
                    chatHandler.PSendSysMessage("Abyss chapter {} completed. Next chapter: {} ({}) access={}",
                        docking->chapterId,
                        nextChapter->chapterId,
                        nextChapter->chapterName,
                        accessState.reason);
                }
            }

            if (unlockedModeType >= 1)
            {
                if (AbyssChapterConfig const* chapterConfig = GetChapterConfig(docking->chapterId))
                    SendAbyssModePrompt(player, *chapterConfig, data, GetChapterUnlockedModeMask(player, data, *chapterConfig));
            }
        }

        return changed;
    }

private:
    std::unordered_map<uint16, AbyssChapterConfig> _chapterConfigs;
    std::unordered_map<uint32, AbyssRelicConfig> _relicConfigs;
    std::unordered_map<uint32, AbyssBossConfig> _bossConfigs;
    std::unordered_map<uint32, AbyssEquipmentTemplate> _equipmentTemplates;
    std::unordered_map<uint32, AbyssSetBonusConfig> _setBonusConfigs;
    std::unordered_map<uint32 /*playerGuid*/, std::unordered_map<uint32 /*setId*/, PlayerSetBonusState>> _playerSetBonusStates;
    std::unordered_map<uint32, AbyssAffixTemplate> _affixTemplates;
    std::unordered_map<uint32, AbyssSpecialEffectTemplate> _specialEffectTemplates;
    std::unordered_map<uint32, AbyssTaskDocking> _taskDockings;
    std::unordered_map<uint32, AbyssBossDocking> _bossDockings;
    std::unordered_map<uint32, AbyssItemDocking> _itemDockings;
    std::unordered_map<uint32, PlayerAbyssData> _playerData;
    std::unordered_map<uint32, std::vector<PlayerAbyssCollectionEntry>> _playerCollections;
    std::unordered_map<uint32, std::unordered_map<uint16, PlayerAbyssChapterModeUnlock>> _playerChapterModeUnlocks;
    std::unordered_map<uint32, PlayerAbyssRunState> _playerRunStates;
    std::unordered_map<uint32, PlayerAbyssProcState> _playerProcStates;
    bool _worldDataLoaded = false;
};

#define sAbyssCultivationMgr AbyssCultivationMgr::instance()

void SendAbyssPayload(Player* player, std::string const& payload)
{
    if (!player || payload.empty())
        return;

    if (payload.length() <= ABYSS_MAX_ADDON_PAYLOAD)
    {
        std::string fullMessage = std::string(ABYSS_ADDON_PREFIX) + '\t' + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
        return;
    }

    size_t totalChunks = (payload.length() + ABYSS_MAX_ADDON_PAYLOAD - 1) / ABYSS_MAX_ADDON_PAYLOAD;
    for (size_t i = 0; i < totalChunks; ++i)
    {
        size_t start = i * ABYSS_MAX_ADDON_PAYLOAD;
        size_t len = std::min(ABYSS_MAX_ADDON_PAYLOAD, payload.length() - start);
        std::string chunk = payload.substr(start, len);

        std::ostringstream chunkMessage;
        chunkMessage << "CHUNK:" << (i + 1) << ":" << totalChunks << ":" << chunk;

        std::string fullMessage = std::string(ABYSS_ADDON_PREFIX) + '\t' + chunkMessage.str();
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }
}

void SendAbyssResult(Player* player, std::string const& action, bool success, std::string const& message)
{
    std::ostringstream payload;
    payload << "RESULT:" << action << '^' << (success ? 1 : 0) << '^' << SanitizeAddonText(message);
    SendAbyssPayload(player, payload.str());
}

void SendAbyssModePrompt(Player* player, AbyssChapterConfig const& chapter, PlayerAbyssData const& data, uint32 modeMask)
{
    if (!player)
        return;

    if (modeMask == 0)
    {
        LOG_INFO("module", "mod-abyss-cultivation: skip MODE_PROMPT for player {} ({}) chapter {} because modeMask=0 unlockedMask={}",
            player->GetName(),
            player->GetGUID().GetCounter(),
            chapter.chapterId,
            data.unlockedModeMask);
        return;
    }

    std::ostringstream payload;
    payload << "MODE_PROMPT:"
            << chapter.chapterId << '|'
            << SanitizeAddonText(chapter.chapterName) << '|'
            << modeMask << '|'
            << data.highestCorruptionTier;

    SendAbyssPayload(player, payload.str());
}

void SendAbyssStateToAddon(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid);
    if (!playerData)
    {
        SendAbyssResult(player, "STATE", false, "player_data_missing");
        return;
    }

    AbyssChapterConfig const* currentChapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
    AbyssChapterConfig const* nextChapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
    PlayerAbyssRunState const* runState = sAbyssCultivationMgr->GetPlayerRunState(guid);

    std::ostringstream payload;
    payload << "STATE:"
            << guid << '|'
            << static_cast<uint32>(player->GetLevel()) << '|'
            << playerData->currentChapter << '|'
            << SanitizeAddonText(currentChapter ? currentChapter->chapterName : std::string()) << '|'
            << playerData->highestChapter << '|'
            << playerData->currentCultivationThreshold << '|'
            << static_cast<uint32>(playerData->storyState) << '|'
            << playerData->unlockedModeMask << '|'
            << playerData->mainRelic << '|'
            << playerData->subRelic1 << '|'
            << playerData->subRelic2 << '|'
            << playerData->subRelic3 << '|'
            << playerData->subRelic4 << '|'
            << playerData->subRelic5 << '|'
            << playerData->phaseArtifact << '|'
            << playerData->ultimateArtifact << '|'
            << playerData->highestCorruptionTier << '|'
            << playerData->cacheBossFailCount << '|'
            << (runState ? 1 : 0) << '|'
            << (runState ? runState->currentChapterId : 0) << '|'
            << (runState ? static_cast<uint32>(runState->modeType) : 0) << '|'
            << (runState ? runState->corruptionTier : 0) << '|'
            << (runState ? runState->currentMapId : 0) << '|'
            << (runState ? runState->anchorBossKillMask : 0) << '|'
            << (runState ? (runState->abyssBossSummoned ? 1 : 0) : 0) << '|'
            << (runState ? (runState->cacheBossSummoned ? 1 : 0) : 0) << '|'
            << (nextChapter ? nextChapter->chapterId : 0) << '|'
            << SanitizeAddonText(nextChapter ? nextChapter->chapterName : std::string());

    SendAbyssPayload(player, payload.str());
}

void SendAbyssChapterListToAddon(Player* player)
{
    if (!player)
        return;

    PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter());

    std::vector<AbyssChapterConfig const*> chapters;
    for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
        chapters.push_back(&pair.second);

    std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
    {
        return left->chapterId < right->chapterId;
    });

    std::ostringstream payload;
    payload << "CHAPTERS:";
    bool first = true;
    for (AbyssChapterConfig const* chapter : chapters)
    {
        if (!first)
            payload << '~';
        first = false;

        payload << chapter->chapterId << '^'
                << SanitizeAddonText(chapter->chapterName) << '^'
                << static_cast<uint32>(chapter->actId) << '^'
                << chapter->mapId << '^'
                << static_cast<uint32>(chapter->chapterType) << '^'
                << chapter->requiredCultivationLevel << '^'
                << chapter->startQuestId << '^'
                << chapter->completeQuestId << '^'
                << (playerData ? sAbyssCultivationMgr->GetChapterUnlockedModeMask(player, *playerData, *chapter) : 0u) << '^'
                << chapter->anchorBossEntry << '^'
                << SanitizeAddonText(sAbyssCultivationMgr->GetBossDisplayName(chapter->anchorBossEntry)) << '^'
                << chapter->finalBossEntry << '^'
                << SanitizeAddonText(sAbyssCultivationMgr->GetBossDisplayName(chapter->finalBossEntry)) << '^'
                << chapter->abyssBossEntry << '^'
                << SanitizeAddonText(sAbyssCultivationMgr->GetBossDisplayName(chapter->abyssBossEntry)) << '^'
                << chapter->cacheBossEntry << '^'
                << SanitizeAddonText(sAbyssCultivationMgr->GetBossDisplayName(chapter->cacheBossEntry));
    }

    SendAbyssPayload(player, payload.str());
}

void SendAbyssRelicsToAddon(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid);

    // 收集所有遗物配置并按 relicType -> actId -> itemId 排序
    std::vector<AbyssRelicConfig const*> allRelics;
    for (auto const& pair : sAbyssCultivationMgr->GetAllRelicConfigs())
        allRelics.push_back(&pair.second);

    std::sort(allRelics.begin(), allRelics.end(), [](AbyssRelicConfig const* a, AbyssRelicConfig const* b)
    {
        if (a->relicType != b->relicType)
            return a->relicType < b->relicType;
        if (a->actId != b->actId)
            return a->actId < b->actId;
        return a->itemId < b->itemId;
    });

    std::ostringstream payload;
    payload << "RELICS:";
    bool first = true;
    for (AbyssRelicConfig const* relic : allRelics)
    {
        if (!first)
            payload << '~';
        first = false;

        bool owned = sAbyssCultivationMgr->PlayerOwnsCollectedRelic(guid, relic->itemId);
        uint8 activeSlot = (owned && playerData) ? sAbyssCultivationMgr->GetActiveRelicSlot(*playerData, relic->itemId) : 0;
        std::string iconPath = GetItemIconPathForAddon(relic->itemId);

        payload << relic->itemId << '^'
                << SanitizeAddonText(relic->name) << '^'
                << static_cast<uint32>(relic->relicType) << '^'
                << relic->relatedChapterId << '^'
                << static_cast<uint32>(relic->actId) << '^'
                << (owned ? 1 : 0) << '^'
                << static_cast<uint32>(relic->activeRule) << '^'
                << relic->exclusiveGroup << '^'
                << static_cast<uint32>(relic->recommendedSlot) << '^'
                << relic->subSlotScale << '^'
                << static_cast<uint32>(activeSlot) << '^'
                << SanitizeAddonText(relic->briefDescription) << '^'
                << SanitizeAddonText(iconPath);
    }

    SendAbyssPayload(player, payload.str());
}

void SendAbyssEquipmentsToAddon(Player* player)
{
    if (!player)
        return;

    std::vector<AbyssEquipmentTemplate const*> allEquipments;
    for (auto const& pair : sAbyssCultivationMgr->GetAllEquipmentTemplates())
        allEquipments.push_back(&pair.second);

    std::sort(allEquipments.begin(), allEquipments.end(), [](AbyssEquipmentTemplate const* a, AbyssEquipmentTemplate const* b)
    {
        if (a->equipmentType != b->equipmentType)
            return a->equipmentType > b->equipmentType;
        if (a->actId != b->actId)
            return a->actId < b->actId;
        if (a->sourceChapter != b->sourceChapter)
            return a->sourceChapter < b->sourceChapter;
        if (a->sourceMode != b->sourceMode)
            return a->sourceMode < b->sourceMode;
        if (a->slotMask != b->slotMask)
            return a->slotMask < b->slotMask;
        return a->itemId < b->itemId;
    });

    std::ostringstream payload;
    payload << "EQUIPMENTS:";
    bool first = true;
    for (AbyssEquipmentTemplate const* equipment : allEquipments)
    {
        if (!first)
            payload << '~';
        first = false;

        AbyssChapterConfig const* sourceChapter = sAbyssCultivationMgr->GetChapterConfig(equipment->sourceChapter);
        std::string iconPath = GetItemIconPathForAddon(equipment->itemId);

        payload << equipment->itemId << '^'
                << SanitizeAddonText(equipment->itemName) << '^'
                << static_cast<uint32>(equipment->equipmentType) << '^'
                << equipment->sourceChapter << '^'
                << SanitizeAddonText(sourceChapter ? sourceChapter->chapterName : std::string()) << '^'
                << static_cast<uint32>(equipment->sourceMode) << '^'
                << equipment->slotMask << '^'
                << static_cast<uint32>(equipment->actId) << '^'
                << equipment->baseItemLevel << '^'
                << (equipment->fromCacheBoss ? 1 : 0) << '^'
                << (equipment->requiresFragments ? 1 : 0) << '^'
                << SanitizeAddonText(equipment->flavorText) << '^'
                << SanitizeAddonText(iconPath);
    }

    SendAbyssPayload(player, payload.str());
}

void SendAbyssSetBonusesToAddon(Player* player)
{
    if (!player)
        return;

    std::ostringstream payload;
    payload << "SET_BONUSES:";
    bool first = true;
    for (auto const& [setId, config] : sAbyssCultivationMgr->GetSetBonusConfigs())
    {
        if (!config.enabled)
            continue;
        if (!first)
            payload << '~';
        first = false;

        payload << config.setId << '^'
                << SanitizeAddonText(config.setName) << '^'
                << static_cast<uint32>(config.actId) << '^'
                << static_cast<uint32>(config.sourceMode) << '^'
                << SanitizeAddonText(config.twoPieceDesc) << '^'
                << SanitizeAddonText(config.fourPieceDesc);
    }

    SendAbyssPayload(player, payload.str());
}

void SendAbyssRewardPreviewCategoryToAddon(Player* player, std::string const& header, AbyssChapterConfig const& chapter, uint32 bossEntry)
{
    if (!player)
        return;

    std::vector<AbyssRewardCandidate> candidates = sAbyssCultivationMgr->GetRewardCandidatesForBoss(chapter, bossEntry);
    std::ostringstream payload;
    payload << header << ':' << chapter.chapterId << '^' << SanitizeAddonText(chapter.chapterName);

    for (AbyssRewardCandidate const& docking : candidates)
    {
        payload << '~'
                << docking.itemId << '^'
                << SanitizeAddonText(docking.itemName) << '^'
                << static_cast<uint32>(docking.dockingType) << '^'
                << static_cast<uint32>(docking.quality) << '^'
                << static_cast<uint32>(docking.inventoryType) << '^'
                << static_cast<uint32>(docking.sourceMode) << '^'
                << docking.baseItemLevel;
    }

    SendAbyssPayload(player, payload.str());
}

void SendAbyssRewardPreviewToAddon(Player* player, AbyssChapterConfig const* chapter, char const* scope)
{
    if (!player || !chapter || !scope)
        return;

    std::string prefix(scope);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_ANCHOR", *chapter, chapter->anchorBossEntry);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_FINAL", *chapter, chapter->finalBossEntry);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_ABYSS", *chapter, chapter->abyssBossEntry);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_CACHE", *chapter, chapter->cacheBossEntry);
}

void SendAbyssRewardPreviewForChapterIdToAddon(Player* player, uint16 chapterId)
{
    if (!player || chapterId == 0)
        return;

    SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetChapterConfig(chapterId), "VIEW");
}

void SendAbyssAllToAddon(Player* player)
{
    SendAbyssStateToAddon(player);
    SendAbyssChapterListToAddon(player);
    SendAbyssRelicsToAddon(player);
    SendAbyssEquipmentsToAddon(player);

    if (player)
    {
        uint32 guid = player->GetGUID().GetCounter();
        if (PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid))
        {
            SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter), "CURRENT");
            SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter), "NEXT");
        }
    }
}

class AbyssCultivationBossAI : public ScriptedAI
{
public:
    explicit AbyssCultivationBossAI(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        events.Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (AbyssBossConfig const* bossConfig = sAbyssCultivationMgr->GetBossConfig(me->GetEntry()))
        {
            if (!bossConfig->introText.empty())
                me->Yell(bossConfig->introText, LANG_UNIVERSAL);

            if (!bossConfig->areaEffect.empty())
                events.ScheduleEvent(1, 12000);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (AbyssBossConfig const* bossConfig = sAbyssCultivationMgr->GetBossConfig(me->GetEntry()))
        {
            if (!bossConfig->deathText.empty())
                me->Yell(bossConfig->deathText, LANG_UNIVERSAL);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
        {
            if (eventId == 1)
            {
                if (AbyssBossConfig const* bossConfig = sAbyssCultivationMgr->GetBossConfig(me->GetEntry()))
                {
                    if (!bossConfig->areaEffect.empty())
                        me->Yell(bossConfig->areaEffect, LANG_UNIVERSAL);
                }

                events.ScheduleEvent(1, 18000);
            }
        }

        DoMeleeAttackIfReady();
    }
};

class AbyssCultivationCreatureScript : public AllCreatureScript
{
public:
    AbyssCultivationCreatureScript() : AllCreatureScript("AbyssCultivationCreatureScript") { }

    CreatureAI* GetCreatureAI(Creature* creature) const override
    {
        if (!IsModuleEnabled() || !creature)
            return nullptr;

        AbyssBossDocking const* docking = sAbyssCultivationMgr->GetBossDocking(creature->GetEntry());
        if (!docking || docking->bossType == 1)
            return nullptr;

        return new AbyssCultivationBossAI(creature);
    }

    void OnCreatureAddWorld(Creature* creature) override
    {
        if (!IsModuleEnabled() || !creature)
            return;

        if (sAbyssCultivationMgr->ApplyBossRuntimeTuning(creature))
        {
            _scaledCreatures[creature->GetGUID().GetCounter()] = true;
            _phaseState[creature->GetGUID().GetCounter()] = 0;
        }
    }

    void OnCreatureRemoveWorld(Creature* creature) override
    {
        if (!creature)
            return;

        uint32 key = creature->GetGUID().GetCounter();
        _scaledCreatures.erase(key);
        _phaseState.erase(key);
    }

    void OnAllCreatureUpdate(Creature* creature, uint32 /*diff*/) override
    {
        if (!IsModuleEnabled() || !creature || !creature->IsAlive())
            return;

        AbyssBossConfig const* bossConfig = sAbyssCultivationMgr->GetBossConfig(creature->GetEntry());
        if (!bossConfig)
            return;

        uint32 key = creature->GetGUID().GetCounter();
        if (!HasActiveAbyssContext(creature, bossConfig->chapterId))
        {
            _scaledCreatures.erase(key);
            _phaseState.erase(key);
            return;
        }

        if (_scaledCreatures.find(key) == _scaledCreatures.end())
            _scaledCreatures[key] = sAbyssCultivationMgr->ApplyBossRuntimeTuning(creature);

        if (!creature->IsInCombat())
        {
            _phaseState[key] = 0;
            return;
        }

        uint8& phase = _phaseState[key];
        if (phase == 0 && !bossConfig->phase1SkillGroup.empty())
        {
            creature->Yell(bossConfig->phase1SkillGroup, LANG_UNIVERSAL);
            phase = 1;
        }

        if (phase < 2 && bossConfig->phase2HealthPct > 0 && creature->HealthBelowPct(bossConfig->phase2HealthPct))
        {
            if (!bossConfig->phase2SkillGroup.empty())
                creature->Yell(bossConfig->phase2SkillGroup, LANG_UNIVERSAL);
            phase = 2;
        }

        if (phase < 3 && bossConfig->phase3HealthPct > 0 && creature->HealthBelowPct(bossConfig->phase3HealthPct))
        {
            if (!bossConfig->phase3SkillGroup.empty())
                creature->Yell(bossConfig->phase3SkillGroup, LANG_UNIVERSAL);
            phase = 3;
        }
    }

private:
    bool HasActiveAbyssContext(Creature* creature, uint16 chapterId) const
    {
        if (!creature || chapterId == 0)
            return false;

        if (creature->IsSummon())
        {
            if (TempSummon* summon = creature->ToTempSummon())
            {
                if (Unit* summoner = summon->GetSummonerUnit())
                {
                    if (Player* player = summoner->ToPlayer())
                    {
                        if (PlayerAbyssRunState const* runState = sAbyssCultivationMgr->GetPlayerRunState(player->GetGUID().GetCounter()))
                        {
                            return runState->currentChapterId == chapterId &&
                                runState->modeType >= 2 &&
                                runState->currentMapId == creature->GetMapId();
                        }
                    }
                }
            }
        }

        Map* map = creature->GetMap();
        if (!map)
            return false;

        for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player || !player->IsInWorld() || !player->IsAlive())
                continue;

            if (player->GetMapId() != creature->GetMapId())
                continue;

            if (player->GetDistance(creature) > 200.0f)
                continue;

            if (PlayerAbyssRunState const* runState = sAbyssCultivationMgr->GetPlayerRunState(player->GetGUID().GetCounter()))
            {
                if (runState->currentChapterId == chapterId &&
                    runState->modeType >= 2 &&
                    runState->currentMapId == creature->GetMapId())
                {
                    return true;
                }
            }
        }

        return false;
    }

    mutable std::unordered_map<uint32, bool> _scaledCreatures;
    mutable std::unordered_map<uint32, uint8> _phaseState;
};

class AbyssCultivationWorldScript : public WorldScript
{
public:
    AbyssCultivationWorldScript() : WorldScript("AbyssCultivationWorldScript",
        {
            WORLDHOOK_ON_AFTER_CONFIG_LOAD,
            WORLDHOOK_ON_UPDATE
        })
    {
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (!reload)
            return;

        if (!IsModuleEnabled())
        {
            sAbyssCultivationMgr->ClearWorldData();
            LOG_INFO("module", "mod-abyss-cultivation: disabled on config reload");
            return;
        }

        sAbyssCultivationMgr->LoadChapterConfigs();
        sAbyssCultivationMgr->LoadRelicConfigs();
        sAbyssCultivationMgr->LoadBossConfigs();
        sAbyssCultivationMgr->LoadEquipmentTemplates();
        sAbyssCultivationMgr->LoadSetBonusConfigs();
        sAbyssCultivationMgr->LoadAffixTemplates();
        sAbyssCultivationMgr->LoadSpecialEffectTemplates();
        sAbyssCultivationMgr->LoadTaskDockings();
        sAbyssCultivationMgr->LoadBossDockings();
        sAbyssCultivationMgr->LoadItemDockings();
        LOG_INFO("module", "mod-abyss-cultivation: world data reloaded");
    }

    void OnUpdate(uint32 diff) override
    {
        if (_initialized)
            return;

        _startupTimer += diff;
        if (_startupTimer < 1000)
            return;

        _initialized = true;

        if (!IsModuleEnabled())
        {
            sAbyssCultivationMgr->ClearWorldData();
            LOG_INFO("server.loading", ">> mod-abyss-cultivation: module disabled");
            return;
        }

        sAbyssCultivationMgr->LoadChapterConfigs();
        sAbyssCultivationMgr->LoadRelicConfigs();
        sAbyssCultivationMgr->LoadBossConfigs();
        sAbyssCultivationMgr->LoadEquipmentTemplates();
        sAbyssCultivationMgr->LoadSetBonusConfigs();
        sAbyssCultivationMgr->LoadAffixTemplates();
        sAbyssCultivationMgr->LoadSpecialEffectTemplates();
        sAbyssCultivationMgr->LoadTaskDockings();
        sAbyssCultivationMgr->LoadBossDockings();
        sAbyssCultivationMgr->LoadItemDockings();
        LOG_INFO("server.loading", ">> mod-abyss-cultivation: bootstrap ready");
    }

private:
    bool _initialized = false;
    uint32 _startupTimer = 0;
};

class AbyssCultivationPlayerScript : public PlayerScript
{
public:
    AbyssCultivationPlayerScript() : PlayerScript("AbyssCultivationPlayerScript",
        {
            PLAYERHOOK_ON_GIVE_EXP,
            PLAYERHOOK_ON_CREATURE_KILL,
            PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST,
            PLAYERHOOK_PASSED_QUEST_KILLED_MONSTER_CREDIT,
            PLAYERHOOK_ON_CHAT_WITH_RECEIVER,
            PLAYERHOOK_ON_LOAD_FROM_DB,
            PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_LOGOUT,
            PLAYERHOOK_ON_SPELL_CAST,
            PLAYERHOOK_ON_UPDATE,
            PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
            PLAYERHOOK_ON_AFTER_UPDATE_MAX_POWER,
            PLAYERHOOK_ON_AFTER_UPDATE_MAX_HEALTH,
            PLAYERHOOK_ON_AFTER_UPDATE_STAT,
            PLAYERHOOK_ON_AFTER_UPDATE_ATTACK_POWER_AND_DAMAGE,
            PLAYERHOOK_ON_AFTER_UPDATE_ARMOR,
            PLAYERHOOK_ON_AFTER_UPDATE_SPELL_DAMAGE_AND_HEALING,
            PLAYERHOOK_ON_AFTER_UPDATE_RATING,
            PLAYERHOOK_ON_DELETE,
            PLAYERHOOK_ON_EQUIP,
            PLAYERHOOK_ON_AFTER_MOVE_ITEM_FROM_INVENTORY
        })
    {
    }

    void OnPlayerLoadFromDB(Player* player) override
    {
        if (!player || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->LoadPlayerData(player, false);
        sAbyssCultivationMgr->LoadPlayerChapterModeUnlocks(player);
        sAbyssCultivationMgr->LoadPlayerCollections(player);
        sAbyssCultivationMgr->LoadPlayerRunState(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !IsModuleEnabled())
            return;

        bool changed = sAbyssCultivationMgr->SyncPlayerData(player, true);
        sAbyssCultivationMgr->RefreshPlayerRuntimeStats(player);
        sAbyssCultivationMgr->RefreshPlayerSetBonuses(player);

        if (!IsDebugEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid);
        if (!playerData)
            return;

        AbyssChapterConfig const* chapterConfig = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
        LOG_DEBUG("module", "mod-abyss-cultivation: player {} ({}) login currentChapter={} chapterName='{}' worldLoaded={}",
            player->GetName(),
            guid,
            playerData->currentChapter,
            chapterConfig ? chapterConfig->chapterName : std::string("n/a"),
            sAbyssCultivationMgr->HasLoadedWorldData());

        if (changed)
        {
            LOG_DEBUG("module", "mod-abyss-cultivation: normalized player {} ({}) data on login",
                player->GetName(), guid);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        _runCheckTimers.erase(guid);
        if (!sAbyssCultivationMgr->HasCachedPlayerData(guid))
            return;

        sAbyssCultivationMgr->SavePlayerData(player);
        sAbyssCultivationMgr->SavePlayerRunState(player);
        sAbyssCultivationMgr->ClearPlayerData(guid);
        sAbyssCultivationMgr->ClearPlayerRunState(guid);
        sAbyssCultivationMgr->ClearPlayerProcState(guid);
        sAbyssCultivationMgr->CleanupPlayerSetBonuses(guid);
    }

    void OnPlayerEquip(Player* player, Item* /*it*/, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        if (!player || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->RefreshPlayerSetBonuses(player);
    }

    void OnPlayerAfterMoveItemFromInventory(Player* player, Item* /*it*/, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        if (!player || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->RefreshPlayerSetBonuses(player);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        _runCheckTimers.erase(guid.GetCounter());
        sAbyssCultivationMgr->DeletePlayerData(guid.GetCounter());
    }

    void OnPlayerCreatureKill(Player* player, Creature* creature) override
    {
        if (!player || !creature || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->TryRestoreDefaultLootForNonAbyssKill(player, creature);
        sAbyssCultivationMgr->HandleCreatureKill(player, creature);
    }

    bool OnPlayerPassedQuestKilledMonsterCredit(Player* player, Quest const* qinfo, uint32 entry, uint32 realEntry, ObjectGuid /*guid*/) override
    {
        if (!player || !qinfo || !IsModuleEnabled())
            return true;

        AbyssTaskDocking const* docking = sAbyssCultivationMgr->GetTaskDocking(qinfo->GetQuestId());
        if (!docking || docking->taskType < 2 || docking->taskType > 5)
            return true;

        AbyssChapterConfig const* chapterConfig = sAbyssCultivationMgr->GetChapterConfig(docking->chapterId);
        if (!chapterConfig)
            return false;

        uint32 requiredEntry = 0;
        if (docking->taskType == 2)
            requiredEntry = chapterConfig->anchorBossEntry;
        else if (docking->taskType == 3)
            requiredEntry = chapterConfig->anchorBossEntry;
        else if (docking->taskType == 4)
            requiredEntry = chapterConfig->abyssBossEntry;
        else
            requiredEntry = chapterConfig->abyssBossEntry;

        if (requiredEntry == 0)
            return false;

        if (docking->taskType >= 3)
        {
            PlayerAbyssRunState const* runState = sAbyssCultivationMgr->GetPlayerRunState(player->GetGUID().GetCounter());
            if (!runState || runState->currentChapterId != docking->chapterId)
                return false;

            uint8 requiredMode = docking->taskType == 3 ? 1 : (docking->taskType == 4 ? 2 : 3);
            if (runState->modeType != requiredMode)
                return false;
        }

        return entry == requiredEntry || realEntry == requiredEntry;
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->HandlePlayerSpellCast(player, spell);
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || !IsModuleEnabled() || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != ABYSS_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "REQ_ALL")
        {
            SendAbyssAllToAddon(player);
            return;
        }

        if (command == "REQ_STATE")
        {
            sAbyssCultivationMgr->SyncPlayerData(player, false);
            SendAbyssStateToAddon(player);
            return;
        }

        if (command == "REQ_CHAPTERS")
        {
            SendAbyssChapterListToAddon(player);
            return;
        }

        if (command == "REQ_RELICS")
        {
            sAbyssCultivationMgr->LoadPlayerCollections(player);
            SendAbyssRelicsToAddon(player);
            return;
        }

        if (command == "REQ_EQUIPMENTS")
        {
            SendAbyssEquipmentsToAddon(player);
            SendAbyssSetBonusesToAddon(player);
            return;
        }

        if (command.rfind("COLLECT_RELIC:", 0) == 0)
        {
            uint32 itemId = static_cast<uint32>(std::strtoul(command.substr(14).c_str(), nullptr, 10));
            std::string failureReason;
            bool ok = sAbyssCultivationMgr->CollectRelicFromInventory(player, itemId, &failureReason);
            SendAbyssResult(player, "COLLECT_RELIC", ok, ok ? "ok" : failureReason);
            SendAbyssAllToAddon(player);
            return;
        }

        if (command == "REQ_REWARD_CURRENT")
        {
            if (PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter()))
                SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter), "CURRENT");
            return;
        }

        if (command == "REQ_REWARD_NEXT")
        {
            if (PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter()))
                SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter), "NEXT");
            return;
        }

        if (command.rfind("REQ_REWARD_CHAPTER:", 0) == 0)
        {
            uint16 chapterId = static_cast<uint16>(std::strtoul(command.substr(17).c_str(), nullptr, 10));
            SendAbyssRewardPreviewForChapterIdToAddon(player, chapterId);
            return;
        }

        if (command.rfind("ENTER:", 0) == 0)
        {
            uint32 chapterId = 0;
            uint32 modeType = 1;
            uint32 corruptionTier = 0;
            std::string args = command.substr(6);
            std::istringstream stream(args);
            if (!(stream >> chapterId))
                chapterId = 0;
            if (!(stream >> modeType))
                modeType = 1;
            if (!(stream >> corruptionTier))
                corruptionTier = 0;

            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter());
            if (chapterId == 0)
                chapterId = playerData ? playerData->currentChapter : 0;

            std::string failureReason;
            bool ok = sAbyssCultivationMgr->BeginPlayerRunAndTeleport(player, static_cast<uint16>(chapterId), static_cast<uint8>(modeType), static_cast<uint16>(corruptionTier), &failureReason);
            SendAbyssResult(player, "ENTER", ok, ok ? "ok" : failureReason);
            SendAbyssAllToAddon(player);
            return;
        }

        if (command == "LEAVE")
        {
            bool ok = sAbyssCultivationMgr->EndPlayerRun(player);
            SendAbyssResult(player, "LEAVE", ok, ok ? "ok" : "no_run");
            SendAbyssAllToAddon(player);
            return;
        }

        if (command.rfind("SET_RELIC:", 0) == 0)
        {
            std::string args = command.substr(10);
            std::istringstream stream(args);
            std::string slotToken;
            uint32 itemId = 0;
            stream >> slotToken >> itemId;

            uint8 slot = 0;
            std::string failureReason;
            bool ok = TryParseRelicSlotToken(slotToken, slot) &&
                sAbyssCultivationMgr->SetPlayerRelicSlot(player, slot, itemId, &failureReason);
            if (!ok && failureReason.empty())
                failureReason = "invalid_args";

            SendAbyssResult(player, "SET_RELIC", ok, ok ? "ok" : failureReason);
            SendAbyssAllToAddon(player);
            return;
        }

        if (command.rfind("CLEAR_RELIC:", 0) == 0)
        {
            std::string slotToken = command.substr(12);
            uint8 slot = 0;
            std::string failureReason;
            bool ok = TryParseRelicSlotToken(slotToken, slot) &&
                sAbyssCultivationMgr->SetPlayerRelicSlot(player, slot, 0, &failureReason);
            if (!ok && failureReason.empty())
                failureReason = "invalid_args";

            SendAbyssResult(player, "CLEAR_RELIC", ok, ok ? "ok" : failureReason);
            SendAbyssAllToAddon(player);
            return;
        }

    }

    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* player) override
    {
        if (!player || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->EndPlayerRun(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player || !IsModuleEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32& timer = _runCheckTimers[guid];
        if (timer > diff)
        {
            timer -= diff;
            return;
        }

        timer = 3000;
        if (sAbyssCultivationMgr->EnsurePlayerRunLocation(player))
            return;

        if (IsAutoBeginOnMapEnterEnabled())
            sAbyssCultivationMgr->TryAutoBeginPlayerRun(player);
        sAbyssCultivationMgr->HandlePlayerUpdate(player, diff);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeXpBonusPct(player);
        if (bonusPct > 0.0f)
            amount = static_cast<uint32>(static_cast<float>(amount) * (1.0f + bonusPct / 100.0f));
    }

    void OnPlayerAfterUpdateStat(Player* player, Stats stat, float& value) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeStatBonusPct(player);
        if (bonusPct != 0.0f)
            value *= (1.0f + bonusPct / 100.0f);

        int32 flatBonus = sAbyssCultivationMgr->GetPlayerRuntimeRelicStatFlatBonus(player, stat);
        if (flatBonus != 0)
            value += static_cast<float>(flatBonus);
    }

    void OnPlayerAfterUpdateMaxPower(Player* player, Powers& /*power*/, float& value) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeStatBonusPct(player);
        if (bonusPct != 0.0f)
            value *= (1.0f + bonusPct / 100.0f);
    }

    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeHealthBonusPct(player);
        if (bonusPct != 0.0f)
            value *= (1.0f + bonusPct / 100.0f);
    }

    void OnPlayerAfterUpdateAttackPowerAndDamage(Player* player, float& /*level*/, float& /*base_attPower*/, float& attPowerMod, float& attPowerMultiplier, bool /*ranged*/) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeAttackBonusPct(player);
        if (bonusPct != 0.0f)
            attPowerMultiplier += bonusPct / 100.0f;

        int32 flatAttackPower = sAbyssCultivationMgr->GetPlayerRuntimeRelicAttackPowerBonus(player);
        if (flatAttackPower != 0)
            attPowerMod += static_cast<float>(flatAttackPower);
    }

    void OnPlayerAfterUpdateArmor(Player* player, float& value) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeArmorBonusPct(player);
        if (bonusPct != 0.0f)
            value *= (1.0f + bonusPct / 100.0f);
    }

    void OnPlayerAfterUpdateSpellDamageAndHealing(Player* player, int32& healingBonus, int32 spellDamage[7]) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeSpellBonusPct(player);
        if (bonusPct == 0.0f)
            return;

        float multiplier = 1.0f + bonusPct / 100.0f;
        healingBonus = static_cast<int32>(static_cast<float>(healingBonus) * multiplier);
        for (uint8 school = 0; school < 7; ++school)
            spellDamage[school] = static_cast<int32>(static_cast<float>(spellDamage[school]) * multiplier);

        int32 flatSpellPower = sAbyssCultivationMgr->GetPlayerRuntimeRelicSpellPowerBonus(player);
        if (flatSpellPower != 0)
        {
            healingBonus += flatSpellPower;
            for (uint8 school = 0; school < 7; ++school)
                spellDamage[school] += flatSpellPower;
        }
    }

    void OnPlayerAfterUpdateRating(Player* player, CombatRating cr, int32& amount) override
    {
        if (!player || !IsModuleEnabled())
            return;

        amount += sAbyssCultivationMgr->GetPlayerRuntimeRelicRatingBonus(player, cr);
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (!player || !quest || !IsModuleEnabled())
            return;

        sAbyssCultivationMgr->HandleQuestCompletion(player, quest);
    }

private:
    std::unordered_map<uint32, uint32> _runCheckTimers;
};

class AbyssCultivationCommandScript : public CommandScript
{
public:
    AbyssCultivationCommandScript() : CommandScript("AbyssCultivationCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable abyssSubTable =
        {
            { "审计",       HandleAuditCommand,      SEC_GAMEMASTER,    Console::No  },
            { "进入",       HandleEnterCommand,      SEC_GAMEMASTER,    Console::No  },
            { "离开",       HandleLeaveCommand,      SEC_GAMEMASTER,    Console::No  },
            { "缺失",       HandleMissingCommand,    SEC_GAMEMASTER,    Console::No  },
            { "模式",       HandleModesCommand,      SEC_GAMEMASTER,    Console::No  },
            { "遗物",       HandleRelicCommand,      SEC_GAMEMASTER,    Console::No  },
            { "奖励",       HandleRewardCommand,     SEC_GAMEMASTER,    Console::No  },
            { "补首领",     HandleSeedBossCommand,   SEC_GAMEMASTER,    Console::No  },
            { "补首领SQL",  HandleSeedBossSqlCommand, SEC_GAMEMASTER,   Console::No  },
            { "补物品",     HandleSeedItemCommand,   SEC_GAMEMASTER,    Console::No  },
            { "补物品SQL",  HandleSeedItemSqlCommand, SEC_GAMEMASTER,   Console::No  },
            { "补任务",     HandleSeedQuestCommand,  SEC_GAMEMASTER,    Console::No  },
            { "补任务SQL",  HandleSeedQuestSqlCommand, SEC_GAMEMASTER,  Console::No  },
            { "局内",       HandleRunCommand,        SEC_GAMEMASTER,    Console::No  },
            { "状态",       HandleStateCommand,      SEC_GAMEMASTER,    Console::No  },
            { "重载",       HandleReloadCommand,     SEC_ADMINISTRATOR, Console::Yes },
            { "同步",       HandleSyncCommand,       SEC_GAMEMASTER,    Console::No  },
            { "设章节",     HandleSetChapterCommand, SEC_GAMEMASTER,    Console::No  }
        };

        static ChatCommandTable rootTable =
        {
            { "深渊", abyssSubTable }
        };

        return rootTable;
    }

    static bool HandleStateCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        sAbyssCultivationMgr->SyncPlayerData(target, false);

        uint32 guid = target->GetGUID().GetCounter();
        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid);
        if (!playerData)
        {
            handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
            return true;
        }

        AbyssChapterConfig const* currentChapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
        AbyssChapterConfig const* highestChapter = sAbyssCultivationMgr->GetChapterConfig(playerData->highestChapter);
        AbyssChapterConfig const* nextChapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);

        handler->PSendSysMessage("Abyss state for {}:", target->GetName());
        handler->PSendSysMessage("  guid={} level={} currentChapter={} highestChapter={} threshold={} storyState={} modeMask={}",
            guid,
            static_cast<uint32>(target->GetLevel()),
            playerData->currentChapter,
            playerData->highestChapter,
            playerData->currentCultivationThreshold,
            playerData->storyState,
            playerData->unlockedModeMask);
        handler->PSendSysMessage("  corruption={} preset={} pityChapter={} lastUpdated={}",
            playerData->highestCorruptionTier,
            static_cast<uint32>(playerData->presetIndex),
            playerData->pityChapterId,
            playerData->lastUpdated);
        handler->PSendSysMessage("  relics={} / {} / {} / {} / {} / {} artifacts={} / {}",
            playerData->mainRelic,
            playerData->subRelic1,
            playerData->subRelic2,
            playerData->subRelic3,
            playerData->subRelic4,
            playerData->subRelic5,
            playerData->phaseArtifact,
            playerData->ultimateArtifact);
        handler->PSendSysMessage("  failCounts=abyss:{} cacheBoss:{}",
            playerData->abyssGearFailCount,
            playerData->cacheBossFailCount);
        handler->PSendSysMessage("  worldLoaded={} chapterCacheCount={} relics={} bossConfigs={} equipTemplates={} affixTemplates={} specialEffects={} taskDockings={} bossDockings={} itemDockings={}",
            sAbyssCultivationMgr->HasLoadedWorldData(),
            sAbyssCultivationMgr->GetChapterCount(),
            sAbyssCultivationMgr->GetRelicConfigCount(),
            sAbyssCultivationMgr->GetBossConfigCount(),
            sAbyssCultivationMgr->GetEquipmentTemplateCount(),
            sAbyssCultivationMgr->GetAffixTemplateCount(),
            sAbyssCultivationMgr->GetSpecialEffectTemplateCount(),
            sAbyssCultivationMgr->GetTaskDockingCount(),
            sAbyssCultivationMgr->GetBossDockingCount(),
            sAbyssCultivationMgr->GetItemDockingCount());
        handler->PSendSysMessage("  currentChapterName={} highestChapterName={}",
            currentChapter ? currentChapter->chapterName : std::string("n/a"),
            highestChapter ? highestChapter->chapterName : std::string("n/a"));

        PrintRunState(handler, target, playerData);

        if (currentChapter)
            PrintChapterDetails(handler, target, *playerData, *currentChapter, "current");

        if (nextChapter)
            PrintChapterDetails(handler, target, *playerData, *nextChapter, "next");

        return true;
    }

    static bool HandleRunCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
        if (!playerData)
        {
            handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
            return true;
        }

        PrintRunState(handler, target, playerData);
        return true;
    }

    static bool HandleEnterCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string chapterToken = "当前";
        uint32 modeType = 1;
        uint32 corruptionTier = 0;

        if (args && *args != '\0')
        {
            std::istringstream stream(args);
            if (!(stream >> chapterToken))
                chapterToken = "当前";
            if (!(stream >> modeType))
                modeType = 1;
            if (!(stream >> corruptionTier))
                corruptionTier = 0;
        }

        uint16 chapterId = 0;
        if (!TryResolveChapterToken(target, chapterToken, chapterId))
        {
            handler->SendSysMessage("用法：.深渊 进入 [当前|下一章|章节ID] [模式类型] [腐化层]");
            return true;
        }

        std::string failureReason;
        bool started = sAbyssCultivationMgr->BeginPlayerRun(
            target,
            chapterId,
            static_cast<uint8>(std::min<uint32>(modeType, 255u)),
            static_cast<uint16>(std::min<uint32>(corruptionTier, 65535u)),
            &failureReason);

        if (!started)
        {
            handler->PSendSysMessage("Failed to enter abyss run for {} chapter {}: {}",
                target->GetName(), static_cast<uint32>(chapterId), failureReason);
            return true;
        }

        AbyssChapterConfig const* chapterConfig = sAbyssCultivationMgr->GetChapterConfig(chapterId);
        handler->PSendSysMessage("Entered abyss run for {} chapter {} ({}) mode={} corruption={} expectedMap={} currentMap={}",
            target->GetName(),
            static_cast<uint32>(chapterId),
            chapterConfig ? chapterConfig->chapterName : std::string("n/a"),
            GetModeTypeName(static_cast<uint8>(std::min<uint32>(modeType, 255u))),
            std::min<uint32>(corruptionTier, 65535u),
            chapterConfig ? chapterConfig->mapId : 0u,
            target->GetMapId());
        PrintRunState(handler, target, sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter()));
        return true;
    }

    static bool HandleLeaveCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        bool ended = sAbyssCultivationMgr->EndPlayerRun(target);
        handler->PSendSysMessage("Leave abyss run for {}: {}", target->GetName(), ended ? "cleared" : "no_active_run");
        return true;
    }

    static bool HandleModesCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
        if (!playerData)
        {
            handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
            return true;
        }

        handler->PSendSysMessage("Abyss modes for {}: mask={} story={} abyss={} corruption={} reincarnation={}",
            target->GetName(),
            playerData->unlockedModeMask,
            (playerData->unlockedModeMask & GetModeTypeMask(1)) != 0,
            (playerData->unlockedModeMask & GetModeTypeMask(2)) != 0,
            (playerData->unlockedModeMask & GetModeTypeMask(3)) != 0,
            (playerData->unlockedModeMask & GetModeTypeMask(4)) != 0);
        return true;
    }

    static bool HandleRelicCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        sAbyssCultivationMgr->LoadPlayerCollections(target);
        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
        if (!playerData)
        {
            handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
            return true;
        }

        std::string action;
        std::string slotToken;
        uint32 itemId = 0;
        if (args && *args != '\0')
        {
            std::istringstream stream(args);
            stream >> action >> slotToken >> itemId;
        }

        if (IsRelicStateActionToken(action))
        {
            handler->PSendSysMessage("{} 的遗物槽位：主={} 副1={} 副2={} 副3={} 副4={} 副5={} 阶段={} 终极={}",
                target->GetName(),
                playerData->mainRelic,
                playerData->subRelic1,
                playerData->subRelic2,
                playerData->subRelic3,
                playerData->subRelic4,
                playerData->subRelic5,
                playerData->phaseArtifact,
                playerData->ultimateArtifact);
            handler->SendSysMessage("用法：.深渊 遗物 列表 | .深渊 遗物 设置 <主|副1|副2|副3|副4|副5|阶段|终极> <物品ID> | .深渊 遗物 清空 <槽位>");
            return true;
        }

        if (IsRelicListActionToken(action))
        {
            std::vector<PlayerAbyssCollectionEntry> const& collection = sAbyssCultivationMgr->GetPlayerCollections(target->GetGUID().GetCounter());
            handler->PSendSysMessage("{} 的遗物收藏：数量={}", target->GetName(), collection.size());
            for (PlayerAbyssCollectionEntry const& entry : collection)
            {
                AbyssRelicConfig const* relic = sAbyssCultivationMgr->GetRelicConfig(entry.itemId);
                if (!relic)
                    continue;

                uint8 activeSlot = sAbyssCultivationMgr->GetActiveRelicSlot(*playerData, entry.itemId);
                handler->PSendSysMessage("  itemId={} name={} type={} chapter={} activeSlot={} exclusiveGroup={} recommendedSlot={} subScale={}",
                    entry.itemId,
                    relic->name,
                    static_cast<uint32>(relic->relicType),
                    entry.relatedChapterId,
                    GetRelicSlotName(activeSlot),
                    relic->exclusiveGroup,
                    static_cast<uint32>(relic->recommendedSlot),
                    relic->subSlotScale);
            }
            return true;
        }

        uint8 slot = 0;
        if (!TryParseRelicSlotToken(slotToken, slot))
        {
            handler->SendSysMessage("无效的遗物槽位，请使用：主|副1|副2|阶段|终极。");
            return true;
        }

        std::string failureReason;
        bool ok = false;
        if (IsRelicSetActionToken(action))
            ok = sAbyssCultivationMgr->SetPlayerRelicSlot(target, slot, itemId, &failureReason);
        else if (IsRelicClearActionToken(action))
            ok = sAbyssCultivationMgr->SetPlayerRelicSlot(target, slot, 0, &failureReason);
        else
        {
            handler->SendSysMessage("用法：.深渊 遗物 列表 | .深渊 遗物 设置 <槽位> <物品ID> | .深渊 遗物 清空 <槽位>");
            return true;
        }

        handler->PSendSysMessage("遗物命令结果：玩家={} 操作={} 槽位={} 物品ID={} 结果={}",
            target->GetName(),
            action,
            GetRelicSlotName(slot),
            itemId,
            ok ? std::string("ok") : failureReason);
        return true;
    }

    static bool HandleRewardCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
        if (!playerData)
        {
            handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
            return true;
        }

        AbyssChapterConfig const* chapter = nullptr;
        if (IsCurrentScopeToken(scope))
            chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
        else if (IsNextScopeToken(scope))
            chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);

        if (!chapter)
        {
            handler->SendSysMessage("用法：.深渊 奖励 [当前|下一章]");
            return true;
        }

        PrintRewardPreview(handler, *chapter);
        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        sAbyssCultivationMgr->LoadChapterConfigs();
        sAbyssCultivationMgr->LoadRelicConfigs();
        sAbyssCultivationMgr->LoadBossConfigs();
        sAbyssCultivationMgr->LoadEquipmentTemplates();
        sAbyssCultivationMgr->LoadSetBonusConfigs();
        sAbyssCultivationMgr->LoadAffixTemplates();
        sAbyssCultivationMgr->LoadSpecialEffectTemplates();
        sAbyssCultivationMgr->LoadTaskDockings();
        sAbyssCultivationMgr->LoadBossDockings();
        sAbyssCultivationMgr->LoadItemDockings();
        handler->PSendSysMessage("mod-abyss-cultivation chapter cache reloaded: {} rows.",
            sAbyssCultivationMgr->GetChapterCount());
        handler->PSendSysMessage("mod-abyss-cultivation relics: {}, boss configs: {}, equipTemplates: {}, affixTemplates: {}, specialEffects: {}.",
            sAbyssCultivationMgr->GetRelicConfigCount(),
            sAbyssCultivationMgr->GetBossConfigCount(),
            sAbyssCultivationMgr->GetEquipmentTemplateCount(),
            sAbyssCultivationMgr->GetAffixTemplateCount(),
            sAbyssCultivationMgr->GetSpecialEffectTemplateCount());
        handler->PSendSysMessage("mod-abyss-cultivation task dockings: {}, custom boss dockings: {}, item dockings: {}.",
            sAbyssCultivationMgr->GetTaskDockingCount(),
            sAbyssCultivationMgr->GetBossDockingCount(),
            sAbyssCultivationMgr->GetItemDockingCount());
        return true;
    }

    static bool HandleSetChapterCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        if (!args)
        {
            handler->SendSysMessage("用法：.深渊 设章节 <章节ID|0>");
            return true;
        }

        while (*args == ' ')
            ++args;

        if (*args == '\0')
        {
            handler->SendSysMessage("用法：.深渊 设章节 <章节ID|0>");
            return true;
        }

        char* end = nullptr;
        unsigned long parsed = std::strtoul(args, &end, 10);
        while (end && *end == ' ')
            ++end;

        if (args == end || (end && *end != '\0') || parsed > 65535UL)
        {
            handler->SendSysMessage("Invalid chapter id.");
            return true;
        }

        uint16 chapterId = static_cast<uint16>(parsed);
        if (!sAbyssCultivationMgr->SetPlayerCurrentChapter(target, chapterId))
        {
            handler->PSendSysMessage("Failed to set chapter {} for {}. Check if the chapter id exists in the abyss chapter table.",
                static_cast<uint32>(chapterId), target->GetName());
            return true;
        }

        AbyssChapterConfig const* chapterConfig = sAbyssCultivationMgr->GetChapterConfig(chapterId);
        handler->PSendSysMessage("Set {} current abyss chapter to {} ({})",
            target->GetName(),
            static_cast<uint32>(chapterId),
            chapterConfig ? chapterConfig->chapterName : std::string("reset"));
        return true;
    }

    static bool HandleSyncCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        bool changed = sAbyssCultivationMgr->SyncPlayerData(target, true);
        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
        if (!playerData)
        {
            handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
            return true;
        }

        handler->PSendSysMessage("Synced abyss data for {}: changed={} currentChapter={} highestChapter={} storyState={}",
            target->GetName(),
            changed,
            playerData->currentChapter,
            playerData->highestChapter,
            static_cast<uint32>(playerData->storyState));
        return true;
    }

    static bool HandleMissingCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintMissingForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintMissingForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintMissingForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 缺失 [当前|下一章|全部]");
        return true;
    }

    static bool HandleAuditCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintAuditForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintAuditForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintAuditForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 审计 [当前|下一章|全部]");
        return true;
    }

    static bool HandleSeedQuestCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintSeedQuestForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintSeedQuestForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintSeedQuestForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 补任务 [当前|下一章|全部]");
        return true;
    }

    static bool HandleSeedQuestSqlCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintSeedQuestSqlForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintSeedQuestSqlForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintSeedQuestSqlForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 补任务SQL [当前|下一章|全部]");
        return true;
    }

    static bool HandleSeedItemCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintSeedItemForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintSeedItemForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintSeedItemForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 补物品 [当前|下一章|全部]");
        return true;
    }

    static bool HandleSeedItemSqlCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintSeedItemSqlForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintSeedItemSqlForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintSeedItemSqlForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 补物品SQL [当前|下一章|全部]");
        return true;
    }

    static bool HandleSeedBossCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintSeedBossForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintSeedBossForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintSeedBossForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 补首领 [当前|下一章|全部]");
        return true;
    }

    static bool HandleSeedBossSqlCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("mod-abyss-cultivation is disabled.");
            return true;
        }

        Player* target = GetSelectedPlayerOrSelf(handler);
        if (!target)
        {
            handler->SendSysMessage("This command requires an online player target.");
            return true;
        }

        std::string scope = "当前";
        if (args)
        {
            while (*args == ' ')
                ++args;

            if (*args != '\0')
                scope = args;
        }

        if (IsCurrentScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 当前没有深渊章节。", target->GetName());
                return true;
            }

            PrintSeedBossSqlForChapter(handler, *chapter, "current");
            return true;
        }

        if (IsNextScopeToken(scope))
        {
            PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(target->GetGUID().GetCounter());
            if (!playerData)
            {
                handler->PSendSysMessage("Abyss data for player {} is not loaded.", target->GetName());
                return true;
            }

            AbyssChapterConfig const* chapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!chapter)
            {
                handler->PSendSysMessage("玩家 {} 没有下一章深渊章节。", target->GetName());
                return true;
            }

            PrintSeedBossSqlForChapter(handler, *chapter, "next");
            return true;
        }

        if (IsAllScopeToken(scope))
        {
            PrintSeedBossSqlForAllChapters(handler);
            return true;
        }

        handler->SendSysMessage("用法：.深渊 补首领SQL [当前|下一章|全部]");
        return true;
    }

private:
    static uint8 ParseChoiceIndex(char const* args)
    {
        if (!args)
            return 0;

        while (*args == ' ')
            ++args;

        if (*args == '\0')
            return 0;

        char* end = nullptr;
        unsigned long parsed = std::strtoul(args, &end, 10);
        while (end && *end == ' ')
            ++end;

        if (args == end || (end && *end != '\0') || parsed == 0 || parsed > 255UL)
            return 0;

        return static_cast<uint8>(parsed);
    }

    static bool TryResolveChapterToken(Player* target, std::string const& token, uint16& chapterId)
    {
        if (!target)
            return false;

        uint32 guid = target->GetGUID().GetCounter();
        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid);
        if (!playerData)
            return false;

        if (IsCurrentScopeToken(token))
        {
            chapterId = playerData->currentChapter;
            return chapterId != 0;
        }

        if (IsNextScopeToken(token))
        {
            AbyssChapterConfig const* nextChapter = sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter);
            if (!nextChapter)
                return false;

            chapterId = nextChapter->chapterId;
            return true;
        }

        char* end = nullptr;
        unsigned long parsed = std::strtoul(token.c_str(), &end, 10);
        if (token.c_str() == end || (end && *end != '\0') || parsed == 0 || parsed > 65535UL)
            return false;

        chapterId = static_cast<uint16>(parsed);
        return sAbyssCultivationMgr->GetChapterConfig(chapterId) != nullptr;
    }

    static void PrintRewardCandidateList(ChatHandler* handler, char const* label, std::vector<AbyssRewardCandidate> const& candidates)
    {
        handler->PSendSysMessage("  {} count={}", label, candidates.size());
        for (AbyssRewardCandidate const& docking : candidates)
        {
            handler->PSendSysMessage("    itemId={} name={} type={} quality={} invType={} unique={} bagActivated={} sourceMode={} baseIlvl={}",
                docking.itemId,
                docking.itemName,
                GetItemDockingTypeName(docking.dockingType),
                static_cast<uint32>(docking.quality),
                static_cast<uint32>(docking.inventoryType),
                docking.uniqueItem,
                docking.bagActivated,
                static_cast<uint32>(docking.sourceMode),
                docking.baseItemLevel);
        }
    }

    static void PrintRewardPreview(ChatHandler* handler, AbyssChapterConfig const& chapter)
    {
        if (!handler)
            return;

        handler->PSendSysMessage("Reward preview for chapter {} ({})", chapter.chapterId, chapter.chapterName);
        handler->PSendSysMessage("  dropRate story={} abyss={} corruption={} reincarnation={} hiddenBonus={} cacheBonus={}",
            chapter.storyDropRate,
            chapter.abyssDropRate,
            chapter.corruptionDropRate,
            chapter.reincarnationDropRate,
            chapter.hiddenRoomBonus,
            chapter.cacheBossRewardBonus);

        PrintRewardCandidateList(handler, "anchorRewards", sAbyssCultivationMgr->GetRewardCandidatesForBoss(chapter, chapter.anchorBossEntry));
        PrintRewardCandidateList(handler, "finalRewards", sAbyssCultivationMgr->GetRewardCandidatesForBoss(chapter, chapter.finalBossEntry));
        PrintRewardCandidateList(handler, "abyssRewards", sAbyssCultivationMgr->GetRewardCandidatesForBoss(chapter, chapter.abyssBossEntry));
        PrintRewardCandidateList(handler, "cacheRewards", sAbyssCultivationMgr->GetRewardCandidatesForBoss(chapter, chapter.cacheBossEntry));
    }

    static void PrintRunState(ChatHandler* handler, Player* target, PlayerAbyssData const* playerData)
    {
        if (!handler || !target || !playerData)
            return;

        PlayerAbyssRunState const* runState = sAbyssCultivationMgr->GetPlayerRunState(target->GetGUID().GetCounter());
        if (!runState)
        {
            handler->SendSysMessage("  activeRun: none");
            return;
        }

        AbyssChapterConfig const* chapterConfig = sAbyssCultivationMgr->GetChapterConfig(runState->currentChapterId);
        bool abyssReady = chapterConfig && !runState->abyssBossSummoned && sAbyssCultivationMgr->IsAbyssSummonReady(*chapterConfig, *runState);

        handler->PSendSysMessage("  activeRun chapter={} name={} mode={} map={} corruption={} started={}",
            runState->currentChapterId,
            chapterConfig ? chapterConfig->chapterName : std::string("n/a"),
            GetModeTypeName(runState->modeType),
            runState->currentMapId,
            runState->corruptionTier,
            runState->startTime);
        handler->PSendSysMessage("  activeRun relics={} / {} / {} / {} / {} / {} killMask={} abyssSummoned={} cacheSummoned={} abyssReady={}",
            runState->runMainRelic,
            runState->runSubRelic1,
            runState->runSubRelic2,
            runState->runSubRelic3,
            runState->runSubRelic4,
            runState->runSubRelic5,
            runState->anchorBossKillMask,
            runState->abyssBossSummoned,
            runState->cacheBossSummoned,
            abyssReady);
        if (AbyssRelicConfig const* mainRelic = sAbyssCultivationMgr->GetRelicConfig(runState->runMainRelic))
            handler->PSendSysMessage("  activeRun mainRelic name={} family={} script={} slot={} rule={} desc={}",
                mainRelic->name,
                mainRelic->effectFamily,
                mainRelic->scriptGroup,
                static_cast<uint32>(mainRelic->activeSlot),
                static_cast<uint32>(mainRelic->activeRule),
                mainRelic->briefDescription);

        if (chapterConfig)
        {
            handler->PSendSysMessage("  activeRun summon anchor={} final={} abyss={} cache={} triggerType={} triggerMask={} cacheChance={} cachePity={} playerCacheFails={}",
                chapterConfig->anchorBossEntry,
                chapterConfig->finalBossEntry,
                chapterConfig->abyssBossEntry,
                chapterConfig->cacheBossEntry,
                static_cast<uint32>(chapterConfig->triggerType),
                chapterConfig->requiredBossKillMask,
                chapterConfig->cacheBossBaseChance,
                chapterConfig->cacheBossPityCount,
                playerData->cacheBossFailCount);

            if (AbyssBossConfig const* abyssBoss = sAbyssCultivationMgr->GetBossConfig(chapterConfig->abyssBossEntry))
            {
                handler->PSendSysMessage("  activeRun abyssBoss name={} hpMod={} dmgMod={} phases=({}:{}, {}:{}, {}:{}) areaEffect={} lootPackage={}",
                    abyssBoss->bossName,
                    abyssBoss->healthModifier,
                    abyssBoss->damageModifier,
                    static_cast<uint32>(abyssBoss->phase1HealthPct),
                    abyssBoss->phase1SkillGroup,
                    static_cast<uint32>(abyssBoss->phase2HealthPct),
                    abyssBoss->phase2SkillGroup,
                    static_cast<uint32>(abyssBoss->phase3HealthPct),
                    abyssBoss->phase3SkillGroup,
                    abyssBoss->areaEffect,
                    abyssBoss->lootPackageId);
            }
        }

    }

    static void PrintMissingForAllChapters(ChatHandler* handler)
    {
        uint32 totalQuestMissing = 0;
        uint32 totalCreatureMissing = 0;
        uint32 totalItemMissing = 0;
        uint32 chapterCountWithMissing = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 questMissing = 0;
            uint32 creatureMissing = 0;
            uint32 itemMissing = 0;

            CountMissingForChapter(*chapter, questMissing, creatureMissing, itemMissing);
            if (questMissing == 0 && creatureMissing == 0 && itemMissing == 0)
                continue;

            ++chapterCountWithMissing;
            totalQuestMissing += questMissing;
            totalCreatureMissing += creatureMissing;
            totalItemMissing += itemMissing;

            handler->PSendSysMessage("Missing chapter {} ({}) quest={} creature={} item={}",
                chapter->chapterId,
                chapter->chapterName,
                questMissing,
                creatureMissing,
                itemMissing);
        }

        handler->PSendSysMessage("Missing summary: chapters={} quest={} creature={} item={}",
            chapterCountWithMissing,
            totalQuestMissing,
            totalCreatureMissing,
            totalItemMissing);
    }

    static void PrintMissingForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Missing report for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anyMissing = false;

        if (chapter.startQuestId != 0 && !sObjectMgr->GetQuestTemplate(chapter.startQuestId))
        {
            handler->PSendSysMessage("  missing quest_template startQuestId={}", chapter.startQuestId);
            anyMissing = true;
        }

        if (chapter.completeQuestId != 0 && !sObjectMgr->GetQuestTemplate(chapter.completeQuestId))
        {
            handler->PSendSysMessage("  missing quest_template completeQuestId={}", chapter.completeQuestId);
            anyMissing = true;
        }

        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
        {
            if (!sObjectMgr->GetQuestTemplate(docking->questId))
            {
                handler->PSendSysMessage("  missing quest_template docking type={} questId={} title={}",
                    GetTaskTypeName(docking->taskType),
                    docking->questId,
                    docking->title);
                anyMissing = true;
            }
        }

        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
        {
            if (!sObjectMgr->GetCreatureTemplate(docking->bossEntry))
            {
                handler->PSendSysMessage("  missing creature_template type={} entry={} name={} script={}",
                    GetBossTypeName(docking->bossType),
                    docking->bossEntry,
                    docking->bossName,
                    docking->suggestedScriptName);
                anyMissing = true;
            }
        }

        if (chapter.relicItemId != 0 && !sObjectMgr->GetItemTemplate(chapter.relicItemId))
        {
            handler->PSendSysMessage("  missing item_template relicItemId={}", chapter.relicItemId);
            anyMissing = true;
        }

        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
        {
            if (!sObjectMgr->GetItemTemplate(docking->itemId))
            {
                handler->PSendSysMessage("  missing item_template type={} itemId={} name={}",
                    GetItemDockingTypeName(docking->dockingType),
                    docking->itemId,
                    docking->itemName);
                anyMissing = true;
            }
        }

        if (!anyMissing)
            handler->SendSysMessage("  no missing template records for this chapter.");
    }

    static void CountMissingForChapter(AbyssChapterConfig const& chapter, uint32& questMissing, uint32& creatureMissing, uint32& itemMissing)
    {
        if (chapter.startQuestId != 0 && !sObjectMgr->GetQuestTemplate(chapter.startQuestId))
            ++questMissing;

        if (chapter.completeQuestId != 0 && !sObjectMgr->GetQuestTemplate(chapter.completeQuestId))
            ++questMissing;

        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
            if (!sObjectMgr->GetQuestTemplate(docking->questId))
                ++questMissing;

        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
            if (!sObjectMgr->GetCreatureTemplate(docking->bossEntry))
                ++creatureMissing;

        if (chapter.relicItemId != 0 && !sObjectMgr->GetItemTemplate(chapter.relicItemId))
            ++itemMissing;

        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
            if (!sObjectMgr->GetItemTemplate(docking->itemId))
                ++itemMissing;
    }

    static void PrintAuditForAllChapters(ChatHandler* handler)
    {
        uint32 totalQuestAudit = 0;
        uint32 totalCreatureAudit = 0;
        uint32 totalItemAudit = 0;
        uint32 chapterCountWithAudit = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 questAudit = 0;
            uint32 creatureAudit = 0;
            uint32 itemAudit = 0;

            CountAuditForChapter(*chapter, questAudit, creatureAudit, itemAudit);
            if (questAudit == 0 && creatureAudit == 0 && itemAudit == 0)
                continue;

            ++chapterCountWithAudit;
            totalQuestAudit += questAudit;
            totalCreatureAudit += creatureAudit;
            totalItemAudit += itemAudit;

            handler->PSendSysMessage("Audit chapter {} ({}) quest={} creature={} item={}",
                chapter->chapterId,
                chapter->chapterName,
                questAudit,
                creatureAudit,
                itemAudit);
        }

        handler->PSendSysMessage("Audit summary: chapters={} quest={} creature={} item={}",
            chapterCountWithAudit,
            totalQuestAudit,
            totalCreatureAudit,
            totalItemAudit);
    }

    static void PrintAuditForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Audit report for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anyAudit = false;

        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
        {
            Quest const* questTemplate = sObjectMgr->GetQuestTemplate(docking->questId);
            if (!questTemplate)
                continue;

            if (questTemplate->GetQuestLevel() != docking->suggestedQuestLevel ||
                questTemplate->GetMinLevel() != docking->suggestedMinLevel ||
                questTemplate->GetZoneOrSort() != docking->suggestedQuestSort)
            {
                handler->PSendSysMessage("  audit quest_template questId={} type={} level={}/{} minLevel={}/{} sort={}/{} title={}",
                    docking->questId,
                    GetTaskTypeName(docking->taskType),
                    questTemplate->GetQuestLevel(),
                    docking->suggestedQuestLevel,
                    questTemplate->GetMinLevel(),
                    static_cast<uint32>(docking->suggestedMinLevel),
                    questTemplate->GetZoneOrSort(),
                    docking->suggestedQuestSort,
                    docking->title);
                anyAudit = true;
            }
        }

        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
        {
            CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(docking->bossEntry);
            if (!creatureTemplate)
                continue;

            std::string actualScriptName = creatureTemplate->ScriptID ? sObjectMgr->GetScriptName(creatureTemplate->ScriptID) : std::string();
            if (creatureTemplate->faction != docking->suggestedFaction ||
                actualScriptName != docking->suggestedScriptName)
            {
                handler->PSendSysMessage("  audit creature_template entry={} type={} faction={}/{} script={}/{} name={}",
                    docking->bossEntry,
                    GetBossTypeName(docking->bossType),
                    creatureTemplate->faction,
                    docking->suggestedFaction,
                    actualScriptName.empty() ? std::string("none") : actualScriptName,
                    docking->suggestedScriptName.empty() ? std::string("none") : docking->suggestedScriptName,
                    docking->bossName);
                anyAudit = true;
            }
        }

        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(docking->itemId);
            if (!itemTemplate)
                continue;

            if (itemTemplate->Quality != docking->suggestedQuality ||
                itemTemplate->Class != docking->suggestedClass ||
                itemTemplate->SubClass != docking->suggestedSubClass ||
                itemTemplate->InventoryType != docking->suggestedInventoryType)
            {
                handler->PSendSysMessage("  audit item_template itemId={} type={} quality={}/{} class={}/{} subclass={}/{} invType={}/{} name={}",
                    docking->itemId,
                    GetItemDockingTypeName(docking->dockingType),
                    itemTemplate->Quality,
                    static_cast<uint32>(docking->suggestedQuality),
                    itemTemplate->Class,
                    static_cast<uint32>(docking->suggestedClass),
                    itemTemplate->SubClass,
                    static_cast<uint32>(docking->suggestedSubClass),
                    itemTemplate->InventoryType,
                    static_cast<uint32>(docking->suggestedInventoryType),
                    docking->itemName);
                anyAudit = true;
            }
        }

        if (!anyAudit)
            handler->SendSysMessage("  no field mismatches detected for this chapter.");
    }

    static void CountAuditForChapter(AbyssChapterConfig const& chapter, uint32& questAudit, uint32& creatureAudit, uint32& itemAudit)
    {
        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
        {
            Quest const* questTemplate = sObjectMgr->GetQuestTemplate(docking->questId);
            if (!questTemplate)
                continue;

            if (questTemplate->GetQuestLevel() != docking->suggestedQuestLevel ||
                questTemplate->GetMinLevel() != docking->suggestedMinLevel ||
                questTemplate->GetZoneOrSort() != docking->suggestedQuestSort)
            {
                ++questAudit;
            }
        }

        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
        {
            CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(docking->bossEntry);
            if (!creatureTemplate)
                continue;

            std::string actualScriptName = creatureTemplate->ScriptID ? sObjectMgr->GetScriptName(creatureTemplate->ScriptID) : std::string();
            if (creatureTemplate->faction != docking->suggestedFaction ||
                actualScriptName != docking->suggestedScriptName)
            {
                ++creatureAudit;
            }
        }

        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(docking->itemId);
            if (!itemTemplate)
                continue;

            if (itemTemplate->Quality != docking->suggestedQuality ||
                itemTemplate->Class != docking->suggestedClass ||
                itemTemplate->SubClass != docking->suggestedSubClass ||
                itemTemplate->InventoryType != docking->suggestedInventoryType)
            {
                ++itemAudit;
            }
        }
    }

    static void PrintSeedQuestForAllChapters(ChatHandler* handler)
    {
        uint32 totalQuestSeeds = 0;
        uint32 chapterCountWithSeeds = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 questSeeds = CountSeedQuestForChapter(*chapter);
            if (questSeeds == 0)
                continue;

            ++chapterCountWithSeeds;
            totalQuestSeeds += questSeeds;
            handler->PSendSysMessage("Seed quest chapter {} ({}) count={}",
                chapter->chapterId,
                chapter->chapterName,
                questSeeds);
        }

        handler->PSendSysMessage("Seed quest summary: chapters={} seeds={}",
            chapterCountWithSeeds,
            totalQuestSeeds);
    }

    static void PrintSeedQuestForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Seed quest preview for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anySeeds = false;
        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
        {
            if (sObjectMgr->GetQuestTemplate(docking->questId))
                continue;

            anySeeds = true;
            handler->PSendSysMessage("  seed questId={} type={} title={} questLevel={} minLevel={} sort={} prevQuest={} nextQuest={} chapter={} dockingStatus={}",
                docking->questId,
                GetTaskTypeName(docking->taskType),
                docking->title,
                docking->suggestedQuestLevel,
                static_cast<uint32>(docking->suggestedMinLevel),
                docking->suggestedQuestSort,
                docking->previousQuestId,
                docking->nextQuestId,
                docking->chapterId,
                static_cast<uint32>(docking->dockingStatus));
        }

        if (!anySeeds)
            handler->SendSysMessage("  no missing quest seeds for this chapter.");
    }

    static uint32 CountSeedQuestForChapter(AbyssChapterConfig const& chapter)
    {
        uint32 count = 0;
        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
            if (!sObjectMgr->GetQuestTemplate(docking->questId))
                ++count;
        return count;
    }

    static void PrintSeedQuestSqlForAllChapters(ChatHandler* handler)
    {
        uint32 totalQuestSqlSeeds = 0;
        uint32 chapterCountWithSeeds = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 questSeeds = CountSeedQuestForChapter(*chapter);
            if (questSeeds == 0)
                continue;

            ++chapterCountWithSeeds;
            totalQuestSqlSeeds += questSeeds;
            handler->PSendSysMessage("Seed quest sql chapter {} ({}) count={}",
                chapter->chapterId,
                chapter->chapterName,
                questSeeds);
        }

        handler->PSendSysMessage("Seed quest sql summary: chapters={} seeds={}",
            chapterCountWithSeeds,
            totalQuestSqlSeeds);
    }

    static void PrintSeedQuestSqlForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Seed quest sql preview for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anySeeds = false;
        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        for (AbyssTaskDocking const* docking : taskDockings)
        {
            if (sObjectMgr->GetQuestTemplate(docking->questId))
                continue;

            anySeeds = true;

            std::string escapedTitle = EscapeSqlString(docking->title);
            std::string escapedDetails = EscapeSqlString(docking->title + " - abyss chapter entry");
            std::string escapedObjectives = EscapeSqlString(chapter.chapterName + " progression");
            std::string escapedAreaDescription = EscapeSqlString(chapter.chapterName);
            std::string escapedCompletion = EscapeSqlString(docking->title + " completed");

            std::ostringstream questSql;
            questSql << "REPLACE INTO quest_template "
                     << "(ID, QuestType, QuestLevel, MinLevel, QuestSortID, RewardNextQuest, StartItem, Flags, RewardTitle, RequiredPlayerKills, RewardTalents, RewardArenaPoints, "
                     << "LogTitle, LogDescription, QuestDescription, AreaDescription, QuestCompletionLog) VALUES ("
                     << docking->questId << ", 0, " << docking->suggestedQuestLevel << ", " << static_cast<uint32>(docking->suggestedMinLevel)
                     << ", " << docking->suggestedQuestSort << ", " << docking->nextQuestId << ", 0, 0, 0, 0, 0, 0, '"
                     << escapedTitle << "', '" << escapedDetails << "', '" << escapedObjectives << "', '"
                     << escapedAreaDescription << "', '" << escapedCompletion << "');";

            std::ostringstream addonSql;
            addonSql << "REPLACE INTO quest_template_addon "
                     << "(ID, MaxLevel, AllowableClasses, SourceSpellID, PrevQuestID, NextQuestID, ExclusiveGroup, RewardMailTemplateID, RewardMailDelay, "
                     << "RequiredSkillID, RequiredSkillPoints, RequiredMinRepFaction, RequiredMaxRepFaction, RequiredMinRepValue, RequiredMaxRepValue, ProvidedItemCount, RewardMailSenderEntry, SpecialFlags) VALUES ("
                     << docking->questId << ", 0, 0, 0, " << docking->previousQuestId << ", " << docking->nextQuestId
                     << ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);";

            handler->SendSysMessage(questSql.str().c_str());
            handler->SendSysMessage(addonSql.str().c_str());
        }

        if (!anySeeds)
            handler->SendSysMessage("  no missing quest sql seeds for this chapter.");
    }

    static void PrintSeedItemForAllChapters(ChatHandler* handler)
    {
        uint32 totalItemSeeds = 0;
        uint32 chapterCountWithSeeds = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 itemSeeds = CountSeedItemForChapter(*chapter);
            if (itemSeeds == 0)
                continue;

            ++chapterCountWithSeeds;
            totalItemSeeds += itemSeeds;
            handler->PSendSysMessage("Seed item chapter {} ({}) count={}",
                chapter->chapterId,
                chapter->chapterName,
                itemSeeds);
        }

        handler->PSendSysMessage("Seed item summary: chapters={} seeds={}",
            chapterCountWithSeeds,
            totalItemSeeds);
    }

    static void PrintSeedItemForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Seed item preview for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anySeeds = false;
        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
        {
            if (sObjectMgr->GetItemTemplate(docking->itemId))
                continue;

            anySeeds = true;
            handler->PSendSysMessage("  seed itemId={} type={} name={} quality={} class={} subclass={} invType={} unique={} bagActivated={} chapter={} dockingStatus={}",
                docking->itemId,
                GetItemDockingTypeName(docking->dockingType),
                docking->itemName,
                static_cast<uint32>(docking->suggestedQuality),
                static_cast<uint32>(docking->suggestedClass),
                static_cast<uint32>(docking->suggestedSubClass),
                static_cast<uint32>(docking->suggestedInventoryType),
                docking->uniqueItem,
                docking->bagActivated,
                docking->chapterId,
                static_cast<uint32>(docking->dockingStatus));
        }

        if (!anySeeds)
            handler->SendSysMessage("  no missing item seeds for this chapter.");
    }

    static uint32 CountSeedItemForChapter(AbyssChapterConfig const& chapter)
    {
        uint32 count = 0;
        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
            if (!sObjectMgr->GetItemTemplate(docking->itemId))
                ++count;
        return count;
    }

    static void PrintSeedItemSqlForAllChapters(ChatHandler* handler)
    {
        uint32 totalItemSqlSeeds = 0;
        uint32 chapterCountWithSeeds = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 itemSeeds = CountSeedItemForChapter(*chapter);
            if (itemSeeds == 0)
                continue;

            ++chapterCountWithSeeds;
            totalItemSqlSeeds += itemSeeds;
            handler->PSendSysMessage("Seed item sql chapter {} ({}) count={}",
                chapter->chapterId,
                chapter->chapterName,
                itemSeeds);
        }

        handler->PSendSysMessage("Seed item sql summary: chapters={} seeds={}",
            chapterCountWithSeeds,
            totalItemSqlSeeds);
    }

    static void PrintSeedItemSqlForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Seed item sql preview for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anySeeds = false;
        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        for (AbyssItemDocking const* docking : itemDockings)
        {
            if (sObjectMgr->GetItemTemplate(docking->itemId))
                continue;

            anySeeds = true;

            std::string escapedName = EscapeSqlString(docking->itemName);
            uint32 requiredLevel = chapter.requiredCultivationLevel == 0 ? 1u : std::min<uint32>(chapter.requiredCultivationLevel, 80u);

            std::ostringstream itemSql;
            itemSql << "REPLACE INTO item_template "
                    << "(entry, class, subclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, AllowableClass, AllowableRace, ItemLevel, RequiredLevel, maxcount, stackable, ScriptName, flagsCustom) VALUES ("
                    << docking->itemId << ", " << static_cast<uint32>(docking->suggestedClass) << ", " << static_cast<uint32>(docking->suggestedSubClass)
                    << ", '" << escapedName << "', " << docking->reservedDisplayId << ", " << static_cast<uint32>(docking->suggestedQuality)
                    << ", 0, 0, 1, 0, 0, " << static_cast<uint32>(docking->suggestedInventoryType)
                    << ", -1, -1, " << requiredLevel << ", " << requiredLevel << ", "
                    << (docking->uniqueItem ? 1 : 0) << ", 1, '', 0);";

            handler->SendSysMessage(itemSql.str().c_str());
        }

        if (!anySeeds)
            handler->SendSysMessage("  no missing item sql seeds for this chapter.");
    }

    static void PrintSeedBossForAllChapters(ChatHandler* handler)
    {
        uint32 totalBossSeeds = 0;
        uint32 chapterCountWithSeeds = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 bossSeeds = CountSeedBossForChapter(*chapter);
            if (bossSeeds == 0)
                continue;

            ++chapterCountWithSeeds;
            totalBossSeeds += bossSeeds;
            handler->PSendSysMessage("Seed boss chapter {} ({}) count={}",
                chapter->chapterId,
                chapter->chapterName,
                bossSeeds);
        }

        handler->PSendSysMessage("Seed boss summary: chapters={} seeds={}",
            chapterCountWithSeeds,
            totalBossSeeds);
    }

    static void PrintSeedBossForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Seed boss preview for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anySeeds = false;
        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
        {
            if (sObjectMgr->GetCreatureTemplate(docking->bossEntry))
                continue;

            anySeeds = true;
            handler->PSendSysMessage("  seed entry={} type={} name={} level={} faction={} script={} writeCreature={} writeLoot={} chapter={} dockingStatus={}",
                docking->bossEntry,
                GetBossTypeName(docking->bossType),
                docking->bossName,
                static_cast<uint32>(docking->suggestedLevel),
                docking->suggestedFaction,
                docking->suggestedScriptName.empty() ? std::string("none") : docking->suggestedScriptName,
                docking->shouldWriteCreatureTemplate,
                docking->shouldWriteLootTemplate,
                docking->chapterId,
                static_cast<uint32>(docking->dockingStatus));
        }

        if (!anySeeds)
            handler->SendSysMessage("  no missing boss seeds for this chapter.");
    }

    static uint32 CountSeedBossForChapter(AbyssChapterConfig const& chapter)
    {
        uint32 count = 0;
        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
            if (!sObjectMgr->GetCreatureTemplate(docking->bossEntry))
                ++count;
        return count;
    }

    static void PrintSeedBossSqlForAllChapters(ChatHandler* handler)
    {
        uint32 totalBossSqlSeeds = 0;
        uint32 chapterCountWithSeeds = 0;

        std::vector<AbyssChapterConfig const*> chapters;
        for (auto const& pair : sAbyssCultivationMgr->GetAllChapterConfigs())
            chapters.push_back(&pair.second);

        std::sort(chapters.begin(), chapters.end(), [](AbyssChapterConfig const* left, AbyssChapterConfig const* right)
        {
            return left->chapterId < right->chapterId;
        });

        for (AbyssChapterConfig const* chapter : chapters)
        {
            uint32 bossSeeds = CountSeedBossForChapter(*chapter);
            if (bossSeeds == 0)
                continue;

            ++chapterCountWithSeeds;
            totalBossSqlSeeds += bossSeeds;
            handler->PSendSysMessage("Seed boss sql chapter {} ({}) count={}",
                chapter->chapterId,
                chapter->chapterName,
                bossSeeds);
        }

        handler->PSendSysMessage("Seed boss sql summary: chapters={} seeds={}",
            chapterCountWithSeeds,
            totalBossSqlSeeds);
    }

    static void PrintSeedBossSqlForChapter(ChatHandler* handler, AbyssChapterConfig const& chapter, char const* label)
    {
        handler->PSendSysMessage("Seed boss sql preview for {} chapter {} ({}):",
            label,
            chapter.chapterId,
            chapter.chapterName);

        bool anySeeds = false;
        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        for (AbyssBossDocking const* docking : bossDockings)
        {
            if (sObjectMgr->GetCreatureTemplate(docking->bossEntry))
                continue;

            anySeeds = true;

            std::string escapedName = EscapeSqlString(docking->bossName);
            std::string escapedScript = EscapeSqlString(docking->suggestedScriptName);

            std::ostringstream bossSql;
            bossSql << "REPLACE INTO creature_template "
                    << "(entry, name, subname, IconName, minlevel, maxlevel, exp, faction, npcflag, speed_walk, speed_run, scale, `rank`, unit_class, type, type_flags, lootid, pickpocketloot, skinloot, AIName, MovementType, HoverHeight, HealthModifier, ManaModifier, ArmorModifier, ExperienceModifier, RacialLeader, movementId, RegenHealth, mechanic_immune_mask, flags_extra, ScriptName) VALUES ("
                    << docking->bossEntry << ", '" << escapedName << "', '', '', "
                    << static_cast<uint32>(docking->suggestedLevel) << ", " << static_cast<uint32>(docking->suggestedLevel)
                    << ", 2, " << docking->suggestedFaction << ", 0, 1, 1.14286, 1, 3, 1, 7, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, '"
                    << escapedScript << "');";

            handler->SendSysMessage(bossSql.str().c_str());
        }

        if (!anySeeds)
            handler->SendSysMessage("  no missing boss sql seeds for this chapter.");
    }

    static void PrintChapterDetails(ChatHandler* handler, Player* target, PlayerAbyssData const& playerData,
        AbyssChapterConfig const& chapter, char const* label)
    {
        AbyssChapterAccessState accessState = sAbyssCultivationMgr->EvaluateChapterAccess(target, playerData, chapter);
        bool startQuestExists = chapter.startQuestId != 0 && sObjectMgr->GetQuestTemplate(chapter.startQuestId) != nullptr;
        bool completeQuestExists = chapter.completeQuestId != 0 && sObjectMgr->GetQuestTemplate(chapter.completeQuestId) != nullptr;
        QuestStatus startQuestStatus = chapter.startQuestId != 0 ? target->GetQuestStatus(chapter.startQuestId) : QUEST_STATUS_NONE;
        QuestStatus completeQuestStatus = chapter.completeQuestId != 0 ? target->GetQuestStatus(chapter.completeQuestId) : QUEST_STATUS_NONE;
        bool startQuestRewarded = chapter.startQuestId != 0 && target->GetQuestRewardStatus(chapter.startQuestId);
        bool completeQuestRewarded = chapter.completeQuestId != 0 && target->GetQuestRewardStatus(chapter.completeQuestId);
        bool relicTemplateExists = chapter.relicItemId != 0 && sObjectMgr->GetItemTemplate(chapter.relicItemId) != nullptr;

        handler->PSendSysMessage("  {}Chapter id={} name={} act={} map={} type={} reqThreshold={} prerequisite={} prerequisiteMet={} levelMet={} readyToEnter={} accessReason={}",
            label,
            chapter.chapterId,
            chapter.chapterName,
            static_cast<uint32>(chapter.actId),
            chapter.mapId,
            static_cast<uint32>(chapter.chapterType),
            chapter.requiredCultivationLevel,
            chapter.prerequisiteChapterId,
            accessState.prerequisiteMet,
            accessState.levelMet,
            accessState.readyToEnter,
            accessState.reason);
        handler->PSendSysMessage("    quests start={} exists={} status={} rewarded={} complete={} exists={} status={} rewarded={}",
            chapter.startQuestId,
            startQuestExists,
            GetQuestStatusName(startQuestStatus),
            startQuestRewarded,
            chapter.completeQuestId,
            completeQuestExists,
            GetQuestStatusName(completeQuestStatus),
            completeQuestRewarded);
        handler->PSendSysMessage("    bosses anchor={} final={} abyss={} cache={} relicItem={} relicTemplate={}",
            chapter.anchorBossEntry,
            chapter.finalBossEntry,
            chapter.abyssBossEntry,
            chapter.cacheBossEntry,
            chapter.relicItemId,
            relicTemplateExists);
        handler->PSendSysMessage("    runtime lootGroup={} preferredSlotMask={} triggerType={} triggerMask={} abyssSummonMap={} abyssSummonPos=({}, {}, {}, {}) cacheChance={} cachePity={} quickFarm={} pity={} dropRate=({}/{}/{}/{})",
            chapter.lootGroupId,
            chapter.preferredSlotMask,
            static_cast<uint32>(chapter.triggerType),
            chapter.requiredBossKillMask,
            chapter.abyssSummonMapId,
            chapter.abyssSummonX,
            chapter.abyssSummonY,
            chapter.abyssSummonZ,
            chapter.abyssSummonO,
            chapter.cacheBossBaseChance,
            chapter.cacheBossPityCount,
            chapter.quickFarmUnlocked,
            chapter.abyssGearPityCount,
            chapter.storyDropRate,
            chapter.abyssDropRate,
            chapter.corruptionDropRate,
            chapter.reincarnationDropRate);

        std::vector<AbyssTaskDocking const*> taskDockings = sAbyssCultivationMgr->GetTaskDockingsForChapter(chapter.chapterId);
        if (taskDockings.empty())
        {
            handler->SendSysMessage("    taskDockings: none");
        }
        else
        {
            for (AbyssTaskDocking const* docking : taskDockings)
            {
                bool questTemplateExists = sObjectMgr->GetQuestTemplate(docking->questId) != nullptr;
                QuestStatus questStatus = target->GetQuestStatus(docking->questId);
                bool rewarded = target->GetQuestRewardStatus(docking->questId);

                handler->PSendSysMessage("    taskDocking type={} questId={} title={} questTemplate={} status={} rewarded={} prev={} next={} dockingStatus={}",
                    GetTaskTypeName(docking->taskType),
                    docking->questId,
                    docking->title,
                    questTemplateExists,
                    GetQuestStatusName(questStatus),
                    rewarded,
                    docking->previousQuestId,
                    docking->nextQuestId,
                    static_cast<uint32>(docking->dockingStatus));
            }
        }

        std::vector<AbyssBossDocking const*> bossDockings = sAbyssCultivationMgr->GetBossDockingsForChapter(chapter.chapterId);
        if (bossDockings.empty())
        {
            handler->SendSysMessage("    bossDockings: none");
        }
        else
        {
            for (AbyssBossDocking const* docking : bossDockings)
            {
                bool creatureTemplateExists = sObjectMgr->GetCreatureTemplate(docking->bossEntry) != nullptr;
                handler->PSendSysMessage("    bossDocking type={} entry={} name={} creatureTemplate={} script={} writeCreature={} writeLoot={} dockingStatus={}",
                    GetBossTypeName(docking->bossType),
                    docking->bossEntry,
                    docking->bossName,
                    creatureTemplateExists,
                    docking->suggestedScriptName,
                    docking->shouldWriteCreatureTemplate,
                    docking->shouldWriteLootTemplate,
                    static_cast<uint32>(docking->dockingStatus));
            }
        }

        std::vector<AbyssItemDocking const*> itemDockings = sAbyssCultivationMgr->GetItemDockingsForChapter(chapter.chapterId);
        if (itemDockings.empty())
        {
            handler->SendSysMessage("    itemDockings: none");
        }
        else
        {
            for (AbyssItemDocking const* docking : itemDockings)
            {
                bool itemTemplateExists = sObjectMgr->GetItemTemplate(docking->itemId) != nullptr;
                handler->PSendSysMessage("    itemDocking type={} itemId={} name={} itemTemplate={} suggestedQ={} suggestedClass={} suggestedSubClass={} suggestedInvType={} unique={} bagActivated={} dockingStatus={}",
                    GetItemDockingTypeName(docking->dockingType),
                    docking->itemId,
                    docking->itemName,
                    itemTemplateExists,
                    static_cast<uint32>(docking->suggestedQuality),
                    static_cast<uint32>(docking->suggestedClass),
                    static_cast<uint32>(docking->suggestedSubClass),
                    static_cast<uint32>(docking->suggestedInventoryType),
                    docking->uniqueItem,
                    docking->bagActivated,
                    static_cast<uint32>(docking->dockingStatus));
            }
        }
    }

    static Player* GetSelectedPlayerOrSelf(ChatHandler* handler)
    {
        if (!handler)
            return nullptr;

        WorldSession* session = handler->GetSession();
        if (!session)
            return nullptr;

        Player* player = session->GetPlayer();
        if (!player)
            return nullptr;

        if (Player* target = player->GetSelectedPlayer())
            return target;

        return player;
    }
};
}

// ============================================
// 装备特效 Spell IDs (89001-89028)
// ============================================
enum AbyssEquipSpells
{
    SPELL_ABYSS_BLOOD_BURST          = 89001,
    SPELL_ABYSS_SOUL_SLASH           = 89002,
    SPELL_ABYSS_THUNDER_CRUSH        = 89003,
    SPELL_ABYSS_THUNDER_DEBUFF       = 89004,
    SPELL_ABYSS_POISON_DOT           = 89005,
    SPELL_ABYSS_POISON_DETONATE      = 89006,
    SPELL_ABYSS_CRIT_STORM           = 89007,
    SPELL_ABYSS_HELLFIRE_RAIN        = 89008,
    SPELL_ABYSS_HELLFIRE_IGNITE      = 89009,
    SPELL_ABYSS_BLOODLUST            = 89010,
    SPELL_ABYSS_VOID_HOLE            = 89011,
    SPELL_ABYSS_ARMOR_CRUSH          = 89012,
    SPELL_ABYSS_ARMOR_DEBUFF         = 89013,
    SPELL_ABYSS_CORPSE_EXPLODE       = 89014,
    SPELL_ABYSS_ABYSS_TOUCH          = 89015,
    SPELL_ABYSS_ABYSS_ROOT           = 89016,
    SPELL_ABYSS_WORLD_CRUSH          = 89017,
    SPELL_ABYSS_WORLD_STUN           = 89018,
    SPELL_ABYSS_BERSERK              = 89019,
    SPELL_ABYSS_LAVA_RIFT            = 89020,
    SPELL_ABYSS_LAVA_SLOW            = 89021,
    SPELL_ABYSS_DOOM_BLADE           = 89022,
    SPELL_ABYSS_DOOM_DOT             = 89023,
    SPELL_ABYSS_DESTROY_PULSE        = 89024,
    SPELL_ABYSS_DESTROY_DEBUFF       = 89025,
    SPELL_ABYSS_SOUL_REAP            = 89026,
    SPELL_ABYSS_CHARGE_DESTROY       = 89027,
    SPELL_ABYSS_CHARGE_BUFF          = 89028,
};

// ============================================
// [1] 血爆裂变 - 命中AOE火焰+连锁爆炸
// ============================================
class spell_abyss_blood_burst : public SpellScript
{
    PrepareSpellScript(spell_abyss_blood_burst);

    uint32 _hitCount = 0;
    uint32 _killCount = 0;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
        int32 damage = int32(ap * 2.8f);

        // 连锁递减：每击杀一个目标，后续目标伤害递减25%
        float reduction = 1.0f - (_killCount * 0.25f);
        if (reduction < 0.25f)
            reduction = 0.25f;
        damage = int32(damage * reduction);

        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE);
        ++_hitCount;

        if (!target->IsAlive())
            ++_killCount;
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_blood_burst::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [2] 碎魂连斩 - 5道穿透暗影斩击波，每穿透+20%
// ============================================
class spell_abyss_soul_slash : public SpellScript
{
    PrepareSpellScript(spell_abyss_soul_slash);

    uint32 _targetIndex = 0;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
        // 每多命中一个目标伤害+20%，最高翻倍
        float scale = 1.0f + (_targetIndex * 0.2f);
        if (scale > 2.0f)
            scale = 2.0f;
        int32 damage = int32(ap * 1.6f * scale);

        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW);
        ++_targetIndex;
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_soul_slash::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [3] 淬毒裂伤·DOT - AuraScript，5层满自动引爆
// ============================================
class spell_abyss_poison_dot : public AuraScript
{
    PrepareAuraScript(spell_abyss_poison_dot);

    void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Aura* aura = GetAura())
        {
            if (aura->GetStackAmount() >= 5)
            {
                Unit* target = GetTarget();
                Unit* caster = GetCaster();
                if (caster && target)
                {
                    // 满5层引爆
                    caster->CastSpell(target, SPELL_ABYSS_POISON_DETONATE, true);
                    aura->Remove();
                }
            }
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_abyss_poison_dot::AfterApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// ============================================
// [4] 淬毒裂伤·引爆 - AOE自然伤害+向周围传播毒层
// ============================================
class spell_abyss_poison_detonate : public SpellScript
{
    PrepareSpellScript(spell_abyss_poison_detonate);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float sp = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_NATURE);
        float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
        float power = std::max(sp, ap);
        int32 damage = int32(power * 2.0f);

        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NATURE);

        // 向周围传播3层毒素
        if (target != GetExplTargetUnit())
            return; // 只在主目标上执行传播逻辑一次

        // 由于DBC已设定AOE目标，每个被命中的目标自动获得毒层传播
        caster->CastSpell(target, SPELL_ABYSS_POISON_DOT, true);
        caster->CastSpell(target, SPELL_ABYSS_POISON_DOT, true);
        caster->CastSpell(target, SPELL_ABYSS_POISON_DOT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_poison_detonate::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [5] 地狱火雨 - AOE火焰伤害+施加点燃DOT
// ============================================
class spell_abyss_hellfire_rain : public SpellScript
{
    PrepareSpellScript(spell_abyss_hellfire_rain);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float sp = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
        int32 damage = int32(sp * 1.2f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE);

        // 施加点燃DOT（可叠加3层）
        caster->CastSpell(target, SPELL_ABYSS_HELLFIRE_IGNITE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_hellfire_rain::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [6] 虚空黑洞 - 暗影AOE+减速60%
// ============================================
class spell_abyss_void_hole : public SpellScript
{
    PrepareSpellScript(spell_abyss_void_hole);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float sp = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW);
        int32 damage = int32(sp * 1.8f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW);

        // 吸引效果：将目标拉向施法者（模拟黑洞吸引）
        float dist = caster->GetDistance(target);
        if (dist > 3.0f)
        {
            float angle = caster->GetAngle(target);
            float pullDist = std::min(dist - 2.0f, 5.0f);
            float newX = target->GetPositionX() + pullDist * cos(angle + M_PI);
            float newY = target->GetPositionY() + pullDist * sin(angle + M_PI);
            float newZ = target->GetPositionZ();
            target->NearTeleportTo(newX, newY, newZ, target->GetOrientation());
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_void_hole::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [7] 尸爆连锁 - 击杀后尸体爆炸，连锁最多5次
// ============================================
class spell_abyss_corpse_explode : public SpellScript
{
    PrepareSpellScript(spell_abyss_corpse_explode);

    uint32 _chainCount = 0;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        // 伤害=目标最大生命值的25%，每次连锁递减15%
        float reduction = 1.0f - (_chainCount * 0.15f);
        if (reduction < 0.25f)
            reduction = 0.25f;
        int32 damage = int32(target->GetMaxHealth() * 0.25f * reduction);

        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL);

        // 如果目标死亡且还有连锁次数，再次触发爆炸
        if (!target->IsAlive() && _chainCount < 5)
        {
            ++_chainCount;
            caster->CastSpell(caster, SPELL_ABYSS_CORPSE_EXPLODE, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_corpse_explode::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [8] 深渊之触 - 暗影AOE+定身3秒
// ============================================
class spell_abyss_abyss_touch : public SpellScript
{
    PrepareSpellScript(spell_abyss_abyss_touch);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
        int32 damage = int32(ap * 3.0f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW);

        // 施加定身
        caster->CastSpell(target, SPELL_ABYSS_ABYSS_ROOT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_abyss_touch::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [9] 岩浆裂地 - 火焰伤害+减速
// ============================================
class spell_abyss_lava_rift : public SpellScript
{
    PrepareSpellScript(spell_abyss_lava_rift);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float sp = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
        int32 damage = int32(sp * 1.5f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE);

        // 施加减速
        caster->CastSpell(target, SPELL_ABYSS_LAVA_SLOW, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_lava_rift::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [10] 毁灭脉冲 - 奥术AOE+击退+施加易伤debuff
// ============================================
class spell_abyss_destroy_pulse : public SpellScript
{
    PrepareSpellScript(spell_abyss_destroy_pulse);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float sp = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE);
        int32 damage = int32(sp * 4.0f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_ARCANE);

        // 击退
        float angle = caster->GetAngle(target);
        target->KnockbackFrom(caster->GetPositionX(), caster->GetPositionY(), 8.0f, 6.0f);

        // 施加易伤debuff (受伤+20%)
        caster->CastSpell(target, SPELL_ABYSS_DESTROY_DEBUFF, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_destroy_pulse::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ============================================
// [11] 噬魂收割 - 暗影AOE+每命中回复3%生命
// ============================================
class spell_abyss_soul_reap : public SpellScript
{
    PrepareSpellScript(spell_abyss_soul_reap);

    uint32 _hitCount = 0;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float sp = caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW);
        int32 damage = int32(sp * 2.6f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW);

        ++_hitCount;
    }

    void AfterCastHandler()
    {
        // 施法结束后，根据命中数回血
        Unit* caster = GetCaster();
        if (!caster || _hitCount == 0)
            return;

        int32 healAmount = int32(caster->GetMaxHealth() * 0.03f * _hitCount);
        caster->ModifyHealth(healAmount);

        if (Player* player = caster->ToPlayer())
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[噬魂收割]|r 命中{}个目标，回复{}点生命值。", _hitCount, healAmount);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_soul_reap::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
        AfterCast += SpellCastFn(spell_abyss_soul_reap::AfterCastHandler);
    }
};

// ============================================
// [12] 冲锋毁灭 - 物理AOE+击飞+自身buff
// ============================================
class spell_abyss_charge_destroy : public SpellScript
{
    PrepareSpellScript(spell_abyss_charge_destroy);

    bool _buffApplied = false;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
        int32 damage = int32(ap * 3.0f);
        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL);

        // 击飞
        target->KnockbackFrom(caster->GetPositionX(), caster->GetPositionY(), 5.0f, 4.0f);

        // 给自身施加全属性+20%的buff（只施加一次）
        if (!_buffApplied)
        {
            caster->CastSpell(caster, SPELL_ABYSS_CHARGE_BUFF, true);
            _buffApplied = true;
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_charge_destroy::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_mod_abyss_cultivation()
{
    new AbyssCultivationWorldScript();
    new AbyssCultivationPlayerScript();
    new AbyssCultivationCreatureScript();
    new AbyssCultivationCommandScript();

    // 装备特效 SpellScript 注册
    RegisterSpellScript(spell_abyss_blood_burst);       // 89001 血爆裂变
    RegisterSpellScript(spell_abyss_soul_slash);        // 89002 碎魂连斩
    RegisterSpellScript(spell_abyss_poison_dot);        // 89005 淬毒裂伤(AuraScript)
    RegisterSpellScript(spell_abyss_poison_detonate);   // 89006 淬毒裂伤·引爆
    RegisterSpellScript(spell_abyss_hellfire_rain);     // 89008 地狱火雨
    RegisterSpellScript(spell_abyss_void_hole);         // 89011 虚空黑洞
    RegisterSpellScript(spell_abyss_corpse_explode);    // 89014 尸爆连锁
    RegisterSpellScript(spell_abyss_abyss_touch);       // 89015 深渊之触
    RegisterSpellScript(spell_abyss_lava_rift);         // 89020 岩浆裂地
    RegisterSpellScript(spell_abyss_destroy_pulse);     // 89024 毁灭脉冲
    RegisterSpellScript(spell_abyss_soul_reap);         // 89026 噬魂收割
    RegisterSpellScript(spell_abyss_charge_destroy);    // 89027 冲锋毁灭
}

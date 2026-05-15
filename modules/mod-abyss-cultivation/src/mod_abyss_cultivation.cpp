#include "ScriptMgr.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Configuration/Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
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
#include <list>
#include <limits>
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

bool IsAbyssCustomBossEntry(uint32 entry)
{
    return (entry >= 910001 && entry <= 910074) || (entry >= 919001 && entry <= 919074);
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

char const* GetModeBossPrefix(uint8 modeType)
{
    switch (modeType)
    {
        case 1: return "正传·";
        case 2: return "深渊·";
        case 3: return "腐化·";
        case 4: return "轮回·";
        default: return "";
    }
}

uint32 GetModeTypeMask(uint8 modeType)
{
    if (modeType == 0 || modeType > 31)
        return 0;

    return 1u << (modeType - 1);
}

constexpr uint64 kTrackedBossMaskOfficialAnchor = 1ULL << 0;
constexpr uint64 kTrackedBossMaskOfficialFinal = 1ULL << 1;
constexpr uint64 kTrackedBossMaskStoryBoss = 1ULL << 4;
constexpr uint64 kTrackedBossMaskAbyssBoss = 1ULL << 5;
constexpr uint64 kTrackedBossMaskCorruptionBoss = 1ULL << 6;
constexpr uint64 kTrackedBossMaskReincarnationBoss = 1ULL << 7;
constexpr uint64 kTrackedBossMaskCacheBoss = 1ULL << 8;

std::vector<std::string> SplitAddonCommandFields(std::string const& text, char delimiter)
{
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, delimiter))
        fields.push_back(token);

    if (!text.empty() && text.back() == delimiter)
        fields.emplace_back();

    return fields;
}
uint8 ParseAddonRewardMode(std::string const& text, uint8 defaultMode = 1)
{
    if (text.empty())
        return defaultMode;

    uint8 modeType = static_cast<uint8>(std::strtoul(text.c_str(), nullptr, 10));
    if (modeType < 1 || modeType > 4)
        return defaultMode;

    return modeType;
}

bool ParseAddonRewardChapterRequest(std::string const& text, uint16& chapterId, uint8& modeType)
{
    std::vector<std::string> fields = SplitAddonCommandFields(text, '|');
    if (!fields.empty() && !fields[0].empty())
        chapterId = static_cast<uint16>(std::strtoul(fields[0].c_str(), nullptr, 10));
    if (fields.size() > 1 && !fields[1].empty())
        modeType = ParseAddonRewardMode(fields[1], modeType);

    return chapterId != 0;
}

bool StringStartsWith(std::string const& value, std::string const& prefix)
{
    if (prefix.size() > value.size())
        return false;

    return value.compare(0, prefix.size(), prefix) == 0;
}

bool StringEndsWith(std::string const& value, std::string const& suffix)
{
    if (suffix.size() > value.size())
        return false;

    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

constexpr uint32 ABYSS_NATIVE_ITEMSET_ID_BASE = 1001;
constexpr uint32 ABYSS_NATIVE_SET_SPELL_BASE = 89201;
constexpr uint32 ABYSS_NATIVE_SET_SPELL_BLOCK_SIZE = 7;
constexpr uint8 ABYSS_STANDARD_SET_SOURCE_MODE_COUNT = 4;
constexpr uint8 ABYSS_ARTIFACT_SET_SOURCE_MODE = 5;
constexpr uint32 ABYSS_NATIVE_SET_CHAPTER_COUNT = 74;
constexpr uint32 ABYSS_NATIVE_SET_ACT_COUNT = 6;
constexpr uint32 ABYSS_NATIVE_ARTIFACT_ITEMSET_ID_BASE =
    ABYSS_NATIVE_ITEMSET_ID_BASE + ABYSS_NATIVE_SET_CHAPTER_COUNT * ABYSS_STANDARD_SET_SOURCE_MODE_COUNT;
constexpr uint32 ABYSS_NATIVE_ARTIFACT_SET_ORDINAL_BASE =
    ABYSS_NATIVE_SET_ACT_COUNT * ABYSS_STANDARD_SET_SOURCE_MODE_COUNT;

struct AbyssNativeSetSpellIds
{
    uint32 twoPieceMain = 0;
    uint32 fourPieceVisible = 0;
    uint32 fourPieceHelper = 0;
    uint32 sixPieceVisible = 0;
    uint32 sixPieceHelper = 0;
    uint32 eightPieceVisible = 0;
    uint32 eightPieceHelper = 0;
};

uint32 GetAbyssNativeItemSetId(uint16 sourceChapter, uint8 sourceMode)
{
    if (sourceChapter == 0 || sourceMode < 1 || sourceMode > ABYSS_ARTIFACT_SET_SOURCE_MODE)
        return 0;

    if (sourceMode == ABYSS_ARTIFACT_SET_SOURCE_MODE)
        return ABYSS_NATIVE_ARTIFACT_ITEMSET_ID_BASE + uint32(sourceChapter - 1);

    return ABYSS_NATIVE_ITEMSET_ID_BASE
        + (uint32(sourceChapter - 1) * ABYSS_STANDARD_SET_SOURCE_MODE_COUNT)
        + uint32(sourceMode - 1);
}

AbyssNativeSetSpellIds GetAbyssNativeSetSpellIds(uint8 actId, uint8 sourceMode)
{
    if (actId < 1 || actId > ABYSS_NATIVE_SET_ACT_COUNT || sourceMode < 1 || sourceMode > ABYSS_ARTIFACT_SET_SOURCE_MODE)
        return {};

    uint32 ordinal = 0;
    if (sourceMode == ABYSS_ARTIFACT_SET_SOURCE_MODE)
        ordinal = ABYSS_NATIVE_ARTIFACT_SET_ORDINAL_BASE + uint32(actId - 1u);
    else
        ordinal = (uint32(actId) - 1u) * ABYSS_STANDARD_SET_SOURCE_MODE_COUNT + uint32(sourceMode - 1u);

    uint32 baseSpellId = ABYSS_NATIVE_SET_SPELL_BASE + ordinal * ABYSS_NATIVE_SET_SPELL_BLOCK_SIZE;

    return {
        baseSpellId,
        baseSpellId + 1u,
        baseSpellId + 2u,
        baseSpellId + 3u,
        baseSpellId + 4u,
        baseSpellId + 5u,
        baseSpellId + 6u
    };
}

uint16 GetAbyssLootModeMask(uint8 modeType)
{
    switch (modeType)
    {
        case 1: return 0x02;
        case 2: return 0x04;
        case 3: return 0x08;
        case 4: return 0x10;
        default: return 0;
    }
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

int64 ScaleUInt64ToInt64(uint64 value, long double scale)
{
    if (value == 0 || scale <= 0.0L)
        return 0;

    long double scaledValue = static_cast<long double>(value) * scale;
    if (scaledValue >= static_cast<long double>(std::numeric_limits<int64>::max()))
        return std::numeric_limits<int64>::max();

    return std::max<int64>(1, static_cast<int64>(std::llround(scaledValue)));
}

uint64 ScaleUInt64ToUInt64(uint64 value, long double scale)
{
    if (value == 0 || scale <= 0.0L)
        return 0;

    long double scaledValue = static_cast<long double>(value) * scale;
    if (scaledValue >= static_cast<long double>(std::numeric_limits<uint64>::max()))
        return std::numeric_limits<uint64>::max();

    return std::max<uint64>(1, static_cast<uint64>(std::llround(scaledValue)));
}

float ScaleUInt64ToFloat(uint64 value, long double scale)
{
    if (value == 0 || scale <= 0.0L)
        return 0.0f;

    long double scaledValue = static_cast<long double>(value) * scale;
    if (scaledValue >= static_cast<long double>(std::numeric_limits<float>::max()))
        return std::numeric_limits<float>::max();

    return static_cast<float>(scaledValue);
}

constexpr uint64 ABYSS_CLIENT_VISIBLE_HEALTH_LIMIT = 2147483520ULL;

uint32 ToAbyssClientHealth(uint64 value)
{
    return value > ABYSS_CLIENT_VISIBLE_HEALTH_LIMIT ? static_cast<uint32>(ABYSS_CLIENT_VISIBLE_HEALTH_LIMIT) : static_cast<uint32>(value);
}

int64 ScaleIntValue(int64 value, float scale)
{
    if (value == 0 || scale <= 0.0f)
        return 0;

    long double scaledValue = static_cast<long double>(value) * static_cast<long double>(scale);
    if (scaledValue >= static_cast<long double>(std::numeric_limits<int64>::max()))
        return std::numeric_limits<int64>::max();
    if (scaledValue <= static_cast<long double>(std::numeric_limits<int64>::min()))
        return std::numeric_limits<int64>::min();

    return static_cast<int64>(std::llround(scaledValue));
}

int64 AddInt64Saturated(int64 left, int64 right)
{
    if (right > 0 && left > std::numeric_limits<int64>::max() - right)
        return std::numeric_limits<int64>::max();
    if (right < 0 && left < std::numeric_limits<int64>::min() - right)
        return std::numeric_limits<int64>::min();

    return left + right;
}

int32 ToInt32Saturated(int64 value)
{
    if (value > std::numeric_limits<int32>::max())
        return std::numeric_limits<int32>::max();

    if (value < std::numeric_limits<int32>::min())
        return std::numeric_limits<int32>::min();

    return static_cast<int32>(value);
}

constexpr uint32 ABYSS_RELIC_MANAGED_SPELL_START = 89101;
constexpr uint32 ABYSS_RELIC_MANAGED_SPELL_END = 89181;
constexpr uint32 ABYSS_RELIC_THEFT_KEY_ITEM = 950035;
constexpr uint32 ABYSS_RELIC_BLESS_CURSE_ITEM = 950042;
constexpr uint32 ABYSS_RELIC_STAGE_STANCE_ITEM = 950058;
constexpr uint32 ABYSS_RELIC_PROTOCOL_EYE_ITEM = 950070;

constexpr uint32 ABYSS_RELIC_THEFT_KEY_SPELL = 89135;
constexpr uint32 ABYSS_RELIC_BLESS_CURSE_SPELL = 89142;
constexpr uint32 ABYSS_RELIC_STAGE_STANCE_SPELL = 89158;
constexpr uint32 ABYSS_RELIC_PROTOCOL_EYE_SPELL = 89170;
constexpr uint32 ABYSS_PHASE_ARTIFACT_BURNING_PACT_ITEM = 960001;
constexpr uint32 ABYSS_PHASE_ARTIFACT_VOID_EXPEDITION_ITEM = 960002;
constexpr uint32 ABYSS_PHASE_ARTIFACT_ICE_TIMEBOX_ITEM = 960003;
constexpr uint32 ABYSS_PHASE_ARTIFACT_BUG_DECREE_ITEM = 960004;
constexpr uint32 ABYSS_PHASE_ARTIFACT_ECLIPSE_KING_ITEM = 960005;
constexpr uint32 ABYSS_PHASE_ARTIFACT_SCOURGE_CHAPTER_ITEM = 960006;
constexpr uint32 ABYSS_ULTIMATE_ARTIFACT_ABYSS_LORD_ITEM = 970001;
constexpr float ABYSS_CACHE_ARTIFACT_DROP_CHANCE = 25.0f;

struct PlayerAbyssData;

    bool IsManagedRelicSpell(uint32 spellId)
    {
        return spellId >= ABYSS_RELIC_MANAGED_SPELL_START && spellId <= ABYSS_RELIC_MANAGED_SPELL_END;
    }

    bool HasAnyActiveRelicSlots(PlayerAbyssData const* data);

bool IsSystemRelicItem(uint32 itemId)
{
    switch (itemId)
    {
        case ABYSS_RELIC_THEFT_KEY_ITEM:
        case ABYSS_RELIC_BLESS_CURSE_ITEM:
        case ABYSS_RELIC_STAGE_STANCE_ITEM:
        case ABYSS_RELIC_PROTOCOL_EYE_ITEM:
            return true;
        default:
            return false;
    }
}

bool IsSystemRelicSpell(uint32 spellId)
{
    switch (spellId)
    {
        case ABYSS_RELIC_THEFT_KEY_SPELL:
        case ABYSS_RELIC_BLESS_CURSE_SPELL:
        case ABYSS_RELIC_STAGE_STANCE_SPELL:
        case ABYSS_RELIC_PROTOCOL_EYE_SPELL:
            return true;
        default:
            return false;
    }
}

bool IsSelfTargetManagedRelicSpell(uint32 spellId)
{
    switch (spellId)
    {
        case 89135: // 禁狱零匙
        case 89142: // 紫狱印典
        case 89149: // 冠军封缄
        case 89157: // 古神耳蜕
        case 89158: // 幕星假面
        case 89165: // 祖灵战鼓
        case 89170: // 观察者棱眼
        case 89175: // 焚界行契
        case 89176: // 虚空远征印
        case 89177: // 冰脉时匣
        case 89178: // 虫神遗诏
        case 89179: // 日蚀王契
        case 89180: // 天灾断章
        case 89181: // 渊主之印
            return true;
        default:
            return false;
    }
}

bool IsAbyssInternalProcSourceSpell(uint32 spellId)
{
    return (spellId >= 89001 && spellId <= 89181) ||
        (spellId >= 89401 && spellId <= 89490);
}

bool SpellHasOffensivePayload(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    if (spellInfo->DmgClass != SPELL_DAMAGE_CLASS_NONE)
        return true;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (spellInfo->Effects[i].Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_INSTAKILL:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_POWER_DRAIN:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_INTERRUPT_CAST:
            case SPELL_EFFECT_TRIGGER_SPELL:
            case SPELL_EFFECT_TRIGGER_MISSILE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_DUMMY:
            case SPELL_EFFECT_SCRIPT_EFFECT:
                return true;
            default:
                break;
        }
    }

    return false;
}

bool IsEligibleAbyssCombatProcSource(Player* player, Spell* spell)
{
    if (!player || !spell)
        return false;

    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return false;

    if (spellInfo->IsPassive() || spellInfo->IsPositive())
        return false;

    if (IsAbyssInternalProcSourceSpell(spellInfo->Id))
        return false;

    if (Unit* unitTarget = spell->m_targets.GetUnitTarget())
        return unitTarget != player && unitTarget->IsAlive() && player->IsValidAttackTarget(unitTarget);

    if (Unit* victim = player->GetVictim())
        return victim->IsAlive() && player->IsValidAttackTarget(victim) && SpellHasOffensivePayload(spellInfo);

    return false;
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
    "SELECT `首领入口`, `首领名称`, `首领类型`, `幕ID`, "
    "CASE "
    "WHEN `章节ID` <> 0 THEN `章节ID` "
    "WHEN `首领类型` = 3 AND `幕ID` = 1 THEN 1 "
    "WHEN `首领类型` = 3 AND `幕ID` = 2 THEN 21 "
    "WHEN `首领类型` = 3 AND `幕ID` = 4 THEN 53 "
    "WHEN `首领类型` = 3 AND `幕ID` = 5 THEN 58 "
    "WHEN `首领类型` = 3 AND `幕ID` = 6 THEN 67 "
    "ELSE `章节ID` END AS `章节ID`, "
    "CASE WHEN `幕ID` <= 1 THEN 35 WHEN `幕ID` = 2 THEN 70 WHEN `幕ID` = 3 THEN 80 WHEN `幕ID` = 4 THEN 83 WHEN `幕ID` = 5 THEN 83 ELSE 83 END AS `建议等级`, "
    "14 AS `建议阵营`, 0 AS `建议模型组`, '' AS `建议脚本名`, "
    "1 AS `是否写入生物模板`, 1 AS `是否写入掉落模板`, 1 AS `对接状态`, '' AS `说明` "
    "FROM `_深渊首领配置` "
    "WHERE `是否启用` = 1 AND `首领类型` IN (2, 3) "
    "ORDER BY `章节ID`, `首领类型`, `首领入口`";

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
    // 6件效果
    uint8  sixPieceDmgPct = 0;
    uint8  sixPieceHpPct = 0;
    uint16 sixPieceCrit = 0;
    uint16 sixPieceHaste = 0;
    std::string sixPieceSpecialEffect;
    std::string sixPieceDesc;
    // 8件效果
    uint8  eightPieceDmgPct = 0;
    uint8  eightPieceHpPct = 0;
    uint16 eightPieceCrit = 0;
    uint16 eightPieceHaste = 0;
    std::string eightPieceSpecialEffect;
    std::string eightPieceDesc;
    bool enabled = true;
};

// 玩家当前活跃套装buff状态
struct PlayerSetBonusState
{
    uint32 setId = 0;
    uint8 pieceCount = 0;
    bool hasTwoPieceBonus = false;
    bool hasFourPieceBonus = false;
    bool hasSixPieceBonus = false;
    bool hasEightPieceBonus = false;
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
    uint32 currentInstanceId = 0;
    uint8 pendingAbyssModeType = 0;
    bool pendingCacheSummon = false;
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
    uint32 sporeKillCounter = 0;
    uint32 dragonRageKillCounter = 0;
    uint32 mushroomKillCounter = 0;
    uint32 soulLampKillCounter = 0;
    uint32 soulFurnaceKillCounter = 0;
    uint32 stitchKillCounter = 0;
    uint32 poisonCastCounter = 0;
    uint32 bloodFlameComboCounter = 0;
    uint32 sunBurstComboCounter = 0;
    uint32 starfallCounter = 0;
    uint32 tideCastCounter = 0;
    uint32 bloomCastCounter = 0;
    uint32 arcaneAshCastCounter = 0;
    uint32 vineCastCounter = 0;
    uint32 orbitalCastCounter = 0;
    uint32 overloadCastCounter = 0;
    uint32 dragonBreathCastCounter = 0;
    uint32 laserOrbitCastCounter = 0;
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
    uint32 lastSporeBurstTime = 0;
    uint32 lastDragonRageTime = 0;
    uint32 lastMushroomBurstTime = 0;
    uint32 lastSoulLampTime = 0;
    uint32 lastSoulFurnaceTime = 0;
    uint32 lastStitchTime = 0;
    uint32 lastArcaneAshTime = 0;
    uint32 lastRiftRainTime = 0;
    uint32 lastPhaseShotTime = 0;
    uint32 lastVineBloomTime = 0;
    uint32 lastSunBurstTime = 0;
    uint32 lastOrbitalMissileTime = 0;
    uint32 lastNoFaceEchoTime = 0;
    uint32 lastTitanMeteorTime = 0;
    uint32 lastThunderAncestorTime = 0;
    uint32 lastArmyPressureTime = 0;
    uint32 lastFearExecuteTime = 0;
    uint32 lastOverloadTime = 0;
    uint32 lastLavaCoreTime = 0;
    uint32 lastDragonBreathTime = 0;
    uint32 lastLaserOrbitTime = 0;
    uint32 lastRekindleTime = 0;
    uint32 lastStillnessTime = 0;
    uint32 lastMountainBreakTime = 0;
    uint32 lastTidePrisonTime = 0;
    uint32 lastHellPrisonTime = 0;
    uint32 lastSandEchoTime = 0;
    uint32 lastPlagueSpreadTime = 0;
    uint32 lastAncestorBlessTime = 0;
    uint32 lastFrostExplodeTime = 0;
    uint32 lastCorruptBloomTime = 0;
    uint32 lastSteamCoreTime = 0;
    uint32 lastChampionSealTime = 0;
    uint32 lastOldGodGiftTime = 0;
    uint32 lastCombatEnterTime = 0;
    uint32 openingAttackCount = 0;
    uint32 artifactMainProcCounter = 0;
    uint32 lastPhaseArtifactTime = 0;
    uint32 lastUltimateArtifactTime = 0;
    uint32 lastWolfMoonTime = 0;
    uint32 lastThornChargeTime = 0;
    uint32 lastFireSchoolCastTime = 0;
    uint32 lastBlackFurnaceTime = 0;
    uint32 lastGeneralEchoTime = 0;
    uint32 lastBulwarkTime = 0;
    uint32 lastBloodOrbTime = 0;
    uint32 lastClockMarkTime = 0;
    uint32 lastSpiderEggTime = 0;
    uint32 lastExecutionStakeTime = 0;
    uint32 lastMirrorShardTime = 0;
    uint32 lastReverseScaleTime = 0;
    uint32 lastEclipseTime = 0;
    uint32 lastPrisonChoiceTime = 0;
    uint32 lastMatrixEchoTime = 0;
    uint32 lastHolyVerdictTime = 0;
    uint32 lastDominionTime = 0;
    uint32 lastSetEchoTime = 0;
    uint32 lastSetSoulDevourTime = 0;
    uint32 lastSetDoubleBurstTime = 0;
    uint32 lastSetFreezeTime = 0;
    uint32 lastSetAncientPowerTime = 0;
    uint32 setAncientPowerEndTime = 0;
    uint32 lastSetAbyssDrainTime = 0;
    uint32 lastSetJudgmentTime = 0;
    uint32 lastSetSurgeTime = 0;
    uint32 lastSetDeathWardTime = 0;
    uint32 lastSetRebirthTime = 0;
    uint32 lastSetCombatGrowthTime = 0;
    uint32 setCombatGrowthStacks = 0;
    uint32 artifactFistComboCounter = 0;
    uint32 artifactStaffCadenceCounter = 0;
    uint32 artifactFalunOrbitCounter = 0;
    uint32 lastSystemRelicStateSyncTime = 0;
    uint32 matrixCastCounter = 0;
    uint32 dominionCounter = 0;
    uint8 blessingCurseChoice = 0;
    uint8 observerProtocol = 0;
    uint8 stagePerformanceStance = 0;
    uint32 stolenMechanicToken = 0;
    bool artifactCombatEchoUsed = false;
    bool artifactSubLinkUsed = false;
    float trackedPosX = 0.0f;
    float trackedPosY = 0.0f;
    float trackedPosZ = 0.0f;
    bool hasTrackedPosition = false;
    float lastPromptX = 0.0f;
    float lastPromptY = 0.0f;
    float lastPromptZ = 0.0f;
    float lastPromptO = 0.0f;
    bool hasPromptPosition = false;
    uint8 beastMode = 0;
    bool abyssModePromptShown = false;
    bool replayingSpell = false;
    uint32 managedSpellScaleFallbackSpellId = 0;
    uint32 managedSpellExecutionDepth = 0;
    ObjectGuid managedSpellTargetGuid;
    std::unordered_map<uint32, uint8> artifactSwordMarkStacks;
    std::unordered_map<uint32, uint8> artifactBowMarkStacks;
};

bool HasAnyActiveRelicSlots(PlayerAbyssData const* data)
{
    if (!data)
        return false;

    return data->mainRelic != 0 ||
        data->subRelic1 != 0 ||
        data->subRelic2 != 0 ||
        data->subRelic3 != 0 ||
        data->subRelic4 != 0 ||
        data->subRelic5 != 0 ||
        data->phaseArtifact != 0 ||
        data->ultimateArtifact != 0;
}

struct ScopedCounterGuard
{
    explicit ScopedCounterGuard(uint32& counter) : _counter(counter)
    {
        ++_counter;
    }

    ~ScopedCounterGuard()
    {
        if (_counter > 0)
            --_counter;
    }

private:
    uint32& _counter;
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
void SendAbyssStateToAddon(Player* player);

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

            if (config.relicType == 2)
            {
                config.activeSlot = ABYSS_RELIC_SLOT_PHASE;
                config.recommendedSlot = ABYSS_RELIC_SLOT_PHASE;
            }
            else if (config.relicType == 3)
            {
                config.activeSlot = ABYSS_RELIC_SLOT_ULTIMATE;
                config.recommendedSlot = ABYSS_RELIC_SLOT_ULTIMATE;
            }

            _relicConfigs[config.itemId] = config;
            ++count;
        } while (result->NextRow());
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
            config.sixPieceDesc     = fields[16].Get<std::string>();
            config.sixPieceDmgPct   = fields[17].Get<uint8>();
            config.sixPieceHpPct    = fields[18].Get<uint8>();
            config.sixPieceCrit     = fields[19].Get<uint16>();
            config.sixPieceHaste    = fields[20].Get<uint16>();
            config.sixPieceSpecialEffect = fields[21].Get<std::string>();
            config.eightPieceDesc   = fields[22].Get<std::string>();
            config.eightPieceDmgPct = fields[23].Get<uint8>();
            config.eightPieceHpPct  = fields[24].Get<uint8>();
            config.eightPieceCrit   = fields[25].Get<uint16>();
            config.eightPieceHaste  = fields[26].Get<uint16>();
            config.eightPieceSpecialEffect = fields[27].Get<std::string>();
            config.enabled          = fields[28].Get<bool>();

            _setBonusConfigs[config.setId] = config;
            ++count;
        } while (result->NextRow());

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

    void ClearPlayerSuspendedRunState(uint32 guid)
    {
        _suspendedRunStates.erase(guid);
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
        ClearPlayerSuspendedRunState(guid);
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

    PlayerAbyssRunState const* GetDisplayRunState(Player* player) const
    {
        if (!player)
            return nullptr;

        if (PlayerAbyssRunState const* activeRunState = GetPlayerRunState(player->GetGUID().GetCounter()))
            return activeRunState;

        auto itr = _suspendedRunStates.find(player->GetGUID().GetCounter());
        if (itr == _suspendedRunStates.end())
            return nullptr;

        PlayerAbyssRunState const& suspendedState = itr->second;
        if (suspendedState.currentChapterId == 0)
            return nullptr;

        AbyssChapterConfig const* chapter = GetChapterConfig(suspendedState.currentChapterId);
        if (!chapter || player->GetMapId() != chapter->mapId)
            return nullptr;

        uint32 instanceSignature = GetPlayerInstanceSignature(player, chapter->mapId);
        if (instanceSignature == 0 || suspendedState.currentInstanceId == 0 || instanceSignature != suspendedState.currentInstanceId)
            return nullptr;

        return &suspendedState;
    }

    uint32 GetPlayerInstanceSignature(Player* player, uint16 expectedMapId = 0) const
    {
        if (!player || !player->IsInWorld())
            return 0;

        if (expectedMapId != 0 && player->GetMapId() != expectedMapId)
            return 0;

        Map* map = player->GetMap();
        if (!map || (!map->IsDungeon() && !map->IsRaid()))
            return 0;

        return player->GetInstanceId();
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

    std::string GetBossBaseName(uint32 bossEntry) const
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

    std::string GetBossDisplayName(uint32 bossEntry) const
    {
        return GetBossBaseName(bossEntry);
    }

    std::string GetBossDisplayNameForMode(uint32 bossEntry, uint8 modeType) const
    {
        std::string baseName = GetBossBaseName(bossEntry);
        if (baseName.empty() || modeType == 0)
            return baseName;

        AbyssBossConfig const* bossConfig = GetBossConfig(bossEntry);
        if (!bossConfig || bossConfig->bossType != 1)
            return baseName;

        return std::string(GetModeBossPrefix(modeType)) + baseName;
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

        if (bossEntry == 0)
            return candidates;

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

        // 不同首领类型对应不同的掉落池语义:
        // - 锚点/最终首领: 基础档 = 正传底材 (sourceMode=1)
        // - 深渊首领: 基础档 = 深渊底材 (sourceMode=2)
        // - 秘藏首领: 忽略 sourceMode, 只掉唯一装备 (fromCacheBoss=true, equipmentType=2)
        // 运行时生效模式 = max(首领基础模式, 玩家当前模式), 从而让腐化/轮回难度下升级到更高品质的底材池。
        uint8 baseBossMode = 1;
        if (isAbyssBoss)
            baseBossMode = 2;
        uint8 effectiveMode = std::max<uint8>(modeType, baseBossMode);
        auto doesDockingMatchMode = [&](AbyssItemDocking const& docking) -> bool
        {
            if (AbyssEquipmentTemplate const* equipment = GetEquipmentTemplate(docking.itemId))
            {
                if (isCacheBoss)
                    return equipment->fromCacheBoss && equipment->equipmentType == 2;

                return !equipment->fromCacheBoss && equipment->sourceMode == effectiveMode;
            }

            if (isCacheBoss)
                return docking.dockingType == 5;

            std::string const& itemName = docking.itemName;
            switch (effectiveMode)
            {
                case 1: return StringStartsWith(itemName, "正传-") || StringStartsWith(itemName, "正传·");
                case 2: return StringStartsWith(itemName, "深渊-") || StringStartsWith(itemName, "深渊·");
                case 3: return StringStartsWith(itemName, "腐化-") || StringStartsWith(itemName, "腐化·");
                case 4: return StringStartsWith(itemName, "轮回-") || StringStartsWith(itemName, "轮回·");
                default: return false;
            }
        };
        auto getDockingSourceMode = [&](AbyssItemDocking const& docking) -> uint8
        {
            if (AbyssEquipmentTemplate const* equipment = GetEquipmentTemplate(docking.itemId))
                return equipment->sourceMode;

            return isCacheBoss ? 0 : effectiveMode;
        };

        auto equipmentMatches = [&](AbyssEquipmentTemplate const& equipment) -> bool
        {
            if (isCacheBoss)
                return equipment.fromCacheBoss && equipment.equipmentType == 2;
            if (isAbyssBoss || isAnchorOrFinal)
                return !equipment.fromCacheBoss
                    && equipment.equipmentType == 1
                    && equipment.sourceMode == effectiveMode;
            return false;
        };

        uint16 bestSourceChapter = 0;
        for (auto const& pair : _equipmentTemplates)
        {
            AbyssEquipmentTemplate const& equipment = pair.second;
            if (!equipment.enabled || equipment.actId != chapter.actId || equipment.sourceChapter > chapter.chapterId)
                continue;

            if (!equipmentMatches(equipment))
                continue;

            if (bestSourceChapter < equipment.sourceChapter)
                bestSourceChapter = equipment.sourceChapter;
        }

        for (auto const& pair : _equipmentTemplates)
        {
            AbyssEquipmentTemplate const& equipment = pair.second;
            if (!equipment.enabled || equipment.actId != chapter.actId || equipment.sourceChapter != bestSourceChapter)
                continue;

            if (!equipmentMatches(equipment))
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
                if (docking->dockingType == 5 && doesDockingMatchMode(*docking))
                    appendCandidate(docking->itemId, docking->itemName, docking->dockingType, docking->uniqueItem, docking->bagActivated, getDockingSourceMode(*docking), 0, 0);
                continue;
            }

            if (isAbyssBoss)
            {
                if ((docking->dockingType == 5 || docking->dockingType == 4) && doesDockingMatchMode(*docking))
                    appendCandidate(docking->itemId, docking->itemName, docking->dockingType, docking->uniqueItem, docking->bagActivated, getDockingSourceMode(*docking), 0, 0);
                continue;
            }

            if (isAnchorOrFinal && docking->dockingType == 4 && doesDockingMatchMode(*docking))
                appendCandidate(docking->itemId, docking->itemName, docking->dockingType, docking->uniqueItem, docking->bagActivated, getDockingSourceMode(*docking), 0, 0);
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

        if (bossEntry == chapter.cacheBossEntry)
        {
            if (RollPercentage() >= ABYSS_CACHE_ARTIFACT_DROP_CHANCE)
                return result;
        }
        else
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
        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(guid);
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
        else if (playerData)
        {
            considerRelicGroup(playerData->mainRelic, 1.0f);
            considerRelicGroup(playerData->subRelic1, GetConfiguredSubRelicScale(playerData->subRelic1));
            considerRelicGroup(playerData->subRelic2, GetConfiguredSubRelicScale(playerData->subRelic2));
            considerRelicGroup(playerData->subRelic3, GetConfiguredSubRelicScale(playerData->subRelic3));
            considerRelicGroup(playerData->subRelic4, GetConfiguredSubRelicScale(playerData->subRelic4));
            considerRelicGroup(playerData->subRelic5, GetConfiguredSubRelicScale(playerData->subRelic5));
        }

        if (playerData)
        {
            considerRelicGroup(playerData->phaseArtifact, 1.0f);
            considerRelicGroup(playerData->ultimateArtifact, 1.0f);
        }

        if (bestScale <= 0.0f && procState.managedSpellScaleFallbackSpellId != 0)
        {
            uint32 fallbackSpellId = procState.managedSpellScaleFallbackSpellId;
            auto fallbackMatches = [&](std::initializer_list<char const*> names)
            {
                for (char const* name : names)
                {
                    if (name && scriptGroup == name)
                        return true;
                }
                return false;
            };

            switch (fallbackSpellId)
            {
                case 89101: if (fallbackMatches({ "遗物_裂火炭核", "特效_裂火爆环" })) return 1.0f; break;
                case 89102: if (fallbackMatches({ "遗物_蛇蜕古胆" })) return 1.0f; break;
                case 89103: if (fallbackMatches({ "遗物_黑潮齿轮", "祝福_追命飞刃" })) return 1.0f; break;
                case 89104: if (fallbackMatches({ "遗物_狼王残月" })) return 1.0f; break;
                case 89105: if (fallbackMatches({ "遗物_深海祈眼", "遗物_潮蛇王鳞" })) return 1.0f; break;
                case 89106: if (fallbackMatches({ "遗物_断罪枷锁", "特效_锁链爆裂" })) return 1.0f; break;
                case 89107: if (fallbackMatches({ "遗物_爆线电枢", "祝福_雷暴连极" })) return 1.0f; break;
                case 89108: if (fallbackMatches({ "遗物_棘魂号角" })) return 1.0f; break;
                case 89109: if (fallbackMatches({ "遗物_血焰圣经", "祝福_日蚀焚城", "特效_血焰审判" })) return 1.0f; break;
                case 89110: if (fallbackMatches({ "遗物_枯王孢囊" })) return 1.0f; break;
                case 89111: if (fallbackMatches({ "遗物_泰坦偏轴", "祝福_逆时回响", "特效_时痕回放" })) return 1.0f; break;
                case 89112: if (fallbackMatches({ "遗物_黄沙时漏" })) return 1.0f; break;
                case 89113: if (fallbackMatches({ "遗物_腐花心核" })) return 1.0f; break;
                case 89114: if (fallbackMatches({ "遗物_梦沼眼膜" })) return 1.0f; break;
                case 89115: if (fallbackMatches({ "遗物_黑炉王印" })) return 1.0f; break;
                case 89116: if (fallbackMatches({ "遗物_龙骨炽芯" })) return 1.0f; break;
                case 89117: if (fallbackMatches({ "遗物_将军狱旗" })) return 1.0f; break;
                case 89118: if (fallbackMatches({ "遗物_古树孢祖" })) return 1.0f; break;
                case 89119: if (fallbackMatches({ "遗物_圣疫火烙" })) return 1.0f; break;
                case 89120: if (fallbackMatches({ "遗物_通灵逆契", "祝福_灵魂沸涌", "特效_魂火追击" })) return 1.0f; break;
                case 89121: if (fallbackMatches({ "遗物_残垒战契" })) return 1.0f; break;
                case 89122: if (fallbackMatches({ "遗物_邪血蒸馏器" })) return 1.0f; break;
                case 89123: if (fallbackMatches({ "遗物_破军碎牌" })) return 1.0f; break;
                case 89124: if (fallbackMatches({ "遗物_潮牢鳞灯" })) return 1.0f; break;
                case 89125: if (fallbackMatches({ "遗物_孢毒母囊" })) return 1.0f; break;
                case 89126: if (fallbackMatches({ "遗物_压阀导芯" })) return 1.0f; break;
                case 89127: if (fallbackMatches({ "遗物_法陵星匣" })) return 1.0f; break;
                case 89128: if (fallbackMatches({ "遗物_祭魂引灯" })) return 1.0f; break;
                case 89129: case 89148: if (fallbackMatches({ "遗物_鸦神命羽", "遗物_王陨号角" })) return 1.0f; break;
                case 89130: if (fallbackMatches({ "遗物_惧影迷盘" })) return 1.0f; break;
                case 89131: if (fallbackMatches({ "遗物_时痕怀表" })) return 1.0f; break;
                case 89132: if (fallbackMatches({ "遗物_裂隙砂轮" })) return 1.0f; break;
                case 89133: if (fallbackMatches({ "遗物_相位动轮" })) return 1.0f; break;
                case 89134: if (fallbackMatches({ "遗物_星植胚囊" })) return 1.0f; break;
                case 89135: if (fallbackMatches({ "遗物_禁狱零匙" })) return 1.0f; break;
                case 89136: if (fallbackMatches({ "遗物_逐日余晖" })) return 1.0f; break;
                case 89137: if (fallbackMatches({ "遗物_维库战祷" })) return 1.0f; break;
                case 89138: if (fallbackMatches({ "遗物_聚魔棱晶" })) return 1.0f; break;
                case 89139: if (fallbackMatches({ "遗物_蛛网夜卵" })) return 1.0f; break;
                case 89140: if (fallbackMatches({ "遗物_无面触冠" })) return 1.0f; break;
                case 89141: if (fallbackMatches({ "遗物_尸霜獠牙" })) return 1.0f; break;
                case 89142: if (fallbackMatches({ "遗物_紫狱印典" })) return 1.0f; break;
                case 89143: if (fallbackMatches({ "遗物_神噬断爪" })) return 1.0f; break;
                case 89144: if (fallbackMatches({ "遗物_泰坦记忆核" })) return 1.0f; break;
                case 89145: if (fallbackMatches({ "遗物_雷祖导体" })) return 1.0f; break;
                case 89146: if (fallbackMatches({ "遗物_时审钟摆" })) return 1.0f; break;
                case 89147: if (fallbackMatches({ "遗物_星界虹膜" })) return 1.0f; break;
                case 89149: if (fallbackMatches({ "遗物_冠军封缄" })) return 1.0f; break;
                case 89150: if (fallbackMatches({ "遗物_魂炉熔渣" })) return 1.0f; break;
                case 89151: if (fallbackMatches({ "遗物_萨钢骨钉" })) return 1.0f; break;
                case 89152: if (fallbackMatches({ "遗物_映像碎镜" })) return 1.0f; break;
                case 89153: if (fallbackMatches({ "遗物_熔界核髓" })) return 1.0f; break;
                case 89154: if (fallbackMatches({ "遗物_逆鳞王冠" })) return 1.0f; break;
                case 89155: if (fallbackMatches({ "遗物_畸变龙脊" })) return 1.0f; break;
                case 89156: if (fallbackMatches({ "遗物_虫群主脑" })) return 1.0f; break;
                case 89157: if (fallbackMatches({ "遗物_古神耳蜕" })) return 1.0f; break;
                case 89158: if (fallbackMatches({ "遗物_幕星假面" })) return 1.0f; break;
                case 89159: if (fallbackMatches({ "遗物_碎山指节" })) return 1.0f; break;
                case 89160: if (fallbackMatches({ "遗物_深狱锁冠" })) return 1.0f; break;
                case 89161: if (fallbackMatches({ "遗物_潮蛇王鳞" })) return 1.0f; break;
                case 89162: if (fallbackMatches({ "遗物_虚空矩阵", "祝福_双生元婴" })) return 1.0f; break;
                case 89163: if (fallbackMatches({ "遗物_守望战旌" })) return 1.0f; break;
                case 89164: if (fallbackMatches({ "遗物_伊利影印", "遗物_映像碎镜" })) return 1.0f; break;
                case 89165: if (fallbackMatches({ "遗物_祖灵战鼓" })) return 1.0f; break;
                case 89166: if (fallbackMatches({ "遗物_日蚀残晕" })) return 1.0f; break;
                case 89167: if (fallbackMatches({ "遗物_亡缝心炉" })) return 1.0f; break;
                case 89168: if (fallbackMatches({ "遗物_暮炎龙瞳" })) return 1.0f; break;
                case 89169: if (fallbackMatches({ "遗物_蓝脉天轮" })) return 1.0f; break;
                case 89170: if (fallbackMatches({ "遗物_观察者棱眼" })) return 1.0f; break;
                case 89171: if (fallbackMatches({ "遗物_圣陨判词", "祝福_断命法旨", "特效_圣陨终裁" })) return 1.0f; break;
                case 89172: if (fallbackMatches({ "遗物_复燃逆鳞" })) return 1.0f; break;
                case 89173: if (fallbackMatches({ "遗物_霜王残印", "祝福_极霜粉碎", "特效_霜王统御" })) return 1.0f; break;
                case 89174: if (fallbackMatches({ "遗物_赤玉界针" })) return 1.0f; break;
                case 89175: if (fallbackMatches({ "神器_焚界行契" })) return 0.85f; break;
                case 89176: if (fallbackMatches({ "神器_虚空远征印" })) return 0.85f; break;
                case 89177: if (fallbackMatches({ "神器_冰脉时匣" })) return 0.85f; break;
                case 89178: if (fallbackMatches({ "神器_虫神遗诏" })) return 0.85f; break;
                case 89179: if (fallbackMatches({ "神器_日蚀王契" })) return 0.85f; break;
                case 89180: if (fallbackMatches({ "神器_天灾断章" })) return 0.85f; break;
                case 89181: if (fallbackMatches({ "神器_渊主之印" })) return 0.95f; break;
                default:
                    break;
            }
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

    uint32 CountNearbyEnemies(Player* player, float radius) const
    {
        if (!player || radius <= 0.0f)
            return 0;

        std::list<Unit*> nearbyTargets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, radius);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(player, nearbyTargets, check);
        Cell::VisitAllObjects(player, searcher, radius);
        return static_cast<uint32>(nearbyTargets.size());
    }

    bool HasCrowdControlAuras(Unit* unit) const
    {
        if (!unit)
            return false;

        return unit->HasAuraType(SPELL_AURA_MOD_ROOT) ||
            unit->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED) ||
            unit->HasAuraType(SPELL_AURA_MOD_STUN) ||
            unit->HasAuraType(SPELL_AURA_MOD_FEAR);
    }

    void EnsureSystemRelicState(Player* player) const
    {
        if (!player)
            return;

        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());

        if (GetActiveRelicScale(player, ABYSS_RELIC_BLESS_CURSE_ITEM) > 0.0f && procState.blessingCurseChoice == 0)
            procState.blessingCurseChoice = 1;

        if (GetActiveRelicScale(player, ABYSS_RELIC_PROTOCOL_EYE_ITEM) > 0.0f && procState.observerProtocol == 0)
            procState.observerProtocol = 1;

        if (GetActiveRelicScale(player, ABYSS_RELIC_STAGE_STANCE_ITEM) > 0.0f && procState.stagePerformanceStance == 0)
            procState.stagePerformanceStance = 1;
    }

    char const* GetSystemRelicLabel(uint32 itemId) const
    {
        switch (itemId)
        {
            case ABYSS_RELIC_THEFT_KEY_ITEM:
                return "禁狱零匙";
            case ABYSS_RELIC_BLESS_CURSE_ITEM:
                return "紫狱印典";
            case ABYSS_RELIC_STAGE_STANCE_ITEM:
                return "幕星假面";
            case ABYSS_RELIC_PROTOCOL_EYE_ITEM:
                return "观察者棱眼";
            default:
                return "系统型遗物";
        }
    }

    void SyncSystemRelicState(Player* player) const
    {
        if (!player)
            return;

        EnsureSystemRelicState(player);

        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        uint32 now = GetNow();
        if (now <= procState.lastSystemRelicStateSyncTime + 30)
            return;

        procState.lastSystemRelicStateSyncTime = now;

        if (!IsDebugEnabled() || !player->GetSession())
            return;

        if (GetActiveRelicScale(player, ABYSS_RELIC_BLESS_CURSE_ITEM) > 0.0f)
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssSystemRelic] {} 已进入系统型骨架，当前暂用选择位={}。",
                GetSystemRelicLabel(ABYSS_RELIC_BLESS_CURSE_ITEM), procState.blessingCurseChoice);

        if (GetActiveRelicScale(player, ABYSS_RELIC_PROTOCOL_EYE_ITEM) > 0.0f)
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssSystemRelic] {} 已进入系统型骨架，当前协议位={}。",
                GetSystemRelicLabel(ABYSS_RELIC_PROTOCOL_EYE_ITEM), procState.observerProtocol);

        if (GetActiveRelicScale(player, ABYSS_RELIC_STAGE_STANCE_ITEM) > 0.0f)
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssSystemRelic] {} 已进入系统型骨架，当前姿态位={}。",
                GetSystemRelicLabel(ABYSS_RELIC_STAGE_STANCE_ITEM), procState.stagePerformanceStance);

        if (GetActiveRelicScale(player, ABYSS_RELIC_THEFT_KEY_ITEM) > 0.0f)
            ChatHandler(player->GetSession()).PSendSysMessage("[AbyssSystemRelic] {} 已进入系统型骨架，当前窃取槽位={}。",
                GetSystemRelicLabel(ABYSS_RELIC_THEFT_KEY_ITEM), procState.stolenMechanicToken);
    }

    float GetActiveRelicScale(Player* player, uint32 itemId) const
    {
        if (!player || itemId == 0)
            return 0.0f;

        AbyssRelicConfig const* relic = GetRelicConfig(itemId);
        if (!relic)
            return 0.0f;

        uint32 guid = player->GetGUID().GetCounter();
        PlayerAbyssRunState const* runState = GetPlayerRunState(guid);
        PlayerAbyssData const* playerData = GetPlayerData(guid);

        float bestScale = 0.0f;
        auto considerSlot = [&](uint32 activeItemId, uint8 slot)
        {
            if (activeItemId != itemId)
                return;

            bestScale = std::max(bestScale, GetRelicSlotScale(*relic, slot));
        };

        if (runState)
        {
            considerSlot(runState->runMainRelic, ABYSS_RELIC_SLOT_MAIN);
            considerSlot(runState->runSubRelic1, ABYSS_RELIC_SLOT_SUB_1);
            considerSlot(runState->runSubRelic2, ABYSS_RELIC_SLOT_SUB_2);
            considerSlot(runState->runSubRelic3, ABYSS_RELIC_SLOT_SUB_3);
            considerSlot(runState->runSubRelic4, ABYSS_RELIC_SLOT_SUB_4);
            considerSlot(runState->runSubRelic5, ABYSS_RELIC_SLOT_SUB_5);
        }
        else if (playerData)
        {
            considerSlot(playerData->mainRelic, ABYSS_RELIC_SLOT_MAIN);
            considerSlot(playerData->subRelic1, ABYSS_RELIC_SLOT_SUB_1);
            considerSlot(playerData->subRelic2, ABYSS_RELIC_SLOT_SUB_2);
            considerSlot(playerData->subRelic3, ABYSS_RELIC_SLOT_SUB_3);
            considerSlot(playerData->subRelic4, ABYSS_RELIC_SLOT_SUB_4);
            considerSlot(playerData->subRelic5, ABYSS_RELIC_SLOT_SUB_5);
        }

        if (playerData)
        {
            considerSlot(playerData->phaseArtifact, ABYSS_RELIC_SLOT_PHASE);
            considerSlot(playerData->ultimateArtifact, ABYSS_RELIC_SLOT_ULTIMATE);
        }

        return bestScale;
    }

    uint32 GetPreferredManagedRelicSpell(Player* player, std::initializer_list<std::pair<uint32, uint32>> relicSpells) const
    {
        if (!player)
            return 0;

        float bestScale = 0.0f;
        uint32 bestSpellId = 0;
        for (auto const& relicSpell : relicSpells)
        {
            float scale = GetActiveRelicScale(player, relicSpell.first);
            if (scale > bestScale)
            {
                bestScale = scale;
                bestSpellId = relicSpell.second;
            }
        }

        return bestSpellId;
    }

    uint32 GetManagedSpellIdForRelicItem(uint32 itemId) const
    {
        if (itemId >= 950001 && itemId <= 950074)
            return 89100 + (itemId - 950000);
        if (itemId >= 960001 && itemId <= 960006)
            return 89174 + (itemId - 960000);
        if (itemId == 970001)
            return 89181;
        return 0;
    }

    bool CastManagedRelicSpell(Player* player, uint32 spellId) const
    {
        if (!player || !IsManagedRelicSpell(spellId))
            return false;

        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        ScopedCounterGuard managedSpellExecutionGuard(procState.managedSpellExecutionDepth);

        Unit* castTarget = player;
        if (!IsSelfTargetManagedRelicSpell(spellId))
        {
            if (Unit* primaryTarget = GetPrimaryCombatTarget(player))
                castTarget = primaryTarget;
            else
            {
                ExecuteManagedRelicSpell(player, spellId);
                castTarget = nullptr;
            }
        }

        if (castTarget)
            player->CastSpell(castTarget, spellId, true);

        if (spellId <= 89174)
        {
            PlayerAbyssData const* playerData = GetPlayerData(player->GetGUID().GetCounter());
            if (playerData)
            {
                uint32 mainRelicSpell = GetManagedSpellIdForRelicItem(playerData->mainRelic);
                uint32 subRelic1Spell = GetManagedSpellIdForRelicItem(playerData->subRelic1);
                uint32 subRelic2Spell = GetManagedSpellIdForRelicItem(playerData->subRelic2);

                if (mainRelicSpell != 0 && spellId == mainRelicSpell)
                {
                    ++procState.artifactMainProcCounter;

                    switch (playerData->phaseArtifact)
                    {
                        case ABYSS_PHASE_ARTIFACT_BURNING_PACT_ITEM:
                            if (procState.artifactMainProcCounter % 4 == 0 && subRelic1Spell != 0)
                                CastManagedRelicSpell(player, subRelic1Spell);
                            break;
                        case ABYSS_PHASE_ARTIFACT_BUG_DECREE_ITEM:
                            if (!procState.artifactCombatEchoUsed)
                            {
                                procState.artifactCombatEchoUsed = true;
                                CastManagedRelicSpell(player, mainRelicSpell);
                            }
                            break;
                        case ABYSS_PHASE_ARTIFACT_SCOURGE_CHAPTER_ITEM:
                        {
                            uint32 echoCandidates[6] = { subRelic1Spell, subRelic2Spell, GetManagedSpellIdForRelicItem(playerData->subRelic3), GetManagedSpellIdForRelicItem(playerData->subRelic4), GetManagedSpellIdForRelicItem(playerData->subRelic5), 0 };
                            std::vector<uint32> valid;
                            for (uint32 candidate : echoCandidates)
                                if (candidate != 0)
                                    valid.push_back(candidate);
                            if (!valid.empty())
                                CastManagedRelicSpell(player, valid[RollWeight(static_cast<uint32>(valid.size()))]);
                            break;
                        }
                        default:
                            break;
                    }
                }

                if ((spellId == subRelic1Spell || spellId == subRelic2Spell) && playerData->phaseArtifact == ABYSS_PHASE_ARTIFACT_ECLIPSE_KING_ITEM)
                {
                    if (!procState.artifactSubLinkUsed)
                    {
                        procState.artifactSubLinkUsed = true;
                        CastManagedRelicSpell(player, 89166);
                    }
                }
            }
        }

        return true;
    }

    void ExecuteManagedRelicSpell(Player* player, uint32 spellId, Unit* explicitTarget = nullptr) const
    {
        if (!player || !player->IsAlive())
            return;

        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        ScopedCounterGuard managedSpellExecutionGuard(procState.managedSpellExecutionDepth);
        ObjectGuid previousManagedTargetGuid = procState.managedSpellTargetGuid;
        uint32 previousManagedSpellScaleFallbackSpellId = procState.managedSpellScaleFallbackSpellId;

        if (explicitTarget && explicitTarget->IsAlive() && player->IsValidAttackTarget(explicitTarget))
            procState.managedSpellTargetGuid = explicitTarget->GetGUID();
        else
            procState.managedSpellTargetGuid.Clear();
        procState.managedSpellScaleFallbackSpellId = spellId;

        uint32 dummyCooldown = 0;
        switch (spellId)
        {
            case 89101: // 裂火炭核
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_裂火炭核", "特效_裂火爆环" });
                TriggerFireRing(player, dummyCooldown, scale);
                break;
            }
            case 89102: // 蛇蜕古胆
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_蛇蜕古胆");
                TriggerPoisonBurst(player, dummyCooldown, scale);
                break;
            }
            case 89103: // 黑潮齿轮
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_黑潮齿轮", "祝福_追命飞刃" });
                if (scale > 0.0f)
                    if (Unit* target = GetPrimaryCombatTarget(player))
                        DealConfiguredBurst(player, target, 100, 160, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89104: // 狼王残月
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_狼王残月");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);
                break;
            }
            case 89105: // 深海祈眼 / 潮蛇王鳞
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_深海祈眼", "遗物_潮蛇王鳞" });
                TriggerTideBurst(player, dummyCooldown, scale);
                break;
            }
            case 89106: // 断罪枷锁
            {
                float scale = std::max(
                    GetActiveScriptGroupScale(player, "遗物_断罪枷锁"),
                    GetActiveScriptGroupScale(player, "特效_锁链爆裂"));
                TriggerLowHealthRetaliation(player, dummyCooldown, scale);
                break;
            }
            case 89107: // 爆线电枢
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_爆线电枢", "祝福_雷暴连极" });
                TriggerChainLightning(player, dummyCooldown, scale);
                break;
            }
            case 89108: // 棘魂号角
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_棘魂号角");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 170, 250, SPELL_SCHOOL_MASK_NORMAL, scale);
                break;
            }
            case 89109: // 血焰圣经
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_血焰圣经", "祝福_日蚀焚城", "特效_血焰审判" });
                TriggerBloodFlameBurst(player, dummyCooldown, scale);
                break;
            }
            case 89110: // 枯王孢囊
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_枯王孢囊");
                TriggerSwarmBurst(player, dummyCooldown, scale);
                break;
            }
            case 89111: // 泰坦偏轴
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_泰坦偏轴", "祝福_逆时回响", "特效_时痕回放" });
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "时痕回放", scale);
                break;
            }
            case 89112: // 黄沙时漏
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_黄沙时漏");
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "黄沙残像", scale);
                break;
            }
            case 89113: // 腐花心核
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_腐花心核");
                TriggerBloomBurst(player, dummyCooldown, scale);
                player->ModifyPower64(player->getPowerType(), ScaleIntValue(10, scale));
                break;
            }
            case 89114: // 梦沼眼膜
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_梦沼眼膜");
                TriggerDreamBurst(player, dummyCooldown, scale);
                break;
            }
            case 89115: // 黑炉王印
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_黑炉王印");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 190, 270, SPELL_SCHOOL_MASK_FIRE, scale);
                break;
            }
            case 89116: // 龙骨炽芯
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_龙骨炽芯");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_FIRE, scale);
                break;
            }
            case 89118: // 古树孢祖
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_古树孢祖");
                TriggerBloomBurst(player, dummyCooldown, scale);
                break;
            }
            case 89119: // 圣疫火烙
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_圣疫火烙");
                TriggerBloodFlameBurst(player, dummyCooldown, scale);
                break;
            }
            case 89120: // 通灵逆契
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_通灵逆契", "祝福_灵魂沸涌", "特效_魂火追击" });
                TriggerSoulBurst(player, dummyCooldown, scale);
                break;
            }
            case 89117: // 将军狱旗
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_将军狱旗");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 140, 220, SPELL_SCHOOL_MASK_NORMAL, scale);
                break;
            }
            case 89121: // 残垒战契
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_残垒战契");
                if (scale > 0.0f)
                {
                    player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(4), scale));
                    if (Unit* target = GetPrimaryCombatTarget(player))
                        DealConfiguredBurst(player, target, 160, 240, SPELL_SCHOOL_MASK_NORMAL, scale);
                }
                break;
            }
            case 89122: // 邪血蒸馏器
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_邪血蒸馏器");
                if (scale > 0.0f)
                {
                    player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(3), scale));
                    if (Unit* target = GetPrimaryCombatTarget(player))
                        DealConfiguredBurst(player, target, 150, 230, SPELL_SCHOOL_MASK_SHADOW, scale);
                }
                break;
            }
            case 89123: // 破军碎牌
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_破军碎牌");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);
                break;
            }
            case 89124: // 潮牢鳞灯
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_潮牢鳞灯");
                TriggerTideBurst(player, dummyCooldown, scale);
                break;
            }
            case 89125: // 孢毒母囊
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_孢毒母囊");
                TriggerSwarmBurst(player, dummyCooldown, scale);
                break;
            }
            case 89126: // 压阀导芯
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_压阀导芯");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_FIRE, scale);

                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                procState.lastChainLightningTime = (procState.lastChainLightningTime > 2 ? procState.lastChainLightningTime - 2 : 0);
                procState.lastPoisonBurstTime = (procState.lastPoisonBurstTime > 2 ? procState.lastPoisonBurstTime - 2 : 0);
                procState.lastTideBurstTime = (procState.lastTideBurstTime > 2 ? procState.lastTideBurstTime - 2 : 0);
                procState.lastArcaneAshTime = (procState.lastArcaneAshTime > 2 ? procState.lastArcaneAshTime - 2 : 0);
                break;
            }
            case 89127: // 法陵星匣
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_法陵星匣");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 200, 280, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89128: // 祭魂引灯
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_祭魂引灯");
                TriggerSoulBurst(player, dummyCooldown, scale);
                break;
            }
            case 89129: // 鸦神命羽 / 王陨号角
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_鸦神命羽", "遗物_王陨号角" });
                TriggerFeatherVolley(player, dummyCooldown, scale);
                break;
            }
            case 89148: // 王陨号角
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_王陨号角");
                TriggerFeatherVolley(player, dummyCooldown, scale);
                break;
            }
            case 89149: // 冠军封缄
            {
                static uint32 const kBlessingPool[] = { 89007, 89010, 89028 };
                player->CastSpell(player, kBlessingPool[RollWeight(3)], true);
                break;
            }
            case 89130: // 惧影迷盘
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_惧影迷盘");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 260, 380, SPELL_SCHOOL_MASK_SHADOW, scale);
                break;
            }
            case 89131: // 时痕怀表
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_时痕怀表");
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "时痕回声", scale);
                break;
            }
            case 89132: // 裂隙砂轮
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_裂隙砂轮");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89133: // 相位动轮
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_相位动轮");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 160, 240, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89134: // 星植胚囊
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_星植胚囊");
                TriggerBloomBurst(player, dummyCooldown, scale);
                break;
            }
            case 89136: // 逐日余晖
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_逐日余晖");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 210, 300, SPELL_SCHOOL_MASK_FIRE, scale);
                break;
            }
            case 89137: // 维库战祷
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_维库战祷");
                if (scale > 0.0f)
                {
                    player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(2), scale));
                    if (Unit* target = GetPrimaryCombatTarget(player))
                        DealConfiguredBurst(player, target, 60, 100, SPELL_SCHOOL_MASK_NORMAL, scale);

                    SendAbyssEffectMessage(player, "[AbyssEffect] 维库战祷触发。");
                }
                break;
            }
            case 89138: // 聚魔棱晶
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_聚魔棱晶");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89139: // 蛛网夜卵
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_蛛网夜卵");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 170, 250, SPELL_SCHOOL_MASK_NATURE, scale);
                break;
            }
            case 89140: // 无面触冠
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_无面触冠");
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "无面复读", scale);
                break;
            }
            case 89141: // 尸霜獠牙
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_尸霜獠牙");
                TriggerDominionBurst(player, dummyCooldown, scale);
                break;
            }
            case 89143: // 神噬断爪
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_神噬断爪");
                if (scale > 0.0f)
                {
                    PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                    procState.beastMode = static_cast<uint8>((procState.beastMode + 1) % 3);
                    if (Unit* target = GetPrimaryCombatTarget(player))
                    {
                        uint32 minDamage = procState.beastMode == 0 ? 90u : (procState.beastMode == 1 ? 110u : 130u);
                        uint32 maxDamage = procState.beastMode == 0 ? 140u : (procState.beastMode == 1 ? 170u : 200u);
                        DealConfiguredBurst(player, target, minDamage, maxDamage, SPELL_SCHOOL_MASK_NORMAL, scale);
                    }

                    SendAbyssEffectMessage(player, "[AbyssEffect] 兽神姿态触发。");
                }
                break;
            }
            case 89144: // 泰坦记忆核
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_泰坦记忆核");
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "泰坦记忆回放", scale);
                break;
            }
            case 89145: // 雷祖导体
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_雷祖导体");
                TriggerChainLightning(player, dummyCooldown, scale);
                break;
            }
            case 89146: // 时审钟摆
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_时审钟摆");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89147: // 星界虹膜
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_星界虹膜");
                TriggerStarfallBurst(player, dummyCooldown, scale);
                break;
            }
            case 89150: // 魂炉熔渣
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_魂炉熔渣");
                TriggerSoulBurst(player, dummyCooldown, scale);
                break;
            }
            case 89151: // 萨钢骨钉
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_萨钢骨钉");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 230, 340, SPELL_SCHOOL_MASK_SHADOW, scale);
                break;
            }
            case 89152: // 映像碎镜
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_映像碎镜");
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "镜像反射", scale);
                break;
            }
            case 89159: // 碎山指节
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_碎山指节");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 260, 360, SPELL_SCHOOL_MASK_NORMAL, scale);
                break;
            }
            case 89160: // 深狱锁冠
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_深狱锁冠");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 200, 300, SPELL_SCHOOL_MASK_FIRE, scale);
                break;
            }
            case 89153: // 熔界核髓
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_熔界核髓");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 190, 280, SPELL_SCHOOL_MASK_FIRE, scale);
                break;
            }
            case 89155: // 畸变龙脊
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_畸变龙脊");
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "畸变超载", scale);
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "畸变超载", scale);
                break;
            }
            case 89156: // 虫群主脑
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_虫群主脑");
                TriggerSwarmBurst(player, dummyCooldown, scale);
                break;
            }
            case 89157: // 古神耳蜕
            {
                static uint32 const kBlessingPool[] = { 89007, 89010, 89028 };
                player->CastSpell(player, kBlessingPool[RollWeight(3)], true);
                player->CastSpell(player, 89025, true);
                break;
            }
            case 89162: // 虚空矩阵
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_虚空矩阵", "祝福_双生元婴" });
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "虚空矩阵复写", scale);
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "虚空矩阵复写", scale);
                break;
            }
            case 89163: // 守望战旌
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_守望战旌");
                TriggerBattleBannerBurst(player, dummyCooldown, scale);
                break;
            }
            case 89164: // 伊利影印 / 映像碎镜
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_伊利影印", "遗物_映像碎镜" });
                PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
                TriggerMirrorEcho(player, procState.lastSpellId, dummyCooldown, "影印残像", scale);
                break;
            }
            case 89167: // 亡缝心炉
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_亡缝心炉");
                TriggerSwarmBurst(player, dummyCooldown, scale);
                break;
            }
            case 89168: // 暮炎龙瞳
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_暮炎龙瞳");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 230, 320, SpellSchoolMask(SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_SHADOW), scale);
                break;
            }
            case 89169: // 蓝脉天轮
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_蓝脉天轮");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 210, 300, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89165: // 祖灵战鼓
            {
                static uint32 const kBlessingPool[] = { 89007, 89010, 89028 };
                uint32 randomSpell = kBlessingPool[RollWeight(3)];
                player->CastSpell(player, randomSpell, true);
                break;
            }
            case 89161: // 潮蛇王鳞
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_潮蛇王鳞");
                TriggerTideBurst(player, dummyCooldown, scale);
                break;
            }
            case 89154: // 逆鳞王冠
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_逆鳞王冠");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);
                break;
            }
            case 89166: // 日蚀残晕
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_日蚀残晕");
                if (Unit* target = GetPrimaryCombatTarget(player))
                    DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_ARCANE, scale);
                break;
            }
            case 89175: // 焚界行契
            {
                player->CastSpell(player, 89007, true);
                break;
            }
            case 89176: // 虚空远征印
            {
                player->CastSpell(player, 89028, true);
                break;
            }
            case 89177: // 冰脉时匣
            {
                player->CastSpell(player, 89019, true);
                break;
            }
            case 89178: // 虫神遗诏
            {
                player->CastSpell(player, 89010, true);
                break;
            }
            case 89179: // 日蚀王契
            {
                CastManagedRelicSpell(player, 89166);
                break;
            }
            case 89180: // 天灾断章
            {
                player->CastSpell(player, 89028, true);
                break;
            }
            case 89181: // 渊主之印
            {
                player->CastSpell(player, 89019, true);
                break;
            }
            case 89171: // 圣陨判词
            {
                float scale = std::max(
                    GetActiveScriptGroupScale(player, "遗物_圣陨判词"),
                    std::max(
                        GetActiveScriptGroupScale(player, "祝福_断命法旨"),
                        GetActiveScriptGroupScale(player, "特效_圣陨终裁")));

                if (player->HealthBelowPct(25))
                    TriggerHolyVerdict(player, dummyCooldown, scale);
                else
                    TriggerLowHealthRetaliation(player, dummyCooldown, scale);
                break;
            }
            case 89173: // 霜王残印
            {
                float scale = GetActiveScriptGroupScale(player, { "遗物_霜王残印", "祝福_极霜粉碎", "特效_霜王统御" });
                TriggerDominionBurst(player, dummyCooldown, scale);
                break;
            }
            case 89174: // 赤玉界针
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_赤玉界针");
                TriggerRedJadeBurst(player, dummyCooldown, scale);
                break;
            }
            case 89172: // 复燃逆鳞
            {
                float scale = GetActiveScriptGroupScale(player, "遗物_复燃逆鳞");
                TriggerLowHealthRetaliation(player, dummyCooldown, scale);
                break;
            }
            default:
                break;
        }

        procState.managedSpellTargetGuid = previousManagedTargetGuid;
        procState.managedSpellScaleFallbackSpellId = previousManagedSpellScaleFallbackSpellId;
    }

    Unit* GetPrimaryCombatTarget(Player* player) const
    {
        if (!player)
            return nullptr;

        PlayerAbyssProcState& procState = const_cast<AbyssCultivationMgr*>(this)->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        if (!procState.managedSpellTargetGuid.IsEmpty())
        {
            if (Unit* managedTarget = ObjectAccessor::GetUnit(*player, procState.managedSpellTargetGuid))
            {
                if (managedTarget->IsAlive() && player->IsValidAttackTarget(managedTarget))
                    return managedTarget;
            }

            procState.managedSpellTargetGuid.Clear();
        }

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

        uint64 damage = scaledMinDamage;
        if (scaledMaxDamage > scaledMinDamage)
            damage += RollWeight(scaledMaxDamage - scaledMinDamage + 1);

        player->DealDamage(player, target, damage, nullptr, SPELL_DIRECT_DAMAGE, schoolMask);
    }

    void PlayChainLightningArc(Unit* source, Unit* target) const
    {
        if (!source || !target || source == target)
            return;

        if ((source->IsCreature() && source->ToCreature()->IsTrigger()) ||
            (target->IsCreature() && target->ToCreature()->IsTrigger()) ||
            !target->isTargetableForAttack(false, source))
            return;

        // 怪对怪电弧 impact 在部分客户端/模型组合下不稳定，保留伤害与弹射，
        // 但不再发送这条额外表现包，避免客户端崩溃。
    }

    Unit* SelectChainLightningBounceTarget(Player* player, WorldObject* center, Unit* exclude1 = nullptr, Unit* exclude2 = nullptr, float radius = 15.0f) const
    {
        if (!player || !center || radius <= 0.0f)
            return nullptr;

        std::list<Unit*> nearbyTargets;
        Acore::AnyUnfriendlyNoTotemUnitInObjectRangeCheck check(center, player, radius);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyNoTotemUnitInObjectRangeCheck> searcher(center, nearbyTargets, check);
        Cell::VisitAllObjects(center, searcher, radius);

        Unit* bestTarget = nullptr;
        float bestDistanceSq = std::numeric_limits<float>::max();
        Unit const* centerUnit = center->ToUnit();
        for (Unit* candidate : nearbyTargets)
        {
            if (!candidate)
                continue;

            if (candidate == player || candidate == centerUnit || candidate == exclude1 || candidate == exclude2)
                continue;

            if (!candidate->IsAlive() || !player->IsValidAttackTarget(candidate))
                continue;

            if ((candidate->IsCreature() && candidate->ToCreature()->IsTrigger()) ||
                !candidate->isTargetableForAttack(false, player))
                continue;

            if (!center->IsWithinLOSInMap(candidate))
                continue;

            float distanceSq = center->GetExactDist2dSq(candidate);
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                bestTarget = candidate;
            }
        }

        if (!bestTarget)
        {
            if (Unit const* centerUnit = center->ToUnit())
            {
                if (Unit* fallbackTarget = centerUnit->SelectNearbyNoTotemTarget(exclude1, radius))
                {
                    if (fallbackTarget != exclude2 &&
                        fallbackTarget != player &&
                        fallbackTarget != centerUnit &&
                        fallbackTarget->IsAlive() &&
                        player->IsValidAttackTarget(fallbackTarget))
                    {
                        bestTarget = fallbackTarget;
                    }
                }
            }
        }

        return bestTarget;
    }

    template <typename... Args>
    void SendAbyssEffectMessage(Player* player, char const* format, Args&&... args) const
    {
        if (!player || !player->GetSession() || !IsDebugEnabled())
            return;

        ChatHandler(player->GetSession()).PSendSysMessage(format, std::forward<Args>(args)...);
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
        SendAbyssEffectMessage(player, "[AbyssEffect] 裂火爆环触发。");
    }

    void TriggerChainLightning(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* primaryTarget = GetPrimaryCombatTarget(player);
        if (!primaryTarget)
            return;

        DealConfiguredBurst(player, primaryTarget, 140, 220, SPELL_SCHOOL_MASK_NATURE, scale);

        if (Unit* bounce1 = SelectChainLightningBounceTarget(player, primaryTarget, nullptr, nullptr, 15.0f))
        {
            DealConfiguredBurst(player, bounce1, 100, 160, SPELL_SCHOOL_MASK_NATURE, scale);
            PlayChainLightningArc(primaryTarget, bounce1);

            if (Unit* bounce2 = SelectChainLightningBounceTarget(player, bounce1, primaryTarget, nullptr, 15.0f))
            {
                DealConfiguredBurst(player, bounce2, 80, 120, SPELL_SCHOOL_MASK_NATURE, scale);
                PlayChainLightningArc(bounce1, bounce2);
            }
        }

        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 连锁雷击触发。");
    }

    void TriggerSoulBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(6), scale));
        player->ModifyPower64(player->getPowerType(), ScaleIntValue(20, scale));
        cooldownTime = GetNow();

        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 90, 140, SPELL_SCHOOL_MASK_SHADOW, scale);

        SendAbyssEffectMessage(player, "[AbyssEffect] 魂火追击触发。");
    }

    void TriggerLowHealthRetaliation(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(12), scale));
        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);

        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 锁链反打触发。");
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

        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(3), scale));
        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 蛇蜕毒爆触发。");
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
        SendAbyssEffectMessage(player, "[AbyssEffect] 血焰审判触发。");
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
        SendAbyssEffectMessage(player, "[AbyssEffect] 命羽齐射触发。");
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
        SendAbyssEffectMessage(player, "[AbyssEffect] 星界坠临触发。");
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

        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(4), scale));
        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 潮汐回流触发。");
    }

    void TriggerBloomBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* target = GetPrimaryCombatTarget(player);
        if (!target)
            return;

        DealConfiguredBurst(player, target, 140, 210, SPELL_SCHOOL_MASK_NATURE, scale);
        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(5), scale));

        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 古树/腐花协战触发。");
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

        SendAbyssEffectMessage(player, "[AbyssEffect] {}触发。", label);
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
        SendAbyssEffectMessage(player, "[AbyssEffect] 虫群/孢囊爆裂触发。");
    }

    void TriggerBattleBannerBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        Unit* target = GetPrimaryCombatTarget(player);
        if (!target)
            return;

        DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_NORMAL, scale);
        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(3), scale));
        cooldownTime = GetNow();

        SendAbyssEffectMessage(player, "[AbyssEffect] 守望战旌触发。");
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
        SendAbyssEffectMessage(player, "[AbyssEffect] 赤玉终焉触发。");
    }

    void TriggerDreamBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 150, 220, SPELL_SCHOOL_MASK_SHADOW, scale);

        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 梦沼暗爆触发。");
    }

    void TriggerHolyVerdict(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(18), scale));
        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_NORMAL, scale);

        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 圣陨终裁触发。");
    }

    void TriggerDominionBurst(Player* player, uint32& cooldownTime, float scale = 1.0f) const
    {
        if (!player || scale <= 0.0f)
            return;

        if (Unit* target = GetPrimaryCombatTarget(player))
            DealConfiguredBurst(player, target, 260, 360, SPELL_SCHOOL_MASK_FROST, scale);

        cooldownTime = GetNow();
        SendAbyssEffectMessage(player, "[AbyssEffect] 霜王统御触发。");
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

        std::string previousChapterName;
        if (AbyssChapterConfig const* chapterConfig = GetChapterConfig(runState->currentChapterId))
            previousChapterName = chapterConfig->chapterName;
        else if (AbyssChapterConfig const* mapChapterConfig = GetChapterConfigByMapId(runState->currentMapId))
            previousChapterName = mapChapterConfig->chapterName;

        SuspendPlayerRun(player);
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("你已经离开 {}。", previousChapterName.empty() ? "当前副本" : previousChapterName);
        return true;
    }

    void ProcessBasicKillRelicTriggers(Player* player, Creature* creature, PlayerAbyssProcState& procState, uint64 trackedMaskBit = 0)
    {
        if (!player || !creature)
            return;

        float fireRingScale = GetActiveScriptGroupScale(player, { "遗物_裂火炭核", "特效_裂火爆环" });
        if (fireRingScale > 0.0f)
        {
            ++procState.killCounter;
            // 测试期下调为击杀 1 个目标即可触发，便于验证 89101 的视觉表现。
            if (GetNow() > procState.lastFireRingTime + 3 && procState.killCounter >= 1)
            {
                procState.killCounter = 0;
                if (CastManagedRelicSpell(player, 89101))
                    procState.lastFireRingTime = GetNow();
            }
        }

        float soulScale = GetActiveScriptGroupScale(player, { "遗物_通灵逆契", "祝福_灵魂沸涌", "特效_魂火追击" });
        if (soulScale > 0.0f)
        {
            ++procState.bloodKillCounter;
            if (GetNow() > procState.lastSoulProcTime + 1 && procState.bloodKillCounter >= 1)
            {
                procState.bloodKillCounter = 0;
                if (CastManagedRelicSpell(player, 89120))
                    procState.lastSoulProcTime = GetNow();
            }
        }

        float sporeScale = GetActiveScriptGroupScale(player, "遗物_枯王孢囊");
        if (sporeScale > 0.0f)
        {
            ++procState.sporeKillCounter;
            if (GetNow() > procState.lastSporeBurstTime + 6 && procState.sporeKillCounter >= 2)
            {
                procState.sporeKillCounter = 0;
                if (CastManagedRelicSpell(player, 89110))
                    procState.lastSporeBurstTime = GetNow();
            }
        }

        float dragonScale = GetActiveScriptGroupScale(player, "遗物_龙骨炽芯");
        if (dragonScale > 0.0f)
        {
            ++procState.dragonRageKillCounter;
            if (GetNow() > procState.lastDragonRageTime + 12 && procState.dragonRageKillCounter >= 3)
            {
                procState.dragonRageKillCounter = 0;
                if (CastManagedRelicSpell(player, 89116))
                    procState.lastDragonRageTime = GetNow();
            }
        }

        float mushroomScale = GetActiveScriptGroupScale(player, "遗物_孢毒母囊");
        if (mushroomScale > 0.0f)
        {
            ++procState.mushroomKillCounter;
            if (GetNow() > procState.lastMushroomBurstTime + 5 && procState.mushroomKillCounter >= 2)
            {
                procState.mushroomKillCounter = 0;
                if (CastManagedRelicSpell(player, 89125))
                    procState.lastMushroomBurstTime = GetNow();
            }
        }

        float soulLampScale = GetActiveScriptGroupScale(player, "遗物_祭魂引灯");
        if (soulLampScale > 0.0f && GetNow() > procState.lastSoulLampTime + 4)
        {
            if (CastManagedRelicSpell(player, 89128))
                procState.lastSoulLampTime = GetNow();
        }

        float sandScale = GetActiveScriptGroupScale(player, "遗物_黄沙时漏");
        if (sandScale > 0.0f && procState.lastSpellId != 0 && GetNow() > procState.lastSandEchoTime + 6)
        {
            if (CastManagedRelicSpell(player, 89112))
                procState.lastSandEchoTime = GetNow();
        }

        float plagueScale = GetActiveScriptGroupScale(player, "遗物_圣疫火烙");
        if (plagueScale > 0.0f && GetNow() > procState.lastPlagueSpreadTime + 5)
        {
            if (CastManagedRelicSpell(player, 89119))
                procState.lastPlagueSpreadTime = GetNow();
        }

        float featherScale = GetActiveScriptGroupScale(player, { "遗物_鸦神命羽", "遗物_王陨号角" });
        if (featherScale > 0.0f && GetNow() > procState.lastFeatherVolleyTime + 2)
        {
            uint32 featherSpellId = GetPreferredManagedRelicSpell(player, { {950029, 89129}, {950048, 89148} });
            if (CastManagedRelicSpell(player, featherSpellId))
                procState.lastFeatherVolleyTime = GetNow();
        }

        float soulFurnaceScale = GetActiveScriptGroupScale(player, "遗物_魂炉熔渣");
        if (soulFurnaceScale > 0.0f && GetNow() > procState.lastSoulFurnaceTime + 3)
        {
            if (CastManagedRelicSpell(player, 89150))
                procState.lastSoulFurnaceTime = GetNow();
        }

        float ancestorScale = GetActiveScriptGroupScale(player, "遗物_祖灵战鼓");
        if (ancestorScale > 0.0f && GetNow() > procState.lastAncestorBlessTime + 4)
        {
            if (CastManagedRelicSpell(player, 89165))
                procState.lastAncestorBlessTime = GetNow();
        }

        float championScale = GetActiveScriptGroupScale(player, "遗物_冠军封缄");
        if (championScale > 0.0f && creature->isElite() && GetNow() > procState.lastChampionSealTime + 8)
        {
            if (CastManagedRelicSpell(player, 89149))
                procState.lastChampionSealTime = GetNow();
        }

        float stitchScale = GetActiveScriptGroupScale(player, "遗物_亡缝心炉");
        if (stitchScale > 0.0f)
        {
            ++procState.stitchKillCounter;
            if (GetNow() > procState.lastStitchTime + 12 && procState.stitchKillCounter >= 5)
            {
                procState.stitchKillCounter = 0;
                if (CastManagedRelicSpell(player, 89167))
                    procState.lastStitchTime = GetNow();
            }
        }

        float frostScale = GetActiveScriptGroupScale(player, "遗物_尸霜獠牙");
        if (frostScale > 0.0f && HasCrowdControlAuras(creature) && GetNow() > procState.lastFrostExplodeTime + 6)
        {
            if (CastManagedRelicSpell(player, 89141))
                procState.lastFrostExplodeTime = GetNow();
        }

        float oldGodScale = GetActiveScriptGroupScale(player, "遗物_古神耳蜕");
        if (oldGodScale > 0.0f && trackedMaskBit != 0 && GetNow() > procState.lastOldGodGiftTime + 20)
        {
            if (CastManagedRelicSpell(player, 89157))
                procState.lastOldGodGiftTime = GetNow();
        }

        float soulDevourScale = GetActiveSetSpecialEffectScale(player, "套装_腐焰噬魂");
        if (soulDevourScale > 0.0f && GetNow() > procState.lastSetSoulDevourTime + 2)
        {
            player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(5), soulDevourScale));
            RestorePlayerPrimaryPowerPct(player, 5.0f * soulDevourScale);
            procState.lastSetSoulDevourTime = GetNow();
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 腐焰噬魂触发，恢复生命与能量。|r");
        }
    }

    void HandlePlayerSpellCast(Player* player, Spell* spell)
    {
        if (!player || !spell)
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (!spellInfo)
            return;

        uint32 spellId = spellInfo->Id;
        if (IsManagedRelicSpell(spellId))
            return;

        uint32 guid = player->GetGUID().GetCounter();
        PlayerAbyssRunState const* runState = GetPlayerRunState(guid);
        PlayerAbyssData const* playerData = GetPlayerData(guid);
        if (!runState)
        {
            if (!HasAnyActiveRelicSlots(playerData) && !HasAnyActiveSetBonuses(player))
            {
                return;
            }
        }

        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(guid);
        if (procState.replayingSpell)
            return;

        if (procState.managedSpellExecutionDepth != 0)
        {
            LOG_DEBUG("module",
                "mod-abyss-cultivation: skip nested spell-cast proc for {} ({}) while managed relic execution depth is {} (spellId={})",
                player->GetName(), guid, procState.managedSpellExecutionDepth, spellId);
            return;
        }

        if (IsManagedRelicSpell(spellId))
            return;

        SpellSchoolMask schoolMask = spellInfo->GetSchoolMask();
        bool hasHealStyleEffect = false;
        bool hasDispelInterruptEffect = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            uint32 effect = spellInfo->Effects[i].Effect;
            switch (effect)
            {
                case SPELL_EFFECT_HEAL:
                case SPELL_EFFECT_HEAL_MAX_HEALTH:
                case SPELL_EFFECT_HEAL_MECHANICAL:
                case SPELL_EFFECT_HEAL_PCT:
                    hasHealStyleEffect = true;
                    break;
                case SPELL_EFFECT_INTERRUPT_CAST:
                case SPELL_EFFECT_DISPEL:
                case SPELL_EFFECT_DISPEL_MECHANIC:
                    hasDispelInterruptEffect = true;
                    break;
                default:
                    break;
            }
        }

        bool isCombatProcSource = IsEligibleAbyssCombatProcSource(player, spell);
        if (!isCombatProcSource && !hasHealStyleEffect && !hasDispelInterruptEffect)
            return;

        uint32 now = GetNow();
        if (isCombatProcSource)
        {
            procState.lastSpellId = spellId;
            procState.lastSpellCastTime = now;

            if (schoolMask & SPELL_SCHOOL_MASK_FIRE)
                procState.lastFireSchoolCastTime = now;
        }

        if (isCombatProcSource)
        if (Unit* target = GetPrimaryCombatTarget(player))
        {
            float emberEchoScale = GetActiveSetSpecialEffectScale(player, "套装_灰烬回响");
            if (emberEchoScale > 0.0f &&
                now > procState.lastSetEchoTime + 6 &&
                RollPercentage() < std::min(45.0f, 18.0f * emberEchoScale))
            {
                DealConfiguredBurst(player, target, 140, 210, SPELL_SCHOOL_MASK_FIRE, emberEchoScale);
                procState.lastSetEchoTime = now;
            }

            float artifactFlameScale = GetActiveSetSpecialEffectScale(player, "套装_焚界天爆");
            if (artifactFlameScale > 0.0f &&
                now > procState.lastSetDoubleBurstTime + 6 &&
                RollPercentage() < std::min(55.0f, 18.0f * artifactFlameScale))
            {
                SpellSchoolMask burstSchool = schoolMask != 0 ? schoolMask : SPELL_SCHOOL_MASK_FIRE;
                DealConfiguredBurst(player, target, 240, 340, burstSchool, artifactFlameScale);
                RestorePlayerPrimaryPowerPct(player, 3.0f * artifactFlameScale);
                procState.lastSetDoubleBurstTime = now;
            }

            float doubleBurstScale = GetActiveSetSpecialEffectScale(player, "套装_轮焰双爆");
            if (artifactFlameScale <= 0.0f &&
                doubleBurstScale > 0.0f &&
                now > procState.lastSetDoubleBurstTime + 8 &&
                RollPercentage() < std::min(45.0f, 15.0f * doubleBurstScale))
            {
                SpellSchoolMask burstSchool = schoolMask != 0 ? schoolMask : SPELL_SCHOOL_MASK_FIRE;
                DealConfiguredBurst(player, target, 180, 260, burstSchool, doubleBurstScale);
                procState.lastSetDoubleBurstTime = now;
            }

            float artifactFreezeScale = GetActiveSetSpecialEffectScale(player, "套装_永冻裁决");
            if (artifactFreezeScale > 0.0f &&
                now > procState.lastSetFreezeTime + 6 &&
                RollPercentage() < std::min(60.0f, 20.0f * artifactFreezeScale))
            {
                DealConfiguredBurst(player, target, 230, 330, SPELL_SCHOOL_MASK_FROST, artifactFreezeScale);
                procState.lastSetFreezeTime = now;
            }

            float frostSealScale = std::max(
                GetActiveSetSpecialEffectScale(player, "套装_寒夜冻结"),
                GetActiveSetSpecialEffectScale(player, "套装_玄霜封印"));
            if (artifactFreezeScale <= 0.0f &&
                frostSealScale > 0.0f &&
                now > procState.lastSetFreezeTime + 8 &&
                RollPercentage() < std::min(50.0f, 18.0f * frostSealScale))
            {
                DealConfiguredBurst(player, target, 150, 230, SPELL_SCHOOL_MASK_FROST, frostSealScale);
                procState.lastSetFreezeTime = now;
            }

            float artifactAbyssScale = GetActiveSetSpecialEffectScale(player, "套装_渊神吞界");
            if (artifactAbyssScale > 0.0f &&
                now > procState.lastSetAbyssDrainTime + 6 &&
                RollPercentage() < std::min(50.0f, 18.0f * artifactAbyssScale))
            {
                DealConfiguredBurst(player, target, 240, 360, SPELL_SCHOOL_MASK_SHADOW, artifactAbyssScale);
                player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(6), artifactAbyssScale));
                RestorePlayerPrimaryPowerPct(player, 4.0f * artifactAbyssScale);
                procState.lastSetAbyssDrainTime = now;
            }

            float abyssDrainScale = GetActiveSetSpecialEffectScale(player, "套装_神蚀吸收");
            if (artifactAbyssScale <= 0.0f &&
                abyssDrainScale > 0.0f &&
                now > procState.lastSetAbyssDrainTime + 8 &&
                RollPercentage() < std::min(40.0f, 15.0f * abyssDrainScale))
            {
                DealConfiguredBurst(player, target, 180, 260, SPELL_SCHOOL_MASK_SHADOW, abyssDrainScale);
                player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(4), abyssDrainScale));
                procState.lastSetAbyssDrainTime = now;
            }

            float artifactConquestScale = GetActiveSetSpecialEffectScale(player, "套装_征魂统御");
            if (artifactConquestScale > 0.0f &&
                now > procState.lastSetSurgeTime + 8)
            {
                DealConfiguredBurst(player, target, 300, 430, SPELL_SCHOOL_MASK_ARCANE, artifactConquestScale);
                RestorePlayerPrimaryPowerPct(player, 5.0f * artifactConquestScale);
                procState.lastSetSurgeTime = now;
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 征魂统御触发。|r");
            }

            float conquestScale = GetActiveSetSpecialEffectScale(player, "套装_征魂爆发");
            if (artifactConquestScale <= 0.0f &&
                conquestScale > 0.0f &&
                now > procState.lastSetSurgeTime + 10)
            {
                DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_ARCANE, conquestScale);
                procState.lastSetSurgeTime = now;
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 征魂爆发触发。|r");
            }
        }

        float ancientPowerScale = GetActiveSetSpecialEffectScale(player, "套装_古神觉醒");
        if (isCombatProcSource &&
            ancientPowerScale > 0.0f &&
            now > procState.lastSetAncientPowerTime + 20 &&
            RollPercentage() < std::min(40.0f, 12.0f * ancientPowerScale))
        {
            procState.lastSetAncientPowerTime = now;
            procState.setAncientPowerEndTime = now + 10;
            player->UpdateAllStats();
            player->UpdateAllRatings();
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 古神觉醒触发，10秒内全属性提升。|r");
        }

        float chaseBladeScale = GetActiveScriptGroupScale(player, { "遗物_黑潮齿轮", "祝福_追命飞刃" });
        if (isCombatProcSource && chaseBladeScale > 0.0f)
        {
            CastManagedRelicSpell(player, 89103);
        }

        float chainLightningScale = GetActiveScriptGroupScale(player, { "遗物_爆线电枢", "祝福_雷暴连极" });
        if (isCombatProcSource && chainLightningScale > 0.0f)
        {
            if (GetNow() > procState.lastChainLightningTime + 2)
            {
                if (CastManagedRelicSpell(player, 89107))
                    procState.lastChainLightningTime = GetNow();
            }
        }

        float poisonScale = GetActiveScriptGroupScale(player, "遗物_蛇蜕古胆");
        if (isCombatProcSource && poisonScale > 0.0f)
        {
            ++procState.poisonCastCounter;
            if (procState.poisonCastCounter >= 3 && GetNow() > procState.lastPoisonBurstTime + 3)
            {
                procState.poisonCastCounter = 0;
                if (CastManagedRelicSpell(player, 89102))
                    procState.lastPoisonBurstTime = GetNow();
            }
        }

        float bloodFlameScale = GetActiveScriptGroupScale(player, { "遗物_血焰圣经", "祝福_日蚀焚城", "特效_血焰审判" });
        if (isCombatProcSource && bloodFlameScale > 0.0f)
        {
            if (procState.lastComboSpellId != spellId)
            {
                procState.lastComboSpellId = spellId;
                ++procState.bloodFlameComboCounter;
            }

            if (procState.bloodFlameComboCounter >= 4 && GetNow() > procState.lastBloodFlameTime + 5)
            {
                procState.bloodFlameComboCounter = 0;
                if (CastManagedRelicSpell(player, 89109))
                    procState.lastBloodFlameTime = GetNow();
            }
        }

        float battlePrayerScale = GetActiveScriptGroupScale(player, "遗物_维库战祷");
        if (isCombatProcSource && battlePrayerScale > 0.0f)
        {
            CastManagedRelicSpell(player, 89137);
        }

        float starfallScale = GetActiveScriptGroupScale(player, "遗物_星界虹膜");
        if (isCombatProcSource && starfallScale > 0.0f)
        {
            ++procState.starfallCounter;
            if (procState.starfallCounter >= 5 && GetNow() > procState.lastStarfallTime + 6)
            {
                procState.starfallCounter = 0;
                if (CastManagedRelicSpell(player, 89147))
                    procState.lastStarfallTime = GetNow();
            }
        }

        float tideScale = GetActiveScriptGroupScale(player, { "遗物_深海祈眼", "遗物_潮蛇王鳞" });
        if (isCombatProcSource && tideScale > 0.0f)
        {
            ++procState.tideCastCounter;
            if (procState.tideCastCounter >= 5 && GetNow() > procState.lastTideBurstTime + 4)
            {
                procState.tideCastCounter = 0;
                uint32 tideSpellId = GetPreferredManagedRelicSpell(player, { {950005, 89105}, {950061, 89161} });
                if (CastManagedRelicSpell(player, tideSpellId))
                    procState.lastTideBurstTime = GetNow();
            }
        }

        float bloomScale = GetActiveScriptGroupScale(player, "遗物_古树孢祖");
        if (isCombatProcSource && bloomScale > 0.0f)
        {
            ++procState.bloomCastCounter;
            if (procState.bloomCastCounter >= 4 && GetNow() > procState.lastBloomBurstTime + 5)
            {
                procState.bloomCastCounter = 0;
                if (CastManagedRelicSpell(player, 89118))
                    procState.lastBloomBurstTime = GetNow();
            }
        }

        float corruptBloomScale = GetActiveScriptGroupScale(player, "遗物_腐花心核");
        if (corruptBloomScale > 0.0f && hasHealStyleEffect && player->GetHealthPct() >= 85.0f && GetNow() > procState.lastCorruptBloomTime + 6)
        {
            if (CastManagedRelicSpell(player, 89113))
                procState.lastCorruptBloomTime = GetNow();
        }

        float mirrorScale = GetActiveScriptGroupScale(player, { "遗物_伊利影印", "遗物_映像碎镜" });
        if (isCombatProcSource && mirrorScale > 0.0f)
        {
            if (procState.lastSpellId != 0 && GetNow() > procState.lastMirrorEchoTime + 8)
            {
                uint32 mirrorSpellId = GetPreferredManagedRelicSpell(player, { {950064, 89164}, {950052, 89152} });
                if (CastManagedRelicSpell(player, mirrorSpellId))
                    procState.lastMirrorEchoTime = GetNow();
            }
        }

        float swarmScale = GetActiveScriptGroupScale(player, "遗物_虫群主脑");
        if (isCombatProcSource && swarmScale > 0.0f)
        {
            if (GetNow() > procState.lastSwarmBurstTime + 7)
            {
                if (CastManagedRelicSpell(player, 89156))
                    procState.lastSwarmBurstTime = GetNow();
            }
        }

        float dreamScale = GetActiveScriptGroupScale(player, "遗物_梦沼眼膜");
        if (isCombatProcSource && dreamScale > 0.0f)
        {
            if (GetNow() > procState.lastDreamBurstTime + 8)
            {
                if (CastManagedRelicSpell(player, 89114))
                    procState.lastDreamBurstTime = GetNow();
            }
        }

        float tidePrisonScale = GetActiveScriptGroupScale(player, "遗物_潮牢鳞灯");
        if (isCombatProcSource && tidePrisonScale > 0.0f)
        {
            if (Unit* target = GetPrimaryCombatTarget(player))
            {
                if (HasCrowdControlAuras(target) && GetNow() > procState.lastTidePrisonTime + 6)
                {
                    if (CastManagedRelicSpell(player, 89124))
                        procState.lastTidePrisonTime = GetNow();
                }
            }
        }

        float beastScale = GetActiveScriptGroupScale(player, "遗物_神噬断爪");
        if (isCombatProcSource && beastScale > 0.0f)
        {
            CastManagedRelicSpell(player, 89143);
        }

        float matrixScale = GetActiveScriptGroupScale(player, { "遗物_虚空矩阵", "祝福_双生元婴" });
        if (isCombatProcSource && matrixScale > 0.0f)
        {
            ++procState.matrixCastCounter;
            if (procState.matrixCastCounter >= 4 && procState.lastSpellId != 0 && GetNow() > procState.lastMatrixEchoTime + 10)
            {
                procState.matrixCastCounter = 0;
                if (CastManagedRelicSpell(player, 89162))
                    procState.lastMatrixEchoTime = GetNow();
            }
        }

        float armyScale = GetActiveScriptGroupScale(player, "遗物_破军碎牌");
        if (isCombatProcSource &&
            armyScale > 0.0f &&
            CountNearbyEnemies(player, 18.0f) >= 4 &&
            GetNow() > procState.lastArmyPressureTime + 5)
        {
            if (CastManagedRelicSpell(player, 89123))
                procState.lastArmyPressureTime = GetNow();
        }

        float arcaneAshScale = GetActiveScriptGroupScale(player, "遗物_法陵星匣");
        if (isCombatProcSource && arcaneAshScale > 0.0f)
        {
            ++procState.arcaneAshCastCounter;
            if (procState.arcaneAshCastCounter >= 4 && GetNow() > procState.lastArcaneAshTime + 8)
            {
                procState.arcaneAshCastCounter = 0;
                if (CastManagedRelicSpell(player, 89127))
                    procState.lastArcaneAshTime = GetNow();
            }
        }

        float steamScale = GetActiveScriptGroupScale(player, "遗物_压阀导芯");
        if (steamScale > 0.0f && hasDispelInterruptEffect && GetNow() > procState.lastSteamCoreTime + 6)
        {
            if (CastManagedRelicSpell(player, 89126))
                procState.lastSteamCoreTime = GetNow();
        }

        float fearScale = GetActiveScriptGroupScale(player, "遗物_惧影迷盘");
        if (isCombatProcSource && fearScale > 0.0f)
        {
            if (Unit* target = GetPrimaryCombatTarget(player))
            {
                if (target->HealthBelowPct(35) && GetNow() > procState.lastFearExecuteTime + 8)
                {
                    if (CastManagedRelicSpell(player, 89130))
                        procState.lastFearExecuteTime = GetNow();
                }
            }
        }

        float phaseScale = GetActiveScriptGroupScale(player, "遗物_相位动轮");
        if (isCombatProcSource && phaseScale > 0.0f && GetNow() > procState.lastPhaseShotTime + 4)
        {
            if (CastManagedRelicSpell(player, 89133))
                procState.lastPhaseShotTime = GetNow();
        }

        float vineScale = GetActiveScriptGroupScale(player, "遗物_星植胚囊");
        if (isCombatProcSource && vineScale > 0.0f)
        {
            ++procState.vineCastCounter;
            if (procState.vineCastCounter >= 5 && GetNow() > procState.lastVineBloomTime + 10)
            {
                procState.vineCastCounter = 0;
                if (CastManagedRelicSpell(player, 89134))
                    procState.lastVineBloomTime = GetNow();
            }
        }

        float sunScale = GetActiveScriptGroupScale(player, "遗物_逐日余晖");
        if (isCombatProcSource && sunScale > 0.0f)
        {
            if (procState.lastComboSpellId != spellId)
                ++procState.sunBurstComboCounter;

            if (procState.sunBurstComboCounter >= 3 && GetNow() > procState.lastSunBurstTime + 8)
            {
                procState.sunBurstComboCounter = 0;
                if (CastManagedRelicSpell(player, 89136))
                    procState.lastSunBurstTime = GetNow();
            }
        }

        float orbitalScale = GetActiveScriptGroupScale(player, "遗物_聚魔棱晶");
        if (isCombatProcSource && orbitalScale > 0.0f)
        {
            ++procState.orbitalCastCounter;
            if (procState.orbitalCastCounter >= 3 && GetNow() > procState.lastOrbitalMissileTime + 7)
            {
                procState.orbitalCastCounter = 0;
                if (CastManagedRelicSpell(player, 89138))
                    procState.lastOrbitalMissileTime = GetNow();
            }
        }

        float faceScale = GetActiveScriptGroupScale(player, "遗物_无面触冠");
        if (isCombatProcSource &&
            faceScale > 0.0f &&
            procState.lastSpellId != 0 &&
            GetNow() > procState.lastNoFaceEchoTime + 8)
        {
            if (RollPercentage() < 30.0f * std::max(faceScale, 0.25f))
            {
                if (CastManagedRelicSpell(player, 89140))
                    procState.lastNoFaceEchoTime = GetNow();
            }
        }

        float thunderScale = GetActiveScriptGroupScale(player, "遗物_雷祖导体");
        if (isCombatProcSource && thunderScale > 0.0f && GetNow() > procState.lastThunderAncestorTime + 4)
        {
            if (CastManagedRelicSpell(player, 89145))
                procState.lastThunderAncestorTime = GetNow();
        }

        float lavaCoreScale = GetActiveScriptGroupScale(player, "遗物_熔界核髓");
        if (isCombatProcSource &&
            lavaCoreScale > 0.0f &&
            (schoolMask & SPELL_SCHOOL_MASK_FIRE) &&
            GetNow() > procState.lastLavaCoreTime + 6)
        {
            if (CastManagedRelicSpell(player, 89153))
                procState.lastLavaCoreTime = GetNow();
        }

        float overloadScale = GetActiveScriptGroupScale(player, "遗物_畸变龙脊");
        if (isCombatProcSource && overloadScale > 0.0f)
        {
            ++procState.overloadCastCounter;
            if (procState.overloadCastCounter >= 4 && procState.lastSpellId != 0 && GetNow() > procState.lastOverloadTime + 10)
            {
                procState.overloadCastCounter = 0;
                if (CastManagedRelicSpell(player, 89155))
                    procState.lastOverloadTime = GetNow();
            }
        }

        float duskScale = GetActiveScriptGroupScale(player, "遗物_暮炎龙瞳");
        if (isCombatProcSource &&
            duskScale > 0.0f &&
            ((schoolMask & SPELL_SCHOOL_MASK_FIRE) || (schoolMask & SPELL_SCHOOL_MASK_SHADOW)))
        {
            ++procState.dragonBreathCastCounter;
            if (procState.dragonBreathCastCounter >= 3 && GetNow() > procState.lastDragonBreathTime + 8)
            {
                procState.dragonBreathCastCounter = 0;
                if (CastManagedRelicSpell(player, 89168))
                    procState.lastDragonBreathTime = GetNow();
            }
        }

        float hellPrisonScale = GetActiveScriptGroupScale(player, "遗物_深狱锁冠");
        if (isCombatProcSource && hellPrisonScale > 0.0f)
        {
            if (Unit* target = GetPrimaryCombatTarget(player))
            {
                if (HasCrowdControlAuras(target) && GetNow() > procState.lastHellPrisonTime + 8)
                {
                    if (CastManagedRelicSpell(player, 89160))
                        procState.lastHellPrisonTime = GetNow();
                }
            }
        }

        float laserScale = GetActiveScriptGroupScale(player, "遗物_蓝脉天轮");
        if (isCombatProcSource && laserScale > 0.0f)
        {
            ++procState.laserOrbitCastCounter;
            if (procState.laserOrbitCastCounter >= 4 && GetNow() > procState.lastLaserOrbitTime + 9)
            {
                procState.laserOrbitCastCounter = 0;
                if (CastManagedRelicSpell(player, 89169))
                    procState.lastLaserOrbitTime = GetNow();
            }
        }
    }

    void HandlePlayerUpdate(Player* player, uint32 diff)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        PlayerAbyssRunState const* runState = GetPlayerRunState(guid);
        PlayerAbyssData const* playerData = GetPlayerData(guid);
        if (!runState && !HasAnyActiveRelicSlots(playerData) && !HasAnyActiveSetBonuses(player))
            return;

        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(guid);
        uint32 now = GetNow();
        SyncSystemRelicState(player);

        if (procState.setAncientPowerEndTime != 0 && now > procState.setAncientPowerEndTime)
        {
            procState.setAncientPowerEndTime = 0;
            player->UpdateAllStats();
            player->UpdateAllRatings();
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff8080[套装] 古神觉醒已结束。|r");
        }

        if (!procState.hasTrackedPosition)
        {
            procState.trackedPosX = player->GetPositionX();
            procState.trackedPosY = player->GetPositionY();
            procState.trackedPosZ = player->GetPositionZ();
            procState.lastStillnessTime = now;
            procState.hasTrackedPosition = true;
        }
        else
        {
            float dx = player->GetPositionX() - procState.trackedPosX;
            float dy = player->GetPositionY() - procState.trackedPosY;
            float dz = player->GetPositionZ() - procState.trackedPosZ;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > 1.0f)
            {
                procState.trackedPosX = player->GetPositionX();
                procState.trackedPosY = player->GetPositionY();
                procState.trackedPosZ = player->GetPositionZ();
                procState.lastStillnessTime = now;
            }
        }

        float artifactEmberScale = GetActiveSetSpecialEffectScale(player, "套装_烬灭永燃");
        if (artifactEmberScale > 0.0f && player->IsInCombat() && now > procState.lastSetCombatGrowthTime + 4)
        {
            uint32 maxStacks = static_cast<uint32>(std::lround(14.0f * artifactEmberScale));
            if (procState.setCombatGrowthStacks < maxStacks)
            {
                ++procState.setCombatGrowthStacks;
                procState.lastSetCombatGrowthTime = now;
                player->UpdateAllStats();
                player->UpdateAllRatings();
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 烬灭永燃层数提升至 %u。|r", procState.setCombatGrowthStacks);
            }
        }

        float emberGrowthScale = GetActiveSetSpecialEffectScale(player, "套装_烬世叠加");
        if (artifactEmberScale <= 0.0f &&
            emberGrowthScale > 0.0f &&
            player->IsInCombat() &&
            now > procState.lastSetCombatGrowthTime + 5)
        {
            uint32 maxStacks = static_cast<uint32>(std::lround(10.0f * emberGrowthScale));
            if (procState.setCombatGrowthStacks < maxStacks)
            {
                ++procState.setCombatGrowthStacks;
                procState.lastSetCombatGrowthTime = now;
                player->UpdateAllStats();
                player->UpdateAllRatings();
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 烬世叠加层数提升至 %u。|r", procState.setCombatGrowthStacks);
            }
        }

        float judgmentScale = GetActiveSetSpecialEffectScale(player, "套装_终焉天罚");
        if (judgmentScale > 0.0f && player->IsInCombat() && now > procState.lastSetJudgmentTime + 30)
        {
            TriggerDominionBurst(player, procState.lastSetJudgmentTime, judgmentScale);
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 终焉天罚触发。|r");
        }

        float lowHealthScale = GetActiveScriptGroupScale(player, { "遗物_断罪枷锁", "特效_锁链爆裂", "遗物_圣陨判词" });
        if (lowHealthScale > 0.0f &&
            player->HealthBelowPct(35) && now > procState.lastLowHealthProcTime + 15)
        {
            if (CastManagedRelicSpell(player, 89106))
                procState.lastLowHealthProcTime = now;
        }

        float holyVerdictScale = GetActiveScriptGroupScale(player, { "遗物_圣陨判词", "祝福_断命法旨", "特效_圣陨终裁" });
        if (holyVerdictScale > 0.0f &&
            player->HealthBelowPct(25) && now > procState.lastHolyVerdictTime + 30)
        {
            if (CastManagedRelicSpell(player, 89171))
                procState.lastHolyVerdictTime = now;
        }

        float replayScale = GetActiveScriptGroupScale(player, { "遗物_泰坦偏轴", "祝福_逆时回响", "特效_时痕回放" });
        if (replayScale > 0.0f &&
            procState.lastSpellId != 0 && now > procState.lastSpellCastTime + 10 && now > procState.lastReplayTime + 10)
        {
            if (CastManagedRelicSpell(player, 89111))
                procState.lastReplayTime = now;
        }

        float bannerScale = GetActiveScriptGroupScale(player, "遗物_守望战旌");
        if (bannerScale > 0.0f && now > procState.lastBattleBannerTime + 20)
            if (CastManagedRelicSpell(player, 89163))
                procState.lastBattleBannerTime = now;

        float redJadeScale = GetActiveScriptGroupScale(player, "遗物_赤玉界针");
        if (redJadeScale > 0.0f && now > procState.lastRedJadeTime + 12)
            if (CastManagedRelicSpell(player, 89174))
                procState.lastRedJadeTime = now;

        float riftScale = GetActiveScriptGroupScale(player, "遗物_裂隙砂轮");
        if (riftScale > 0.0f && CountNearbyEnemies(player, 20.0f) >= 5 && now > procState.lastRiftRainTime + 15)
            if (CastManagedRelicSpell(player, 89132))
                procState.lastRiftRainTime = now;

        float memoryScale = GetActiveScriptGroupScale(player, "遗物_泰坦记忆核");
        if (memoryScale > 0.0f &&
            procState.lastSpellId != 0 &&
            now > procState.lastSpellCastTime + 5 &&
            now > procState.lastTitanMeteorTime + 15)
        {
            if (CastManagedRelicSpell(player, 89144))
                procState.lastTitanMeteorTime = now;
        }

        float rekindleScale = GetActiveScriptGroupScale(player, "遗物_复燃逆鳞");
        if (rekindleScale > 0.0f &&
            player->HealthBelowPct(40) &&
            now > procState.lastRekindleTime + 12)
        {
            if (CastManagedRelicSpell(player, 89172))
                procState.lastRekindleTime = now;
        }

        if (playerData)
        {
            if (playerData->phaseArtifact == ABYSS_PHASE_ARTIFACT_ICE_TIMEBOX_ITEM &&
                runState && runState->modeType >= 3 &&
                now > procState.lastPhaseArtifactTime + 20)
            {
                if (CastManagedRelicSpell(player, 89177))
                    procState.lastPhaseArtifactTime = now;
            }

            if (playerData->phaseArtifact == ABYSS_PHASE_ARTIFACT_ECLIPSE_KING_ITEM &&
                now > procState.lastPhaseArtifactTime + 25)
            {
                if (CastManagedRelicSpell(player, 89179))
                    procState.lastPhaseArtifactTime = now;
            }

            if (playerData->phaseArtifact == ABYSS_PHASE_ARTIFACT_SCOURGE_CHAPTER_ITEM &&
                now > procState.lastPhaseArtifactTime + 18)
            {
                if (CastManagedRelicSpell(player, 89180))
                    procState.lastPhaseArtifactTime = now;
            }

            if (playerData->ultimateArtifact == ABYSS_ULTIMATE_ARTIFACT_ABYSS_LORD_ITEM &&
                now > procState.lastUltimateArtifactTime + 45)
            {
                if (CastManagedRelicSpell(player, 89181))
                    procState.lastUltimateArtifactTime = now;
            }
        }

        float mountainScale = GetActiveScriptGroupScale(player, "遗物_碎山指节");
        if (mountainScale > 0.0f &&
            now > procState.lastStillnessTime + 3 &&
            now > procState.lastMountainBreakTime + 10)
        {
            if (CastManagedRelicSpell(player, 89159))
                procState.lastMountainBreakTime = now;
        }

        float dominionScale = GetActiveScriptGroupScale(player, { "遗物_霜王残印", "祝福_极霜粉碎", "特效_霜王统御" });
        if (dominionScale > 0.0f &&
            procState.dominionCounter >= 6 && now > procState.lastDominionTime + 18)
        {
            procState.dominionCounter = 0;
            if (CastManagedRelicSpell(player, 89173))
                procState.lastDominionTime = now;
        }

        (void)diff;
    }

    void UpdateActiveRunInstanceSignature(Player* player)
    {
        if (!player)
            return;

        auto itr = _playerRunStates.find(player->GetGUID().GetCounter());
        if (itr == _playerRunStates.end())
            return;

        uint32 instanceSignature = GetPlayerInstanceSignature(player, itr->second.currentMapId);
        if (instanceSignature != 0)
            itr->second.currentInstanceId = instanceSignature;
    }

    bool SuspendPlayerRun(Player* player)
    {
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = _playerRunStates.find(guid);
        if (itr == _playerRunStates.end())
            return false;

        _suspendedRunStates[guid] = itr->second;
        DeletePlayerRunState(guid);
        ClearPlayerProcState(guid);
        RefreshPlayerRuntimeStats(player);
        return true;
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

        auto suspendedItr = _suspendedRunStates.find(guid);
        if (suspendedItr != _suspendedRunStates.end())
        {
            uint32 instanceSignature = GetPlayerInstanceSignature(player, chapter->mapId);
            PlayerAbyssRunState suspendedState = suspendedItr->second;

            if (suspendedState.currentChapterId == chapter->chapterId &&
                suspendedState.currentMapId == chapter->mapId &&
                suspendedState.currentInstanceId != 0 &&
                suspendedState.currentInstanceId == instanceSignature)
            {
                suspendedState.currentInstanceId = instanceSignature;
                _playerRunStates[guid] = suspendedState;
                _suspendedRunStates.erase(suspendedItr);
                SavePlayerRunState(player);
                RefreshPlayerRuntimeStats(player);

                if (suspendedState.pendingAbyssModeType != 0)
                    ScheduleDelayedModeBossSummon(player, *chapter, _playerRunStates[guid], suspendedState.pendingAbyssModeType);
                if (suspendedState.pendingCacheSummon)
                    ScheduleDelayedCacheBossSummon(player, *chapter, _playerRunStates[guid]);

                if (player->GetSession())
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("Abyss run restored for chapter {} ({}) in instance {}.",
                        chapter->chapterId,
                        chapter->chapterName,
                        instanceSignature);
                }

                return true;
            }

            _suspendedRunStates.erase(suspendedItr);
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

    int64 GetRelicAttributeContribution(AbyssRelicConfig const& relic, uint8 slot, RelicRuntimeAttribute attribute, float modeScale) const
    {
        uint32 totalWeight = GetRelicWeightTotal(relic);
        uint32 weight = GetRelicAttributeWeight(relic, attribute);
        if (totalWeight == 0 || weight == 0)
            return 0;

        float slotScale = GetRelicSlotScale(relic, slot);
        if (slotScale <= 0.0f)
            return 0;

        long double budget = static_cast<long double>(GetRelicBaseBudget(relic)) * static_cast<long double>(slotScale) * static_cast<long double>(std::max(modeScale, 0.1f));
        long double value = budget * (static_cast<long double>(weight) / static_cast<long double>(totalWeight));
        if (value >= static_cast<long double>(std::numeric_limits<int64>::max()))
            return std::numeric_limits<int64>::max();

        return static_cast<int64>(std::llround(value));
    }

    int64 GetPlayerRuntimeRelicAttributeBonus(Player* player, RelicRuntimeAttribute attribute) const
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
        int64 total = 0;
        for (auto const& slotEntry : slots)
        {
            if (slotEntry.second == 0)
                continue;

            AbyssRelicConfig const* relic = GetRelicConfig(slotEntry.second);
            if (!relic)
                continue;

            total = AddInt64Saturated(total, GetRelicAttributeContribution(*relic, slotEntry.first, attribute, modeScale));
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
            case STAT_AGILITY: return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_AGILITY));
            case STAT_STRENGTH: return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_STRENGTH));
            case STAT_INTELLECT: return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_INTELLECT));
            case STAT_SPIRIT: return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_SPIRIT));
            case STAT_STAMINA: return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_STAMINA));
            default: return 0;
        }
    }

    int64 GetPlayerRuntimeRelicRatingBonus(Player* player, CombatRating cr) const
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
        return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_ATTACK_POWER));
    }

    int32 GetPlayerRuntimeRelicSpellPowerBonus(Player* player) const
    {
        return ToInt32Saturated(GetPlayerRuntimeRelicAttributeBonus(player, RELIC_ATTR_SPELL_POWER));
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

        // 套装属性与件数判定已经迁移到 ItemSet.dbc + Spell.dbc。
        // 这里不再手动改玩家属性，只清理旧的运行时缓存，
        // 特殊效果统一改为按原生套装 aura 判定。
        _playerSetBonusStates.erase(player->GetGUID().GetCounter());
    }

    void CleanupPlayerSetBonuses(uint32 guid)
    {
        _playerSetBonusStates.erase(guid);
    }

    bool HasAnyActiveSetBonuses(Player* player) const
    {
        if (!player)
            return false;

        for (auto const& [setId, config] : _setBonusConfigs)
        {
            (void)setId;
            if (!config.enabled)
                continue;

            AbyssNativeSetSpellIds ids = GetAbyssNativeSetSpellIds(config.actId, config.sourceMode);
            if ((ids.twoPieceMain && player->HasAura(ids.twoPieceMain))
                || (ids.fourPieceVisible && player->HasAura(ids.fourPieceVisible))
                || (ids.fourPieceHelper && player->HasAura(ids.fourPieceHelper))
                || (ids.sixPieceVisible && player->HasAura(ids.sixPieceVisible))
                || (ids.sixPieceHelper && player->HasAura(ids.sixPieceHelper))
                || (ids.eightPieceVisible && player->HasAura(ids.eightPieceVisible))
                || (ids.eightPieceHelper && player->HasAura(ids.eightPieceHelper)))
            {
                return true;
            }
        }

        return false;
    }

    float GetActiveSetSpecialEffectScale(Player* player, std::string const& specialEffect) const
    {
        if (!player || specialEffect.empty())
            return 0.0f;

        float bestScale = 0.0f;
        for (auto const& [setId, config] : _setBonusConfigs)
        {
            (void)setId;
            if (!config.enabled)
                continue;

            AbyssNativeSetSpellIds ids = GetAbyssNativeSetSpellIds(config.actId, config.sourceMode);
            float scale = 1.0f;
            bool matched = false;
            if (ids.eightPieceHelper != 0 && player->HasAura(ids.eightPieceHelper) && config.eightPieceSpecialEffect == specialEffect)
            {
                scale = 1.15f;
                matched = true;
            }
            else if (ids.sixPieceHelper != 0 && player->HasAura(ids.sixPieceHelper) && config.sixPieceSpecialEffect == specialEffect)
            {
                scale = 1.00f;
                matched = true;
            }
            else if (ids.fourPieceHelper != 0 && player->HasAura(ids.fourPieceHelper) && config.fourPieceSpecialEffect == specialEffect)
            {
                scale = 0.85f;
                matched = true;
            }

            if (!matched)
                continue;

            switch (config.sourceMode)
            {
                case 3: scale += 0.05f; break;
                case 4: scale += 0.10f; break;
                case 5: scale += 0.18f; break;
                default: break;
            }

            bestScale = std::max(bestScale, scale);
        }

        return bestScale;
    }

    bool HasActiveSetSpecialEffect(Player* player, std::string const& specialEffect) const
    {
        return GetActiveSetSpecialEffectScale(player, specialEffect) > 0.0f;
    }

    void RestorePlayerPrimaryPowerPct(Player* player, float pct) const
    {
        if (!player || pct <= 0.0f)
            return;

        Powers powerType = player->getPowerType();
        if (powerType == POWER_HEALTH)
            return;

        uint64 maxPower = player->GetMaxPowerForCombat(powerType);
        if (maxPower == 0)
            return;

        int64 addPower = ScaleUInt64ToInt64(maxPower, static_cast<long double>(pct) / 100.0L);
        if (addPower <= 0)
            return;

        player->ModifyPower64(powerType, addPower);
    }

    bool HandlePlayerSetDeathProtection(Player* player)
    {
        if (!player)
            return false;

        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        uint32 now = GetNow();

        float artifactRebirthScale = GetActiveSetSpecialEffectScale(player, "套装_归墟主宰");
        if (artifactRebirthScale > 0.0f && now > procState.lastSetRebirthTime + 240)
        {
            procState.lastSetRebirthTime = now;
            player->ResurrectPlayer(1.0f, false);
            player->SetExtendedHealth(player->GetMaxHealthForCombat());
            player->SyncClientHealthFromExtended();
            RestorePlayerPrimaryPowerPct(player, 100.0f);
            player->UpdateAllStats();
            player->UpdateAllRatings();
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 归墟主宰触发，已强制复苏。|r");
            return true;
        }

        float rebirthScale = GetActiveSetSpecialEffectScale(player, "套装_归墟不灭");
        if (artifactRebirthScale <= 0.0f &&
            rebirthScale > 0.0f &&
            now > procState.lastSetRebirthTime + 300)
        {
            procState.lastSetRebirthTime = now;
            player->ResurrectPlayer(1.0f, false);
            player->SetExtendedHealth(player->GetMaxHealthForCombat());
            player->SyncClientHealthFromExtended();
            RestorePlayerPrimaryPowerPct(player, 100.0f);
            player->UpdateAllStats();
            player->UpdateAllRatings();
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 归墟不灭触发，已强制复苏。|r");
            return true;
        }

        float bloodOathScale = GetActiveSetSpecialEffectScale(player, "套装_血誓不灭");
        if (bloodOathScale > 0.0f && now > procState.lastSetDeathWardTime + 60)
        {
            float chance = std::min(60.0f, 30.0f * bloodOathScale);
            if (RollPercentage() < chance)
            {
                procState.lastSetDeathWardTime = now;
                player->ResurrectPlayer(0.0f, false);
                player->SetExtendedHealth(std::max<uint64>(1u, player->CountPctFromMaxHealth(20)));
                player->SyncClientHealthFromExtended();
                RestorePlayerPrimaryPowerPct(player, 20.0f);
                player->UpdateAllStats();
                player->UpdateAllRatings();
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 血誓不灭触发，已免死并恢复生命。|r");
                return true;
            }
        }

        return false;
    }

    AbyssSetBonusConfig const* GetSetBonusConfig(uint32 setId) const
    {
        auto itr = _setBonusConfigs.find(setId);
        return itr != _setBonusConfigs.end() ? &itr->second : nullptr;
    }

private:
    void ApplySetDamagePctBonus(Player* player, uint8 damagePct, bool apply)
    {
        if (!player || damagePct == 0)
            return;

        float pctValue = float(damagePct);
        player->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, pctValue, apply);
        player->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, pctValue, apply);
        player->HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, pctValue, apply);
        player->ApplyPercentModFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT, pctValue, apply);
    }

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
                player->ApplySpellPowerBonus(config->twoPieceSP, true);

            ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[套装] 已激活 %s (2件)：%s|r",
                config->setName.c_str(), config->twoPieceDesc.c_str());
        }
        else if (tier == 4)
        {
            // 4件效果：加属性 + 特殊效果标记
            if (config->fourPieceDmgPct > 0)
                ApplySetDamagePctBonus(player, config->fourPieceDmgPct, true);
            if (config->fourPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->fourPieceCrit, true);
            if (config->fourPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->fourPieceHaste, true);
            if (config->fourPieceHpPct > 0)
            {
                float bonus = ScaleUInt64ToFloat(player->GetMaxHealthForCombat(), static_cast<long double>(config->fourPieceHpPct) / 100.0L);
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, true);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000[套装] 已激活 %s (4件)：%s|r",
                config->setName.c_str(), config->fourPieceDesc.c_str());
        }
        else if (tier == 6)
        {
            if (config->sixPieceDmgPct > 0)
                ApplySetDamagePctBonus(player, config->sixPieceDmgPct, true);
            if (config->sixPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->sixPieceCrit, true);
            if (config->sixPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->sixPieceHaste, true);
            if (config->sixPieceHpPct > 0)
            {
                float bonus = ScaleUInt64ToFloat(player->GetMaxHealthForCombat(), static_cast<long double>(config->sixPieceHpPct) / 100.0L);
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, true);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cff66ccff[套装] 已激活 %s (6件)：%s|r",
                config->setName.c_str(), config->sixPieceDesc.c_str());
        }
        else if (tier == 8)
        {
            if (config->eightPieceDmgPct > 0)
                ApplySetDamagePctBonus(player, config->eightPieceDmgPct, true);
            if (config->eightPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->eightPieceCrit, true);
            if (config->eightPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->eightPieceHaste, true);
            if (config->eightPieceHpPct > 0)
            {
                float bonus = ScaleUInt64ToFloat(player->GetMaxHealthForCombat(), static_cast<long double>(config->eightPieceHpPct) / 100.0L);
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, true);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffcc66ff[套装] 已激活 %s (8件)：%s|r",
                config->setName.c_str(), config->eightPieceDesc.c_str());
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
                player->ApplySpellPowerBonus(config->twoPieceSP, false);

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[套装] 已失去 %s (2件) 效果|r",
                config->setName.c_str());
        }
        else if (tier == 4)
        {
            if (config->fourPieceDmgPct > 0)
                ApplySetDamagePctBonus(player, config->fourPieceDmgPct, false);
            if (config->fourPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->fourPieceCrit, false);
            if (config->fourPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->fourPieceHaste, false);
            if (config->fourPieceHpPct > 0)
            {
                float bonus = ScaleUInt64ToFloat(player->GetMaxHealthForCombat(), static_cast<long double>(config->fourPieceHpPct) / 100.0L);
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, false);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[套装] 已失去 %s (4件) 效果|r",
                config->setName.c_str());
        }
        else if (tier == 6)
        {
            if (config->sixPieceDmgPct > 0)
                ApplySetDamagePctBonus(player, config->sixPieceDmgPct, false);
            if (config->sixPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->sixPieceCrit, false);
            if (config->sixPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->sixPieceHaste, false);
            if (config->sixPieceHpPct > 0)
            {
                float bonus = ScaleUInt64ToFloat(player->GetMaxHealthForCombat(), static_cast<long double>(config->sixPieceHpPct) / 100.0L);
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, false);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[套装] 已失去 %s (6件) 效果|r",
                config->setName.c_str());
        }
        else if (tier == 8)
        {
            if (config->eightPieceDmgPct > 0)
                ApplySetDamagePctBonus(player, config->eightPieceDmgPct, false);
            if (config->eightPieceCrit > 0)
                player->ApplyRatingMod(CR_CRIT_MELEE, config->eightPieceCrit, false);
            if (config->eightPieceHaste > 0)
                player->ApplyRatingMod(CR_HASTE_MELEE, config->eightPieceHaste, false);
            if (config->eightPieceHpPct > 0)
            {
                float bonus = ScaleUInt64ToFloat(player->GetMaxHealthForCombat(), static_cast<long double>(config->eightPieceHpPct) / 100.0L);
                player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, bonus, false);
            }

            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[套装] 已失去 %s (8件) 效果|r",
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

    PlayerAbyssRunState const* GetActiveRunStateForBoss(Creature* creature, uint16 chapterId) const
    {
        if (!creature || chapterId == 0)
            return nullptr;

        auto matchesRunState = [&](PlayerAbyssRunState const* runState) -> bool
        {
            return runState &&
                runState->currentChapterId == chapterId &&
                runState->modeType >= 1 &&
                runState->currentMapId == creature->GetMapId();
        };

        if (creature->IsSummon())
        {
            if (TempSummon* summon = creature->ToTempSummon())
            {
                if (Unit* summoner = summon->GetSummonerUnit())
                {
                    if (Player* player = summoner->ToPlayer())
                    {
                        if (PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter()))
                            if (matchesRunState(runState))
                                return runState;
                    }
                }
            }
        }

        Map* map = creature->GetMap();
        if (!map)
            return nullptr;

        for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player || !player->IsInWorld() || !player->IsAlive())
                continue;

            if (player->GetMapId() != creature->GetMapId())
                continue;

            if (player->GetDistance(creature) > 200.0f)
                continue;

            if (PlayerAbyssRunState const* runState = GetPlayerRunState(player->GetGUID().GetCounter()))
                if (matchesRunState(runState))
                    return runState;
        }

        return nullptr;
    }

    PlayerAbyssRunState const* GetActiveRunStateForTrackedBoss(Creature* creature, AbyssChapterConfig const*& chapterConfig) const
    {
        chapterConfig = nullptr;
        if (!creature)
            return nullptr;

        auto tryMatchRunState = [&](PlayerAbyssRunState const* runState) -> PlayerAbyssRunState const*
        {
            if (!runState || runState->currentChapterId == 0 || runState->modeType == 0)
                return nullptr;

            AbyssChapterConfig const* candidateChapter = GetChapterConfig(runState->currentChapterId);
            if (!candidateChapter)
                return nullptr;

            if (runState->currentMapId != creature->GetMapId())
                return nullptr;

            uint32 creatureEntry = creature->GetEntry();
            if (creatureEntry != candidateChapter->abyssBossEntry &&
                creatureEntry != candidateChapter->cacheBossEntry)
            {
                return nullptr;
            }

            chapterConfig = candidateChapter;
            return runState;
        };

        if (creature->IsSummon())
        {
            if (TempSummon* summon = creature->ToTempSummon())
            {
                if (Unit* summoner = summon->GetSummonerUnit())
                {
                    if (Player* player = summoner->ToPlayer())
                    {
                        if (PlayerAbyssRunState const* runState = tryMatchRunState(GetPlayerRunState(player->GetGUID().GetCounter())))
                            return runState;
                    }
                }
            }
        }

        Map* map = creature->GetMap();
        if (!map)
            return nullptr;

        for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player || !player->IsInWorld() || !player->IsAlive())
                continue;

            if (player->GetMapId() != creature->GetMapId())
                continue;

            if (player->GetDistance(creature) > 200.0f)
                continue;

            if (PlayerAbyssRunState const* runState = tryMatchRunState(GetPlayerRunState(player->GetGUID().GetCounter())))
                return runState;
        }

        return nullptr;
    }

    bool SyncTrackedBossLootMode(Creature* creature) const
    {
        if (!creature)
            return false;

        AbyssChapterConfig const* chapterConfig = nullptr;
        PlayerAbyssRunState const* activeRunState = GetActiveRunStateForTrackedBoss(creature, chapterConfig);
        if (!activeRunState || !chapterConfig)
            return false;

        uint16 abyssLootMode = GetAbyssLootModeMask(activeRunState->modeType);
        if (abyssLootMode == 0)
            return false;

        if (creature->GetLootMode() != abyssLootMode)
            creature->SetLootMode(abyssLootMode);

        return true;
    }

    bool ApplyBossRuntimeTuning(Creature* creature) const
    {
        if (!creature)
            return false;

        AbyssBossConfig const* bossConfig = GetBossConfig(creature->GetEntry());
        if (!bossConfig)
            return false;

        PlayerAbyssRunState const* activeRunState = GetActiveRunStateForBoss(creature, bossConfig->chapterId);
        if (!activeRunState)
            return false;

        uint8 modeType = activeRunState->modeType;
        float modeModifier = GetBossModeModifier(*bossConfig, modeType);
        uint16 corruptionTier = modeType == 3 ? activeRunState->corruptionTier : 0;

        if (bossConfig->bossType == 1)
        {
            std::string displayName = GetBossDisplayNameForMode(creature->GetEntry(), modeType);
            if (!displayName.empty() && creature->GetName() != displayName)
                creature->SetName(displayName);
        }

        float healthScale = std::max(1.0f, bossConfig->healthModifier * modeModifier * (1.0f + static_cast<float>(corruptionTier) * 0.03f));
        float scale = 1.0f + ((healthScale - 1.0f) * 0.10f);
        scale = std::min(std::max(scale, 1.0f), 2.0f);

        uint64 baseHealth = creature->GetMaxHealthForCombat();
        if (baseHealth > 0)
        {
            uint64 tunedHealth = ScaleUInt64ToUInt64(baseHealth, static_cast<long double>(healthScale));
            tunedHealth = std::max<uint64>(tunedHealth, baseHealth);
            uint32 clientHealth = ToAbyssClientHealth(tunedHealth);
            creature->SetMaxHealth(clientHealth);
            if (tunedHealth > clientHealth)
            {
                creature->SetExtendedMaxHealth(tunedHealth);
                creature->SetExtendedHealth(tunedHealth);
                creature->SyncClientHealthFromExtended();
            }
            else
                creature->SetHealth(clientHealth);
        }

        creature->SetObjectScale(scale);
        return true;
    }

    uint64 GetTrackedBossMaskBit(AbyssChapterConfig const& chapter, uint32 creatureEntry) const
    {
        if (creatureEntry == 0)
            return 0;

        if (creatureEntry == chapter.anchorBossEntry)
            return kTrackedBossMaskOfficialAnchor;

        if (creatureEntry == chapter.finalBossEntry && creatureEntry != chapter.anchorBossEntry)
            return kTrackedBossMaskOfficialFinal;

        return 0;
    }

    uint64 GetStageBossKillMaskBitForMode(uint8 modeType) const
    {
        switch (modeType)
        {
            case 1: return kTrackedBossMaskStoryBoss;
            case 2: return kTrackedBossMaskAbyssBoss;
            case 3: return kTrackedBossMaskCorruptionBoss;
            case 4: return kTrackedBossMaskReincarnationBoss;
            default: return 0;
        }
    }

    uint64 GetRuntimeBossProgressMaskBit(AbyssChapterConfig const& chapter, PlayerAbyssRunState const& state, uint32 creatureEntry) const
    {
        uint64 trackedMaskBit = GetTrackedBossMaskBit(chapter, creatureEntry);
        if (trackedMaskBit != 0)
            return trackedMaskBit;

        if (creatureEntry == chapter.abyssBossEntry)
            return GetStageBossKillMaskBitForMode(state.modeType);

        if (creatureEntry == chapter.cacheBossEntry)
            return kTrackedBossMaskCacheBoss;

        return 0;
    }

    bool IsSequentialModeChainActive(PlayerAbyssRunState const& state) const
    {
        switch (state.modeType)
        {
            case 2: return (state.anchorBossKillMask & kTrackedBossMaskStoryBoss) != 0;
            case 3: return (state.anchorBossKillMask & kTrackedBossMaskAbyssBoss) != 0;
            case 4: return (state.anchorBossKillMask & kTrackedBossMaskCorruptionBoss) != 0;
            default: return false;
        }
    }

    bool ShouldUseSequentialModeChain(PlayerAbyssRunState const& state) const
    {
        return state.modeType == 1 || IsSequentialModeChainActive(state);
    }

    uint8 GetNextSequentialModeType(PlayerAbyssRunState const& state) const
    {
        if ((state.anchorBossKillMask & (kTrackedBossMaskOfficialAnchor | kTrackedBossMaskOfficialFinal)) != 0 &&
            (state.anchorBossKillMask & kTrackedBossMaskStoryBoss) == 0)
        {
            return 1;
        }

        if ((state.anchorBossKillMask & kTrackedBossMaskStoryBoss) != 0 &&
            (state.anchorBossKillMask & kTrackedBossMaskAbyssBoss) == 0)
        {
            return 2;
        }

        if ((state.anchorBossKillMask & kTrackedBossMaskAbyssBoss) != 0 &&
            (state.anchorBossKillMask & kTrackedBossMaskCorruptionBoss) == 0)
        {
            return 3;
        }

        if ((state.anchorBossKillMask & kTrackedBossMaskCorruptionBoss) != 0 &&
            (state.anchorBossKillMask & kTrackedBossMaskReincarnationBoss) == 0)
        {
            return 4;
        }

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

    bool ScheduleDelayedModeBossSummon(Player* player, AbyssChapterConfig const& chapter, PlayerAbyssRunState& state, uint8 summonModeType)
    {
        if (!player || chapter.abyssBossEntry == 0 || summonModeType == 0)
            return false;

        UpdateActiveRunInstanceSignature(player);
        state.pendingAbyssModeType = summonModeType;

        ObjectGuid playerGuid = player->GetGUID();
        uint16 chapterId = chapter.chapterId;
        uint32 expectedInstanceId = state.currentInstanceId;

        player->m_Events.AddEventAtOffset([this, playerGuid, chapterId, summonModeType, expectedInstanceId]()
        {
            ExecuteDelayedModeBossSummon(playerGuid, chapterId, summonModeType, expectedInstanceId);
        }, 5s);

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("将在 5 秒后自动召唤{}。", GetBossDisplayNameForMode(chapter.abyssBossEntry, summonModeType));

        return true;
    }

    bool ScheduleDelayedCacheBossSummon(Player* player, AbyssChapterConfig const& chapter, PlayerAbyssRunState& state)
    {
        if (!player || chapter.cacheBossEntry == 0)
            return false;

        UpdateActiveRunInstanceSignature(player);
        state.pendingCacheSummon = true;

        ObjectGuid playerGuid = player->GetGUID();
        uint16 chapterId = chapter.chapterId;
        uint32 expectedInstanceId = state.currentInstanceId;

        player->m_Events.AddEventAtOffset([this, playerGuid, chapterId, expectedInstanceId]()
        {
            ExecuteDelayedCacheBossSummon(playerGuid, chapterId, expectedInstanceId);
        }, 5s);

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("将在 5 秒后自动召唤{}。", GetBossDisplayName(chapter.cacheBossEntry));
        return true;
    }

    void ExecuteDelayedModeBossSummon(ObjectGuid playerGuid, uint16 chapterId, uint8 summonModeType, uint32 expectedInstanceId)
    {
        Player* player = ObjectAccessor::FindPlayer(playerGuid);
        if (!player || !player->IsInWorld())
            return;

        uint32 guid = playerGuid.GetCounter();
        auto runItr = _playerRunStates.find(guid);
        auto playerItr = _playerData.find(guid);
        if (runItr == _playerRunStates.end() || playerItr == _playerData.end())
            return;

        PlayerAbyssRunState& state = runItr->second;
        if (state.currentChapterId != chapterId || state.pendingAbyssModeType != summonModeType)
            return;

        if (state.currentMapId != 0 && player->GetMapId() != state.currentMapId)
            return;

        uint32 instanceSignature = GetPlayerInstanceSignature(player, state.currentMapId);
        if (expectedInstanceId != 0 && instanceSignature != 0 && expectedInstanceId != instanceSignature)
            return;

        AbyssChapterConfig const* chapter = GetChapterConfig(chapterId);
        if (!chapter)
        {
            state.pendingAbyssModeType = 0;
            return;
        }

        PlayerAbyssData& data = playerItr->second;
        state.pendingAbyssModeType = 0;

        uint8 previousModeType = state.modeType;
        uint16 previousCorruptionTier = state.corruptionTier;

        state.modeType = summonModeType;
        state.corruptionTier = summonModeType == 3 ? std::max<uint16>(state.corruptionTier, data.highestCorruptionTier) : 0;
        NormalizePlayerRunState(state, &data);

        if (!TrySummonBoss(player, *chapter, chapter->abyssBossEntry, true))
        {
            state.modeType = previousModeType;
            state.corruptionTier = previousCorruptionTier;
            NormalizePlayerRunState(state, &data);
            SavePlayerRunState(player);
            return;
        }

        state.abyssBossSummoned = true;
        state.currentInstanceId = instanceSignature != 0 ? instanceSignature : state.currentInstanceId;
        SavePlayerRunState(player);

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("已自动召唤{}。", GetBossDisplayNameForMode(chapter->abyssBossEntry, summonModeType));

        SendAbyssStateToAddon(player);
    }

    void ExecuteDelayedCacheBossSummon(ObjectGuid playerGuid, uint16 chapterId, uint32 expectedInstanceId)
    {
        Player* player = ObjectAccessor::FindPlayer(playerGuid);
        if (!player || !player->IsInWorld())
            return;

        uint32 guid = playerGuid.GetCounter();
        auto runItr = _playerRunStates.find(guid);
        auto playerItr = _playerData.find(guid);
        if (runItr == _playerRunStates.end() || playerItr == _playerData.end())
            return;

        PlayerAbyssRunState& state = runItr->second;
        if (state.currentChapterId != chapterId || !state.pendingCacheSummon)
            return;

        if (state.currentMapId != 0 && player->GetMapId() != state.currentMapId)
            return;

        uint32 instanceSignature = GetPlayerInstanceSignature(player, state.currentMapId);
        if (expectedInstanceId != 0 && instanceSignature != 0 && expectedInstanceId != instanceSignature)
            return;

        AbyssChapterConfig const* chapter = GetChapterConfig(chapterId);
        if (!chapter)
        {
            state.pendingCacheSummon = false;
            return;
        }

        PlayerAbyssData& data = playerItr->second;
        state.pendingCacheSummon = false;

        if (!TrySummonBoss(player, *chapter, chapter->cacheBossEntry, false))
        {
            SavePlayerRunState(player);
            return;
        }

        state.cacheBossSummoned = true;
        data.cacheBossFailCount = 0;
        state.currentInstanceId = instanceSignature != 0 ? instanceSignature : state.currentInstanceId;
        SavePlayerData(player);
        SavePlayerRunState(player);

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage("已自动召唤{}。", GetBossDisplayName(chapter->cacheBossEntry));

        SendAbyssStateToAddon(player);
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

    bool TryUnlockStageArtifacts(uint32 guid, PlayerAbyssData& /*data*/)
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

            AbyssTaskDocking const* officialTask = GetTaskDockingForChapter(chapter.chapterId, 2);
            if (officialTask)
            {
                QuestStatus status = player->GetQuestStatus(officialTask->questId);
                bool rewarded = player->GetQuestRewardStatus(officialTask->questId);
                if (rewarded || status == QUEST_STATUS_COMPLETE)
                {
                    if (UnlockChapterMode(player, chapter.chapterId, 1))
                        changed = true;
                }
            }

            AbyssTaskDocking const* storyTask = GetTaskDockingForChapter(chapter.chapterId, 3);
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

            AbyssTaskDocking const* abyssTask = GetTaskDockingForChapter(chapter.chapterId, 4);
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

        if (chapterId == 0 || (_suspendedRunStates.find(guid) != _suspendedRunStates.end() && _suspendedRunStates[guid].currentChapterId != chapterId))
            ClearPlayerSuspendedRunState(guid);

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

    bool TryReuseExistingRunAndTeleport(Player* player, uint16 chapterId, std::string* failureReason)
    {
        if (failureReason)
            failureReason->clear();

        if (!player || chapterId == 0)
            return false;

        AbyssChapterConfig const* chapterConfig = GetChapterConfig(chapterId);
        if (!chapterConfig)
        {
            if (failureReason)
                *failureReason = "chapter_not_found";
            return false;
        }

        uint32 guid = player->GetGUID().GetCounter();
        auto runItr = _playerRunStates.find(guid);
        if (runItr != _playerRunStates.end() && runItr->second.currentChapterId == chapterId)
        {
            if (player->GetMapId() == chapterConfig->mapId)
            {
                UpdateActiveRunInstanceSignature(player);
                SavePlayerRunState(player);
                return true;
            }

            std::string teleportFailureReason;
            if (TryTeleportPlayerToChapter(player, *chapterConfig, &teleportFailureReason))
                return true;

            if (failureReason)
                *failureReason = teleportFailureReason.empty() ? "teleport_failed" : teleportFailureReason;
            return false;
        }

        auto suspendedItr = _suspendedRunStates.find(guid);
        if (suspendedItr != _suspendedRunStates.end() && suspendedItr->second.currentChapterId == chapterId)
        {
            if (player->GetMapId() == chapterConfig->mapId)
                return true;

            std::string teleportFailureReason;
            if (TryTeleportPlayerToChapter(player, *chapterConfig, &teleportFailureReason))
                return true;

            if (failureReason)
                *failureReason = teleportFailureReason.empty() ? "teleport_failed" : teleportFailureReason;
            return false;
        }

        return false;
    }

    bool BeginPlayerRunAndTeleport(Player* player, uint16 chapterId, uint8 modeType, uint16 corruptionTier, std::string* failureReason)
    {
        std::string reuseFailureReason;
        if (TryReuseExistingRunAndTeleport(player, chapterId, &reuseFailureReason))
            return true;

        if (!reuseFailureReason.empty())
        {
            if (failureReason)
                *failureReason = reuseFailureReason;
            return false;
        }

        uint8 requestedModeType = modeType;
        if (requestedModeType == 0)
            requestedModeType = 1;

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
        state.currentInstanceId = GetPlayerInstanceSignature(player, chapterConfig->mapId);
        state.pendingAbyssModeType = 0;
        state.pendingCacheSummon = false;
        state.hasDatabaseRow = true;

        ClearPlayerSuspendedRunState(guid);
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
        ClearPlayerSuspendedRunState(guid);
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
        state.pendingAbyssModeType = 0;
        state.pendingCacheSummon = false;
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
        state.currentInstanceId = GetPlayerInstanceSignature(player, chapterConfig->mapId);
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

        Creature* summon = player->SummonCreature(chapterConfig->abyssBossEntry, x, y, z, o, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300000);
        if (!summon)
        {
            if (failureReason)
                *failureReason = "summon_failed";
            return false;
        }

        uint16 storyLootMode = GetAbyssLootModeMask(1);
        if (storyLootMode != 0)
            summon->SetLootMode(storyLootMode);

        summon->SetInCombatWith(player);
        summon->AddThreat(player, 1000.0f);
        state.pendingAbyssModeType = 0;
        state.pendingCacheSummon = false;
        state.abyssBossSummoned = true;
        state.currentInstanceId = GetPlayerInstanceSignature(player, chapterConfig->mapId);
        procState.abyssModePromptShown = true;
        SavePlayerRunState(player);
        return true;
    }

    void GrantModeKillQuestCredit(Player* player, Creature* creature, AbyssChapterConfig const& chapterConfig, PlayerAbyssRunState const& runState) const
    {
        if (!player || !creature)
            return;

        uint8 taskType = 0;
        uint32 requiredEntry = 0;
        switch (runState.modeType)
        {
            case 1:
                taskType = 3;
                requiredEntry = chapterConfig.abyssBossEntry;
                break;
            case 2:
                taskType = 4;
                requiredEntry = chapterConfig.abyssBossEntry;
                break;
            case 3:
                taskType = 5;
                requiredEntry = chapterConfig.abyssBossEntry;
                break;
            default:
                return;
        }

        if (requiredEntry == 0 || creature->GetEntry() != requiredEntry)
            return;

        AbyssTaskDocking const* docking = GetTaskDockingForChapter(chapterConfig.chapterId, taskType);
        if (!docking || player->GetQuestRewardStatus(docking->questId))
            return;

        QuestStatus status = player->GetQuestStatus(docking->questId);
        if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_COMPLETE)
            return;

        player->KilledMonsterCredit(requiredEntry, creature->GetGUID());
    }

    bool HandleCreatureKill(Player* player, Creature* creature)
    {
        if (!player || !creature || !_worldDataLoaded)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 creatureEntry = creature->GetEntry();

        AbyssChapterConfig const* mappedChapterByMap = GetChapterConfigByMapId(player->GetMapId());

        auto runItr = _playerRunStates.find(playerGuid);
        if (runItr == _playerRunStates.end())
        {
            if (!mappedChapterByMap)
            {
                auto playerItr = _playerData.find(playerGuid);
                if (playerItr != _playerData.end() && HasAnyActiveRelicSlots(&playerItr->second))
                {
                    PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(playerGuid);
                    ProcessBasicKillRelicTriggers(player, creature, procState, 0);
                    return true;
                }

                return false;
            }

            uint64 lazyTrackedMaskBit = GetTrackedBossMaskBit(*mappedChapterByMap, creatureEntry);
            bool isLazyBootstrapKill = lazyTrackedMaskBit == (1ULL << 0) || lazyTrackedMaskBit == (1ULL << 1);
            if (!isLazyBootstrapKill)
                return false;

            // 允许锚点/最终首领击杀时懒启动正传流程，用于记录击杀状态并弹出模式选择。
            // 运行时直接发装备逻辑已移除，因此这里只恢复流程，不恢复官方直掉模块装备。
            std::string bootstrapFailureReason;
            if (!BeginPlayerRun(player, mappedChapterByMap->chapterId, 1, 0, &bootstrapFailureReason))
                return false;

            runItr = _playerRunStates.find(playerGuid);
            if (runItr == _playerRunStates.end())
                return false;
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

        UpdateActiveRunInstanceSignature(player);

        uint8 modeTypeOnKill = state.modeType;
        uint16 corruptionTierOnKill = state.corruptionTier;
        uint64 trackedMaskBit = GetRuntimeBossProgressMaskBit(*chapterConfig, state, creatureEntry);
        bool changed = false;
        PlayerAbyssProcState& procState = GetOrCreatePlayerProcState(playerGuid);

        if (trackedMaskBit != 0 && (state.anchorBossKillMask & trackedMaskBit) == 0)
        {
            state.anchorBossKillMask |= trackedMaskBit;
            changed = true;
        }

        bool isOfficialKill = trackedMaskBit == kTrackedBossMaskOfficialAnchor || trackedMaskBit == kTrackedBossMaskOfficialFinal;
        bool isModeBossKill = creatureEntry == chapterConfig->abyssBossEntry;
        bool isCacheBossKill = creatureEntry == chapterConfig->cacheBossEntry;

        if (isModeBossKill)
        {
            // 仅清理待召唤标记；保留 abyssBossSummoned=true，避免在非连锁模式（如腐化直入）
            // 因锚点击杀位仍置位而被 IsAbyssSummonReady 判定为可再次召唤，造成同一模式首领被无限重新召唤。
            // 连锁分支不依赖该标志（ScheduleDelayedModeBossSummon 不读取它，ExecuteDelayedModeBossSummon 会再写入 true）。
            if (state.pendingAbyssModeType != 0)
            {
                state.pendingAbyssModeType = 0;
                changed = true;
            }
        }

        if (isCacheBossKill && state.pendingCacheSummon)
        {
            state.pendingCacheSummon = false;
            changed = true;
        }

        if (isOfficialKill)
        {
            procState.lastPromptX = creature->GetPositionX();
            procState.lastPromptY = creature->GetPositionY();
            procState.lastPromptZ = creature->GetPositionZ();
            procState.lastPromptO = creature->GetOrientation();
            procState.hasPromptPosition = true;
        }

        bool readyForPrompt = IsAbyssSummonReady(*chapterConfig, state);
        GrantModeKillQuestCredit(player, creature, *chapterConfig, state);

        if (state.modeType == 1)
        {
            if (isModeBossKill)
            {
                uint8 nextModeType = GetNextSequentialModeType(state);
                if (nextModeType > 1 && !IsChapterModeUnlocked(player, data, *chapterConfig, nextModeType))
                {
                    if (player->GetSession())
                        ChatHandler(player->GetSession()).PSendSysMessage("{}模式尚未解锁，自动连锁已停止。", GetModeBossPrefix(nextModeType));
                }
                else if (nextModeType != 0 && ScheduleDelayedModeBossSummon(player, *chapterConfig, state, nextModeType))
                {
                    changed = true;
                }
            }
            else if (isOfficialKill && readyForPrompt && state.pendingAbyssModeType == 0)
            {
                if (ScheduleDelayedModeBossSummon(player, *chapterConfig, state, 1))
                    changed = true;
            }

            if (changed)
            {
                SavePlayerData(player);
                SavePlayerRunState(player);
            }

            return changed;
        }

        ProcessBasicKillRelicTriggers(player, creature, procState, trackedMaskBit);

        if (HasActiveScriptGroup(player, "遗物_霜王残印"))
            ++procState.dominionCounter;

        if (ShouldUseSequentialModeChain(state))
        {
            if (isModeBossKill)
            {
                uint8 nextModeType = GetNextSequentialModeType(state);
                if (nextModeType > 1 && !IsChapterModeUnlocked(player, data, *chapterConfig, nextModeType))
                {
                    if (player->GetSession())
                        ChatHandler(player->GetSession()).PSendSysMessage("{}模式尚未解锁，自动连锁已停止。", GetModeBossPrefix(nextModeType));
                }
                else if (nextModeType != 0 && ScheduleDelayedModeBossSummon(player, *chapterConfig, state, nextModeType))
                {
                    changed = true;
                }
                else if ((state.anchorBossKillMask & kTrackedBossMaskReincarnationBoss) != 0 &&
                    (state.anchorBossKillMask & kTrackedBossMaskCacheBoss) == 0 &&
                    !state.cacheBossSummoned &&
                    !state.pendingCacheSummon &&
                    chapterConfig->cacheBossEntry != 0 &&
                    ScheduleDelayedCacheBossSummon(player, *chapterConfig, state))
                {
                    changed = true;
                }
            }
        }
        else
        {
            if (IsAbyssSummonReady(*chapterConfig, state) && state.pendingAbyssModeType == 0)
            {
                if (ScheduleDelayedModeBossSummon(player, *chapterConfig, state, state.modeType))
                    changed = true;
            }

            if (isOfficialKill && !state.cacheBossSummoned && !state.pendingCacheSummon && chapterConfig->cacheBossEntry != 0)
            {
                bool pityReached = chapterConfig->cacheBossPityCount > 0 &&
                    static_cast<uint32>(data.cacheBossFailCount + 1) >= chapterConfig->cacheBossPityCount;
                bool rollSuccess = pityReached || RollPercentage() < chapterConfig->cacheBossBaseChance;

                if (rollSuccess && ScheduleDelayedCacheBossSummon(player, *chapterConfig, state))
                {
                    changed = true;
                }
                else if (!state.cacheBossSummoned)
                {
                    ++data.cacheBossFailCount;
                    changed = true;
                }
            }
        }

        if (isCacheBossKill && data.cacheBossFailCount != 0)
        {
            data.cacheBossFailCount = 0;
            changed = true;
        }

        if (isModeBossKill && modeTypeOnKill == 3 && data.highestCorruptionTier < corruptionTierOnKill)
        {
            data.highestCorruptionTier = corruptionTierOnKill;
            changed = true;
        }

        if (changed)
        {
            SavePlayerData(player);
            SavePlayerRunState(player);
        }

        if (IsAbyssCustomBossEntry(creatureEntry))
            EnsureCustomBossLootGenerated(player, creature, chapterConfig->chapterId, state.modeType);

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

    bool EnsureCustomBossLootGenerated(Player* player, Creature* creature, uint16 chapterId = 0, uint8 modeType = 0)
    {
        if (!player || !creature || !_worldDataLoaded)
            return false;

        if (!IsAbyssCustomBossEntry(creature->GetEntry()) || !creature->isDead())
            return false;

        if (creature->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE) && !creature->loot.isLooted())
            return false;

        CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
        if (!creatureTemplate || creatureTemplate->lootid == 0)
            return false;

        creature->loot.clear();
        if (!creature->GetLootRecipient())
            creature->SetLootRecipient(player, player->GetGroup() != nullptr);

        creature->loot.loot_type = LOOT_CORPSE;
        creature->loot.FillLoot(creatureTemplate->lootid, LootTemplates_Creature, player, false, false, creature->GetLootMode(), creature);

        if (creature->GetLootMode())
            creature->loot.generateMoneyLoot(creatureTemplate->mingold, creatureTemplate->maxgold);

        if (!creature->loot.isLooted())
            creature->SetDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        return !creature->loot.isLooted();
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
                SendAbyssStateToAddon(player);
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
    std::unordered_map<uint32, PlayerAbyssRunState> _suspendedRunStates;
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

void SendAbyssOpenUI(Player* player)
{
    SendAbyssPayload(player, "OPEN_UI");
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
    PlayerAbyssRunState const* activeRunState = sAbyssCultivationMgr->GetPlayerRunState(guid);
    PlayerAbyssRunState const* runState = sAbyssCultivationMgr->GetDisplayRunState(player);
    AbyssChapterConfig const* currentMapChapter = sAbyssCultivationMgr->GetChapterConfigByMapId(player->GetMapId());

    std::string currentChapterName = currentChapter ? SanitizeAddonText(currentChapter->chapterName) : "0";
    if (currentChapterName.empty())
        currentChapterName = "0";

    std::string nextChapterName = nextChapter ? SanitizeAddonText(nextChapter->chapterName) : "0";
    if (nextChapterName.empty())
        nextChapterName = "0";

    std::string currentMapChapterName = currentMapChapter ? SanitizeAddonText(currentMapChapter->chapterName) : "0";
    if (currentMapChapterName.empty())
        currentMapChapterName = "0";

    std::ostringstream payload;
    payload << "STATE:"
            << guid << '|'
            << static_cast<uint32>(player->GetLevel()) << '|'
            << playerData->currentChapter << '|'
            << currentChapterName << '|'
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
            << (activeRunState ? 1 : 0) << '|'
            << (runState ? runState->currentChapterId : 0) << '|'
            << (runState ? static_cast<uint32>(runState->modeType) : 0) << '|'
            << (runState ? runState->corruptionTier : 0) << '|'
            << (runState ? runState->currentMapId : 0) << '|'
            << (runState ? runState->anchorBossKillMask : 0) << '|'
            << (runState ? (runState->abyssBossSummoned ? 1 : 0) : 0) << '|'
            << (runState ? (runState->cacheBossSummoned ? 1 : 0) : 0) << '|'
            << (nextChapter ? nextChapter->chapterId : 0) << '|'
            << nextChapterName << '|'
            << player->GetMapId() << '|'
            << (currentMapChapter ? currentMapChapter->chapterId : 0) << '|'
            << currentMapChapterName;

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

void SendAbyssRewardPreviewCategoryToAddon(Player* player, std::string const& header, AbyssChapterConfig const& chapter, uint32 bossEntry, uint8 modeType)
{
    if (!player)
        return;

    // 首领Entry为0表示该章节没有此类首领，不发送数据
    if (bossEntry == 0)
        return;

    std::vector<AbyssRewardCandidate> candidates = sAbyssCultivationMgr->GetRewardCandidatesForBoss(chapter, bossEntry, modeType);
    std::ostringstream payload;
    payload << header << ':' << chapter.chapterId << '^' << SanitizeAddonText(chapter.chapterName) << '^' << static_cast<uint32>(modeType);

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

void SendAbyssRewardPreviewToAddon(Player* player, AbyssChapterConfig const* chapter, char const* scope, uint8 modeType = 1)
{
    if (!player || !chapter || !scope)
        return;

    std::string prefix(scope);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_ANCHOR", *chapter, chapter->anchorBossEntry, modeType);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_FINAL", *chapter, chapter->finalBossEntry, modeType);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_ABYSS", *chapter, chapter->abyssBossEntry, modeType);
    SendAbyssRewardPreviewCategoryToAddon(player, "REWARD_" + prefix + "_CACHE", *chapter, chapter->cacheBossEntry, modeType);
}

void SendAbyssRewardPreviewForChapterIdToAddon(Player* player, uint16 chapterId, uint8 modeType = 1)
{
    if (!player || chapterId == 0)
        return;

    SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetChapterConfig(chapterId), "VIEW", modeType);
}

void SendAbyssAllToAddon(Player* player)
{
    SendAbyssStateToAddon(player);
    SendAbyssChapterListToAddon(player);
    SendAbyssRelicsToAddon(player);

    if (player)
    {
        uint32 guid = player->GetGUID().GetCounter();
        if (PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(guid))
        {
            SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter), "CURRENT", 1);
            SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter), "NEXT", 1);
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

        sAbyssCultivationMgr->SyncTrackedBossLootMode(creature);

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

        uint32 key = creature->GetGUID().GetCounter();

        sAbyssCultivationMgr->SyncTrackedBossLootMode(creature);

        AbyssBossConfig const* bossConfig = sAbyssCultivationMgr->GetBossConfig(creature->GetEntry());
        if (!bossConfig)
            return;

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
        if (phase == 0)
            phase = 1;

        if (phase < 2 && bossConfig->phase2HealthPct > 0 && creature->HealthBelowPct(bossConfig->phase2HealthPct))
        {
            phase = 2;
        }

        if (phase < 3 && bossConfig->phase3HealthPct > 0 && creature->HealthBelowPct(bossConfig->phase3HealthPct))
        {
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
        LOG_INFO("server.loading", "→深渊修炼系统√");
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
            PLAYERHOOK_ON_AFTER_MOVE_ITEM_FROM_INVENTORY,
            PLAYERHOOK_ON_APPLY_WEAPON_DAMAGE,
            PLAYERHOOK_ON_PLAYER_ENTER_COMBAT,
            PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT
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
        sAbyssCultivationMgr->ClearPlayerSuspendedRunState(guid);
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
        if (sAbyssCultivationMgr->HandleCreatureKill(player, creature))
            SendAbyssStateToAddon(player);
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
            requiredEntry = chapterConfig->abyssBossEntry;
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

        if (command.rfind("COLLECT_RELIC:", 0) == 0)
        {
            uint32 itemId = static_cast<uint32>(std::strtoul(command.substr(14).c_str(), nullptr, 10));
            std::string failureReason;
            bool ok = sAbyssCultivationMgr->CollectRelicFromInventory(player, itemId, &failureReason);
            SendAbyssResult(player, "COLLECT_RELIC", ok, ok ? "ok" : failureReason);
            SendAbyssAllToAddon(player);
            return;
        }

        if (command == "REQ_REWARD_CURRENT" || command.rfind("REQ_REWARD_CURRENT:", 0) == 0)
        {
            uint8 modeType = 1;
            if (command.length() > 19)
                modeType = ParseAddonRewardMode(command.substr(19), 1);
            if (PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter()))
                SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetChapterConfig(playerData->currentChapter), "CURRENT", modeType);
            return;
        }

        if (command == "REQ_REWARD_NEXT" || command.rfind("REQ_REWARD_NEXT:", 0) == 0)
        {
            uint8 modeType = 1;
            if (command.length() > 16)
                modeType = ParseAddonRewardMode(command.substr(16), 1);
            if (PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter()))
                SendAbyssRewardPreviewToAddon(player, sAbyssCultivationMgr->GetNextChapterConfig(playerData->currentChapter), "NEXT", modeType);
            return;
        }

        if (command.rfind("REQ_REWARD_CHAPTER:", 0) == 0)
        {
            uint16 chapterId = 0;
            uint8 modeType = 1;
            if (ParseAddonRewardChapterRequest(command.substr(19), chapterId, modeType))
                SendAbyssRewardPreviewForChapterIdToAddon(player, chapterId, modeType);
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

        if (sAbyssCultivationMgr->HandlePlayerSetDeathProtection(player))
            return;

        if (sAbyssCultivationMgr->EndPlayerRun(player))
            SendAbyssStateToAddon(player);
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
        {
            SendAbyssStateToAddon(player);
            return;
        }

        sAbyssCultivationMgr->UpdateActiveRunInstanceSignature(player);

        if (IsAutoBeginOnMapEnterEnabled())
        {
            if (sAbyssCultivationMgr->TryAutoBeginPlayerRun(player))
                SendAbyssStateToAddon(player);
        }
        sAbyssCultivationMgr->HandlePlayerUpdate(player, diff);
    }

    void OnPlayerEnterCombat(Player* player, Unit* /*enemy*/) override
    {
        if (!player || !IsModuleEnabled())
            return;

        PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        procState.lastCombatEnterTime = GetNow();
        procState.openingAttackCount = 0;
        procState.artifactCombatEchoUsed = false;
        procState.artifactSubLinkUsed = false;
        procState.artifactMainProcCounter = 0;
        procState.artifactFistComboCounter = 0;
        procState.artifactStaffCadenceCounter = 0;
        procState.artifactFalunOrbitCounter = 0;
        procState.artifactSwordMarkStacks.clear();
        procState.artifactBowMarkStacks.clear();

        PlayerAbyssData const* playerData = sAbyssCultivationMgr->GetPlayerData(player->GetGUID().GetCounter());
        if (!playerData)
            return;

        if (playerData->phaseArtifact == ABYSS_PHASE_ARTIFACT_BURNING_PACT_ITEM)
            sAbyssCultivationMgr->CastManagedRelicSpell(player, 89175);

        if (playerData->phaseArtifact == ABYSS_PHASE_ARTIFACT_VOID_EXPEDITION_ITEM)
            sAbyssCultivationMgr->CastManagedRelicSpell(player, 89176);
    }

    void OnPlayerLeaveCombat(Player* player) override
    {
        if (!player || !IsModuleEnabled())
            return;

        PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        procState.openingAttackCount = 0;
        if (procState.setCombatGrowthStacks != 0)
        {
            procState.setCombatGrowthStacks = 0;
            procState.lastSetCombatGrowthTime = 0;
            player->UpdateAllStats();
            player->UpdateAllRatings();
            if (player->GetSession())
            {
                char const* growthLabel = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_烬灭永燃") > 0.0f
                    ? "烬灭永燃"
                    : "烬世叠加";
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff8080[套装] %s已重置。|r", growthLabel);
            }
        }

        procState.artifactFistComboCounter = 0;
        procState.artifactStaffCadenceCounter = 0;
        procState.artifactFalunOrbitCounter = 0;
        procState.artifactSwordMarkStacks.clear();
        procState.artifactBowMarkStacks.clear();
    }

    void OnPlayerApplyWeaponDamage(Player* player, uint8 /*slot*/, ItemTemplate const* proto, float& /*minDamage*/, float& /*maxDamage*/, uint8 /*damageIndex*/) override
    {
        if (!player || !proto || !IsModuleEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        if (!sAbyssCultivationMgr->GetPlayerRunState(guid) &&
            !HasAnyActiveRelicSlots(sAbyssCultivationMgr->GetPlayerData(guid)) &&
            !sAbyssCultivationMgr->HasAnyActiveSetBonuses(player))
            return;

        PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(guid);
        uint32 now = GetNow();
        if (Unit* target = sAbyssCultivationMgr->GetPrimaryCombatTarget(player))
        {
            float emberEchoScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_灰烬回响");
            if (emberEchoScale > 0.0f &&
                now > procState.lastSetEchoTime + 6 &&
                RollPercentage() < std::min(45.0f, 15.0f * emberEchoScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 120, 180, SPELL_SCHOOL_MASK_FIRE, emberEchoScale);
                procState.lastSetEchoTime = now;
            }

            float artifactFlameScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_焚界天爆");
            if (artifactFlameScale > 0.0f &&
                now > procState.lastSetDoubleBurstTime + 6 &&
                RollPercentage() < std::min(55.0f, 15.0f * artifactFlameScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_FIRE, artifactFlameScale);
                sAbyssCultivationMgr->RestorePlayerPrimaryPowerPct(player, 3.0f * artifactFlameScale);
                procState.lastSetDoubleBurstTime = now;
            }

            float doubleBurstScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_轮焰双爆");
            if (artifactFlameScale <= 0.0f &&
                doubleBurstScale > 0.0f &&
                now > procState.lastSetDoubleBurstTime + 8 &&
                RollPercentage() < std::min(45.0f, 12.0f * doubleBurstScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 170, 240, SPELL_SCHOOL_MASK_NORMAL, doubleBurstScale);
                procState.lastSetDoubleBurstTime = now;
            }

            float artifactFreezeScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_永冻裁决");
            if (artifactFreezeScale > 0.0f &&
                now > procState.lastSetFreezeTime + 6 &&
                RollPercentage() < std::min(60.0f, 18.0f * artifactFreezeScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 210, 300, SPELL_SCHOOL_MASK_FROST, artifactFreezeScale);
                procState.lastSetFreezeTime = now;
            }

            float frostSealScale = std::max(
                sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_寒夜冻结"),
                sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_玄霜封印"));
            if (artifactFreezeScale <= 0.0f &&
                frostSealScale > 0.0f &&
                now > procState.lastSetFreezeTime + 8 &&
                RollPercentage() < std::min(50.0f, 15.0f * frostSealScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 130, 200, SPELL_SCHOOL_MASK_FROST, frostSealScale);
                procState.lastSetFreezeTime = now;
            }

            float artifactAbyssScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_渊神吞界");
            if (artifactAbyssScale > 0.0f &&
                now > procState.lastSetAbyssDrainTime + 6 &&
                RollPercentage() < std::min(50.0f, 15.0f * artifactAbyssScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 220, 320, SPELL_SCHOOL_MASK_SHADOW, artifactAbyssScale);
                player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(5), artifactAbyssScale));
                sAbyssCultivationMgr->RestorePlayerPrimaryPowerPct(player, 4.0f * artifactAbyssScale);
                procState.lastSetAbyssDrainTime = now;
            }

            float abyssDrainScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_神蚀吸收");
            if (artifactAbyssScale <= 0.0f &&
                abyssDrainScale > 0.0f &&
                now > procState.lastSetAbyssDrainTime + 8 &&
                RollPercentage() < std::min(40.0f, 12.0f * abyssDrainScale))
            {
                sAbyssCultivationMgr->DealConfiguredBurst(player, target, 150, 220, SPELL_SCHOOL_MASK_SHADOW, abyssDrainScale);
                player->ModifyHealth(ScaleIntValue(player->CountPctFromMaxHealth(3), abyssDrainScale));
                procState.lastSetAbyssDrainTime = now;
            }
        }

        float ancientPowerScale = sAbyssCultivationMgr->GetActiveSetSpecialEffectScale(player, "套装_古神觉醒");
        if (ancientPowerScale > 0.0f &&
            now > procState.lastSetAncientPowerTime + 20 &&
            RollPercentage() < std::min(40.0f, 10.0f * ancientPowerScale))
        {
            procState.lastSetAncientPowerTime = now;
            procState.setAncientPowerEndTime = now + 10;
            player->UpdateAllStats();
            player->UpdateAllRatings();
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff80[套装] 古神觉醒触发，10秒内全属性提升。|r");
        }

        float wolfScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_狼王残月");
        if (wolfScale > 0.0f && now > procState.lastWolfMoonTime + 6)
        {
            if (RollPercentage() < 25.0f * std::max(wolfScale, 0.25f))
            {
                if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89104))
                    procState.lastWolfMoonTime = now;
            }
        }

        float thornScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_棘魂号角");
        if (thornScale > 0.0f && now < procState.lastStillnessTime + 2 && now > procState.lastThornChargeTime + 5)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89108))
                procState.lastThornChargeTime = now;
        }

        float furnaceScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_黑炉王印");
        if (furnaceScale > 0.0f && now < procState.lastFireSchoolCastTime + 8 && now > procState.lastBlackFurnaceTime + 6)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89115))
                procState.lastBlackFurnaceTime = now;
        }

        float generalScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_将军狱旗");
        if (generalScale > 0.0f && procState.lastCombatEnterTime != 0 && now < procState.lastCombatEnterTime + 30 && now > procState.lastGeneralEchoTime + 5)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89117))
                procState.lastGeneralEchoTime = now;
        }

        float bulwarkScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_残垒战契");
        if (bulwarkScale > 0.0f && procState.openingAttackCount < 3 && now > procState.lastBulwarkTime + 2)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89121))
            {
                procState.lastBulwarkTime = now;
                ++procState.openingAttackCount;
            }
        }

        float bloodOrbScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_邪血蒸馏器");
        if (bloodOrbScale > 0.0f && now > procState.lastBloodOrbTime + 4)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89122))
                procState.lastBloodOrbTime = now;
        }

        float clockScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_时痕怀表");
        if (clockScale > 0.0f && procState.lastSpellId != 0 && now < procState.lastStillnessTime + 2 && now > procState.lastClockMarkTime + 8)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89131))
                procState.lastClockMarkTime = now;
        }

        float spiderScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_蛛网夜卵");
        if (spiderScale > 0.0f && now > procState.lastSpiderEggTime + 8)
        {
            if (RollPercentage() < 20.0f * std::max(spiderScale, 0.25f))
            {
                if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89139))
                    procState.lastSpiderEggTime = now;
            }
        }

        float clockJudgeScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_时审钟摆");
        if (clockJudgeScale > 0.0f && procState.lastCombatEnterTime != 0 && now > procState.lastCombatEnterTime + 10 && now > procState.lastExecutionStakeTime + 10)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89146))
                procState.lastExecutionStakeTime = now;
        }

        float stakeScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_萨钢骨钉");
        if (stakeScale > 0.0f && procState.lastCombatEnterTime != 0 && now > procState.lastCombatEnterTime + 15 && now > procState.lastExecutionStakeTime + 8)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89151))
                procState.lastExecutionStakeTime = now;
        }

        float mirrorShardScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_映像碎镜");
        if (mirrorShardScale > 0.0f && procState.lastSpellId != 0 && now > procState.lastMirrorShardTime + 10)
        {
            if (RollPercentage() < 18.0f * std::max(mirrorShardScale, 0.25f))
            {
                if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89152))
                    procState.lastMirrorShardTime = now;
            }
        }

        float reverseScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_逆鳞王冠");
        if (reverseScale > 0.0f && now < procState.lastStillnessTime + 2 && now > procState.lastReverseScaleTime + 6)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89154))
                procState.lastReverseScaleTime = now;
        }

        float eclipseScale = sAbyssCultivationMgr->GetActiveScriptGroupScale(player, "遗物_日蚀残晕");
        if (eclipseScale > 0.0f && player->GetPowerPct(player->getPowerType()) >= 90.0f && now > procState.lastEclipseTime + 12)
        {
            if (sAbyssCultivationMgr->CastManagedRelicSpell(player, 89166))
                procState.lastEclipseTime = now;
        }
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

        PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
        uint32 now = GetNow();
        if (procState.setAncientPowerEndTime != 0 && now <= procState.setAncientPowerEndTime)
            value += 50.0f;
        if (procState.setCombatGrowthStacks != 0)
            value += static_cast<float>(procState.setCombatGrowthStacks * 2);
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

    void OnPlayerAfterUpdateSpellDamageAndHealing(Player* player, int64& healingBonus, int64 spellDamage[7]) override
    {
        if (!player || !IsModuleEnabled())
            return;

        float bonusPct = sAbyssCultivationMgr->GetPlayerRuntimeSpellBonusPct(player);
        int32 flatSpellPower = sAbyssCultivationMgr->GetPlayerRuntimeRelicSpellPowerBonus(player);
        if (bonusPct == 0.0f && flatSpellPower == 0)
            return;

        if (bonusPct != 0.0f)
        {
            float multiplier = 1.0f + bonusPct / 100.0f;
            healingBonus = ScaleIntValue(healingBonus, multiplier);
            for (uint8 school = 0; school < 7; ++school)
                spellDamage[school] = ScaleIntValue(spellDamage[school], multiplier);
        }

        if (flatSpellPower != 0)
        {
            healingBonus = AddInt64Saturated(healingBonus, flatSpellPower);
            for (uint8 school = 0; school < 7; ++school)
                spellDamage[school] = AddInt64Saturated(spellDamage[school], flatSpellPower);
        }
    }

    void OnPlayerAfterUpdateRating(Player* player, CombatRating cr, int64& amount) override
    {
        if (!player || !IsModuleEnabled())
            return;

        amount = AddInt64Saturated(amount, sAbyssCultivationMgr->GetPlayerRuntimeRelicRatingBonus(player, cr));
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
            { "界面",       HandleOpenUICommand,      SEC_PLAYER,         Console::No  },
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
            { "深渊", abyssSubTable },
            { "深渊修仙", abyssSubTable }
        };

        return rootTable;
    }

    static bool HandleOpenUICommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!IsModuleEnabled())
        {
            handler->SendSysMessage("深渊修仙系统当前未启用。");
            return true;
        }

        SendAbyssOpenUI(player);
        SendAbyssAllToAddon(player);
        return true;
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
        handler->PSendSysMessage("  cacheArtifactDropChance={}", ABYSS_CACHE_ARTIFACT_DROP_CHANCE);

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

constexpr uint8 ABYSS_POISON_MAX_STACKS = 5;
constexpr uint8 ABYSS_POISON_SPREAD_STACKS = 3;
constexpr float ABYSS_POISON_SPREAD_RADIUS = 8.0f;
constexpr uint32 SPELL_ABYSS_ARTIFACT_WEAPON_START = 89401;
constexpr uint32 SPELL_ABYSS_ARTIFACT_WEAPON_END = 89490;
constexpr uint32 SPELL_ABYSS_ARTIFACT_WEAPON_VISUAL_START = 996401;
constexpr uint8 ABYSS_ARTIFACT_WEAPON_VARIANTS_PER_ACT = 15;

enum ArtifactWeaponFamilyId : uint8
{
    ARTIFACT_WEAPON_SWORD = 1,
    ARTIFACT_WEAPON_AXE = 2,
    ARTIFACT_WEAPON_HAMMER = 3,
    ARTIFACT_WEAPON_DAGGER = 4,
    ARTIFACT_WEAPON_FIST = 5,
    ARTIFACT_WEAPON_GREATSWORD = 6,
    ARTIFACT_WEAPON_GREATAXE = 7,
    ARTIFACT_WEAPON_GREATHAMMER = 8,
    ARTIFACT_WEAPON_POLEARM = 9,
    ARTIFACT_WEAPON_STAFF = 10,
    ARTIFACT_WEAPON_BOW = 11,
    ARTIFACT_WEAPON_GUN = 12,
    ARTIFACT_WEAPON_CROSSBOW = 13,
    ARTIFACT_WEAPON_WAND = 14,
    ARTIFACT_WEAPON_FALUN = 15
};

struct ArtifactWeaponActThemeConfig
{
    uint8 actId = 0;
    char const* label = "";
    SpellSchoolMask primarySchool = SPELL_SCHOOL_MASK_NORMAL;
    SpellSchoolMask secondarySchool = SPELL_SCHOOL_MASK_NORMAL;
    float scalar = 1.0f;
};

ArtifactWeaponActThemeConfig const& GetArtifactWeaponActThemeConfig(uint8 actId)
{
    static std::array<ArtifactWeaponActThemeConfig, 6> const configs =
    {{
        { 1, "灰烬", SPELL_SCHOOL_MASK_FIRE,   SPELL_SCHOOL_MASK_NORMAL, 1.00f },
        { 2, "虚空", SPELL_SCHOOL_MASK_SHADOW, SPELL_SCHOOL_MASK_NORMAL, 1.10f },
        { 3, "霜雷", SPELL_SCHOOL_MASK_FROST,  SPELL_SCHOOL_MASK_NATURE, 1.18f },
        { 4, "古神", SPELL_SCHOOL_MASK_SHADOW, SPELL_SCHOOL_MASK_ARCANE, 1.26f },
        { 5, "日蚀", SPELL_SCHOOL_MASK_HOLY,   SPELL_SCHOOL_MASK_FIRE,   1.36f },
        { 6, "统御", SPELL_SCHOOL_MASK_ARCANE, SPELL_SCHOOL_MASK_HOLY,   1.48f },
    }};

    size_t index = std::clamp<size_t>(static_cast<size_t>(actId), 1u, configs.size()) - 1u;
    return configs[index];
}

bool DecodeArtifactWeaponSpell(uint32 spellId, uint8& actId, uint8& familyId)
{
    if (spellId < SPELL_ABYSS_ARTIFACT_WEAPON_START || spellId > SPELL_ABYSS_ARTIFACT_WEAPON_END)
        return false;

    uint32 offset = spellId - SPELL_ABYSS_ARTIFACT_WEAPON_START;
    actId = static_cast<uint8>(offset / ABYSS_ARTIFACT_WEAPON_VARIANTS_PER_ACT) + 1u;
    familyId = static_cast<uint8>(offset % ABYSS_ARTIFACT_WEAPON_VARIANTS_PER_ACT) + 1u;
    return true;
}

SpellSchoolMask GetArtifactWeaponSchoolMask(uint8 actId, ArtifactWeaponFamilyId familyId, bool alternate = false)
{
    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    if (!alternate)
        return theme.primarySchool;

    switch (familyId)
    {
        case ARTIFACT_WEAPON_SWORD:
        case ARTIFACT_WEAPON_AXE:
        case ARTIFACT_WEAPON_GREATSWORD:
        case ARTIFACT_WEAPON_GREATAXE:
            return theme.secondarySchool != SPELL_SCHOOL_MASK_NORMAL ? theme.secondarySchool : SPELL_SCHOOL_MASK_NORMAL;
        case ARTIFACT_WEAPON_HAMMER:
        case ARTIFACT_WEAPON_GREATHAMMER:
            return actId == 3 ? SPELL_SCHOOL_MASK_NATURE : theme.secondarySchool;
        case ARTIFACT_WEAPON_DAGGER:
            return actId <= 2 ? SPELL_SCHOOL_MASK_NATURE : theme.secondarySchool;
        case ARTIFACT_WEAPON_FIST:
            return actId >= 5 ? SPELL_SCHOOL_MASK_FIRE : SPELL_SCHOOL_MASK_NORMAL;
        case ARTIFACT_WEAPON_POLEARM:
        case ARTIFACT_WEAPON_CROSSBOW:
            return SPELL_SCHOOL_MASK_NORMAL;
        case ARTIFACT_WEAPON_STAFF:
        case ARTIFACT_WEAPON_WAND:
        case ARTIFACT_WEAPON_FALUN:
            return theme.secondarySchool;
        case ARTIFACT_WEAPON_BOW:
            return actId == 3 ? SPELL_SCHOOL_MASK_FROST : theme.secondarySchool;
        case ARTIFACT_WEAPON_GUN:
            return actId == 1 ? SPELL_SCHOOL_MASK_FIRE : SPELL_SCHOOL_MASK_NORMAL;
        default:
            return theme.secondarySchool;
    }
}

double GetAbyssAttackPower(Unit* caster)
{
    if (!caster)
        return 0.0;

    if (Player* player = caster->ToPlayer())
        return player->GetExtendedTotalAttackPowerValue(BASE_ATTACK);

    return caster->GetTotalAttackPowerValue(BASE_ATTACK);
}

double GetAbyssSpellPower(Unit* caster, SpellSchoolMask schoolMask)
{
    if (!caster)
        return 0.0;

    if (Player* player = caster->ToPlayer())
        return player->GetExtendedSpellDamageBonus();

    return caster->SpellBaseDamageBonusDone(schoolMask);
}

double GetArtifactWeaponPower(Player* player, ArtifactWeaponFamilyId familyId, SpellSchoolMask /*schoolMask*/)
{
    if (!player)
        return 0.0f;

    double attackPower = player->GetExtendedTotalAttackPowerValue(BASE_ATTACK);
    double spellPower = player->GetExtendedSpellDamageBonus();

    switch (familyId)
    {
        case ARTIFACT_WEAPON_STAFF:
        case ARTIFACT_WEAPON_WAND:
        case ARTIFACT_WEAPON_FALUN:
            return std::max(spellPower, attackPower * 0.65f);
        case ARTIFACT_WEAPON_BOW:
        case ARTIFACT_WEAPON_GUN:
        case ARTIFACT_WEAPON_CROSSBOW:
            return std::max(attackPower, spellPower * 0.45f);
        default:
            return std::max(attackPower, spellPower * 0.35f);
    }
}

Unit* ResolveArtifactWeaponPrimaryTarget(Player* player, Unit* hitUnit, Unit* explicitTarget)
{
    if (hitUnit)
        return hitUnit;
    if (explicitTarget)
        return explicitTarget;
    return player ? sAbyssCultivationMgr->GetPrimaryCombatTarget(player) : nullptr;
}

std::list<Unit*> CollectArtifactWeaponTargets(Unit* center, Unit* caster, float radius)
{
    std::list<Unit*> targets;
    if (!center || !caster || radius <= 0.0f)
        return targets;

    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, caster, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, targets, check);
    Cell::VisitAllObjects(center, searcher, radius);
    return targets;
}

void DealArtifactWeaponDamage(Player* player, Unit* target, ArtifactWeaponFamilyId familyId, SpellSchoolMask schoolMask, float scale)
{
    if (!player || !target || scale <= 0.0f)
        return;

    double power = GetArtifactWeaponPower(player, familyId, schoolMask);
    int64 damage = std::max<int64>(1, static_cast<int64>(std::llround(power * scale)));
    player->DealDamage(player, target, damage, nullptr, SPELL_DIRECT_DAMAGE, schoolMask);
}

void DealArtifactWeaponAreaDamage(Player* player, Unit* center, ArtifactWeaponFamilyId familyId, SpellSchoolMask schoolMask, float scale, float radius, bool frontOnly = false)
{
    if (!player || !center || scale <= 0.0f || radius <= 0.0f)
        return;

    std::list<Unit*> targets = CollectArtifactWeaponTargets(center, player, radius);
    for (Unit* unit : targets)
    {
        if (!unit)
            continue;
        if (frontOnly && !player->HasInArc(float(M_PI * 0.75f), unit))
            continue;

        DealArtifactWeaponDamage(player, unit, familyId, schoolMask, scale);
    }
}

void PullArtifactWeaponTarget(Player* player, Unit* target, float distance)
{
    if (!player || !target || distance <= 0.0f)
        return;

    float angle = target->GetAngle(player);
    float newX = target->GetPositionX() + std::cos(angle) * distance;
    float newY = target->GetPositionY() + std::sin(angle) * distance;
    target->NearTeleportTo(newX, newY, target->GetPositionZ(), target->GetOrientation());
}

void ApplyArtifactWeaponActFollowup(Player* player, Unit* target, uint8 actId, ArtifactWeaponFamilyId familyId, float scale)
{
    if (!player || !target || scale <= 0.0f)
        return;

    switch (actId)
    {
        case 1:
            DealArtifactWeaponAreaDamage(player, target, familyId, SPELL_SCHOOL_MASK_FIRE, 0.35f * scale, 4.0f);
            break;
        case 2:
        {
            int64 healAmount = ScaleUInt64ToInt64(player->GetMaxHealthForCombat(), 0.02L * static_cast<long double>(scale));
            player->ModifyHealth(healAmount);
            break;
        }
        case 3:
            player->CastSpell(target, SPELL_ABYSS_ABYSS_ROOT, true);
            break;
        case 4:
            player->CastSpell(target, familyId == ARTIFACT_WEAPON_HAMMER || familyId == ARTIFACT_WEAPON_GUN
                ? SPELL_ABYSS_ARMOR_DEBUFF
                : SPELL_ABYSS_DESTROY_DEBUFF, true);
            break;
        case 5:
            DealArtifactWeaponDamage(player, target, familyId, SPELL_SCHOOL_MASK_FIRE, 0.45f * scale);
            break;
        case 6:
            player->CastSpell(target, SPELL_ABYSS_WORLD_STUN, true);
            break;
        default:
            break;
    }
}

void HandleArtifactWeaponSwordProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
    uint32 targetGuid = target->GetGUID().GetCounter();
    uint8& stacks = procState.artifactSwordMarkStacks[targetGuid];
    stacks = std::min<uint8>(3, stacks + 1);

    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_SWORD, theme.primarySchool, 1.05f * theme.scalar);
    if (stacks < 3)
        return;

    stacks = 0;
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_SWORD, theme.primarySchool, 2.20f * theme.scalar);
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_SWORD, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_SWORD, true), 0.55f * theme.scalar, 10.0f, true);
    ApplyArtifactWeaponActFollowup(player, target, actId, ARTIFACT_WEAPON_SWORD, theme.scalar);
}

void HandleArtifactWeaponAxeProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_AXE, theme.primarySchool, 1.40f * theme.scalar);

    if (target->HealthBelowPct(35))
    {
        DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_AXE, theme.primarySchool, 2.40f * theme.scalar);
        DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_AXE, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_AXE, true), 0.80f * theme.scalar, 6.0f);
        int64 healAmount = ScaleUInt64ToInt64(player->GetMaxHealthForCombat(), 0.04L * static_cast<long double>(theme.scalar));
        player->ModifyHealth(healAmount);
        return;
    }

    ApplyArtifactWeaponActFollowup(player, target, actId, ARTIFACT_WEAPON_AXE, theme.scalar * 0.75f);
}

void HandleArtifactWeaponHammerProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_HAMMER, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_HAMMER), 2.10f * theme.scalar, 8.0f);
    player->CastSpell(target, SPELL_ABYSS_ARMOR_DEBUFF, true);
    if (actId == 3 || actId == 6)
        player->CastSpell(target, SPELL_ABYSS_WORLD_STUN, true);
    else if (actId == 1)
        player->CastSpell(target, SPELL_ABYSS_LAVA_SLOW, true);
}

void HandleArtifactWeaponDaggerProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_DAGGER, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_DAGGER), 1.50f * theme.scalar);
    player->CastSpell(target, SPELL_ABYSS_POISON_DOT, true);

    if (Aura* poisonAura = target->GetAura(SPELL_ABYSS_POISON_DOT))
        if (poisonAura->GetStackAmount() >= 4)
            player->CastSpell(target, SPELL_ABYSS_POISON_DETONATE, true);
}

void HandleArtifactWeaponFistProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
    ++procState.artifactFistComboCounter;

    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_FIST, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_FIST), 1.00f * theme.scalar);
    if (procState.artifactFistComboCounter < 3)
        return;

    procState.artifactFistComboCounter = 0;
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_FIST, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_FIST, true), 1.80f * theme.scalar, 8.0f);
    player->CastSpell(player, SPELL_ABYSS_CHARGE_BUFF, true);
}

void HandleArtifactWeaponGreatswordProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_GREATSWORD, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATSWORD), 2.80f * theme.scalar);
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_GREATSWORD, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATSWORD, true), 0.70f * theme.scalar, 12.0f, true);
    ApplyArtifactWeaponActFollowup(player, target, actId, ARTIFACT_WEAPON_GREATSWORD, theme.scalar);
}

void HandleArtifactWeaponGreataxeProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_GREATAXE, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATAXE), 1.65f * theme.scalar);

    if (target->HealthBelowPct(30))
    {
        DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_GREATAXE, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATAXE), 3.20f * theme.scalar);
        DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_GREATAXE, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATAXE, true), 0.90f * theme.scalar, 8.0f);
        int64 healAmount = ScaleUInt64ToInt64(player->GetMaxHealthForCombat(), 0.06L * static_cast<long double>(theme.scalar));
        player->ModifyHealth(healAmount);
    }
}

void HandleArtifactWeaponGreathammerProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_GREATHAMMER, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATHAMMER), 3.40f * theme.scalar);
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_GREATHAMMER, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GREATHAMMER, true), 1.40f * theme.scalar, 10.0f);
    target->KnockbackFrom(player->GetPositionX(), player->GetPositionY(), 6.0f, 5.0f);
    player->CastSpell(target, SPELL_ABYSS_WORLD_STUN, true);
}

void HandleArtifactWeaponPolearmProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_POLEARM, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_POLEARM), 2.10f * theme.scalar);
    PullArtifactWeaponTarget(player, target, 4.0f);
    if (actId == 3)
        player->CastSpell(target, SPELL_ABYSS_ABYSS_ROOT, true);
    else
        ApplyArtifactWeaponActFollowup(player, target, actId, ARTIFACT_WEAPON_POLEARM, theme.scalar * 0.8f);
}

void HandleArtifactWeaponStaffProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
    ++procState.artifactStaffCadenceCounter;

    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_STAFF, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_STAFF), 1.20f * theme.scalar);
    if (procState.artifactStaffCadenceCounter < 3)
        return;

    procState.artifactStaffCadenceCounter = 0;
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_STAFF, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_STAFF), 2.40f * theme.scalar, 10.0f);
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_STAFF, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_STAFF, true), 0.60f * theme.scalar, 10.0f);
}

void HandleArtifactWeaponBowProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
    uint32 targetGuid = target->GetGUID().GetCounter();
    uint8& stacks = procState.artifactBowMarkStacks[targetGuid];
    stacks = std::min<uint8>(3, stacks + 1);

    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_BOW, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_BOW), 1.05f * theme.scalar);
    if (stacks < 3)
        return;

    stacks = 0;
    DealArtifactWeaponAreaDamage(player, target, ARTIFACT_WEAPON_BOW, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_BOW), 2.00f * theme.scalar, 10.0f);
    player->CastSpell(target, SPELL_ABYSS_DESTROY_DEBUFF, true);
}

void HandleArtifactWeaponGunProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_GUN, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GUN), 2.30f * theme.scalar);

    std::list<Unit*> targets = CollectArtifactWeaponTargets(target, player, 14.0f);
    for (Unit* unit : targets)
    {
        if (!unit || unit == target)
            continue;
        if (!player->HasInArc(float(M_PI / 3.0f), unit))
            continue;

        DealArtifactWeaponDamage(player, unit, ARTIFACT_WEAPON_GUN, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_GUN, true), 0.75f * theme.scalar);
    }

    player->CastSpell(target, SPELL_ABYSS_ARMOR_DEBUFF, true);
}

void HandleArtifactWeaponCrossbowProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_CROSSBOW, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_CROSSBOW), 2.80f * theme.scalar);
    player->CastSpell(target, SPELL_ABYSS_ABYSS_ROOT, true);
    if (target->HasAura(SPELL_ABYSS_ABYSS_ROOT) || target->HasAura(SPELL_ABYSS_WORLD_STUN))
        DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_CROSSBOW, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_CROSSBOW, true), 0.75f * theme.scalar);
}

void HandleArtifactWeaponWandProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !target || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    DealArtifactWeaponDamage(player, target, ARTIFACT_WEAPON_WAND, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_WAND), 1.15f * theme.scalar);

    Powers powerType = player->getPowerType();
    bool highPower = false;
    if (powerType != POWER_HEALTH && player->GetMaxPowerForCombat(powerType) > 0)
        highPower = player->GetPowerPct(powerType) >= 80.0f;

    PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
    if (!highPower && GetNow() > procState.lastSpellCastTime + 5)
        return;

    std::list<Unit*> targets = CollectArtifactWeaponTargets(target, player, 12.0f);
    uint8 bounced = 0;
    for (Unit* unit : targets)
    {
        if (!unit || unit == target)
            continue;

        float decay = 1.0f - (float(bounced) * 0.15f);
        if (decay < 0.55f)
            decay = 0.55f;

        DealArtifactWeaponDamage(player, unit, ARTIFACT_WEAPON_WAND, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_WAND, bounced > 0), 1.70f * theme.scalar * decay);
        if (++bounced >= 3)
            break;
    }
}

void HandleArtifactWeaponFalunProc(uint32 spellId, Player* player, Unit* target)
{
    uint8 actId = 0;
    uint8 familyId = 0;
    if (!player || !DecodeArtifactWeaponSpell(spellId, actId, familyId))
        return;

    ArtifactWeaponActThemeConfig const& theme = GetArtifactWeaponActThemeConfig(actId);
    PlayerAbyssProcState& procState = sAbyssCultivationMgr->GetOrCreatePlayerProcState(player->GetGUID().GetCounter());
    ++procState.artifactFalunOrbitCounter;

    DealArtifactWeaponAreaDamage(player, player, ARTIFACT_WEAPON_FALUN, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_FALUN), 1.10f * theme.scalar, 9.0f);
    if (procState.artifactFalunOrbitCounter < 2)
        return;

    procState.artifactFalunOrbitCounter = 0;
    std::list<Unit*> targets = CollectArtifactWeaponTargets(player, player, 12.0f);
    uint8 hitCount = 0;
    for (Unit* unit : targets)
    {
        if (!unit)
            continue;

        DealArtifactWeaponDamage(player, unit, ARTIFACT_WEAPON_FALUN, GetArtifactWeaponSchoolMask(actId, ARTIFACT_WEAPON_FALUN, hitCount > 0), 1.90f * theme.scalar);
        if (++hitCount >= 3)
            break;
    }

    int64 healAmount = ScaleUInt64ToInt64(player->GetMaxHealthForCombat(), 0.06L * static_cast<long double>(theme.scalar));
    player->ModifyHealth(healAmount);
}

void HandleArtifactWeaponProc(uint32 spellId, ArtifactWeaponFamilyId familyId, Unit* casterUnit, Unit* hitUnit, Unit* explicitTarget)
{
    Player* player = casterUnit ? casterUnit->ToPlayer() : nullptr;
    if (!player)
        return;

    Unit* target = ResolveArtifactWeaponPrimaryTarget(player, hitUnit, explicitTarget);
    switch (familyId)
    {
        case ARTIFACT_WEAPON_SWORD: HandleArtifactWeaponSwordProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_AXE: HandleArtifactWeaponAxeProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_HAMMER: HandleArtifactWeaponHammerProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_DAGGER: HandleArtifactWeaponDaggerProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_FIST: HandleArtifactWeaponFistProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_GREATSWORD: HandleArtifactWeaponGreatswordProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_GREATAXE: HandleArtifactWeaponGreataxeProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_GREATHAMMER: HandleArtifactWeaponGreathammerProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_POLEARM: HandleArtifactWeaponPolearmProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_STAFF: HandleArtifactWeaponStaffProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_BOW: HandleArtifactWeaponBowProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_GUN: HandleArtifactWeaponGunProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_CROSSBOW: HandleArtifactWeaponCrossbowProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_WAND: HandleArtifactWeaponWandProc(spellId, player, target); break;
        case ARTIFACT_WEAPON_FALUN: HandleArtifactWeaponFalunProc(spellId, player, target); break;
        default: break;
    }
}

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

        double ap = GetAbyssAttackPower(caster);
        int64 damage = static_cast<int64>(ap * 2.8);

        // 连锁递减：每击杀一个目标，后续目标伤害递减25%
        float reduction = 1.0f - (_killCount * 0.25f);
        if (reduction < 0.25f)
            reduction = 0.25f;
        damage = static_cast<int64>(damage * reduction);

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

        double ap = GetAbyssAttackPower(caster);
        // 每多命中一个目标伤害+20%，最高翻倍
        float scale = 1.0f + (_targetIndex * 0.2f);
        if (scale > 2.0f)
            scale = 2.0f;
        int64 damage = static_cast<int64>(ap * 1.6 * scale);

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
        Aura* aura = GetAura();
        Unit* target = GetTarget();
        Unit* caster = GetCaster();
        if (!aura || !target || !caster)
            return;

        // 只在真正达到满层时引爆，并且先移除 Aura，避免在重入回调里递归自触发。
        if (aura->GetStackAmount() < ABYSS_POISON_MAX_STACKS)
            return;

        aura->Remove();
        caster->CastSpell(target, SPELL_ABYSS_POISON_DETONATE, true);
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

        double sp = GetAbyssSpellPower(caster, SPELL_SCHOOL_MASK_NATURE);
        double ap = GetAbyssAttackPower(caster);
        double power = std::max(sp, ap);
        int64 damage = static_cast<int64>(power * 2.0);

        caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NATURE);

        Unit* explosionTarget = GetExplTargetUnit();
        if (!explosionTarget)
            explosionTarget = target;

        // 只在引爆源目标上执行一次传播逻辑，避免 AOE 命中多个目标时重复扩散。
        if (target != explosionTarget)
            return;

        std::list<Unit*> nearbyTargets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(explosionTarget, caster, ABYSS_POISON_SPREAD_RADIUS);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(explosionTarget, nearbyTargets, check);
        Cell::VisitAllObjects(explosionTarget, searcher, ABYSS_POISON_SPREAD_RADIUS);

        for (Unit* nearbyTarget : nearbyTargets)
        {
            if (!nearbyTarget || nearbyTarget == explosionTarget)
                continue;

            // 传播只补给“当前没有毒层”的目标，避免多个满层目标互相回种形成连锁递归。
            if (nearbyTarget->HasAura(SPELL_ABYSS_POISON_DOT))
                continue;

            for (uint8 i = 0; i < ABYSS_POISON_SPREAD_STACKS; ++i)
                caster->CastSpell(nearbyTarget, SPELL_ABYSS_POISON_DOT, true);
        }
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

        double sp = GetAbyssSpellPower(caster, SPELL_SCHOOL_MASK_FIRE);
        int64 damage = static_cast<int64>(sp * 1.2);
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
// [6] 虚空黑洞 - 周期性暗影伤害+拖拽
// ============================================
class spell_abyss_void_hole : public AuraScript
{
    PrepareAuraScript(spell_abyss_void_hole);

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        double sp = GetAbyssSpellPower(caster, SPELL_SCHOOL_MASK_SHADOW);
        int64 damage = static_cast<int64>(sp * 1.8);
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
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_abyss_void_hole::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
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
        int64 damage = ScaleUInt64ToInt64(target->GetMaxHealthForCombat(), 0.25L * static_cast<long double>(reduction));

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

        double ap = GetAbyssAttackPower(caster);
        int64 damage = static_cast<int64>(ap * 3.0);
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

    bool _processed = false;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (_processed)
            return;

        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        _processed = true;

        std::list<Unit*> nearbyTargets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(target, caster, 8.0f);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(target, nearbyTargets, check);
        Cell::VisitAllObjects(target, searcher, 8.0f);

        double sp = GetAbyssSpellPower(caster, SPELL_SCHOOL_MASK_FIRE);
        int64 damage = static_cast<int64>(sp * 1.5);

        for (Unit* nearbyTarget : nearbyTargets)
        {
            if (!nearbyTarget)
                continue;

            caster->DealDamage(caster, nearbyTarget, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE);
            caster->CastSpell(nearbyTarget, SPELL_ABYSS_LAVA_SLOW, true);
        }
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

        double sp = GetAbyssSpellPower(caster, SPELL_SCHOOL_MASK_ARCANE);
        double ap = GetAbyssAttackPower(caster);
        double power = std::max(sp, ap);
        int64 damage = static_cast<int64>(power * 4.0);
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
    bool _processed = false;

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (_processed)
            return;

        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        _processed = true;

        std::list<Unit*> nearbyTargets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 12.0f);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, nearbyTargets, check);
        Cell::VisitAllObjects(caster, searcher, 12.0f);

        double sp = GetAbyssSpellPower(caster, SPELL_SCHOOL_MASK_SHADOW);
        double ap = GetAbyssAttackPower(caster);
        double power = std::max(sp, ap);
        int64 damage = static_cast<int64>(power * 2.6);

        for (Unit* nearbyTarget : nearbyTargets)
        {
            if (!nearbyTarget)
                continue;

            if (!caster->HasInArc(float(M_PI), nearbyTarget))
                continue;

            caster->DealDamage(caster, nearbyTarget, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW);
            ++_hitCount;
        }

        if (_hitCount == 0)
        {
            caster->DealDamage(caster, target, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW);
            _hitCount = 1;
        }
    }

    void AfterCastHandler()
    {
        // 施法结束后，根据命中数回血
        Unit* caster = GetCaster();
        if (!caster || _hitCount == 0)
            return;

        int64 healAmount = ScaleUInt64ToInt64(caster->GetMaxHealthForCombat(), 0.03L * static_cast<long double>(_hitCount));
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

        double ap = GetAbyssAttackPower(caster);
        int64 damage = static_cast<int64>(ap * 3.0);
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

#define DEFINE_ARTIFACT_WEAPON_SCRIPT(className, familyConst) \
class className : public SpellScript \
{ \
    PrepareSpellScript(className); \
    bool _processed = false; \
    void HandleScript(SpellEffIndex /*effIndex*/) \
    { \
        if (_processed) \
            return; \
        _processed = true; \
        HandleArtifactWeaponProc(GetSpellInfo()->Id, familyConst, GetCaster(), GetHitUnit(), GetExplTargetUnit()); \
    } \
    void Register() override \
    { \
        OnEffectHitTarget += SpellEffectFn(className::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT); \
    } \
};

DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_sword, ARTIFACT_WEAPON_SWORD)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_axe, ARTIFACT_WEAPON_AXE)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_hammer, ARTIFACT_WEAPON_HAMMER)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_dagger, ARTIFACT_WEAPON_DAGGER)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_fist, ARTIFACT_WEAPON_FIST)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_greatsword, ARTIFACT_WEAPON_GREATSWORD)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_greataxe, ARTIFACT_WEAPON_GREATAXE)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_greathammer, ARTIFACT_WEAPON_GREATHAMMER)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_polearm, ARTIFACT_WEAPON_POLEARM)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_staff, ARTIFACT_WEAPON_STAFF)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_bow, ARTIFACT_WEAPON_BOW)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_gun, ARTIFACT_WEAPON_GUN)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_crossbow, ARTIFACT_WEAPON_CROSSBOW)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_wand, ARTIFACT_WEAPON_WAND)
DEFINE_ARTIFACT_WEAPON_SCRIPT(spell_abyss_weapon_falun, ARTIFACT_WEAPON_FALUN)

#undef DEFINE_ARTIFACT_WEAPON_SCRIPT

// ============================================
// [13] 深渊遗物托管法术 - 891xx
// 触发时机由深渊模块负责，具体效果由 spell.dbc + 本脚本统一接管
// ============================================
class spell_abyss_managed_relic : public SpellScript
{
    PrepareSpellScript(spell_abyss_managed_relic);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;

        uint32 spellId = GetSpellInfo()->Id;
        if (!IsManagedRelicSpell(spellId))
            return;

        Unit* explicitTarget = GetHitUnit();
        if (!explicitTarget)
            explicitTarget = GetExplTargetUnit();

        sAbyssCultivationMgr->ExecuteManagedRelicSpell(player, spellId, explicitTarget);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_abyss_managed_relic::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
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
    RegisterSpellScript(spell_abyss_weapon_sword);      // 89401-89476 剑系神器武器
    RegisterSpellScript(spell_abyss_weapon_axe);        // 89402-89477 斧系神器武器
    RegisterSpellScript(spell_abyss_weapon_hammer);     // 89403-89478 锤系神器武器
    RegisterSpellScript(spell_abyss_weapon_dagger);     // 89404-89479 匕首系神器武器
    RegisterSpellScript(spell_abyss_weapon_fist);       // 89405-89480 拳套系神器武器
    RegisterSpellScript(spell_abyss_weapon_greatsword); // 89406-89481 双手剑神器武器
    RegisterSpellScript(spell_abyss_weapon_greataxe);   // 89407-89482 双手斧神器武器
    RegisterSpellScript(spell_abyss_weapon_greathammer);// 89408-89483 双手锤神器武器
    RegisterSpellScript(spell_abyss_weapon_polearm);    // 89409-89484 长柄神器武器
    RegisterSpellScript(spell_abyss_weapon_staff);      // 89410-89485 法杖神器武器
    RegisterSpellScript(spell_abyss_weapon_bow);        // 89411-89486 弓系神器武器
    RegisterSpellScript(spell_abyss_weapon_gun);        // 89412-89487 枪系神器武器
    RegisterSpellScript(spell_abyss_weapon_crossbow);   // 89413-89488 弩系神器武器
    RegisterSpellScript(spell_abyss_weapon_wand);       // 89414-89489 魔杖神器武器
    RegisterSpellScript(spell_abyss_weapon_falun);      // 89415-89490 法轮神器武器
    RegisterSpellScript(spell_abyss_managed_relic);     // 891xx 遗物 / 神器托管技能
}

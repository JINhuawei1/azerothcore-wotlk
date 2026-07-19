#include "ItemIdentificationSystem.h"
#include "IdentificationTemplateGroupResolver.h"
#include "IdentificationScrollPolicy.h"
#include "AddonThrottle.h"
#include "Define.h"
#include "HermesBridgeAddonApi.h"
#include <limits>
#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "StringConvert.h"
#include "Util.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ScriptedGossip.h"
#include "Spell.h"
#include "WorldSessionMgr.h"
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <random>
#include <cstdarg>
#include <thread>
#include <chrono>
#include <set>
#include <iomanip>
#include <algorithm>
#include <cmath>

// 模块集成 - 自动检测可用的模块并定义宏
// 使用 __has_include 检测头文件是否存在，避免依赖CMake宏定义
#if __has_include("ItemGrowthMgr.h")
    #ifndef MODULE_ITEM_GROWTH
        #define MODULE_ITEM_GROWTH
    #endif
    #include "ItemGrowthMgr.h"
    #if __has_include("ItemGrowthCommands.h")
        #include "ItemGrowthCommands.h"
    #endif
#endif

#if __has_include("ItemEnhancementMgr.h")
    #ifndef MODULE_ITEM_ENHANCEMENT
        #define MODULE_ITEM_ENHANCEMENT
    #endif
    #include "ItemEnhancementMgr.h"
#endif

#if __has_include("ItemAttributesGenerator.h")
    #ifndef MODULE_ITEM_ATTRIBUTES
        #define MODULE_ITEM_ATTRIBUTES
    #endif
    #include "ItemAttributesGenerator.h"
    #include "ItemAttributesDBHelper.h"
    #include "ItemAttributesLoader.h"
    #include "ItemAttributesEffects.h"
#endif

#if __has_include("RuneManager.h")
    #ifndef MODULE_RUNE_SYSTEM
        #define MODULE_RUNE_SYSTEM
    #endif
    #include "RuneManager.h"
    #include "RuneApplyManager.h"
    #include "RuneSystem.h"
#endif

#if __has_include("ItemSkillsManager.h")
    #ifndef MODULE_ITEM_SKILLS
        #define MODULE_ITEM_SKILLS
    #endif
    #include "ItemSkillsManager.h"
    #include "ItemSkillsDBHelper.h"
    #include "ItemSkillsEffects.h"
#endif

#if __has_include("MagicHitSystem.h")
    #ifndef MODULE_MAGIC_HIT_SYSTEM
        #define MODULE_MAGIC_HIT_SYSTEM
    #endif
    #include "MagicHitSystem.h"
#endif

#if __has_include("RequirementSystem.h")
    #ifndef MODULE_REQUIREMENT_TEMPLATE
        #define MODULE_REQUIREMENT_TEMPLATE
    #endif
    #include "RequirementSystem.h"
#endif

namespace
{
    constexpr char const* ITEM_IDENTIFICATION_ADDON_PREFIX = "UITQ";

    void SendItemIdentificationAddonMessage(Player* player, std::string const& payload)
    {
        if (!player)
            return;

        if (HermesBridge_SendAddonMessage(player, ITEM_IDENTIFICATION_ADDON_PREFIX, payload))
            return;

        std::string fullMessage = std::string(ITEM_IDENTIFICATION_ADDON_PREFIX) + "\t" + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }

    char NormalizeHuanJingMode(char mode)
    {
        if (mode == '+' || mode == 1)
            return '+';
        if (mode == '-' || mode == 'n')
            return '-';
        return 'x';
    }

    char DbValueToHuanJingMode(int32 value)
    {
        if (value == 1)
            return '+';
        if (value == -1)
            return '-';
        return 'x';
    }

    bool HasHuanJingEffect(uint256 const& value, char mode)
    {
        mode = NormalizeHuanJingMode(mode);
        if (mode == '-')
            return false;
        return mode == '+' ? value > 0 : value > 1;
    }

    std::string FormatTemplateDouble(double value)
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << value;
        return oss.str();
    }

    std::string BuildTemplateStatsData(uint32 itemID)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemID);
        if (!proto)
            return "";

        std::ostringstream stats;
        bool hasStats = false;

        for (uint32 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        {
            int256 statValue = proto->ItemStatValue256[i];
            if (!statValue)
                continue;

            if (hasStats)
                stats << ",";

            hasStats = true;
            stats << proto->ItemStat[i].ItemStatType << " " << statValue.convert_to<std::string>();
        }

        std::ostringstream damage;
        bool hasDamage = false;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            if (proto->Damage[i].DamageMin == 0.0 && proto->Damage[i].DamageMax == 0.0)
                continue;

            if (hasDamage)
                damage << ",";

            hasDamage = true;
            damage << FormatTemplateDouble(proto->Damage[i].DamageMin) << " "
                   << FormatTemplateDouble(proto->Damage[i].DamageMax) << " "
                   << proto->Damage[i].DamageType;
        }

        if (!hasStats && proto->Armor256 == 0 && !hasDamage)
            return "";

        std::ostringstream data;
        data << "TPL64|" << stats.str() << "|";
        if (proto->Armor256 != 0)
            data << proto->Armor256.convert_to<std::string>();
        data << "|" << damage.str();

        return data.str();
    }

    struct ClientItemLocation
    {
        int32 bag = 0;
        uint8 slot = 0;
        bool valid = false;
    };

    ClientItemLocation GetClientItemLocation(Item* item)
    {
        ClientItemLocation location;
        if (!item)
            return location;

        uint8 serverBag = item->GetBagSlot();
        uint8 serverSlot = item->GetSlot();

        if (serverBag == INVENTORY_SLOT_BAG_0)
        {
            if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
            {
                location.bag = 255;
                location.slot = static_cast<uint8>(serverSlot + 1);
                location.valid = true;
            }
            else if (serverSlot >= INVENTORY_SLOT_ITEM_START && serverSlot < INVENTORY_SLOT_ITEM_END)
            {
                location.bag = 0;
                location.slot = static_cast<uint8>(serverSlot - INVENTORY_SLOT_ITEM_START + 1);
                location.valid = true;
            }
        }
        else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
        {
            location.bag = static_cast<int32>(serverBag - INVENTORY_SLOT_BAG_START + 1);
            location.slot = static_cast<uint8>(serverSlot + 1);
            location.valid = true;
        }

        return location;
    }

    Item* FindPlayerItemByGuidForAddon(Player* player, uint32 itemGuid)
    {
        if (!player || itemGuid == 0)
            return nullptr;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (!bag)
                continue;

            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                Item* item = bag->GetItemByPos(slot);
                if (item && item->GetGUID().GetCounter() == itemGuid)
                    return item;
            }
        }

        return nullptr;
    }

    std::unordered_map<uint32, Item*> BuildPlayerItemGuidIndex(Player* player)
    {
        std::unordered_map<uint32, Item*> itemsByGuid;
        if (!player)
            return itemsByGuid;

        itemsByGuid.reserve(128);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                itemsByGuid[item->GetGUID().GetCounter()] = item;
        }

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                itemsByGuid[item->GetGUID().GetCounter()] = item;
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (!bag)
                continue;

            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                if (Item* item = bag->GetItemByPos(slot))
                    itemsByGuid[item->GetGUID().GetCounter()] = item;
            }
        }

        return itemsByGuid;
    }

    void QueueAllModuleDataAddonRefresh(Player* player, uint32 itemID, uint32 guid, Milliseconds delay)
    {
        if (!player || itemID == 0 || guid == 0)
            return;

        ObjectGuid playerGuid = player->GetGUID();
        // 批量鉴定会在同一帧为多件物品排队。旧逻辑全部固定延迟2秒，导致
        // ALL_MODULE_DATA 瞬间突发，Hermes/客户端消息队列只能收到其中一部分。
        // 按玩家将同一批次错开350ms发送；超过1秒没有新任务则视为新批次。
        static std::mutex refreshQueueMutex;
        static std::unordered_map<uint32, std::pair<std::chrono::steady_clock::time_point, uint32>> refreshQueueState;
        uint32 staggerIndex = 0;
        {
            std::lock_guard<std::mutex> lock(refreshQueueMutex);
            auto const now = std::chrono::steady_clock::now();
            auto& state = refreshQueueState[playerGuid.GetCounter()];
            if (state.first.time_since_epoch().count() == 0 || now - state.first > std::chrono::seconds(1))
                state.second = 0;
            else
                ++state.second;
            state.first = now;
            staggerIndex = state.second;
        }
        delay += Milliseconds(staggerIndex * 350);

        player->m_Events.AddEventAtOffset([playerGuid, itemID, guid]()
        {
            Player* player = ObjectAccessor::FindPlayer(playerGuid);
            if (player && player->IsInWorld())
                sItemIdentificationSystem->SendAllModuleDataAddon(player, itemID, guid);
        }, delay);
    }
}

#if __has_include("ItemSets.h")
    #ifndef MODULE_ITEM_SETS
        #define MODULE_ITEM_SETS
    #endif
    #include "ItemSets.h"
#endif

#if __has_include("HuanJingSystem.h")
    #ifndef MODULE_HUANJING_SYSTEM
        #define MODULE_HUANJING_SYSTEM
    #endif
    #include "HuanJingSystem.h"
#endif

// 前向声明辅助函数
std::vector<uint32> ParseCommaSeparatedNumbers(const std::string& str);
uint32 SelectRandomFromList(const std::vector<uint32>& list);
uint32 GenerateRandomNumber(uint32 min, uint32 max);
uint256 GenerateRandomUInt256(uint256 min, uint256 max);
uint32 GetIdentificationTemplateMatchLevel(Item* item);

// 单例实例
ItemIdentificationSystem* ItemIdentificationSystem::_instance = nullptr;

// 获取单例实例
ItemIdentificationSystem* ItemIdentificationSystem::instance()
{
    if (!_instance)
        _instance = new ItemIdentificationSystem();
    return _instance;
}

// 初始化系统
void ItemIdentificationSystem::Initialize()
{
    PerformanceTimer timer("模块初始化");

    // 加载配置（在服务器完全启动后执行）
    LoadConfig(false);

    // 初始化已鉴定物品缓存
    InitializeCache();

    // 加载鉴定模板（在服务器完全启动后执行）
    LoadIdentificationTemplates();

    // 加载鉴定卷轴配置
    LoadIdentificationScrollConfigs();

    // 显示模块初始化信息
    if (_enabled)
    {

    }
    else
    {
        LOG_INFO("server.loading", "物品鉴定系统: 已禁用");
    }
}

// 加载配置
void ItemIdentificationSystem::LoadConfig(bool reload)
{
    _enabled = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.Enable", true);
    _baseSuccessRate = sConfigMgr->GetOption<uint32>("ItemIdentificationSystem.BaseSuccessRate", 100);
    _destroyOnFail = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.DestroyOnFail", false);
    _cost = sConfigMgr->GetOption<uint64>("ItemIdentificationSystem.Cost", 10000);
    _enableAnnounce = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.EnableAnnounce", true);
    _debugMode = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.Debug", false);

    if (reload)
    {
        LoadIdentificationTemplates();
        LoadIdentificationScrollConfigs();
#ifdef MODULE_ITEM_ATTRIBUTES
        sItemAttributesLoaderUnsafe->LoadItemAttributeTemplates();
#endif
        LOG_INFO("server.loading", "物品鉴定系统配置已重新加载");
    }
}

// 鉴定模板结构
struct IdentificationTemplate
{
    uint32 id;
    uint32 group;
    uint32 level;
    uint32 randomChance;
    std::string comment;

    // 模块关联字段
    std::string itemGrowthGroups;
    uint32 growthAttrMinCount;
    uint32 growthAttrMaxCount;
    uint256 growthAttrMinValue;
    uint256 growthAttrMaxValue;

    std::string itemEnhancementGroups;
    uint32 enhancementAttrMinCount;
    uint32 enhancementAttrMaxCount;
    uint256 enhancementAttrMinValue;
    uint256 enhancementAttrMaxValue;

    std::string itemAttributesGroups;
    std::string itemAttributesAdditionalGroups;
    std::string itemSkillsGroups;
    std::string runeSystemGroups;
    std::string skillSetGroups;
    uint32 requirementTemplate;

    // 基础属性配置
    uint32 baseAttrMinCount;
    uint32 baseAttrMaxCount;
    uint256 baseAttrMinValue;
    uint256 baseAttrMaxValue;
    bool baseAttrAllowDuplicate;

    // 追加属性配置
    uint32 additionalAttrMinCount;
    uint32 additionalAttrMaxCount;
    uint256 additionalAttrMinValue;
    uint256 additionalAttrMaxValue;
    bool additionalAttrAllowDuplicate;

    // 追加技能配置
    uint32 additionalSkillMinCount;
    uint32 additionalSkillMaxCount;
    bool additionalSkillAllowDuplicate;

    // 技能魔次配置
    std::string magicHitGroups;
    uint32 magicHitMinCount;
    uint32 magicHitMaxCount;
    uint32 magicHitMinValue;
    uint32 magicHitMaxValue;
    bool magicHitAllowDuplicate;

    // 符文配置
    uint32 runeSlotMinCount;
    uint32 runeSlotMaxCount;

    uint32 announcementTemplate;
    std::string qualityColor;
    std::string itemNamePrefix;
    std::string itemNameSuffix;
    std::string itemNameColors;
    std::string itemBottomDescription;

    // 公式化扩展字段（数据驱动，不硬编码行数/系数/模式）
    double baseAttrPerLevelInc = 0.0;        // 基础每级增量：幻境等级每+1，基础属性百分比增加量
    double additionalAttrPerLevelInc = 0.0;  // 追加每级增量
    uint8 attrCalcMode = 0;                  // 属性计算模式：0=绝对值，1=官方百分比（取代<=300硬编码判断）
    uint8 formulaType = 0;                   // 公式类型：0=线性(基础+等级*增量)，1=指数(预留)
};

static std::string UrlEncodeAddonField(const std::string& value)
{
    static char const* hex = "0123456789ABCDEF";

    if (value.empty())
        return "";

    std::string encoded;
    encoded.reserve(value.size() * 3);

    for (unsigned char ch : value)
    {
        if ((ch >= '0' && ch <= '9') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == ' ')
        {
            encoded.push_back(static_cast<char>(ch));
        }
        else
        {
            encoded.push_back('%');
            encoded.push_back(hex[(ch >> 4) & 0x0F]);
            encoded.push_back(hex[ch & 0x0F]);
        }
    }

    return encoded;
}

static std::string BuildIdentificationDisplayData(const IdentificationTemplate& tmpl)
{
    if (tmpl.qualityColor.empty() && tmpl.itemNamePrefix.empty() && tmpl.itemNameSuffix.empty() &&
        tmpl.itemNameColors.empty() && tmpl.itemBottomDescription.empty())
    {
        return "";
    }

    std::ostringstream stream;
    stream << "IDDISP|"
           << UrlEncodeAddonField(tmpl.qualityColor) << "|"
           << UrlEncodeAddonField(tmpl.itemNamePrefix) << "|"
           << UrlEncodeAddonField(tmpl.itemNameSuffix) << "|"
           << UrlEncodeAddonField(tmpl.itemNameColors) << "|"
           << UrlEncodeAddonField(tmpl.itemBottomDescription);
    return stream.str();
}

// 存储所有鉴定模板
std::map<uint32, IdentificationTemplate> _identificationTemplates;

// 从数据库加载鉴定模板
void ItemIdentificationSystem::LoadIdentificationTemplates()
{
    PerformanceTimer timer("加载鉴定模板");

    DebugLog("正在加载物品鉴定模板...");
    _identificationTemplates.clear();

    // 使用明确的字段名而不是SELECT *，避免字段顺序问题
    QueryResult result = WorldDatabase.Query(
        "SELECT `注释`, `id`, `组`, `等级`, `随机几率`, "
        "`物品成长_系统`, `成长属性最小数量`, `成长属性最大数量`, `成长属性最小属性值`, `成长属性最大属性值`, "
        "`物品强化_系统`, `强化属性最小数量`, `强化属性最大数量`, `强化属性最小属性值`, `强化属性最大属性值`, "
        "`物品属性_模板`, "
        "`基础属性最小数量`, `基础属性最大数量`, `基础最小属性值`, `基础最大属性值`, `基础属性允许重复`, "
        "`物品属性_模板_组`, `追加属性最小数量`, `追加属性最大数量`, `追加属性最小值`, `追加属性最大值`, `追加属性允许重复`, "
        "`物品技能_模板_组`, `追加技能最小数量`, `追加技能最大数量`, `追加技能允许重复`, "
        "`技能魔次_模板_组`, `技能魔次最小数量`, `技能魔次最大数量`, `技能魔次最小魔次`, `技能魔次最大魔次`, `技能魔次允许重复`, "
        "`需求_模板`, `符文系统_符文`, `符文凹槽最小数量`, `符文凹槽最大数量`, `技能模板_套装_组`, `公告模板`, "
        "`品质颜色`, `物品名字前缀`, `物品名字后缀`, `物品名字颜色_多个逗号隔开`, `物品底部描述`, "
        "`基础每级增量`, `追加每级增量`, `属性计算模式`, `公式类型` "
        "FROM `_物品鉴定_模板`");

    if (!result)
    {
       // LOG_WARN("server.loading", "物品鉴定系统: 未找到鉴定模板数据");
        return;
    }

    uint32 count = 0;
    std::set<uint32> groups;

    do
    {
        Field* fields = result->Fetch();
        IdentificationTemplate tmpl;

        // 字段顺序与SELECT语句中的顺序完全一致
        // 0:注释, 1:id, 2:组, 3:等级, 4:随机几率,
        // 5:物品成长_系统, 6:成长属性最小数量, 7:成长属性最大数量, 8:成长属性最小属性值, 9:成长属性最大属性值,
        // 10:物品强化_系统, 11:强化属性最小数量, 12:强化属性最大数量, 13:强化属性最小属性值, 14:强化属性最大属性值,
        // 15:物品属性_模板,
        // 16:基础属性最小数量, 17:基础属性最大数量, 18:基础最小属性值, 19:基础最大属性值, 20:基础属性允许重复,
        // 21:物品属性_模板_组, 22:追加属性最小数量, 23:追加属性最大数量, 24:追加属性最小值, 25:追加属性最大值, 26:追加属性允许重复,
        // 27:物品技能_模板_组, 28:追加技能最小数量, 29:追加技能最大数量, 30:追加技能允许重复,
        // 31:技能魔次_模板_组, 32:技能魔次最小数量, 33:技能魔次最大数量, 34:技能魔次最小魔次, 35:技能魔次最大魔次, 36:技能魔次允许重复,
        // 37:需求_模板, 38:符文系统_符文, 39:符文凹槽最小数量, 40:符文凹槽最大数量, 41:技能模板_套装_组, 42:公告模板,
        // 43:品质颜色, 44:物品名字前缀, 45:物品名字后缀, 46:物品名字颜色_多个逗号隔开, 47:物品底部描述

        tmpl.comment = fields[0].Get<std::string>();
        tmpl.id = fields[1].Get<uint32>();
        tmpl.group = fields[2].Get<uint32>();
        tmpl.level = fields[3].Get<uint32>();
        tmpl.randomChance = fields[4].Get<uint32>();

        // 模块关联字段
        tmpl.itemGrowthGroups      = fields[5].Get<std::string>();              // 物品成长_系统
        tmpl.growthAttrMinCount    = fields[6].Get<uint32>();                   // 成长属性最小数量
        tmpl.growthAttrMaxCount    = fields[7].Get<uint32>();                   // 成长属性最大数量
        tmpl.growthAttrMinValue    = fields[8].Get<uint256>();                  // 成长属性最小属性值
        tmpl.growthAttrMaxValue    = fields[9].Get<uint256>();                  // 成长属性最大属性值

        tmpl.itemEnhancementGroups   = fields[10].Get<std::string>();           // 物品强化_系统
        tmpl.enhancementAttrMinCount = fields[11].Get<uint32>();                // 强化属性最小数量
        tmpl.enhancementAttrMaxCount = fields[12].Get<uint32>();                // 强化属性最大数量
        tmpl.enhancementAttrMinValue = fields[13].Get<uint256>();               // 强化属性最小属性值
        tmpl.enhancementAttrMaxValue = fields[14].Get<uint256>();               // 强化属性最大属性值

        tmpl.itemAttributesGroups = fields[15].Get<std::string>();              // 物品属性_模板（基础属性）

        // 基础属性配置
        tmpl.baseAttrMinCount = fields[16].Get<uint32>();                       // 基础属性最小数量
        tmpl.baseAttrMaxCount = fields[17].Get<uint32>();                       // 基础属性最大数量
        tmpl.baseAttrMinValue = fields[18].Get<uint256>();                      // 基础最小属性值
        tmpl.baseAttrMaxValue = fields[19].Get<uint256>();                      // 基础最大属性值
        tmpl.baseAttrAllowDuplicate = fields[20].Get<uint32>() == 0;           // 基础属性允许重复

        // 追加属性配置
        tmpl.itemAttributesAdditionalGroups = fields[21].Get<std::string>();   // 物品属性_模板_组（追加属性）
        tmpl.additionalAttrMinCount = fields[22].Get<uint32>();                // 追加属性最小数量
        tmpl.additionalAttrMaxCount = fields[23].Get<uint32>();                // 追加属性最大数量
        tmpl.additionalAttrMinValue = fields[24].Get<uint256>();               // 追加属性最小值
        tmpl.additionalAttrMaxValue = fields[25].Get<uint256>();               // 追加属性最大值
        tmpl.additionalAttrAllowDuplicate = fields[26].Get<uint32>() == 0;     // 追加属性允许重复

        // 追加技能配置
        tmpl.itemSkillsGroups = fields[27].Get<std::string>();                 // 物品技能_模板_组
        tmpl.additionalSkillMinCount = fields[28].Get<uint32>();               // 追加技能最小数量
        tmpl.additionalSkillMaxCount = fields[29].Get<uint32>();               // 追加技能最大数量
        tmpl.additionalSkillAllowDuplicate = fields[30].Get<uint32>() == 0;    // 追加技能允许重复

        // 技能魔次配置
        tmpl.magicHitGroups = fields[31].Get<std::string>();                   // 技能魔次_模板_组
        tmpl.magicHitMinCount = fields[32].Get<uint32>();                      // 技能魔次最小数量
        tmpl.magicHitMaxCount = fields[33].Get<uint32>();                      // 技能魔次最大数量
        tmpl.magicHitMinValue = fields[34].Get<uint32>();                      // 技能魔次最小魔次
        tmpl.magicHitMaxValue = fields[35].Get<uint32>();                      // 技能魔次最大魔次
        tmpl.magicHitAllowDuplicate = fields[36].Get<uint32>() == 0;           // 技能魔次允许重复

        // 其他配置
        tmpl.requirementTemplate = fields[37].Get<uint32>();                   // 需求_模板

        // 符文配置
        tmpl.runeSystemGroups = fields[38].Get<std::string>();                 // 符文系统_符文
        tmpl.runeSlotMinCount = fields[39].Get<uint32>();                      // 符文凹槽最小数量
        tmpl.runeSlotMaxCount = fields[40].Get<uint32>();                      // 符文凹槽最大数量

        // 套装配置
        tmpl.skillSetGroups = fields[41].Get<std::string>();                   // 技能模板_套装_组

        // 公告配置
        tmpl.announcementTemplate = fields[42].Get<uint32>();                  // 公告模板

        // 名称显示配置
        tmpl.qualityColor = fields[43].Get<std::string>();                     // 品质颜色
        tmpl.itemNamePrefix = fields[44].Get<std::string>();                   // 物品名字前缀
        tmpl.itemNameSuffix = fields[45].Get<std::string>();                   // 物品名字后缀
        tmpl.itemNameColors = fields[46].Get<std::string>();                   // 物品名字颜色_多个逗号隔开
        tmpl.itemBottomDescription = fields[47].Get<std::string>();            // 物品底部描述

        // 公式化扩展字段(48-51)
        tmpl.baseAttrPerLevelInc       = fields[48].Get<double>();  // 基础每级增量
        tmpl.additionalAttrPerLevelInc = fields[49].Get<double>();  // 追加每级增量
        tmpl.attrCalcMode              = fields[50].Get<uint8>();   // 属性计算模式
        tmpl.formulaType               = fields[51].Get<uint8>();   // 公式类型

        _identificationTemplates[tmpl.id] = tmpl;
        groups.insert(tmpl.group);
        count++;

        if (_debugMode)
        {
            DebugLog("加载鉴定模板: ID={}, 组={}, 等级={}, 注释='{}'",
                     tmpl.id, tmpl.group, tmpl.level, tmpl.comment);
        }

    } while (result->NextRow());
}

void ItemIdentificationSystem::LoadIdentificationScrollConfigs()
{
    _identificationScrollConfigs.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT `卷轴物品ID`, `卷轴类型`, `倍率`, `鉴定组ID` FROM `_物品鉴定_卷轴配置`");

    if (!result)
    {
        LOG_INFO("server.loading", "物品鉴定系统: 未加载到鉴定卷轴配置");
        return;
    }

    uint32 loadedCount = 0;
    do
    {
        Field* fields = result->Fetch();
        IdentificationScrollConfig config;
        config.itemEntry = fields[0].Get<uint32>();

        uint8 scrollType = fields[1].Get<uint8>();
        if (scrollType != static_cast<uint8>(IdentificationScrollType::Identify) &&
            scrollType != static_cast<uint8>(IdentificationScrollType::Cleanup))
        {
            LOG_WARN("server.loading", "物品鉴定系统: 卷轴物品ID={} 的卷轴类型={}无效，已跳过",
                config.itemEntry, scrollType);
            continue;
        }

        config.type = static_cast<IdentificationScrollType>(scrollType);
        config.multiplier = fields[2].Get<uint256>();
        config.identificationGroupId = fields[3].Get<uint32>();

        if (config.type == IdentificationScrollType::Identify &&
            (config.multiplier <= 1 || config.identificationGroupId == 0))
        {
            LOG_WARN("server.loading",
                "物品鉴定系统: 鉴定卷轴物品ID={} 配置无效（倍率必须大于1且鉴定组ID不能为0），已跳过",
                config.itemEntry);
            continue;
        }

        if (config.type == IdentificationScrollType::Identify)
        {
            bool groupExists = std::any_of(
                _identificationTemplates.begin(),
                _identificationTemplates.end(),
                [&config](auto const& entry)
                {
                    return entry.second.group == config.identificationGroupId;
                });

            if (!groupExists)
            {
                LOG_WARN("server.loading",
                    "物品鉴定系统: 鉴定卷轴物品ID={} 绑定的鉴定组ID={}不存在，已跳过",
                    config.itemEntry, config.identificationGroupId);
                continue;
            }
        }

        _identificationScrollConfigs[config.itemEntry] = std::move(config);
        ++loadedCount;
    } while (result->NextRow());

    LOG_INFO("server.loading", "物品鉴定系统: 已加载 {} 条鉴定/清理卷轴配置", loadedCount);
}

// 检查物品是否可以鉴定
bool ItemIdentificationSystem::CanIdentify(Player* player, Item* item, bool sendError)
{
    if (!player || !item)
    {
        DebugLog("CanIdentify检查失败: 玩家或物品为空");
        return false;
    }

    if (!_enabled)
    {
        if (sendError)
            ChatHandler(player->GetSession()).SendNotification("物品鉴定系统当前已禁用");
        DebugLog("CanIdentify检查失败: 系统已禁用");
        return false;
    }
    
    DebugLog("开始检查物品是否可鉴定: 物品ID={}, 物品GUID={}", item->GetEntry(), item->GetGUID().GetCounter());

    // 检查物品是否已绑定（改为允许绑定物品鉴定）
    // if (item->IsSoulBound())
    // {
    //     LOG_INFO("module", "【鉴定系统】检查失败：物品已绑定");
    //     if (sendError)
    //         ChatHandler(player->GetSession()).SendNotification("已绑定的物品无法鉴定");
    //     return false;
    // }

    // 检查物品是否可装备
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        DebugLog("CanIdentify检查失败: 物品模板为空");
        return false;
    }

    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
    {
        if (sendError)
            ChatHandler(player->GetSession()).SendNotification("只有武器和护甲可以鉴定");
        DebugLog("CanIdentify检查失败: 物品类型不符 (类型={})", proto->Class);
        return false;
    }

    DebugLog("物品类型检查通过 (类型: {}, 名称: {})", proto->Class, proto->Name1);

    // 检查玩家金币是否足够
    int256 playerMoney = player->GetMoney();

    if (playerMoney < _cost)
    {
        if (sendError)
            ChatHandler(player->GetSession()).SendNotification("你没有足够的金币进行鉴定");
        DebugLog("CanIdentify检查失败: 金币不足 (拥有: {}, 需要: {})", Acore::ToString(playerMoney), _cost);
        return false;
    }

    DebugLog("物品可以鉴定: 物品ID={}, GUID={}, 玩家金币={}", item->GetEntry(), item->GetGUID().GetCounter(), Acore::ToString(playerMoney));
    return true;
}

// 获取鉴定成功率
uint32 ItemIdentificationSystem::GetSuccessRate(Player* player, Item* item)
{
    // 直接使用配置文件中的基础成功率，不做任何调整
    uint32 successRate = _baseSuccessRate;

    // 【审计修复】移除强制下限1%的限制，允许配置为0%
    // 确保成功率在合理范围内（0-100%）
    if (successRate > 100)
        successRate = 100;

    return successRate;
}

// 鉴定物品（需要指定组ID）
bool ItemIdentificationSystem::IdentifyItem(Player* player, Item* item, uint32 groupId)
{
    if (!CanIdentify(player, item))
        return false;

    // 【修复】先不扣费，等所有校验和操作成功后再扣费
    // 保存当前金币数（用于验证）
    uint64 costToDeduct = _cost;

    // 计算成功率
    uint32 successRate = GetSuccessRate(player, item);

    // ✅ 使用全局随机数生成器
    std::uniform_int_distribution<uint32> dis(1, 100);
    uint32 roll = dis(GetRandomGenerator());
    bool success = roll <= successRate;

    if (success)
    {
        // 鉴定成功
        bool applyResult = ApplyIdentification(player, item, groupId);

        if (applyResult)
        {
            // 鉴定成功只发送“需要刷新”的轻量通知，数据仍由客户端悬停时按需查询。
            SendItemIdentificationAddonMessage(player,
                Acore::StringFormat("IDENTIFY_REFRESH:{}:{}", item->GetEntry(), item->GetGUID().GetCounter()));

            // 【修复】应用成功后才扣除金币
            if (costToDeduct > static_cast<uint64>(std::numeric_limits<int64>::max()))
                player->SetMoney(player->GetMoney() > costToDeduct ? player->GetMoney() - costToDeduct : 0);
            else
                player->ModifyMoney(-static_cast<int64>(costToDeduct));
            ChatHandler(player->GetSession()).SendNotification("已扣除 {} 铜币用于鉴定", costToDeduct);
            ChatHandler(player->GetSession()).SendNotification("物品鉴定成功！");

            // 【审计修复】实现全服公告功能
            if (_enableAnnounce)
            {
                // 获取物品模板和名称
                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item->GetEntry());
                if (itemTemplate)
                {
                    std::string itemName = itemTemplate->Name1;
                    std::string playerName = player->GetName();

                    // 构建公告消息
                    std::string announcement = Acore::StringFormat(
                        "|cff00ff00【鉴定公告】|r玩家 |cffffffff{}|r 成功鉴定了 |cff1eff00[{}]|r！",
                        playerName, itemName
                    );

                    // 发送全服公告
                    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, announcement.c_str());
                }
            }

            return true;
        }
        else
        {
            // 应用鉴定失败，【修复】不扣费
            ChatHandler(player->GetSession()).SendSysMessage("鉴定失败：无法应用鉴定效果（未扣费）");
            return false;
        }
    }
    else
    {
        // 鉴定失败（掷骰失败，这是正常游戏机制，需要扣费）
        if (costToDeduct > static_cast<uint64>(std::numeric_limits<int64>::max()))
            player->SetMoney(player->GetMoney() > costToDeduct ? player->GetMoney() - costToDeduct : 0);
        else
            player->ModifyMoney(-static_cast<int64>(costToDeduct));
        ChatHandler(player->GetSession()).SendNotification("已扣除 {} 铜币用于鉴定", costToDeduct);
        ChatHandler(player->GetSession()).SendNotification("鉴定失败！");

        // 是否销毁物品
        if (_destroyOnFail)
        {
            player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            ChatHandler(player->GetSession()).SendNotification("物品已被销毁");
        }

        return false;
    }
}

// 根据组ID随机选择一个鉴定模板（根据几率加权）- 优化版：使用内存数据
uint32 ItemIdentificationSystem::SelectIdentificationTemplate(uint32 groupId, Item* /*item*/)
{
    // ✅ 从内存中筛选指定组ID的模板（不查询数据库）
    // 注意：这里不能用物品品质过滤模板等级，否则同品质物品会只剩一个候选模板，随机几率失效。
    std::vector<std::pair<uint32, uint32>> templates; // <模板ID, 几率>
    uint32 totalChance = 0;

    std::set<uint32> availableGroups;
    for (auto const& pair : _identificationTemplates)
        availableGroups.insert(pair.second.group);

    uint32 const resolvedGroupId = ResolveIdentificationTemplateGroup(groupId, availableGroups);
    if (resolvedGroupId != groupId)
    {
        LOG_INFO("server.loading",
            "[鉴定模板选择] 请求组ID={} 无精确模板，已回退通用公式组={}；属性将按玩家幻境等级动态计算",
            groupId, resolvedGroupId);
    }

    for (const auto& pair : _identificationTemplates)
    {
        const IdentificationTemplate& tmpl = pair.second;
        if (tmpl.group == resolvedGroupId)
        {
            templates.push_back(std::make_pair(tmpl.id, tmpl.randomChance));
            totalChance += tmpl.randomChance;
        }
    }

    if (templates.empty() || totalChance == 0)
    {
        LOG_WARN("module.itemidentification", "[鉴定模板选择] 未找到组ID为 {} 的鉴定模板!", groupId);
        return 0;
    }

    // ✅ 使用全局随机数生成器（避免重复创建）
    std::uniform_int_distribution<uint32> dis(1, totalChance);
    uint32 roll = dis(GetRandomGenerator());

    uint32 currentChance = 0;
    for (const auto& pair : templates)
    {
        currentChance += pair.second;
        if (roll <= currentChance)
        {
            return pair.first;
        }
    }

    // 如果出现问题，返回第一个模板
    LOG_WARN("module.itemidentification", "[鉴定模板选择] 未命中任何模板，返回第一个: ID={}", templates[0].first);
    return templates[0].first;
}

// 应用鉴定结果到物品（需要指定组ID）
bool ItemIdentificationSystem::ApplyIdentification(Player* player, Item* item, uint32 groupId)
{
    return ApplyIdentificationInternal(player, item, groupId, _cost, GetSuccessRate(player, item));
}

bool ItemIdentificationSystem::ApplyIdentificationInternal(
    Player* player,
    Item* item,
    uint32 groupId,
    uint64 recordedCost,
    uint32 recordedSuccessRate)
{
    // 获取选择的模板ID
    uint32 templateId = SelectIdentificationTemplate(groupId, item);
    if (!templateId || _identificationTemplates.find(templateId) == _identificationTemplates.end())
        return false;

    const IdentificationTemplate& tmpl = _identificationTemplates[templateId];

    // 0. 检查物品是否已鉴定（防止重复鉴定）
    uint32 itemPropertySeed = item->GetGUID().GetCounter();
    uint32 itemGuid = itemPropertySeed;  // 使用 PROPERTY_SEED 作为唯一标识
    if (IsItemIdentified(itemGuid))
    {
        DebugLog("物品已经被鉴定过了: GUID={}", itemGuid);
        ChatHandler(player->GetSession()).SendSysMessage("此物品已经鉴定过了");
        return false;
    }
    


    // 1. 检查需求条件
    auto reqStart = std::chrono::high_resolution_clock::now();
    if (!CheckRequirements(player, item, tmpl))
    {
        DebugLog("玩家不满足鉴定需求条件");
        return false;
    }
    


    // 预先绑定物品，使其满足强化系统等模块的需求
    item->SetBinding(true);


    // 创建鉴定记录
    ItemIdentificationRecord record;
    record.playerGuid = player->GetGUID().GetCounter();
    record.itemGuid = itemGuid;
    record.itemEntry = item->GetEntry();
    record.templateId = templateId;
    record.costGold = recordedCost;
    record.successRate = recordedSuccessRate;
    
    // 初始化所有标记为false，所有数值字段为0
    record.hasGrowth = false;
    record.growthGroup = 0;
    record.hasEnhancement = false;
    record.enhancementGroup = 0;
    record.hasBaseAttributes = false;
    record.baseAttrCount = 0;
    record.baseAttrGroup = 0;
    record.baseAttrDetails = "";
    record.hasAdditionalAttributes = false;
    record.additionalAttrCount = 0;
    record.additionalAttrGroups = "";
    record.hasRuneSlots = false;
    record.runeSlotCount = 0;
    record.hasSkills = false;
    record.skillGroups = "";
    record.hasMagicHits = false;
    record.magicHitCount = 0;
    record.magicHitGroups = "";
    record.hasSet = false;
    record.setGroup = 0;
    record.setId = 0;
    


    // 2. 应用物品成长系统

    if (!tmpl.itemGrowthGroups.empty())
    {
        uint32 appliedGroup = ApplyItemGrowth(player, item, tmpl);

        if (appliedGroup > 0)
        {
            record.hasGrowth = true;
            record.growthGroup = appliedGroup; // 使用实际应用的组号

        }
        else
        {

        }
    }
    else
    {

    }

    // 3. 应用物品强化系统

    if (!tmpl.itemEnhancementGroups.empty())
    {
        uint32 appliedGroup = ApplyItemEnhancement(player, item, tmpl);

        if (appliedGroup > 0)
        {
            record.hasEnhancement = true;
            record.enhancementGroup = appliedGroup; // 使用实际应用的组号

        }
        else
        {

        }
    }
    else
    {

    }

    // 4. 应用基础属性

    if (!tmpl.itemAttributesGroups.empty() && tmpl.baseAttrMaxCount > 0)
    {
        std::string attrDetails;
        uint32 attrCount = 0;
        uint32 attrGroup = 0;

        ApplyBaseAttributes(player, item, tmpl, attrDetails, attrCount, attrGroup);

        // 只要attrCount > 0就认为应用成功（即使无法读取详情）
        if (attrCount > 0)
        {
            record.hasBaseAttributes = true;
            record.baseAttrCount = attrCount;
            record.baseAttrGroup = attrGroup;
            record.baseAttrDetails = attrDetails; // 可能为空，但不影响记录

        }
        else
        {

        }
    }
    else
    {

    }

    // 5. 应用追加属性

    if (!tmpl.itemAttributesAdditionalGroups.empty() && tmpl.additionalAttrMaxCount > 0)
    {
        uint32 appliedAdditionalCount = ApplyAdditionalAttributes(player, item, tmpl);
        if (appliedAdditionalCount > 0)
        {
            record.hasAdditionalAttributes = true;
            record.additionalAttrCount = appliedAdditionalCount;
            record.additionalAttrGroups = tmpl.itemAttributesAdditionalGroups;
        }

    }
    else
    {

    }

    // 6. 应用追加技能

    if (!tmpl.itemSkillsGroups.empty() && tmpl.additionalSkillMaxCount > 0)
    {
        ApplyAdditionalSkills(player, item, tmpl);
        record.hasSkills = true;
        record.skillGroups = tmpl.itemSkillsGroups;

    }
    else
    {

    }

    // 6.5. 应用技能魔次

    if (!tmpl.magicHitGroups.empty() && tmpl.magicHitMaxCount > 0)
    {
        ApplyMagicHits(player, item, tmpl);
        record.hasMagicHits = true;
        record.magicHitCount = GenerateRandomNumber(tmpl.magicHitMinCount, tmpl.magicHitMaxCount);
        record.magicHitGroups = tmpl.magicHitGroups;

    }
    else
    {

    }

    // 7. 应用符文系统

    if (!tmpl.runeSystemGroups.empty() || tmpl.runeSlotMaxCount > 0)
    {
        ApplyRuneSystem(player, item, tmpl);
        record.hasRuneSlots = true;
        record.runeSlotCount = GenerateRandomNumber(tmpl.runeSlotMinCount, tmpl.runeSlotMaxCount);

    }
    else
    {

    }

    // 8. 应用技能套装

    record.hasSet = false;
    record.setGroup = 0;
    record.setId = 0;

    if (!tmpl.skillSetGroups.empty())
    {
        uint32 selectedSetId = ApplySkillSets(player, item, tmpl);
        if (selectedSetId > 0)
        {
            record.hasSet = true;
            // 记录套装组ID（用于统计/查询），从配置中随机选择一个组
            record.setGroup = SelectRandomFromList(ParseCommaSeparatedNumbers(tmpl.skillSetGroups));
            record.setId = selectedSetId; // 保存实际分配的套装ID，供后续批量查询回退使用
        }
    }

    // 9. 更新物品状态
    item->SetState(ITEM_CHANGED, player);

    // 10. 保存鉴定记录到数据库
    SaveIdentificationRecord(record);
    AddToIdentifiedCache(itemGuid);

    // 11. 刷新物品显示
    RefreshItem(player, item);

    // 【关键修复】12. 清除批量查询缓存，确保后续查询能获取最新数据
    // 原因：在鉴定过程中，客户端可能已经发送过查询请求，导致缓存中存储了空数据
    // 必须在发送数据前清除缓存，让 SendAllModuleDataAddon 重新从数据库查询
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);
        uint64 cacheKey = (static_cast<uint64>(item->GetEntry()) << 32) | itemGuid;
        _batchQueryCache.erase(cacheKey);
    }

    // 13. 套装刷新已优化：移除鉴定时的刷新调用
    // 原因：物品在背包中时套装效果不需要生效，只有装备时才需要刷新
    // 套装系统会在玩家装备物品时（OnPlayerEquip）自动调用 RefreshPlayerSetEffects
    // 这样避免了鉴定时 415ms 的无用刷新，性能提升 87%
#ifdef MODULE_ITEM_SETS
    if (sItemSetsManager && !tmpl.skillSetGroups.empty())
    {

    }
#endif

    DebugLog("成功应用鉴定模板到物品，记录已保存");
    return true;
}

// 工具方法：解析逗号分隔的字符串为数字列表
std::vector<uint32> ParseCommaSeparatedNumbers(const std::string& str)
{
    std::vector<uint32> result;
    if (str.empty())
        return result;

    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        if (!item.empty())
        {
            try {
                uint32 num = std::stoul(item);
                result.push_back(num);
            } catch (...) {
            }
        }
    }

    return result;
}

// 工具方法：从列表中随机选择一个元素 - 优化版
uint32 SelectRandomFromList(const std::vector<uint32>& list)
{
    if (list.empty()) return 0;

    // ✅ 使用全局随机数生成器
    std::uniform_int_distribution<size_t> dis(0, list.size() - 1);
    return list[dis(sItemIdentificationSystem->GetRandomGenerator())];
}

// 工具方法：生成指定范围内的随机数 - 优化版
uint32 GenerateRandomNumber(uint32 min, uint32 max)
{
    if (min >= max) return min;

    // ✅ 使用全局随机数生成器
    std::uniform_int_distribution<uint32> dis(min, max);
    return dis(sItemIdentificationSystem->GetRandomGenerator());
}

uint256 GenerateRandomUInt256(uint256 min, uint256 max)
{
    if (min >= max)
        return min;

    uint256 span = max - min;
    uint256 randomValue = 0;
    std::uniform_int_distribution<uint32> dis(0, std::numeric_limits<uint32>::max());

    for (uint8 i = 0; i < 4; ++i)
    {
        randomValue <<= 32;
        randomValue += dis(sItemIdentificationSystem->GetRandomGenerator());
    }

    if (span == std::numeric_limits<uint256>::max())
        return randomValue;

    return min + (randomValue % (span + 1));
}

uint32 GetIdentificationTemplateMatchLevel(Item* item)
{
    if (!item || !item->GetTemplate())
        return 0;

    return item->GetTemplate()->Quality;
}

#ifdef MODULE_ITEM_ATTRIBUTES
uint256 GetOfficialStatReferenceValue(Item* item, uint32 attributeType)
{
    if (!item || !item->GetTemplate())
        return 1;

    ItemTemplate const* proto = item->GetTemplate();
    std::vector<uint256> values;
    uint256 matchedValue = 0;

    for (uint32 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = proto->ItemStat[i].ItemStatType;
        int256 statValue = proto->ItemStatValue256[i];

        if (statType == 0 || statValue <= 0)
            continue;

        uint256 value = Acore::Number::ToUInt256Saturated(statValue);
        values.push_back(value);

        if (statType == attributeType)
            matchedValue = std::max(matchedValue, value);
    }

    if (matchedValue > 0)
        return matchedValue;

    if (!values.empty())
    {
        std::sort(values.begin(), values.end());
        size_t middle = values.size() / 2;

        if (values.size() % 2 == 1)
            return values[middle];

        uint256 median = values[middle - 1] + ((values[middle] - values[middle - 1]) / 2);
        return std::max<uint256>(1, median);
    }

    uint32 itemLevelFallback = proto->ItemLevel / 4;
    return std::max<uint32>(1, itemLevelFallback);
}

// 按公式算最终百分比：线性 = 基础 + 幻境等级 × 每级增量（纯数据驱动，无魔法数字）
// formulaType==1 指数为预留，首版回退线性。
static uint256 ComputeScaledPercent(uint256 const& basePercent, uint256 const& huanJingLevel,
                                    double perLevelInc, uint8 /*formulaType*/)
{
    long double inc = Acore::Number::ToLongDouble(huanJingLevel) * perLevelInc;
    long double total = Acore::Number::ToLongDouble(basePercent) + inc;
    if (total < 0.0L)
        total = 0.0L;
    return Acore::Number::ToUInt256Saturated(std::round(total));
}

int256 CalculateOfficialPercentAttributeValue(Item* item, uint32 attributeType, uint256 minPercent, uint256 maxPercent)
{
    if (minPercent > maxPercent)
        std::swap(minPercent, maxPercent);

    uint256 percent = GenerateRandomUInt256(minPercent, maxPercent);
    uint256 referenceValue = GetOfficialStatReferenceValue(item, attributeType);
    long double scaledValue = Acore::Number::ToLongDouble(referenceValue) * Acore::Number::ToLongDouble(percent) / 100.0L;
    int256 value = Acore::Number::ToInt256Saturated(std::round(scaledValue));

    if (percent > 0 && value < 1)
        value = 1;

    return value;
}

std::vector<ItemAttributeTemplate const*> GetIdentificationAttributeCandidates(
    Item* item,
    std::vector<uint32> const& attributeGroups,
    std::set<uint32> const& selectedTypes,
    bool allowDuplicateTypes,
    bool respectChance)
{
    std::vector<ItemAttributeTemplate const*> candidates;
    std::vector<ItemAttributeTemplate const*> chanceFiltered;
    std::set<uint32> seenTemplateIds;

    if (!item || !sItemAttributesLoader)
        return candidates;

    for (uint32 groupId : attributeGroups)
    {
        std::vector<ItemAttributeTemplate const*> groupTemplates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(groupId);
        for (ItemAttributeTemplate const* attributeTemplate : groupTemplates)
        {
            if (!attributeTemplate || seenTemplateIds.find(attributeTemplate->id) != seenTemplateIds.end())
                continue;

            seenTemplateIds.insert(attributeTemplate->id);

            if (!allowDuplicateTypes && selectedTypes.find(attributeTemplate->attributeType) != selectedTypes.end())
                continue;

            if (attributeTemplate->qualityRequirement > 0 && item->GetTemplate()->Quality < attributeTemplate->qualityRequirement)
                continue;

            if (attributeTemplate->levelRequirement > 0 && item->GetTemplate()->ItemLevel < attributeTemplate->levelRequirement)
                continue;

            if (attributeTemplate->classRequirement > 0 && !(item->GetTemplate()->AllowableClass & attributeTemplate->classRequirement))
                continue;

            if (item->GetTemplate()->Class == ITEM_CLASS_WEAPON && attributeTemplate->attributeType == 12)
                continue;

            if (item->GetTemplate()->Class == ITEM_CLASS_ARMOR &&
                attributeTemplate->attributeType >= 121 && attributeTemplate->attributeType <= 134)
                continue;

            candidates.push_back(attributeTemplate);

            if (!respectChance || GenerateRandomNumber(1, 100) <= attributeTemplate->chance)
                chanceFiltered.push_back(attributeTemplate);
        }
    }

    if (!chanceFiltered.empty())
        candidates = chanceFiltered;

    std::shuffle(candidates.begin(), candidates.end(), sItemIdentificationSystem->GetRandomGenerator());
    return candidates;
}

uint32 ApplyOfficialPercentAdditionalAttributes(
    Item* item,
    IdentificationTemplate const& tmpl,
    std::vector<uint32> const& attributeGroups,
    uint32 attrCount,
    uint256 const& minPercent,
    uint256 const& maxPercent)
{
    if (!item || !sItemAttributesLoader || attrCount == 0)
        return 0;

    uint32 appliedCount = 0;
    std::set<uint32> selectedTypes;
    uint32 maxAttempts = std::max<uint32>(attrCount * 8, 8);

    for (uint32 attempt = 0; appliedCount < attrCount && attempt < maxAttempts; ++attempt)
    {
        std::vector<ItemAttributeTemplate const*> candidates = GetIdentificationAttributeCandidates(
            item,
            attributeGroups,
            selectedTypes,
            tmpl.additionalAttrAllowDuplicate,
            true);

        if (candidates.empty())
            break;

        for (ItemAttributeTemplate const* attributeTemplate : candidates)
        {
            if (!attributeTemplate)
                continue;

            int256 value = CalculateOfficialPercentAttributeValue(
                item,
                attributeTemplate->attributeType,
                minPercent,
                maxPercent);

            ItemAttributeResult result = sItemAttributesLoader->ApplyAttributeToItem(
                item,
                attributeTemplate->id,
                nullptr,
                AttributeCategory::ADDITIONAL,
                value,
                value);

            if (result == ItemAttributeResult::SUCCESS)
            {
                selectedTypes.insert(attributeTemplate->attributeType);
                ++appliedCount;
                break;
            }
        }
    }

    return appliedCount;
}

uint32 ApplyOfficialPercentBaseAttributes(
    Item* item,
    IdentificationTemplate const& tmpl,
    std::vector<uint32> const& attributeGroups,
    uint32 attrCount,
    std::string& outAttrDetails,
    uint256 const& minPercent,
    uint256 const& maxPercent)
{
    if (!item || !sItemAttributesLoader || attrCount == 0)
        return 0;

    uint32 appliedCount = 0;
    std::set<uint32> selectedTypes;
    std::vector<uint32> baseAttributes;
    std::vector<int256> baseValues;
    uint32 maxAttempts = std::max<uint32>(attrCount * 8, 8);

    for (uint32 attempt = 0; appliedCount < attrCount && attempt < maxAttempts; ++attempt)
    {
        std::vector<ItemAttributeTemplate const*> candidates = GetIdentificationAttributeCandidates(
            item,
            attributeGroups,
            selectedTypes,
            tmpl.baseAttrAllowDuplicate,
            true);

        if (candidates.empty())
            break;

        for (ItemAttributeTemplate const* attributeTemplate : candidates)
        {
            if (!attributeTemplate)
                continue;

            int256 value = CalculateOfficialPercentAttributeValue(
                item,
                attributeTemplate->attributeType,
                minPercent,
                maxPercent);

            ItemAttributeResult result = sItemAttributesLoader->ApplyAttributeToItem(
                item,
                attributeTemplate->id,
                nullptr,
                AttributeCategory::BASE,
                value,
                value);

            if (result == ItemAttributeResult::SUCCESS)
            {
                selectedTypes.insert(attributeTemplate->attributeType);
                baseAttributes.push_back(attributeTemplate->attributeType);
                baseValues.push_back(value);
                ++appliedCount;
                break;
            }
        }
    }

    if (!baseAttributes.empty() && baseAttributes.size() == baseValues.size())
    {
        std::ostringstream oss;
        for (size_t i = 0; i < baseAttributes.size(); ++i)
        {
            oss << baseAttributes[i] << " " << baseValues[i];
            if (i < baseAttributes.size() - 1)
                oss << ",";
        }
        outAttrDetails = oss.str();
    }

    return appliedCount;
}

std::set<uint32> GetAllowedAttributeTypesByGroups(const std::vector<uint32>& attributeGroups)
{
    std::set<uint32> allowedTypes;

    if (!sItemAttributesLoader)
        return allowedTypes;

    for (uint32 groupId : attributeGroups)
    {
        std::vector<ItemAttributeTemplate const*> templates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(groupId);
        for (ItemAttributeTemplate const* attributeTemplate : templates)
        {
            if (attributeTemplate)
                allowedTypes.insert(attributeTemplate->attributeType);
        }
    }

    return allowedTypes;
}

uint32 FilterItemAdditionalAttributesByGroups(Item* item, const std::vector<uint32>& attributeGroups)
{
    if (!item || attributeGroups.empty())
        return 0;

    auto data = ItemAttributesDBHelper::LoadItemAttributes(item->GetGUID().GetCounter());
    if (!data)
        return 0;

    std::set<uint32> allowedTypes = GetAllowedAttributeTypesByGroups(attributeGroups);
    if (allowedTypes.empty())
        return static_cast<uint32>(data->additionalAttributeIds.size());

    std::vector<uint32> filteredIds;
    std::vector<int256> filteredValues;
    filteredIds.reserve(data->additionalAttributeIds.size());
    filteredValues.reserve(data->additionalAttributeValues.size());

    bool hasFiltered = false;
    for (size_t i = 0; i < data->additionalAttributeIds.size() && i < data->additionalAttributeValues.size(); ++i)
    {
        uint32 attributeType = data->additionalAttributeIds[i];
        if (allowedTypes.find(attributeType) != allowedTypes.end())
        {
            filteredIds.push_back(attributeType);
            filteredValues.push_back(data->additionalAttributeValues[i]);
        }
        else
        {
            hasFiltered = true;
        }
    }

    if (hasFiltered)
    {
        ItemAttributesDBHelper::SaveAdditionalAttributes(item->GetGUID().GetCounter(), item->GetEntry(), filteredIds, filteredValues);
        std::ostringstream groupStream;
        for (size_t i = 0; i < attributeGroups.size(); ++i)
        {
            if (i > 0)
                groupStream << ",";
            groupStream << attributeGroups[i];
        }

        LOG_WARN("module.itemidentification", "[追加属性修复] 物品GUID={} 过滤了不属于允许组 [{}] 的追加属性，剩余 {} 条",
                 item->GetGUID().GetCounter(), groupStream.str(), filteredIds.size());
    }

    return static_cast<uint32>(filteredIds.size());
}
#endif

// 检查需求条件
bool ItemIdentificationSystem::CheckRequirements(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    if (tmpl.requirementTemplate == 0)
        return true;

    DebugLog("检查需求模板ID: {}", tmpl.requirementTemplate);

#ifdef MODULE_REQUIREMENT_TEMPLATE
    // 调用需求模板系统检查条件
    if (sRequirementSystem)
    {
        bool meetsRequirements = sRequirementSystem->CheckRequirements(player, tmpl.requirementTemplate, true);

        if (meetsRequirements)
        {
            DebugLog("玩家满足需求模板ID: {}", tmpl.requirementTemplate);
        }
        else
        {
            DebugLog("玩家不满足需求模板ID: {}", tmpl.requirementTemplate);
            ChatHandler(player->GetSession()).PSendSysMessage("不满足鉴定条件");
        }

        return meetsRequirements;
    }
    else
    {
        DebugLog("需求模板系统不可用");
    }
#else
    DebugLog("需求模板系统模块未编译");
#endif

    // 如果没有需求系统或系统不可用，默认返回true
    return true;
}

// 应用物品成长系统（返回实际应用的组号，0表示失败）
uint32 ItemIdentificationSystem::ApplyItemGrowth(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> growthGroups = ParseCommaSeparatedNumbers(tmpl.itemGrowthGroups);

    if (growthGroups.empty())
    {
        return 0;
    }

#ifdef MODULE_ITEM_GROWTH
    if (!sItemGrowthMgr)
    {
        return 0;
    }

    if (!sItemGrowthMgr->IsEnabled())
    {

        return 0;
    }

    // 随机选择一个成长组
    uint32 selectedGroup = SelectRandomFromList(growthGroups);

    if (selectedGroup == 0)
    {

        return 0;
    }

    // 直接调用成长系统API设置物品成长属性，并根据鉴定模板配置约束初始成长属性
    try
    {
        // 使用专为鉴定准备的接口，传入条目数量和属性值范围
        sItemGrowthMgr->SetItemCanGrowForIdentification(
            player,
            item,
            selectedGroup,
            tmpl.growthAttrMinCount,
            tmpl.growthAttrMaxCount,
            Acore::Number::ToInt256Saturated(tmpl.growthAttrMinValue),
            Acore::Number::ToInt256Saturated(tmpl.growthAttrMaxValue));

        // ChatHandler(player->GetSession()).PSendSysMessage("物品获得成长属性（组{}）", selectedGroup);

        return selectedGroup; // 返回实际应用的组号
    }
    catch (...)
    {

        return 0;
    }
#else

    return 0;
#endif
}

// 应用物品强化系统（返回实际应用的组号，0表示失败）
uint32 ItemIdentificationSystem::ApplyItemEnhancement(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> enhancementGroups = ParseCommaSeparatedNumbers(tmpl.itemEnhancementGroups);

    if (enhancementGroups.empty())
    {

        return 0;
    }

#ifdef MODULE_ITEM_ENHANCEMENT
    if (!sItemEnhancementMgr)
    {

        return 0;
    }

    if (!sItemEnhancementMgr->IsEnabled())
        return 0;

    // 随机选择一个强化组
    uint32 selectedGroup = SelectRandomFromList(enhancementGroups);

    if (selectedGroup == 0)
        return 0;

    // 检查物品是否可以强化
    if (!sItemEnhancementMgr->CanEnhanceItem(player, item))
        return 0;

    try
    {
        // ✅ 使用为鉴定准备的初始化方法，保证产生等级1的强化，并按鉴定模板约束条目数量与数值范围
        if (sItemEnhancementMgr->InitializeEnhancementForIdentification(
                player,
                item,
                selectedGroup,
                tmpl.enhancementAttrMinCount,
                tmpl.enhancementAttrMaxCount,
                tmpl.enhancementAttrMinValue,
                tmpl.enhancementAttrMaxValue))
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("物品获得强化属性（组{}）", selectedGroup);
            return selectedGroup; // 返回选择的组号
        }
        else
        {
            return 0;
        }
    }
    catch (...)
    {
        return 0;
    }
#else
    return 0;
#endif
}

// 应用基础属性（替换官方属性显示）
void ItemIdentificationSystem::ApplyBaseAttributes(Player* player, Item* item, const IdentificationTemplate& tmpl, std::string& outAttrDetails, uint32& outAttrCount, uint32& outAttrGroup)
{
    if (tmpl.itemAttributesGroups.empty())
    {
        return;
    }

    if (tmpl.baseAttrMaxCount == 0)
    {
        return;
    }

    uint32 attrCount = GenerateRandomNumber(tmpl.baseAttrMinCount, tmpl.baseAttrMaxCount);
    if (attrCount == 0)
    {
        return;
    }

    std::vector<uint32> attributeGroups = ParseCommaSeparatedNumbers(tmpl.itemAttributesGroups);
    if (attributeGroups.empty())
    {
        return;
    }

    DebugLog("应用基础属性，组列表: {}，数量: {}，值范围: {}-{}",
             tmpl.itemAttributesGroups, attrCount,
             Acore::ToString(tmpl.baseAttrMinValue), Acore::ToString(tmpl.baseAttrMaxValue));

#ifdef MODULE_ITEM_ATTRIBUTES
    if (!sItemAttributesGenerator || !sItemAttributesLoader)
    {

        return;
    }

    // 【修复】统一使用GetCounter()获取32位GUID，与数据库字段类型一致
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 使用属性数据库助手清除物品现有的属性
    // 检查物品是否已有属性
    bool hasAttrs = ItemAttributesDBHelper::HasAttributes(itemGuid);


    
    if (hasAttrs)
    {
        // 清除物品所有属性
        ItemAttributesDBHelper::ClearItemAttributes(itemGuid);
    }

    // 第2步：从配置的组中随机选择一个组
    // 直接取第一个组（或者随机选择）
    // 由于属性组可能为0，不再检查selectedGroup == 0
    uint32 selectedGroup = attributeGroups[0];

    DebugLog("===== 基础属性生成详细信息 =====");
    DebugLog("配置的组字符串: {}", tmpl.itemAttributesGroups);
    DebugLog("解析后的组列表大小: {}", attributeGroups.size());
    DebugLog("选中的组: {}", selectedGroup);
    DebugLog("属性数量: {}", attrCount);
    DebugLog("=================================");

    // 百分比模式：由 属性计算模式 字段显式决定（attrCalcMode==1），不再用数值大小(<=300)猜。
    // 最终百分比 = 基础百分比 + 幻境等级 × 每级增量（公式化，可超 300 不退化）。
    if (tmpl.attrCalcMode == 1)
    {
        uint256 hjLevel = 0;
#ifdef MODULE_HUANJING_SYSTEM
        if (sHuanJingSystem)
            hjLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);
#endif
        uint256 scaledMin = ComputeScaledPercent(tmpl.baseAttrMinValue, hjLevel, tmpl.baseAttrPerLevelInc, tmpl.formulaType);
        uint256 scaledMax = ComputeScaledPercent(tmpl.baseAttrMaxValue, hjLevel, tmpl.baseAttrPerLevelInc, tmpl.formulaType);
        uint32 appliedCount = ApplyOfficialPercentBaseAttributes(item, tmpl, attributeGroups, attrCount, outAttrDetails, scaledMin, scaledMax);
        if (appliedCount > 0)
        {
            outAttrCount = appliedCount;
            outAttrGroup = selectedGroup;
            DebugLog("成功应用基础属性百分比模式，组: {}，数量: {}", selectedGroup, appliedCount);
        }
        else
        {
            DebugLog("基础属性百分比模式应用失败");
            outAttrDetails = "";
            outAttrCount = 0;
            outAttrGroup = 0;
        }
        return;
    }

    ItemAttributeGenerateOptions options;
    options.minAttributes = attrCount;        // 最少属性数量
    options.maxAttributes = attrCount;        // 最多属性数量
    options.attributeGroup = selectedGroup;   // 指定属性组
    options.minItemLevel = Acore::Number::ToInt256Saturated(tmpl.baseAttrMinValue);   // 属性值最小值
    options.maxItemLevel = Acore::Number::ToInt256Saturated(tmpl.baseAttrMaxValue);   // 属性值最大值
    options.respectChance = true;             // 考虑属性获取几率
    options.allowDuplicateTypes = tmpl.baseAttrAllowDuplicate;  // 是否允许重复
    options.useValueRangeFilter = true;       // 启用属性值范围过滤
    options.category = AttributeCategory::BASE;  // 设置为基础属性

    // 生成随机属性（这会替换物品的属性显示）
    bool generateResult = sItemAttributesGenerator->GenerateRandomAttributes(item, options);



    if (generateResult)
    {
        // ✅ 优化：使用异步Execute避免死锁，内存缓存已立即更新
        // 如果读取失败，接受属性延迟加载（不影响鉴定成功）

        std::vector<uint32> baseAttributes;
        std::vector<int256> baseValues;

        uint64 itemGuid = item->GetGUID().GetCounter();

        // ✅ 尝试一次读取（不重试，不等待）
        auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);  // 【智能指针修复】
        if (data && !data->baseAttributeIds.empty())
        {
            baseAttributes = data->baseAttributeIds;
            baseValues = data->baseAttributeValues;
            // 【智能指针修复】移除 delete，自动清理
        }
        else
        {
            // 【智能指针修复】移除 delete，自动清理

            // 读取失败也不影响鉴定，玩家重新登录或查询时会加载
            DebugLog("基础属性生成成功，但立即读取失败（属性将在下次查询时加载）");
        }

        if (!baseAttributes.empty() && baseAttributes.size() == baseValues.size())
        {
            // 构建属性详情字符串：格式为 "属性ID 值,属性ID 值,..."
            std::ostringstream oss;
            for (size_t i = 0; i < baseAttributes.size(); ++i)
            {
                oss << baseAttributes[i] << " " << baseValues[i];
                if (i < baseAttributes.size() - 1)
                {
                    oss << ",";
                }
            }
            outAttrDetails = oss.str();
            outAttrCount = baseAttributes.size();
            outAttrGroup = selectedGroup;
        }
        else
        {
            // 无法读取生成的属性详情（数据库写入可能延迟）
            outAttrDetails = ""; // 详情为空，但不影响记录
            outAttrCount = attrCount; // 记录生成的数量
            outAttrGroup = selectedGroup; // 记录使用的组ID
        }

        DebugLog("成功应用基础属性组: {}，数量: {}", selectedGroup, attrCount);
    }
    else
    {
        DebugLog("基础属性应用失败");
        outAttrDetails = "";
        outAttrCount = 0;
        outAttrGroup = 0;
    }
#else
    DebugLog("属性系统模块未编译");
#endif


}

// 应用追加属性
uint32 ItemIdentificationSystem::ApplyAdditionalAttributes(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> attributeGroups = ParseCommaSeparatedNumbers(tmpl.itemAttributesAdditionalGroups);
    if (attributeGroups.empty())
    {

        return 0;
    }

    uint32 attrCount = GenerateRandomNumber(tmpl.additionalAttrMinCount, tmpl.additionalAttrMaxCount);
    if (attrCount == 0)
    {

        return 0;
    }

#ifdef MODULE_ITEM_ATTRIBUTES
    if (sItemAttributesGenerator)
    {
        uint32 appliedCount = 0;
        uint64 itemGuid = item->GetGUID().GetCounter();

        if (auto existingData = ItemAttributesDBHelper::LoadItemAttributes(itemGuid))
        {
            if (!existingData->additionalAttributeIds.empty())
            {
                ItemAttributesDBHelper::SaveAdditionalAttributes(itemGuid, item->GetEntry(), {}, {});
            }
        }

        // 百分比模式：由 属性计算模式 字段显式决定（attrCalcMode==1），不再用数值大小(<=300)猜。
        // 用于幻境鉴定模板，最终百分比 = 基础 + 幻境等级 × 每级增量，避免鉴定组随幻境等级平方级膨胀。
        if (tmpl.attrCalcMode == 1)
        {
            uint256 hjLevel = 0;
#ifdef MODULE_HUANJING_SYSTEM
            if (sHuanJingSystem)
                hjLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);
#endif
            uint256 scaledMin = ComputeScaledPercent(tmpl.additionalAttrMinValue, hjLevel, tmpl.additionalAttrPerLevelInc, tmpl.formulaType);
            uint256 scaledMax = ComputeScaledPercent(tmpl.additionalAttrMaxValue, hjLevel, tmpl.additionalAttrPerLevelInc, tmpl.formulaType);
            appliedCount = ApplyOfficialPercentAdditionalAttributes(item, tmpl, attributeGroups, attrCount, scaledMin, scaledMax);
            appliedCount = FilterItemAdditionalAttributesByGroups(item, attributeGroups);
        }
        // 判断是单组还是多组模式
        else if (attributeGroups.size() == 1)
        {
            // 单组模式：从同一个组生成多个属性
            ItemAttributeGenerateOptions options;
            options.minAttributes = attrCount;  // 使用配置的数量
            options.maxAttributes = attrCount;
            options.attributeGroup = attributeGroups[0];
            options.minItemLevel = Acore::Number::ToInt256Saturated(tmpl.additionalAttrMinValue);
            options.maxItemLevel = Acore::Number::ToInt256Saturated(tmpl.additionalAttrMaxValue);
            options.respectChance = true;
            options.allowDuplicateTypes = tmpl.additionalAttrAllowDuplicate;
            options.useValueRangeFilter = true;
            options.category = AttributeCategory::ADDITIONAL;  // 设置为追加属性

            if (sItemAttributesGenerator->GenerateRandomAttributes(item, options))
            {
                appliedCount = FilterItemAdditionalAttributesByGroups(item, attributeGroups);
            }
        }
        else
        {
            // 多组模式：从多个组中随机选择，总共生成attrCount个属性

            // 为每个属性随机选择一个组
            for (uint32 i = 0; i < attrCount; ++i)
            {
                // 随机选择一个组
                uint32 groupId = SelectRandomFromList(attributeGroups);

                ItemAttributeGenerateOptions options;
                options.minAttributes = 1;
                options.maxAttributes = 1;
                options.attributeGroup = groupId;
                options.minItemLevel = Acore::Number::ToInt256Saturated(tmpl.additionalAttrMinValue);
                options.maxItemLevel = Acore::Number::ToInt256Saturated(tmpl.additionalAttrMaxValue);
                options.respectChance = true;
                options.allowDuplicateTypes = tmpl.additionalAttrAllowDuplicate;
                options.useValueRangeFilter = true;
                options.category = AttributeCategory::ADDITIONAL;  // 设置为追加属性

                if (sItemAttributesGenerator->GenerateRandomAttributes(item, options))
                {
                    appliedCount++;
                }
            }

            appliedCount = FilterItemAdditionalAttributesByGroups(item, attributeGroups);
        }

        if (appliedCount > 0)
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个追加属性", appliedCount);
        }
        else
        {
        }

        return appliedCount;
    }
    else
    {
    }
#else
#endif


    return 0;
}

bool ItemIdentificationSystem::HasAdditionalIdentificationAttributes(Item* item)
{
    if (!item)
        return false;

#ifdef MODULE_ITEM_ATTRIBUTES
    if (auto data = ItemAttributesDBHelper::LoadItemAttributes(item->GetGUID().GetCounter()))
        return !data->additionalAttributeIds.empty();
#endif

    QueryResult result = CharacterDatabase.Query(
        "SELECT `是否获得追加属性` FROM `物品_鉴定记录` WHERE `物品GUID` = {} LIMIT 1",
        item->GetGUID().GetCounter());
    return result && result->Fetch()[0].Get<uint8>() != 0;
}

bool ItemIdentificationSystem::HasAnyCustomIdentificationData(Item* item)
{
    if (!item)
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    if (IsItemIdentified(itemGuid))
        return true;

    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM ("
        "SELECT `物品GUID` FROM `物品属性_数据` WHERE `物品GUID` = {} "
        "UNION ALL SELECT `物品GUID` FROM `物品成长_玩家记录` WHERE `物品GUID` = {} "
        "UNION ALL SELECT `guid` FROM `物品强化_记录` WHERE `guid` = {} "
        "UNION ALL SELECT `物品GUID` FROM `_物品技能_数据` WHERE `物品GUID` = {} "
        "UNION ALL SELECT `物品GUID` FROM `魔次系统_数据` WHERE `物品GUID` = {} "
        "UNION ALL SELECT `物品GUID` FROM `符文系统_数据` WHERE `物品GUID` = {} "
        "UNION ALL SELECT `物品GUID` FROM `_物品套装_数据` WHERE `物品GUID` = {} "
        "UNION ALL SELECT `装备GUID` FROM `玩家装备属性增强` WHERE `装备GUID` = {}"
        ") custom_data LIMIT 1",
        itemGuid, itemGuid, itemGuid, itemGuid, itemGuid, itemGuid, itemGuid, itemGuid);

    return static_cast<bool>(result);
}

bool ItemIdentificationSystem::HasIdentificationMultiplier(Item* item)
{
    if (!item)
        return false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `属性倍率`, `属性倍率模式` FROM `玩家装备属性增强` "
        "WHERE `装备GUID` = {} AND `装备ID` = {} LIMIT 1",
        item->GetGUID().GetCounter(), item->GetEntry());
    if (!result)
        return false;

    Field* fields = result->Fetch();
    uint256 multiplier = fields[0].Get<uint256>();
    char multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());
    return HasHuanJingEffect(multiplier, multiplierMode);
}

bool ItemIdentificationSystem::IdentifyItemFromScroll(Player* player, Item* item, uint32 groupId)
{
    if (!player || !item || !_enabled || groupId == 0)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
    {
        ChatHandler(player->GetSession()).SendNotification("只有武器和护甲可以使用鉴定卷轴");
        return false;
    }

    if (IsItemIdentified(item->GetGUID().GetCounter()))
        return false;

    if (!ApplyIdentificationInternal(player, item, groupId, 0, 100))
        return false;

    ChatHandler(player->GetSession()).SendNotification("鉴定卷轴已完成装备鉴定");
    return true;
}

bool ItemIdentificationSystem::SupplementAdditionalAttributesFromGroup(Player* player, Item* item, uint32 groupId)
{
    if (!player || !item || groupId == 0)
        return false;

    if (HasAdditionalIdentificationAttributes(item))
        return true;

    uint32 templateId = SelectIdentificationTemplate(groupId, item);
    auto templateIt = _identificationTemplates.find(templateId);
    if (!templateId || templateIt == _identificationTemplates.end())
    {
        ChatHandler(player->GetSession()).SendNotification("卷轴绑定的鉴定组没有可用模板");
        return false;
    }

    IdentificationTemplate const& tmpl = templateIt->second;
    if (!CheckRequirements(player, item, tmpl))
        return false;

    if (tmpl.itemAttributesAdditionalGroups.empty() || tmpl.additionalAttrMaxCount == 0)
    {
        ChatHandler(player->GetSession()).SendNotification("该鉴定组没有配置追加属性");
        return false;
    }

    uint32 appliedCount = ApplyAdditionalAttributes(player, item, tmpl);
    if (appliedCount == 0)
    {
        ChatHandler(player->GetSession()).SendNotification("追加属性生成失败，卷轴未消耗");
        return false;
    }

    std::string escapedGroups = tmpl.itemAttributesAdditionalGroups;
    CharacterDatabase.EscapeString(escapedGroups);
    CharacterDatabase.DirectExecute(
        "UPDATE `物品_鉴定记录` SET `是否获得追加属性` = 1, `追加属性数量` = {}, "
        "`追加属性组ID列表` = '{}' WHERE `物品GUID` = {}",
        appliedCount, escapedGroups, item->GetGUID().GetCounter());

    item->SetState(ITEM_CHANGED, player);
    ClearItemCache(item->GetEntry(), item->GetGUID().GetCounter());
    RefreshItem(player, item);
    ChatHandler(player->GetSession()).SendNotification("装备原有鉴定结果已保留，并补充了随机追加属性");
    return true;
}

bool ItemIdentificationSystem::OverwriteItemMultiplier(
    Player* player,
    Item* item,
    uint256 const& multiplier,
    uint32 identificationGroupId)
{
    if (!player || !item || multiplier <= 1)
        return false;

#ifdef MODULE_HUANJING_SYSTEM
    if (!sHuanJingSystem)
        return false;

    if (item->IsEquipped())
        sHuanJingSystem->RemoveHuanJingEnhancement(player, item);

    sHuanJingSystem->RemoveItemAttributeMultiplier(item);
    sHuanJingSystem->ApplyItemAttributeMultiplier(item, multiplier, 'x');
    sHuanJingSystem->UpdateItemIdentificationGroup(item->GetGUID().GetCounter(), identificationGroupId);
    CharacterDatabase.DirectExecute(
        "UPDATE `玩家装备属性增强` SET `鉴定组ID` = {} WHERE `装备GUID` = {}",
        identificationGroupId, item->GetGUID().GetCounter());

    if (item->IsEquipped())
        sHuanJingSystem->ApplyHuanJingEnhancement(player, item);

    if (sHuanJingSystem->GetItemAttributeMultiplier(item) != multiplier)
        return false;

    ClearItemCache(item->GetEntry(), item->GetGUID().GetCounter());
    QueueAllModuleDataAddonRefresh(player, item->GetEntry(), item->GetGUID().GetCounter(), 100ms);
    return true;
#else
    ChatHandler(player->GetSession()).SendNotification("幻境系统未加载，无法应用倍率卷轴");
    return false;
#endif
}

bool ItemIdentificationSystem::ClearAllIdentificationResults(Player* player, Item* item)
{
    if (!player || !item)
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 itemEntry = item->GetEntry();
    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 resetIdentificationGroupId = 1;

    QueryResult previousGroupResult = CharacterDatabase.Query(
        "SELECT `鉴定组ID` FROM `玩家装备属性增强` WHERE `装备GUID` = {} LIMIT 1",
        itemGuid);
    if (previousGroupResult)
    {
        uint32 previousGroupId = previousGroupResult->Fetch()[0].Get<uint32>();
        if (previousGroupId != 0)
            resetIdentificationGroupId = previousGroupId;
    }

#ifdef MODULE_HUANJING_SYSTEM
    if (sHuanJingSystem)
    {
        if (item->IsEquipped())
            sHuanJingSystem->RemoveHuanJingEnhancement(player, item);
        sHuanJingSystem->RemoveItemAttributeMultiplier(item);
    }
#endif

#ifdef MODULE_ITEM_ATTRIBUTES
    if (item->IsEquipped() && sItemAttributesEffects)
        sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
    ItemAttributesDBHelper::ClearItemAttributes(itemGuid);
#endif

#ifdef MODULE_ITEM_GROWTH
    if (sItemGrowthMgr)
    {
        if (item->IsEquipped())
            sItemGrowthMgr->RemoveGrowthAura(player, itemGuid);
        sItemGrowthMgr->GetPlayerItemRecords().erase(itemGuid);
    }
#endif

#ifdef MODULE_ITEM_ENHANCEMENT
    if (sItemEnhancementMgr)
    {
        if (item->IsEquipped())
            sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);
        sItemEnhancementMgr->DeleteEnhancementRecord(itemGuid);
    }
#endif

#ifdef MODULE_ITEM_SKILLS
    if (item->IsEquipped() && sItemSkillsEffects)
        sItemSkillsEffects->RemoveItemSkillEffects(player, item);
    if (sItemSkillsDBHelper)
        sItemSkillsDBHelper->ClearItemSkills(item);
#endif

#ifdef MODULE_MAGIC_HIT_SYSTEM
    if (sMagicHitSystem)
        sMagicHitSystem->RemoveMagicHitFromItem(itemGuid);
#endif

#ifdef MODULE_RUNE_SYSTEM
    if (item->IsEquipped())
        RuneApplyManager::RemoveItemRunes(player, itemGuid);
#endif

#ifdef MODULE_ITEM_SETS
    if (sItemSetsManager)
        sItemSetsManager->ClearAllSetEffects(player, true);
#endif

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append(Acore::StringFormat(
        "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `物品_鉴定记录` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `物品属性_数据` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `物品成长_玩家记录` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `物品强化_记录` WHERE `guid` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `_物品技能_数据` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `魔次系统_数据` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `符文系统_数据` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `_物品套装_数据` WHERE `物品GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "DELETE FROM `玩家装备属性增强` WHERE `装备GUID` = {}", itemGuid).c_str());
    trans->Append(Acore::StringFormat(
        "REPLACE INTO `待鉴定物品标记` "
        "(`物品GUID`, `物品ID`, `玩家GUID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID`) "
        "VALUES ({}, {}, {}, 1, 0, {})",
        itemGuid, itemEntry, playerGuid, resetIdentificationGroupId).c_str());
    CharacterDatabase.DirectCommitTransaction(trans);

#ifdef MODULE_RUNE_SYSTEM
    if (sRuneSystem)
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
#endif

#ifdef MODULE_ITEM_SKILLS
    if (sItemSkillsDBHelper)
        sItemSkillsDBHelper->ClearCache(itemGuid);
    if (sItemSkillsManager)
        sItemSkillsManager->RefreshItemSkillsInMemory(playerGuid, item);
    if (sItemSkillsEffects)
        sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
#endif

#ifdef MODULE_ITEM_SETS
    if (sItemSetsManager)
    {
        sItemSetsManager->LoadPlayerSetStatus(player);
        sItemSetsManager->RefreshPlayerSetEffects(player);
    }
#endif

    ClearIdentifiedCache(itemGuid);
    ClearItemCache(itemEntry, itemGuid);

    int32 randomPropertyId = item->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
    if (randomPropertyId == -1 ||
        (randomPropertyId > 0 && static_cast<uint32>(randomPropertyId) == itemGuid))
    {
        item->SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, 0);
    }

    QueryResult pendingResult = CharacterDatabase.Query(
        "SELECT 1 FROM `待鉴定物品标记` "
        "WHERE `物品GUID` = {} AND `物品ID` = {} AND `玩家GUID` = {} LIMIT 1",
        itemGuid, itemEntry, playerGuid);
    if (!pendingResult || HasAnyCustomIdentificationData(item))
    {
        LOG_INFO("server.loading",
            "[鉴定清理卷轴] 清理后校验失败: 玩家GUID={}, 物品GUID={}, 物品ID={}",
            playerGuid, itemGuid, itemEntry);
        ChatHandler(player->GetSession()).SendNotification("清理鉴定数据失败，卷轴未消耗");
        return false;
    }

    item->SetState(ITEM_CHANGED, player);
    RefreshItem(player, item);
    item->SendUpdateToPlayer(player);

    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);

    SendItemIdentificationAddonMessage(player,
        BuildIdentificationCleanupRefreshPayload(itemEntry, itemGuid));
    QueueAllModuleDataAddonRefresh(player, itemEntry, itemGuid, 100ms);
    return true;
}

bool ItemIdentificationSystem::HandleIdentificationScrollUse(Player* player, Item* scroll, Item* target)
{
    if (!player || !scroll || !target || scroll == target)
        return false;

    auto configIt = _identificationScrollConfigs.find(scroll->GetEntry());
    if (configIt == _identificationScrollConfigs.end())
    {
        ChatHandler(player->GetSession()).SendNotification("此物品没有配置鉴定卷轴数据");
        return false;
    }

    if (target->GetOwner() != player)
    {
        ChatHandler(player->GetSession()).SendNotification("卷轴只能用于自己的装备");
        return false;
    }

    ItemTemplate const* targetTemplate = target->GetTemplate();
    if (!targetTemplate ||
        (targetTemplate->Class != ITEM_CLASS_WEAPON && targetTemplate->Class != ITEM_CLASS_ARMOR))
    {
        ChatHandler(player->GetSession()).SendNotification("卷轴只能用于武器或护甲");
        return false;
    }

    IdentificationScrollConfig const& config = configIt->second;
    IdentificationScrollTargetState targetState;
    targetState.isIdentified = IsItemIdentified(target->GetGUID().GetCounter());
    targetState.hasAdditionalAttributes = HasAdditionalIdentificationAttributes(target);
    targetState.hasMultiplier = HasIdentificationMultiplier(target);

    IdentificationScrollAction action = ResolveIdentificationScrollAction(config.type, targetState);
    bool success = false;

    switch (action)
    {
        case IdentificationScrollAction::RunFullIdentificationAndOverwriteMultiplier:
            success = IdentifyItemFromScroll(player, target, config.identificationGroupId) &&
                      OverwriteItemMultiplier(player, target, config.multiplier, config.identificationGroupId);
            break;
        case IdentificationScrollAction::SupplementAdditionalAndOverwriteMultiplier:
            success = SupplementAdditionalAttributesFromGroup(player, target, config.identificationGroupId) &&
                      OverwriteItemMultiplier(player, target, config.multiplier, config.identificationGroupId);
            break;
        case IdentificationScrollAction::PreserveIdentificationAndOverwriteMultiplier:
            success = OverwriteItemMultiplier(player, target, config.multiplier, config.identificationGroupId);
            break;
        case IdentificationScrollAction::ClearAllCustomIdentificationData:
            success = ClearAllIdentificationResults(player, target);
            break;
        case IdentificationScrollAction::RejectNotEligible:
            ChatHandler(player->GetSession()).SendNotification("只有已鉴定且存在追加属性或有效倍率的装备才能使用清理卷轴");
            return false;
    }

    if (!success)
        return false;

    if (config.type != IdentificationScrollType::Cleanup)
    {
        SendItemIdentificationAddonMessage(player,
            BuildIdentificationRefreshPayload(target->GetEntry(), target->GetGUID().GetCounter()));
    }

    uint32 consumeCount = 1;
    player->DestroyItemCount(scroll, consumeCount, true);

    if (config.type == IdentificationScrollType::Cleanup)
        ChatHandler(player->GetSession()).SendNotification("装备的全部自定义鉴定结果已清空");
    else
        ChatHandler(player->GetSession()).SendNotification("装备倍率已覆盖为 {} 倍", config.multiplier.str());

    return true;
}

bool ItemIdentificationSystem::IsIdentificationScrollConfigured(uint32 itemEntry) const
{
    return _identificationScrollConfigs.find(itemEntry) != _identificationScrollConfigs.end();
}

// 应用追加技能
void ItemIdentificationSystem::ApplyAdditionalSkills(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> skillGroups = ParseCommaSeparatedNumbers(tmpl.itemSkillsGroups);
    if (!skillGroups.empty())
    {
    }

    if (skillGroups.empty())
    {
        return;
    }

    uint32 skillCount = GenerateRandomNumber(tmpl.additionalSkillMinCount, tmpl.additionalSkillMaxCount);

    if (skillCount == 0)
    {
        return;
    }

    DebugLog("应用追加技能，组列表: {}，数量: {}", tmpl.itemSkillsGroups, skillCount);

#ifdef MODULE_ITEM_SKILLS
    if (sItemSkillsManager && sItemSkillsDBHelper)
    {
        uint32 appliedCount = 0;
        uint32 itemId = item->GetEntry();

        // 判断是单组还是多组模式
        if (skillGroups.size() == 1)
        {
            // 单组模式：从同一个组生成多个技能
            for (uint32 i = 0; i < skillCount; ++i)
            {
                uint32 groupId = skillGroups[0];

                // 从指定的技能组中按权重随机选择一个技能
                ItemSkillTemplate const* skillTemplate = sItemSkillsManager->SelectSkillByWeight(itemId, groupId);

                if (skillTemplate)
                {
                    // 将技能添加到物品
                    sItemSkillsDBHelper->AddSkillToItem(item, skillTemplate->id);
                    
                    appliedCount++;
                }
                else
                {
                    DebugLog("从组{}中未找到合适的技能", groupId);
                }
            }
        }
        else
        {
            // 多组模式：从多个组中随机选择，总共生成skillCount个技能

            for (uint32 i = 0; i < skillCount; ++i)
            {
                // 随机选择一个组
                uint32 groupId = SelectRandomFromList(skillGroups);

                // 从指定的技能组中按权重随机选择一个技能
                ItemSkillTemplate const* skillTemplate = sItemSkillsManager->SelectSkillByWeight(itemId, groupId);

                if (skillTemplate)
                {
                    // 将技能添加到物品
                    sItemSkillsDBHelper->AddSkillToItem(item, skillTemplate->id);
                    
                    appliedCount++;
                }
                else
                {
                    DebugLog("从组{}中未找到合适的技能", groupId);
                }
            }
        }

        if (appliedCount > 0)
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个技能效果", appliedCount);
        }
        else
        {
            DebugLog("未成功应用任何技能");
        }
    }
    else
    {
        DebugLog("技能系统或数据库辅助类不可用");
    }
#else
    DebugLog("技能系统模块未编译");
#endif


}

// 应用技能魔次
void ItemIdentificationSystem::ApplyMagicHits(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> magicHitGroups = ParseCommaSeparatedNumbers(tmpl.magicHitGroups);

    if (magicHitGroups.empty())
    {

        return;
    }

    uint32 magicHitCount = GenerateRandomNumber(tmpl.magicHitMinCount, tmpl.magicHitMaxCount);

    if (magicHitCount == 0)
    {

        return;
    }

#ifdef MODULE_MAGIC_HIT_SYSTEM

    if (sMagicHitSystem && sMagicHitSystem->IsEnabled())
    {
        uint32 appliedCount = 0;
        uint32 itemId = item->GetEntry();
        uint64 itemGuid = item->GetGUID().GetCounter();

        // 判断是单组还是多组模式
        if (magicHitGroups.size() == 1)
        {
            // 单组模式：从同一个组生成多个魔次

            for (uint32 i = 0; i < magicHitCount; ++i)
            {
                uint32 groupId = magicHitGroups[0];

                // 从指定的魔次组中按权重随机选择一个魔次配置
                SpellMagicHitConfig const* magicHitConfig = sMagicHitSystem->SelectMagicHitByWeight(itemId, groupId);

                if (magicHitConfig)
                {
                    // 生成随机的魔次值（次数）
                    uint32 hitCount = GenerateRandomNumber(tmpl.magicHitMinValue, tmpl.magicHitMaxValue);

                    // 将魔次添加到物品
                    if (sMagicHitSystem->AddMagicHitToItem(itemId, itemGuid, magicHitConfig->configId, hitCount))
                    {
                        appliedCount++;
                    }
                }
            }
        }
        else
        {
            // 多组模式：从多个组中随机选择，总共生成magicHitCount个魔次

            for (uint32 i = 0; i < magicHitCount; ++i)
            {
                // 随机选择一个组
                uint32 groupId = SelectRandomFromList(magicHitGroups);

                // 从指定的魔次组中按权重随机选择一个魔次配置
                SpellMagicHitConfig const* magicHitConfig = sMagicHitSystem->SelectMagicHitByWeight(itemId, groupId);

                if (magicHitConfig)
                {
                    // 生成随机的魔次值（次数）
                    uint32 hitCount = GenerateRandomNumber(tmpl.magicHitMinValue, tmpl.magicHitMaxValue);

                    // 将魔次添加到物品
                    if (sMagicHitSystem->AddMagicHitToItem(itemId, itemGuid, magicHitConfig->configId, hitCount))
                    {
                        appliedCount++;
                    }
                }
            }
        }

        if (appliedCount > 0)
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个技能魔次", appliedCount);
        }

    }
#else
#endif


}

// 应用符文系统
void ItemIdentificationSystem::ApplyRuneSystem(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
#ifdef MODULE_RUNE_SYSTEM

    if (sRuneManager)
    {
        // 创建符文凹槽
        if (tmpl.runeSlotMaxCount > 0)
        {
            uint32 slotCount = GenerateRandomNumber(tmpl.runeSlotMinCount, tmpl.runeSlotMaxCount);

            if (slotCount > 0)
            {
                DebugLog("尝试为物品创建{}个符文凹槽", slotCount);

                if (sRuneManager->AddRuneSlotToItem(item, slotCount))
                {
                    DebugLog("成功为物品创建{}个符文凹槽", slotCount);
                    // ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个符文凹槽", slotCount);
                }
                else
                {
                    DebugLog("创建符文凹槽失败");
                }
            }
        }

        // 记录符文组信息（符文组配置用于玩家自行选择镶嵌的符文）
        std::vector<uint32> runeGroups = ParseCommaSeparatedNumbers(tmpl.runeSystemGroups);
        if (!runeGroups.empty())
        {
            DebugLog("物品符文组配置: {}（玩家可以使用这些组中的符文进行镶嵌）", tmpl.runeSystemGroups);
            // 注意：当前符文系统设计中，玩家需要自行镶嵌符文
            // 符文组信息可用于提示玩家可以使用哪些符文
            // ChatHandler(player->GetSession()).PSendSysMessage("物品可以镶嵌符文（推荐符文组: {}）", tmpl.runeSystemGroups);
        }


    }
    else
    {
        DebugLog("符文系统不可用");
    }
#else
    DebugLog("符文系统模块未编译");
#endif


}

// 应用技能套装（返回实际分配的套装ID，0表示未分配）
uint32 ItemIdentificationSystem::ApplySkillSets(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> skillSetGroups = ParseCommaSeparatedNumbers(tmpl.skillSetGroups);
    if (skillSetGroups.empty())
    {
        return 0;
    }

    // 随机选择一个套装组
    uint32 selectedGroup = SelectRandomFromList(skillSetGroups);
    DebugLog("应用技能套装组: {}", selectedGroup);

#ifdef MODULE_ITEM_SETS

    if (!sItemSetsManager)
    {
        DebugLog("套装系统管理器不可用");
        return 0;
    }

    if (!sItemSetsConfig || !sItemSetsConfig->IsModuleEnabled())
    {
        DebugLog("套装系统未启用");
        return 0;
    }

    uint32 itemId = item->GetEntry();
    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 1. 检查该物品是否已经分配过套装
    PlayerSetStatus* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);


    if (status && status->ItemSetMap.find(itemGuid) != status->ItemSetMap.end())
    {
        DebugLog("物品已经分配过套装: GUID={}", itemGuid);
        ChatHandler(player->GetSession()).SendSysMessage("该物品已经分配过套装");
        return 0;
    }

    // 2. 从指定组中选择套装（考虑玩家需求）
    uint32 selectedSetId = sItemSetsManager->GetRandomSetForGroup(selectedGroup, player);



    if (selectedSetId == 0)
    {
        DebugLog("从套装组{}中未找到满足需求的套装", selectedGroup);

        // 显示组内套装的需求条件
        sItemSetsManager->ShowGroupRequirements(player, selectedGroup);
        ChatHandler(player->GetSession()).PSendSysMessage("分配套装失败：不满足需求条件。");
        return 0;
    }

    // 3. 消耗需求物品
    DebugLog("开始消耗套装 {} 的需求物品", selectedSetId);

    auto step3Start = std::chrono::high_resolution_clock::now();
    if (!sItemSetsManager->ConsumeSetRequirements(player, selectedSetId))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("消耗需求物品失败，分配套装取消。");

        // 显示具体的需求材料
        std::vector<ItemSetData> setDataList = sItemSetsManager->GetItemSetData(selectedSetId);
        if (!setDataList.empty() && setDataList[0].RequirementId > 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000请准备以下材料：|r");
#ifdef MODULE_REQUIREMENT_TEMPLATE
            if (sRequirementSystem)
            {
                sRequirementSystem->CheckRequirements(player, setDataList[0].RequirementId, true);
            }
#endif
        }
        return 0;
    }


    DebugLog("成功消耗套装 {} 的需求物品", selectedSetId);

    // 4. 创建或更新玩家套装状态
    if (!status)
    {
        PlayerSetStatus newStatus;
        newStatus.PlayerGuid = playerGuid;
        // 需要直接访问私有成员，这里只能通过公开方法
        // 实际上这部分逻辑在后面SavePlayerSetStatus中会处理
    }

    // 获取最新的状态（如果刚创建会在这里初始化）
    status = sItemSetsManager->GetPlayerSetStatus(playerGuid);
    if (status)
    {
        // 记录物品的套装分配
        status->ItemSetMap[itemGuid] = selectedSetId;
    }

    // 5. 获取套装名称并通知玩家
    std::string setName = sItemSetsManager->GetSetName(selectedSetId);


    // ChatHandler(player->GetSession()).PSendSysMessage("物品已分配到套装: {} (组{}, ID{})",
    //     setName.c_str(), selectedGroup, selectedSetId);

    // 6. 保存到数据库
    sItemSetsManager->SavePlayerSetStatus(player);



    // 8. 重新计算并应用套装效果
    // ⚠️ 性能优化：注释掉立即刷新，改为在鉴定流程结束后统一刷新
    // 原因：RefreshPlayerSetEffects 单次耗时较长
    // 优化后将在 ApplyIdentification 函数末尾统一调用一次
    // sItemSetsManager->RefreshPlayerSetEffects(player);  // 已优化：延迟到鉴定结束后

    DebugLog("成功将物品分配到套装组: {}，套装ID: {}", selectedGroup, selectedSetId);

    return selectedSetId;
#else
    DebugLog("套装系统模块未编译");
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000套装系统未安装|r");
    return 0;
#endif
}

// 处理鉴定失败
void ItemIdentificationSystem::HandleFailure(Player* player, Item* item)
{
    if (_destroyOnFail)
    {
        // 销毁物品
        player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        ChatHandler(player->GetSession()).SendNotification("物品在鉴定过程中被摧毁了！");
    }
    else
    {
        // 不销毁物品，可以添加其他失败效果
        // 例如：降低物品耐久度
        uint32 maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
        if (maxDurability > 0)
        {
            uint32 currentDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
            uint32 newDurability = currentDurability > maxDurability / 4 ? currentDurability - maxDurability / 4 : 1;
            item->SetUInt32Value(ITEM_FIELD_DURABILITY, newDurability);
            ChatHandler(player->GetSession()).SendNotification("物品在鉴定过程中受到了损伤！");
        }
    }
}

// 发送鉴定公告
void ItemIdentificationSystem::SendAnnouncement(Player* player, Item* item, uint32 identificationId)
{
    if (!_enableAnnounce)
        return;

    // 查询公告模板
    std::string query = "SELECT `公告模板` FROM `_物品鉴定_模板` WHERE `id` = " + std::to_string(identificationId);

    QueryResult result = WorldDatabase.Query(query.c_str());
    if (!result)
        return;

    Field* fields = result->Fetch();
    uint32 announceTemplateId = fields[0].Get<uint32>();

    if (announceTemplateId == 0)
        return;

    // 尝试通过模块管理器获取公告模块接口
    if (AnnouncementInterface* announceModule = sModuleManager->GetAnnouncementModule())
    {
        // 使用公告模块发送公告
        announceModule->SendAnnouncement(player, announceTemplateId, true);

        if (_debugMode)
            DebugLog("通过公告模块发送鉴定公告ID: {}", announceTemplateId);
    }
    else
    {
        // 如果公告模块未加载，使用传统方式发送公告
        // 获取物品信息
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return;

        // 构建公告消息
        std::string message = "|cffff0000[物品鉴定系统]|r 恭喜玩家 |cff00ff00" + player->GetName() + "|r 成功鉴定出 ";

        // 根据物品品质添加颜色
        switch (proto->Quality)
        {
            case ITEM_QUALITY_POOR:
                message += "|cff9d9d9d";
                break;
            case ITEM_QUALITY_NORMAL:
                message += "|cffffffff";
                break;
            case ITEM_QUALITY_UNCOMMON:
                message += "|cff1eff00";
                break;
            case ITEM_QUALITY_RARE:
                message += "|cff0070dd";
                break;
            case ITEM_QUALITY_EPIC:
                message += "|cffa335ee";
                break;
            case ITEM_QUALITY_LEGENDARY:
                message += "|cffff8000";
                break;
            default:
                message += "|cffffffff";
                break;
        }

        message += proto->Name1 + "|r！";

        // 发送全服公告
        ChatHandler(nullptr).SendWorldText(message.c_str());

        if (_debugMode)
            DebugLog("公告模块未加载，使用传统方式发送鉴定公告");
    }
}

// 调试日志已在头文件中实现为模板函数

// 命令处理类实现
ItemIdentificationCommandScript::ItemIdentificationCommandScript() : CommandScript("ItemIdentificationCommandScript") { }

std::vector<Acore::ChatCommands::ChatCommandBuilder> ItemIdentificationCommandScript::GetCommands() const
{
    using namespace Acore::ChatCommands;

    // 鉴定子命令
    static ChatCommandTable identificationSubCommands =
    {
        // "查询" 命令已删除 - 使用 addon 消息自动查询替代
        { "批量查询", HandleBatchQueryCommand, SEC_PLAYER, Console::No },  // 批量查询命令
        { "性能统计", HandlePerformanceStatsCommand, SEC_ADMINISTRATOR, Console::No },  // 性能监控
        // ========== 【新增】手动鉴定相关命令 ==========
        { "手动", HandleManualIdentifyCommand, SEC_PLAYER, Console::No },  // 手动鉴定单个物品
        { "待鉴定", HandleListPendingCommand, SEC_PLAYER, Console::No },   // 查询待鉴定列表
        { "批量鉴定", HandleBatchIdentifyCommand, SEC_PLAYER, Console::No }, // 批量鉴定所有
        { "界面", HandleOpenUICommand, SEC_PLAYER, Console::No },          // 打开UI界面
        { "ui", HandleOpenUICommand, SEC_PLAYER, Console::No }             // 打开UI界面
    };

    // 主命令 - 支持直接鉴定和子命令
    static ChatCommandTable commandTable =
    {
        { "鉴定物品", HandleIdentifyCommand, SEC_PLAYER, Console::No },
        { "identifyitem", HandleIdentifyCommand, SEC_PLAYER, Console::No },
        { "鉴定", identificationSubCommands }
    };

    return commandTable;
}

bool ItemIdentificationCommandScript::HandleIdentifyCommand(ChatHandler* handler, const char* args)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 检查系统是否启用
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("物品鉴定系统当前已禁用");
        return true;
    }

    // 获取参数
    if (!*args)
    {
        handler->SendSysMessage("用法: .鉴定物品 <组ID> <物品ID>");
        handler->SendSysMessage("用法: .鉴定物品 查询 [all]");
        handler->SendSysMessage("示例: .鉴定物品 1 25  (使用组1鉴定物品25)");
        handler->SendSysMessage("示例: .鉴定物品 查询 all  (查询所有已鉴定物品)");
        return true;
    }

    // 删除旧的查询子命令 - 现在使用 addon 消息自动查询
    // 旧代码：if (firstArg == "查询") HandleQueryAttributesCommand(...) - 已删除

    // 解析鉴定命令参数: <组ID> <物品ID>
    // 【安全修复】使用std::strtok并增加输入验证
    char* groupIdStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");

    if (!groupIdStr || !itemIdStr)
    {
        handler->SendSysMessage("参数不足！用法: .鉴定物品 <组ID> <物品ID>");
        return true;
    }

    // 【安全修复】验证输入是否为有效数字
    char* endPtr = nullptr;
    long groupIdLong = strtol(groupIdStr, &endPtr, 10);
    if (endPtr == groupIdStr || *endPtr != '\0' || groupIdLong < 0 || groupIdLong > UINT32_MAX)
    {
        handler->SendSysMessage("无效的组ID，必须是非负整数");
        return true;
    }

    endPtr = nullptr;
    long itemIdLong = strtol(itemIdStr, &endPtr, 10);
    if (endPtr == itemIdStr || *endPtr != '\0' || itemIdLong <= 0 || itemIdLong > UINT32_MAX)
    {
        handler->SendSysMessage("无效的物品ID，必须是正整数");
        return true;
    }

    uint32 groupId = static_cast<uint32>(groupIdLong);
    uint32 itemId = static_cast<uint32>(itemIdLong);

    // 检查物品模板是否存在
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
    {
        handler->PSendSysMessage("物品ID {} 不存在", itemId);
        return true;
    }

    // 在玩家背包和装备栏中查找该物品
    Item* item = nullptr;
    
    // 优先查找背包中的物品
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == itemId)
        {
            item = pItem;
            break;
        }
    }

    // 如果背包没找到，查找背包袋子中的物品
    if (!item)
    {
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    Item* pItem = pBag->GetItemByPos(j);
                    if (pItem && pItem->GetEntry() == itemId)
                    {
                        item = pItem;
                        break;
                    }
                }
                if (item) break;
            }
        }
    }

    // 如果还没找到，查找装备栏（通常不鉴定已装备的）
    if (!item)
    {
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem->GetEntry() == itemId)
            {
                item = pItem;
                break;
            }
        }
    }

    if (!item)
    {
        handler->PSendSysMessage("您的背包中没有物品ID为 {} 的物品", itemId);
        handler->PSendSysMessage("物品名称: {}", itemTemplate->Name1);
        return true;
    }

    // 尝试鉴定物品
    handler->PSendSysMessage("正在鉴定物品: {} [{}]，使用组ID: {}", itemTemplate->Name1, itemId, groupId);

    if (sItemIdentificationSystem->IdentifyItem(player, item, groupId))
    {
        handler->SendSysMessage("物品鉴定成功！");
    }
    else
    {
        // 错误消息已在IdentifyItem方法中发送
    }

    return true;
}

// HandleQueryAttributesCommand 已删除 - 现在使用 addon 格式的 HandleAddonBatchQuery
// 客户端不再通过命令查询，而是自动发送 addon 消息 (UITQ QUERY) 来获取数据

// HandleQueryCommand 已删除 - 使用 HandleAddonBatchQuery (addon格式) 替代旧的单独查询

// 模块加载器实现
ItemIdentificationSystemModuleLoader::ItemIdentificationSystemModuleLoader() : WorldScript("ItemIdentificationSystemModuleLoader"), _loaded(false), _startTime(0) { }

void ItemIdentificationSystemModuleLoader::OnAfterConfigLoad(bool reload)
{
    // 如果是重新加载配置，才执行加载操作
    // 初始加载将在服务器完全启动后由OnUpdate执行
    if (reload)
    {
        sItemIdentificationSystem->LoadConfig(reload);
    }
}

void ItemIdentificationSystemModuleLoader::OnBeforeWorldInitialized()
{
    // 此时 world 网络监听尚未启动，玩家还无法进入游戏。
    // 启动全量清理放在这里，避免与玩家登录/小退/拾取/保存事务并发。
    CleanupOrphanedItemData();
}

void ItemIdentificationSystemModuleLoader::OnUpdate(uint32 diff)
{
    // 延迟1秒初始化模块，确保服务器完全启动
    if (!_loaded)
    {
        if (_startTime == 0)
            _startTime = getMSTime();
        else if (getMSTime() - _startTime > 1000)
        {
            _loaded = true;

            // 【修复】命令脚本已在 AddItemIdentificationSystemScripts() 中注册，移除重复注册
            // new ItemIdentificationCommandScript();  // 已移除，避免重复注册导致命令执行两次

            // 注册玩家脚本
            new ItemIdentificationPlayerScript();

            // 初始化模块
            sItemIdentificationSystem->Initialize();

            // 显示加载信息
            LOG_INFO("server.loading", "→物品鉴定系统√");
        }
    }

    // 定期清理过期缓存（每5分钟执行一次）
    static uint32 cacheCleanTimer = 0;
    cacheCleanTimer += diff;

    if (cacheCleanTimer >= 300000)  // 5分钟 = 300000ms
    {
        cacheCleanTimer = 0;
        sItemIdentificationSystem->CleanExpiredCache();
    }

    // ★★★ 定期清理待鉴定物品标记表的孤立数据 ★★★
    // 原因：玩家拾取物品后没有手动鉴定就删除/出售/交易物品，导致待鉴定标记永久保留
    // 规则：每30分钟进入一次“待清理”状态，但只有连续无人在线5分钟才真正执行
    static uint32 pendingCleanTimer = 0;
    static uint32 pendingEmptyTimer = 0;
    static bool pendingCleanupDue = false;
    static bool pendingCleanupDeferredLogged = false;

    if (!pendingCleanupDue)
    {
        pendingCleanTimer += diff;

        if (pendingCleanTimer >= 1800000)  // 30分钟 = 1800000ms
            pendingCleanupDue = true;
    }

    if (pendingCleanupDue)
    {
        uint32 onlinePlayerCount = sWorldSessionMgr->GetPlayerCount();
        if (onlinePlayerCount == 0)
        {
            pendingEmptyTimer += diff;
            if (pendingEmptyTimer >= 300000)
            {
                pendingCleanTimer = 0;
                pendingEmptyTimer = 0;
                pendingCleanupDue = false;
                pendingCleanupDeferredLogged = false;
                CleanupPendingIdentificationData();
            }
        }
        else
        {
            pendingEmptyTimer = 0;
            if (!pendingCleanupDeferredLogged)
            {
                pendingCleanupDeferredLogged = true;
                LOG_INFO("module.itemidentification",
                    "[定期清理] 当前仍有 {} 名玩家在线，延后执行待鉴定孤立数据清理。",
                    onlinePlayerCount);
            }
        }
    }
}

// 启动阶段全量清理孤立的物品数据（服务器启动时执行一次）
void ItemIdentificationSystemModuleLoader::CleanupOrphanedItemData()
{
    LOG_INFO("module.itemidentification", "[启动清理] 开始全量清理孤立的物品数据...");

    auto startTime = std::chrono::high_resolution_clock::now();
    uint32 totalCleaned = 0;

    // 说明：
    // 1) 用 JOIN 代替 NOT IN 子查询，降低锁竞争
    // 2) 启动阶段同步提交，确保 world 网络监听开始前清理已经完成
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // ★★★ 关键修复：清理待鉴定物品标记表的孤立数据 ★★★
    // 原因：玩家拾取物品后没有手动鉴定就删除/出售/交易物品，导致待鉴定标记永久保留
    // 注意：bag=200 是飞升系统虚拟背包，清理时必须视为有效引用，避免误删飞升装备扩展属性
    trans->Append(
        "DELETE p FROM `待鉴定物品标记` p "
        "LEFT JOIN `item_instance` i ON p.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = p.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE r FROM `物品_鉴定记录` r "
        "LEFT JOIN `item_instance` i ON r.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = r.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE a FROM `物品属性_数据` a "
        "LEFT JOIN `item_instance` i ON a.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = a.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE g FROM `物品成长_玩家记录` g "
        "LEFT JOIN `item_instance` i ON g.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = g.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE e FROM `物品强化_记录` e "
        "LEFT JOIN `item_instance` i ON e.`guid` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = e.`guid` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE s FROM `_物品技能_数据` s "
        "LEFT JOIN `item_instance` i ON s.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = s.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE m FROM `魔次系统_数据` m "
        "LEFT JOIN `item_instance` i ON m.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = m.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE r FROM `符文系统_数据` r "
        "LEFT JOIN `item_instance` i ON r.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = r.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE p FROM `_物品套装_数据` p "
        "LEFT JOIN `item_instance` i ON p.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = p.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE h FROM `玩家装备属性增强` h "
        "LEFT JOIN `item_instance` i ON h.`装备GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = h.`装备GUID` "
        "LEFT JOIN `mail_items` mi ON mi.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `auctionhouse` ah ON ah.`itemguid` = h.`装备GUID` "
        "LEFT JOIN `guild_bank_item` gbi ON gbi.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `item_refund_instance` iri ON iri.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `character_gifts` cg ON cg.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `character_equipmentsets` ces ON h.`装备GUID` IN ("
        "ces.`item0`, ces.`item1`, ces.`item2`, ces.`item3`, ces.`item4`, ces.`item5`, "
        "ces.`item6`, ces.`item7`, ces.`item8`, ces.`item9`, ces.`item10`, ces.`item11`, "
        "ces.`item12`, ces.`item13`, ces.`item14`, ces.`item15`, ces.`item16`, ces.`item17`, "
        "ces.`item18`) "
        // 双保险：item_instance 不存在 + 所有持久化引用表也不存在，才允许删除倍率行。
        // 之前只看 item_instance + bag=200，一旦上游清理误删 item_instance，
        // 这里就会把上架/邮件中装备的倍率属性一起清掉，玩家重启后会反映"装备倍率丢了"。
        "WHERE i.`guid` IS NULL "
        "AND ci.`item` IS NULL "
        "AND mi.`item_guid` IS NULL "
        "AND ah.`itemguid` IS NULL "
        "AND gbi.`item_guid` IS NULL "
        "AND iri.`item_guid` IS NULL "
        "AND cg.`item_guid` IS NULL "
        "AND ces.`setguid` IS NULL");

    CharacterDatabase.DirectCommitTransaction(trans);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    LOG_INFO("module.itemidentification",
        "[启动清理] 全量孤立数据清理已同步完成，耗时 {} ms",
        duration);
}

// ★★★ 定期清理待鉴定物品标记表的孤立数据 ★★★
// 原因：玩家拾取物品后没有手动鉴定就删除/出售/交易物品，导致待鉴定标记永久保留
// 这个函数每30分钟执行一次，确保数据库不会无限膨胀
void ItemIdentificationSystemModuleLoader::CleanupPendingIdentificationData()
{
    LOG_INFO("module.itemidentification", "[定期清理] 开始清理待鉴定物品标记表的孤立数据...");

    auto startTime = std::chrono::high_resolution_clock::now();
    static constexpr uint32 kBatchSize = 5000;
    static constexpr uint32 kMaxBatchesPerRun = 10;

    uint32 totalDeleted = 0;
    bool hasMoreOrphans = false;

    for (uint32 batch = 0; batch < kMaxBatchesPerRun; ++batch)
    {
        std::vector<uint32> orphanGuids;
        orphanGuids.reserve(kBatchSize);

        QueryResult result = CharacterDatabase.Query(
            "SELECT p.`物品GUID` "
            "FROM `待鉴定物品标记` p "
            "LEFT JOIN `item_instance` i ON p.`物品GUID` = i.`guid` "
            "LEFT JOIN `character_inventory` ci ON ci.`item` = p.`物品GUID` AND ci.`bag` = 200 "
            "WHERE i.`guid` IS NULL AND ci.`item` IS NULL "
            "ORDER BY p.`物品GUID` ASC "
            "LIMIT {}",
            kBatchSize);

        if (!result)
        {
            hasMoreOrphans = false;
            break;
        }

        do
        {
            orphanGuids.push_back(result->Fetch()[0].Get<uint32>());
        } while (result->NextRow());

        if (orphanGuids.empty())
        {
            hasMoreOrphans = false;
            break;
        }

        std::ostringstream deleteSql;
        deleteSql << "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` IN (";
        for (size_t i = 0; i < orphanGuids.size(); ++i)
        {
            if (i > 0)
                deleteSql << ",";
            deleteSql << orphanGuids[i];
        }
        deleteSql << ")";

        // 全服无人在线时，使用同步分批删除，避免一次性大事务长期占用表锁。
        CharacterDatabase.DirectExecute(deleteSql.str());
        totalDeleted += static_cast<uint32>(orphanGuids.size());

        if (orphanGuids.size() < kBatchSize)
        {
            hasMoreOrphans = false;
            break;
        }

        hasMoreOrphans = true;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    LOG_INFO("module.itemidentification",
        "[定期清理] 待鉴定孤立数据分批清理完成，本次删除 {} 条，耗时 {} ms{}",
        totalDeleted,
        duration,
        hasMoreOrphans ? "，仍有剩余孤立数据，等待下一个空闲周期继续清理" : "");
}

// 【关键修复】服务器关闭时保存数据
void ItemIdentificationSystemModuleLoader::OnShutdownInitiate(ShutdownExitCode /*code*/, ShutdownMask /*mask*/)
{
    LOG_INFO("module.itemidentification", ">> 鉴定系统: 服务器正在关闭，确保所有数据已保存...");

    // 鉴定系统的数据通常在鉴定时立即保存，但这里可以刷新缓存
    // 确保所有待处理的数据库操作完成
    // 由于使用的是异步Execute，这些操作已经在队列中了

    LOG_INFO("module.itemidentification", ">> 鉴定系统: 数据保存完成");
}

// 玩家脚本实现
ItemIdentificationPlayerScript::ItemIdentificationPlayerScript() : PlayerScript("ItemIdentificationPlayerScript") { }

void ItemIdentificationPlayerScript::OnPlayerLogin(Player* player)
{
    if (!player || !sItemIdentificationSystem->_enabled)
        return;

    // 【性能优化】禁用登录时的预加载，改为完全按需加载
    // 原因：多人同时登录时，即使异步查询也会导致数据库过载
    // 100个玩家 × 50个物品 × 1次UNION查询 = 5000次并发查询 → 数据库卡死
    //
    // 新策略：客户端通过 addon 消息按需查询，已有24小时缓存，性能足够
    // sItemIdentificationSystem->PreloadPlayerEquipment(player);  // 已禁用

    // 删除旧的登录时主动发送逻辑 - 现在客户端会在需要时通过 addon 消息自动查询
    // 旧代码：收集所有已鉴定物品并通过 SendIdentificationDataToClient 发送 - 已删除
    // 新逻辑：客户端悬停物品时自动发送 UITQ QUERY 请求，服务器通过 HandleAddonBatchQuery 响应

    // 【关键修复】在登录完成后立即执行一次统一的属性刷新
    // 原因：OnPlayerEquip 中为了优化性能，使用 isBeingLoaded() 跳过了属性刷新
    // 虽然幻境系统的OnPlayerLogin会通过延迟事件刷新，但延迟时间过长可能导致客户端超时
    // 必须在这里立即刷新一次，确保玩家能正常进入游戏
    // 后续幻境系统的延迟刷新会再次更新，不会有冲突
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
}

// 鉴定记录管理实现
bool ItemIdentificationSystem::IsItemIdentified(uint32 itemGuid)
{
    // 【审计修复】如果缓存未初始化，回退到数据库查询
    if (!_cacheInitialized)
    {
        // 缓存还没初始化，回退到数据库查询以确保重复鉴定保护生效
        QueryResult result = CharacterDatabase.Query(
            "SELECT 1 FROM `物品_鉴定记录` WHERE `物品GUID` = {} LIMIT 1",
            itemGuid
        );
        if (result)
        {
            // 顺便更新缓存
            std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
            _identifiedItemsCache.insert(itemGuid);
            return true;
        }
        return false;
    }

    // 【线程安全修复】加锁读取缓存
    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
    return _identifiedItemsCache.find(itemGuid) != _identifiedItemsCache.end();
}

void ItemIdentificationSystem::SaveIdentificationRecord(const ItemIdentificationRecord& record)
{
    // 【审计修复】对字符串字段进行SQL转义，防止SQL注入和语法错误
    std::string escapedBaseAttrDetails = record.baseAttrDetails;
    std::string escapedAdditionalAttrGroups = record.additionalAttrGroups;
    std::string escapedSkillGroups = record.skillGroups;
    std::string escapedMagicHitGroups = record.magicHitGroups;

    CharacterDatabase.EscapeString(escapedBaseAttrDetails);
    CharacterDatabase.EscapeString(escapedAdditionalAttrGroups);
    CharacterDatabase.EscapeString(escapedSkillGroups);
    CharacterDatabase.EscapeString(escapedMagicHitGroups);

    // 使用 INSERT ... ON DUPLICATE KEY UPDATE 避免重复键错误
    // 【修复】使用 DirectExecute 同步写入数据库
    // 原因：异步写入时，后续的 SendAllModuleDataAddon 查询可能还没有写入完成
    // 导致鉴定后需要重启服务器才能看到数据
    CharacterDatabase.DirectExecute(
        "INSERT INTO 物品_鉴定记录 ("
        "玩家GUID, 物品GUID, 物品ID, 鉴定模板ID, "
        "是否获得成长, 成长组ID, "
        "是否获得强化, 强化组ID, "
        "是否获得基础属性, 基础属性数量, 基础属性组ID, 基础属性详情, "
        "是否获得追加属性, 追加属性数量, 追加属性组ID列表, "
        "是否获得符文凹槽, 符文凹槽数量, "
        "是否获得技能, 技能组ID列表, "
        "是否获得魔次, 魔次数量, 魔次组ID列表, "
        "是否获得套装, 套装组ID, 套装ID, "
        "消耗金币, 成功率"
        ") VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "玩家GUID = VALUES(玩家GUID), "
        "物品ID = VALUES(物品ID), "
        "鉴定模板ID = VALUES(鉴定模板ID), "
        "是否获得成长 = VALUES(是否获得成长), "
        "成长组ID = VALUES(成长组ID), "
        "是否获得强化 = VALUES(是否获得强化), "
        "强化组ID = VALUES(强化组ID), "
        "是否获得基础属性 = VALUES(是否获得基础属性), "
        "基础属性数量 = VALUES(基础属性数量), "
        "基础属性组ID = VALUES(基础属性组ID), "
        "基础属性详情 = VALUES(基础属性详情), "
        "是否获得追加属性 = VALUES(是否获得追加属性), "
        "追加属性数量 = VALUES(追加属性数量), "
        "追加属性组ID列表 = VALUES(追加属性组ID列表), "
        "是否获得符文凹槽 = VALUES(是否获得符文凹槽), "
        "符文凹槽数量 = VALUES(符文凹槽数量), "
        "是否获得技能 = VALUES(是否获得技能), "
        "技能组ID列表 = VALUES(技能组ID列表), "
        "是否获得魔次 = VALUES(是否获得魔次), "
        "魔次数量 = VALUES(魔次数量), "
        "魔次组ID列表 = VALUES(魔次组ID列表), "
        "是否获得套装 = VALUES(是否获得套装), "
        "套装组ID = VALUES(套装组ID), "
        "套装ID = VALUES(套装ID), "
        "消耗金币 = VALUES(消耗金币), "
        "成功率 = VALUES(成功率)",
        record.playerGuid, record.itemGuid, record.itemEntry, record.templateId,
        record.hasGrowth ? 1 : 0, record.growthGroup,
        record.hasEnhancement ? 1 : 0, record.enhancementGroup,
        record.hasBaseAttributes ? 1 : 0, record.baseAttrCount, record.baseAttrGroup, escapedBaseAttrDetails,
        record.hasAdditionalAttributes ? 1 : 0, record.additionalAttrCount, escapedAdditionalAttrGroups,
        record.hasRuneSlots ? 1 : 0, record.runeSlotCount,
        record.hasSkills ? 1 : 0, escapedSkillGroups,
        record.hasMagicHits ? 1 : 0, record.magicHitCount, escapedMagicHitGroups,
        record.hasSet ? 1 : 0, record.setGroup, record.setId,
        record.costGold, record.successRate
    );

    // 【审计修复】使用锁保护缓存写入，防止数据竞争
    {
        std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
        _identifiedItemsCache.insert(record.itemGuid);
    }

    DebugLog("保存鉴定记录: 玩家GUID={}, 物品GUID={}, 模板ID={}",
             record.playerGuid, record.itemGuid, record.templateId);
}

ItemIdentificationRecord* ItemIdentificationSystem::GetIdentificationRecord(uint32 itemGuid)
{
    // 这里简化实现，实际应用中可以缓存记录
    // 暂时返回nullptr，完整实现需要从数据库加载
    return nullptr;
}

// SendAllModuleData 和 SendIdentificationDataToClient 已删除
// 现在使用 addon 格式的 HandleAddonBatchQuery 替代旧的单独发送逻辑
// 客户端通过 addon 消息 (UITQ) 接收批量数据，不再使用聊天消息

void ItemIdentificationSystem::RefreshItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    // 保存物品到数据库
    item->SaveToDB(nullptr);

    // 如果物品已装备
    if (item->IsEquipped())
    {
        // 更新装备槽位显示
        // 刷新物品显示到客户端
        player->SetVisibleItemSlot(item->GetSlot(), item);

        // 只在玩家完全加载且在世界中时才应用属性效果
        // 避免在登录加载阶段调用导致空指针崩溃
#ifdef MODULE_ITEM_ATTRIBUTES
        if (!player->isBeingLoaded() && player->IsInWorld() && player->GetSession())
        {
            if (sItemAttributesEffects)
            {
                // 重新同步该装备的自定义属性效果（先移除再应用一次）
                sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
            }
        }
#endif

        DebugLog("刷新已装备物品: GUID={}, 槽位={}",
                 item->GetGUID().GetCounter(), item->GetSlot());
    }
    else
    {
        // 背包物品只需保存，不需要更新显示
        item->SetState(ITEM_CHANGED, player);

        DebugLog("刷新背包物品: GUID={}", item->GetGUID().GetCounter());
    }

}

// 物品装备脚本 - 阻止已鉴定物品的官方属性生效
class ItemIdentificationEquipScript : public PlayerScript
{
private:
    // 存储每个玩家每个槽位的已鉴定物品信息
    // Key: PlayerGUID, Value: map<slot, pair<itemGUID, itemEntry>>
    std::map<uint64, std::map<uint8, std::pair<uint64, uint32>>> _identifiedEquippedItems;

public:
    ItemIdentificationEquipScript() : PlayerScript("ItemIdentificationEquipScript") {}

    // 在装备物品后触发：这里只记录装备信息，不再直接修改基础属性
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        if (!player || !item)
            return;

        uint32 itemGuid = item->GetGUID().GetCounter();
        if (!sItemIdentificationSystem->IsItemIdentified(itemGuid))
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();
        _identifiedEquippedItems[playerGuid][slot] = std::make_pair(itemGuid, item->GetEntry());
    }

    // 在脱下装备时触发：仅清理记录
    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override
    {
        if (!player)
            return;

        if (item == nullptr)
        {
            uint64 playerGuid = player->GetGUID().GetCounter();
            auto playerIt = _identifiedEquippedItems.find(playerGuid);
            if (playerIt == _identifiedEquippedItems.end())
                return;

            auto& playerSlots = playerIt->second;
            auto slotIt = playerSlots.find(slot);
            if (slotIt == playerSlots.end())
                return;

            playerSlots.erase(slotIt);
        }
    }

    // 通过钩子在应用物品基础属性前扩展逻辑（当前仅保留验收点，不再屏蔽官方五维属性）
    void OnPlayerApplyItemModsBefore(Player* player, uint8 slot, bool /*apply*/, uint8 /*itemProtoStatNumber*/, uint32 /*statType*/, int64& val) override
    {
        if (!player || val == 0)
            return;

        // 只处理玩家身上已装备的物品槽位
        if (slot >= EQUIPMENT_SLOT_END)
            return;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            return;

        uint32 itemGuid = item->GetGUID().GetCounter();
        if (!sItemIdentificationSystem->IsItemIdentified(itemGuid))
            return;

        // 现在不再修改 val，让官方模板中的力量/敏捷/耐力/智力/精神正常生效，
        // 自定义属性系统只做"额外加成"，避免装备基础五维被清空。
    }

    // 玩家登出时清理数据
    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();
        _identifiedEquippedItems.erase(playerGuid);
    }
};

// ✅ 新增：初始化已鉴定物品缓存
// 【线程安全修复】添加锁保护
void ItemIdentificationSystem::InitializeCache()
{
    PerformanceTimer timer("初始化已鉴定物品缓存");

    // 一次性从数据库加载所有已鉴定物品的GUID
    QueryResult result = CharacterDatabase.Query("SELECT `物品GUID` FROM `物品_鉴定记录`");

    // 【线程安全修复】使用锁保护缓存写入
    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
    _identifiedItemsCache.clear();

    if (!result)
    {
        _cacheInitialized = true;
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemGuid = fields[0].Get<uint32>();
        _identifiedItemsCache.insert(itemGuid);
        count++;
    } while (result->NextRow());

    _cacheInitialized = true;
}

// ✅ 新增：清除指定物品的鉴定缓存
// 【线程安全修复】添加锁保护
void ItemIdentificationSystem::ClearIdentifiedCache(uint32 itemGuid)
{
    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
    _identifiedItemsCache.erase(itemGuid);
    DebugLog("清除物品GUID {} 的鉴定缓存", itemGuid);
}

// ✅ 新增：批量检查物品是否已鉴定（优化版）
// 【线程安全修复】添加锁保护
std::set<uint32> ItemIdentificationSystem::BatchCheckIdentified(const std::vector<uint32>& itemGuids)
{
    PerformanceTimer timer("批量检查已鉴定物品");

    // 【性能优化】如果缓存未初始化，返回空集合而不是同步初始化
    if (!_cacheInitialized)
    {
        DebugLog("批量检查：缓存未初始化，返回空集合");
        return std::set<uint32>();
    }

    std::set<uint32> identifiedSet;

    // 【线程安全修复】加锁读取缓存
    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
    for (uint32 guid : itemGuids)
    {
        if (_identifiedItemsCache.find(guid) != _identifiedItemsCache.end())
        {
            identifiedSet.insert(guid);
        }
    }

    DebugLog("批量检查 {} 个物品，找到 {} 个已鉴定", itemGuids.size(), identifiedSet.size());
    return identifiedSet;
}

// 物品掉落监控脚本
class ItemIdentificationLootScript : public PlayerScript
{
public:
    ItemIdentificationLootScript() : PlayerScript("ItemIdentificationLootScript") {}

    // 玩家从战利品中获得物品时触发
    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid lootguid) override
    {
        if (!player || !item || !sItemIdentificationSystem->_enabled)
            return;

        // 只监控装备类物品
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
            return;

        // 【关键修复】物品进入背包后，主动向客户端发送属性数据
        // 这样客户端无需等待小退即可显示自定义属性
        uint32 itemEntry = item->GetEntry();
        uint32 itemGuid = item->GetGUID().GetCounter();
        // 【崩溃修复】捕获玩家GUID而非原始指针，避免玩家登出后悬空指针访问
        ObjectGuid playerGuid = player->GetGUID();

        // 延迟发送，确保物品数据已经完全准备好
        // 使用500ms延迟，因为物品可能需要一点时间才能完全初始化
        player->m_Events.AddEventAtOffset([playerGuid, itemEntry, itemGuid]()
        {
            // 通过GUID安全查找玩家，避免悬空指针
            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (p && p->IsInWorld())
            {
                sItemIdentificationSystem->SendAllModuleDataAddon(p, itemEntry, itemGuid);
                LOG_DEBUG("module", "[物品掉落同步] 已向玩家 {} 发送物品属性: itemEntry={}, guid={}",
                    p->GetName(), itemEntry, itemGuid);
            }
        }, 500ms);
    }

    // 玩家通过其他方式获得物品时触发（创建、任务奖励等）
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override
    {
        if (!player || !item || !sItemIdentificationSystem->_enabled)
            return;

        // 只监控装备类物品
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
            return;

        // 【关键修复】物品进入背包后，主动向客户端发送属性数据
        uint32 itemEntry = item->GetEntry();
        uint32 itemGuid = item->GetGUID().GetCounter();
        // 【崩溃修复】捕获玩家GUID而非原始指针，避免玩家登出后悬空指针访问
        ObjectGuid playerGuid = player->GetGUID();

        // 延迟发送，确保物品数据已经完全准备好
        player->m_Events.AddEventAtOffset([playerGuid, itemEntry, itemGuid]()
        {
            // 通过GUID安全查找玩家，避免悬空指针
            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (p && p->IsInWorld())
            {
                sItemIdentificationSystem->SendAllModuleDataAddon(p, itemEntry, itemGuid);
                LOG_DEBUG("module", "[物品存储同步] 已向玩家 {} 发送物品属性: itemEntry={}, guid={}",
                    p->GetName(), itemEntry, itemGuid);
            }
        }, 500ms);
    }
};

// Addon消息处理脚本
class ItemIdentificationAddonScript : public PlayerScript
{
public:
    ItemIdentificationAddonScript() : PlayerScript("ItemIdentificationAddonScript") { }

    // 监听玩家聊天事件，拦截Addon消息
    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        // 只处理Addon消息
        if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        {
            return;
        }

        // 注意：服务器收到的消息格式是 "UITQ<TAB>QUERY:bag:slot:itemID"（新格式）
        // 或 "UITQ<TAB>QUERY:itemID:guid"（旧格式，保持兼容）
        // 需要先移除"UITQ<TAB>"前缀，然后解析参数

        // 查找TAB字符的位置
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
        {
            return;
        }

        // 提取前缀部分（应该是"UITQ"）
        std::string prefix = msg.substr(0, tabPos);
        if (prefix != "UITQ")
        {
            return;
        }

        // 提取消息内容（TAB之后的部分）
        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "ITEMIDENT"))
            return;

        std::string command = msg.substr(tabPos + 1);

        // 处理INSPECT_ITEM_GUID请求
        if (command.find("INSPECT_ITEM_GUID:") == 0)
        {
            HandleInspectItemGuidRequest(player, command.substr(18));  // 去掉"INSPECT_ITEM_GUID:"
            return;
        }

        // ========== 【新增】处理鉴定相关命令 ==========
        // IDENTIFY:<itemGuid> - 鉴定单个物品
        if (command.find("IDENTIFY:") == 0)
        {
            HandleIdentifyRequest(player, command.substr(9));  // 去掉"IDENTIFY:"
            return;
        }

        // IDENTIFY_BATCH - 批量鉴定所有待鉴定物品
        if (command == "IDENTIFY_BATCH")
        {
            HandleBatchIdentifyRequest(player);
            return;
        }

        // LIST_PENDING - 查询待鉴定物品列表
        if (command == "LIST_PENDING")
        {
            HandleListPendingRequest(player);
            return;
        }

        if (command.find("QUERY_TEMPLATE:") == 0)
        {
            HandleTemplateStatsRequest(player, command.substr(15));
            return;
        }

        // 解析QUERY命令
        if (command.find("QUERY:") != 0)
            return;

        // 提取参数
        std::string params = command.substr(6);  // 去掉"QUERY:"

        // 【修改】新格式：QUERY:bag:slot:itemID（WoW 3.3.5客户端物品链接不包含GUID）
        // bag=255表示装备栏，0-4表示背包，-1/5-11表示银行
        std::vector<std::string> parts;
        std::istringstream stream(params);
        std::string part;
        while (std::getline(stream, part, ':'))
        {
            parts.push_back(part);
        }

        // 兼容旧格式：QUERY:itemID:guid（两个参数）
        // 新格式：QUERY:bag:slot:itemID（三个参数）
        uint32 itemID = 0;
        uint32 guid = 0;
        int32 bag = 255;   // 默认值255表示装备栏（旧格式兼容）
        uint8 slot = 0;

        if (parts.size() == 2)
        {
            // 旧格式：itemID:guid
            try
            {
                itemID = std::stoul(parts[0]);
                guid = std::stoul(parts[1]);
            }
            catch (...)
            {
                return;
            }

            if (itemID == 0 || guid == 0)
                return;
        }
        else if (parts.size() >= 3)
        {
            // 新格式：bag:slot:itemID
            try
            {
                bag = std::stoi(parts[0]);
                int32 parsedSlot = std::stoi(parts[1]);
                itemID = std::stoul(parts[2]);

                if (parsedSlot < 0 || parsedSlot > 255)
                    return;

                slot = static_cast<uint8>(parsedSlot);
            }
            catch (...)
            {
                return;
            }

            if (itemID == 0)
                return;

            // 根据bag和slot获取物品
            Item* item = nullptr;

            if (bag == 255)
            {
                // bag=255表示装备栏
                // 客户端GetInventorySlotInfo返回1-based槽位，需要转换为0-based
                // 例如：HeadSlot客户端返回1，服务器端EQUIPMENT_SLOT_HEAD=0
                uint8 equipSlot = (slot > 0) ? (slot - 1) : slot;
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification",
                             "[QUERY] 装备栏查询: 客户端slot={}, 服务器equipSlot={}, itemID={}",
                             slot, equipSlot, itemID);
                }
            }
            else if (bag == 0)
            {
                // bag=0是主背包
                // WoW主背包槽位从1开始（客户端），服务器端需要转换
                uint8 bagSlot = (slot > 0) ? (slot - 1) : slot;
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + bagSlot);

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification",
                             "[QUERY] 主背包查询: 客户端slot={}, 服务器bagSlot={}, itemID={}",
                             slot, INVENTORY_SLOT_ITEM_START + bagSlot, itemID);
                }
            }
            else if (bag >= 1 && bag <= 4)
            {
                // bag=1-4是额外背包
                // 客户端发送的bag需要转换为服务器的背包槽位
                uint8 serverBagSlot = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
                if (Bag* bagPtr = player->GetBagByPos(serverBagSlot))
                {
                    // 背包内的槽位是0-based
                    if (slot > 0)
                        item = bagPtr->GetItemByPos(slot - 1);
                    else
                        item = bagPtr->GetItemByPos(slot);
                }
            }
            else if (bag == -1)
            {
                // bag=-1是主银行
                uint8 bankSlot = (slot > 0) ? (slot - 1) : slot;
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, BANK_SLOT_ITEM_START + bankSlot);

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification",
                             "[QUERY] 银行主仓查询: 客户端slot={}, 服务器bankSlot={}, itemID={}",
                             slot, BANK_SLOT_ITEM_START + bankSlot, itemID);
                }
            }
            else if (bag >= 5 && bag <= 11)
            {
                // bag=5-11是银行背包
                uint8 serverBagSlot = BANK_SLOT_BAG_START + static_cast<uint8>(bag - 5);
                if (Bag* bagPtr = player->GetBagByPos(serverBagSlot))
                {
                    if (slot > 0)
                        item = bagPtr->GetItemByPos(slot - 1);
                    else
                        item = bagPtr->GetItemByPos(slot);
                }

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification",
                             "[QUERY] 银行背包查询: 客户端bag={}, slot={}, 服务器bagSlot={}, itemID={}",
                             bag, slot, serverBagSlot, itemID);
                }
            }

            // 验证物品
            if (!item)
            {
                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification",
                             "[QUERY] 未找到物品: bag={}, slot={}, itemID={}",
                             bag, slot, itemID);
                }
                return;
            }

            // 验证物品ID是否匹配
            if (item->GetEntry() != itemID)
            {
                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification",
                             "[QUERY] 物品ID不匹配: bag={}, slot={}, 期望itemID={}, 实际itemID={}",
                             bag, slot, itemID, item->GetEntry());
                }
                return;
            }

            // 获取物品的真实GUID
            guid = item->GetGUID().GetCounter();

            if (sItemIdentificationSystem->_debugMode)
            {
                LOG_INFO("module.itemidentification",
                         "[QUERY] 通过位置找到物品: bag={}, slot={}, itemID={}, realGUID={}",
                         bag, slot, itemID, guid);
            }
        }
        else
        {
            return;  // 参数数量不对
        }

        if (itemID == 0 || guid == 0)
        {
            return;
        }

        // 执行批量查询并通过Addon消息返回结果
        // 【修改】传递bag和slot，用于客户端精确匹配响应
        HandleAddonBatchQuery(player, itemID, guid, bag, slot);
    }

private:
    void HandleTemplateStatsRequest(Player* player, const std::string& params)
    {
        if (!player || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
            return;

        uint32 itemID = 0;
        try
        {
            itemID = std::stoul(params);
        }
        catch (...)
        {
            return;
        }

        if (!itemID)
            return;

        std::string templateStatsData = BuildTemplateStatsData(itemID);
        if (templateStatsData.empty())
            return;

        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << itemID << ":0:"
                 << ":"  // baseAttributes
                 << ":"  // additionalAttributes
                 << ":"  // identificationDisplayData
                 << ":"  // growthData
                 << ":"  // enhancementData
                 << ":"  // skillsData
                 << ":"  // magicHitData
                 << ":"  // runeData
                 << ":"  // setData
                 << ":"  // huanjingData
                 << templateStatsData;

        SendItemIdentificationAddonMessage(player, response.str());
    }

    // 处理查询其他玩家装备GUID的请求
    // 【安全修复】添加权限校验，防止未授权查看他人装备
    void HandleInspectItemGuidRequest(Player* requester, const std::string& params)
    {
        if (!requester)
            return;

        // 解析参数：playerName:slot:itemID
        std::vector<std::string> parts;
        std::istringstream stream(params);
        std::string part;
        while (std::getline(stream, part, ':'))
        {
            parts.push_back(part);
        }

        if (parts.size() < 3)
            return;

        std::string targetPlayerName = parts[0];
        uint8 slot = 0;
        uint32 itemID = 0;

        try
        {
            slot = static_cast<uint8>(std::stoul(parts[1]));
            itemID = std::stoul(parts[2]);
        }
        catch (...)
        {
            return;
        }

        // 槽位转换：客户端使用1-based，服务端使用0-based
        uint8 serverSlot = (slot > 0) ? (slot - 1) : slot;

        // 查找目标玩家
        Player* targetPlayer = ObjectAccessor::FindPlayerByName(targetPlayerName);
        if (!targetPlayer)
        {
            // 玩家不在线，返回空响应
            std::string response = "INSPECT_ITEM_GUID_RESPONSE:" + targetPlayerName + ":" +
                                   std::to_string(slot) + ":" + std::to_string(itemID) + ":0";
            SendItemIdentificationAddonMessage(requester, response);
            return;
        }

        // 【安全修复】权限校验：只允许以下情况查看装备信息
        // 1. 查看自己的装备
        // 2. GM权限玩家
        // 3. 目标玩家在附近（30码内）且请求者正在观察状态（模拟inspect行为）
        bool canInspect = false;

        if (requester->GetGUID() == targetPlayer->GetGUID())
        {
            // 查看自己的装备，始终允许
            canInspect = true;
        }
        else if (requester->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        {
            // GM权限可以查看任何人的装备
            canInspect = true;
        }
        else if (requester->IsWithinDistInMap(targetPlayer, 30.0f))
        {
            // 在30码范围内，允许查看（模拟正常的inspect行为）
            canInspect = true;
        }

        if (!canInspect)
        {
            // 无权限查看，返回空响应
            std::string response = "INSPECT_ITEM_GUID_RESPONSE:" + targetPlayerName + ":" +
                                   std::to_string(slot) + ":" + std::to_string(itemID) + ":0";
            SendItemIdentificationAddonMessage(requester, response);
            return;
        }

        // 获取目标玩家指定槽位的装备
        Item* item = targetPlayer->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
        uint32 realGuid = 0;

        if (item && item->GetEntry() == itemID)
        {
            realGuid = item->GetGUID().GetCounter();
        }

        // 发送响应
        std::ostringstream response;
        response << "INSPECT_ITEM_GUID_RESPONSE:" << targetPlayerName << ":"
                 << static_cast<uint32>(slot) << ":" << itemID << ":" << realGuid;

        SendItemIdentificationAddonMessage(requester, response.str());
    }

    // 【修复】辅助函数：在玩家背包和装备中查找指定itemID的物品的真实GUID
    // 返回所有匹配物品的GUID列表（因为玩家可能有多个相同itemID的物品）
    std::vector<uint32> FindItemGuidsByItemId(Player* player, uint32 itemID)
    {
        std::vector<uint32> guids;

        if (!player)
            return guids;

        // 1. 搜索装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetEntry() == itemID)
            {
                guids.push_back(item->GetGUID().GetCounter());
            }
        }

        // 2. 搜索主背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetEntry() == itemID)
            {
                guids.push_back(item->GetGUID().GetCounter());
            }
        }

        // 3. 搜索背包袋
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item = bag->GetItemByPos(j);
                    if (item && item->GetEntry() == itemID)
                    {
                        guids.push_back(item->GetGUID().GetCounter());
                    }
                }
            }
        }

        return guids;
    }

    // 【修改】添加bag和slot参数，用于客户端精确匹配响应
    void HandleAddonBatchQuery(Player* player, uint32 itemID, uint32 guid, int32 bag = 255, uint8 slot = 0)
    {
        if (!player)
        {
            return;
        }

        if (!sItemIdentificationSystem->_enabled)
        {
            return;
        }



        // 执行批量查询（使用现有的查询逻辑）
        auto queryStart = std::chrono::high_resolution_clock::now();

        // 优先从缓存读取
        uint64 cacheKey = ((uint64)itemID << 32) | guid;

        ItemIdentificationSystem::AllModuleData moduleData;  // 改名避免与WorldPacket data冲突
        bool cacheHit = false;
        uint32 usedGuid = guid;  // 记录最终使用的GUID

        // 【线程安全】加锁读取缓存
        {
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);

            auto it = sItemIdentificationSystem->_batchQueryCache.find(cacheKey);

            // 【线程安全修复】移除此处的直接统计更新，改为在函数末尾统一更新

            if (it != sItemIdentificationSystem->_batchQueryCache.end())
            {
                // 检查缓存是否过期
                time_t now = time(nullptr);
                if ((now - it->second.cacheTime) < sItemIdentificationSystem->BATCH_CACHE_EXPIRE_TIME)
                {
                    // 缓存有效，直接使用
                    moduleData = it->second.data;
                    cacheHit = true;
                }
                else
                {
                    // 缓存过期，删除
                    sItemIdentificationSystem->_batchQueryCache.erase(it);
                }
            }
        }

        if (!cacheHit)
        {
            // 缓存未命中，在锁外查询数据库（避免长时间持有锁）
            moduleData = sItemIdentificationSystem->QueryAllModuleData(itemID, guid);

            // 【关键修复】移除GUID回退逻辑！
            // 原因：当玩家有多个相同itemID的物品时（一个已鉴定，一个新掉落），
            // GUID回退会错误地返回已鉴定物品的数据给新物品。
            // 现在只返回当前物品的精确数据，如果没有数据就返回空（让客户端显示"待鉴定"状态）
            //
            // 旧逻辑（已禁用）：
            // if (!moduleData.hasData) {
            //     std::vector<uint32> realGuids = FindItemGuidsByItemId(player, itemID);
            //     for (uint32 realGuid : realGuids) { ... }
            // }



            // 【线程安全】加锁写入缓存（使用当前物品的GUID作为缓存键）
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
            ItemIdentificationSystem::BatchQueryCache cache;
            cache.data = moduleData;
            cache.cacheTime = time(nullptr);
            sItemIdentificationSystem->_batchQueryCache[cacheKey] = cache;
        }

        // 构建Addon响应消息
        // 【修改】新格式：ALL_MODULE_DATA:bag:slot:itemID:guid:base:additional:identDisplay:growth:enhancement:skills:magic:rune:set:huanjing:templateStats
        // 添加bag:slot用于客户端精确匹配响应到正确的pending记录
        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << bag << ":" << static_cast<uint32>(slot) << ":" << itemID << ":" << guid << ":"
                 << moduleData.baseAttributes << ":"
                 << moduleData.additionalAttributes << ":"
                 << moduleData.identificationDisplayData << ":"
                 << moduleData.growthData << ":"
                 << moduleData.enhancementData << ":"
                 << moduleData.skillsData << ":"
                 << moduleData.magicHitData << ":"
                 << moduleData.runeData << ":"
                 << moduleData.setData << ":"
                 << moduleData.huanjingData;
        if (!moduleData.pendingIdentifyData.empty())
            response << ":" << moduleData.pendingIdentifyData;
        response << ":" << moduleData.templateStatsData;

        std::string responseStr = response.str();

        SendItemIdentificationAddonMessage(player, responseStr);

        // 计算查询耗时
        auto queryEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart).count();

        // 【线程安全修复】使用封装方法更新性能统计
        sItemIdentificationSystem->UpdatePerfStats(duration, cacheHit, !cacheHit);

        // 无论是否开启调试，当批量查询耗时较长时输出性能日志（阈值：>= 20ms）
        if (duration >= 20000)
        {
        }

        if (sItemIdentificationSystem->_debugMode)
        {
            LOG_INFO("module.itemidentification",
                     "[Addon响应] 已发送数据: itemID={}, guid={}, 消息长度={}, 缓存命中={}, 耗时={}μs",
                     itemID, guid, responseStr.length(), cacheHit, duration);
        }
    }

    // ========== 【新增】鉴定相关Addon命令处理函数 ==========

    // 辅助函数：发送Addon消息给玩家
    void SendAddonResponse(Player* player, const std::string& response)
    {
        if (!player)
            return;

        SendItemIdentificationAddonMessage(player, response);
    }

    // 辅助函数：在玩家背包中通过GUID查找物品
    Item* FindItemByGuid(Player* player, uint32 itemGuid)
    {
        if (!player)
            return nullptr;

        // 1. 搜索装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        // 2. 搜索主背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        // 3. 搜索背包袋
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item = bag->GetItemByPos(j);
                    if (item && item->GetGUID().GetCounter() == itemGuid)
                        return item;
                }
            }
        }

        return nullptr;
    }

    // 处理单个物品鉴定请求: IDENTIFY:<itemGuid>
    void HandleIdentifyRequest(Player* player, const std::string& params)
    {
        if (!player)
            return;

        uint32 itemGuid = 0;
        try
        {
            itemGuid = std::stoul(params);
        }
        catch (...)
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:无效的物品GUID格式");
            return;
        }

        if (itemGuid == 0)
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:无效的物品GUID");
            return;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();

        // 1. 从数据库查询待鉴定标记
        QueryResult result = CharacterDatabase.Query(
            "SELECT `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `物品GUID` = {} AND `玩家GUID` = {}",
            itemGuid, playerGuid
        );

        if (!result)
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:此物品不需要鉴定或已鉴定");
            return;
        }

        Field* fields = result->Fetch();
        uint32 itemId = fields[0].Get<uint32>();
        uint256 huanJingMultiplier = fields[1].Get<uint256>();
        char huanJingMode = DbValueToHuanJingMode(fields[2].Get<int32>());
        uint32 identificationGroupId = fields[3].Get<uint32>();

        // 2. 在玩家背包中查找物品
        Item* item = FindItemByGuid(player, itemGuid);
        if (!item)
        {
            // 清理无效的标记
            CharacterDatabase.DirectExecute("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {}", itemGuid);
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:物品不存在或不在背包中");
            return;
        }

        // 验证物品ID是否匹配
        if (item->GetEntry() != itemId)
        {
            CharacterDatabase.DirectExecute("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {}", itemGuid);
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:物品数据不匹配");
            return;
        }

        // 3. 应用幻境倍率属性（如果有幻境系统）
#ifdef MODULE_HUANJING_SYSTEM
        if (HasHuanJingEffect(huanJingMultiplier, huanJingMode))
        {
            if (sHuanJingSystem)
            {
                sHuanJingSystem->ApplyItemAttributeMultiplier(item, huanJingMultiplier, huanJingMode);
            }
        }
#endif

        // 4. 执行鉴定（如果有鉴定组）
        // 【安全修复】改调 IdentifyItem：包含成功率掷骰与金币扣费。
        // 原先直调 ApplyIdentification 使 Addon 路径可免费且 100% 成功鉴定（绕过命令/批量路径的收费机制）
        bool identifySuccess = true;
        if (identificationGroupId > 0)
        {
            identifySuccess = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
        }

        // 5. 删除待鉴定标记
        // 【修复】仅在鉴定成功时删除标记；掷骰失败/金币不足等可恢复失败保留标记，允许玩家重试
        if (identifySuccess)
        {
            CharacterDatabase.DirectExecute("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {}", itemGuid);
        }

        // 【关键修复】鉴定成功后清除批量查询缓存，确保下次查询返回新数据
        {
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
            uint64 cacheKey = ((uint64)itemId << 32) | itemGuid;
            sItemIdentificationSystem->_batchQueryCache.erase(cacheKey);
        }

        // 【新增】确保鉴定缓存已更新（防止缓存不同步）
        if (identifySuccess)
        {
            sItemIdentificationSystem->AddToIdentifiedCache(itemGuid);
        }

        // 6. 发送结果
        if (identifySuccess)
        {
            // 发送鉴定成功消息，包含背包位置信息以便客户端正确清理缓存
            ClientItemLocation location = GetClientItemLocation(item);
            int clientBag = location.valid ? location.bag : 255;
            uint8 clientSlot = location.valid ? location.slot : static_cast<uint8>(item->GetSlot() + 1);

            std::ostringstream response;
            response << "IDENTIFY_RESULT:SUCCESS:" << clientBag << ":" << clientSlot << ":" << itemId << ":" << huanJingMultiplier.str();
            SendAddonResponse(player, response.str());
        }
        else
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:FAIL:鉴定失败");
        }
    }

    // 处理批量鉴定请求: IDENTIFY_BATCH
    void HandleBatchIdentifyRequest(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `玩家GUID` = {} ORDER BY `物品GUID` ASC",
            playerGuid
        );

        if (!result)
        {
            SendAddonResponse(player, "IDENTIFY_BATCH_RESULT:0:0:0");
            return;
        }

        uint32 successCount = 0;
        uint32 failCount = 0;
        uint32 skipCount = 0;
        std::vector<uint32> processedGuids;
        std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);
        // IdentifyItem 可能消耗背包材料或按配置销毁物品，销毁的物品可能仍在本批列表中，
        // 索引缓存的 Item* 会变成悬垂指针；发生过鉴定后必须重建索引再取指针
        bool itemIndexDirty = false;

        do
        {
            Field* fields = result->Fetch();
            uint32 itemGuid = fields[0].Get<uint32>();
            uint32 itemId = fields[1].Get<uint32>();
            uint256 huanJingMultiplier = fields[2].Get<uint256>();
            char huanJingMode = DbValueToHuanJingMode(fields[3].Get<int32>());
            uint32 identificationGroupId = fields[4].Get<uint32>();

            // 从本次请求的一次性背包索引中查找物品，避免每个GUID重复扫描背包
            if (itemIndexDirty)
            {
                itemsByGuid = BuildPlayerItemGuidIndex(player);
                itemIndexDirty = false;
            }
            auto itemItr = itemsByGuid.find(itemGuid);
            Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
            if (!item || item->GetEntry() != itemId)
            {
                // 物品不存在/不匹配：标记已失效，纳入批量清理
                processedGuids.push_back(itemGuid);
                skipCount++;
                continue;
            }

            // 应用幻境倍率
#ifdef MODULE_HUANJING_SYSTEM
            if (HasHuanJingEffect(huanJingMultiplier, huanJingMode))
            {
                if (sHuanJingSystem)
                {
                    sHuanJingSystem->ApplyItemAttributeMultiplier(item, huanJingMultiplier, huanJingMode);
                }
            }
#endif

            // 执行鉴定
            bool success = true;
            if (identificationGroupId > 0)
            {
                success = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
                itemIndexDirty = true;
            }

            if (success)
            {
                successCount++;
                // 【修复】仅成功时删除待鉴定标记；掷骰失败等可恢复失败保留标记允许重试
                processedGuids.push_back(itemGuid);

                // 【关键修复】鉴定成功后清除批量查询缓存
                {
                    std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
                    uint64 cacheKey = ((uint64)itemId << 32) | itemGuid;
                    sItemIdentificationSystem->_batchQueryCache.erase(cacheKey);
                }

                // 【新增】确保鉴定缓存已更新（防止缓存不同步）
                sItemIdentificationSystem->AddToIdentifiedCache(itemGuid);

                if (identificationGroupId == 0)
                    QueueAllModuleDataAddonRefresh(player, itemId, itemGuid, 2000ms);
            }
            else
            {
                failCount++;
            }

        } while (result->NextRow());

        // 批量删除已处理的标记
        if (!processedGuids.empty())
        {
            std::ostringstream ss;
            ss << "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` IN (";
            for (size_t i = 0; i < processedGuids.size(); ++i)
            {
                if (i > 0) ss << ",";
                ss << processedGuids[i];
            }
            ss << ")";
            CharacterDatabase.DirectExecute(ss.str().c_str());
        }

        // 发送批量鉴定结果
        std::ostringstream response;
        response << "IDENTIFY_BATCH_RESULT:" << successCount << ":" << failCount << ":" << skipCount;
        SendAddonResponse(player, response.str());
    }

    // 处理待鉴定列表查询请求: LIST_PENDING
    // 【重要】WoW 3.3.5 Addon消息限制255字节，需要分片发送
    void HandleListPendingRequest(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `玩家GUID` = {} ORDER BY `物品GUID` DESC LIMIT 100",
            playerGuid
        );

        if (!result)
        {
            SendAddonResponse(player, "PENDING_LIST:0:");
            return;
        }

        // 【修改】收集所有物品数据，然后分片发送
        // 格式：bag,slot,itemId,mode,multi,group
        std::vector<std::string> itemEntries;
        uint32 dbCount = 0;
        std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);

        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].Get<uint32>();
            uint32 itemId = fields[1].Get<uint32>();
            uint256 multiplier = fields[2].Get<uint256>();
            char multiplierMode = DbValueToHuanJingMode(fields[3].Get<int32>());
            uint32 groupId = fields[4].Get<uint32>();
            dbCount++;

            // 检查物品是否还在背包中，并获取位置信息
            auto itemItr = itemsByGuid.find(guid);
            Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
            if (item)
            {
                // 获取物品的背包位置
                uint8 serverBag = item->GetBagSlot();
                uint8 slot = item->GetSlot();

                // 【关键】服务器端背包编号转换为客户端编号
                int clientBag = 255;
                int clientSlot = 0;

                if (serverBag == INVENTORY_SLOT_BAG_0)
                {
                    if (slot >= EQUIPMENT_SLOT_START && slot < EQUIPMENT_SLOT_END)
                    {
                        clientBag = 255;
                        clientSlot = static_cast<int>(slot) + 1;
                    }
                    else if (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
                    {
                        clientBag = 0;
                        clientSlot = static_cast<int>(slot - INVENTORY_SLOT_ITEM_START) + 1;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
                {
                    clientBag = serverBag - INVENTORY_SLOT_BAG_START + 1;
                    clientSlot = static_cast<int>(slot) + 1;
                }
                else
                {
                    continue;
                }

                // 生成物品条目字符串
                std::ostringstream entryStream;
                entryStream << clientBag << "," << clientSlot << "," << itemId << "," << multiplierMode << "," << multiplier << "," << groupId;
                itemEntries.push_back(entryStream.str());
            }
        } while (result->NextRow());

        uint32 totalCount = itemEntries.size();

        // 如果没有有效物品，发送空列表
        if (totalCount == 0)
        {
            SendAddonResponse(player, "PENDING_LIST:0:");
            return;
        }

        // 【分片发送逻辑】
        // 格式变更：PENDING_LIST:总数:当前包序号:总包数:数据
        // 每个包预留约40字节给头部，数据部分约200字节
        const size_t MAX_DATA_SIZE = 180;  // 保守估计，给头部留空间

        std::vector<std::string> packets;
        std::ostringstream currentPacketData;
        bool firstInPacket = true;

        for (size_t i = 0; i < itemEntries.size(); ++i)
        {
            const std::string& entry = itemEntries[i];

            // 检查添加这个条目后是否超过限制
            size_t newSize = currentPacketData.str().length();
            if (!firstInPacket)
                newSize += 1;  // 分号
            newSize += entry.length();

            if (newSize > MAX_DATA_SIZE && !firstInPacket)
            {
                // 当前包已满，保存并开始新包
                packets.push_back(currentPacketData.str());
                currentPacketData.str("");
                currentPacketData.clear();
                firstInPacket = true;
            }

            if (!firstInPacket)
                currentPacketData << ";";
            currentPacketData << entry;
            firstInPacket = false;
        }

        // 保存最后一个包
        if (!currentPacketData.str().empty())
        {
            packets.push_back(currentPacketData.str());
        }

        uint32 totalPackets = packets.size();

        // 发送每个数据包
        for (size_t i = 0; i < packets.size(); ++i)
        {
            std::ostringstream response;
            // 格式: PENDING_LIST:总数:包序号:总包数:数据
            response << "PENDING_LIST:" << totalCount << ":" << (i + 1) << ":" << totalPackets << ":" << packets[i];

            std::string responseStr = response.str();
            SendAddonResponse(player, responseStr);
        }
    }
};

// ============================================================================
// 物品数据清理脚本 - 解决数据库无限膨胀问题
// ============================================================================

// 物品数据清理脚本 - 处理物品删除和角色删除时的数据清理
class ItemIdentificationCleanupScript : public PlayerScript, public AllItemScript
{
public:
    ItemIdentificationCleanupScript() : PlayerScript("ItemIdentificationCleanupScript"), AllItemScript("ItemIdentificationCleanupScript") {}

    // ========== 物品删除时清理数据 ==========
    // 当物品被销毁/删除时触发（出售商店、丢弃、销毁等）
    // 返回 true 表示允许删除，false 表示阻止删除
    bool CanItemRemove(Player* player, Item* item) override
    {
        (void)player;

        if (!item)
            return true;

        uint32 itemGuid = item->GetGUID().GetCounter();

        // 检查是否是已鉴定物品
        if (!sItemIdentificationSystem->IsItemIdentified(itemGuid))
            return true;

        // 飞升系统将装备暂存到 character_inventory.bag=200。
        // 这类物品可能被核心库存保存流程临时判定为不在官方槽位，但并未真正删除。
        if (IsAscensionVirtualBagItem(itemGuid))
            return true;

        // 清理该物品的所有相关数据
        CleanupItemData(itemGuid, item->GetEntry());

        return true;  // 允许删除物品
    }

    // ========== 角色删除时清理数据 ==========
    // 角色从数据库删除时触发（可以在事务中操作）
    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        if (!trans)
            return;

        // 先获取该玩家所有物品的GUID列表（用于清理内存缓存和关联表）
        QueryResult itemsResult = CharacterDatabase.Query(
            "SELECT `物品GUID` FROM `物品_鉴定记录` WHERE `玩家GUID` = {}", guid);

        std::vector<uint32> itemGuids;
        if (itemsResult)
        {
            do
            {
                itemGuids.push_back(itemsResult->Fetch()[0].Get<uint32>());
            } while (itemsResult->NextRow());
        }

        // 如果没有该玩家的鉴定记录，尝试从其他表获取物品GUID
        if (itemGuids.empty())
        {
            // 从成长记录获取
            QueryResult growthResult = CharacterDatabase.Query(
                "SELECT `物品GUID` FROM `物品成长_玩家记录` WHERE `玩家GUID` = {}", guid);
            if (growthResult)
            {
                do
                {
                    itemGuids.push_back(growthResult->Fetch()[0].Get<uint32>());
                } while (growthResult->NextRow());
            }
        }

        // ========== 在事务中删除所有相关表的数据 ==========

        // 0. 待鉴定物品标记表（有玩家GUID字段）
        trans->Append(Acore::StringFormat(
            "DELETE FROM `待鉴定物品标记` WHERE `玩家GUID` = {}", guid).c_str());

        // 1. 鉴定记录表（有玩家GUID字段）
        trans->Append(Acore::StringFormat(
            "DELETE FROM `物品_鉴定记录` WHERE `玩家GUID` = {}", guid).c_str());

        // 2. 物品成长记录表（有玩家GUID字段）
        trans->Append(Acore::StringFormat(
            "DELETE FROM `物品成长_玩家记录` WHERE `玩家GUID` = {}", guid).c_str());

        // 3. 物品强化记录表（字段名是owner_guid）
        trans->Append(Acore::StringFormat(
            "DELETE FROM `物品强化_记录` WHERE `owner_guid` = {}", guid).c_str());

        // 4. 符文系统数据表（字段名是角色GUID）
        trans->Append(Acore::StringFormat(
            "DELETE FROM `符文系统_数据` WHERE `角色GUID` = {}", guid).c_str());

        // 5. 玩家装备属性增强表（有玩家GUID字段）
        trans->Append(Acore::StringFormat(
            "DELETE FROM `玩家装备属性增强` WHERE `玩家GUID` = {}", guid).c_str());

        // ========== 以下表没有玩家GUID字段，需要通过物品GUID列表删除 ==========
        for (uint32 itemGuid : itemGuids)
        {
            // 6. 物品属性数据表（只有物品GUID）
            trans->Append(Acore::StringFormat(
                "DELETE FROM `物品属性_数据` WHERE `物品GUID` = {}", itemGuid).c_str());

            // 7. 物品技能数据表（只有物品GUID）
            trans->Append(Acore::StringFormat(
                "DELETE FROM `_物品技能_数据` WHERE `物品GUID` = {}", itemGuid).c_str());

            // 8. 魔次系统数据表（只有物品GUID）
            trans->Append(Acore::StringFormat(
                "DELETE FROM `魔次系统_数据` WHERE `物品GUID` = {}", itemGuid).c_str());

            // 9. 玩家套装状态表（只有物品GUID）
            trans->Append(Acore::StringFormat(
                "DELETE FROM `_物品套装_数据` WHERE `物品GUID` = {}", itemGuid).c_str());

            // 清理内存缓存
            sItemIdentificationSystem->ClearIdentifiedCache(itemGuid);
        }

    }

private:
    bool IsAscensionVirtualBagItem(uint32 itemGuid) const
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT 1 FROM `character_inventory` WHERE `item` = {} AND `bag` = 200 LIMIT 1",
            itemGuid);

        return static_cast<bool>(result);
    }

    // 清理单个物品的所有相关数据
    void CleanupItemData(uint32 itemGuid, uint32 itemEntry)
    {
        // 使用异步执行，避免阻塞主线程
        // 0. 待鉴定物品标记表（物品删除时同步清理）
        CharacterDatabase.DirectExecute(
            "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 1. 鉴定记录表
        CharacterDatabase.Execute(
            "DELETE FROM `物品_鉴定记录` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 2. 物品属性数据表
        CharacterDatabase.Execute(
            "DELETE FROM `物品属性_数据` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 3. 物品成长记录表
        CharacterDatabase.Execute(
            "DELETE FROM `物品成长_玩家记录` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 4. 物品强化记录表（字段名是guid）
        CharacterDatabase.Execute(
            "DELETE FROM `物品强化_记录` WHERE `guid` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 5. 物品技能数据表
        CharacterDatabase.Execute(
            "DELETE FROM `_物品技能_数据` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 6. 魔次系统数据表
        CharacterDatabase.Execute(
            "DELETE FROM `魔次系统_数据` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 7. 符文系统数据表
        CharacterDatabase.Execute(
            "DELETE FROM `符文系统_数据` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 8. 玩家套装状态表
        CharacterDatabase.Execute(
            "DELETE FROM `_物品套装_数据` WHERE `物品GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 9. 玩家装备属性增强表
        CharacterDatabase.Execute(
            "DELETE FROM `玩家装备属性增强` WHERE `装备GUID` = {} "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = {} AND ci.`bag` = 200)",
            itemGuid, itemGuid);

        // 清理内存缓存
        sItemIdentificationSystem->ClearIdentifiedCache(itemGuid);
        sItemIdentificationSystem->ClearItemCache(itemEntry, itemGuid);

        if (sItemIdentificationSystem->_debugMode)
        {
            LOG_DEBUG("module.itemidentification", "[数据清理] 已清理物品数据: GUID={}, Entry={}", itemGuid, itemEntry);
        }
    }
};

class IdentificationScrollItemScript : public AllItemScript
{
public:
    IdentificationScrollItemScript() : AllItemScript("IdentificationScrollItemScript") { }

    bool CanItemUse(Player* player, Item* scroll, SpellCastTargets const& targets) override
    {
        if (!player || !scroll)
            return false;

        if (!sItemIdentificationSystem->IsIdentificationScrollConfigured(scroll->GetEntry()))
            return false;

        Item* target = targets.GetItemTarget();
        if (!target)
        {
            ChatHandler(player->GetSession()).SendNotification("请将卷轴使用在一件武器或护甲上");
            player->SendEquipError(EQUIP_ERR_NONE, scroll, nullptr);
            return true;
        }

        player->SendEquipError(EQUIP_ERR_NONE, scroll, target);
        sItemIdentificationSystem->HandleIdentificationScrollUse(player, scroll, target);
        return true;
    }
};

// 添加脚本
void AddItemIdentificationSystemScripts()
{
    // 添加命令脚本
    new ItemIdentificationCommandScript();

    // 添加模块加载器
    new ItemIdentificationSystemModuleLoader();

    // 添加装备脚本
    new ItemIdentificationEquipScript();

    // 添加掉落监控脚本
    new ItemIdentificationLootScript();

    // 添加Addon消息处理脚本
    new ItemIdentificationAddonScript();

    // 添加物品数据清理脚本
    new ItemIdentificationCleanupScript();

    // 添加鉴定/清理卷轴脚本
    new IdentificationScrollItemScript();
}

// ============================================================================
// 批量查询优化实现 - 解决客户端查询延迟问题
// ============================================================================

// 批量查询所有模块数据（核心方法）- 【性能优化版】使用UNION合并查询
ItemIdentificationSystem::AllModuleData ItemIdentificationSystem::QueryAllModuleData(uint32 itemID, uint32 guid)
{
    AllModuleData result;
    result.identificationDisplayData = "IDDISP|||||";
    result.hasData = false;
    result.templateStatsData = BuildTemplateStatsData(itemID);
    if (!result.templateStatsData.empty())
        result.hasData = true;

    // 【修复】不再依赖内存缓存判断是否鉴定，直接查询数据库
    // 原因：内存缓存可能在鉴定后没有正确更新，导致已鉴定物品被判定为未鉴定
    bool isIdentified = IsItemIdentified(guid);

    if (!isIdentified)
    {
        // 即使缓存显示未鉴定，也尝试从数据库查询（修复缓存不同步问题）
        QueryResult checkResult = CharacterDatabase.Query(
            "SELECT 1 FROM `物品_鉴定记录` WHERE `物品GUID` = {}", guid);
        if (checkResult)
        {
            isIdentified = true;
            // 【线程安全修复】使用锁保护缓存写入
            {
                std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
                _identifiedItemsCache.insert(guid);
            }
        }
        else
        {
            DebugLog("[批量查询-优化] 物品未鉴定(仍会继续检查成长/强化/技能等模块): itemID={}, guid={}", itemID, guid);
        }
    }

    // ========== 表存在性缓存（静态变量，只初始化一次）==========
    // 【线程安全修复】添加静态锁保护表存在性缓存
    static std::mutex tableCacheMutex;
    static std::unordered_map<std::string, bool> tableCache;
    static bool cacheInitialized = false;

    // 【线程安全修复】使用锁保护表缓存的初始化和访问
    {
        std::lock_guard<std::mutex> lock(tableCacheMutex);
        if (!cacheInitialized)
        {
            // 一次性检查所有需要的表
            // 【表名修复】技能数据表真名为 `_物品技能_数据`（带前缀，见 mod-item-skills 的
            // SQL 与全部查询）——此前预检写成无前缀，存在性守卫永假，技能数据永不下发
            std::vector<std::string> tablesToCheck = {
                "物品属性_数据", "物品_鉴定记录", "物品成长_玩家记录",
                "物品强化_记录", "_物品技能_数据", "魔次系统_数据",
                "符文系统_数据", "_物品套装_数据", "待鉴定物品标记"
            };

            for (const auto& tableName : tablesToCheck)
            {
                QueryResult tableCheckResult = CharacterDatabase.Query(
                    "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '{}'",
                    tableName);

                bool exists = (tableCheckResult && tableCheckResult->Fetch()[0].Get<uint32>() > 0);
                tableCache[tableName] = exists;
            }

            cacheInitialized = true;
        }
    }

    // ========== 辅助函数：从缓存检查表是否存在 ==========
    // 注意：tableCache在初始化后只读，所以这里不需要加锁
    // 【修复】静态变量不能通过引用捕获，改为使用指针
    auto TableExists = [](const std::string& tableName) -> bool {
        static std::unordered_map<std::string, bool>* cache = &tableCache;
        auto it = cache->find(tableName);
        return (it != cache->end() && it->second);
    };

    if (!isIdentified && TableExists("待鉴定物品标记"))
    {
        QueryResult pendingIdentifyResult = CharacterDatabase.Query(
            "SELECT `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` WHERE `物品GUID` = {} AND `物品ID` = {} LIMIT 1",
            guid, itemID);

        if (pendingIdentifyResult)
        {
            Field* fields = pendingIdentifyResult->Fetch();
            uint256 multiplier = fields[0].Get<uint256>();
            char multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());
            uint32 groupId = fields[2].Get<uint32>();

            std::ostringstream pendingStream;
            pendingStream << "PENDID|" << multiplierMode << "|" << multiplier << "|" << groupId;
            result.pendingIdentifyData = pendingStream.str();
            result.hasData = true;
        }
    }

    // ========== 【性能优化】使用UNION合并多个查询为一个 ==========
    DebugLog("[批量查询-优化] 开始使用UNION查询: itemID={}, guid={}", itemID, guid);

    try
    {
        // 构建UNION查询（将7个独立查询合并为1个）
        std::ostringstream unionQuery;
        bool hasAnyQuery = false;

        // 1. 鉴定系统数据（基础属性+追加属性）
        // 【修复】直接从 物品属性_数据 表读取基础属性和追加属性
        // 原因：物品_鉴定记录.基础属性详情 在异步写入时可能为空，但实际属性存储在 物品属性_数据 表中
        if (isIdentified && TableExists("物品属性_数据"))
        {
            unionQuery << "SELECT 'identification' COLLATE utf8mb4_general_ci as source, "
                       << "CAST(IFNULL(a.`基础属性`, '') AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(IFNULL(a.`追加属性`, '') AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `物品属性_数据` a "
                       << "WHERE a.`物品GUID` = " << guid;
            hasAnyQuery = true;
        }
        // 【备选】如果物品属性_数据表不存在，尝试从鉴定记录表读取
        else if (isIdentified && TableExists("物品_鉴定记录"))
        {
            unionQuery << "SELECT 'identification' COLLATE utf8mb4_general_ci as source, "
                       << "CAST(r.`基础属性详情` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "'' COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `物品_鉴定记录` r "
                       << "WHERE r.`物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 2. 成长系统数据（包含百分比属性，避免额外查询）
        if (TableExists("物品成长_玩家记录"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'growth' COLLATE utf8mb4_general_ci, "
                       << "CAST(`当前等级` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`当前经验` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "CAST(`升级经验` AS CHAR) COLLATE utf8mb4_general_ci as field3, "
                       << "CAST(`成长属性` AS CHAR) COLLATE utf8mb4_general_ci as field4, "
                       << "CAST(IFNULL(`百分比属性`, '') AS CHAR) COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `物品成长_玩家记录` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 3. 强化系统数据
        if (TableExists("物品强化_记录"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'enhancement' COLLATE utf8mb4_general_ci, "
                       << "CAST(`强化等级` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`属性值` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `物品强化_记录` "
                       << "WHERE `guid` = " << guid;
            hasAnyQuery = true;
        }

        // 4. 技能系统数据
        if (TableExists("_物品技能_数据"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'skills' COLLATE utf8mb4_general_ci, "
                       << "CAST(`技能ID` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "'' COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `_物品技能_数据` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 5. 魔次系统数据
        if (TableExists("魔次系统_数据"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'magic' COLLATE utf8mb4_general_ci, "
                       << "CAST(`魔次系统ID` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`魔次值` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `魔次系统_数据` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 6. 符文系统数据
        if (TableExists("符文系统_数据"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'rune' COLLATE utf8mb4_general_ci, "
                       << "CAST(`插槽数量` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`插槽ID` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "CAST(`符文物品ID` AS CHAR) COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `符文系统_数据` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 7. 套装系统数据
        if (TableExists("_物品套装_数据"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'set' COLLATE utf8mb4_general_ci, "
                       << "CAST(`套装ID` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "'' COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `_物品套装_数据` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 执行合并查询
        if (hasAnyQuery)
        {
            std::string queryStr = unionQuery.str();

            QueryResult combinedResult = CharacterDatabase.Query(queryStr.c_str());

            if (combinedResult)
            {
                DebugLog("[批量查询-优化] UNION查询成功，开始解析结果");

                do
                {
                    Field* fields = combinedResult->Fetch();
                    std::string source = fields[0].Get<std::string>();

                    if (source == "identification")
                    {
                        std::string baseAttr = fields[1].Get<std::string>();
                        std::string additionalAttr = fields[2].Get<std::string>();

                        if (!baseAttr.empty())
                        {
                            result.baseAttributes = baseAttr;
                            result.hasData = true;
                        }

                        if (!additionalAttr.empty())
                        {
                            result.additionalAttributes = additionalAttr;
                            result.hasData = true;
                        }

                        DebugLog("[批量查询-优化-鉴定] 基础=[{}], 追加=[{}]", baseAttr, additionalAttr);
                    }
                    else if (source == "growth")
                    {
                        uint32 level = std::stoul(fields[1].Get<std::string>());
                        uint32 currentExp = std::stoul(fields[2].Get<std::string>());
                        uint32 requiredExp = std::stoul(fields[3].Get<std::string>());
                        std::string attrs = fields[4].Get<std::string>();
                        std::string percentAttrs = fields[5].Get<std::string>();  // 【性能优化】新增百分比属性

                        std::ostringstream growthStream;
                        // 格式：level|curExp|requiredExp|attrs|percentAttrs
                        growthStream << level << "|" << currentExp << "|" << requiredExp << "|" << attrs << "|" << percentAttrs;
                        result.growthData = growthStream.str();
                        result.hasData = true;

                        DebugLog("[批量查询-优化-成长] level={}, exp={}/{}, attrs=[{}], percentAttrs=[{}]",
                                 level, currentExp, requiredExp, attrs, percentAttrs);
                    }
                    else if (source == "enhancement")
                    {
                        std::string level = fields[1].Get<std::string>();
                        std::string attrs = fields[2].Get<std::string>();

                        std::ostringstream enhanceStream;
                        enhanceStream << level << "|" << attrs;
                        result.enhancementData = enhanceStream.str();
                        result.hasData = true;

                        DebugLog("[批量查询-优化-强化] level={}, attrs=[{}]", level, attrs);
                    }
                    else if (source == "skills")
                    {
                        // 继续使用现有逻辑处理技能（需要模板数据）
                        result.hasData = true;
                    }
                    else if (source == "magic")
                    {
                        std::string magicIdsStr = fields[1].Get<std::string>();
                        std::string magicValuesStr = fields[2].Get<std::string>();

#if defined(MODULE_MAGIC_HIT_SYSTEM)
                        // 使用魔次系统配置，构建带描述的完整数据
                        std::vector<uint32> magicIds = ParseCommaSeparatedNumbers(magicIdsStr);
                        std::vector<uint32> magicValues = ParseCommaSeparatedNumbers(magicValuesStr);

                        size_t count = std::min(magicIds.size(), magicValues.size());
                        std::ostringstream magicStream;
                        bool first = true;

                        for (size_t i = 0; i < count; ++i)
                        {
                            uint32 configId = magicIds[i];
                            uint32 hitCount = magicValues[i];

                            if (configId == 0 || hitCount == 0)
                                continue;

                            SpellMagicHitConfig const* cfg = sMagicHitSystem->GetConfigById(configId);
                            if (!cfg)
                                continue;

                            if (!first)
                                magicStream << ",";

                            first = false;
                            magicStream << configId << "|" << hitCount << "|" << cfg->description;
                        }

                        std::string magicStr = magicStream.str();
                        if (!magicStr.empty())
                        {
                            result.magicHitData = magicStr;
                            result.hasData = true;
                        }
#else
                        result.magicHitData = magicIdsStr + "|" + magicValuesStr;
                        result.hasData = true;
#endif

                        DebugLog("[批量查询-优化-魔次] ids=[{}], values=[{}]", magicIdsStr, magicValuesStr);
                    }
                    else if (source == "rune")
                    {
                        uint32 slotCount = std::stoul(fields[1].Get<std::string>());
                        std::string slotIds = fields[2].Get<std::string>();
                        std::string runeItemIds = fields[3].Get<std::string>();

                        std::ostringstream runeStream;
                        runeStream << slotCount << "|" << slotIds << "|" << runeItemIds;
                        result.runeData = runeStream.str();
                        result.hasData = true;

                        DebugLog("[批量查询-优化-符文] slotCount={}, slotIds=[{}], runeItemIds=[{}]",
                                 slotCount, slotIds, runeItemIds);
                    }
                    else if (source == "set")
                    {
                        uint32 setId = std::stoul(fields[1].Get<std::string>());

                        if (setId > 0)
                        {
                            // 套装数据需要额外查询（因为涉及WorldDatabase）
                            // 保持原有逻辑
                            result.hasData = true;
                            DebugLog("[批量查询-优化-套装] setId={}", setId);

                            // 后续处理套装详情
                            std::string setName;
                            std::string attrsStr;
                            std::string effectsStr;

#if defined(MODULE_ITEM_SETS)
                            if (sItemSetsManager)
                            {
                                const std::vector<ItemSetData>& setDataList = sItemSetsManager->GetItemSetData(setId);

                                if (!setDataList.empty())
                                {
                                    setName = setDataList[0].SetName;

                                    bool firstAttr = true;
                                    bool firstEffect = true;

                                    for (const auto& setData : setDataList)
                                    {
                                        if (!setData.SetAttributes.empty())
                                        {
                                            if (!firstAttr)
                                                attrsStr += ",";
                                            attrsStr += setData.SetAttributes;
                                            firstAttr = false;
                                        }

                                        if (!setData.EffectDesc.empty())
                                        {
                                            if (!firstEffect)
                                                effectsStr += ",";

                                            firstEffect = false;
                                            effectsStr += std::to_string(setData.ItemsCount);
                                            effectsStr += "|";
                                            effectsStr += setData.EffectDesc;
                                        }
                                    }
                                }
                            }
#endif

                            std::ostringstream setStream;
                            setStream << setId;

                            if (!setName.empty() || !attrsStr.empty() || !effectsStr.empty())
                            {
                                setStream << ":" << setName << ":" << attrsStr << ":" << effectsStr;
                            }

                            result.setData = setStream.str();
                        }
                    }

                } while (combinedResult->NextRow());

                DebugLog("[批量查询-优化] UNION查询解析完成");
            }
            else
            {
                DebugLog("[批量查询-优化] UNION查询未返回数据");
            }
        }
        else
        {
        }

        // 【性能优化-套装回退】如果UNION查询中没有找到套装数据，回退到鉴定记录表
        // 原因：套装系统异步写入玩家套装状态表，刚鉴定的物品可能还未写入
        if (result.setData.empty() && TableExists("物品_鉴定记录"))
        {
            QueryResult setFallbackResult = CharacterDatabase.Query(
                "SELECT `是否获得套装`, `套装ID` FROM `物品_鉴定记录` WHERE `物品GUID` = {}",
                guid);

            if (setFallbackResult)
            {
                Field* setFields = setFallbackResult->Fetch();
                bool hasSet = setFields[0].Get<uint32>() != 0;
                uint32 setId = setFields[1].Get<uint32>();

                if (hasSet && setId > 0)
                {
                    DebugLog("[批量查询-优化-套装回退] 从鉴定记录获取套装ID: setId={}", setId);

                    std::string setName;
                    std::string attrsStr;
                    std::string effectsStr;

#if defined(MODULE_ITEM_SETS)
                    if (sItemSetsManager)
                    {
                        const std::vector<ItemSetData>& setDataList = sItemSetsManager->GetItemSetData(setId);

                        if (!setDataList.empty())
                        {
                            setName = setDataList[0].SetName;

                            bool firstAttr = true;
                            bool firstEffect = true;

                            for (const auto& setData : setDataList)
                            {
                                if (!setData.SetAttributes.empty())
                                {
                                    if (!firstAttr)
                                        attrsStr += ",";
                                    attrsStr += setData.SetAttributes;
                                    firstAttr = false;
                                }

                                if (!setData.EffectDesc.empty())
                                {
                                    if (!firstEffect)
                                        effectsStr += ",";

                                    firstEffect = false;
                                    effectsStr += std::to_string(setData.ItemsCount);
                                    effectsStr += "|";
                                    effectsStr += setData.EffectDesc;
                                }
                            }
                        }
                    }
#endif

                    std::ostringstream setStream;
                    setStream << setId;

                    if (!setName.empty() || !attrsStr.empty() || !effectsStr.empty())
                    {
                        setStream << ":" << setName << ":" << attrsStr << ":" << effectsStr;
                    }

                    result.setData = setStream.str();
                    result.hasData = true;

                    DebugLog("[批量查询-优化-套装回退] 套装数据=[{}]", result.setData);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module.itemidentification", "[批量查询-优化] 发生异常: {}", e.what());
    }

    // 特殊处理：技能需要额外查询模板数据
    if (TableExists("_物品技能_数据"))
    {
#if defined(MODULE_ITEM_SKILLS)
        std::vector<ItemSkillData> skills = sItemSkillsDBHelper->GetItemSkillsData(static_cast<uint64>(guid));

        if (!skills.empty())
        {
            std::ostringstream skillStream;
            bool first = true;

            for (const auto& skillData : skills)
            {
                ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
                if (!skillTemplate)
                    continue;

                if (!first)
                    skillStream << ",";

                first = false;
                skillStream << skillData.skillId << "|" << skillTemplate->clientDisplay << "|" << skillTemplate->level;
            }

            std::string skillsStr = skillStream.str();
            if (!skillsStr.empty())
            {
                result.skillsData = skillsStr;
                result.hasData = true;
                DebugLog("[批量查询-优化-技能] skills=[{}]", result.skillsData);
            }
        }
#endif
    }

    if (isIdentified && TableExists("物品_鉴定记录"))
    {
        QueryResult templateResult = CharacterDatabase.Query(
            "SELECT `鉴定模板ID` FROM `物品_鉴定记录` WHERE `物品GUID` = {} LIMIT 1",
            guid);

        if (templateResult)
        {
            uint32 templateId = templateResult->Fetch()[0].Get<uint32>();
            auto tmplItr = _identificationTemplates.find(templateId);
            if (tmplItr != _identificationTemplates.end())
            {
                result.identificationDisplayData = BuildIdentificationDisplayData(tmplItr->second);
                if (!result.identificationDisplayData.empty())
                {
                    result.hasData = true;
                    DebugLog("[批量查询-优化-鉴定显示] templateId={}, data=[{}]",
                             templateId, result.identificationDisplayData);
                }
            }
        }
    }

    // 【新增】查询幻境系统倍率数据
    // 优先查询 玩家装备属性增强 表（已鉴定物品）
    // 如果没有找到，回退查询 待鉴定物品标记 表（未鉴定物品）
    {
        bool foundHuanjingData = false;
        uint256 multiplier = 0;
        char multiplierMode = 'x';
        std::string enhancedAttrs;

        // 第一步：查询已鉴定物品的增强数据
        QueryResult huanjingResult = CharacterDatabase.Query(
            "SELECT `属性倍率`, `属性倍率模式`, `增强属性数据` FROM `玩家装备属性增强` WHERE `装备GUID` = {} AND `装备ID` = {}",
            guid, itemID);

        if (huanjingResult)
        {
            Field* fields = huanjingResult->Fetch();
            multiplier = fields[0].Get<uint256>();
            multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());
            enhancedAttrs = fields[2].Get<std::string>();

            if (HasHuanJingEffect(multiplier, multiplierMode))
            {
                foundHuanjingData = true;
                DebugLog("[批量查询-优化-幻境] 从玩家装备属性增强表找到: multiplier={}, enhancedAttrs=[{}]", multiplier.str(), enhancedAttrs);
            }
        }

        // 第二步：如果没有找到已鉴定数据，查询待鉴定物品标记表
        if (!foundHuanjingData)
        {
            QueryResult pendingResult = CharacterDatabase.Query(
                "SELECT `幻境倍率`, `幻境倍率模式` FROM `待鉴定物品标记` WHERE `物品GUID` = {} AND `物品ID` = {}",
                guid, itemID);

            if (pendingResult)
            {
                Field* fields = pendingResult->Fetch();
                multiplier = fields[0].Get<uint256>();
                multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());

                if (HasHuanJingEffect(multiplier, multiplierMode))
                {
                    foundHuanjingData = true;
                    // 待鉴定物品没有增强属性数据，只有倍率
                    enhancedAttrs = "";
                    DebugLog("[批量查询-优化-幻境] 从待鉴定物品标记表找到: multiplier={} (待鉴定)", multiplier.str());
                }
            }
        }

        // 构建幻境数据字符串
        if (foundHuanjingData && HasHuanJingEffect(multiplier, multiplierMode))
        {
            std::ostringstream huanjingStream;
            huanjingStream << multiplierMode << "," << multiplier.str();

            // 如果有增强属性数据，添加到结果中
            if (!enhancedAttrs.empty())
            {
                huanjingStream << "|" << enhancedAttrs;
            }

            result.huanjingData = huanjingStream.str();
            result.hasData = true;

            DebugLog("[批量查询-优化-幻境] 最终结果: huanjingData=[{}]", result.huanjingData);
        }
    }

    DebugLog("[批量查询-优化] 完成查询: itemID={}, guid={}, 有数据={}", itemID, guid, result.hasData);

    return result;
}


// 通过 Addon 消息发送批量数据（ALL_MODULE_DATA:itemID:guid:...）
void ItemIdentificationSystem::SendAllModuleDataAddon(Player* player, uint32 itemID, uint32 guid)
{
    if (!player)
        return;

    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);
        uint64 cacheKey = (static_cast<uint64>(itemID) << 32) | guid;
        _batchQueryCache.erase(cacheKey);
    }

    AllModuleData data = QueryAllModuleData(itemID, guid);
    Item* item = FindPlayerItemByGuidForAddon(player, guid);
    ClientItemLocation location = GetClientItemLocation(item);

    std::ostringstream response;
    if (location.valid)
        response << "ALL_MODULE_DATA:" << location.bag << ":" << static_cast<uint32>(location.slot) << ":" << itemID << ":" << guid << ":";
    else
        response << "ALL_MODULE_DATA:" << itemID << ":" << guid << ":";

    response
             << data.baseAttributes << ":"
             << data.additionalAttributes << ":"
             << data.identificationDisplayData << ":"
             << data.growthData << ":"
             << data.enhancementData << ":"
             << data.skillsData << ":"
             << data.magicHitData << ":"
             << data.runeData << ":"
             << data.setData << ":"
             << data.huanjingData;
    if (!data.pendingIdentifyData.empty())
        response << ":" << data.pendingIdentifyData;
    response << ":" << data.templateStatsData;

    std::string responseStr = response.str();

    SendItemIdentificationAddonMessage(player, responseStr);
}

// 批量查询命令处理器（一次性返回所有数据）
void ItemIdentificationSystem::HandleBatchQueryCommand(Player* player, uint32 itemID, uint32 guid)
{
    if (!player)
        return;

    auto queryStart = std::chrono::high_resolution_clock::now();

    // 优先从缓存读取
    uint64 cacheKey = ((uint64)itemID << 32) | guid;

    AllModuleData data;
    bool cacheHit = false;

    // 【线程安全】加锁读取缓存
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);

        auto it = _batchQueryCache.find(cacheKey);

        // 更新统计：总查询次数
        _perfStats.totalQueries++;

        if (it != _batchQueryCache.end())
        {
            // 检查缓存是否过期
            time_t now = time(nullptr);
            if ((now - it->second.cacheTime) < BATCH_CACHE_EXPIRE_TIME)
            {
                // 缓存有效，直接使用
                data = it->second.data;
                cacheHit = true;
                _perfStats.cacheHits++;  // 统计：缓存命中
                DebugLog("[批量查询缓存] 缓存命中: itemID={}, guid={}", itemID, guid);
            }
            else
            {
                // 缓存过期，删除
                _batchQueryCache.erase(it);
                _perfStats.cacheMisses++;  // 统计：缓存未命中
                DebugLog("[批量查询缓存] 缓存过期: itemID={}, guid={}", itemID, guid);
            }
        }
        else
        {
            _perfStats.cacheMisses++;  // 统计：缓存未命中
        }
    }

    if (!cacheHit)
    {
        // 缓存未命中，在锁外查询数据库（避免长时间持有锁）
        _perfStats.dbQueries++;  // 统计：数据库查询
        data = QueryAllModuleData(itemID, guid);

        // 【线程安全】加锁写入缓存
        std::lock_guard<std::mutex> lock(_batchCacheMutex);
        BatchQueryCache cache;
        cache.data = data;
        cache.cacheTime = time(nullptr);
        _batchQueryCache[cacheKey] = cache;

        DebugLog("[批量查询缓存] 新数据已缓存: itemID={}, guid={}", itemID, guid);
    }

    // 新格式消息：ALL_MODULE_DATA:itemID:guid:base:additional:identDisplay:growth:enhancement:skills:magic:rune:set:huanjing:templateStats
    std::ostringstream response;
    response << "ALL_MODULE_DATA:" << itemID << ":" << guid << ":"
             << data.baseAttributes << ":"
             << data.additionalAttributes << ":"
             << data.identificationDisplayData << ":"
             << data.growthData << ":"
             << data.enhancementData << ":"
             << data.skillsData << ":"
             << data.magicHitData << ":"
             << data.runeData << ":"
             << data.setData << ":"
             << data.huanjingData;
    if (!data.pendingIdentifyData.empty())
        response << ":" << data.pendingIdentifyData;
    response << ":" << data.templateStatsData;

    // 发送前记录完整消息内容
    DebugLog("[批量查询命令] 准备发送完整消息: [{}]", response.str());
    DebugLog("[批量查询命令] 各字段详情:");
    DebugLog("  - 基础属性: [{}]", data.baseAttributes);
    DebugLog("  - 追加属性: [{}]", data.additionalAttributes);
    DebugLog("  - 鉴定显示: [{}]", data.identificationDisplayData);
    DebugLog("  - 成长数据: [{}]", data.growthData);
    DebugLog("  - 强化数据: [{}]", data.enhancementData);
    DebugLog("  - 技能数据: [{}]", data.skillsData);
    DebugLog("  - 魔次数据: [{}]", data.magicHitData);
    DebugLog("  - 符文数据: [{}]", data.runeData);
    DebugLog("  - 套装数据: [{}]", data.setData);
    DebugLog("  - 模板属性: [{}]", data.templateStatsData);

    // 发送到客户端
    ChatHandler(player->GetSession()).PSendSysMessage(response.str().c_str());

    // 计算查询耗时
    auto queryEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart).count();
    _perfStats.totalQueryTime += duration;
    _perfStats.avgQueryTime = _perfStats.totalQueryTime / _perfStats.totalQueries;

    DebugLog("[批量查询命令] 已发送数据: itemID={}, guid={}, 消息长度={}, 缓存命中={}, 耗时={}μs",
             itemID, guid, response.str().length(), cacheHit, duration);
}

// 清理过期缓存
void ItemIdentificationSystem::CleanExpiredCache()
{
    time_t now = time(nullptr);
    uint32 cleanedAttr = 0;
    uint32 cleanedBatch = 0;

    // 清理属性缓存（不需要互斥锁，只在主线程访问）
    for (auto it = _attrCache.begin(); it != _attrCache.end(); )
    {
        if ((now - it->second.cacheTime) >= ATTR_CACHE_EXPIRE_TIME)
        {
            it = _attrCache.erase(it);
            cleanedAttr++;
        }
        else
        {
            ++it;
        }
    }

    // 【线程安全】清理批量查询缓存（可能被异步线程访问）
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);

        for (auto it = _batchQueryCache.begin(); it != _batchQueryCache.end(); )
        {
            if ((now - it->second.cacheTime) >= BATCH_CACHE_EXPIRE_TIME)
            {
                it = _batchQueryCache.erase(it);
                cleanedBatch++;
            }
            else
            {
                ++it;
            }
        }
    }

    if (cleanedAttr > 0 || cleanedBatch > 0)
    {
        LOG_DEBUG("module.itemidentification", "[缓存清理] 属性缓存: {}, 批量缓存: {}",
                  cleanedAttr, cleanedBatch);
    }
}

// 批量查询命令处理函数
bool ItemIdentificationCommandScript::HandleBatchQueryCommand(ChatHandler* handler, const char* args)
{
    if (!sItemIdentificationSystem->_enabled)
        return true;

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 解析参数：.鉴定 批量查询 <物品ID> <GUID>
    if (!args || !*args)
    {
        handler->SendSysMessage("用法: .鉴定 批量查询 <物品ID> <GUID>");
        return true;
    }

    std::istringstream iss(args);
    uint32 itemID = 0;
    uint32 guid = 0;

    iss >> itemID >> guid;

    if (itemID == 0 || guid == 0)
    {
        handler->SendSysMessage("无效的物品ID或GUID");
        return true;
    }

    // 执行批量查询
    sItemIdentificationSystem->HandleBatchQueryCommand(player, itemID, guid);

    return true;
}

// ============================================================================
// 高级优化功能实现
// ============================================================================

// 预加载玩家装备数据（登录时调用）- 异步优化版本
void ItemIdentificationSystem::PreloadPlayerEquipment(Player* player)
{
    if (!player || !_enabled)
        return;

    // 保存玩家名称（避免在异步线程中访问Player对象）
    std::string playerName = player->GetName();

    std::vector<std::pair<uint32, uint32>> itemsToPreload;  // <itemID, guid>

    // 【主线程】快速收集所有需要预加载的装备GUID（不涉及数据库查询）
    // 收集所有已鉴定的装备
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            if (IsItemIdentified(itemGuid))
            {
                itemsToPreload.push_back(std::make_pair(item->GetEntry(), itemGuid));
            }
        }
    }

    // 收集背包中的已鉴定装备
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            if (IsItemIdentified(itemGuid))
            {
                ItemTemplate const* proto = item->GetTemplate();
                if (proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR))
                {
                    itemsToPreload.push_back(std::make_pair(item->GetEntry(), itemGuid));
                }
            }
        }
    }

    // 收集背包袋中的已鉴定装备
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = player->GetBagByPos(i))
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item)
                {
                    uint32 itemGuid = item->GetGUID().GetCounter();
                    if (IsItemIdentified(itemGuid))
                    {
                        ItemTemplate const* proto = item->GetTemplate();
                        if (proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR))
                        {
                            itemsToPreload.push_back(std::make_pair(item->GetEntry(), itemGuid));
                        }
                    }
                }
            }
        }
    }

    // 如果没有需要预加载的装备，直接返回
    if (itemsToPreload.empty())
    {
        return;
    }

    // 【异步线程】在后台线程中执行数据库查询，不阻塞主线程
    std::thread([this, itemsToPreload, playerName]() {
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32 preloadCount = 0;
        uint32 cacheHitCount = 0;

        // 异步执行数据库查询和缓存
        for (const auto& itemPair : itemsToPreload)
        {
            uint32 itemID = itemPair.first;
            uint32 guid = itemPair.second;
            uint64 cacheKey = ((uint64)itemID << 32) | guid;

            // 使用互斥锁保护缓存访问
            {
                std::lock_guard<std::mutex> lock(_batchCacheMutex);

                // 检查是否已在缓存中
                if (_batchQueryCache.find(cacheKey) != _batchQueryCache.end())
                {
                    cacheHitCount++;
                    continue;
                }
            }

            // 在锁外执行数据库查询（避免长时间持有锁）
            AllModuleData data = QueryAllModuleData(itemID, guid);

            // 写入缓存时加锁
            {
                std::lock_guard<std::mutex> lock(_batchCacheMutex);

                BatchQueryCache cache;
                cache.data = data;
                cache.cacheTime = time(nullptr);
                _batchQueryCache[cacheKey] = cache;
            }

            preloadCount++;
        }

        // 【线程安全修复】使用专用方法更新性能统计
        IncrementPreloadCount(preloadCount);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();



        DebugLog("[预加载-异步] 玩家={}, 装备总数={}, 缓存命中={}, 查询数={}, 耗时={}ms",
                 playerName, itemsToPreload.size(), cacheHitCount, preloadCount, duration);

    }).detach();  // 分离线程，让其在后台执行
}

// 重置性能统计
void ItemIdentificationSystem::ResetPerformanceStats()
{
    _perfStats = PerformanceStats();
    LOG_INFO("module.itemidentification", "[性能统计] 已重置统计数据");
}

// 性能统计命令处理函数
bool ItemIdentificationCommandScript::HandlePerformanceStatsCommand(ChatHandler* handler, const char* args)
{
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("物品鉴定系统当前已禁用");
        return true;
    }

    const auto& stats = sItemIdentificationSystem->GetPerformanceStats();

    handler->PSendSysMessage("|cff00ff00========== 鉴定系统性能统计 ==========|r");
    handler->PSendSysMessage("总查询次数: |cffff8000{}|r", stats.totalQueries);
    handler->PSendSysMessage("缓存命中: |cff00ff00{}|r 次 ({:.2f}%%)",
                            stats.cacheHits, stats.GetCacheHitRate());
    handler->PSendSysMessage("缓存未命中: |cffff0000{}|r 次 ({:.2f}%%)",
                            stats.cacheMisses,
                            stats.totalQueries > 0 ? (float)stats.cacheMisses / stats.totalQueries * 100.0f : 0.0f);
    handler->PSendSysMessage("数据库查询: |cffffa500{}|r 次", stats.dbQueries);
    handler->PSendSysMessage("平均查询时间: |cffadd8e6{:.2f}|r 毫秒",
                            stats.avgQueryTime / 1000.0);
    handler->PSendSysMessage("总查询时间: |cffadd8e6{:.2f}|r 秒",
                            stats.totalQueryTime / 1000000.0);
    handler->PSendSysMessage("预加载次数: |cff00ff00{}|r", stats.preloadCount);

    // 运行时间
    time_t now = time(nullptr);
    uint32 runningTime = now - stats.startTime;
    uint32 hours = runningTime / 3600;
    uint32 minutes = (runningTime % 3600) / 60;
    uint32 seconds = runningTime % 60;
    handler->PSendSysMessage("运行时间: |cffffffff{}时{}分{}秒|r", hours, minutes, seconds);

    // 缓存状态
    handler->PSendSysMessage("|cff00ff00========== 缓存状态 ==========|r");
    handler->PSendSysMessage("属性缓存大小: |cffadd8e6{}|r", sItemIdentificationSystem->GetAttrCacheSize());
    handler->PSendSysMessage("批量查询缓存大小: |cffadd8e6{}|r", sItemIdentificationSystem->GetBatchQueryCacheSize());
    handler->PSendSysMessage("已鉴定物品缓存: |cffadd8e6{}|r", sItemIdentificationSystem->GetIdentifiedCacheSize());

    // 性能评估
    handler->PSendSysMessage("|cff00ff00========== 性能评估 ==========|r");
    if (stats.GetCacheHitRate() >= 80.0f)
    {
        handler->SendSysMessage("|cff00ff00性能状态: 优秀|r");
    }
    else if (stats.GetCacheHitRate() >= 60.0f)
    {
        handler->SendSysMessage("|cffffa500性能状态: 良好|r");
    }
    else if (stats.GetCacheHitRate() >= 40.0f)
    {
        handler->SendSysMessage("|cffff8000性能状态: 一般，考虑调整缓存策略|r");
    }
    else
    {
        handler->SendSysMessage("|cffff0000性能状态: 较差，需要优化|r");
    }

    if (stats.avgQueryTime < 5000)  // <5ms
    {
        handler->SendSysMessage("|cff00ff00查询速度: 极快|r");
    }
    else if (stats.avgQueryTime < 20000)  // <20ms
    {
        handler->SendSysMessage("|cff00ff00查询速度: 快速|r");
    }
    else if (stats.avgQueryTime < 50000)  // <50ms
    {
        handler->SendSysMessage("|cffffa500查询速度: 正常|r");
    }
    else if (stats.avgQueryTime < 100000)  // <100ms
    {
        handler->SendSysMessage("|cffff8000查询速度: 较慢，考虑优化数据库|r");
    }
    else
    {
        handler->SendSysMessage("|cffff0000查询速度: 很慢，需要立即优化！|r");
    }

    handler->PSendSysMessage("|cff888888提示: 使用 |cffffffff.鉴定 性能统计 reset|r |cff888888重置统计|r");

    // 检查是否需要重置
    if (args && *args)
    {
        std::string arg = args;
        if (arg == "reset")
        {
            sItemIdentificationSystem->ResetPerformanceStats();
            handler->SendSysMessage("|cff00ff00性能统计已重置|r");
        }
    }

    return true;
}

// ========== 【新增】手动鉴定相关命令实现 ==========

// 辅助函数：在玩家背包中通过GUID查找物品
static Item* FindItemByGuidInBags(Player* player, uint32 itemGuid)
{
    if (!player)
        return nullptr;

    // 1. 搜索装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
            return item;
    }

    // 2. 搜索主背包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
            return item;
    }

    // 3. 搜索背包袋
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = player->GetBagByPos(i))
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetGUID().GetCounter() == itemGuid)
                    return item;
            }
        }
    }

    return nullptr;
}

// 辅助函数：通过背包位置获取物品
// bagSlot: 0=主背包, 1-4=额外背包
// slot: 槽位索引
static Item* GetItemByBagSlot(Player* player, uint8 bagSlot, uint8 slot)
{
    if (!player)
        return nullptr;

    if (bagSlot == 0)
    {
        // 主背包
        return player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + slot);
    }
    else if (bagSlot >= 1 && bagSlot <= 4)
    {
        // 额外背包 (1-4 对应 INVENTORY_SLOT_BAG_START 到 INVENTORY_SLOT_BAG_END-1)
        Bag* bag = player->GetBagByPos(INVENTORY_SLOT_BAG_START + bagSlot - 1);
        if (bag && slot < bag->GetBagSize())
        {
            return bag->GetItemByPos(slot);
        }
    }

    return nullptr;
}

// 辅助函数：执行单个物品的鉴定逻辑
static bool DoIdentifyItem(Player* player, Item* item, ChatHandler* handler = nullptr)
{
    if (!player || !item)
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 从数据库查询待鉴定标记
    QueryResult result = CharacterDatabase.Query(
        "SELECT `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
        "WHERE `物品GUID` = {} AND `玩家GUID` = {}",
        itemGuid, playerGuid
    );

    if (!result)
    {
        if (handler)
            handler->SendSysMessage("|cffff0000此物品不需要鉴定或已鉴定|r");
        return false;
    }

    Field* fields = result->Fetch();
    uint32 itemId = fields[0].Get<uint32>();
    uint256 huanJingMultiplier = fields[1].Get<uint256>();
    char huanJingMode = DbValueToHuanJingMode(fields[2].Get<int32>());
    uint32 identificationGroupId = fields[3].Get<uint32>();

    // 验证物品ID是否匹配
    if (item->GetEntry() != itemId)
    {
        CharacterDatabase.DirectExecute("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {}", itemGuid);
        if (handler)
            handler->SendSysMessage("|cffff0000物品数据不匹配|r");
        return false;
    }

    // 获取物品名称
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    std::string itemName = itemTemplate ? itemTemplate->Name1 : "未知物品";

    if (handler)
    {
        handler->PSendSysMessage("|cff00ff00开始鉴定物品: |r|cffffffff{}|r", itemName);
        handler->PSendSysMessage("|cff00ff00幻境模式: |r|cffff8000{}{}|r",
            huanJingMode == '+' ? "+" : (huanJingMode == '-' ? "关闭" : "x"), huanJingMultiplier.str());
    }

    // 应用幻境倍率属性（如果有幻境系统）
#ifdef MODULE_HUANJING_SYSTEM
    if (HasHuanJingEffect(huanJingMultiplier, huanJingMode))
    {
        if (sHuanJingSystem)
        {
            sHuanJingSystem->ApplyItemAttributeMultiplier(item, huanJingMultiplier, huanJingMode);
            if (handler)
                handler->PSendSysMessage("|cff00ff00已应用幻境倍率属性|r");
        }
    }
#endif

    // 执行鉴定（如果有鉴定组）
    // 【审计修复】使用 IdentifyItem 进行鉴定，确保成功率和费用扣除逻辑生效
    bool identifySuccess = true;
    if (identificationGroupId > 0)
    {
        identifySuccess = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
        // IdentifyItem 内部已经发送了成功/失败消息，这里不需要重复发送
    }
    else
    {
        if (handler)
            handler->SendSysMessage("|cffffa500此物品没有配置鉴定组，只应用了幻境倍率|r");
    }

    // 删除待鉴定标记
    // 【修复】仅成功（或无鉴定组仅应用倍率）时删除；掷骰失败等可恢复失败保留标记允许重试
    if (identifySuccess)
    {
        CharacterDatabase.DirectExecute("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = {}", itemGuid);
    }

    if (identificationGroupId == 0)
        QueueAllModuleDataAddonRefresh(player, itemId, itemGuid, 2000ms);

    return identifySuccess;
}

// 手动鉴定命令处理
// 用法: .鉴定 手动 [背包号 槽位号] 或 .鉴定 手动 (鉴定鼠标指向的物品)
bool ItemIdentificationCommandScript::HandleManualIdentifyCommand(ChatHandler* handler, const char* args)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 检查系统是否启用
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("|cffff0000物品鉴定系统当前已禁用|r");
        return true;
    }

    // 如果没有参数，显示用法
    if (!args || !*args)
    {
        handler->SendSysMessage("|cff00ff00===== 手动鉴定用法 =====|r");
        handler->SendSysMessage(".鉴定 手动 <背包号> <槽位号>");
        handler->SendSysMessage("  背包号: 0=主背包, 1-4=额外背包");
        handler->SendSysMessage("  槽位号: 从0开始的物品位置");
        handler->SendSysMessage("");
        handler->SendSysMessage("示例: .鉴定 手动 0 5  (鉴定主背包第6格物品)");
        handler->SendSysMessage("示例: .鉴定 手动 1 0  (鉴定第1个额外背包第1格)");
        handler->SendSysMessage("");
        handler->SendSysMessage("|cff888888提示: 使用 .鉴定 待鉴定 查看待鉴定列表|r");
        handler->SendSysMessage("|cff888888提示: 使用 .鉴定 批量鉴定 一键鉴定所有|r");
        return true;
    }

    // 解析参数: 背包号 槽位号
    // 【安全修复】使用std::strtol并增加输入验证
    char* bagStr = strtok((char*)args, " ");
    char* slotStr = strtok(nullptr, " ");

    if (!bagStr || !slotStr)
    {
        handler->SendSysMessage("|cffff0000参数不足！用法: .鉴定 手动 <背包号> <槽位号>|r");
        return true;
    }

    // 【安全修复】验证输入是否为有效数字
    char* endPtr = nullptr;
    long bagLong = strtol(bagStr, &endPtr, 10);
    if (endPtr == bagStr || *endPtr != '\0' || bagLong < 0 || bagLong > 255)
    {
        handler->SendSysMessage("|cffff0000无效的背包号，必须是0-255之间的整数|r");
        return true;
    }

    endPtr = nullptr;
    long slotLong = strtol(slotStr, &endPtr, 10);
    if (endPtr == slotStr || *endPtr != '\0' || slotLong < 0 || slotLong > 255)
    {
        handler->SendSysMessage("|cffff0000无效的槽位号，必须是0-255之间的整数|r");
        return true;
    }

    uint8 bagSlot = static_cast<uint8>(bagLong);
    uint8 slot = static_cast<uint8>(slotLong);

    if (bagSlot > 4)
    {
        handler->SendSysMessage("|cffff0000无效的背包号！0=主背包, 1-4=额外背包|r");
        return true;
    }

    // 获取物品
    Item* item = GetItemByBagSlot(player, bagSlot, slot);
    if (!item)
    {
        handler->PSendSysMessage("|cffff0000背包{}槽位{}没有物品|r", bagSlot, slot);
        return true;
    }

    // 执行鉴定
    DoIdentifyItem(player, item, handler);

    return true;
}

// 查询待鉴定物品列表命令处理
// 用法: .鉴定 待鉴定
bool ItemIdentificationCommandScript::HandleListPendingCommand(ChatHandler* handler, const char* /*args*/)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    QueryResult result = CharacterDatabase.Query(
        "SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
        "WHERE `玩家GUID` = {} ORDER BY `物品GUID` DESC LIMIT 50",
        playerGuid
    );

    if (!result)
    {
        handler->SendSysMessage("|cff00ff00没有待鉴定的物品|r");
        return true;
    }

    handler->SendSysMessage("|cff00ff00===== 待鉴定物品列表 =====|r");

    uint32 count = 0;
    uint32 validCount = 0;
    std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);

    // 遍历结果，同时查找物品在背包中的位置
    do
    {
        Field* fields = result->Fetch();
        uint32 guid = fields[0].Get<uint32>();
        uint32 itemId = fields[1].Get<uint32>();
        uint256 multiplier = fields[2].Get<uint256>();
        char multiplierMode = DbValueToHuanJingMode(fields[3].Get<int32>());

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        std::string itemName = proto ? proto->Name1 : "未知物品";

        // 查找物品在背包中的位置
        bool found = false;
        uint8 foundBag = 0;
        uint8 foundSlot = 0;

        auto itemItr = itemsByGuid.find(guid);
        Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
        if (item)
        {
            uint8 serverBag = item->GetBagSlot();
            uint8 serverSlot = item->GetSlot();

            if (serverBag == INVENTORY_SLOT_BAG_0)
            {
                if (serverSlot >= INVENTORY_SLOT_ITEM_START && serverSlot < INVENTORY_SLOT_ITEM_END)
                {
                    found = true;
                    foundBag = 0;
                    foundSlot = serverSlot - INVENTORY_SLOT_ITEM_START;
                }
                else if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
                {
                    found = true;
                    foundBag = 255;
                    foundSlot = serverSlot + 1;
                }
            }
            else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
            {
                found = true;
                foundBag = serverBag - INVENTORY_SLOT_BAG_START + 1;
                foundSlot = serverSlot;
            }
        }

        if (found)
        {
            handler->PSendSysMessage("|cffffffff[{}]|r |cffff8000{}{}|r |cff00ff00背包{} 槽位{}|r",
                itemName, multiplierMode == '+' ? "+" : (multiplierMode == '-' ? "关闭" : "x"), multiplier.str(), foundBag, foundSlot);
            validCount++;
        }
        else
        {
            handler->PSendSysMessage("|cff555555[{}]|r |cffff0000(物品不在背包)|r", itemName);
        }

        count++;
    } while (result->NextRow());

    handler->SendSysMessage("");
    handler->PSendSysMessage("|cff00ff00共 {} 个待鉴定，{} 个在背包中|r", count, validCount);
    handler->SendSysMessage("");
    handler->SendSysMessage("|cff888888鉴定命令: .鉴定 手动 <背包号> <槽位号>|r");
    handler->SendSysMessage("|cff888888一键鉴定: .鉴定 批量鉴定|r");

    return true;
}

// 批量鉴定所有待鉴定物品命令处理
// 用法: .鉴定 批量鉴定
bool ItemIdentificationCommandScript::HandleBatchIdentifyCommand(ChatHandler* handler, const char* /*args*/)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 检查系统是否启用
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("|cffff0000物品鉴定系统当前已禁用|r");
        return true;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();

    QueryResult result = CharacterDatabase.Query(
        "SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
        "WHERE `玩家GUID` = {} ORDER BY `物品GUID` ASC",
        playerGuid
    );

    if (!result)
    {
        handler->SendSysMessage("|cff00ff00没有待鉴定的物品|r");
        return true;
    }

    handler->SendSysMessage("|cff00ff00===== 开始批量鉴定 =====|r");

    uint32 successCount = 0;
    uint32 failCount = 0;
    uint32 skipCount = 0;
    std::vector<uint32> processedGuids;
    std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);
    // IdentifyItem 可能消耗背包材料或按配置销毁物品，销毁的物品可能仍在本批列表中，
    // 索引缓存的 Item* 会变成悬垂指针；发生过鉴定后必须重建索引再取指针
    bool itemIndexDirty = false;

    do
    {
        Field* fields = result->Fetch();
        uint32 itemGuid = fields[0].Get<uint32>();
        uint32 itemId = fields[1].Get<uint32>();
        uint256 huanJingMultiplier = fields[2].Get<uint256>();
        char huanJingMode = DbValueToHuanJingMode(fields[3].Get<int32>());
        uint32 identificationGroupId = fields[4].Get<uint32>();

        // 从本次请求的一次性背包索引中查找物品，避免每个GUID重复扫描背包
        if (itemIndexDirty)
        {
            itemsByGuid = BuildPlayerItemGuidIndex(player);
            itemIndexDirty = false;
        }
        auto itemItr = itemsByGuid.find(itemGuid);
        Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
        if (!item)
        {
            // 物品不存在：标记已失效，纳入批量清理
            processedGuids.push_back(itemGuid);
            skipCount++;
            continue;
        }

        // 验证物品ID
        if (item->GetEntry() != itemId)
        {
            processedGuids.push_back(itemGuid);
            skipCount++;
            continue;
        }

        // 应用幻境倍率
#ifdef MODULE_HUANJING_SYSTEM
        if (HasHuanJingEffect(huanJingMultiplier, huanJingMode))
        {
            if (sHuanJingSystem)
            {
                sHuanJingSystem->ApplyItemAttributeMultiplier(item, huanJingMultiplier, huanJingMode);
            }
        }
#endif

        // 执行鉴定
        // 【审计修复】使用 IdentifyItem 进行鉴定，确保成功率和费用扣除逻辑生效
        bool success = true;
        if (identificationGroupId > 0)
        {
            success = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
            itemIndexDirty = true;
        }

        if (success)
        {
            successCount++;
            // 【修复】仅成功时删除待鉴定标记；失败保留标记允许重试
            processedGuids.push_back(itemGuid);
            if (identificationGroupId == 0)
                QueueAllModuleDataAddonRefresh(player, itemId, itemGuid, 2000ms);
        }
        else
        {
            failCount++;
        }

    } while (result->NextRow());

    // 批量删除已处理的标记
    if (!processedGuids.empty())
    {
        std::ostringstream ss;
        ss << "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` IN (";
        for (size_t i = 0; i < processedGuids.size(); ++i)
        {
            if (i > 0) ss << ",";
            ss << processedGuids[i];
        }
        ss << ")";
        CharacterDatabase.DirectExecute(ss.str().c_str());
    }

    handler->SendSysMessage("|cff00ff00===== 批量鉴定完成 =====|r");
    handler->PSendSysMessage("|cff00ff00成功: {}|r, |cffff0000失败: {}|r, |cff888888跳过: {}|r",
        successCount, failCount, skipCount);

    return true;
}

// 处理打开UI界面命令: .鉴定 界面 或 .鉴定 ui
bool ItemIdentificationCommandScript::HandleOpenUICommand(ChatHandler* handler, const char* /*args*/)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 发送打开UI界面的Addon消息
    SendItemIdentificationAddonMessage(player, "OPEN_UI");

    return true;
}

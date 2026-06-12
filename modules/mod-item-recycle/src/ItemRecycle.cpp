#include "ItemRecycle.h"
#include "AddonThrottle.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "QuestDef.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <map>
#include <unordered_map>
#include <sstream>
#include <string>
#include <limits>

namespace
{
    void AddCopperReward(Player* player, uint64 amount)
    {
        if (amount <= static_cast<uint64>(std::numeric_limits<int64>::max()))
        {
            player->ModifyMoney(static_cast<int64>(amount));
            return;
        }

        player->SetMoney(player->GetMoney() + static_cast<int128>(amount));
    }

    std::set<uint32> const& GetProtectedQuestItems()
    {
        static std::set<uint32> protectedItems;
        static bool initialized = false;

        if (!initialized)
        {
            for (auto const& pair : sObjectMgr->GetQuestTemplates())
            {
                Quest const* quest = pair.second;
                if (!quest)
                    continue;

                if (quest->GetSrcItemId())
                    protectedItems.insert(quest->GetSrcItemId());

                for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
                {
                    if (quest->RequiredItemId[i] && quest->RequiredItemCount[i] > 0)
                        protectedItems.insert(quest->RequiredItemId[i]);
                }
            }

            initialized = true;
        }

        return protectedItems;
    }
}

// 获取需求模板的详细信息
std::string GetRequirementDetails(uint32 templateId)
{
    try
    {
        // 查询需求模板的详细信息
        QueryResult result = WorldDatabase.Query("SELECT `客户端显示`, `注释`, `需要人物等级`, `消耗金币`, `消耗物品` FROM `需求_模板` WHERE `id` = {}", templateId);
        if (!result)
            return "";

        Field* fields = result->Fetch();
        std::string clientDisplay = fields[0].Get<std::string>();
        std::string comment = fields[1].Get<std::string>();
        std::string levelReq = fields[2].Get<std::string>();
        uint64 goldCost = fields[3].Get<uint64>();
        std::string itemCost = fields[4].Get<std::string>();

        // 如果有客户端显示信息，优先使用
        if (!clientDisplay.empty() && clientDisplay != "0")
        {
            return clientDisplay;
        }

        // 否则构建详细需求信息
        std::ostringstream ss;
        ss << "需要：";

        bool hasRequirement = false;

        // 等级需求
        if (!levelReq.empty() && levelReq != "0")
        {
            ss << "等级" << levelReq;
            hasRequirement = true;
        }

        // 金币需求
        if (goldCost > 0)
        {
            if (hasRequirement) ss << "，";
            ss << goldCost << "金币";
            hasRequirement = true;
        }

        // 物品需求
        if (!itemCost.empty() && itemCost != "0")
        {
            if (hasRequirement) ss << "，";
            ss << "指定物品";
            hasRequirement = true;
        }

        // 如果有注释，添加注释
        if (!comment.empty() && comment != "0")
        {
            if (hasRequirement) ss << "，";
            ss << comment;
            hasRequirement = true;
        }

        return hasRequirement ? ss.str() : "不满足回收条件";
    }
    catch (std::exception& e)
    {
        LOG_ERROR("server.loading", "获取需求模板{}详细信息失败: {}", templateId, e.what());
        return "不满足回收条件";
    }
}

// 模块局部变量
namespace ItemRecycleGlobals
{
    bool bItemRecycleInitialized = false;
    bool bItemRecycleEnabled = false;
    bool bItemRecycleDebugMode = false;
    uint32 autoRecycleCheckInterval = 5000; // 5秒检查一次
    // 【审计修复】从配置文件读取的配置项
    uint32 defaultRecycleInterval = 60;     // 默认回收间隔（秒）
    uint32 minRecycleInterval = 10;         // 最小回收间隔（秒）
    uint32 maxRecycleInterval = 3600;       // 最大回收间隔（秒）
    bool allowCustomSettings = true;        // 是否允许玩家自定义设置
}

// Addon 消息前缀（需与客户端保持一致，长度<=16）
static constexpr char const* ITEM_RECYCLE_ADDON_PREFIX = "ITEMRECYCLE";

// Addon 消息处理脚本 - 用于与客户端UI交换结构化数据
class ItemRecycleAddonScript : public PlayerScript
{
public:
    ItemRecycleAddonScript() : PlayerScript("ItemRecycleAddonScript") { }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player)
            return;

        // 只处理 Addon 私聊消息
        if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        // 服务器收到的格式为："前缀\t消息"
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != ITEM_RECYCLE_ADDON_PREFIX)
            return;

        // 【防刷】统一令牌桶节流：默认 500ms/突发4，超频静默丢弃（modules/AddonThrottle.h）
        if (!ModuleAddon::Throttle::Allow(player->GetGUID(), "ITEMRECYCLE"))
            return;

        std::string command = msg.substr(tabPos + 1);

        // UI 配置请求
        if (command == "UI_REQUEST")
        {
            HandleAddonUIRequest(player);
        }
    }

private:
    static void SendAddonMessage(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        // 构造 "前缀\t消息" 格式，兼容 AzerothCore Addon 通道
        std::string fullMessage = std::string(ITEM_RECYCLE_ADDON_PREFIX) + '\t' + payload;

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }

    static void HandleAddonUIRequest(Player* player)
    {
        if (!player)
            return;

        ChatHandler handler(player->GetSession());

        // 【审计修复】检查配置文件中的Enable设置，不允许绕过
        if (!sConfigMgr->GetOption<bool>("ItemRecycle.Enable", true))
        {
            handler.SendSysMessage("|cffff0000[回收系统]|r 模块已被配置禁用");
            return;
        }

        // 强制初始化检查和修复（与 .回收 界面 命令保持一致）
        if (!ItemRecycleGlobals::bItemRecycleInitialized)
        {
            handler.SendSysMessage("|cffff8800[回收系统]|r 模块正在初始化，正在尝试强制初始化...");

            ItemRecycleGlobals::bItemRecycleEnabled = true;

            try
            {
                ItemRecycleScript::LoadItemRecycleFromDB();
                ItemRecycleScript::LoadPlayerRecycleSettings();
                ItemRecycleGlobals::bItemRecycleInitialized = true;
                handler.SendSysMessage("|cff00ff00[回收系统]|r 模块初始化成功");
            }
            catch (std::exception& e)
            {
                handler.PSendSysMessage("|cffff0000[回收系统]|r 初始化失败: {}", e.what());
                return;
            }
        }

        uint32 playerGuid = player->GetGUID().GetCounter();
        PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

        if (ItemRecycleGlobals::bItemRecycleDebugMode)
        {
            LOG_DEBUG("module.itemrecycle", "Addon UI_REQUEST: 构建UI数据, playerGuid={}", playerGuid);
        }

        std::ostringstream ss;
        ss << "RECYCLE_UI_DATA:";
        ss << (settings.autoRecycleEnabled ? 1 : 0) << ":";
        ss << settings.recycleInterval << ":";
        for (int i = 1; i <= 6; ++i)
            ss << (settings.recycleTypes[i] ? 1 : 0) << ":";
        ss << (settings.recycleTypes[7] ? 1 : 0) << ":"; // "所有类型" 开关
        ss << settings.minQuality << ":";
        ss << settings.maxQuality << ":";
        ss << settings.minLevel << ":";
        ss << settings.maxLevel << ":";
        ss << (settings.protectEquipped ? 1 : 0) << ":";

        if (!settings.filteredItems.empty())
        {
            bool first = true;
            for (uint32 itemId : settings.filteredItems)
            {
                if (!first)
                    ss << ",";
                ss << itemId;
                first = false;
            }
        }
        else
        {
            ss << "6948"; // 默认炉石
        }

        std::string uiData = ss.str();

        if (ItemRecycleGlobals::bItemRecycleDebugMode)
        {
            LOG_DEBUG("module.itemrecycle", "Addon UI_REQUEST: UI数据长度={} 内容={}", uiData.length(), uiData);
        }

        // 通过 Addon 通道发送给客户端
        SendAddonMessage(player, uiData);
    }
};

std::vector<ItemRecycleInfo> ItemRecycleScript::m_ItemRecycleStore;
std::unordered_map<uint32, PlayerRecycleSettings> ItemRecycleScript::m_PlayerSettings;
std::unordered_map<uint32, std::pair<std::unordered_set<uint32>, uint32>> ItemRecycleScript::m_MaterialReserveCache;

// 构造函数
ItemRecycleScript::ItemRecycleScript() : CommandScript("ItemRecycleScript")
{
    // 初始化操作移至WorldScript的OnUpdate中
}

// 脚本注册函数
void AddItemRecycleScripts()
{
    new ItemRecycleScript();
    new ItemRecyclePlayerScript();
    new ItemRecycle_Worldscript();
    new ItemRecycleAddonScript();
}

Acore::ChatCommands::ChatCommandTable ItemRecycleScript::GetCommands() const
{
    using namespace Acore::ChatCommands;

    static ChatCommandTable recycleSubCommands =
    {
        { "界面",   HandleRecycleUICommand,      SEC_PLAYER, Console::No },
        { "状态",   HandleRecycleStatusCommand,  SEC_PLAYER, Console::No },
        { "自动",   HandleRecycleAutoCommand,    SEC_PLAYER, Console::No },
        { "设置",   HandleRecycleSetCommand,     SEC_PLAYER, Console::No },
        { "回收",   HandleRecycleGroupCommand,   SEC_PLAYER, Console::No },
        { "执行",   HandleRecycleExecuteCommand, SEC_PLAYER, Console::No },
        { "测试",   HandleRecycleTestCommand,    SEC_PLAYER, Console::No },
        { "过滤",   HandleRecycleFilterCommand,  SEC_PLAYER, Console::No },
        { "更新配置", HandleRecycleUpdateConfigCommand, SEC_PLAYER, Console::No }
    };

    static ChatCommandTable commandTable =
    {
        { "回收", recycleSubCommands }
    };
    return commandTable;
}

void ItemRecycleScript::LoadItemRecycleFromDB()
{
    m_ItemRecycleStore.clear();

    QueryResult result = WorldDatabase.Query("SELECT `id`, `组`, `优先级`, `物品id`, `回收需求`, `回收奖励`, `回收类型`, `最小等级`, `最大等级`, `品质要求`, `过滤物品`, `启用状态`, `注释` FROM `_物品回收_模板` WHERE `启用状态` = 1 ORDER BY `优先级` ASC");
    if (!result)
    {
         //LOG_WARN("server.loading", "物品回收: 未找到任何回收配置记录");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ItemRecycleInfo info;
        info.id = fields[0].Get<uint32>();
        info.group = fields[1].Get<uint32>();
        info.priority = fields[2].Get<uint32>();
        info.itemId = fields[3].Get<uint32>();
        info.requirementTemplateId = fields[4].Get<uint32>();
        info.rewardTemplateId = fields[5].Get<uint32>();
        info.recycleType = fields[6].Get<uint32>();
        info.minLevel = fields[7].Get<uint32>();
        info.maxLevel = fields[8].Get<uint32>();
        info.qualityRequirement = fields[9].Get<int32>();

        // 解析过滤物品列表（按规则定义的不参与自动/界面回收的物品）
        info.filterItems.clear();
        std::string filterItemsStr = fields[10].Get<std::string>();
        if (!filterItemsStr.empty())
        {
            std::istringstream iss(filterItemsStr);
            std::string itemIdStr;
            while (std::getline(iss, itemIdStr, ','))
            {
                if (!itemIdStr.empty())
                {
                    try
                    {
                        uint32 itemId = std::stoul(itemIdStr);
                        if (itemId > 0)
                        {
                            info.filterItems.insert(itemId);
                        }
                    }
                    catch (const std::exception&)
                    {
                        // 忽略无效的物品ID
                    }
                }
            }
        }

        info.enabled = fields[11].Get<bool>();
        info.comment = fields[12].Get<std::string>();

        m_ItemRecycleStore.push_back(info);
        count++;
    } while (result->NextRow());
}

bool ItemRecycleScript::IsProtectedQuestItem(uint32 itemId)
{
    if (!itemId)
        return false;

    std::set<uint32> const& protectedItems = GetProtectedQuestItems();
    return protectedItems.find(itemId) != protectedItems.end();
}

bool ItemRecycleScript::IsActiveQuestItem(Player* player, uint32 itemId)
{
    if (!player || !itemId)
        return false;

    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = player->GetQuestSlotQuestId(slot);
        if (!questId)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        if (quest->GetSrcItemId() == itemId && quest->GetSrcItemCount() > 0)
            return true;

        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (quest->RequiredItemId[i] == itemId && quest->RequiredItemCount[i] > 0)
                return true;
        }
    }

    return false;
}

void ItemRecycleScript::LoadPlayerRecycleSettings()
{
    m_PlayerSettings.clear();

    QueryResult result = CharacterDatabase.Query("SELECT `玩家GUID`, `自动回收启用`, `回收间隔`, `回收类型1`, `回收类型2`, `回收类型3`, `回收类型4`, `回收类型5`, `回收类型6`, `回收所有类型`, `最小品质`, `最大品质`, `最小等级`, `最大等级`, `保护装备`, `过滤物品列表` FROM `物品_回收玩家配置`");
    if (!result)
    {
        LOG_INFO("server.loading", "物品回收: 未找到玩家回收设置");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        PlayerRecycleSettings settings;
        settings.playerGuid = fields[0].Get<uint32>();
        settings.autoRecycleEnabled = fields[1].Get<bool>();
        settings.recycleInterval = fields[2].Get<uint32>();
        settings.recycleTypes[1] = fields[3].Get<bool>();
        settings.recycleTypes[2] = fields[4].Get<bool>();
        settings.recycleTypes[3] = fields[5].Get<bool>();
        settings.recycleTypes[4] = fields[6].Get<bool>();
        settings.recycleTypes[5] = fields[7].Get<bool>();
        settings.recycleTypes[6] = fields[8].Get<bool>();
        settings.recycleTypes[7] = fields[9].Get<bool>(); // 所有类型
        settings.minQuality = fields[10].Get<uint32>();
        settings.maxQuality = fields[11].Get<uint32>();
        settings.minLevel = fields[12].Get<uint32>();
        settings.maxLevel = fields[13].Get<uint32>();
        settings.protectEquipped = fields[14].Get<bool>();

        // 解析过滤物品列表
        settings.filteredItems.clear();
        std::string filteredItemsStr = fields[15].Get<std::string>();
        if (!filteredItemsStr.empty())
        {
            std::istringstream iss(filteredItemsStr);
            std::string itemIdStr;
            while (std::getline(iss, itemIdStr, ','))
            {
                if (!itemIdStr.empty())
                {
                    try
                    {
                        uint32 itemId = std::stoul(itemIdStr);
                        if (itemId > 0)
                        {
                            settings.filteredItems.insert(itemId);
                        }
                    }
                    catch (const std::exception&)
                    {
                        // 忽略无效的物品ID
                    }
                }
            }
        }

        // 默认添加炉石到过滤列表（如果还没有的话）
        settings.filteredItems.insert(6948);

        m_PlayerSettings[settings.playerGuid] = settings;
        count++;
    } while (result->NextRow());

    //LOG_INFO("server.loading", ">> 已加载 {} 个玩家回收设置", count);
}

// 检查物品是否可以回收
bool ItemRecycleScript::CanRecycleItem(Player* player, Item* item, const PlayerRecycleSettings& settings)
{
    if (!item || !player)
        return false;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return false;

    if (IsReservedByMaterialWarehouse(player, item->GetEntry()))
        return false;

    // 当前任务仍需要的来源/需求物品不能回收，避免玩家正在做任务时误删进度物品。
    if (IsActiveQuestItem(player, item->GetEntry()))
        return false;

    // 任务奖励装备允许回收；只对真正的任务道具保留全局保护，避免回收未来任务会直接用到的道具。
    if (itemTemplate->Class == ITEM_CLASS_QUEST && IsProtectedQuestItem(item->GetEntry()))
        return false;

    // 检查是否在过滤物品列表中
    if (settings.filteredItems.find(item->GetEntry()) != settings.filteredItems.end())
        return false;

    // 检查是否在全局回收规则的过滤物品列表中（按规则配置的过滤物品）
    if (const ItemRecycleInfo* recycleRule = FindMatchingRecycleRule(item, settings))
    {
        if (!recycleRule->filterItems.empty() &&
            recycleRule->filterItems.find(item->GetEntry()) != recycleRule->filterItems.end())
        {
            return false;
        }
    }

    // 检查是否是已装备的物品
    if (settings.protectEquipped && item->IsEquipped())
        return false;

    // 检查物品品质
    if (itemTemplate->Quality < settings.minQuality || itemTemplate->Quality > settings.maxQuality)
        return false;

    // 检查物品等级（支持无限等级范围）
    if (itemTemplate->ItemLevel < settings.minLevel)
        return false;

    // 只有当maxLevel不是999999999时才检查上限（支持无限等级）
    if (settings.maxLevel < 999999999 && itemTemplate->ItemLevel > settings.maxLevel)
        return false;

    // 根据物品类型判断回收类型
    uint32 recycleType = RECYCLE_TYPE_JUNK; // 默认垃圾类型

    switch (itemTemplate->Class)
    {
        case ITEM_CLASS_WEAPON:
        case ITEM_CLASS_ARMOR:
            recycleType = RECYCLE_TYPE_EQUIPMENT;
            break;
        case ITEM_CLASS_CONSUMABLE:
            recycleType = RECYCLE_TYPE_CONSUMABLE;
            break;
        case ITEM_CLASS_QUEST:
            recycleType = RECYCLE_TYPE_QUEST;
            break;
        case ITEM_CLASS_GEM:
            recycleType = RECYCLE_TYPE_GEM;
            break;
        case ITEM_CLASS_TRADE_GOODS:
            if (itemTemplate->SubClass == ITEM_SUBCLASS_ENCHANTING)
                recycleType = RECYCLE_TYPE_ENCHANT;
            else
                recycleType = RECYCLE_TYPE_JUNK;
            break;
        default:
            recycleType = RECYCLE_TYPE_JUNK;
            break;
    }

    // 新增：检查回收类型权限
    // 如果启用了"所有类型"开关，则跳过具体类型检查
    if (!settings.recycleTypes[7]) // 索引7对应RECYCLE_TYPE_ALL(-1)
    {
        // 检查具体类型是否启用
        if (recycleType >= 1 && recycleType <= 6 && !settings.recycleTypes[recycleType])
            return false;
    }

    // 注意：需求模板检查在实际回收时进行，这里只检查基本条件

    return true;
}

bool ItemRecycleScript::IsReservedByMaterialWarehouse(Player* player, uint32 itemId)
{
    if (!player || !itemId)
        return false;

    if (!sConfigMgr->GetOption<bool>("MaterialWarehouse.Enable", true))
        return false;

    // 每玩家一次性加载"自动存储"物品集合并缓存 60 秒。
    // 此前自动回收每周期对每个背包物品都同步查一次库。
    uint32 const guid = player->GetGUID().GetCounter();
    uint32 const now = getMSTime();
    auto& cache = m_MaterialReserveCache[guid];
    if (!cache.second || getMSTimeDiff(cache.second, now) >= 60 * IN_MILLISECONDS)
    {
        cache.first.clear();
        if (QueryResult result = CharacterDatabase.Query(
            "SELECT `物品ID` FROM `_材料仓库玩家` WHERE `玩家GUID` = {} AND `自动存储` = 1",
            guid))
        {
            do
            {
                cache.first.insert((*result)[0].Get<uint32>());
            } while (result->NextRow());
        }
        cache.second = now ? now : 1;
    }

    return cache.first.count(itemId) > 0;
}

// 新增：查找匹配的回收规则
const ItemRecycleInfo* ItemRecycleScript::FindMatchingRecycleRule(Item* item, const PlayerRecycleSettings& settings)
{
    if (!item)
        return nullptr;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return nullptr;

    // 确定物品的回收类型
    uint32 recycleType = RECYCLE_TYPE_JUNK; // 默认垃圾类型
    switch (itemTemplate->Class)
    {
        case ITEM_CLASS_WEAPON:
        case ITEM_CLASS_ARMOR:
            recycleType = RECYCLE_TYPE_EQUIPMENT;
            break;
        case ITEM_CLASS_CONSUMABLE:
            recycleType = RECYCLE_TYPE_CONSUMABLE;
            break;
        case ITEM_CLASS_QUEST:
            recycleType = RECYCLE_TYPE_QUEST;
            break;
        case ITEM_CLASS_GEM:
            recycleType = RECYCLE_TYPE_GEM;
            break;
        case ITEM_CLASS_TRADE_GOODS:
            if (itemTemplate->SubClass == ITEM_SUBCLASS_ENCHANTING)
                recycleType = RECYCLE_TYPE_ENCHANT;
            else
                recycleType = RECYCLE_TYPE_JUNK;
            break;
        default:
            recycleType = RECYCLE_TYPE_JUNK;
            break;
    }

    // 查找匹配的回收规则（按优先级排序）
    const ItemRecycleInfo* bestMatch = nullptr;
    uint32 bestPriority = UINT32_MAX;

    for (const auto& recycleInfo : m_ItemRecycleStore)
    {
        if (!recycleInfo.enabled)
            continue;

        // 检查物品ID匹配（0表示通用规则）
        if (recycleInfo.itemId != 0 && recycleInfo.itemId != item->GetEntry())
            continue;

        // 检查回收类型匹配
        if (recycleInfo.recycleType != recycleType)
            continue;

        // 检查品质要求（-1表示所有品质）
        if (recycleInfo.qualityRequirement != -1 && itemTemplate->Quality != static_cast<uint32>(recycleInfo.qualityRequirement))
            continue;

        // 检查等级范围（支持无限等级）
        if (recycleInfo.minLevel > 0 && itemTemplate->ItemLevel < recycleInfo.minLevel)
            continue;
        // 只有当maxLevel不是999999999时才检查上限（支持无限等级）
        if (recycleInfo.maxLevel > 0 && recycleInfo.maxLevel < 999999999 && itemTemplate->ItemLevel > recycleInfo.maxLevel)
            continue;

        // 选择优先级最高的规则（数字越小优先级越高）
        if (recycleInfo.priority < bestPriority)
        {
            bestMatch = &recycleInfo;
            bestPriority = recycleInfo.priority;
        }
    }

    return bestMatch;
}

// 修改奖励系统：支持奖励模板和商店价格奖励
uint32 ItemRecycleScript::GetItemRecycleReward(Item* item, Player* player, const PlayerRecycleSettings& settings)
{
    if (!item)
        return 0;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return 0;

    // 查找匹配的回收规则
    const ItemRecycleInfo* recycleRule = FindMatchingRecycleRule(item, settings);
    if (recycleRule && recycleRule->rewardTemplateId > 0)
    {
        // 如果找到匹配的规则且设置了奖励模板，使用模板奖励
        if (RewardInterface* rewardModule = sModuleManager->GetRewardModule())
        {
            // 返回奖励模板ID，表示使用模板奖励
            return recycleRule->rewardTemplateId;
        }
    }

    // 如果没有匹配的规则或奖励模板ID为0，检查商店出售价格
    if (!recycleRule || recycleRule->rewardTemplateId == 0)
    {
        // 检查物品是否有商店出售价格
        if (itemTemplate->SellPrice > 0)
        {
            // 返回特殊值表示使用商店价格奖励
            return UINT32_MAX; // 使用最大值作为标识符，表示按商店价格给奖励
        }
        else
        {
            // 商店不能出售，返回特殊值表示摧毁
            return UINT32_MAX - 1; // 使用最大值-1作为标识符，表示摧毁物品
        }
    }

    // 默认情况下没有奖励
    return 0;
}

// 执行自动回收
void ItemRecycleScript::PerformAutoRecycle(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerRecycleSettings settings = GetPlayerRecycleSettings(playerGuid);

    if (!settings.autoRecycleEnabled)
        return;

    // 自动回收逻辑由定时器控制，这里直接执行

    std::vector<Item*> itemsToRecycle;
    uint32 totalReward = 0;
    uint32 totalCount = 0;

    // 遍历背包中的物品
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && CanRecycleItem(player, item, settings))
        {
            itemsToRecycle.push_back(item);
        }
    }

    // 遍历背包袋中的物品
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && CanRecycleItem(player, item, settings))
                {
                    itemsToRecycle.push_back(item);
                }
            }
        }
    }

    // 【白扣材料修复】原实现：先对每个模板"检查 N 次"（检查无副作用，N 次结果恒同，
    // 拥有 1 次的材料即可通过 N 次检查），再批量消耗 N 次——第 i 次消耗失败时直接 return，
    // 前 i-1 次已扣材料且对应物品未回收（白扣）。
    // 现改为回收循环内逐堆叠原子处理：消耗成功多少件就回收多少件，需求耗尽即停止。
    RequirementInterface* recycleReqModule = sModuleManager->GetRequirementModule();
    bool requirementExhausted = false;

    // 执行回收并收集奖励
    std::map<uint32, uint32> rewardTemplateCount; // 奖励模板ID -> 数量
    uint64 totalCopperReward = 0; // 总铜币奖励
    uint32 destroyedCount = 0; // 摧毁的物品数量

    for (Item* item : itemsToRecycle)
    {
        if (!item || requirementExhausted)
            continue;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            continue;

        uint32 stackCount = item->GetCount();
        uint32 count = stackCount;

        // 需求消耗：每回收 1 件消耗 1 次，失败即停（已消耗的次数与已回收件数一一对应）
        if (const ItemRecycleInfo* recycleRule = FindMatchingRecycleRule(item, settings))
        {
            if (recycleRule->requirementTemplateId > 0)
            {
                if (!recycleReqModule)
                    continue; // 配置了需求但需求模块不可用：跳过该物品，避免白拿

                uint32 consumed = 0;
                while (consumed < stackCount && recycleReqModule->ConsumeRequirements(player, recycleRule->requirementTemplateId))
                    ++consumed;

                if (consumed < stackCount)
                {
                    requirementExhausted = true;
                    if (consumed == 0)
                        continue; // 一次都没消耗成功：本堆叠原样保留

                    count = consumed; // 部分回收：按已消耗次数回收对应件数
                }
            }
        }

        uint32 rewardType = GetItemRecycleReward(item, player, settings);

        if (rewardType == UINT32_MAX)
        {
            // 按商店价格给奖励（铜币）
            uint64 sellPrice = itemTemplate->SellPrice;
            if (sellPrice > 0)
            {
                totalCopperReward += sellPrice * count;
            }
        }
        else if (rewardType == UINT32_MAX - 1)
        {
            // 摧毁物品，无奖励
            destroyedCount += count;
        }
        else if (rewardType > 0)
        {
            // 使用奖励模板
            rewardTemplateCount[rewardType] += count;
        }
        // 如果rewardType为0，则没有奖励

        totalCount += count;

        // 销毁物品（部分回收时只销毁已消耗需求对应的件数）
        if (count >= stackCount)
        {
            player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        }
        else
        {
            uint32 destroyPartial = count;
            player->DestroyItemCount(item, destroyPartial, true);
        }
    }

    if (requirementExhausted)
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff0000[自动回收]|r 回收需求材料不足，剩余物品未回收。");
    }

    if (totalCount > 0)
    {
        // 发放铜币奖励
        if (totalCopperReward > 0)
        {
            AddCopperReward(player, totalCopperReward);
        }

        // 发放奖励模板奖励（禁用单条消息，最后统一发送汇总消息）
        std::map<std::string, uint32> rewardSummary; // 基础描述 -> 总数量
        if (RewardInterface* rewardModule = sModuleManager->GetRewardModule())
        {
            for (const auto& pair : rewardTemplateCount)
            {
                uint32 rewardTemplateId = pair.first;
                uint32 count = pair.second;

                // 获取奖励描述用于汇总
                std::vector<std::string> rewardDesc = rewardModule->GetRewardDescription(player, rewardTemplateId);
                
                // 根据数量发放奖励，禁用单条消息通知
                for (uint32 i = 0; i < count; ++i)
                {
                    rewardModule->GiveReward(player, rewardTemplateId, true, false);
                }
                
                // 汇总奖励信息（解析数量并累加）
                for (const auto& desc : rewardDesc)
                {
                    std::string baseDesc = desc;
                    uint32 singleCount = 1;
                    
                    // 查找 " x" 后面的数字
                    size_t xPos = desc.rfind(" x");
                    if (xPos != std::string::npos && xPos + 2 < desc.length())
                    {
                        std::string numStr = desc.substr(xPos + 2);
                        bool isNumber = !numStr.empty();
                        for (char c : numStr)
                        {
                            if (!std::isdigit(c)) { isNumber = false; break; }
                        }
                        if (isNumber)
                        {
                            baseDesc = desc.substr(0, xPos);
                            singleCount = std::stoul(numStr);
                        }
                    }
                    rewardSummary[baseDesc] += singleCount * count;
                }
            }
        }

        // 发送回收结果消息给玩家
        std::string rewardMessage = "";
        if (totalCopperReward > 0)
        {
            uint64 gold = totalCopperReward / 10000;
            uint64 silver = (totalCopperReward % 10000) / 100;
            uint64 copper = totalCopperReward % 100;

            std::string moneyStr = "";
            if (gold > 0) moneyStr += std::to_string(gold) + "金";
            if (silver > 0) moneyStr += std::to_string(silver) + "银";
            if (copper > 0) moneyStr += std::to_string(copper) + "铜";

            if (!moneyStr.empty())
                rewardMessage += "，获得" + moneyStr;
        }
        if (destroyedCount > 0)
        {
            rewardMessage += "，摧毁" + std::to_string(destroyedCount) + "个无价值物品";
        }

        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[自动回收]|r 回收了 |cffff0000{}|r 个物品{}",
            totalCount, rewardMessage);
        
        // 发送奖励汇总消息
        if (!rewardSummary.empty())
        {
            for (const auto& summary : rewardSummary)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("  获得：{} x{}", summary.first, summary.second);
            }
        }

        // 保存设置（如果有变化）
        SavePlayerRecycleSettings(settings);
    }
}

// 保存玩家回收设置
void ItemRecycleScript::SavePlayerRecycleSettings(const PlayerRecycleSettings& settings)
{
    try
    {
        // 构建过滤物品列表字符串
        std::string filteredItemsStr;
        if (!settings.filteredItems.empty())
        {
            std::ostringstream oss;
            bool first = true;
            for (uint32 itemId : settings.filteredItems)
            {
                if (!first) oss << ",";
                oss << itemId;
                first = false;
            }
            filteredItemsStr = oss.str();
        }

        // 使用正确的SQL格式
        std::string sql = "REPLACE INTO `物品_回收玩家配置` (`玩家GUID`, `自动回收启用`, `回收间隔`, `回收类型1`, `回收类型2`, `回收类型3`, `回收类型4`, `回收类型5`, `回收类型6`, `回收所有类型`, `最小品质`, `最大品质`, `最小等级`, `最大等级`, `保护装备`, `过滤物品列表`) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, '{}')";

        CharacterDatabase.Execute(sql,
            settings.playerGuid,
            settings.autoRecycleEnabled ? 1 : 0,
            settings.recycleInterval,
            settings.recycleTypes[1] ? 1 : 0,
            settings.recycleTypes[2] ? 1 : 0,
            settings.recycleTypes[3] ? 1 : 0,
            settings.recycleTypes[4] ? 1 : 0,
            settings.recycleTypes[5] ? 1 : 0,
            settings.recycleTypes[6] ? 1 : 0,
            settings.recycleTypes[7] ? 1 : 0, // 所有类型
            settings.minQuality,
            settings.maxQuality,
            settings.minLevel,
            settings.maxLevel,
            settings.protectEquipped ? 1 : 0,
            filteredItemsStr);

        // 更新内存中的设置
        m_PlayerSettings[settings.playerGuid] = settings;

        LOG_DEBUG("server.loading", "物品回收: 已保存玩家 {} 的设置到数据库", settings.playerGuid);
    }
    catch (std::exception& e)
    {
        LOG_ERROR("server.loading", "物品回收: 保存玩家设置失败: {}", e.what());
    }
}

// 获取玩家回收设置
PlayerRecycleSettings ItemRecycleScript::GetPlayerRecycleSettings(uint32 playerGuid)
{
    auto it = m_PlayerSettings.find(playerGuid);
    if (it != m_PlayerSettings.end())
        return it->second;

    // 尝试从数据库加载设置
    try
    {
        QueryResult result = CharacterDatabase.Query("SELECT `自动回收启用`, `回收间隔`, `回收类型1`, `回收类型2`, `回收类型3`, `回收类型4`, `回收类型5`, `回收类型6`, `回收所有类型`, `最小品质`, `最大品质`, `最小等级`, `最大等级`, `保护装备`, `过滤物品列表` FROM `物品_回收玩家配置` WHERE `玩家GUID` = {}", playerGuid);

        if (result)
        {
            Field* fields = result->Fetch();

            PlayerRecycleSettings settings;
            settings.playerGuid = playerGuid;
            settings.autoRecycleEnabled = fields[0].Get<bool>();
            settings.recycleInterval = fields[1].Get<uint32>();
            settings.recycleTypes[1] = fields[2].Get<bool>();
            settings.recycleTypes[2] = fields[3].Get<bool>();
            settings.recycleTypes[3] = fields[4].Get<bool>();
            settings.recycleTypes[4] = fields[5].Get<bool>();
            settings.recycleTypes[5] = fields[6].Get<bool>();
            settings.recycleTypes[6] = fields[7].Get<bool>();
            settings.recycleTypes[7] = fields[8].Get<bool>(); // 所有类型
            settings.minQuality = fields[9].Get<uint32>();
            settings.maxQuality = fields[10].Get<uint32>();
            settings.minLevel = fields[11].Get<uint32>();
            settings.maxLevel = fields[12].Get<uint32>();
            settings.protectEquipped = fields[13].Get<bool>();
            settings.lastRecycleTime = 0; // 重新登录时重置时间

            // 解析过滤物品列表
            settings.filteredItems.clear();
            std::string filteredItemsStr = fields[14].Get<std::string>();
            if (!filteredItemsStr.empty())
            {
                std::istringstream iss(filteredItemsStr);
                std::string itemIdStr;
                while (std::getline(iss, itemIdStr, ','))
                {
                    if (!itemIdStr.empty())
                    {
                        try
                        {
                            uint32 itemId = std::stoul(itemIdStr);
                            if (itemId > 0)
                            {
                                settings.filteredItems.insert(itemId);
                            }
                        }
                        catch (const std::exception&)
                        {
                            // 忽略无效的物品ID
                        }
                    }
                }
            }

            // 确保炉石在过滤列表中
            settings.filteredItems.insert(6948);

            // 加载到内存中
            m_PlayerSettings[playerGuid] = settings;

            LOG_DEBUG("server.loading", "物品回收: 从数据库加载玩家 {} 的配置", playerGuid);
            return settings;
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR("server.loading", "物品回收: 从数据库加载玩家 {} 配置失败: {}", playerGuid, e.what());
    }

    // 如果数据库中没有记录或加载失败，创建默认设置
    PlayerRecycleSettings defaultSettings;
    defaultSettings.playerGuid = playerGuid;
    defaultSettings.autoRecycleEnabled = false;        // 默认关闭自动回收
    defaultSettings.recycleInterval = ItemRecycleGlobals::defaultRecycleInterval; // 【审计修复】使用配置的默认间隔
    for (int i = 1; i <= 6; ++i)
        defaultSettings.recycleTypes[i] = (i == 4);    // 默认只启用垃圾回收（类型4）
    defaultSettings.recycleTypes[7] = false;           // 新增：默认不启用"所有类型"开关
    defaultSettings.minQuality = 0;                    // 默认最小品质：灰色
    defaultSettings.maxQuality = 2;                    // 默认最大品质：绿色
    defaultSettings.minLevel = 1;                      // 默认最小等级：1级
    defaultSettings.maxLevel = 999999999;              // 默认最大等级：无限制
    defaultSettings.protectEquipped = true;            // 默认保护已装备物品
    defaultSettings.lastRecycleTime = 0;               // 初始化上次回收时间

    // 默认过滤炉石
    defaultSettings.filteredItems.clear();
    defaultSettings.filteredItems.insert(6948);        // 炉石

    // 保存默认设置到数据库和内存
    SavePlayerRecycleSettings(defaultSettings);
    m_PlayerSettings[playerGuid] = defaultSettings;

    LOG_DEBUG("server.loading", "物品回收: 为玩家 {} 创建默认配置", playerGuid);
    return defaultSettings;
}



// 删除玩家回收设置
void ItemRecycleScript::DeletePlayerRecycleSettings(uint32 playerGuid)
{
    try
    {
        // 从内存中移除玩家设置
        auto it = m_PlayerSettings.find(playerGuid);
        if (it != m_PlayerSettings.end())
        {
            m_PlayerSettings.erase(it);
            LOG_DEBUG("server.loading", "物品回收: 已从内存中移除玩家 {} 的配置", playerGuid);
        }

        // 从数据库中删除玩家设置
        CharacterDatabase.Execute("DELETE FROM `物品_回收玩家配置` WHERE `玩家GUID` = {}", playerGuid);

    }
    catch (std::exception& e)
    {
        LOG_ERROR("server.loading", "物品回收: 删除玩家 {} 配置时发生错误: {}", playerGuid, e.what());
    }
}

// 玩家脚本实现
void ItemRecyclePlayerScript::OnPlayerLogin(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 从数据库加载玩家设置到内存（GetPlayerRecycleSettings已经处理了数据库加载）
    PlayerRecycleSettings settings = ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);

    // 重新登录时重置回收时间，避免立即触发回收
    settings.lastRecycleTime = GameTime::GetGameTime().count();

    // 更新内存中的设置
    ItemRecycleScript::m_PlayerSettings[playerGuid] = settings;

    // 保存更新后的时间到数据库
    ItemRecycleScript::SavePlayerRecycleSettings(settings);

    // 调试信息
    if (ItemRecycleGlobals::bItemRecycleDebugMode)
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[自动回收调试]|r 玩家登录，配置已加载。自动回收: {}, 间隔: {}秒",
            settings.autoRecycleEnabled ? "开启" : "关闭", settings.recycleInterval);
    }
}

void ItemRecyclePlayerScript::OnPlayerLogout(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 清理材料仓库保留集合缓存
    ItemRecycleScript::m_MaterialReserveCache.erase(playerGuid);

    // 保存当前设置到数据库，然后从内存中移除
    auto it = ItemRecycleScript::m_PlayerSettings.find(playerGuid);
    if (it != ItemRecycleScript::m_PlayerSettings.end())
    {
        // 保存当前设置到数据库
        ItemRecycleScript::SavePlayerRecycleSettings(it->second);

        // 从内存中移除玩家设置
        ItemRecycleScript::m_PlayerSettings.erase(it);

        LOG_DEBUG("server.loading", "物品回收: 玩家 {} 登出时已保存配置到数据库", playerGuid);
    }
}

void ItemRecyclePlayerScript::OnPlayerUpdate(Player* player, uint32 diff)
{
    if (!player || !ItemRecycleGlobals::bItemRecycleEnabled)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 直接引用内存中的设置：此前这里每 tick 按值深拷贝整个设置（含 std::set）
    auto it = ItemRecycleScript::m_PlayerSettings.find(playerGuid);
    if (it == ItemRecycleScript::m_PlayerSettings.end())
    {
        // 登录时已加载，这里只做兜底加载
        ItemRecycleScript::GetPlayerRecycleSettings(playerGuid);
        it = ItemRecycleScript::m_PlayerSettings.find(playerGuid);
        if (it == ItemRecycleScript::m_PlayerSettings.end())
            return;
    }

    PlayerRecycleSettings& settings = it->second;

    if (!settings.autoRecycleEnabled)
        return;

    // 获取当前时间（秒）
    uint32 currentTime = GameTime::GetGameTime().count();

    // 如果是第一次或者上次回收时间为0，初始化时间
    // （lastRecycleTime 登录时会重置为0，只在内存中维护即可，不需要写库）
    if (settings.lastRecycleTime == 0)
    {
        settings.lastRecycleTime = currentTime;
        return;
    }

    // 检查是否到了回收时间
    uint32 timeSinceLastRecycle = currentTime - settings.lastRecycleTime;
    if (timeSinceLastRecycle >= settings.recycleInterval)
    {
        // 更新上次回收时间（仅内存）
        settings.lastRecycleTime = currentTime;

        // 执行自动回收
        ItemRecycleScript::PerformAutoRecycle(player);

        // 调试信息
        if (ItemRecycleGlobals::bItemRecycleDebugMode)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00[自动回收调试]|r 执行自动回收，间隔: {}秒", settings.recycleInterval);
        }
    }
}

void ItemRecyclePlayerScript::OnPlayerDelete(ObjectGuid guid, uint32 accountId)
{
    if (!guid.IsPlayer())
        return;

    uint32 playerGuid = guid.GetCounter();

    // 删除该角色的所有回收配置数据
    ItemRecycleScript::DeletePlayerRecycleSettings(playerGuid);

}

// ItemRecycle_Worldscript 类的实现
ItemRecycle_Worldscript::ItemRecycle_Worldscript() : WorldScript("ItemRecycle_Worldscript"), _initTimer(0), _initialized(false)
{
}

void ItemRecycle_Worldscript::OnAfterConfigLoad(bool /*reload*/)
{
    try
    {
        ItemRecycleGlobals::bItemRecycleEnabled = sConfigMgr->GetOption<bool>("ItemRecycle.Enable", true);
        ItemRecycleGlobals::bItemRecycleDebugMode = sConfigMgr->GetOption<bool>("ItemRecycle.DebugMode", false);
        // 【审计修复】加载配置文件中的所有配置项
        ItemRecycleGlobals::autoRecycleCheckInterval = sConfigMgr->GetOption<uint32>("ItemRecycle.AutoCheckInterval", 5000);
        ItemRecycleGlobals::defaultRecycleInterval = sConfigMgr->GetOption<uint32>("ItemRecycle.DefaultInterval", 60);
        ItemRecycleGlobals::minRecycleInterval = sConfigMgr->GetOption<uint32>("ItemRecycle.MinInterval", 10);
        ItemRecycleGlobals::maxRecycleInterval = sConfigMgr->GetOption<uint32>("ItemRecycle.MaxInterval", 3600);
        ItemRecycleGlobals::allowCustomSettings = sConfigMgr->GetOption<bool>("ItemRecycle.AllowCustomSettings", true);
    }
    catch (std::exception& e)
    {
        LOG_ERROR("server.loading", "物品回收模块: 配置加载错误: {}", e.what());
        ItemRecycleGlobals::bItemRecycleEnabled = true; // 默认启用
    }
}

void ItemRecycle_Worldscript::OnUpdate(uint32 diff)
{
    try
    {
        // 如果已经初始化或模块未启用，则不执行
        if (_initialized || !ItemRecycleGlobals::bItemRecycleEnabled)
            return;

        _initTimer += diff;

        // 在服务器启动后1秒（1000毫秒）初始化并显示信息
        if (_initTimer >= 1000)
        {
            _initialized = true;

            // 在这里执行所有初始化操作


            // 加载数据库数据
            ItemRecycleScript::LoadItemRecycleFromDB();
            ItemRecycleScript::LoadPlayerRecycleSettings();

            LOG_INFO("server.loading", "→物品回收系统√");

            // 设置模块初始化标志
            ItemRecycleGlobals::bItemRecycleInitialized = true;

            if (ItemRecycleGlobals::bItemRecycleDebugMode)
            {
                LOG_INFO("server.loading", "物品回收模块: 调试模式已启用");
            }
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR("server.loading", "物品回收模块: OnUpdate错误: {}", e.what());
        _initialized = true; // 防止继续尝试
    }
}


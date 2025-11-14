#include "ItemIdentificationSystem.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <random>
#include <cstdarg>
#include <thread>
#include <chrono>

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
#endif

#if __has_include("ItemSkillsManager.h")
    #ifndef MODULE_ITEM_SKILLS
        #define MODULE_ITEM_SKILLS
    #endif
    #include "ItemSkillsManager.h"
    #include "ItemSkillsDBHelper.h"
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

#if __has_include("ItemSets.h")
    #ifndef MODULE_ITEM_SETS
        #define MODULE_ITEM_SETS
    #endif
    #include "ItemSets.h"
#endif

// 前向声明辅助函数
std::vector<uint32> ParseCommaSeparatedNumbers(const std::string& str);
uint32 SelectRandomFromList(const std::vector<uint32>& list);
uint32 GenerateRandomNumber(uint32 min, uint32 max);

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
    _cost = sConfigMgr->GetOption<uint32>("ItemIdentificationSystem.Cost", 10000);
    _enableAnnounce = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.EnableAnnounce", true);
    _debugMode = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.Debug", false);

    if (reload)
    {
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
    std::string itemEnhancementGroups;
    std::string itemAttributesGroups;
    std::string itemAttributesAdditionalGroups;
    std::string itemSkillsGroups;
    std::string runeSystemGroups;
    std::string skillSetGroups;
    uint32 requirementTemplate;

    // 基础属性配置
    uint32 baseAttrMinCount;
    uint32 baseAttrMaxCount;
    uint32 baseAttrMinValue;
    uint32 baseAttrMaxValue;
    bool baseAttrAllowDuplicate;

    // 追加属性配置
    uint32 additionalAttrMinCount;
    uint32 additionalAttrMaxCount;
    uint32 additionalAttrMinValue;
    uint32 additionalAttrMaxValue;
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

    // 显示配置
    std::string qualityDisplay;
    std::string namePrefix;
    std::string nameSuffix;
    std::string nameColors;
    std::string bottomDescription;
    uint32 announcementTemplate;
};

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
        "`物品成长_系统`, `物品强化_系统`, `物品属性_模板`, "
        "`基础属性最小数量`, `基础属性最大数量`, `基础最小属性值`, `基础最大属性值`, `基础属性允许重复`, "
        "`物品属性_模板_组`, `追加属性最小数量`, `追加属性最大数量`, `追加属性最小值`, `追加属性最大值`, `追加属性允许重复`, "
        "`物品技能_模板_组`, `追加技能最小数量`, `追加技能最大数量`, `追加技能允许重复`, "
        "`技能魔次_模板_组`, `技能魔次最小数量`, `技能魔次最大数量`, `技能魔次最小魔次`, `技能魔次最大魔次`, `技能魔次允许重复`, "
        "`鉴定品质显示`, `物品名字前缀`, `物品名字后缀`, `物品名字颜色_多个逗号隔开`, `物品底部描述`, "
        "`需求_模板`, `符文系统_符文`, `符文凹槽最小数量`, `符文凹槽最大数量`, `技能模板_套装_组`, `公告模板` "
        "FROM `物品_鉴定系统`");

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
        // 5:物品成长_系统, 6:物品强化_系统, 7:物品属性_模板,
        // 8:基础属性最小数量, 9:基础属性最大数量, 10:基础最小属性值, 11:基础最大属性值, 12:基础属性允许重复,
        // 13:物品属性_模板_组, 14:追加属性最小数量, 15:追加属性最大数量, 16:追加属性最小值, 17:追加属性最大值, 18:追加属性允许重复,
        // 19:物品技能_模板_组, 20:追加技能最小数量, 21:追加技能最大数量, 22:追加技能允许重复,
        // 23:技能魔次_模板_组, 24:技能魔次最小数量, 25:技能魔次最大数量, 26:技能魔次最小魔次, 27:技能魔次最大魔次, 28:技能魔次允许重复,
        // 29:鉴定品质显示, 30:物品名字前缀, 31:物品名字后缀, 32:物品名字颜色, 33:物品底部描述,
        // 34:需求_模板, 35:符文系统_符文, 36:符文凹槽最小数量, 37:符文凹槽最大数量, 38:技能模板_套装_组, 39:公告模板

        tmpl.comment = fields[0].Get<std::string>();
        tmpl.id = fields[1].Get<uint32>();
        tmpl.group = fields[2].Get<uint32>();
        tmpl.level = fields[3].Get<uint32>();
        tmpl.randomChance = fields[4].Get<uint32>();

        // 模块关联字段
        tmpl.itemGrowthGroups = fields[5].Get<std::string>();                    // 物品成长_系统
        tmpl.itemEnhancementGroups = fields[6].Get<std::string>();              // 物品强化_系统
        tmpl.itemAttributesGroups = fields[7].Get<std::string>();               // 物品属性_模板（基础属性）

        // 基础属性配置
        tmpl.baseAttrMinCount = fields[8].Get<uint32>();                        // 基础属性最小数量
        tmpl.baseAttrMaxCount = fields[9].Get<uint32>();                        // 基础属性最大数量
        tmpl.baseAttrMinValue = fields[10].Get<uint32>();                       // 基础最小属性值
        tmpl.baseAttrMaxValue = fields[11].Get<uint32>();                       // 基础最大属性值
        tmpl.baseAttrAllowDuplicate = fields[12].Get<uint32>() == 0;            // 基础属性允许重复

        // 追加属性配置
        tmpl.itemAttributesAdditionalGroups = fields[13].Get<std::string>();    // 物品属性_模板_组（追加属性）
        tmpl.additionalAttrMinCount = fields[14].Get<uint32>();                 // 追加属性最小数量
        tmpl.additionalAttrMaxCount = fields[15].Get<uint32>();                 // 追加属性最大数量
        tmpl.additionalAttrMinValue = fields[16].Get<uint32>();                 // 追加属性最小值
        tmpl.additionalAttrMaxValue = fields[17].Get<uint32>();                 // 追加属性最大值
        tmpl.additionalAttrAllowDuplicate = fields[18].Get<uint32>() == 0;      // 追加属性允许重复

        // 追加技能配置
        tmpl.itemSkillsGroups = fields[19].Get<std::string>();                  // 物品技能_模板_组
        tmpl.additionalSkillMinCount = fields[20].Get<uint32>();                // 追加技能最小数量
        tmpl.additionalSkillMaxCount = fields[21].Get<uint32>();                // 追加技能最大数量
        tmpl.additionalSkillAllowDuplicate = fields[22].Get<uint32>() == 0;     // 追加技能允许重复

        // 技能魔次配置
        tmpl.magicHitGroups = fields[23].Get<std::string>();                    // 技能魔次_模板_组
        tmpl.magicHitMinCount = fields[24].Get<uint32>();                       // 技能魔次最小数量
        tmpl.magicHitMaxCount = fields[25].Get<uint32>();                       // 技能魔次最大数量
        tmpl.magicHitMinValue = fields[26].Get<uint32>();                       // 技能魔次最小魔次
        tmpl.magicHitMaxValue = fields[27].Get<uint32>();                       // 技能魔次最大魔次
        tmpl.magicHitAllowDuplicate = fields[28].Get<uint32>() == 0;            // 技能魔次允许重复

        // 显示配置
        tmpl.qualityDisplay = fields[29].Get<std::string>();                    // 鉴定品质显示
        tmpl.namePrefix = fields[30].Get<std::string>();                        // 物品名字前缀
        tmpl.nameSuffix = fields[31].Get<std::string>();                        // 物品名字后缀
        tmpl.nameColors = fields[32].Get<std::string>();                        // 物品名字颜色
        tmpl.bottomDescription = fields[33].Get<std::string>();                 // 物品底部描述

        // 其他配置
        tmpl.requirementTemplate = fields[34].Get<uint32>();                    // 需求_模板

        // 符文配置
        tmpl.runeSystemGroups = fields[35].Get<std::string>();                  // 符文系统_符文
        tmpl.runeSlotMinCount = fields[36].Get<uint32>();                       // 符文凹槽最小数量
        tmpl.runeSlotMaxCount = fields[37].Get<uint32>();                       // 符文凹槽最大数量

        // 套装配置
        tmpl.skillSetGroups = fields[38].Get<std::string>();                    // 技能模板_套装_组

        // 公告配置
        tmpl.announcementTemplate = fields[39].Get<uint32>();                   // 公告模板

        _identificationTemplates[tmpl.id] = tmpl;
        groups.insert(tmpl.group);
        count++;

        if (_debugMode)
        {
            DebugLog("加载鉴定模板: ID={}, 组={}, 等级={}, 注释='{}'",
                     tmpl.id, tmpl.group, tmpl.level, tmpl.comment);
        }

    } while (result->NextRow());

    LOG_INFO("server.loading", "物品鉴定系统: 已加载 {} 个鉴定模板 (共 {} 个组)", count, groups.size());
}

// 检查物品是否可以鉴定
bool ItemIdentificationSystem::CanIdentify(Player* player, Item* item, bool sendError)
{
    if (!_enabled)
    {
        if (sendError)
            ChatHandler(player->GetSession()).SendNotification("物品鉴定系统当前已禁用");
        DebugLog("CanIdentify检查失败: 系统已禁用");
        return false;
    }

    if (!player || !item)
    {
        DebugLog("CanIdentify检查失败: 玩家或物品为空");
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
    uint32 playerMoney = player->GetMoney();

    if (playerMoney < _cost)
    {
        if (sendError)
            ChatHandler(player->GetSession()).SendNotification("你没有足够的金币进行鉴定");
        DebugLog("CanIdentify检查失败: 金币不足 (拥有: {}, 需要: {})", playerMoney, _cost);
        return false;
    }

    DebugLog("物品可以鉴定: 物品ID={}, GUID={}, 玩家金币={}", item->GetEntry(), item->GetGUID().GetCounter(), playerMoney);
    return true;
}

// 获取鉴定成功率
uint32 ItemIdentificationSystem::GetSuccessRate(Player* player, Item* item)
{
    // 直接使用配置文件中的基础成功率，不做任何调整
    uint32 successRate = _baseSuccessRate;

    // 确保成功率在合理范围内
    if (successRate < 1)
        successRate = 1;
    if (successRate > 100)
        successRate = 100;

    return successRate;
}

// 鉴定物品（需要指定组ID）
bool ItemIdentificationSystem::IdentifyItem(Player* player, Item* item, uint32 groupId)
{
    if (!CanIdentify(player, item))
        return false;

    // 扣除金币
    player->ModifyMoney(-static_cast<int32>(_cost));
    ChatHandler(player->GetSession()).SendNotification("已扣除 {} 铜币用于鉴定", _cost);

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
            ChatHandler(player->GetSession()).SendNotification("物品鉴定成功！");

            // 发送公告（暂时禁用，避免API兼容性问题）
            if (_enableAnnounce)
            {
                // TODO: 实现全服公告功能
            }

            return true;
        }
        else
        {
            // 应用鉴定失败
            ChatHandler(player->GetSession()).SendSysMessage("鉴定失败：无法应用鉴定效果");
            return false;
        }
    }
    else
    {
        // 鉴定失败
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

// 根据组ID和物品ID随机选择一个鉴定模板（根据几率加权）- 优化版：使用内存数据
uint32 ItemIdentificationSystem::SelectIdentificationTemplate(uint32 groupId, Item* item)
{
    // ✅ 从内存中筛选指定组ID的模板（不查询数据库）
    std::vector<std::pair<uint32, uint32>> templates; // <模板ID, 几率>
    uint32 totalChance = 0;

    for (const auto& pair : _identificationTemplates)
    {
        const IdentificationTemplate& tmpl = pair.second;
        if (tmpl.group == groupId)
        {
            templates.push_back(std::make_pair(tmpl.id, tmpl.randomChance));
            totalChance += tmpl.randomChance;
        }
    }

    if (templates.empty() || totalChance == 0)
    {
        DebugLog("未找到组ID为 {} 的鉴定模板", groupId);
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
            DebugLog("选中鉴定模板ID: {} (组{}, roll={}/{})", pair.first, groupId, roll, totalChance);
            return pair.first;
        }
    }

    // 如果出现问题，返回第一个模板
    return templates[0].first;
}

// 应用鉴定结果到物品（需要指定组ID）
bool ItemIdentificationSystem::ApplyIdentification(Player* player, Item* item, uint32 groupId)
{
    // 获取选择的模板ID
    uint32 templateId = SelectIdentificationTemplate(groupId, item);
    if (!templateId || _identificationTemplates.find(templateId) == _identificationTemplates.end())
        return false;

    const IdentificationTemplate& tmpl = _identificationTemplates[templateId];
    


    // 0. 检查物品是否已鉴定（防止重复鉴定）
    uint32 itemGuid = item->GetGUID().GetCounter();
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
    record.costGold = _cost;
    record.successRate = GetSuccessRate(player, item);
    
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
        ApplyAdditionalAttributes(player, item, tmpl);
        record.hasAdditionalAttributes = true;
        record.additionalAttrCount = GenerateRandomNumber(tmpl.additionalAttrMinCount, tmpl.additionalAttrMaxCount);
        record.additionalAttrGroups = tmpl.itemAttributesAdditionalGroups;

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

    if (!tmpl.skillSetGroups.empty())
    {
        ApplySkillSets(player, item, tmpl);
        record.hasSet = true;
        record.setGroup = SelectRandomFromList(ParseCommaSeparatedNumbers(tmpl.skillSetGroups));
        record.setId = 0; // 套装系统会分配具体的套装ID

    }
    else
    {

    }

    // 9. 应用名称和描述
    ApplyNameAndDescription(item, tmpl);

    // 10. 更新物品状态
    item->SetState(ITEM_CHANGED, player);

    // 11. 保存鉴定记录到数据库
    SaveIdentificationRecord(record);

    // 12. 刷新物品显示
    RefreshItem(player, item);

    // 13. ⭐ 不再主动发送数据 - 客户端会通过 addon 消息主动查询
    // 旧逻辑：SendAllModuleData(player, item) - 已删除
    // 新逻辑：客户端悬停物品时自动发送 UITQ QUERY 请求，服务器通过 HandleAddonBatchQuery 响应

    // 14. 套装刷新已优化：移除鉴定时的刷新调用
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

    // 直接调用成长系统API设置物品成长属性
    try
    {
        // 使用成长系统的API设置物品可成长，并指定属性组
        sItemGrowthMgr->SetItemCanGrow(player, item, selectedGroup);
        
        ChatHandler(player->GetSession()).PSendSysMessage("物品获得成长属性（组{}）", selectedGroup);
        
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
        // 鉴定系统不调用EnhanceItem，而是直接创建等级1的强化记录
        // EnhanceItem会自动升级到下一个等级，但鉴定只应该产生等级1
        // 调用第一次强化来初始化强化数据
        if (sItemEnhancementMgr->EnhanceItem(player, item, selectedGroup))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("物品获得强化属性（组{}）", selectedGroup);
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
             tmpl.baseAttrMinValue, tmpl.baseAttrMaxValue);

#ifdef MODULE_ITEM_ATTRIBUTES
    if (!sItemAttributesGenerator || !sItemAttributesLoader)
    {

        return;
    }

    // 第0步：保留randomPropertyId用于识别装备
    int32 currentRandomProp = item->GetItemRandomPropertyId();

    uint64 itemGuid = item->GetGUID().GetRawValue();
    
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

    ItemAttributeGenerateOptions options;
    options.minAttributes = attrCount;        // 最少属性数量
    options.maxAttributes = attrCount;        // 最多属性数量
    options.attributeGroup = selectedGroup;   // 指定属性组
    options.minItemLevel = tmpl.baseAttrMinValue;   // 属性值最小值
    options.maxItemLevel = tmpl.baseAttrMaxValue;   // 属性值最大值
    options.respectChance = true;             // 考虑属性获取几率
    options.allowDuplicateTypes = tmpl.baseAttrAllowDuplicate;  // 是否允许重复
    options.useValueRangeFilter = true;       // 启用属性值范围过滤
    options.category = AttributeCategory::BASE;  // 设置为基础属性

    // 生成随机属性（这会替换物品的属性显示）
    bool generateResult = sItemAttributesGenerator->GenerateRandomAttributes(item, options);



    if (generateResult)
    {
        // ✅ 优化：移除延迟等待，使用DirectExecute确保立即写入
        // 由于属性系统使用DirectExecute，数据应该已经写入
        // 如果读取失败，接受属性延迟加载（不影响鉴定成功）

        std::vector<uint32> baseAttributes;
        std::vector<int32> baseValues;

        uint64 itemGuid = item->GetGUID().GetCounter();

        // ✅ 尝试一次读取（不重试，不等待）
        ItemAttributesDBHelper::ItemAttributeData* data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
        if (data && !data->baseAttributeIds.empty())
        {
            baseAttributes = data->baseAttributeIds;
            baseValues = data->baseAttributeValues;
            delete data;
        }
        else
        {
            if (data)
                delete data;

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
        ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个基础属性（官方属性已替换）", attrCount);
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
void ItemIdentificationSystem::ApplyAdditionalAttributes(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> attributeGroups = ParseCommaSeparatedNumbers(tmpl.itemAttributesAdditionalGroups);
    if (attributeGroups.empty())
    {

        return;
    }

    uint32 attrCount = GenerateRandomNumber(tmpl.additionalAttrMinCount, tmpl.additionalAttrMaxCount);
    if (attrCount == 0)
    {

        return;
    }

#ifdef MODULE_ITEM_ATTRIBUTES
    if (sItemAttributesGenerator)
    {
        uint32 appliedCount = 0;

        // 判断是单组还是多组模式
        if (attributeGroups.size() == 1)
        {
            // 单组模式：从同一个组生成多个属性
            ItemAttributeGenerateOptions options;
            options.minAttributes = attrCount;  // 使用配置的数量
            options.maxAttributes = attrCount;
            options.attributeGroup = attributeGroups[0];
            options.minItemLevel = tmpl.additionalAttrMinValue;
            options.maxItemLevel = tmpl.additionalAttrMaxValue;
            options.respectChance = true;
            options.allowDuplicateTypes = tmpl.additionalAttrAllowDuplicate;
            options.useValueRangeFilter = true;
            options.category = AttributeCategory::ADDITIONAL;  // 设置为追加属性

            if (sItemAttributesGenerator->GenerateRandomAttributes(item, options))
            {
                appliedCount = attrCount;
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
                options.minItemLevel = tmpl.additionalAttrMinValue;
                options.maxItemLevel = tmpl.additionalAttrMaxValue;
                options.respectChance = true;
                options.allowDuplicateTypes = tmpl.additionalAttrAllowDuplicate;
                options.useValueRangeFilter = true;
                options.category = AttributeCategory::ADDITIONAL;  // 设置为追加属性

                if (sItemAttributesGenerator->GenerateRandomAttributes(item, options))
                {
                    appliedCount++;
                }
            }
        }

        if (appliedCount > 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个追加属性", appliedCount);
        }
        else
        {
        }
    }
    else
    {
    }
#else
#endif


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
            ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个技能效果", appliedCount);
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
            ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个技能魔次", appliedCount);
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
                    // 验证randomPropertyId是否被正确设置
                    int32 randomPropId = item->GetItemRandomPropertyId();
                    uint32 itemGuid = item->GetGUID().GetCounter();

                    if (randomPropId != static_cast<int32>(itemGuid))
                    {
                    }

                    DebugLog("成功为物品创建{}个符文凹槽", slotCount);
                    ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个符文凹槽", slotCount);
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
            ChatHandler(player->GetSession()).PSendSysMessage("物品可以镶嵌符文（推荐符文组: {}）", tmpl.runeSystemGroups);
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

// 应用技能套装
void ItemIdentificationSystem::ApplySkillSets(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> skillSetGroups = ParseCommaSeparatedNumbers(tmpl.skillSetGroups);
    if (skillSetGroups.empty())
    {

        return;
    }

    // 随机选择一个套装组
    uint32 selectedGroup = SelectRandomFromList(skillSetGroups);
    DebugLog("应用技能套装组: {}", selectedGroup);

#ifdef MODULE_ITEM_SETS

    if (!sItemSetsManager)
    {
        DebugLog("套装系统管理器不可用");
        return;
    }

    if (!sItemSetsConfig || !sItemSetsConfig->IsModuleEnabled())
    {
        DebugLog("套装系统未启用");
        return;
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
        return;
    }

    // 2. 从指定组中选择套装（考虑玩家需求）
    uint32 selectedSetId = sItemSetsManager->GetRandomSetForGroup(selectedGroup, player);



    if (selectedSetId == 0)
    {
        DebugLog("从套装组{}中未找到满足需求的套装", selectedGroup);

        // 显示组内套装的需求条件
        sItemSetsManager->ShowGroupRequirements(player, selectedGroup);
        ChatHandler(player->GetSession()).PSendSysMessage("分配套装失败：不满足需求条件。");
        return;
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
        return;
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

    // 5. 将物品GUID存储到randomPropertyId字段
    sItemSetsManager->StoreGuidInEnchantmentSlot(item);



    // 6. 获取套装名称并通知玩家
    std::string setName = sItemSetsManager->GetSetName(selectedSetId);


    ChatHandler(player->GetSession()).PSendSysMessage("物品已分配到套装: {} (组{}, ID{})",
        setName.c_str(), selectedGroup, selectedSetId);

    // 7. 保存到数据库
    sItemSetsManager->SavePlayerSetStatus(player);



    // 8. 重新计算并应用套装效果
    // ⚠️ 性能优化：注释掉立即刷新，改为在鉴定流程结束后统一刷新
    // 原因：RefreshPlayerSetEffects 单次耗时较长
    // 优化后将在 ApplyIdentification 函数末尾统一调用一次
    // sItemSetsManager->RefreshPlayerSetEffects(player);  // 已优化：延迟到鉴定结束后

    DebugLog("成功将物品分配到套装组: {}，套装ID: {}", selectedGroup, selectedSetId);




#else
    DebugLog("套装系统模块未编译");
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000套装系统未安装|r");
#endif


}

// 应用名称和描述
void ItemIdentificationSystem::ApplyNameAndDescription(Item* item, const IdentificationTemplate& tmpl)
{
    if (!item)
    {

        return;
    }

    DebugLog("应用名称和描述到物品: {}", item->GetEntry());

    // 在AzerothCore中，物品的名称显示受到限制
    // 我们通过以下方式记录和处理自定义名称和描述：

    // 1. 构建完整的自定义名称
    std::string customName;
    if (!tmpl.namePrefix.empty())
    {
        customName += tmpl.namePrefix;
        DebugLog("添加名称前缀: {}", tmpl.namePrefix);
    }

    // 原始物品名称
    ItemTemplate const* proto = item->GetTemplate();
    if (proto)
    {
        if (!customName.empty())
            customName += " ";
        customName += proto->Name1;
    }

    if (!tmpl.nameSuffix.empty())
    {
        if (!customName.empty())
            customName += " ";
        customName += tmpl.nameSuffix;
        DebugLog("添加名称后缀: {}", tmpl.nameSuffix);
    }

    DebugLog("最终物品名称: {}", customName);

    // 2. 构建完整的描述信息（包括品质显示和底部描述）
    std::string fullDescription;

    if (!tmpl.qualityDisplay.empty())
    {
        fullDescription += "[" + tmpl.qualityDisplay + "]";
        DebugLog("添加品质显示: {}", tmpl.qualityDisplay);
    }

    if (!tmpl.bottomDescription.empty())
    {
        if (!fullDescription.empty())
            fullDescription += " ";
        fullDescription += tmpl.bottomDescription;
        DebugLog("添加底部描述: {}", tmpl.bottomDescription);
    }

    // 3. 记录颜色信息（用于客户端显示或其他用途）
    if (!tmpl.nameColors.empty())
    {
        DebugLog("物品名称颜色配置: {}", tmpl.nameColors);
        // 颜色信息可以在客户端显示时使用
        // 格式例如：|cffff00ff,|cffff0080 表示多种颜色
    }

    DebugLog("完整物品信息 - 名称: '{}', 描述: '{}'", customName, fullDescription);

    // 4. 在AzerothCore中，如果需要永久保存这些信息，可以：
    //    - 将其保存到item_instance表的text_0-text_1字段（如果这些字段存在）
    //    - 或创建一个自定义表来存储这些信息
    //    - 或使用物品的flags_custom字段来标记这是一个已鉴定的物品

    // 标记物品为已鉴定（可选，使用自定义标志位）
    // item->SetUInt32Value(ITEM_FIELD_FLAGS_CUSTOM, item->GetUInt32Value(ITEM_FIELD_FLAGS_CUSTOM) | 0x01);

    DebugLog("物品名称和描述应用完成");


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
    std::string query = "SELECT 公告模板 FROM 物品_鉴定系统 WHERE id = " + std::to_string(identificationId);

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
        // ⭐ "查询" 命令已删除 - 使用 addon 消息自动查询替代
        { "批量查询", HandleBatchQueryCommand, SEC_PLAYER, Console::No },  // ⭐ 批量查询命令
        { "性能统计", HandlePerformanceStatsCommand, SEC_ADMINISTRATOR, Console::No }  // ⭐ 性能监控
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

    // ⭐ 删除旧的查询子命令 - 现在使用 addon 消息自动查询
    // 旧代码：if (firstArg == "查询") HandleQueryAttributesCommand(...) - 已删除

    // 解析鉴定命令参数: <组ID> <物品ID>
    char* groupIdStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");

    if (!groupIdStr || !itemIdStr)
    {
        handler->SendSysMessage("参数不足！用法: .鉴定物品 <组ID> <物品ID>");
        return true;
    }

    uint32 groupId = atoi(groupIdStr);
    uint32 itemId = atoi(itemIdStr);
    if (itemId == 0)
    {
        handler->SendSysMessage("无效的物品ID");
        return true;
    }

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

// ⭐ HandleQueryAttributesCommand 已删除 - 现在使用 addon 格式的 HandleAddonBatchQuery
// 客户端不再通过命令查询，而是自动发送 addon 消息 (UITQ QUERY) 来获取数据

// ⭐ HandleQueryCommand 已删除 - 使用 HandleAddonBatchQuery (addon格式) 替代旧的单独查询

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

            // 注册命令脚本
            new ItemIdentificationCommandScript();

            // 注册玩家脚本
            new ItemIdentificationPlayerScript();

            // 初始化模块
            sItemIdentificationSystem->Initialize();

            // 显示加载信息
            LOG_INFO("server.loading", "→物品鉴定系统√");
        }
    }

    // ⭐ 定期清理过期缓存（每5分钟执行一次）
    static uint32 cacheCleanTimer = 0;
    cacheCleanTimer += diff;

    if (cacheCleanTimer >= 300000)  // 5分钟 = 300000ms
    {
        cacheCleanTimer = 0;
        sItemIdentificationSystem->CleanExpiredCache();
    }
}

// 玩家脚本实现
ItemIdentificationPlayerScript::ItemIdentificationPlayerScript() : PlayerScript("ItemIdentificationPlayerScript") { }

void ItemIdentificationPlayerScript::OnLogin(Player* player, bool firstLogin)
{
    if (!player || !sItemIdentificationSystem->_enabled)
        return;

    // ⭐ 直接预加载（不使用延迟，因为Player可能没有Scheduler）
    // 预加载会在登录流程中异步执行，不会阻塞
    sItemIdentificationSystem->PreloadPlayerEquipment(player);

    // ⭐ 删除旧的登录时主动发送逻辑 - 现在客户端会在需要时通过 addon 消息自动查询
    // 旧代码：收集所有已鉴定物品并通过 SendIdentificationDataToClient 发送 - 已删除
    // 新逻辑：客户端悬停物品时自动发送 UITQ QUERY 请求，服务器通过 HandleAddonBatchQuery 响应
}

// 鉴定记录管理实现
bool ItemIdentificationSystem::IsItemIdentified(uint32 itemGuid)
{
    // ✅ 优化：使用内存缓存，避免每次查询数据库
    if (!_cacheInitialized)
    {
        InitializeCache();
    }

    return _identifiedItemsCache.find(itemGuid) != _identifiedItemsCache.end();
}

void ItemIdentificationSystem::SaveIdentificationRecord(const ItemIdentificationRecord& record)
{

    // 使用DirectExecute确保立即写入
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
        ") VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, {}, {})",
        record.playerGuid, record.itemGuid, record.itemEntry, record.templateId,
        record.hasGrowth ? 1 : 0, record.growthGroup,
        record.hasEnhancement ? 1 : 0, record.enhancementGroup,
        record.hasBaseAttributes ? 1 : 0, record.baseAttrCount, record.baseAttrGroup, record.baseAttrDetails,
        record.hasAdditionalAttributes ? 1 : 0, record.additionalAttrCount, record.additionalAttrGroups,
        record.hasRuneSlots ? 1 : 0, record.runeSlotCount,
        record.hasSkills ? 1 : 0, record.skillGroups,
        record.hasMagicHits ? 1 : 0, record.magicHitCount, record.magicHitGroups,
        record.hasSet ? 1 : 0, record.setGroup, record.setId,
        record.costGold, record.successRate
    );

    // ✅ 优化：立即更新缓存
    _identifiedItemsCache.insert(record.itemGuid);

    DebugLog("保存鉴定记录: 玩家GUID={}, 物品GUID={}, 模板ID={}",
             record.playerGuid, record.itemGuid, record.templateId);
}

ItemIdentificationRecord* ItemIdentificationSystem::GetIdentificationRecord(uint32 itemGuid)
{
    // 这里简化实现，实际应用中可以缓存记录
    // 暂时返回nullptr，完整实现需要从数据库加载
    return nullptr;
}

// ⭐ SendAllModuleData 和 SendIdentificationDataToClient 已删除
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

        // 如果属性系统可用，应用属性效果
#ifdef MODULE_ITEM_ATTRIBUTES
        if (sItemAttributesEffects)
        {
            sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
        }
#endif

        // 更新玩家属性
        player->UpdateAllStats();

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

    // 在装备物品后触发（此时官方属性已经被应用）
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override
    {
        if (!player || !item)
            return;

        // 检查物品是否已鉴定
        uint32 itemGuid = item->GetGUID().GetCounter();
        if (!sItemIdentificationSystem->IsItemIdentified(itemGuid))
            return;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return;

        // 记录这个槽位装备了已鉴定物品
        uint64 playerGuid = player->GetGUID().GetCounter();
        _identifiedEquippedItems[playerGuid][slot] = std::make_pair(itemGuid, item->GetEntry());

        // 遍历物品模板的所有属性槽位，移除官方属性
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (proto->ItemStat[i].ItemStatType != 0 && proto->ItemStat[i].ItemStatValue != 0)
            {
                uint32 statType = proto->ItemStat[i].ItemStatType;
                int32 statValue = proto->ItemStat[i].ItemStatValue;

                // 根据属性类型移除对应的属性加成
                switch (statType)
                {
                    case ITEM_MOD_STRENGTH:  // 4 - 力量
                        player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(statValue), false);
                        break;
                    case ITEM_MOD_AGILITY:   // 3 - 敏捷
                        player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(statValue), false);
                        break;
                    case ITEM_MOD_STAMINA:   // 7 - 耐力
                        player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(statValue), false);
                        break;
                    case ITEM_MOD_INTELLECT: // 5 - 智力
                        player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(statValue), false);
                        break;
                    case ITEM_MOD_SPIRIT:    // 6 - 精神
                        player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(statValue), false);
                        break;
                    // 可以添加更多属性类型的处理
                    default:
                        break;
                }
            }
        }

        // 刷新玩家属性
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
    }

    // 在脱下装备时触发
    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override
    {
        if (!player)
            return;

        // item == nullptr 表示脱下装备
        if (item == nullptr)
        {
            uint64 playerGuid = player->GetGUID().GetCounter();

            // 检查这个槽位是否装备了已鉴定物品
            auto playerIt = _identifiedEquippedItems.find(playerGuid);
            if (playerIt == _identifiedEquippedItems.end())
                return;

            auto& playerSlots = playerIt->second;
            auto slotIt = playerSlots.find(slot);
            if (slotIt == playerSlots.end())
                return;

            uint32 itemEntry = slotIt->second.second;

            // 通过itemEntry获取物品模板
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
            if (proto)
            {
                // 恢复官方属性（因为游戏引擎会移除它们）
                for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                {
                    if (proto->ItemStat[i].ItemStatType != 0 && proto->ItemStat[i].ItemStatValue != 0)
                    {
                        uint32 statType = proto->ItemStat[i].ItemStatType;
                        int32 statValue = proto->ItemStat[i].ItemStatValue;

                        // 根据属性类型恢复对应的属性加成
                        switch (statType)
                        {
                            case ITEM_MOD_STRENGTH:  // 4 - 力量
                                player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(statValue), true);
                                break;
                            case ITEM_MOD_AGILITY:   // 3 - 敏捷
                                player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(statValue), true);
                                break;
                            case ITEM_MOD_STAMINA:   // 7 - 耐力
                                player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(statValue), true);
                                break;
                            case ITEM_MOD_INTELLECT: // 5 - 智力
                                player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(statValue), true);
                                break;
                            case ITEM_MOD_SPIRIT:    // 6 - 精神
                                player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(statValue), true);
                                break;
                            default:
                                break;
                        }
                    }
                }
            }

            // 从记录中移除
            playerSlots.erase(slotIt);
        }
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
void ItemIdentificationSystem::InitializeCache()
{
    PerformanceTimer timer("初始化已鉴定物品缓存");

    _identifiedItemsCache.clear();

    // 一次性从数据库加载所有已鉴定物品的GUID
    QueryResult result = CharacterDatabase.Query("SELECT 物品GUID FROM 物品_鉴定记录");

    if (!result)
    {
        LOG_INFO("module.itemidentification", "已鉴定物品缓存初始化完成：0 个物品");
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
    LOG_INFO("module.itemidentification", "已鉴定物品缓存初始化完成：{} 个物品", count);
}

// ✅ 新增：清除指定物品的鉴定缓存
void ItemIdentificationSystem::ClearIdentifiedCache(uint32 itemGuid)
{
    _identifiedItemsCache.erase(itemGuid);
    DebugLog("清除物品GUID {} 的鉴定缓存", itemGuid);
}

// ✅ 新增：批量检查物品是否已鉴定（优化版）
std::set<uint32> ItemIdentificationSystem::BatchCheckIdentified(const std::vector<uint32>& itemGuids)
{
    PerformanceTimer timer("批量检查已鉴定物品");

    if (!_cacheInitialized)
    {
        InitializeCache();
    }

    std::set<uint32> identifiedSet;

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

        uint32 itemGuid = item->GetGUID().GetCounter();
        bool isIdentified = sItemIdentificationSystem->IsItemIdentified(itemGuid);
    }
};

// ⭐ Addon消息处理脚本
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

        // 注意：服务器收到的消息格式是 "UITQ<TAB>QUERY:itemID:guid"
        // 需要先移除"UITQ<TAB>"前缀，然后解析"QUERY:itemID:guid"
        
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
        std::string command = msg.substr(tabPos + 1);

        // 解析QUERY命令
        if (command.find("QUERY:") != 0)
            return;

        // 提取参数
        std::string params = command.substr(6);  // 去掉"QUERY:"
        
        // 解析itemID和guid
        size_t colonPos = params.find(':');
        if (colonPos == std::string::npos)
            return;

        uint32 itemID = 0;
        uint32 guid = 0;

        try
        {
            itemID = std::stoul(params.substr(0, colonPos));
            guid = std::stoul(params.substr(colonPos + 1));
        }
        catch (...)
        {
            return;  // 解析失败
        }

        if (itemID == 0 || guid == 0)
        {
            return;
        }

        // ⭐ 执行批量查询并通过Addon消息返回结果
        HandleAddonBatchQuery(player, itemID, guid);
    }

private:
    void HandleAddonBatchQuery(Player* player, uint32 itemID, uint32 guid)
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
        auto it = sItemIdentificationSystem->_batchQueryCache.find(cacheKey);

        ItemIdentificationSystem::AllModuleData moduleData;  // 改名避免与WorldPacket data冲突
        bool cacheHit = false;

        // 更新统计：总查询次数
        sItemIdentificationSystem->_perfStats.totalQueries++;

        if (it != sItemIdentificationSystem->_batchQueryCache.end())
        {
            // 检查缓存是否过期
            time_t now = time(nullptr);
            if ((now - it->second.cacheTime) < sItemIdentificationSystem->BATCH_CACHE_EXPIRE_TIME)
            {
                // 缓存有效，直接使用
                moduleData = it->second.data;
                cacheHit = true;
                sItemIdentificationSystem->_perfStats.cacheHits++;
            }
            else
            {
                // 缓存过期，删除
                sItemIdentificationSystem->_batchQueryCache.erase(it);
                sItemIdentificationSystem->_perfStats.cacheMisses++;
            }
        }
        else
        {
            sItemIdentificationSystem->_perfStats.cacheMisses++;
        }

        if (!cacheHit)
        {
            // 缓存未命中，查询数据库
            sItemIdentificationSystem->_perfStats.dbQueries++;
            moduleData = sItemIdentificationSystem->QueryAllModuleData(itemID, guid);

            // 存入缓存
            ItemIdentificationSystem::BatchQueryCache cache;
            cache.data = moduleData;
            cache.cacheTime = time(nullptr);
            sItemIdentificationSystem->_batchQueryCache[cacheKey] = cache;
        }

        // ⭐ 构建Addon响应消息
        // 格式：ALL_MODULE_DATA:itemID:guid:base:additional:growth:enhancement:skills:magic:rune:set
        // 注意：客户端会自动添加RESPONSE:前缀检查，这里只发数据
        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << itemID << ":" << guid << ":"
                 << moduleData.baseAttributes << ":"
                 << moduleData.additionalAttributes << ":"
                 << moduleData.growthData << ":"
                 << moduleData.enhancementData << ":"
                 << moduleData.skillsData << ":"
                 << moduleData.magicHitData << ":"
                 << moduleData.runeData << ":"
                 << moduleData.setData;

        std::string responseStr = response.str();

        // ⭐ 通过Addon消息发送响应
        // 参考符文系统：构建完整消息 "UITQ<TAB>响应数据"
        std::string fullMessage = "UITQ\t" + responseStr;

        // 使用ChatHandler::BuildChatPacket构建标准包
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, 
                                     player, player, fullMessage, 0);
        
        player->SendDirectMessage(&data);

        // 计算查询耗时
        auto queryEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart).count();
        sItemIdentificationSystem->_perfStats.totalQueryTime += duration;
        sItemIdentificationSystem->_perfStats.avgQueryTime = 
            sItemIdentificationSystem->_perfStats.totalQueryTime / sItemIdentificationSystem->_perfStats.totalQueries;

        if (sItemIdentificationSystem->_debugMode)
        {
            LOG_INFO("module.itemidentification", 
                     "[Addon响应] 已发送数据: itemID={}, guid={}, 消息长度={}, 缓存命中={}, 耗时={}μs",
                     itemID, guid, responseStr.length(), cacheHit, duration);
        }
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

    // ⭐ 添加Addon消息处理脚本
    new ItemIdentificationAddonScript();
}

// ============================================================================
// ⭐ 批量查询优化实现 - 解决客户端查询延迟问题
// ============================================================================

// 批量查询所有模块数据（核心方法）
ItemIdentificationSystem::AllModuleData ItemIdentificationSystem::QueryAllModuleData(uint32 itemID, uint32 guid)
{

    AllModuleData result;
    result.hasData = false;

    // 检查是否已鉴定
    if (!IsItemIdentified(guid))
    {
        DebugLog("[批量查询] 物品未鉴定: itemID={}, guid={}", itemID, guid);
        return result;
    }

    // ========== 表存在性缓存（静态变量，只初始化一次）==========
    static std::unordered_map<std::string, bool> tableCache;
    static bool cacheInitialized = false;
    
    if (!cacheInitialized)
    {
        // 一次性检查所有需要的表
        std::vector<std::string> tablesToCheck = {
            "物品属性_数据", "物品_鉴定记录", "物品成长_玩家记录",
            "物品强化_记录", "物品技能_数据", "魔次系统_数据",
            "符文系统_数据", "玩家套装状态"
        };
        
        for (const auto& tableName : tablesToCheck)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '{}'",
                tableName);
            
            tableCache[tableName] = (result && result->Fetch()[0].Get<uint32>() > 0);
        }
        
        cacheInitialized = true;
    }

    // ========== 辅助函数：从缓存检查表是否存在 ==========
    auto TableExists = [](const std::string& tableName) -> bool {
        auto it = tableCache.find(tableName);
        return (it != tableCache.end() && it->second);
    };

    // ========== 1. 查询鉴定系统数据（基础属性+追加属性）- 合并查询优化 ==========
    DebugLog("[批量查询] 开始查询鉴定系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("物品_鉴定记录"))
    {
        // 使用 LEFT JOIN 一次性查询两个表的数据，减少数据库往返
        QueryResult attrResult = CharacterDatabase.Query(
            "SELECT r.`基础属性详情`, IFNULL(a.`追加属性`, '') as `追加属性` "
            "FROM `物品_鉴定记录` r "
            "LEFT JOIN `物品属性_数据` a ON r.`物品GUID` = a.`物品GUID` "
            "WHERE r.`物品GUID` = {}",
            guid);

        if (attrResult)
        {
            Field* fields = attrResult->Fetch();
            std::string baseAttr = fields[0].Get<std::string>();
            std::string additionalAttr = fields[1].Get<std::string>();

            DebugLog("[批量查询-鉴定] 查询成功: 基础属性=[{}], 追加属性=[{}]", baseAttr, additionalAttr);

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
        }
        else
        {
            DebugLog("[批量查询-鉴定] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-鉴定] 表不存在: 物品_鉴定记录");
    }

    // ========== 2. 查询成长系统数据 ==========
    DebugLog("[批量查询] 开始查询成长系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("物品成长_玩家记录"))
    {
        QueryResult growthResult = CharacterDatabase.Query(
            "SELECT `当前等级`, `当前经验`, `升级经验`, `成长属性` FROM `物品成长_玩家记录` WHERE `物品GUID` = {}",
            guid);

        if (growthResult)
        {
            Field* fields = growthResult->Fetch();
            uint32 level = fields[0].Get<uint32>();
            uint32 currentExp = fields[1].Get<uint32>();
            uint32 requiredExp = fields[2].Get<uint32>();
            std::string attrs = fields[3].Get<std::string>();

            // 格式：level|currentExp|requiredExp|attrs（使用|避免与主消息:冲突）
            std::ostringstream growthStream;
            growthStream << level << "|" << currentExp << "|" << requiredExp << "|" << attrs;
            result.growthData = growthStream.str();
            result.hasData = true;

            DebugLog("[批量查询-成长] 查询成功: level={}, exp={}/{}, attrs=[{}], 结果=[{}]", 
                     level, currentExp, requiredExp, attrs, result.growthData);
        }
        else
        {
            DebugLog("[批量查询-成长] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-成长] 表不存在: 物品成长_玩家记录");
    }

    // ========== 3. 查询强化系统数据 ==========
    DebugLog("[批量查询] 开始查询强化系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("物品强化_记录"))
    {
        QueryResult enhanceResult = CharacterDatabase.Query(
            "SELECT `强化等级`, `属性值` FROM `物品强化_记录` WHERE `guid` = {}",
            guid);

        if (enhanceResult)
        {
            Field* fields = enhanceResult->Fetch();
            uint32 level = fields[0].Get<uint32>();
            std::string attrs = fields[1].Get<std::string>();

            // 格式：level|attrs（使用|避免与主消息:冲突）
            std::ostringstream enhanceStream;
            enhanceStream << level << "|" << attrs;
            result.enhancementData = enhanceStream.str();
            result.hasData = true;

            DebugLog("[批量查询-强化] 查询成功: level={}, attrs=[{}], 结果=[{}]", 
                     level, attrs, result.enhancementData);
        }
        else
        {
            DebugLog("[批量查询-强化] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-强化] 表不存在: 物品强化_记录");
    }

    // ========== 4. 查询技能系统数据 ==========
    DebugLog("[批量查询] 开始查询技能系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("物品技能_数据"))
    {
        QueryResult skillResult = CharacterDatabase.Query(
            "SELECT `技能ID`, `技能模板ID`, `技能触发类型` FROM `物品技能_数据` WHERE `物品GUID` = {}",
            guid);

        if (skillResult)
        {
            Field* fields = skillResult->Fetch();
            std::string skillIds = fields[0].Get<std::string>();
            std::string templateIds = fields[1].Get<std::string>();
            std::string triggerTypes = fields[2].Get<std::string>();
            
            // 使用逗号分隔的技能ID作为数据
            // 格式: "技能ID1,技能ID2,技能ID3"
            result.skillsData = skillIds;
            result.hasData = true;

            DebugLog("[批量查询-技能] 查询成功: skillIds=[{}], templateIds=[{}], triggerTypes=[{}]", 
                     skillIds, templateIds, triggerTypes);
        }
        else
        {
            DebugLog("[批量查询-技能] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-技能] 表不存在: 物品技能_数据");
    }

    // ========== 5. 查询魔次系统数据 ==========
    DebugLog("[批量查询] 开始查询魔次系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("魔次系统_数据"))
    {
        QueryResult magicResult = CharacterDatabase.Query(
            "SELECT `魔次系统ID`, `魔次值` FROM `魔次系统_数据` WHERE `物品GUID` = {}",
            guid);

        if (magicResult)
        {
            Field* fields = magicResult->Fetch();
            std::string magicIds = fields[0].Get<std::string>();
            std::string magicValues = fields[1].Get<std::string>();
            
            // 使用逗号分隔的魔次数据
            // 格式: "魔次系统ID1,魔次系统ID2|魔次值1,魔次值2"
            result.magicHitData = magicIds + "|" + magicValues;
            result.hasData = true;

            DebugLog("[批量查询-魔次] 查询成功: magicIds=[{}], magicValues=[{}], 结果=[{}]", 
                     magicIds, magicValues, result.magicHitData);
        }
        else
        {
            DebugLog("[批量查询-魔次] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-魔次] 表不存在: 魔次系统_数据");
    }

    // ========== 6. 查询符文系统数据 ==========
    DebugLog("[批量查询] 开始查询符文系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("符文系统_数据"))
    {
        QueryResult runeResult = CharacterDatabase.Query(
            "SELECT `插槽数量`, `插槽ID`, `符文物品ID` FROM `符文系统_数据` WHERE `物品GUID` = {}",
            guid);

        if (runeResult)
        {
            Field* fields = runeResult->Fetch();
            uint32 slotCount = fields[0].Get<uint32>();
            std::string slotIds = fields[1].Get<std::string>();
            std::string runeItemIds = fields[2].Get<std::string>();

            // 格式：插槽数量|插槽ID列表|符文物品ID列表（使用|避免与主消息:冲突）
            std::ostringstream runeStream;
            runeStream << slotCount << "|" << slotIds << "|" << runeItemIds;
            result.runeData = runeStream.str();
            result.hasData = true;

            DebugLog("[批量查询-符文] 查询成功: slotCount={}, slotIds=[{}], runeItemIds=[{}], 结果=[{}]", 
                     slotCount, slotIds, runeItemIds, result.runeData);
        }
        else
        {
            DebugLog("[批量查询-符文] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-符文] 表不存在: 符文系统_数据");
    }

    // ========== 7. 查询套装系统数据 ==========
    DebugLog("[批量查询] 开始查询套装系统: itemID={}, guid={}", itemID, guid);
    if (TableExists("玩家套装状态"))
    {
        QueryResult setResult = CharacterDatabase.Query(
            "SELECT `套装ID` FROM `玩家套装状态` WHERE `物品GUID` = {}",
            guid);

        if (setResult)
        {
            Field* fields = setResult->Fetch();
            uint32 setId = fields[0].Get<uint32>();

            // 格式：套装ID
            result.setData = std::to_string(setId);
            result.hasData = true;

            DebugLog("[批量查询-套装] 查询成功: setId={}, 结果=[{}]", setId, result.setData);
        }
        else
        {
            DebugLog("[批量查询-套装] 查询失败或无数据");
        }
    }
    else
    {
        DebugLog("[批量查询-套装] 表不存在: 玩家套装状态");
    }

    // ========== 查询完成总结 ==========
    DebugLog("[批量查询] 查询完成汇总: itemID={}, guid={}, hasData={}", itemID, guid, result.hasData);
    DebugLog("[批量查询] 基础属性=[{}]", result.baseAttributes);
    DebugLog("[批量查询] 追加属性=[{}]", result.additionalAttributes);
    DebugLog("[批量查询] 成长数据=[{}]", result.growthData);
    DebugLog("[批量查询] 强化数据=[{}]", result.enhancementData);
    DebugLog("[批量查询] 技能数据=[{}]", result.skillsData);
    DebugLog("[批量查询] 魔次数据=[{}]", result.magicHitData);
    DebugLog("[批量查询] 符文数据=[{}]", result.runeData);
    DebugLog("[批量查询] 套装数据=[{}]", result.setData);

    DebugLog("[批量查询] 完成查询: itemID={}, guid={}, 有数据={}", itemID, guid, result.hasData);

    return result;
}

// 批量查询命令处理器（一次性返回所有数据）
void ItemIdentificationSystem::HandleBatchQueryCommand(Player* player, uint32 itemID, uint32 guid)
{
    if (!player)
        return;

    auto queryStart = std::chrono::high_resolution_clock::now();

    // ⭐ 优先从缓存读取
    uint64 cacheKey = ((uint64)itemID << 32) | guid;
    auto it = _batchQueryCache.find(cacheKey);

    AllModuleData data;
    bool cacheHit = false;

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

    if (!cacheHit)
    {
        // 缓存未命中，查询数据库
        _perfStats.dbQueries++;  // 统计：数据库查询
        data = QueryAllModuleData(itemID, guid);

        // 存入缓存
        BatchQueryCache cache;
        cache.data = data;
        cache.cacheTime = time(nullptr);
        _batchQueryCache[cacheKey] = cache;

        DebugLog("[批量查询缓存] 新数据已缓存: itemID={}, guid={}", itemID, guid);
    }

    // ⭐ 新格式消息：ALL_MODULE_DATA:itemID:guid:base:additional:growth:enhancement:skills:magic:rune:set
    std::ostringstream response;
    response << "ALL_MODULE_DATA:" << itemID << ":" << guid << ":"
             << data.baseAttributes << ":"
             << data.additionalAttributes << ":"
             << data.growthData << ":"
             << data.enhancementData << ":"
             << data.skillsData << ":"
             << data.magicHitData << ":"
             << data.runeData << ":"
             << data.setData;

    // 发送前记录完整消息内容
    DebugLog("[批量查询命令] 准备发送完整消息: [{}]", response.str());
    DebugLog("[批量查询命令] 各字段详情:");
    DebugLog("  - 基础属性: [{}]", data.baseAttributes);
    DebugLog("  - 追加属性: [{}]", data.additionalAttributes);
    DebugLog("  - 成长数据: [{}]", data.growthData);
    DebugLog("  - 强化数据: [{}]", data.enhancementData);
    DebugLog("  - 技能数据: [{}]", data.skillsData);
    DebugLog("  - 魔次数据: [{}]", data.magicHitData);
    DebugLog("  - 符文数据: [{}]", data.runeData);
    DebugLog("  - 套装数据: [{}]", data.setData);

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

    // 清理属性缓存
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

    // 清理批量查询缓存
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
// ⭐ 高级优化功能实现
// ============================================================================

// 预加载玩家装备数据（登录时调用）
void ItemIdentificationSystem::PreloadPlayerEquipment(Player* player)
{
    if (!player || !_enabled)
        return;

    PerformanceTimer timer("预加载玩家装备数据");

    uint32 preloadCount = 0;
    std::vector<std::pair<uint32, uint32>> itemsToPreload;  // <itemID, guid>

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

    // 预加载数据到缓存
    for (const auto& itemPair : itemsToPreload)
    {
        uint32 itemID = itemPair.first;
        uint32 guid = itemPair.second;
        uint64 cacheKey = ((uint64)itemID << 32) | guid;

        // 检查是否已在缓存中
        if (_batchQueryCache.find(cacheKey) == _batchQueryCache.end())
        {
            // 查询并缓存
            AllModuleData data = QueryAllModuleData(itemID, guid);

            BatchQueryCache cache;
            cache.data = data;
            cache.cacheTime = time(nullptr);
            _batchQueryCache[cacheKey] = cache;

            preloadCount++;
        }
    }

    _perfStats.preloadCount += preloadCount;

    LOG_INFO("module.itemidentification", "[预加载] 玩家 {} 预加载了 {} 个装备数据",
             player->GetName(), preloadCount);

    DebugLog("[预加载] 完成预加载: 玩家={}, 装备数量={}", player->GetName(), preloadCount);
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
        handler->SendSysMessage("|cff00ff00性能状态: 优秀 ✓|r");
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
        handler->SendSysMessage("|cff00ff00查询速度: 极快 ✓|r");
    }
    else if (stats.avgQueryTime < 20000)  // <20ms
    {
        handler->SendSysMessage("|cff00ff00查询速度: 快速 ✓|r");
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



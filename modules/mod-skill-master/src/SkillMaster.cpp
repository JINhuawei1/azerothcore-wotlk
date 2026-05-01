#include "SkillMaster.h"
#include "Log.h"
#include "Chat.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include <cmath>

// 全局变量
bool skillMasterEnableModule = true;

// 存储当前正在使用技能大师的玩家及其选择的技能类型
std::map<ObjectGuid, uint32> playerSkillMasterSessions;

// 各职业官方训练师ID (从 npc_trainer 表获取)
// 使用引用了完整技能列表的训练师ID
std::map<uint8, uint32> classTrainerIds = {
    {CLASS_WARRIOR,      913},   // 战士训练师 (引用200001+200002)
    {CLASS_PALADIN,      927},   // 圣骑士训练师 (引用200003+200004)
    {CLASS_HUNTER,       987},   // 猎人训练师 (引用200013+200014)
    {CLASS_ROGUE,        917},   // 盗贼训练师 (引用200015+200016)
    {CLASS_PRIEST,       376},   // 牧师训练师 (引用200011+200012)
    {CLASS_DEATH_KNIGHT, 28471}, // 死亡骑士训练师 (引用200019)
    {CLASS_SHAMAN,       986},   // 萨满训练师 (引用200017+200018)
    {CLASS_MAGE,         328},   // 法师训练师 (引用200007+200008)
    {CLASS_WARLOCK,      461},   // 术士训练师 (引用200009+200010)
    {CLASS_DRUID,        3033},  // 德鲁伊训练师 (引用200005+200006)
};

// 武器大师训练师ID列表 (多个武器大师教不同武器)
std::vector<uint32> weaponTrainerIds = {11865, 11866, 11867, 11868, 11869, 11870};

// 骑术训练师ID - 使用引用了完整骑术数据的NPC
// 31238 引用了 -202010(初级/中级), -202011(专家/大师), -202012(寒冷天气飞行)
uint32 ridingTrainerId = 31238;

// SkillMasterCreatureScript 实现
SkillMasterCreatureScript::SkillMasterCreatureScript() : CreatureScript("npc_skill_master") {}

bool SkillMasterCreatureScript::OnGossipHello(Player* player, Creature* creature)
{
    if (!skillMasterEnableModule)
        return false;

    ShowMainMenu(player, creature);
    return true;
}

bool SkillMasterCreatureScript::OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
{
    if (!skillMasterEnableModule)
        return false;

    player->PlayerTalkClass->ClearMenus();

    switch (action)
    {
        case SKILL_MASTER_MENU_CLASS_SKILLS:
            SendTrainerListForType(player, creature, player->getClass());
            break;
        case SKILL_MASTER_MENU_WEAPON_SKILLS:
            SendTrainerListForType(player, creature, SKILL_TYPE_WEAPON);
            break;
        case SKILL_MASTER_MENU_RIDING:
            SendTrainerListForType(player, creature, SKILL_TYPE_RIDING);
            break;
        default:
            ShowMainMenu(player, creature);
            break;
    }

    return true;
}

void SkillMasterCreatureScript::ShowMainMenu(Player* player, Creature* creature)
{
    player->PlayerTalkClass->ClearMenus();

    // 获取玩家职业名称
    const char* className = GetClassName(player->getClass());

    // 添加菜单选项 - 使用通用的技能书图标
    AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
        Acore::StringFormat("|TInterface\\Icons\\INV_Misc_Book_09:30:30|t 学习{}技能", className),
        GOSSIP_SENDER_MAIN, SKILL_MASTER_MENU_CLASS_SKILLS);

    AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
        "|TInterface\\Icons\\INV_Sword_27:30:30|t 学习武器技能",
        GOSSIP_SENDER_MAIN, SKILL_MASTER_MENU_WEAPON_SKILLS);

    AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
        "|TInterface\\Icons\\Ability_Mount_RidingHorse:30:30|t 学习骑术",
        GOSSIP_SENDER_MAIN, SKILL_MASTER_MENU_RIDING);

    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
}

void SkillMasterCreatureScript::SendTrainerListForType(Player* player, Creature* creature, uint32 skillType)
{
    // 不要在这里关闭 Gossip 菜单，让客户端自己处理
    // CloseGossipMenuFor(player);

    std::string trainerName;
    uint32 trainerType = 0; // 默认训练师类型

    // 记录玩家会话，用于后续处理购买请求
    playerSkillMasterSessions[player->GetGUID()] = skillType;

    // 先计算技能数量和收集有效的技能
    std::vector<TrainerSpell const*> validSpells;
    std::set<uint32> addedSpells; // 用于去重
    float fDiscountMod = player->GetReputationPriceDiscount(creature);
    bool canLearnPrimaryProf = player->GetFreePrimaryProfessionPoints() > 0;

    // 武器技能 - 合并所有武器训练师的数据
    if (skillType == SKILL_TYPE_WEAPON)
    {
        trainerName = "武器大师";
        trainerType = 0; // 通用训练师

        for (uint32 trainerId : weaponTrainerIds)
        {
            TrainerSpellData const* trainerSpells = sObjectMgr->GetNpcTrainerSpells(trainerId);
            if (!trainerSpells)
                continue;

            for (TrainerSpellMap::const_iterator itr = trainerSpells->spellList.begin();
                 itr != trainerSpells->spellList.end(); ++itr)
            {
                TrainerSpell const* tSpell = &itr->second;

                // 去重
                if (addedSpells.find(tSpell->spell) != addedSpells.end())
                    continue;

                // 武器技能不检查职业/种族限制，所有职业都可以学习
                // 检查前置技能要求（一般武器技能没有前置）
                if (tSpell->reqSpell && !player->HasSpell(tSpell->reqSpell))
                    continue;

                addedSpells.insert(tSpell->spell);
                validSpells.push_back(tSpell);
            }
        }
    }
    // 骑术 - 使用完整的骑术训练师
    else if (skillType == SKILL_TYPE_RIDING)
    {
        trainerName = "骑术训练师";

        TrainerSpellData const* trainerSpells = sObjectMgr->GetNpcTrainerSpells(ridingTrainerId);
        if (!trainerSpells)
        {
            LOG_ERROR("module", "[技能大师] 骑术训练师 {} 没有技能数据", ridingTrainerId);
            ChatHandler(player->GetSession()).PSendSysMessage("骑术训练师数据不存在");
            return;
        }

        trainerType = trainerSpells->trainerType;

        for (TrainerSpellMap::const_iterator itr = trainerSpells->spellList.begin();
             itr != trainerSpells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            bool valid = true;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                if (!tSpell->learnedSpell[i])
                    continue;
                if (!player->IsSpellFitByClassAndRace(tSpell->learnedSpell[i]))
                {
                    valid = false;
                    break;
                }
            }

            if (!valid)
                continue;

            // 检查前置技能要求
            if (tSpell->reqSpell && !player->HasSpell(tSpell->reqSpell))
                continue;

            validSpells.push_back(tSpell);
        }
    }
    // 职业技能
    else
    {
        auto it = classTrainerIds.find(static_cast<uint8>(skillType));
        if (it == classTrainerIds.end())
        {
            LOG_ERROR("module", "[技能大师] 未找到职业 {} 的训练师", skillType);
            ChatHandler(player->GetSession()).PSendSysMessage("未找到对应的训练师数据");
            return;
        }

        uint32 trainerId = it->second;
        trainerName = GetClassName(static_cast<uint8>(skillType));

        TrainerSpellData const* trainerSpells = sObjectMgr->GetNpcTrainerSpells(trainerId);
        if (!trainerSpells)
        {
            LOG_ERROR("module", "[技能大师] 训练师 {} 没有技能数据", trainerId);
            ChatHandler(player->GetSession()).PSendSysMessage("训练师数据不存在 (ID: {})", trainerId);
            return;
        }

        trainerType = trainerSpells->trainerType;

        for (TrainerSpellMap::const_iterator itr = trainerSpells->spellList.begin();
             itr != trainerSpells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            bool valid = true;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                if (!tSpell->learnedSpell[i])
                    continue;
                if (!player->IsSpellFitByClassAndRace(tSpell->learnedSpell[i]))
                {
                    valid = false;
                    break;
                }
            }

            if (!valid)
                continue;

            // 检查前置技能要求
            if (tSpell->reqSpell && !player->HasSpell(tSpell->reqSpell))
                continue;

            validSpells.push_back(tSpell);
        }
    }

    // 使用我们自定义的标题
    std::string strTitle = Acore::StringFormat("技能综合大师 - {}", trainerName);

    // 构建数据包 - 按照官方格式，预分配大小
    size_t packetSize = 8 + 4 + 4 + validSpells.size() * 38 + strTitle.size() + 1;
    WorldPacket data(SMSG_TRAINER_LIST, packetSize);

    data << creature->GetGUID();                              // NPC GUID (8 bytes)
    data << uint32(trainerType);                              // trainer type (4 bytes)
    data << uint32(validSpells.size());                       // spell count (4 bytes)

    for (TrainerSpell const* tSpell : validSpells)
    {
        bool primaryProfFirstRank = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (!tSpell->learnedSpell[i])
                continue;
            SpellInfo const* learnedSpellInfo = sSpellMgr->GetSpellInfo(tSpell->learnedSpell[i]);
            if (learnedSpellInfo && learnedSpellInfo->IsPrimaryProfessionFirstRank())
            {
                primaryProfFirstRank = true;
                break;
            }
        }

        TrainerSpellState state = player->GetTrainerSpellState(tSpell);

        data << uint32(tSpell->spell);                                                      // spell id (4 bytes)
        data << uint8(state == TRAINER_SPELL_GREEN_DISABLED ? TRAINER_SPELL_GREEN : state); // state (1 byte)
        data << uint32(std::floor(tSpell->spellCost * fDiscountMod));                       // cost (4 bytes)
        data << uint32(primaryProfFirstRank && canLearnPrimaryProf ? 1 : 0);                // can learn (4 bytes)
        data << uint32(primaryProfFirstRank ? 1 : 0);                                       // is primary (4 bytes)
        data << uint8(tSpell->reqLevel);                                                    // required level (1 byte)
        data << uint32(tSpell->reqSkill);                                                   // required skill (4 bytes)
        data << uint32(tSpell->reqSkillValue);                                              // required skill value (4 bytes)

        // 前置技能 (3 x 4 bytes = 12 bytes)
        uint8 maxReq = 0;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (!tSpell->learnedSpell[i])
                continue;
            if (uint32 prevSpellId = sSpellMgr->GetPrevSpellInChain(tSpell->learnedSpell[i]))
            {
                data << uint32(prevSpellId);
                ++maxReq;
            }
            if (maxReq == 3)
                break;
            SpellsRequiringSpellMapBounds spellsRequired = sSpellMgr->GetSpellsRequiredForSpellBounds(tSpell->learnedSpell[i]);
            for (SpellsRequiringSpellMap::const_iterator itr2 = spellsRequired.first; itr2 != spellsRequired.second && maxReq < 3; ++itr2)
            {
                data << uint32(itr2->second);
                ++maxReq;
            }
            if (maxReq == 3)
                break;
        }
        while (maxReq < 3)
        {
            data << uint32(0);
            ++maxReq;
        }
    }

    data << strTitle;                                         // 训练师窗口标题

    player->GetSession()->SendPacket(&data);

    if (validSpells.empty())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("没有可学习的技能");
    }
}

const char* SkillMasterCreatureScript::GetClassName(uint8 classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:     return "战士";
        case CLASS_PALADIN:     return "圣骑士";
        case CLASS_HUNTER:      return "猎人";
        case CLASS_ROGUE:       return "盗贼";
        case CLASS_PRIEST:      return "牧师";
        case CLASS_DEATH_KNIGHT:return "死亡骑士";
        case CLASS_SHAMAN:      return "萨满";
        case CLASS_MAGE:        return "法师";
        case CLASS_WARLOCK:     return "术士";
        case CLASS_DRUID:       return "德鲁伊";
        default:                return "未知";
    }
}

const char* SkillMasterCreatureScript::GetClassIcon(uint8 classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:     return "Ability_Warrior_BattleShout";
        case CLASS_PALADIN:     return "Spell_Holy_HolyBolt";
        case CLASS_HUNTER:      return "Ability_Hunter_SteadyShot";
        case CLASS_ROGUE:       return "Ability_BackStab";
        case CLASS_PRIEST:      return "Spell_Holy_PowerWordShield";
        case CLASS_DEATH_KNIGHT:return "Spell_Deathknight_ClassIcon";
        case CLASS_SHAMAN:      return "Spell_Nature_Lightning";
        case CLASS_MAGE:        return "Spell_Frost_IceStorm";
        case CLASS_WARLOCK:     return "Spell_Shadow_CurseOfTounAA";
        case CLASS_DRUID:       return "Ability_Druid_Maul";
        default:                return "INV_Misc_QuestionMark";
    }
}

// SkillMasterWorldScript 实现
SkillMasterWorldScript::SkillMasterWorldScript() : WorldScript("SkillMasterWorldScript") {}

void SkillMasterWorldScript::OnAfterConfigLoad(bool /*reload*/)
{
    skillMasterEnableModule = sConfigMgr->GetOption<bool>("SkillMaster.Enable", true);
}

void SkillMasterWorldScript::OnStartup()
{
    if (skillMasterEnableModule)
    {
        LOG_INFO("server.loading", ">> 技能综合大师模块已加载 (支持任何绑定 npc_skill_master 脚本的NPC)");
    }
}

// 添加脚本
void AddSkillMasterScripts()
{
    new SkillMasterCreatureScript();
    new SkillMasterWorldScript();
    new SkillMasterServerScript();
}

// SkillMasterServerScript 实现
SkillMasterServerScript::SkillMasterServerScript()
    : ServerScript("SkillMasterServerScript", { SERVERHOOK_CAN_PACKET_RECEIVE })
{
}

bool SkillMasterServerScript::CanPacketReceive(WorldSession* session, WorldPacket& packet)
{
    if (!skillMasterEnableModule)
        return true;

    if (!session)
        return true;

    // 检查是否是训练师购买技能的数据包
    if (packet.GetOpcode() == CMSG_TRAINER_BUY_SPELL)
        return HandleTrainerBuySpell(session, packet);

    return true;
}

bool SkillMasterServerScript::HandleTrainerBuySpell(WorldSession* session, WorldPacket& packet)
{
    Player* player = session->GetPlayer();
    if (!player)
        return true;

    // 检查玩家是否有活跃的技能大师会话
    auto it = playerSkillMasterSessions.find(player->GetGUID());
    if (it == playerSkillMasterSessions.end())
        return true; // 没有会话，让默认处理器处理

    ObjectGuid guid;
    uint32 spellId;

    // 读取数据包
    packet >> guid >> spellId;

    // 使用 ObjectAccessor 直接获取 Creature
    Creature* creature = ObjectAccessor::GetCreature(*player, guid);
    if (!creature)
    {
        playerSkillMasterSessions.erase(it);
        return true;
    }

    // 检查NPC是否有TRAINER标志
    if (!(creature->GetCreatureTemplate()->npcflag & UNIT_NPC_FLAG_TRAINER))
    {
        playerSkillMasterSessions.erase(it);
        return true;
    }

    // 检查距离（允许30码内）
    if (!player->IsWithinDistInMap(creature, 30.0f))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("你离训练师太远了");
        return false;
    }

    uint32 skillType = it->second;

    // 查找技能
    TrainerSpell const* trainerSpell = FindTrainerSpell(spellId, skillType, player->getClass());
    if (!trainerSpell)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("技能数据未找到 (ID: {})", spellId);
        return false; // 阻止默认处理器
    }

    // 检查前置技能要求
    if (trainerSpell->reqSpell && !player->HasSpell(trainerSpell->reqSpell))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("缺少前置技能");
        return false;
    }

    // 检查技能状态
    TrainerSpellState state = player->GetTrainerSpellState(trainerSpell);

    if (state != TRAINER_SPELL_GREEN && state != TRAINER_SPELL_GREEN_DISABLED)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("无法学习该技能 (状态: {})", static_cast<int>(state));
        return false;
    }

    // 计算费用（包含声望折扣）
    float discountMod = player->GetReputationPriceDiscount(creature);
    uint32 cost = uint32(std::floor(trainerSpell->spellCost * discountMod));

    // 检查金币
    if (!player->HasEnoughMoney(cost))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("金币不足");
        return false;
    }

    // 扣除金币
    player->ModifyMoney(-static_cast<int64>(cost));

    // 播放特效
    creature->SendPlaySpellVisual(179);
    creature->SendPlaySpellImpact(player->GetGUID(), 362);

    // 学习技能
    if (trainerSpell->IsCastable())
        player->CastSpell(player, trainerSpell->spell, true);
    else
        player->learnSpell(spellId);

    // 发送购买成功数据包
    WorldPacket data(SMSG_TRAINER_BUY_SUCCEEDED, 12);
    data << guid;
    data << uint32(spellId);
    session->SendPacket(&data);

    return false; // 阻止默认处理器处理
}

TrainerSpell const* SkillMasterServerScript::FindTrainerSpell(uint32 spellId, uint32 skillType, uint8 playerClass)
{
    // 武器技能 - 遍历所有武器训练师
    if (skillType == SKILL_TYPE_WEAPON)
    {
        for (uint32 trainerId : weaponTrainerIds)
        {
            TrainerSpellData const* trainerSpells = sObjectMgr->GetNpcTrainerSpells(trainerId);
            if (!trainerSpells)
                continue;

            TrainerSpell const* spell = trainerSpells->Find(spellId);
            if (spell)
                return spell;
        }
        return nullptr;
    }

    // 骑术
    if (skillType == SKILL_TYPE_RIDING)
    {
        TrainerSpellData const* trainerSpells = sObjectMgr->GetNpcTrainerSpells(ridingTrainerId);
        if (trainerSpells)
        {
            TrainerSpell const* spell = trainerSpells->Find(spellId);
            if (spell)
                return spell;
        }
        return nullptr;
    }

    // 职业技能
    auto it = classTrainerIds.find(playerClass);
    if (it != classTrainerIds.end())
    {
        uint32 trainerId = it->second;
        TrainerSpellData const* trainerSpells = sObjectMgr->GetNpcTrainerSpells(trainerId);
        if (trainerSpells)
        {
            TrainerSpell const* spell = trainerSpells->Find(spellId);
            if (spell)
                return spell;
        }
    }

    return nullptr;
}

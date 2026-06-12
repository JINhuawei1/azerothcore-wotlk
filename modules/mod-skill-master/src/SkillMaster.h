#ifndef SKILL_MASTER_H
#define SKILL_MASTER_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "ScriptedGossip.h"
#include "GossipDef.h"
#include "WorldSession.h"
#include "Config.h"
#include "Chat.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <map>
#include <vector>
#include <set>

// 技能类型定义
#define SKILL_TYPE_WEAPON 100
#define SKILL_TYPE_RIDING 101

// 菜单选项
enum SkillMasterMenu
{
    SKILL_MASTER_MENU_MAIN          = 0,
    SKILL_MASTER_MENU_CLASS_SKILLS  = 1,
    SKILL_MASTER_MENU_WEAPON_SKILLS = 2,
    SKILL_MASTER_MENU_RIDING        = 3,
};

// 全局配置
extern bool skillMasterEnableModule;

// 各职业官方训练师ID
extern std::map<uint8, uint32> classTrainerIds;
extern std::vector<uint32> weaponTrainerIds;
extern uint32 ridingTrainerId;

// 脚本类声明
class SkillMasterCreatureScript : public CreatureScript
{
public:
    SkillMasterCreatureScript();
    bool OnGossipHello(Player* player, Creature* creature) override;
    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override;

private:
    void ShowMainMenu(Player* player, Creature* creature);
    void SendTrainerListForType(Player* player, Creature* creature, uint32 skillType);
    const char* GetClassName(uint8 classId);
    const char* GetClassIcon(uint8 classId);
};

class SkillMasterWorldScript : public WorldScript
{
public:
    SkillMasterWorldScript();
    void OnAfterConfigLoad(bool reload) override;
    void OnStartup() override;
};

// 购买流程由核心默认的 CMSG_TRAINER_BUY_SPELL 处理器完成:
// NPC 通过 npc_trainer 引用行(见 sql/world/技能综合大师.sql)持有全量技能列表,
// 不再注册 ServerScript 拦包(注册任意 ServerScript 会让全服每个收发包多一次深拷贝)。

// 添加脚本函数
void AddSkillMasterScripts();

#endif // SKILL_MASTER_H

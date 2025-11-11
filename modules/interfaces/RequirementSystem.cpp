/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "RequirementSystem.h"
#include "Log.h"

// 全局实例指针，将由需求模板模块设置
static RequirementSystem* _instance = nullptr;

RequirementSystem* RequirementSystem::instance()
{
    // 如果实例未设置，记录错误
    if (!_instance)
    {
        LOG_ERROR("module", "需求系统未初始化，请确保需求模板模块已加载");
    }
    
    return _instance;
}

// 设置全局实例的函数，由需求模板模块调用
void SetRequirementSystemInstance(RequirementSystem* instance)
{
    _instance = instance;
    LOG_DEBUG("module", "需求系统实例已设置");
}

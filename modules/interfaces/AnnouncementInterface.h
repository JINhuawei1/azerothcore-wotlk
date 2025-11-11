/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef ANNOUNCEMENT_INTERFACE_H
#define ANNOUNCEMENT_INTERFACE_H

#include "Player.h"

/**
 * @class AnnouncementInterface
 * @brief 公告模块接口
 * 
 * 此接口定义了公告模块的标准API，允许其他模块发送公告
 */
class AnnouncementInterface
{
public:
    virtual ~AnnouncementInterface() = default;
    
    /**
     * 发送公告
     * 
     * @param player 玩家指针
     * @param entry 公告模板ID
     * @param success 是否成功（true=成功消息，false=失败消息）
     * @return 是否成功发送
     */
    virtual bool SendAnnouncement(Player* player, uint32 entry, bool success = true) = 0;
};

#endif // ANNOUNCEMENT_INTERFACE_H

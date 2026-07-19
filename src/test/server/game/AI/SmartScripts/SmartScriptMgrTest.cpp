/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "SmartScriptMgr.h"
#include "gtest/gtest.h"

TEST(SmartScriptMgrTest, RecognizesCreatureUsedAsTimedActionListTarget)
{
    SmartAIEventMap eventMaps[SMART_SCRIPT_TYPE_MAX];

    SmartScriptHolder caller;
    caller.action.type = SMART_ACTION_CALL_TIMED_ACTIONLIST;
    caller.action.timedActionList.id = 2385500;
    caller.target.type = SMART_TARGET_CREATURE_DISTANCE;
    caller.target.unitDistance.creature = 23855;
    eventMaps[SMART_SCRIPT_TYPE_CREATURE][23850].push_back(caller);

    SmartScriptHolder timedAction;
    timedAction.entryOrGuid = 2385500;
    timedAction.source_type = SMART_SCRIPT_TYPE_TIMED_ACTIONLIST;
    eventMaps[SMART_SCRIPT_TYPE_TIMED_ACTIONLIST][2385500].push_back(timedAction);

    EXPECT_TRUE(SmartAIMgr::IsCreatureUsedAsTimedActionListTarget(23855, eventMaps));
    EXPECT_FALSE(SmartAIMgr::IsCreatureUsedAsTimedActionListTarget(23854, eventMaps));
}

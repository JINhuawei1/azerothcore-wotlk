# 宣传奖励系统模块
# 玩家宣传服务器获取兑换码,兑换专属武器,武器属性随宣传天数成长

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/PromotionRewardModule.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/PromotionRewardAudit.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/PromotionRewardCommands.cpp")

AC_ADD_SCRIPT_LOADER("PromotionReward" "${CMAKE_CURRENT_LIST_DIR}/src/loader.h")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_promotion_reward.conf.dist")

#
# Merged item commerce module: lease, purchase condition, purchase limit, sell reward.
#

set(MODULE_NAME "mod-item-commerce-system")

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemLeaseSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemPurchaseCondition.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemPurchaseCondition.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/PlacementNew.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemPurchaseLimit.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemPurchaseLimit.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemSellReward.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemSellReward.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemSellRewardCommand.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemSellRewardLoader.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RequirementTemplateInterface.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemUseCondition.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemUseReward.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemUseReward.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemUseRewardCommand.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemUseRewardLoader.cpp")
AC_ADD_SCRIPT_LOADER("mod_item_commerce_system" "${CMAKE_CURRENT_LIST_DIR}/src/mod_item_commerce_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/item_commerce_system.conf.dist")

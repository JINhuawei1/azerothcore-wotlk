# 仙门系统模块

include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-magic-hit-system/src/MagicHitSystem.h")
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-magic-hit-system/src)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-wear-control/src/WearControl.h")
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-wear-control/src)
endif()

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_xianmen_system.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/XianmenArtifactSlots.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/XianqiFeatureSpells.cpp")
AC_ADD_SCRIPT_LOADER("mod_xianmen_system" "${CMAKE_CURRENT_LIST_DIR}/src/mod_xianmen_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_xianmen_system.conf.dist")

message("-- 仙门系统模块已加载")

# 仙门系统模块

include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_xianmen_system.cpp")
AC_ADD_SCRIPT_LOADER("mod_xianmen_system" "${CMAKE_CURRENT_LIST_DIR}/src/mod_xianmen_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_xianmen_system.conf.dist")

message("-- 仙门系统模块已加载")

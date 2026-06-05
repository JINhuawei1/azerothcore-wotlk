set(MODULE_NAME "mod-db-config-manager")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/DbConfigManager.cpp")
AC_ADD_SCRIPT_LOADER("DbConfigManager" "${CMAKE_CURRENT_LIST_DIR}/src/loader.h")

message(STATUS ">> 数据库通用配置管理模块已加载")

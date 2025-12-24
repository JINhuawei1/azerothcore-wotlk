# 飞升系统模块 CMakeLists.txt

# 添加包含路径（需求模板系统接口）
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
  ${CMAKE_SOURCE_DIR}/modules/mod-requirement-template/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/AscensionSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/AscensionSystem.h")
AC_ADD_SCRIPT_LOADER("AscensionSystem" "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.h")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_ascension_system.conf.dist")

message("-- 飞升系统模块已加载")


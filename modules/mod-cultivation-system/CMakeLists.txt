#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
#

# 添加包含路径
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/mod-requirement-template/src
  ${CMAKE_SOURCE_DIR}/modules/mod-elemental-system/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add scripts
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_cultivation_system.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_cultivation_system_loader.cpp")

# Add config file
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_cultivation_system.conf.dist" "${CONF_DIR}/mod_cultivation_system.conf.dist")

#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
#

# 添加包含路径
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
  ${CMAKE_SOURCE_DIR}/modules/mod-requirement-template/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add scripts
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/Reincarnation.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ReincarnationModule.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ReincarnationCommands.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ReincarnationLoader.cpp")

# Add config file
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/reincarnation.conf.dist" "${CONF_DIR}/reincarnation.conf.dist")

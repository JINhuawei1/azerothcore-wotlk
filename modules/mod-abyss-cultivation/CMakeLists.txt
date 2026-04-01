#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
#

include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_abyss_cultivation.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_abyss_cultivation_loader.cpp")

AC_ADD_CONFIG_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_abyss_cultivation.conf.dist"
  "${CONF_DIR}/mod_abyss_cultivation.conf.dist"
)

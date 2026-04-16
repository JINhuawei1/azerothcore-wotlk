#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#

# 添加包含路径（需求模板系统接口）
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
  ${CMAKE_SOURCE_DIR}/modules/mod-requirement-template/src
  ${CMAKE_SOURCE_DIR}/modules/mod-item-skills/src
)

# Add scripts
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_tujian_system.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_tujian_system_loader.cpp")

# Add config file
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_tujian_system.conf.dist")

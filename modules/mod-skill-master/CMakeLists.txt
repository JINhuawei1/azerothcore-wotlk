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

# 设置模块名称
set(MODULE_NAME "mod-skill-master")

# 添加包含路径
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
)

# 添加源文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/SkillMaster.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/SkillMaster.h")
AC_ADD_SCRIPT_LOADER("SkillMaster" "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.h")

# 添加配置文件
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/SkillMaster.conf.dist")

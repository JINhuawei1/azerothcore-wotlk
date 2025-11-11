#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

# 添加脚本文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/ModuleManager.cpp")

# 添加包含路径
include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

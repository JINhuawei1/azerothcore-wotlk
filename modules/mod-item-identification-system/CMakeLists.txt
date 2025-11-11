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
set(MODULE_NAME "mod-item-identification-system")

# 添加包含路径
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 检查并集成其他模块 - 必须在 AC_ADD_SCRIPT 之前定义
if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-item-growth")
  message(STATUS "鉴定系统: 找到成长系统模块，启用集成")
  add_definitions(-DMODULE_ITEM_GROWTH)
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-item-growth/src)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-item-enhancement")
  message(STATUS "鉴定系统: 找到强化系统模块，启用集成")
  add_definitions(-DMODULE_ITEM_ENHANCEMENT)
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-item-enhancement/src)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-item-attributes")
  message(STATUS "鉴定系统: 找到属性系统模块，启用集成")
  add_definitions(-DMODULE_ITEM_ATTRIBUTES)
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-item-attributes/src)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-rune-system")
  message(STATUS "鉴定系统: 找到符文系统模块，启用集成")
  add_definitions(-DMODULE_RUNE_SYSTEM)
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-rune-system/src)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-requirement-template")
  message(STATUS "鉴定系统: 找到需求系统模块，启用集成")
  add_definitions(-DMODULE_REQUIREMENT_TEMPLATE)
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-requirement-template/src)
endif()

# 添加源文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemIdentificationSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemIdentificationSystem.h")
AC_ADD_SCRIPT_LOADER("ItemIdentificationSystem" "${CMAKE_CURRENT_SOURCE_DIR}/src/ItemIdentificationSystem_loader.cpp")

# 添加配置文件
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/ItemIdentificationSystem.conf.dist")

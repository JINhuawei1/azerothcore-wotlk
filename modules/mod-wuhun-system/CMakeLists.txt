include_directories(
  ${CMAKE_SOURCE_DIR}/modules
)

if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-item-attributes/src/ItemAttributesDBHelper.h")
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-item-attributes/src)
endif()

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_wuhun_system.cpp")
AC_ADD_SCRIPT_LOADER("mod_wuhun_system" "${CMAKE_CURRENT_LIST_DIR}/src/mod_wuhun_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_wuhun_system.conf.dist")

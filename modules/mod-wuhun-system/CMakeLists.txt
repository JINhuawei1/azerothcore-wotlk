include_directories(
  ${CMAKE_SOURCE_DIR}/modules
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_wuhun_system.cpp")
AC_ADD_SCRIPT_LOADER("mod_wuhun_system" "${CMAKE_CURRENT_LIST_DIR}/src/mod_wuhun_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_wuhun_system.conf.dist")

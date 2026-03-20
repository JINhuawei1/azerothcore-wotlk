include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_heal_rune_system.cpp")
AC_ADD_SCRIPT_LOADER("mod_heal_rune_system" "${CMAKE_CURRENT_LIST_DIR}/src/mod_heal_rune_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_heal_rune_system.conf.dist")

#
# Item synthesis system
#

include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/mod-requirement-template/src
  ${CMAKE_SOURCE_DIR}/modules/mod-reward-template/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_synthesis_system.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_synthesis_system_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_synthesis_system.conf.dist")

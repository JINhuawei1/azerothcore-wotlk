#
# Hermes bridge
#

include_directories(
  ${CMAKE_SOURCE_DIR}/modules/mod-hermes-bridge/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_hermes_bridge.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/HermesBridgeHandler.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/mod_hermes_bridge_loader.cpp")

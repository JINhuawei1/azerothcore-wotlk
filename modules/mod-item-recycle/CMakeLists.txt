AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemRecycle.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemRecycleCommands.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemRecycle.h")
AC_ADD_SCRIPT_LOADER("ItemRecycle" "${CMAKE_CURRENT_SOURCE_DIR}/src/ItemRecycleLoader.h")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/ItemRecycle.conf.dist" "conf/ItemRecycle.conf")
# 穿戴控制模块

include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/mod-hardcore-mode/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/WearControl.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/WearControl.h")
AC_ADD_SCRIPT_LOADER("WearControl" "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.h")

message("-- 穿戴控制模块已加载")

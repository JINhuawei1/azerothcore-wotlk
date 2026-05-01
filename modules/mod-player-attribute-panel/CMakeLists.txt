if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-huanjinxitong/src/HuanJingSystem.h")
  include_directories(${CMAKE_SOURCE_DIR}/modules/mod-huanjinxitong/src)
endif()

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_player_attribute_panel.cpp")
AC_ADD_SCRIPT_LOADER("mod_player_attribute_panel" "${CMAKE_CURRENT_LIST_DIR}/src/mod_player_attribute_panel_loader.cpp")

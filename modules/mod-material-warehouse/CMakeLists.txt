include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/interfaces
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_material_warehouse.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/MaterialWarehouseSystem.h")
AC_ADD_SCRIPT_LOADER("mod_material_warehouse" "${CMAKE_CURRENT_LIST_DIR}/src/mod_material_warehouse_loader.cpp")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_material_warehouse.conf.dist")

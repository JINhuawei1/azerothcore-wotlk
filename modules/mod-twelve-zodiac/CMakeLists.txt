# 十二生肖系统（mod-twelve-zodiac）

include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/TwelveZodiac.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/TwelveZodiacLoader.cpp")

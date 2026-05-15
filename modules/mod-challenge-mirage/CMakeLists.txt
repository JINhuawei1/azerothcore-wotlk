# 挑战幻境系统：玩家不隔离，只隔离生物维度

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ChallengeMirage.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ChallengeMirageBossAI.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ChallengeMirage.h")
AC_ADD_SCRIPT_LOADER("mod_challenge_mirage" "${CMAKE_CURRENT_LIST_DIR}/src/mod_challenge_mirage_loader.cpp")
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_challenge_mirage.conf.dist")

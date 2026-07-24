#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
CollectSourceFiles(
        ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE_SOURCES
)

include_directories(
        "mocks"
)

include_directories(
        ${CMAKE_SOURCE_DIR}/modules/mod-magic-hit-system/src
        ${CMAKE_SOURCE_DIR}/modules/mod-talent-soul/src
        ${CMAKE_SOURCE_DIR}/modules/mod-xianmen-system/src
)

add_executable(
        unit_tests
        ${PRIVATE_SOURCES}
)

target_link_libraries(
        unit_tests
        game
        gtest_main
        gmock_main
        game-interface
)

add_test(
        NAME
        unit
        COMMAND
        ${CMAKE_BINARY_DIR}/src/test/unit_tests
)

set(LOAD_PROTECTION_TEST_SOURCES
        server/game/Server/PlayerCastRateLimiterTest.cpp
        server/game/Spells/MagicHitBatchTest.cpp
        server/game/Spells/TalentSoulGcdTest.cpp
        server/game/Spells/XianqiSoulDebtTimersTest.cpp
)

add_executable(
        load_protection_tests
        ${LOAD_PROTECTION_TEST_SOURCES}
)

target_link_libraries(
        load_protection_tests
        gtest_main
        game-interface
)

add_test(
        NAME
        load_protection
        COMMAND
        $<TARGET_FILE:load_protection_tests>
)

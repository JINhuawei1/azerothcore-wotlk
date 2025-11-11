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

# JsonCpp library for JSON parsing
# We'll use a simple header-only implementation or compile from source

# Check if we can use system jsoncpp
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(JSONCPP jsoncpp)
endif()

if(JSONCPP_FOUND)
    # Use system jsoncpp
    add_library(jsoncpp INTERFACE)
    target_link_libraries(jsoncpp INTERFACE ${JSONCPP_LIBRARIES})
    target_include_directories(jsoncpp INTERFACE ${JSONCPP_INCLUDE_DIRS})
    target_compile_options(jsoncpp INTERFACE ${JSONCPP_CFLAGS_OTHER})
    message(STATUS "Using system jsoncpp")
else()
    # Build jsoncpp from source
    message(STATUS "Building jsoncpp from source")
    
    # Create a simple JSON parser implementation
    set(JSONCPP_SOURCES
        src/json_reader.cpp
        src/json_value.cpp
        src/json_writer.cpp
    )
    
    set(JSONCPP_HEADERS
        include/json/json.h
        include/json/reader.h
        include/json/value.h
        include/json/writer.h
    )
    
    add_library(jsoncpp STATIC ${JSONCPP_SOURCES})
    
    target_include_directories(jsoncpp
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(jsoncpp
        PRIVATE
            acore-dependency-interface
    )
    
    set_target_properties(jsoncpp
        PROPERTIES
            FOLDER "deps"
    )
endif()

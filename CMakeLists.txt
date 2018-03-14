cmake_minimum_required(VERSION 3.10)
project(reaper_ossia_extension)


set(WDL_INCLUDE_PATH /Users/pchd/Repositories/WDL)
set(WDL_LIBRARY_PATH)

set(OSSIA_INCLUDE_PATH)
set(OSSIA_LIBRARY_PATH)

set(OSSIA_REAPER_HEADERS
    include/csurf.h)
set(OSSIA_REAPER_SOURCES
    #csurf_main.cpp
    ossia-reaper.cpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OSSIA_STATIC_LIB /Users/pchd/Repositories/libossia-static-macos/lib/static/libossia.a)
add_library(ossia STATIC IMPORTED)
set_target_properties(ossia PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "TINYSPLINE_DOUBLE_PRECISION;ASIO_STANDALONE=1;RAPIDJSON_HAS_STDSTRING=1;\$<\$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING>;\$<\$<CONFIG:Debug>:BOOST_MULTI_INDEX_ENABLE_SAFE_MODE>"
    INTERFACE_INCLUDE_DIRECTORIES "${OSSIA_INCLUDE_DIR}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION ${OSSIA_STATIC_LIB} )


add_library(swell STATIC IMPORTED)
set_target_properties(swell PROPERTIES
    IMPORTED_LOCATION /Users/pchd/Repositories/WDL/WDL/build-swell-Desktop_Qt_5_10_0_clang_64bit-Du00e9faut/libswell.a)

add_library (${PROJECT_NAME} SHARED ${OSSIA_REAPER_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PUBLIC include/ossia)
target_include_directories(${PROJECT_NAME} PUBLIC ${WDL_INCLUDE_PATH})

target_link_libraries(${PROJECT_NAME} ossia )
target_link_libraries(${PROJECT_NAME} swell )



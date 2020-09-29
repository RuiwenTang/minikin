cmake_minimum_required(VERSION 3.10)

project(minikin)

set(CMAKE_CXX_STANDARD 14)
# for icu4c
find_package(PkgConfig REQUIRED)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/usr/local/opt/icu4c/")
pkg_search_module(ICU_UC icu-uc)
link_directories(${ICU_UC_LIBRARY_DIRS})

set(HB_HAVE_ICU ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/harfbuzz)


include_directories(third_party/harfbuzz/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${ICU_UC_INCLUDE_DIRS})

set(MINIKIN_HEADERS
        include/minikin/AnalyzeStyle.h
        include/minikin/CmapCoverage.h
        include/minikin/FontCollection.h
        include/minikin/FontFamily.h
        include/minikin/GraphemeBreak.h
        include/minikin/Hyphenator.h
        include/minikin/Layout.h
        include/minikin/LineBreaker.h
        include/minikin/Measurement.h
        include/minikin/MinikinFont.h
        include/minikin/MinikinRefCounted.h
        include/minikin/SparseBitSet.h
        include/minikin/WordBreaker.h
        )

set(MINIKIN_SOURCES
        libs/minikin/AnalyzeStyle.cpp
        libs/minikin/CmapCoverage.cpp
        libs/minikin/FontCollection.cpp
        libs/minikin/FontFamily.cpp
        libs/minikin/FontLanguage.cpp
        libs/minikin/FontLanguage.h
        libs/minikin/FontLanguageListCache.cpp
        libs/minikin/FontLanguageListCache.h
        libs/minikin/GraphemeBreak.cpp
        libs/minikin/HbFontCache.cpp
        libs/minikin/HbFontCache.h
        libs/minikin/Hyphenator.cpp
        libs/minikin/Layout.cpp
        libs/minikin/LayoutUtils.cpp
        libs/minikin/LayoutUtils.h
        libs/minikin/LineBreaker.cpp
        libs/minikin/Measurement.cpp
        libs/minikin/MinikinFont.cpp
        libs/minikin/MinikinInternal.cpp
        libs/minikin/MinikinInternal.h
        libs/minikin/MinikinRefCounted.cpp
        libs/minikin/SparseBitSet.cpp
        libs/minikin/WordBreaker.cpp
        )

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(MINIKIN_HEADERS ${MINIKIN_HEADERS} include/minikin/MinikinFontCoreText.h)
    set(MINIKIN_SOURCES ${MINIKIN_SOURCES} libs/minikin/MinikinFontCoreText.cpp)
endif ()

set(UTILS_HEADERS
        include/utils/JenkinsHash.h
        include/utils/LruCache.h
        include/utils/TypeHelpers.h
        )

set(UTILS_SOURCES
        libs/utils/JenkinsHash.cpp
        )

set(UNICODE_DATA ${CMAKE_CURRENT_BINARY_DIR}/generated/UnicodeData.h)

file(WRITE ${UNICODE_DATA})
add_custom_target(unicode_data
        COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/libs/minikin/unicode_emoji_h_gen.py
        "--input" ${CMAKE_CURRENT_SOURCE_DIR}/libs/minikin/emoji-data.txt "--output" ${UNICODE_DATA}
        )


add_library(minikin SHARED ${MINIKIN_HEADERS} ${MINIKIN_SOURCES} ${UTILS_HEADERS} ${UTILS_SOURCES})
target_include_directories(minikin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})


message(${ICU_UC_LIBRARY_DIRS})
message(${ICU_UC_LIBRARIES})


add_dependencies(minikin unicode_data)

target_link_libraries(minikin harfbuzz harfbuzz-icu ${ICU_UC_LIBRARIES})
cmake_minimum_required(VERSION 3.10)

project(minikin)

set(CMAKE_CXX_STANDARD 14)
# for icu4c
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    cmake_policy(SET CMP0074 NEW)
    find_package(ICU REQUIRED uc io)
    include_directories(${ICU_INCLUDE_DIRS})
else()
    find_package(PkgConfig REQUIRED)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/usr/local/opt/icu4c/")
    pkg_search_module(ICU_UC icu-uc)
    link_directories(${ICU_UC_LIBRARY_DIRS})
endif()
find_package(Freetype)

if (${FREETYPE_FOUND})
    message("FreeType Found with version ${FREETYPE_VERSION_STRING}")
    set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
endif ()

set(HB_HAVE_ICU ON CACHE BOOL "" FORCE)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # Fix compile error
    set(BUILD_SHARED_LIBS ON)
endif()
add_subdirectory(third_party/harfbuzz)


include_directories(third_party/harfbuzz/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${ICU_UC_INCLUDE_DIRS})
if (${FREETYPE_FOUND})
    include_directories(${FREETYPE_INCLUDE_DIRS})
    add_definitions(-DUSE_FT)
endif ()

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
        include/txt/FontCollection.h
        include/txt/Paragraph.h
        include/txt/ParagraphBuilder.h
        include/txt/ParagraphStyle.h
        include/txt/PlaceHolderRun.h
        include/txt/Platform.h
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
        libs/txt/ParagraphStyle.cpp
        libs/txt/FontCollection.cpp
        )

if (${FREETYPE_FOUND})
    set(MINIKIN_HEADERS ${MINIKIN_HEADERS} include/minikin/MinikinFontFreeType.h)
    set(MINIKIN_SOURCES ${MINIKIN_SOURCES} libs/minikin/MinikinFontFreeType.cpp)
endif ()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(MINIKIN_HEADERS ${MINIKIN_HEADERS} include/minikin/MinikinFontCoreText.h)
    set(MINIKIN_SOURCES ${MINIKIN_SOURCES}
            libs/minikin/MinikinFontCoreText.cpp
            libs/txt/platform/PlatformMac.mm
            )

    set(MINIKIN_LIBS ${MINIKIN_LIBS} "-framework CoreGraphics")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(MINIKIN_SOURCES ${MINIKIN_SOURCES}
            libs/txt/platform/PlatformWindows.cpp
        )
endif()

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


add_dependencies(minikin unicode_data)

if (${FREETYPE_FOUND})
    set(MINIKIN_LIBS ${MINIKIN_LIBS} ${FREETYPE_LIBRARIES})
endif ()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(MINIKIN_LIBS ${MINIKIN_LIBS} ${ICU_LIBRARIES})
endif()

target_link_libraries(minikin harfbuzz harfbuzz-icu ${ICU_UC_LIBRARIES} ${MINIKIN_LIBS})

add_subdirectory(sample)
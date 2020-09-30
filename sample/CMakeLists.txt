
set(FONT_DIR ${CMAKE_SOURCE_DIR}/third_party/fonts)
configure_file(SampleConfig.h.in SampleConfig.h @ONLY)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_executable(ct_example ct_example.cpp)
    target_link_libraries(ct_example minikin)
endif ()



if (${FREETYPE_FOUND})
    add_executable(example example.cpp)
    target_link_libraries(example minikin)
endif ()

add_executable(paragraph_example paragraph_example.cpp)
target_link_libraries(paragraph_example minikin)
cmake_minimum_required(VERSION 3.21)
project(Logger)

add_library(${PROJECT_NAME} SHARED)

target_compile_definitions(${PROJECT_NAME}
        PRIVATE LOGGER_BUILDING_LIBRARY
) # 定义导出宏

file(GLOB_RECURSE srcs CONFIGURE_DEPENDS
        "src/*.cpp"
        "include/*.hpp"
        "include/*.h")

file(GLOB_RECURSE srcs_p CONFIGURE_DEPENDS
        "private/*.cpp"
        "private/*.h")

target_sources(${PROJECT_NAME} PRIVATE ${srcs} ${srcs_p})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_include_directories(${PROJECT_NAME} PRIVATE private)

#include(../cmake/3rd.cmake)

# 链接第三方库
# 这里用PRIVATE明确私有化
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE yaml-tool)

#set_target_properties(${PROJECT_NAME}
#        PROPERTIES
#        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
#        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
#        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
#)


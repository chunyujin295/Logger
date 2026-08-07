# spdlog
# 设置导出静态库，这样install脚本中不需要顺带发布
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "spdlog Build Shared Lib" FORCE) # 覆盖spdlog的Option，生成静态库

# 根据 SPDLOG_VERSION 选择对应版本（默认 1.17.0）
if(NOT SPDLOG_VERSION)
    set(SPDLOG_VERSION "1.17.0")
endif()

if(SPDLOG_VERSION STREQUAL "1.17.0")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/spdlog-1.17.0)
elseif(SPDLOG_VERSION STREQUAL "1.16.0")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/spdlog-1.16.0)
else()
    message(FATAL_ERROR "Unsupported SPDLOG_VERSION: ${SPDLOG_VERSION}. Supported: 1.16.0, 1.17.0")
endif()

set_target_properties(spdlog
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# yaml_tool
# yaml_tool目前只能支持动态链接
set(YAML_TOOL_BUILD_SHARED_LIBS ON CACHE BOOL "yaml-tool Build Shared Lib" FORCE)
set(YAML_TOOL_INSTALL OFF CACHE BOOL "yaml-tool install" FORCE)
CPMAddPackage(
        NAME yaml-tool
        GIT_REPOSITORY git@github.com:chunyujin295/yaml-tool.git
        GIT_TAG v1.1.4
)
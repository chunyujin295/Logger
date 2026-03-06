# spdlog
# 设置导出静态库，这样install脚本中不需要顺带发布
set(SPDLOG_BUILD_SHARED ON CACHE BOOL "spdlog Build Shared Lib" FORCE) # 覆盖spdlog的Option，生成静态库

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/spdlog-1.16.0)

set_target_properties(spdlog
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# yaml_tool
set(YAML_TOOL_BUILD_SHARED_LIBS ON CACHE BOOL "yaml-tool Build Shared Lib" FORCE)
set(YAML_TOOL_INSTALL OFF CACHE BOOL "yaml-tool install" FORCE)
CPMAddPackage(
        NAME yaml-tool
        GIT_REPOSITORY git@github.com:chunyujin295/yaml-tool.git
        GIT_TAG v1.1.0
)

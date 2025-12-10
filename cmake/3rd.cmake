# spdlog
# 设置导出静态库，这样install脚本中不需要顺带发布
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "yamlCpp Build Shared Lib" FORCE) # 覆盖spdlog的Option，生成静态库

add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/spdlog-1.16.0)

set_target_properties(spdlog
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# yaml_tool

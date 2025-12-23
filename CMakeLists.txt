cmake_minimum_required(VERSION 3.25)
project(Logger)

set(CMAKE_CXX_STANDARD 17)

add_library(Logger SHARED)

target_compile_definitions(Logger
        PRIVATE LOGGER_BUILDING_LIBRARY
) # 定义导出宏

file(GLOB_RECURSE srcs CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

file(GLOB_RECURSE srcs_p CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/private/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/private/*.h")

target_sources(Logger PRIVATE ${srcs} ${srcs_p})

target_include_directories(Logger
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_include_directories(Logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rd.cmake)

# 链接第三方库
# 这里用PRIVATE明确私有化
target_link_libraries(Logger PRIVATE spdlog)
target_link_libraries(Logger PRIVATE yaml-tool)

set_target_properties(Logger
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# 发布/安装脚本
install(TARGETS Logger
        EXPORT LoggerTargets
        RUNTIME DESTINATION bin      # Windows DLL
        LIBRARY DESTINATION lib      # .so / .dylib
        ARCHIVE DESTINATION lib      # static lib / import lib
)
install(
        DIRECTORY include/
        DESTINATION include
)

# 生成Config文件，确保调用方可以使用find_package
install(EXPORT LoggerTargets
        FILE LoggerTargets.cmake
        NAMESPACE Logger::
        DESTINATION lib/cmake/Logger
)

# 如果第三方库导出作为动态库，那么install的时候需要附带
#install(TARGETS yaml-cpp
#        EXPORT LoggerTargets
#)





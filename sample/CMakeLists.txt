## developer playground & black box testing
#cmake_minimum_required(VERSION 3.21)
#project(sample VERSION 0.0.1)
#
#include(../cmake/utils.cmake)
#config_project(ON)
#add_executable(${PROJECT_NAME} "" )
#
#file(GLOB_RECURSE "src" CONFIGURE_DEPENDS "*.cpp" "*.h")
#file(GLOB_RECURSE "uis" CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.ui")
#file(GLOB_RECURSE "qrcs" CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/*.qrc")
#
#target_sources(${PROJECT_NAME} PRIVATE ${src} ${uis} ${qrcs})
#target_link_libraries(${PROJECT_NAME} PRIVATE cxi_controls cxi_common)
#
#copy_imported_dependencies(${PROJECT_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

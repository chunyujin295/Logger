cmake_minimum_required(VERSION 3.21)
project(Logger_LIB)
set(CMAKE_CXX_STANDARD 17)

include(cmake/3rd.cmake)
include(cmake/option.cmake)

set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(logger)

if (BUILD_TEST)
    add_subdirectory(test)
endif ()



# =========================
# 全家桶安装/导出（重点）
# =========================
if (LOGGER_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # 1) 安装/导出：Logger
    install(TARGETS Logger
            EXPORT LoggerTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    # 2) 安装/导出：yaml-tool（从 CPM 拉来的 target）
    if (NOT TARGET yaml-tool)
        message(FATAL_ERROR "Target yaml-tool not found. CPMAddPackage may have failed.")
    endif ()

    install(TARGETS yaml-tool
            EXPORT LoggerTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    # yaml-tool 头文件（从 CPM 的源码目录装出去）
    # CPM 通常会提供 yaml-tool_SOURCE_DIR
    if (DEFINED yaml-tool_SOURCE_DIR AND EXISTS "${yaml-tool_SOURCE_DIR}/include")
        install(DIRECTORY "${yaml-tool_SOURCE_DIR}/include/"
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )
    else ()
        message(WARNING "yaml-tool_SOURCE_DIR not set or include/ not found; yaml-tool headers may not be installed.")
    endif ()

    # 3) 安装/导出：yaml-cpp（因为 yaml-tool 内部 add_subdirectory 进来了）
    # 保险起见：一起导出，彻底避免 export set 依赖检查炸裂
    if (TARGET yaml-cpp)
        install(TARGETS yaml-cpp
                EXPORT LoggerTargets
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

        # yaml-cpp 头文件（它在源码里 include/）
        # 注意：yaml-cpp 的源码目录取决于你拉取方式；这里用 TARGET 的 SOURCE_DIR 更稳
        get_target_property(_yamlcpp_src yaml-cpp SOURCE_DIR)
        if (_yamlcpp_src AND EXISTS "${_yamlcpp_src}/include")
            install(DIRECTORY "${_yamlcpp_src}/include/"
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
        endif ()
    else ()
        message(WARNING "Target yaml-cpp not found; skipping yaml-cpp install/export.")
    endif ()

    # 4) 安装/导出：spdlog（通常 target 是 spdlog::spdlog）
    if (TARGET spdlog)
        install(TARGETS spdlog
                EXPORT LoggerTargets
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
        # spdlog 头文件
        get_target_property(_spdlog_src spdlog SOURCE_DIR)
        if (_spdlog_src AND EXISTS "${_spdlog_src}/include")
            install(DIRECTORY "${_spdlog_src}/include/"
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
        endif ()
    elseif (TARGET spdlog)
        install(TARGETS spdlog
                EXPORT LoggerTargets
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
        get_target_property(_spdlog_src spdlog SOURCE_DIR)
        if (_spdlog_src AND EXISTS "${_spdlog_src}/include")
            install(DIRECTORY "${_spdlog_src}/include/"
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
        endif ()
    else ()
        message(WARNING "spdlog target not found; skipping spdlog install/export.")
    endif ()

    # 5) 安装你自己的 Logger public 头文件
    install(DIRECTORY "logger/include"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # 6) 导出 targets（全家桶都在 LoggerTargets 里）
    install(EXPORT LoggerTargets
            FILE LoggerTargets.cmake
            NAMESPACE Logger::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Logger
    )

    # 7) 生成 LoggerConfig.cmake / Version
    configure_package_config_file(
            "cmake/LoggerConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/LoggerConfig.cmake"
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Logger"
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/LoggerConfigVersion.cmake"
            VERSION 1.0.0
            COMPATIBILITY SameMajorVersion
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/LoggerConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/LoggerConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Logger"
    )
    # 8) 安装test
    if (BUILD_TEST)
        install(TARGETS Logger_test
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif ()
endif ()


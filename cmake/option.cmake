option(BUILD_TEST "Build Test" ON)
option(LOGGER_INSTALL "Install Logger bundle (Logger + yaml-tool + yaml-cpp + spdlog + test)" ON)
set(SPDLOG_VERSION "1.17.0" CACHE STRING "spdlog version (1.16.0 or 1.17.0)")
set_property(CACHE SPDLOG_VERSION PROPERTY STRINGS "1.16.0" "1.17.0")

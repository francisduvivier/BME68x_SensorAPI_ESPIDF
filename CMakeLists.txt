idf_component_register(
    SRC_DIRS "." "./BME68X_SensorAPI/"
    INCLUDE_DIRS "." "./BME68X_SensorAPI/"
    REQUIRES "driver"
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

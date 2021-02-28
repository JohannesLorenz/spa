cmake_minimum_required(VERSION 2.8)
include(cmake/init_project.txt)

# settings
project(spa)
set(DESCRIPTION "OSC plugin api")

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "0")
SET(VERSION_PATCH "1")

# processing
include(cmake/process_project.txt)

# Spa CMake Config. Not finished and not working yet. May be removed
# Use pkg config instead
include(CMakePackageConfigHelpers)
export(EXPORT spa-export FILE cmake/modules/spa-config.cmake NAMESPACE TestSpace)
configure_file(cmake/modules/spa-config.cmake "${CMAKE_CURRENT_BINARY_DIR}/SPA/spa-config.cmake" COPYONLY)
install(FILES cmake/modules/spa-config.cmake DESTINATION ${INSTALL_LIB_DIR}/cmake/SPA/ COMPONENT Devel)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/spa.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/spa.pc @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/spa.pc"
	DESTINATION "${INSTALL_LIB_DIR}/pkgconfig")


# examples
add_subdirectory(test)
add_subdirectory(examples)

print_summary_base()


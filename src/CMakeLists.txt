# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

list(FILTER app_sources EXCLUDE REGEX ".*/BoilerManager\\.cpp$")
list(FILTER app_sources EXCLUDE REGEX ".*/OpenThermManager\\.cpp$")

idf_component_register(SRCS ${app_sources})

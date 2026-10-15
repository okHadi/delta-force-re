cmake_minimum_required(VERSION 3.16)
project(delta_force_re LANGUAGES CXX)

add_library(dfre_core STATIC src/filename.cpp)
target_include_directories(dfre_core PUBLIC include)
target_compile_features(dfre_core PUBLIC cxx_std_17)
set_target_properties(dfre_core PROPERTIES CXX_EXTENSIONS OFF)

add_executable(filename_probe tools/filename_probe.cpp)
target_link_libraries(filename_probe PRIVATE dfre_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(native_filename tests/native_filename.cpp)
    target_link_libraries(native_filename PRIVATE dfre_core)
    add_test(NAME native_filename COMMAND native_filename)
endif()

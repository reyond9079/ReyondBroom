cmake_minimum_required(VERSION 3.16)
project(reyond_autolevel LANGUAGES CXX)
add_library(autolevel SHARED autolevel.cpp)
target_compile_features(autolevel PRIVATE cxx_std_17)
target_include_directories(autolevel PRIVATE ../../sdk)
find_package(Threads REQUIRED)
target_link_libraries(autolevel PRIVATE Threads::Threads)
set_target_properties(autolevel PROPERTIES PREFIX "")
add_custom_command(TARGET autolevel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.AutoLevel.txt
        $<TARGET_FILE_DIR:autolevel>)

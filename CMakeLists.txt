cmake_minimum_required(VERSION 2.8)
project(uuidworker)


set(CMAKE_C_FLAGS_DEBUG, "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG, "${CMAKE_CXX_FLAGS_DEBUG}")


add_subdirectory(lib)


add_executable(uuidworker uuidworker.c connection.c uuid.c)

target_link_libraries(uuidworker wxworker)
#target_link_libraries(uuidworker wxworker profiler)
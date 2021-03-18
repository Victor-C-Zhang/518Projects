cmake_minimum_required(VERSION 3.10)
project(asst1 C)

set(CMAKE_C_STANDARD 99)

#add_library(my_pthread SHARED library.c library.h)

set(CMAKE_C_FLAGS "-g")
add_executable(tests datastructs.c my_scheduler.c my_pthread.c pthread_test.c )
target_link_libraries(tests -lrt -lm)

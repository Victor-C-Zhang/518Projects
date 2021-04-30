cmake_minimum_required(VERSION 3.10)
project(asst2 C)

set(CMAKE_C_STANDARD 99)

#add_library(my_pthread SHARED library.c library.h)

set(CMAKE_C_FLAGS "-g")
add_executable(tests
    test_runner.c
    pthread_test.c
    malloc_test.c
    open_address_ht_test.c
    datastructs.c
    my_scheduler.c
    my_pthread.c
    my_malloc.c
    open_address_ht.c
    direct_mapping.c direct_mapping.h
    memory_finder.h memory_finder.c)
target_link_libraries(tests -lrt -lm)

add_executable(bchmk datastructs.c my_scheduler.c my_pthread.c data1.c my_malloc.c open_address_ht.c open_address_ht.h memory_finder.h memory_finder.c)
target_link_libraries(bchmk -lrt -lm)

#include "test_runner.h"
#include <stdio.h>
int main() {
//  testLinkedList();
//  testHashMap();
//  test_alarm();
//  test_thread_create();
//  testMutex();
//  test_thread_create_join();
//  test_thread_yield();

//  TEST_create();
//  TEST_get_put_delete();

//  TEST_malloc_thread_create_join();
//  TEST_malloc_directmapping();

  int throwaway = -1000;
  TEST_thread_func_swap1(&throwaway);

  printf("swap1\n");
  TEST_thread_swap(TEST_thread_func_swap1);
  printf("swap2\n");
  TEST_thread_swap(TEST_thread_func_swap2);
  printf("swap3\n");
  TEST_thread_swap(TEST_thread_func_swap3);
}

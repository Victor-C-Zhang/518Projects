#ifndef ASST1_TEST_RUNNER_H
#define ASST1_TEST_RUNNER_H

// scheduler tests
  void testLinkedList();
  void testHashMap();
  void test_alarm();
  void test_thread_create();
  void testMutex();
  void test_thread_create_join();
  void test_thread_yield();

// open-address ht tests
void TEST_create();
void TEST_get_put_delete();

// malloc tests
void TEST_malloc_thread_create_join();
void TEST_malloc_directmapping();
void TEST_malloc_directmapping1();
void* TEST_thread_func_swap1(void* i);
void* TEST_thread_func_swap2(void* i);
void* TEST_thread_func_swap3(void* i);
void TEST_thread_swap();

#endif //ASST1_TEST_RUNNER_H

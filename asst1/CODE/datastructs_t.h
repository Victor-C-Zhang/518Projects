// File:  my_pthread_t.h
// Author:  Yujie REN
// Date:  09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef DATASTRUCTS_T_H
#define DATASTRUCTS_T_H
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdint.h>

#define HASHSIZE 256 //must be a power of 2!

/* linked list structs */
typedef struct _node {
  void* data;
  struct _node* next;
} node_t;

typedef struct _ll { 
  node_t* head;
  node_t* tail;
} linked_list_t;

/* hashmap structs */
typedef struct _hash_node {
  uint32_t key;
  void* value;
  struct _hash_node* next;
} hash_node; 

typedef struct _hashmap {
  size_t num_buckets;
  size_t entries; /* Total number of entries in the table. */
  hash_node** buckets;
} hashmap;

/* Function Declarations: */

/* Datastructure functions */
void print_list(linked_list_t* list, void (*fptr)(void *));
void* get_head(linked_list_t* list);
void* get_tail(linked_list_t* list);
node_t* create_node(void* data);
linked_list_t* create_list();
void insert_head(linked_list_t* head, void* thing);
void insert_tail(linked_list_t* head, void* thing);
void* delete_head(linked_list_t* list);

hashmap* create_map();
void free_map(hashmap* h);
void* put(hashmap* h, uint32_t key, void* value);
void* get(hashmap* h, uint32_t key);

#endif

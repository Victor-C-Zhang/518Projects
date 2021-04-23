#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "datastructs_t.h"
#include "my_malloc.h"
/** linked list functions **/ 

void print_list(linked_list_t* list, void (*fptr)(void *)) {
  if (list == NULL) {return;}
  node_t* ptr = list->head;
  while (ptr != NULL) {
    (*fptr)(ptr ->data);
    ptr = ptr->next;
  }
  printf("\n");
}

void* get_head(linked_list_t* list) {
  if (list == NULL) {return NULL;}
  if (list-> head == NULL) {return NULL;}
  return list->head->data;
}

void* get_tail(linked_list_t* list) {
  if (list == NULL) {return NULL;}
  if (list-> tail == NULL) {return NULL;}
  return list->tail->data;
}

node_t* create_node(void* data) {
  node_t* node = (node_t*) myallocate(sizeof(node_t), __FILE__, __LINE__, LIBRARYREQ);
  node->data = data;
  node->next = NULL;
  return node;
}

linked_list_t* create_list() {
  linked_list_t* list = (linked_list_t*)myallocate(sizeof(linked_list_t), __FILE__, __LINE__, LIBRARYREQ);
  list->head = list->tail = NULL;
  return list;
}

void insert_head(linked_list_t* list, void* thing) {
  if (list == NULL) {return;}
  node_t* node = create_node(thing);
  if (list-> head == NULL) { 
    list->head = node;
    list->tail = node;
  } else {
    node->next = list->head;
    list->head = node;
  }
}


void insert_tail(linked_list_t* list, void* thing){
  if (list == NULL) {return;}
  node_t* node = create_node(thing);
  if (list->tail == NULL) {
    list->head = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
}

void* delete_head(linked_list_t* list) {
  if (list == NULL) {return NULL;}
  if (list-> head == NULL || list->head==0) {return NULL;}

  node_t* temp = list->head;   
  list -> head = list -> head -> next; 
  if (list->head == NULL) {
    list->tail = NULL;
  }
  void* d = temp->data;
  mydeallocate(temp, __FILE__, __LINE__, LIBRARYREQ);
  return d;
}

int isEmpty(linked_list_t *list) {
  return list->head == NULL;
}

void free_list(linked_list_t* list) {
  assert(list->head == NULL); // list should be empty to be destroyed
  mydeallocate(list, __FILE__, __LINE__, LIBRARYREQ);
}

/** hashmap functions **/
unsigned int hash(size_t num_buckets, unsigned int x) {
  register size_t i = sizeof(x);
  register unsigned int hv = 0; /* could put a seed here instead of zero */
  register const unsigned char *s = (const unsigned char *) &x;
  while (i--) {
    hv += *s++;
    hv += (hv << 10);
    hv ^= (hv >> 6);
  }
  hv += (hv << 3);
  hv ^= (hv >> 11);
  hv += (hv << 15);

  unsigned int hashval = hv & (num_buckets-1);
  return hashval;
}

static unsigned int hash_size(unsigned int s) {
  unsigned int i = 1;
  while (i < s) i <<= 1;
  return i;
}

int rehash(hashmap* h) {
  size_t new_size, i;
  hash_node** new_buckets = NULL;
  new_size = hash_size(h->entries * 2);
  new_buckets = calloc(new_size, sizeof(hash_node*));
  for (i = 0; i < h->num_buckets; i++) {
    hash_node* node = h->buckets[i];
    while (node) {
      hash_node* next = node->next;
      unsigned int index = hash(new_size, node->key);
      node->next = new_buckets[index];
      new_buckets[index] = node;
      node = next;
    }
  }

  h->num_buckets = new_size;
  free(h->buckets);
  h->buckets = new_buckets;
  return 1;
}

hash_node* create_hash_node(uint32_t key, void* value) {  
  hash_node* node = (hash_node*) calloc(1, sizeof(hash_node));
  node->key = key;
  node->value = value;
  node->next = NULL;
  return node;
}

hashmap* create_map() {
  hashmap* hm = (hashmap*) myallocate(sizeof(hashmap), __FILE__, __LINE__, LIBRARYREQ);
  memset(hm, '\000', sizeof(hashmap));
  hm->num_buckets = HASHSIZE;
  hm->entries = 0;
  hm->buckets = calloc(HASHSIZE, sizeof(hash_node*));
  return hm;
}

void free_map(hashmap* h) {
  for (size_t i = 0; i < h->num_buckets; i++) {
    hash_node* node = h->buckets[i];
    while (node) {
      hash_node* next = node->next;
      // context, linkedlists free'd earlier
      mydeallocate(node->value, __FILE__, __LINE__, LIBRARYREQ); //free tcb
      free(node); //calloc'd
      node = next;
    }
  }
  
  free(h->buckets);
  mydeallocate(h, __FILE__, __LINE__, LIBRARYREQ);
  return;
}

void* put(hashmap* h, uint32_t key, void* value){
  if (h == NULL) {return NULL;}
  unsigned int index = hash(h->num_buckets, key);
  hash_node* node = NULL;

  for (node = h->buckets[index]; node; node = node->next) {
    if (key == node->key) break;
  }

  if (node) {
    void* temp = node->value;
    node->value = value;
    return temp;
  }
  
  node = create_hash_node(key, value);
  node->next = h->buckets[index];
  h->buckets[index] = node;
  h->entries++;
  if ( (float)h->entries/(float)h->num_buckets > .75 ) rehash(h);
  return node->value;
}

void* get(hashmap* h, uint32_t key){
  if (h == NULL) {return NULL;}
  unsigned int index = hash(h->num_buckets, key);
  hash_node* node;
  for (node = h->buckets[index]; node; node = node->next) {
    if (key == node->key) break;
  }

  if (node) {
    return node->value;
  }

  return NULL;
}

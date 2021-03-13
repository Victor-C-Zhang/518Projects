// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:
#include <stdio.h>
#include <stdlib.h>
#include "my_pthread_t.h"

void print_list(linked_list_t* list, void (*fptr)(void *)){
	if (list == NULL) {return;}
	node_t* ptr = list->head;
	while (ptr != NULL) 
	{ 
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

void* get_tail(linked_list_t* list){
	if (list == NULL) {return NULL;}
	if (list-> tail == NULL) {return NULL;}
        return list->tail->data;
}

node_t* create_node(void* data){
	node_t* node = (node_t*) malloc(sizeof(node_t));
	node->data = data;
	node->next = NULL;
	return node;
}

linked_list_t* create_list() 
{ 
    linked_list_t* list = (linked_list_t*)malloc(sizeof(linked_list_t)); 
    list->head = list->tail = NULL; 
    return list; 
}

void insert_head(linked_list_t* list, void* thing){
	if (list == NULL) {return;}
	node_t* node = create_node(thing);
	if (list-> head == NULL) { 
		list -> head = node;
		list -> tail = node;
	}
	else {
		node -> next = list -> head;
		list -> head = node;
	}
}


void insert_tail(linked_list_t* list, void* thing){
	if (list == NULL) {return;}
	node_t* node = create_node(thing);
	if (list-> tail == NULL) { 
		list -> head = node;
		list -> tail = node;
	}
	else {
		list -> tail -> next = node;
		list -> tail = node;
	}
}

void* delete_head(linked_list_t* list) {
	if (list == NULL) {return NULL;}
	if (list-> head == NULL) {return NULL;}
  	node_t* temp = list->head; 
  
 	list -> head = list -> head -> next; 
	if (list->head == NULL){ 
		list->tail = NULL;
	}

    	free(temp);
	return temp->data;
}



// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:
#include <stdio.h>
#include <stdlib.h>
#include "my_pthread_t.h"

void printInt(void* data){
	printf("%d ", *(int *)data); 
}

void freeList(linked_list_t* list) {
	while (list->head != NULL) {
		delete_head(list);
	}
	free(list);
}

void testLinkedList() {
	linked_list_t* list = create_list();
	int i = 2;
	int i2 = 3;
	int i3 = 1;
	int i4 = 4;
	insert_head(list, (void*) &i);
	insert_tail(list, (void*) &i2);
	insert_head(list, (void*) &i3);
	insert_tail(list, (void*) &i4);
	print_list(list, printInt);
	printf("head: %d \n", *(int *)get_head(list)); 
	printf("tail: %d \n", *(int *)get_tail(list)); 
	freeList(list);
	return;
}

int main(int argc, char** argv){
	testLinkedList();
	return 0;
}

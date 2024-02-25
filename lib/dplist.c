/**
 * \author Carl Walleghem ---------- lab3
 */
//#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};

// ------------------------- hieronder
/**
typedef struct {
    int id;
    char* name;
} my_element_t;

void* element_copy(void * element);
void element_free(void ** element);
int element_compare(void * x, void * y);

void * element_copy(void * element) {
    my_element_t* copy = malloc(sizeof (my_element_t));
    char* new_name;
    asprintf(&new_name,"%s",((my_element_t*)element)->name);
    assert(copy != NULL);
    copy->id = ((my_element_t*)element)->id;
    copy->name = new_name;
    return (void *) copy;
}

void element_free(void ** element) {
    free((((my_element_t*)*element))->name);
    free(*element);
    *element = NULL;
}

int element_compare(void * x, void * y) {
    return ((((my_element_t*)x)->id < ((my_element_t*)y)->id) ? -1 : (((my_element_t*)x)->id == ((my_element_t*)y)->id) ? 0 : 1);
}
*/


// ------------------------ hierboven en GNU niet vergeten

dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

/******************************************************************************************************/

void dpl_free(dplist_t **list, bool free_element) {

    if(*list == NULL) return;
   
    if((*list)->head == NULL){    // check if there is a first element
    	free(*list);
    	*list = NULL;
    	return;
    	}
    	
    char finished = 0;
    dplist_node_t *ptrCopy = (*list)->head;
    (*list)->head = NULL;
	
    while(finished == 0){
    	if(ptrCopy->next == NULL){
    		if(free_element == true) (*list)->element_free(&ptrCopy->element);
    		free(ptrCopy);
    		finished = 1;
    		}
    	else{
    		dplist_node_t *nextCopy = ptrCopy->next;
    		if(free_element == true) (*list)->element_free(&ptrCopy->element);
    		free(ptrCopy);
    		ptrCopy = nextCopy;
    	}
    	}
    free(*list);
    *list = NULL; 

}

/******************************************************************************************************/

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {

    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;
    if (element == NULL) return list;
    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);
    if(insert_copy == true) list_node->element = list->element_copy(element);
    else list_node->element = element;
    
    // pointer drawing breakpoint
    if (list->head == NULL) { // covers case 1
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
        // pointer drawing breakpoint
    } else if (index <= 0) { // covers case 2
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
            // pointer drawing breakpoint
        }
    }
    return list;

}

/******************************************************************************************************/

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {

    if (list == NULL) return NULL;
    if ((list)->head == NULL) return list;
    if(index < 1){				// first element
    	if(list->head->next == NULL){
    		dplist_node_t * headCopy = list->head;
		if(free_element == true) list->element_free(&headCopy->element);
    		free(list->head);
    		list->head = NULL;
    		}
    	else{
    		dplist_node_t * headCopy = list->head;
    		list->head = list->head->next;
    		list->head->prev = NULL;
    		if(free_element == true) list->element_free(&headCopy->element);
    		free(headCopy);
    		}
    	return list;
    	}
    	
    if(index >= (dpl_size(list)-1)){          // last element
    	dplist_node_t * lastCopy = dpl_get_reference_at_index(list, index);
    	if(dpl_size(list) == 1){
    		list->head = NULL;
    		}
    	else{
    		lastCopy->prev->next = NULL;
    		}
    	if(free_element == true) list->element_free(&lastCopy->element);
    	free(lastCopy);
    	return list;
    	}
    	
    else{					// in between two elements
    	dplist_node_t * copy = dpl_get_reference_at_index(list, index);
    	copy->next->prev = copy->prev;
    	copy->prev->next = copy->next;
    	if(free_element == true) list->element_free(&copy->element);
    	free(copy);
    	return list;
    	}

}

/******************************************************************************************************/

int dpl_size(dplist_t *list) {

    if(list == NULL) return -1;
    if(list->head == NULL) return 0;

    int size = 1;
    dplist_node_t * headCopy = list->head;
    while(headCopy->next != NULL){
    	size++;
    	headCopy = headCopy->next;
    	}
    return size;

}

/******************************************************************************************************/

void *dpl_get_element_at_index(dplist_t *list, int index) {

    if(list == NULL) return 0;
    if(list->head == NULL) return 0;
    
    int size = dpl_size(list);
    if(index < 0) index = 0;
    if(index >= size) index = size-1;
    
    dplist_node_t * pointer = dpl_get_reference_at_index(list, index);
    void * element = pointer->element;
    return element;


}

/******************************************************************************************************/

int dpl_get_index_of_element(dplist_t *list, void *element) {

    if(list == NULL) return -1;
    if(list->head == NULL) return -1;
    if(element == NULL) return -1;
    int index = 0;
    char found = 0;
    dplist_node_t * nodePtr = list->head;
    
    while(found == 0){
    	if(list->element_compare(element, nodePtr->element)==0) found = 1;
    	else{
    		if(nodePtr->next == NULL) return -1;
    		index++;
    		nodePtr = nodePtr->next;
    	}
    }
    
    return index;

}

/******************************************************************************************************/

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
	
    if (list == NULL) return NULL;
    int count;
    dplist_node_t *dummy;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if (list->head == NULL) return NULL;
    for (dummy = list->head, count = 0; dummy->next != NULL; dummy = dummy->next, count++) {
        if (count >= index) return dummy;
    }
    return dummy;

}

/******************************************************************************************************/

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {

    if(list == NULL) return NULL;
    if(reference == NULL) return NULL;
    if(list->head == NULL) return NULL;
    
    int size = dpl_size(list);
    for(int i = 0; i<size; i++){
    	dplist_node_t *temp = dpl_get_reference_at_index(list,i);
    	if(reference == temp) return temp->element;
    	}
    return NULL;
}

/******************************************************************************************************/

dplist_node_t *dpl_get_first_reference(dplist_t *list){
	if(list == NULL) return NULL;
	if(list->head == NULL) return NULL;
	return dpl_get_reference_at_index(list,0);
	}
	
/******************************************************************************************************/

dplist_node_t *dpl_get_last_reference(dplist_t *list){
	if(list == NULL) return NULL;
	if(list->head == NULL) return NULL;
	int last = dpl_size(list)-1;
	return dpl_get_reference_at_index(list,last);
}

/******************************************************************************************************/

dplist_node_t *dpl_get_next_reference(dplist_t *list, dplist_node_t *reference){
	if(dpl_get_element_at_reference(list, reference) == NULL) return NULL;
	dplist_node_t *next = reference->next;
	return next;
}

/******************************************************************************************************/

dplist_node_t *dpl_get_previous_reference(dplist_t *list, dplist_node_t *reference){
	if(dpl_get_element_at_reference(list, reference) == NULL) return NULL;
	dplist_node_t *prev = reference->prev;
	return prev;
}

/******************************************************************************************************/

dplist_node_t *dpl_get_reference_of_element(dplist_t *list, void *element){
    if(list == NULL) return NULL;
    if(list->head == NULL) return NULL;
    if(element == NULL) return NULL;
    
    int size = dpl_size(list);
    for(int i = 0; i<size; i++){
    	if(dpl_get_element_at_index(list,i) == element){
    		return dpl_get_reference_at_index(list,i);
    		} 
    	}
    return NULL;
}

/******************************************************************************************************/

int dpl_get_index_of_reference(dplist_t *list, dplist_node_t *reference){
	if(list == NULL) return -1;
    	if(list->head == NULL) return -1;
	int index = 0;
	dplist_node_t *temp = list->head;
	
	while(temp != NULL){
		if(temp == reference) return index;
		index++;
		temp = dpl_get_next_reference(list,temp);
		}
	return -1;
}

/******************************************************************************************************/

dplist_t *dpl_insert_at_reference(dplist_t *list, void *element, dplist_node_t *reference, bool insert_copy){
	if(list == NULL) return NULL;
	if(reference == NULL) return NULL;
	int index = dpl_get_index_of_reference(list, reference);
	if(index == -1) return list;
	return dpl_insert_at_index(list, element, index, insert_copy);
}
	
/******************************************************************************************************/

dplist_t *dpl_remove_at_reference(dplist_t *list, dplist_node_t *reference, bool free_element){
	if(list == NULL) return NULL;
	if(reference == NULL) return NULL;
	int index = dpl_get_index_of_reference(list, reference);
	if(index == -1) return list;	
	return dpl_remove_at_index(list, index, free_element);
}
	
/******************************************************************************************************/

dplist_t *dpl_remove_element(dplist_t *list, void *element, bool free_element){
	if(list == NULL) return NULL;
	int index = dpl_get_index_of_element(list, element);
	if(index == -1) return list;
	return dpl_remove_at_index(list, index, free_element);
}
	
/**int main(void){
	dplist_t *list = dpl_create(&element_copy, element_free, element_compare);

  	my_element_t element2;
  	char var2 = 'Y';
  	element2.name = &var2;
 	element2.id = 1;
 	
 	my_element_t element3;
  	char var3 = 'X';
  	element3.name = &var3;
  	element3.id = 1;

  	my_element_t element4;
  	char var4 = 'Y';
  	element4.name = &var4;
 	element4.id = 1;
 	
  	list = dpl_insert_at_index(list, &element2, 99, true);
  	list = dpl_insert_at_index(list, &element3, 99, true);
  	list = dpl_insert_at_index(list, &element4, 99, true);
  	
  	dpl_remove_at_index(list,10,true);
  	dpl_remove_at_index(list,10,true);
  	dpl_remove_at_index(list,10,true);
  	printf("%d\n", dpl_size(list));
  	} */





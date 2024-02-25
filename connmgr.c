#define _GNU_SOURCE

/**
 * \author Carl Walleghem
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "config.h"
#include "connmgr.h"
#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include <assert.h>
#include <poll.h>

/**
 * Callback functions
 */

/*
void * element_copy(void * element){
	conn_element_t * copy = malloc(sizeof(conn_element_t));
	assert(copy != NULL);
	copy->sensor_id = ((conn_element_t*)element)->sensor_id;
	copy->tcpsock = ((conn_element_t*)element)->tcpsock;
	copy->update_time = ((conn_element_t*)element)->update_time;
	copy->poll_fd = ((conn_element_t*)element)->poll_fd;
	return (void*)copy;
}

void element_free(void ** element){ 
    free(*element);
    *element = NULL;
}

int element_compare(void * x, void * y){
	return 0;
};*/

/**
 * Server implementation
 */

void connmgr_listen(int port_number, sbuffer_t * sbuffer) {
	if(MISSING_ARGUMENT){
		printf("Missing argument: TIMEOUT not set\n");
		exit(EXIT_FAILURE);
		}
		
	FILE *fp;
	fp = fopen(FIFO_NAME,"w");
	if(fp == NULL){
		printf("error opening FIFO\n");
		exit(EXIT_FAILURE);
	}
	
	conn_element_t conn_mngr;
	struct pollfd conn_mngr_fd;
	connectionList = dpl_create(element_copy,element_free,element_compare);  //creating the linked list with connections
	
	tcpsock_t *server,*client;    // later reused in loop
    	if (tcp_passive_open(&server, port_number) != TCP_NO_ERROR){
    	 	printf("error creating server\n");
    	 	exit(EXIT_FAILURE);
    	}
    	if (tcp_get_sd(server,&(conn_mngr_fd.fd)) != TCP_NO_ERROR){
    		printf("error getting socket\n");
    	 	exit(EXIT_FAILURE);
    	}
    	
    	conn_mngr.sensor_id = 0;		// server gets sensor_id 0
    	conn_mngr.tcpsock = server;
    	conn_mngr.update_time = time(NULL);
    	conn_mngr.poll_fd = conn_mngr_fd;
    	conn_mngr.poll_fd.events = POLLIN | POLLRDHUP;
    	
    	dpl_insert_at_index(connectionList, (void*)&conn_mngr, -1, true); 
    	
    	bool exit_loop = false;
	int pollReady;
	int bytes, result;		// size of incoming data
	sensor_data_t data;		// stores incoming data

	do{
		for(int i = 0; i < dpl_size(connectionList); i++){
			conn_element_t * conn_element = ((conn_element_t*)dpl_get_element_at_index(connectionList, i));
			if(conn_element->sensor_id == 0){       									// connection manager code
				pollReady = poll(&(conn_element->poll_fd),1,0);
				if(pollReady == -1){
					printf("Error in connection manager poll");
					exit(EXIT_FAILURE); 
				}
				if(conn_element->poll_fd.revents == POLLIN){
					struct pollfd new_conn_fd;
					
					if(tcp_wait_for_connection(conn_element->tcpsock,&client) != TCP_NO_ERROR) exit(EXIT_FAILURE);
					if(tcp_get_sd(client, &(new_conn_fd.fd)) != TCP_NO_ERROR) exit(EXIT_FAILURE);
					
					conn_element_t new_element;
					new_element.tcpsock = client;
					new_element.sensor_id = -1;		// dummy value, will get updated
					new_element.poll_fd = new_conn_fd;
					new_element.poll_fd.events = POLLIN | POLLRDHUP; 
					new_element.update_time = time(NULL);
					dpl_insert_at_index(connectionList, (void*)&new_element, dpl_size(connectionList)+1, true);
					printf("There are now %d connection(s) \n", dpl_size(connectionList)-1);
					
					pthread_mutex_lock(getFifoMutex(&sbuffer));
					fprintf(fp, "A sensor node has opened a new connection\n");
					fflush(fp);
					pthread_mutex_unlock(getFifoMutex(&sbuffer));
					
					}
				
				if(dpl_size(connectionList) != 1) conn_element->update_time = time(NULL);
				if((time(NULL) - conn_element->update_time) > TIMEOUT){
					tcp_close(&(conn_element->tcpsock));
					#ifdef DEBUG
					printf("Server is shutting down\n");
					#endif
					data.id = 0;
	
					pthread_rwlock_wrlock(&(sbuffer->buffer_rwlock));
					sbuffer_insert(sbuffer, &data);
					pthread_rwlock_unlock(&(sbuffer->buffer_rwlock));
					pthread_cond_broadcast(&(sbuffer->read_cond));
					
					fclose(fp);
					connmgr_free();
					pthread_exit(0);
					}
				
			}
			else{
				if(time(NULL) - conn_element->update_time > TIMEOUT){
					tcp_close(&(conn_element->tcpsock));
					dpl_remove_at_index(connectionList, i, true);
					#ifdef DEBUG
					printf("Node timed out - there are now %d connection(s) left \n", dpl_size(connectionList)-1);
					#endif
					pthread_mutex_lock(getFifoMutex(&sbuffer));
					fprintf(fp, "Node timed out - there are now %d connection(s) left \n", dpl_size(connectionList)-1);
					fflush(fp);
					pthread_mutex_unlock(getFifoMutex(&sbuffer));
					
					break;
					}
												
				pollReady = poll(&(conn_element->poll_fd),1,0);			// sensor gets updated
				if(pollReady == -1){
					printf("error in node poll");
					exit(EXIT_FAILURE);
					}
					
				if(conn_element->poll_fd.revents == POLLRDHUP){
					tcp_close(&(conn_element->tcpsock));
					dpl_remove_at_index(connectionList, i, true);
					#ifdef DEBUG
					printf("Node disconnected - there are now %d connection(s) left \n", dpl_size(connectionList)-1);
					#endif	
					pthread_mutex_lock(getFifoMutex(&sbuffer));
					fprintf(fp, "The sensor node with %d has closed the connection\n", conn_element->sensor_id);
					fflush(fp);
					pthread_mutex_unlock(getFifoMutex(&sbuffer));	
					}
					
				if(conn_element->poll_fd.revents == POLLIN){				// new measurement
					bytes = sizeof(data.id);
					tcp_receive(conn_element->tcpsock, (void *) &data.id, &bytes);
					conn_element->sensor_id = data.id;					
					
					bytes = sizeof(data.value);
					tcp_receive(conn_element->tcpsock, (void *) &data.value, &bytes);
					
					bytes = sizeof(data.ts);
					result = tcp_receive(conn_element->tcpsock, (void *) &data.ts, &bytes);
					conn_element->update_time = time(NULL);
					
					if (result == TCP_NO_ERROR && bytes){
						#ifdef DEBUG
    						printf("New measurement by sensor %d\n", data.id);
    						#endif
						sensor_data_t * sensor = malloc(sizeof(sensor_data_t));
						sensor->id = data.id;
						sensor->value = data.value;
						sensor->ts = data.ts;
					
						pthread_rwlock_wrlock(&(sbuffer->buffer_rwlock));
						sbuffer_insert(sbuffer, sensor);
						pthread_rwlock_unlock(&(sbuffer->buffer_rwlock));
						pthread_cond_broadcast(&(sbuffer->read_cond));
						}
					}
				}

		}
	} while (exit_loop != true);
	
	// cleanup
	fclose(fp);
	connmgr_free();
	return;

}

void connmgr_free(){
	dpl_free(&connectionList, true);
}





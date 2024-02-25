 
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sqlite3.h>
#include <errno.h>
#include <string.h>
#include <wait.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "config.h"
#include "sensor_db.h"
#include "sbuffer.h"
#include "connmgr.h"
#include "datamgr.h"



#define LOG_NAME	"gateway.log"
#define MAX		100

void * connectionMngr(void * port_number);
void * dataMngr();
void * storeMngr();

static sbuffer_t * sbuffer;


int main(int argc, char *argv[]) {
	if(argc != 2){
		printf("Enter PORT-number as only argument \n");
		exit(EXIT_FAILURE);
		}
	int port = atoi(argv[1]);
	
	char buffer[MAX];
	int res;
	FILE * fp;
	pid_t child_pid;
	int child_exit_status;
	child_pid = fork();

	/********************************************************
	Child - Log process
	*********************************************************/
	if(child_pid == 0){
		#ifdef DEBUG
    		printf("Logging process has started. \n");
    		fflush(stdout);
    		#endif
		FILE * fp_log;
		int seqNr = 1;
	
		res = mkfifo(FIFO_NAME, 0666);
		if((res == -1) & (errno != EEXIST)){
			printf("error creating FIFO \n");
			exit(EXIT_FAILURE);
			}
		
		fp = fopen(FIFO_NAME, "r");
		fp_log = fopen(LOG_NAME, "w");
	
		if(fp_log == NULL){
			printf("error finding log file \n");
			exit(EXIT_FAILURE);
			}
	
  		do {
    			char * str = fgets(buffer, MAX, fp);
    			if ( strcmp(buffer, "0") == 0 ){
    				printf("Logging process will exit now \n");
    				fclose(fp);
    				fclose(fp_log);
    				exit(EXIT_SUCCESS);
    				}
    			else if ( str != NULL ){
    				fprintf(fp_log,"%d - %ld - %s", seqNr, time(NULL), buffer);
    				fflush(fp_log);
    				}
		      	seqNr++;
		      	
  		} while(1); 
	
	res = fclose( fp );
	#ifdef DEBUG
	printf("Logging process will exit now \n");
	#endif
	fclose(fp_log);	
	exit(EXIT_SUCCESS);
	}
	
	/********************************************************
	Parent - main code
	*********************************************************/
	else{
	
	// create threads and buffer
	
	if(sbuffer_init(&sbuffer)!=SBUFFER_SUCCESS){
		printf("error initiating buffer");
		exit(EXIT_FAILURE);
		} 
	pthread_t thread_con, thread_data, thread_store;
	 
	res = mkfifo(FIFO_NAME, 0666);
	if((res == -1) & (errno != EEXIST)){
		printf("error creating FIFO \n");
		exit(EXIT_FAILURE);
		}
	
	fp = fopen(FIFO_NAME, "w");
	
	snprintf(buffer, sizeof(buffer),"Main program has started \n");
	if ( fputs( buffer, fp ) == EOF ){
      		fprintf( stderr, "Error writing data to fifo\n");
      		exit( EXIT_FAILURE );
    		}
    	fflush(fp);

	
	// Start threads
	pthread_create(&thread_con, NULL, connectionMngr, &port);
	pthread_create(&thread_data, NULL, dataMngr, NULL);
	pthread_create(&thread_store, NULL, storeMngr, NULL);
	#ifdef DEBUG
	printf("Threads have been created. \n");
	#endif
	
	pthread_join(thread_con, NULL);
	#ifdef DEBUG
	printf("Connection manager thread has returned. \n");
	#endif
	pthread_join(thread_data, NULL);
	#ifdef DEBUG
	printf("Datamanager thread has returned. \n");
	#endif
	pthread_join(thread_store, NULL);
	#ifdef DEBUG
	printf("Storage manager thread has returned. \n");
	#endif
	
	fprintf(fp, "0");	// to terminate the fifo
	fflush(fp);
	fclose(fp);
	
	// wait for child
	wait(&child_exit_status);
	sbuffer_free(&sbuffer);
	exit(EXIT_SUCCESS);
	}
}


void * connectionMngr(void * port_number){
	int * port = (int*) port_number;
	connmgr_listen(*port, sbuffer);
	
	return NULL;
}

void * dataMngr(void * arg){
	FILE * roomSens = fopen("room_sensor.map","r");
	datamgr_parse_sensor_files(roomSens, sbuffer);       // start data manager
	fclose(roomSens);
	return NULL;
}

void * storeMngr(void * arg){		
	runStorageMngr(sbuffer);
	return NULL;
	}













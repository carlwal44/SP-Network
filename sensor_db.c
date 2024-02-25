#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "sqlite3.h"
#include "sensor_db.h"
#include <unistd.h>
#include "sbuffer.h"

void runStorageMngr(sbuffer_t * sbuffer){
	const char flag = 1;
	uint16_t sensor;
	double temperature;
	time_t time;
	
	FILE *fp;
	fp = fopen(FIFO_NAME,"w");
	if(fp == NULL){
		printf("error opening FIFO\n");
		exit(EXIT_FAILURE);
	}
	
	DBCONN * init = init_connection(flag);
	if(init == NULL){
		pthread_mutex_lock(getFifoMutex(&sbuffer));
		fprintf(fp, "Unable to connect to SQL server\n");
		fflush(fp);
		pthread_mutex_unlock(getFifoMutex(&sbuffer));
		exit(EXIT_FAILURE);	
	}
	
	pthread_mutex_lock(getFifoMutex(&sbuffer));
	fprintf(fp, "Connection to SQL server established.\n");
	fprintf(fp, "New table %s created.\n", TO_STRING(TABLE_NAME));
	fflush(fp);
	pthread_mutex_unlock(getFifoMutex(&sbuffer));	
	
	while(1){
		pthread_mutex_lock(&(sbuffer->condition_mutex));
		pthread_cond_wait(&(sbuffer->read_cond), &(sbuffer->condition_mutex));
		pthread_mutex_unlock(&(sbuffer->condition_mutex));
		
		pthread_rwlock_rdlock(&(sbuffer->buffer_rwlock));
		//**************************************************** DATA ACCESSIBLE AFTER RWLOCK **********************************************************************
		if(sbuffer->tail->data.id == 0){
				disconnect(init);
				fclose(fp);
				pthread_exit(0);
				}
		sbuffer_node_t * node = sbuffer->head;
		while(node->next != NULL){
			temperature = node->data.value;
			time = node->data.ts;
			sensor = node->data.id;
			
			if(insert_sensor(init, sensor, temperature, time) != 0){
				pthread_mutex_lock(getFifoMutex(&sbuffer));
				fprintf(fp, "Error inserting sensor into database\n");
				fflush(fp);
				pthread_mutex_unlock(getFifoMutex(&sbuffer));
				}
			node = node->next;
		}
		
		pthread_barrier_wait(&(sbuffer->read_barrier));
		
		//**************************************************** DATA INACCESSIBLE AFTER RWUNLOCK **********************************************************************
		pthread_rwlock_unlock(&(sbuffer->buffer_rwlock));
		
		}
	
	
	fclose(fp);
	}
	
DBCONN *init_connection(char clear_up_flag){
	sqlite3 * db;
	char *err_msg = 0;
	int rc = sqlite3_open(TO_STRING(DB_NAME), &db);
	if (rc != SQLITE_OK) {
		sleep(0.1);
		rc = sqlite3_open(TO_STRING(DB_NAME), &db);
		if(db != NULL){;
			sleep(0.1);
			rc = sqlite3_open(TO_STRING(DB_NAME), &db);
			if(db == NULL){
    				fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    				sqlite3_close(db);
    				return NULL;
    				}
    			}
	}
	if(clear_up_flag == 1){
		char *clear = "DROP TABLE IF EXISTS " TO_STRING(TABLE_NAME);
		sqlite3_exec(db, clear, 0, 0, &err_msg);   
		char *sql = "CREATE TABLE " TO_STRING(TABLE_NAME) " (id INTEGER PRIMARY KEY AUTOINCREMENT, sensor_id INT, sensor_value DECIMAL(4,2), timestamp TIMESTAMP);";

		rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	}
	if (rc != SQLITE_OK ) {
        
        fprintf(stderr, "Failed to create table\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        
    	} else {
        #ifdef DEBUG
        fprintf(stdout, "Table " TO_STRING(TABLE_NAME) " created successfully\n");
    	#endif
    	}
    	return db;
}

void disconnect(DBCONN *conn){
	sqlite3_close(conn);
	}
	
int insert_sensor(DBCONN *conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
	char sql[200];
	char *err_msg = 0;
	snprintf(sql,200,"INSERT INTO %s(sensor_id, sensor_value, timestamp) VALUES(%hu,%f,%ld)",TO_STRING(TABLE_NAME),id,value,ts);
	int rc = sqlite3_exec(conn, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK ) {
        	fprintf(stderr, "Failed to update table\n");
        	fprintf(stderr, "SQL error: %s\n", err_msg);
        	sqlite3_free(err_msg);
        	return 1;
    	}

	return 0;
}






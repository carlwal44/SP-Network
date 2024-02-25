/**
 * \author Carl Walleghem
 */
#define _GNU_SOURCE
#include "datamgr.h"
#include "lib/dplist.h"
#include "config.h"
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "sbuffer.h"

#define INVALID_ENTRY "1"
#define INVALID_FILE_SENSOR "2"

void* element_copy(void * element);
void element_free(void ** element);
int element_compare(void * x, void * y);

dplist_t *list = NULL;

void datamgr_parse_sensor_files(FILE *fp_sensor_map, sbuffer_t * sbuffer){

	FILE *fp;
	fp = fopen(FIFO_NAME,"w");
	if(fp == NULL){
		printf("error opening FIFO\n");
		exit(EXIT_FAILURE);
	}
	
	uint16_t room;
	uint16_t sensor;
	list = dpl_create(&element_copy, element_free, element_compare);
	while(fscanf(fp_sensor_map, "%hu %hu", &room, &sensor)==2){
		sensor_data_t * sensorNode = malloc(sizeof(sensor_data_t));
		sensorNode->roomID = room;
		sensorNode->id = sensor;
		for(int i = 0; i < RUN_AVG_LENGTH; i++){
			sensorNode->average[i] = 0;
		}
		int size = dpl_size(list) + 10;    // just to insert at end of list
		dpl_insert_at_index(list,sensorNode,size,false);
		}
	rewind(fp_sensor_map);
	time_t time;
	double temperature;
	sensor_data_t * sensorNode = NULL;
	int size = dpl_size(list);
	
	while(1){
		pthread_mutex_lock(&(sbuffer->condition_mutex));
		pthread_cond_wait(&(sbuffer->read_cond), &(sbuffer->condition_mutex));
		pthread_mutex_unlock(&(sbuffer->condition_mutex));
		
		pthread_rwlock_rdlock(&(sbuffer->buffer_rwlock));

		//**************************************************** DATA ACCESSIBLE AFTER RWLOCK **********************************************************************
		sbuffer_node_t * node = sbuffer->head;
		if(sbuffer->tail->data.id == 0){
				fclose(fp);
				datamgr_free();
				//pthread_exit(0);
				return;
				}
		while(node->next != NULL){
			temperature = node->data.value;
			time = node->data.ts;
			sensor = node->data.id;
			#ifdef DEBUG
			printf("temp is %f and sensor id is %d\n", temperature, sensor);
			fflush(stdout);
			#endif


			for(int i=0; i<size; i++){				// find sensor
				sensorNode = (sensor_data_t *) dpl_get_element_at_index(list,i);
				if(sensorNode->id == sensor) i = size;   // escape loop - replace with a break maybe
			}
		
			if(sensorNode->id != sensor){
			printf("Error: sensor not found\n");
			pthread_mutex_lock(getFifoMutex(&sbuffer));
			fprintf(fp, "Received sensor data with invalid sensor node ID %d \n", sensorNode->id);
			fflush(fp);
			pthread_mutex_unlock(getFifoMutex(&sbuffer));
			}
		
			sensorNode->ts = time;
			for(int j=0; j<RUN_AVG_LENGTH-1; j++){
				sensorNode->average[j] = sensorNode->average[j+1];
			} 
			sensorNode->average[RUN_AVG_LENGTH-1] = temperature;
			sensor_value_t curAvg = 0;
			int zeroesPresent = 0;
			for(int k=0; k<RUN_AVG_LENGTH; k++){		// compute value for sensor
					curAvg += sensorNode->average[k];
					if(sensorNode->average[k] == 0){
						zeroesPresent++;
						}

			}
			if(zeroesPresent == RUN_AVG_LENGTH){
				sensorNode->value = 0;
				}
			else{
				sensorNode->value = curAvg/(RUN_AVG_LENGTH-zeroesPresent);
				if(sensorNode->value > SET_MAX_TEMP){
					pthread_mutex_lock(getFifoMutex(&sbuffer));
					fprintf(fp, "The sensor node with %d reports it’s too hot (running avg temperature = %f) \n", (sensorNode->id), (sensorNode->value));
					fflush(fp);
					pthread_mutex_unlock(getFifoMutex(&sbuffer));
					}
				if(sensorNode->value < SET_MIN_TEMP){
					pthread_mutex_lock(getFifoMutex(&sbuffer));
					fprintf(fp, "The sensor node with %d reports it’s too cold (running avg temperature = %f) \n", (sensorNode->id), (sensorNode->value));
					fflush(fp);
					pthread_mutex_unlock(getFifoMutex(&sbuffer));
					}
				}
			node = node->next;
		}
		//**************************************************** DATA INACCESSIBLE AFTER RWUNLOCK **********************************************************************
		pthread_barrier_wait(&(sbuffer->read_barrier));
		
		node = sbuffer->head;
		sbuffer_node_t * copy = node;
		while(node->next != NULL){
			node = node->next;
			sbuffer_remove(sbuffer, &(copy->data));
			copy = node;	
		}
		pthread_rwlock_unlock(&(sbuffer->buffer_rwlock));
	}
	
	free(sensorNode);
	datamgr_free();
}

void datamgr_free(){
	dpl_free(&list,true);
	}

//-------------------------------------------------------------------------------------------------------/
// callback functions
//-------------------------------------------------------------------------------------------------------/



void * element_copy(void * element) {
    //element = (sensor_data_t)
    sensor_data_t* copy = malloc(sizeof (sensor_data_t));
    assert(copy != NULL);
    copy->id = ((sensor_data_t*)element)->id;
    copy->ts = ((sensor_data_t*)element)->ts;
    for(int i=0; i<RUN_AVG_LENGTH; i++){
    	copy->average[i] = ((sensor_data_t*)element)->average[i];
    }
    copy->value = ((sensor_data_t*)element)->value;
    copy->roomID = ((sensor_data_t*)element)->roomID;
    return (void *) copy;
}

void element_free(void ** element) {
    free(*element);
    *element = NULL;
}

int element_compare(void * x, void * y) {
    return ((((sensor_data_t*)x)->id < ((sensor_data_t*)y)->id) ? -1 : (((sensor_data_t*)x)->id == ((sensor_data_t*)y)->id) ? 0 : 1);
} 








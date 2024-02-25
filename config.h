/**
 * \author Carl Walleghem
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif

#ifndef FIFO_NAME
#define FIFO_NAME	"logFifo" 
#endif

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>


typedef uint16_t sensor_id_t;
typedef double sensor_value_t;
typedef uint16_t room_id_t;
typedef time_t sensor_ts_t;         // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine

typedef struct {
    sensor_id_t id;         /** < sensor id */
    room_id_t roomID;	     
    sensor_value_t value;   /** < sensor value */
    sensor_ts_t ts;         /** < sensor timestamp */
    double average[RUN_AVG_LENGTH];   /** last x values */
    
} sensor_data_t;

typedef struct sbuffer_node {
    sensor_data_t data;
    struct sbuffer_node* next;
} sbuffer_node_t;

typedef struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
    pthread_mutex_t fifoMutex;
    
    pthread_cond_t read_cond;	// signal to reader threads
    pthread_rwlock_t buffer_rwlock;		// read/write permission
    pthread_barrier_t read_barrier;
    pthread_mutex_t condition_mutex;
} sbuffer_t;


#endif /* _CONFIG_H_ */

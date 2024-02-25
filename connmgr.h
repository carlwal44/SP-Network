/**
 * \author Carl Walleghem
 */
 
 #ifndef CONNMGR
 #define CONNMGR

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "config.h"
#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include <poll.h>
#include "sbuffer.h"

#ifndef TIMEOUT				
#define MISSING_ARGUMENT 1
#define TIMEOUT 0
#else
#define MISSING_ARGUMENT 0					
#endif


/**
 * Implements a sequential test server (only one connection at the same time)
 */

void connmgr_listen(int port_number, sbuffer_t * sbuffer);

void connmgr_free();

dplist_t * connectionList;	

/**
 * Callback functions and structs
 */
 
typedef struct {
    int sensor_id;
    tcpsock_t * tcpsock;
    time_t update_time;
    struct pollfd poll_fd;
} conn_element_t;

void* element_copy(void * element);
void element_free(void ** element);
int element_compare(void * x, void * y);

#endif



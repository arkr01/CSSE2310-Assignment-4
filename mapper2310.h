#ifndef MAPPER_2310_H
#define MAPPER_2310_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "errors.h"
#include "general.h"

/* The mapper will store airports. Let us allow 10 airports to be stored as a
 * start, and reallocate memory if more airports are to be stored. */
#define INITIAL_NUM_AIRPORTS 10

/* The mapper will store connection information for each client. Let us allow
 * information of upto 15 connections to be stored initially, and reallocate
 * memory if more connections are made. */
#define INITIAL_NUM_CONNECTIONS 15

/* Initialise port numbers of airports to 0. Set to valid port after added by
 * user. */
#define INVALID_PORT 0

/* Client Commands Types */
typedef enum {
    GET_PORT_NUMBER = 1,
    ADD_AIRPORT = 2,
    GET_AIRPORTS = 3,
    ERROR = 4
} CommandType;

/* Airport representation */
typedef struct {
    char* id;
    uint16_t portNum;
} Airport;

/* Connection Information Representation */
typedef struct {
    Airport** airports;
    int* numAirports;
    sem_t* guard;
    int connectionWrite;
} ConnectionInfo;

/* Takes in the listening socket. Accepts connections and acts as entry point
 * for client-server communication. This function should ideally never return
 * as it will continuously accept connections, thus running until killed. It
 * may return prematurely should any error(s) arise, at which point the
 * program will terminate.
 *
 * NOTE: Although this function bears striking resemblance to handle_planes()
 * in control2310.?, due to the fundamentally different designs between the
 * ConnectingPlane and ConnectionInfo (from mapper2310) data structures,
 * particularly regarding the differences in the members fields, generalising
 * this function proved to be, although doable, much messier and higher
 * coupled in design compared to creating two separate similar functions.
 * General files required dependencies on specific files, defeating the
 * purpose of generalisation.
 *
 * Function Pointers, void pointers, and several flags (to check which program
 * called said function and what the function should do in the given case)
 * were explored, and overall it was deemed to be better design to keep these
 * functions separate. */
void handle_connections(int serverEnd);

/* Takes in the connection information representations, the number of said
 * representations, and the thread lock. If required, reallocates more memory
 * to store more connection information representations. */
void resize_connections(ConnectionInfo** connections, int* numConnections,
	sem_t* lock);

/* Takes in a connection's information representation. Listens to a specific
 * client for any commands and processes them accordingly.
 *
 * NOTE: Although this function bears striking resemblance to each_plane() in
 * control2310.?, due to the fundamentally different designs between the
 * ConnectingPlane and ConnectionInfo (from mapper2310) data structures,
 * particularly regarding the differences in the members fields, generalising
 * this function proved to be, although doable, much messier and higher
 * coupled in design compared to creating two separate similar functions.
 * General files required dependencies on specific files, defeating the
 * purpose of generalisation.
 *
 * Function Pointers, void pointers, and several flags (to check which program
 * called said function and what the function should do in the given case)
 * were explored, and overall it was deemed to be better design to keep these
 * functions separate. */
void* each_connection(void* thisConnection);

/* Takes in the client's command, this connection's information
 * representation, and this connection's network output stream. Executes the
 * appropriate action based on the client's command. (Entry point for all
 * command processing and validation). */
void process_command(char* command, ConnectionInfo* thisConnection,
	FILE** writeEnd);

/* Takes in a command from the client. Validates the command and returns the
 * appropriate type. */
CommandType get_command_type(char* command);

/* Takes in the thread lock and the initial number of connection information
 * representations. Allocates memory for each connection information
 * representation and returns said representations. */
ConnectionInfo* init_connections(sem_t* lock, int numConnections);

/* Takes in the airports. Allocates memory for each airport representation. */
void init_airports(Airport** airports);

/* Takes in this connection's information representation, and the command to
 * add a new airport. Adds said airport and reallocates memory in case more
 * than *(thisConnection->numAirports) airports are to be stored. */
void add_airport(ConnectionInfo* thisConnection, char* command);

/* Helper function for add_airport(). Takes in this connection's information
 * representation, the id of the airport to be added, the length of the id to
 * be added, and the port number of the airport to be added. Reallocates more
 * memory to store the new airport and adds said airport. */
void resize_and_add_airport(ConnectionInfo* thisConnection, char* idToAdd,
	int idLength, int portNumberToAdd);

/* Takes in this connection's information representation, and the write end of
 * the network communication. Displays the airports in lexicographic order of
 * the airport IDs (after being sorted). */
void display_airports(ConnectionInfo* thisConnection, FILE** writeEnd);

/* Takes in this connection's information representation. Sorts the airports
 * in lexicographic order of the airport IDs. */
void sort_airports(ConnectionInfo* thisConnection);

/* Takes in this connection's information representation, and the airport ID
 * of the port number in question. Returns the port number of the airport
 * requested. If no such airport exists, returns INVALID_PORT. */
int get_port_number(ConnectionInfo* thisConnection, char* idOfPort);

#endif

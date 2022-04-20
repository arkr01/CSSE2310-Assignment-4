#ifndef CONTROL_2310_H
#define CONTROL_2310_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include "errors.h"
#include "general.h"

/* The minimum number of command arguments that the control program can
 * take. */
#define MIN_NUM_COMMAND_LINE_ARGS 3

/* The maximum number of command arguments that the control program can
 * take. */
#define MAX_NUM_COMMAND_LINE_ARGS 4

/* Used to index argv for the Airport ID. */
#define ID 1

/* Used to index argv for the Airport info. */
#define INFO 2

/* Used to index argv for the mapper port. */
#define MAPPER_PORT 3

/* The control will store connection information about each connecting plane.
 * Let us allow information of upto 15 planes to be stored initially, and
 * reallocate memory if more planes connect to this airport. */
#define INITIAL_NUM_PLANES 15

/* A plane may connect to the control multiple times. The control will store
 * connection information about the plane each time it connects. Let us allow
 * the plane to connect 10 times initially and reallocate memory should it
 * connect more than 10 times. */
#define INITIAL_NUM_PLANE_IDS 10

/* Connecting Plane Representation */
typedef struct {
    char*** planeIds;
    char* controlInfo;
    int* numPlaneIds; // A plane can connect multiple times
    sem_t* guard;
    int connectionWrite;
} ConnectingPlane;

/* Takes in this airport's (validated) ID, the mapper's port, and this
 * airport's port number. This function connects to the mapper, registers the
 * ID and port of this airport, and returns the appropriate exit code. */
ControlExitCodes register_with_mapper(char* id, char* mapperPort,
	int thisPortNumber);

/* Takes in the listening socket and this airport's information. Waits for and
 * acts on incoming connections by planes. This function should ideally never
 * return as it will continuously accept plane connections, thus running until
 * killed. It may return prematurely should any error(s) arise, in which case
 * the program will terminate. 
 *
 * NOTE: Although this function bears striking resemblance to
 * handle_connections() in mapper2310.?, due to the fundamentally different
 * designs between the ConnectingPlane and ConnectionInfo (from mapper2310)
 * data structures, particularly regarding the differences in the members
 * fields, generalising this function proved to be, although doable, much
 * messier and higher coupled in design compared to creating two separate
 * similar functions. General files required dependencies on specific files,
 * defeating the purpose of generalisation.
 *
 * Function Pointers, void pointers, and several flags (to check which program
 * called said function and what the function should do in the given case)
 * were explored, and overall it was deemed to be better design to keep these
 * functions separate. */
void handle_planes(int serverEnd, char* controlInfo);

/* Takes in the thread lock, the initial number of planes that can connect to
 * this airport, and the info about this airport. Allocates memory for each
 * connecting plane representation and returns said representations. */
ConnectingPlane* init_connecting_planes(sem_t* lock, int numPlanes,
	char* controlInfo);

/* Takes in the connecting plane representations, the number of said
 * representations, and the thread lock. If required, reallocates more memory
 * to store more connecting plane representations. */
void resize_connecting_planes(ConnectingPlane** planes, int* numPlanes,
	sem_t* lock);

/* Takes in a connecting plane's representation. Listens and processes any
 * commands given by the plane.
 *
 * NOTE: Although this function bears striking resemblance to
 * each_connection() in mapper2310.?, due to the fundamentally different
 * designs between the ConnectingPlane and ConnectionInfo (from mapper2310)
 * data structures, particularly regarding the differences in the members
 * fields, generalising this function proved to be, although doable, much
 * messier and higher coupled in design compared to creating two separate
 * similar functions. General files required dependencies on specific files,
 * defeating the purpose of generalisation.
 *
 * Function Pointers, void pointers, and several flags (to check which program
 * called said function and what the function should do in the given case)
 * were explored, and overall it was deemed to be better design to keep these
 * functions separate. */
void* each_plane(void* thisPlane);

/* Takes in the plane's command, the plane's representation, and the output
 * stream of the connection with said plane. Executes the appropriate action
 * based on the given command. Entry point for all command processing. NOTE:
 * this function will terminate the program if an invalid plane ID is given */
void handle_command(char* command, ConnectingPlane* thisPlane,
	FILE** writeEnd);

/* Takes in the connecting plane's representation, and the write end of the
 * network communication. Displays the plane IDs in lexicographic order. */
void display_plane_ids(ConnectingPlane* thisPlane, FILE** writeEnd);

/* Takes in a connecting plane's representation and sorts all plane IDs known
 * by control in lexicographic order. */
void sort_plane_ids(ConnectingPlane* thisPlane);

/* Takes in this connecting plane's representation and the id of the plane.
 * Adds said plane id and reallocates more memory if required. */
void add_plane_id(ConnectingPlane* thisPlane, char* planeIdToAdd);

/* Helper function for add_plane_id(). Takes in a connecting plane's
 * representation and the plane ID of the new (or existing) plane to be added.
 * Reallocates more memory to store the new (or existing) plane ID and adds
 * said plane ID. NOTE: When referring to existing plane IDs, this refers to
 * the fact that a plane may connect to this airport several times. */
void resize_and_add_plane_id(ConnectingPlane* thisPlane, char* planeIdToAdd);

#endif

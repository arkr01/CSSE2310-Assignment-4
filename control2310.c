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
#include "control2310.h"

int main(int argc, char** argv) {
    // Squash SIGPIPE to prevent issues arising from sudden termination of
    // client(s)
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = SIG_IGN; // Ignore the siganl
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, NULL);

    if (argc < MIN_NUM_COMMAND_LINE_ARGS ||
	    argc > MAX_NUM_COMMAND_LINE_ARGS) {
	return control_error_message(CONTROL_ARGS);
    }
    if (check_invalid_chars(argv[ID]) || check_invalid_chars(argv[INFO])) {
	return control_error_message(CONTROL_CHAR);
    }
    bool mapperProvided = false;
    if (argc == MAX_NUM_COMMAND_LINE_ARGS) {
	mapperProvided = true;
	char* mapperPortErrors;
	int mapperPortNumber = strtol(argv[MAPPER_PORT], &mapperPortErrors,
		10);

	if (strtol_invalid(argv[MAPPER_PORT], mapperPortErrors) ||
		mapperPortNumber > PORT_MAX ||
		mapperPortNumber < PORT_MIN) {
	    return control_error_message(CONTROL_PORT);
	}
    }
    uint16_t thisPortNumber;
    int* serverEnd = setup_server(&thisPortNumber);
    if (serverEnd == NULL) {
	return UNSPECIFIED_ERROR;
    }

    if (mapperProvided) {
	ControlExitCodes mapperReturn =
		register_with_mapper(argv[ID], argv[MAPPER_PORT],
		thisPortNumber);
	if (mapperReturn != CONTROL_NORMAL) {
	    free(serverEnd);
	    return control_error_message(mapperReturn);
	}
    }
    // wait for and act on plane connections
    handle_planes(*serverEnd, argv[INFO]);

    free(serverEnd);
    // Should never reach here - control should run until killed
    return UNSPECIFIED_ERROR;
}

ControlExitCodes register_with_mapper(char* id, char* mapperPort,
	int thisPortNumber) {
    int thisEnd; // stores mapper socket

    // Used to differentiate behaviour of functions based on which program(s)
    // are calling said functions
    bool controlCalled = true;

    ControlExitCodes mapperError = setup_client(mapperPort, &thisEnd,
	    controlCalled);
    if (mapperError == CONTROL_MAPPER) {
	return mapperError;
    }

    FILE* toWrite = fdopen(thisEnd, "w");
    
    // Ensure fdopen() succeeded
    if (!toWrite) {
	return CONTROL_MAPPER;
    }
    fprintf(toWrite, "!%s:%d\n", id, thisPortNumber);
    fflush(toWrite);
    fclose(toWrite);
    return CONTROL_NORMAL;
}

void handle_planes(int serverEnd, char* controlInfo) {
    int connectionWrite; // file descriptor for accepted socket

    sem_t* lock = (sem_t*)malloc(sizeof(sem_t));
    int numPlanes = INITIAL_NUM_PLANES;
    ConnectingPlane* planes = init_connecting_planes(lock, numPlanes,
	    controlInfo);

    // Ensures each thread gets a unique semaphore
    int currentPlane = 0;
    sem_init(lock, SHARED_BETWEEN_THREADS, 1);

    while (connectionWrite = accept(serverEnd, NULL, NULL),
	    connectionWrite >= 0) { // Ensure accept() succeeded

	// If numConnections is exceeded, allocate more memory (-1 for
	// indexing purposes)
	if (currentPlane == numPlanes - 1) {
	    numPlanes++;
	    resize_connecting_planes(&planes, &numPlanes, lock);
	    if (numPlanes == ERROR_RETURN) {
		return; // realloc() failed
	    }
	}
	(planes[currentPlane]).connectionWrite = connectionWrite;

	pthread_t threadId;
	pthread_attr_t attributes;

	if (pthread_attr_init(&attributes) ||
		pthread_attr_setdetachstate(&attributes,
		PTHREAD_CREATE_DETACHED) ||
		pthread_create(&threadId, &attributes, each_plane,
		planes + currentPlane) ||
		pthread_attr_destroy(&attributes)) {
	    sem_destroy(lock);
	    free(lock);
	    return;
	}
	currentPlane++;
    }
    sem_destroy(lock);
    free(lock);
}

ConnectingPlane* init_connecting_planes(sem_t* lock, int numPlanes,
	char* controlInfo) {
    ConnectingPlane* planes =
	    (ConnectingPlane*)malloc(numPlanes * sizeof(ConnectingPlane));

    int* numPlaneIds = (int*)malloc(sizeof(int));
    *numPlaneIds = INITIAL_NUM_PLANE_IDS;

    // Form a pointer to an array of plane IDs (strings), such that if one
    // thread resizes the array, the other threads point to the resized array
    char*** planeIds = (char***)malloc(sizeof(char**));
    *planeIds = (char**)malloc(sizeof(char*) * (*numPlaneIds));

    for (int id = 0; id < *numPlaneIds; id++) {
	(*planeIds)[id] = (char*)malloc(sizeof(char) * INITIAL_BUFFER_SIZE);
	((*planeIds)[id])[0] = '\0';
    }

    for (int plane = 0; plane < numPlanes; plane++) {
	// all connecting planes should have access to the same plane info so
	// that each connection can update information (e.g. add a new plane)
	// and all other connections will register any changes
	(planes[plane]).planeIds = planeIds;
	(planes[plane]).controlInfo = controlInfo;
	(planes[plane]).numPlaneIds = numPlaneIds;
	(planes[plane]).guard = lock;
    }
    return planes;
}

void resize_connecting_planes(ConnectingPlane** planes, int* numPlanes,
	sem_t* lock) {
    void* morePlanes =
	    realloc(*planes, (*numPlanes) * sizeof(ConnectingPlane));

    if (!morePlanes) {
	*numPlanes = ERROR_RETURN;
	return; // realloc() failed
    }
    *planes = morePlanes;

    // after reallocating memory, provide the new connecting plane with the
    // required information. NOTE: all connecting planes point to the same
    // plane ID array, hence we pick any connecting plane (in this case the
    // first)
    ((*planes)[*numPlanes - 1]).planeIds = ((*planes)[0]).planeIds;
    ((*planes)[*numPlanes - 1]).controlInfo = ((*planes)[0]).controlInfo;
    ((*planes)[*numPlanes - 1]).numPlaneIds = ((*planes)[0]).numPlaneIds;
    ((*planes)[*numPlanes - 1]).guard = lock;
}

void* each_plane(void* thisPlane) {
    ConnectingPlane* thisPlaneOriginal = (ConnectingPlane*)thisPlane;

    int connectionRead = dup(thisPlaneOriginal->connectionWrite);
    
    // Ensure dup() succeeded
    if (connectionRead == ERROR_RETURN) {
	return NULL;
    }

    FILE* readEnd = fdopen(connectionRead, "r");
    FILE* writeEnd = fdopen(thisPlaneOriginal->connectionWrite, "w");
    
    // Ensure fdopen() succeeded
    if (!readEnd || !writeEnd) {
	return NULL;
    }
    
    size_t commandLength = INITIAL_BUFFER_SIZE;
    char* command = (char*)malloc(commandLength * sizeof(char));

    while (get_line(&command, &commandLength, readEnd),
	    strlen(command) != 0) {
	// Apart from the lock, only the plane IDs shared, hence only
	// processing commands (thus consequently manipulating the plane IDs)
	// requires the lock as each thread has its own socket
	sem_wait(thisPlaneOriginal->guard);

	handle_command(command, thisPlaneOriginal, &writeEnd);
	
	if (*(thisPlaneOriginal->numPlaneIds) == ERROR_RETURN) {
	    free(command);
	    fflush(writeEnd);
	    fclose(writeEnd);
	    fclose(readEnd);
	    sem_post(thisPlaneOriginal->guard);
	    return NULL; // realloc() failed, return prevents segfault
	}
	sem_post(thisPlaneOriginal->guard);
    }
    free(command);
    fflush(writeEnd);
    fclose(writeEnd);
    fclose(readEnd); 
    return NULL;
}

void handle_command(char* command, ConnectingPlane* thisPlane,
	FILE** writeEnd) {
    if (!strcmp(command, "log")) {
	display_plane_ids(thisPlane, writeEnd);
    } else if (check_invalid_chars(command)) {
	// Invalid chars found in plane ID. Handling this is unspecified in
	// the spec however Joel mentioned to simply exit in this case.
	ControlExitCodes invalidPlaneId = control_error_message(CONTROL_CHAR);
	exit(invalidPlaneId);
    } else {
	add_plane_id(thisPlane, command);
	fprintf(*writeEnd, "%s\n", thisPlane->controlInfo);
	fflush(*writeEnd);
    }
}

void add_plane_id(ConnectingPlane* thisPlane, char* planeIdToAdd) {
    for (int planeId = 0; planeId < *(thisPlane->numPlaneIds); planeId++) {

	// *(thisPlane->planeIDs) is initialised with INITIAL_NUM_PLANE_IDS
	// plane IDs, all set to be == \0 to denote available space. Find the
	// first available space and add the plane ID there
	if (((*(thisPlane->planeIds))[planeId])[0] == '\0') {
	    strcat((*(thisPlane->planeIds))[planeId], planeIdToAdd);
	    return;
	}
    }
    // If reached here, more memory is required to store the given plane ID
    resize_and_add_plane_id(thisPlane, planeIdToAdd);
}

void resize_and_add_plane_id(ConnectingPlane* thisPlane, char* planeIdToAdd) {
    (*(thisPlane->numPlaneIds))++;

    void* morePlaneIds = realloc(*(thisPlane->planeIds),
	    (*(thisPlane->numPlaneIds)) * sizeof(char**));

    if (!morePlaneIds) {
	// flag this error to avoid segfaults
	*(thisPlane->numPlaneIds) = ERROR_RETURN;
	return; // realloc() failed
    }
    *(thisPlane->planeIds) = morePlaneIds;

    // After re-allocating memory, add the new plane ID
    (*(thisPlane->planeIds))[*(thisPlane->numPlaneIds) - 1] =
	    (char*)malloc(INITIAL_BUFFER_SIZE * sizeof(char));

    ((*(thisPlane->planeIds))[*(thisPlane->numPlaneIds) - 1])[0] = '\0';
    strcat((*(thisPlane->planeIds))[*(thisPlane->numPlaneIds) - 1],
	    planeIdToAdd);
}

void display_plane_ids(ConnectingPlane* thisPlane, FILE** writeEnd) {
    sort_plane_ids(thisPlane);
    for (int planeId = 0; planeId < *(thisPlane->numPlaneIds); planeId++) {
	if (((*(thisPlane->planeIds))[planeId])[0] != '\0') {
	    fprintf(*writeEnd, "%s\n", (*(thisPlane->planeIds))[planeId]);
	    fflush(*writeEnd);
	}
    }
    fprintf(*writeEnd, ".\n");
    fflush(*writeEnd);
}

void sort_plane_ids(ConnectingPlane* thisPlane) {
    for (int planeId = 0; planeId < *(thisPlane->numPlaneIds); planeId++) {
	
	// Compare each element with the succeeding elements and work to the
	// end of the array
	for (int planeIdNext = planeId + 1;
		planeIdNext < *(thisPlane->numPlaneIds); planeIdNext++) {

	    // strcmp() returns a positive int if, in terms of lexicographic
	    // order, the first argument should come after the second
	    // argument. Swap this ordering if this is the case
	    if (strcmp((*(thisPlane->planeIds))[planeId],
		    (*(thisPlane->planeIds))[planeIdNext]) > 0) {
		
		char* toSwap =
			(char*)malloc(INITIAL_BUFFER_SIZE * sizeof(char));
		char* currentPlaneId = (*(thisPlane->planeIds))[planeId];
		char* nextPlaneId = (*(thisPlane->planeIds))[planeIdNext];

		strcpy(toSwap, currentPlaneId);
		strcpy(currentPlaneId, nextPlaneId);
		strcpy(nextPlaneId, toSwap);

		free(toSwap);
	    }
	}
    }
}

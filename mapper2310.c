#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include "errors.h"
#include "general.h"
#include "mapper2310.h"

int main(int argc, char** argv) {
    // Squash SIGPIPE to prevent issues arising from sudden termination of
    // client(s)
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = SIG_IGN; // Ignore the siganl
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, NULL);

    uint16_t thisPortNumber;
    int* serverEnd = setup_server(&thisPortNumber); 

    // Check for any errors when setting up the server
    if (serverEnd == NULL) {
	return UNSPECIFIED_ERROR;
    }

    handle_connections(*serverEnd); // Communicate with clients

    free(serverEnd);
    // Should never reach here - mapper should run until killed
    return UNSPECIFIED_ERROR;
}

void handle_connections(int serverEnd) {
    int connectionWrite; // file descriptor for accepted socket

    sem_t* lock = (sem_t*)malloc(sizeof(sem_t));
    int numConnections = INITIAL_NUM_CONNECTIONS;
    ConnectionInfo* connections = init_connections(lock, numConnections);
    
    // Used to ensure each thread gets a unique semaphore
    int currentConnection = 0;
    sem_init(lock, SHARED_BETWEEN_THREADS, 1);

    while (connectionWrite = accept(serverEnd, NULL, NULL),
	    connectionWrite >= 0) { // Ensure accept() succeeded

	// If numConnections is exceeded, allocate more memory (-1 for
	// indexing purposes)
	if (currentConnection == numConnections - 1) {
	    numConnections++;
	    resize_connections(&connections, &numConnections, lock);
	    if (numConnections == ERROR_RETURN) {
		return; // realloc() failed in resize_connections()
	    }
	}
	(connections[currentConnection]).connectionWrite = connectionWrite;

	pthread_t threadId;
	pthread_attr_t attributes;

	// Ensure success of all pthread function calls
	if (pthread_attr_init(&attributes) ||
		pthread_attr_setdetachstate(&attributes,
		PTHREAD_CREATE_DETACHED) ||
		pthread_create(&threadId, &attributes, each_connection,
		connections + currentConnection) ||
		pthread_attr_destroy(&attributes)) {
	    sem_destroy(lock);
	    free(lock);
	    return;
	}	
	currentConnection++;
    }
    sem_destroy(lock);
    free(lock);
}

ConnectionInfo* init_connections(sem_t* lock, int numConnections) {
    ConnectionInfo* connections =
	    (ConnectionInfo*)malloc(numConnections * sizeof(ConnectionInfo));
    
    int* numAirports = (int*)malloc(sizeof(int));
    *numAirports = INITIAL_NUM_AIRPORTS;
    Airport** airports = (Airport**)malloc(sizeof(Airport*));
    *airports = (Airport*)malloc(sizeof(Airport) * (*numAirports));
    init_airports(airports);
    
    for (int connection = 0; connection < numConnections; connection++) {
	// all connections should have access to the same array of airports
	// and the same lock
	(connections[connection]).airports = airports;
	(connections[connection]).numAirports = numAirports;
	(connections[connection]).guard = lock;
    }
    return connections;
}

void resize_connections(ConnectionInfo** connections, int* numConnections,
	sem_t* lock) {
    void* moreConnections =
	    realloc(*connections, (*numConnections) * sizeof(ConnectionInfo));
    if (!moreConnections) {
	*numConnections = ERROR_RETURN;
	return; // realloc() failed
    }
    *connections = moreConnections;
	    
    // after reallocating memory, provide the new connection with
    // required information. NOTE: all connections point to the same
    // airports array, hence we pick any connection (in this case the
    // first)
    ((*connections)[*numConnections - 1]).airports =
	    ((*connections)[0]).airports;
    ((*connections)[*numConnections - 1]).numAirports =
	    ((*connections)[0]).numAirports;
    ((*connections)[*numConnections - 1]).guard = lock;
}

void* each_connection(void* thisConnection) {
    ConnectionInfo* thisConnectionOriginal = (ConnectionInfo*)thisConnection;
    
    int connectionRead = dup(thisConnectionOriginal->connectionWrite);
    
    // Ensure dup() succeeded
    if (connectionRead == ERROR_RETURN) {
	return NULL;
    }
    FILE* readEnd = fdopen(connectionRead, "r");
    FILE* writeEnd = fdopen(thisConnectionOriginal->connectionWrite, "w");
    
    // Ensure fdopen() succeeded
    if (!readEnd || !writeEnd) {
	return NULL;
    }

    size_t commandLength = INITIAL_BUFFER_SIZE;
    char* command = (char*)malloc(commandLength * sizeof(char));

    while (get_line(&command, &commandLength, readEnd),
	    strlen(command) != 0) {
	// Apart from the lock, only the airport data is shared, hence only
	// processing commands (thus consequently manipulating the airport
	// data) requires the lock as each thread has its own socket
	sem_wait(thisConnectionOriginal->guard);
	
	process_command(command, thisConnectionOriginal, &writeEnd);

	if (*(thisConnectionOriginal->numAirports) == ERROR_RETURN) {
	    free(command);
	    fflush(writeEnd);
	    fclose(writeEnd);
	    fclose(readEnd);
	    sem_post(thisConnectionOriginal->guard);
	    return NULL; // realloc() failed, return prevents segfault
	}
	sem_post(thisConnectionOriginal->guard);
    }
    free(command);
    fflush(writeEnd);
    fclose(writeEnd);
    fclose(readEnd); 
    return NULL;
}

void init_airports(Airport** airports) {
    for (int airport = 0; airport < INITIAL_NUM_AIRPORTS; airport++) {
	((*airports)[airport]).id = (char*)malloc(INITIAL_BUFFER_SIZE *
		sizeof(char));
	(((*airports)[airport]).id)[0] = '\0';
	((*airports)[airport]).portNum = INVALID_PORT;
    }
}

void process_command(char* command, ConnectionInfo* thisConnection,
	FILE** writeEnd) {
    int portNum;
    switch (get_command_type(command)) {
	case GET_PORT_NUMBER:
	    // Check if ID exists (extract ID by excluding '?')
	    if ((portNum = get_port_number(thisConnection, command + 1)) ==
		    INVALID_PORT) {
		fprintf(*writeEnd, ";\n");
	    } else {
		fprintf(*writeEnd, "%d\n", portNum);
	    }
	    fflush(*writeEnd);
	    break;
	case ADD_AIRPORT:
	    add_airport(thisConnection, command);
	    break;
	case GET_AIRPORTS:
	    display_airports(thisConnection, writeEnd);
	    break;
	case ERROR:
	    break;
    }
}

void add_airport(ConnectionInfo* thisConnection, char* command) {
    // The ID is given between ! and : (+ 1 to exclude '!')
    char* colonAndPortNum = index(command, ':'); 
    if (colonAndPortNum == NULL) { // Check if index() failed
	return;
    }
    int idLength = colonAndPortNum - (command + 1);
    char* idToAdd = (char*)malloc(INITIAL_BUFFER_SIZE * sizeof(char));
    idToAdd[0] = '\0';
    strncat(idToAdd, command + 1, idLength);

    // Extract the port number (i.e. beginning at the index after the
    // colon) and convert to int
    char* port = colonAndPortNum + 1;
    char* portErrors;
    int portNumberToAdd = strtol(port, &portErrors, 10);

    // Check if idToAdd already exists
    if (get_port_number(thisConnection, idToAdd) !=
	    INVALID_PORT) {
	free(idToAdd);
	return;
    }

    // *(thisConnection->airports) is initialised with INITIAL_NUM_AIRPORTS
    // airports all set to sentinel values to denote available space. Find the
    // first available space and add the airport there
    for (int airport = 0; airport < *(thisConnection->numAirports);
	    airport++) {
	if (((*(thisConnection->airports))[airport]).portNum ==
		INVALID_PORT) {

	    strncat(((*(thisConnection->airports))[airport]).id, idToAdd,
		    idLength);
	    free(idToAdd);

	    ((*(thisConnection->airports))[airport]).portNum =
		    portNumberToAdd;
	    return;
	}
    }
    // realloc memory for more airports if required
    resize_and_add_airport(thisConnection, idToAdd, idLength,
	    portNumberToAdd);
}

void resize_and_add_airport(ConnectionInfo* thisConnection, char* idToAdd,
	int idLength, int portNumberToAdd) {
    (*(thisConnection->numAirports))++;
    
    void* moreAirports = realloc(*(thisConnection->airports),
	    (*(thisConnection->numAirports)) * sizeof(Airport));
    
    if (!moreAirports) {
	// flag this error to avoid segfaults
	*(thisConnection->numAirports) = ERROR_RETURN;
	free(idToAdd);
	return; // realloc failed
    }
    *(thisConnection->airports) = moreAirports;

    // After re-allocating memory, add the new airport
    ((*(thisConnection->airports))[*(thisConnection->numAirports) - 1]).id
	    = (char*)malloc(INITIAL_BUFFER_SIZE * sizeof(char));

    ((*(thisConnection->airports))[*(thisConnection->numAirports) - 1]).id[0]
	    = '\0';
    
    strncat(((*(thisConnection->airports))[*(thisConnection->numAirports) -
	    1]).id, idToAdd, idLength);
    free(idToAdd);
    
    ((*(thisConnection->airports))[*(thisConnection->numAirports) -
	    1]).portNum = portNumberToAdd; 
}

void display_airports(ConnectionInfo* thisConnection, FILE** writeEnd) {
    sort_airports(thisConnection);
    for (int airport = 0; airport < *(thisConnection->numAirports);
	    airport++) {
	if (((*(thisConnection->airports))[airport]).id[0] != '\0') {
	    fprintf(*writeEnd, "%s:%d\n",
		    ((*(thisConnection->airports))[airport]).id,
		    ((*(thisConnection->airports))[airport].portNum));
	    fflush(*writeEnd);
	}
    }
}

void sort_airports(ConnectionInfo* thisConnection) {
    for (int airport = 0; airport < *(thisConnection->numAirports);
	    airport++) {

	// Compare each element with the succeeding elements and work to the
	// end of the array
	for (int nextAirport = airport + 1;
		nextAirport < *(thisConnection->numAirports); nextAirport++) {
	    
	    // strcmp() returns a positive int if, in terms of lexicographic
	    // order, the first argument should come after the second
	    // argument. Swap this ordering if this is the case 
	    if (strcmp(((*(thisConnection->airports))[airport]).id,
		    ((*(thisConnection->airports))[nextAirport]).id) > 0) {
		
		// standard swap with temporary variable
		Airport toSwap = (*(thisConnection->airports))[airport];
		
		(*(thisConnection->airports))[airport] =
			(*(thisConnection->airports))[nextAirport];
		
		(*(thisConnection->airports))[nextAirport] = toSwap;
	    }
	}
    }
}

int get_port_number(ConnectionInfo* thisConnection, char* idOfPort) {
    for (int airport = 0; airport < *(thisConnection->numAirports);
	    airport++) {
	if (!strcmp(((*(thisConnection->airports))[airport]).id, idOfPort)) {
	    return ((*(thisConnection->airports))[airport]).portNum;
	}
    }
    return INVALID_PORT; // if no such airport exists
}

CommandType get_command_type(char* command) {
    // Ensures that an ID and/or port is actually provided
    if (strlen(command) > 1) {
	// Validate the ID, i.e. the command + 1 (to exclude '?')
	if (command[0] == '?' && !check_invalid_chars(command + 1)) {
	    return GET_PORT_NUMBER;
	}
	if (command[0] == '!') {
	    // The ID is given between ! and :
	    char* colonAndPortNum = index(command, ':'); 
	    if (colonAndPortNum != NULL) { // Check if index() failed
		int idLength = colonAndPortNum - (command + 1);
		char* id = (char*)malloc(INITIAL_BUFFER_SIZE * sizeof(char));
		id[0] = '\0';
		strncat(id, command + 1, idLength);

		// Extract the port number (i.e. beginning at the index after
		// the colon) and convert to int
		char* port = colonAndPortNum + 1;
		char* portErrors;
		int portNumber = strtol(port, &portErrors, 10);

		// Ensure neither the ID nor the port number contain any
		// invalid chars, and that the port number is real
		if (!check_invalid_chars(id) &&
			!strtol_invalid(port, portErrors) &&
			portNumber <= PORT_MAX &&
			portNumber >= PORT_MIN) {
		    free(id);
		    return ADD_AIRPORT;
		}
	    }
	}
    }
    if (!strcmp(command, "@")) {
	return GET_AIRPORTS;
    }
    return ERROR;
}

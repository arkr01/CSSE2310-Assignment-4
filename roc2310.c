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
#include "roc2310.h"

int main(int argc, char** argv) {
    // Squash SIGPIPE to prevent issues arising from sudden termination of
    // client(s)
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = SIG_IGN; // Ignore the siganl
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, NULL);

    if (argc < MIN_NUM_COMMAND_LINE_ARGS) {
	return roc_error_message(ROC_ARGS);
    }

    // ID should not have any invalid chars and roc should not be called log
    // as this is a command for the control
    if (check_invalid_chars(argv[ID]) || !strcmp(argv[ID], "log")) {
	exit(UNSPECIFIED_ERROR);
    }

    // Only validate mapper if destinations are present
    if (argc > MIN_NUM_COMMAND_LINE_ARGS && strcmp(argv[MAPPER_PORT], "-")) {
	char* mapperPortErrors;
	int mapperPort = strtol(argv[MAPPER_PORT], &mapperPortErrors, 10);
	if (strtol_invalid(argv[MAPPER_PORT], mapperPortErrors) ||
		mapperPort < PORT_MIN || mapperPort > PORT_MAX) {
	    return roc_error_message(ROC_INVALID_MAPPER);
	}
    }

    // Roc can fly to 0 or more destinations, hence the following
    int numDestinations = argc - MIN_NUM_COMMAND_LINE_ARGS;

    // Allocate enough space to store the port numbers of all destinations
    char** portNumbers = (char**)malloc(numDestinations * sizeof(char*));
    for (int portNumber = 0; portNumber < numDestinations; portNumber++) {
	portNumbers[portNumber] =
		(char*)malloc(INITIAL_BUFFER_SIZE * sizeof(char));
    }

    // Pass in the mapper port (or -) as well as the destinations
    RocExitCodes portError = get_ports(argv + MAPPER_PORT, numDestinations,
	    &portNumbers);
    if (portError == ROC_NORMAL) {
	portError = connect_to_ports(portNumbers, argv[ID], numDestinations);
    }
    free_port_numbers(portNumbers, numDestinations);
    return roc_error_message(portError);
}

RocExitCodes get_ports(char** destinationsAndMapper,
	int numDestinations, char*** portNumbers) {
    for (int destination = 1; destination <= numDestinations; destination++) {
	char* destinationErrors;
	int destinationPort = strtol(destinationsAndMapper[destination],
		&destinationErrors, 10);

	// Check if destinationPort is an invalid port number
	if (strtol_invalid(destinationsAndMapper[destination],
		destinationErrors) || destinationPort < PORT_MIN ||
		destinationPort > PORT_MAX) {

	    // Mapper port is stored at first entry, check if mapper was given
	    if (strcmp(destinationsAndMapper[0], "-")) {
		// Mapper port is first entry thus pass in destination - 1
		RocExitCodes mapperError =
			query_mapper(destinationsAndMapper[destination],
			destination - 1, destinationsAndMapper[0],
			portNumbers);
		if (mapperError != ROC_NORMAL) {
		    return mapperError;
		}
	    } else {
		// No mapper provided but destinationPort is not a valid port
		// number
		return ROC_MAPPER_REQUIRED;
	    }
	} else {
	    // Valid port number was given, add to *portNumbers (-1 to exclude
	    // the mapper)
	    ((*portNumbers)[destination - 1])[0] = '\0';
	    strcat((*portNumbers)[destination - 1],
		    destinationsAndMapper[destination]);
	}
    }
    return ROC_NORMAL;
}

RocExitCodes query_mapper(char* destinationToQuery, int destination,
	char* mapperPort, char*** portNumbers) {
    if (check_invalid_chars(destinationToQuery)) {
	return ROC_DESTINATION; // Invalid destination given in command line
    }
    int thisEndWrite; // stores mapper socket

    // Used to differentiate behaviour of functions based on which program(s)
    // are calling said functions
    bool controlCalled = false;

    RocExitCodes mapperError = setup_client(mapperPort, &thisEndWrite,
	    controlCalled);
    if (mapperError == ROC_MAPPER_CONNECT) {
	return mapperError;
    }

    int thisEndRead = dup(thisEndWrite);
    // Ensure dup() succeeded
    if (thisEndWrite == ERROR_RETURN) { 
	return ROC_MAPPER_CONNECT;
    }

    FILE* toWrite = fdopen(thisEndWrite, "w");
    FILE* toRead = fdopen(thisEndRead, "r");
    // Ensure fdopen() succeeded
    if (!toWrite || !toRead) {
	return ROC_MAPPER_CONNECT;
    }
    // Query mapper for port number
    fprintf(toWrite, "?%s\n", destinationToQuery);
    fflush(toWrite);
    size_t portNumberLength = INITIAL_BUFFER_SIZE;
    char* portNumber = (char*)malloc(portNumberLength * sizeof(char));

    RocExitCodes queryReturn = ROC_NORMAL;
    if (get_line(&portNumber, &portNumberLength, toRead),
	    strlen(portNumber) != 0) {
	if (!strcmp(portNumber, ";")) {
	    queryReturn = ROC_MAP_ENTRY;
	} else {
	    ((*portNumbers)[destination])[0] = '\0';
	    strcat((*portNumbers)[destination], portNumber);
	}
    } else {
	queryReturn = ROC_MAP_ENTRY;
    }
    free(portNumber);
    fclose(toRead);
    fclose(toWrite);
    return queryReturn;
}

RocExitCodes connect_to_ports(char** portNumbers, char* id,
	int numDestinations) {
    RocExitCodes connectionError = ROC_NORMAL;

    // Used to differentiate behaviour of functions based on which program(s)
    // are calling said functions
    bool controlCalled = false;

    for (int destination = 0; destination < numDestinations; destination++) {
	int thisEndWrite; // stores control socket

	// connect to each control and then print info\n
	connectionError = setup_client(portNumbers[destination],
		&thisEndWrite, controlCalled);

	// setup_client() returns ROC_MAPPER_CONNECT on error, in this case,
	// we want ROC_DESTINATION to be the return value instead
	connectionError = (connectionError == ROC_NORMAL) ? ROC_NORMAL :
		ROC_DESTINATION;

	int thisEndRead = dup(thisEndWrite);
	if (thisEndRead == ERROR_RETURN) { // Ensure dup() succeeded
	    connectionError = ROC_DESTINATION;
	    continue; // Attempt to connect to the other airports as normal
	}
	FILE* writeEnd = fdopen(thisEndWrite, "w");
	FILE* readEnd = fdopen(thisEndRead, "r");
	if (!writeEnd || !readEnd) { // Ensure fdopen() succeeded
	    connectionError = ROC_DESTINATION;
	    continue; // Attempt to connect to the other airports as normal
	}
	fprintf(writeEnd, "%s\n", id);
	fflush(writeEnd);

	size_t airportInfoLength = INITIAL_BUFFER_SIZE;
	char* airportInfo = (char*)malloc(airportInfoLength * sizeof(char));
	if (get_line(&airportInfo, &airportInfoLength, readEnd),
		strlen(airportInfo) != 0) {
	    if (check_invalid_chars(airportInfo)) {
		connectionError = ROC_DESTINATION;
	    } else {
		printf("%s\n", airportInfo); // display the log
		fflush(stdout);
	    }
	}
	free(airportInfo);
	fclose(readEnd);
	fflush(writeEnd);
	fclose(writeEnd);
    }
    return connectionError;
}

void free_port_numbers(char** portNumbers, int numPortNumbers) {
    for (int portNum = 0; portNum < numPortNumbers; portNum++) {
	free(portNumbers[portNum]);
    }
    free(portNumbers);
}

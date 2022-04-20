#ifndef ROC_2310_H
#define ROC_2310_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include "errors.h"
#include "general.h"

/* The minimum number of command line arguments that the roc program can
 * take. */
#define MIN_NUM_COMMAND_LINE_ARGS 3

/* Used to index argv for the plane ID. */
#define ID 1

/* Used to index argv for the mapper port. */
#define MAPPER_PORT 2

/* Takes in the plane's destinations and the mapper port (or -), an empty
 * space to store port numbers, and the number of destinations. Validates and
 * populates portNumbers with the port numbers of the give destinations. If a
 * mapper is provided, and a port number is found invalid, this function
 * queries said mapper and obtains a valid port number. Returns the
 * appropriate exit code. */
RocExitCodes get_ports(char** destinationsAndMapper, int numDestinations,
	char*** portNumbers);

/* Takes in a destination airport ID, the index of the destination of
 * interest (with respect to the order of destinations specified in the
 * command line arguments), the mapper port, and the port numbers of the
 * plane. Sets up a connection with the mapper and queries for the port number
 * of the provided destination airport ID. If a mapping is found, adds the
 * port number to *portNumbers. Returns the appropriate exit code. */
RocExitCodes query_mapper(char* destinationToQuery, int thisDestination,
	char* mapperPort, char*** portNumbers);

/* Takes in the (validated) port numbers, the plane id, and the number of
 * destinations. Connects to each destination and populates the log with
 * airport information. Returns the appropriate exit code. */
RocExitCodes connect_to_ports(char** portNumbers, char* id,
	int numDestinations);

/* Takes in (and free()s) the port numbers of the destinations. Also takes in
 * the number of destinations. */
void free_port_numbers(char** portNumbers, int numPortNumbers);

#endif

#ifndef GENERAL_H
#define GENERAL_H

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

/* The get_line() function re-allocates memory if necessary. Hence, whenever
 * using get_line, let us begin with an initial buffer size of 79 bytes. */
#define INITIAL_BUFFER_SIZE 79

/* The get_line() function re-allocates memory if necessary. Below provides a
 * re-sizing factor to scale the memory re-allocation appropriately. */
#define RESIZING_FACTOR 2

/* Lowest port number possible on Linux systems. */
#define PORT_MIN 1

/* Highest port number possible on Linux systems. */
#define PORT_MAX 65535

/* In the call to socket(), the third parameter denotes the network protocol
 * to be used. 0 denotes the default protocol. */
#define DEFAULT_PROTOCOL 0

/* In the call to sem_init(), the second parameter denotes how the semaphore
 * lock should be shared. 0 denotes the lock to be shared between threads in a
 * process. */
#define SHARED_BETWEEN_THREADS 0

/* Takes in an empty space to store the port number. Sets up a server to
 * listen on an ephemeral port and displays said port number. Returns a socket
 * upon success, otherwise returns NULL. NOTE: the socket is created via
 * dynamically allocated memory, and should be free'd if no longer in use. */
int* setup_server(uint16_t* thisPortNumber);

/* Takes in a port to connect to, a socket endpoint to communicate with, and a
 * flag to check whether an airport or a plane called this function. Sets up a
 * client connection to the port specified, via the socket end point provided,
 * and returns the appropriate exit code (based on controlCalled). */
int setup_client(char* portToConnectTo, int* thisEnd, bool controlCalled);

/* Calculates (and returns) the maximum number of server connections allowed
 * by this system. Returns UNSPECIFIED_ERROR on error. */
int get_num_connections(void);

/* Takes in a buffer to store the line read, an initial minimum length of the
 * line to be read, and the source of the line to be read. Reads in a single
 * line of input and stores in the buffer. If the line of input is longer than
 * the minimum length provided, more space is allocated to store the remainder
 * of the line. Returns if valid line could be read (e.g. no unexpected EOF).
 * */
bool get_line(char** buffer, size_t* minBufferSize, FILE* sourceOfLine);

/* Takes in the input converted via strtol call, as well as the error
 * stored via strtol call, and checks (and returns) if the input was
 * invalid. */
bool strtol_invalid(char* input, char* error);

/* Takes in a string and a character to count, and counts (and returns) the
 * number of times said character appears in said string. */
int character_counter(char* stringToSearch, char toCount);

/* Takes in a string. Checks (and returns) if said string contains any invalid
 * chars as per the spec (':', '\r', '\n'). */
bool check_invalid_chars(char* stringToCheck);

#endif

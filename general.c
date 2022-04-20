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
#include "errors.h"
#include "general.h"

int* setup_server(uint16_t* thisPortNumber) {
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo("localhost", NULL, &hints, &ai)) {
	freeaddrinfo(ai);
	return NULL;
    }

    int* serverEnd = (int*)malloc(sizeof(int));
    if ((*serverEnd = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) ==
	    ERROR_RETURN) {
	freeaddrinfo(ai);
	free(serverEnd);
	return NULL;
    }
    if (bind(*serverEnd, (struct sockaddr*)ai->ai_addr,
	    sizeof(struct sockaddr))) {
	freeaddrinfo(ai);
	free(serverEnd);
	return NULL;
    }
    freeaddrinfo(ai); // no need for ai anymore

    struct sockaddr_in internetAddress;
    socklen_t lengthOfSocket = sizeof(struct sockaddr_in);
    memset(&internetAddress, 0, lengthOfSocket);
    if (getsockname(*serverEnd, (struct sockaddr*)&internetAddress,
	    &lengthOfSocket)) {
	free(serverEnd);
	return NULL;
    }

    int numConnections = get_num_connections();
    if (numConnections == UNSPECIFIED_ERROR ||
	    listen(*serverEnd, numConnections)) {
	free(serverEnd);
	return NULL;
    }
    *thisPortNumber = ntohs(internetAddress.sin_port);
    printf("%u\n", *thisPortNumber); // display port number
    fflush(stdout);

    return serverEnd;
}

int setup_client(char* portToConnectTo, int* thisEnd, bool controlCalled) {
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("localhost", portToConnectTo, &hints, &ai)) {
	freeaddrinfo(ai);
	return (controlCalled) ? CONTROL_MAPPER : ROC_MAPPER_CONNECT;
    }

    if ((*thisEnd = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) ==
	    ERROR_RETURN) {
	freeaddrinfo(ai);
        return (controlCalled) ? CONTROL_MAPPER : ROC_MAPPER_CONNECT;
    }
    
    if (connect(*thisEnd, (struct sockaddr*)ai->ai_addr,
	    sizeof(struct sockaddr)) == ERROR_RETURN) {
	freeaddrinfo(ai);
	return (controlCalled) ? CONTROL_MAPPER : ROC_MAPPER_CONNECT;
    }
    freeaddrinfo(ai);
    return (controlCalled) ? CONTROL_NORMAL : ROC_NORMAL;
}

int get_num_connections(void) {
    // Stores maximum number of possible server connections
    FILE* maxConnectionsFile = fopen("/proc/sys/net/core/somaxconn", "r");

    // Ensure fopen() succeeded
    if (!maxConnectionsFile) {
	return UNSPECIFIED_ERROR;
    }

    size_t initialBufferSize = INITIAL_BUFFER_SIZE;
    char* numConnectionsLine = (char*)
	    malloc(initialBufferSize * sizeof(char));
    int numConnections;

    if (get_line(&numConnectionsLine, &initialBufferSize, maxConnectionsFile),
	    strlen(numConnectionsLine) != 0) {
	char* numConnectionErrors;
	numConnections = strtol(numConnectionsLine, &numConnectionErrors, 10);
	
	// At least one connection should be allowed
	if (numConnections < 1 || strtol_invalid(numConnectionsLine,
		numConnectionErrors)) {
	    numConnections = UNSPECIFIED_ERROR;
	}
    } else {
	numConnections = UNSPECIFIED_ERROR;
    }
    free(numConnectionsLine);
    fclose(maxConnectionsFile);
    return numConnections;
}

bool get_line(char** buffer, size_t* lineLength, FILE* sourceOfLine) {
    size_t bufferIndex = 0;
    (*buffer)[0] = '\0';
    int input;
    while (input = fgetc(sourceOfLine), input != EOF && input != '\n') {
	// Check if buffer needs to be expanded (ensure space is available
	// prior to buffer expansion by checking *lineLength - 2)
        if (bufferIndex > *lineLength - 2) {
	    // Create a new, larger buffer and point the current buffer to the
	    // new buffer
            size_t newLineLength = (size_t)(*lineLength * RESIZING_FACTOR);
            void* newBuffer = realloc(*buffer, newLineLength);
            if (newBuffer == NULL) { // realloc returns NULL on error
                return false;
            }
            *lineLength = newLineLength;
            *buffer = newBuffer;
        }
        (*buffer)[bufferIndex] = (char)input;
        (*buffer)[++bufferIndex] = '\0'; // Ensure string is null-terminated
    }
    return input != EOF;
}

bool strtol_invalid(char* input, char* error) {
    // From man strtol: strtol call valid if *input != '\0' && *error == '\0'
    return (!(*input != '\0' && *error == '\0'));
}

int character_counter(char* stringToSearch, char toCount) {
    int characterCount = 0;
    for (int i = 0; stringToSearch[i] != '\0'; i++) {
	if (stringToSearch[i] == toCount) {
	    characterCount++;
	}
    }
    return characterCount;
}

bool check_invalid_chars(char* stringToCheck) {
    return (character_counter(stringToCheck, ':') ||
	    character_counter(stringToCheck, '\n') ||
	    character_counter(stringToCheck, '\r'));
}

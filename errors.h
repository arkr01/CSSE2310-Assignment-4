#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>

/* For any error handling not specified by the spec, return 7. */
#define UNSPECIFIED_ERROR 7

/* Several system calls return -1 on error. Check for this. */
#define ERROR_RETURN -1

/* Control Exit Codes */
typedef enum {
    CONTROL_NORMAL = 0, // For function returns only
    CONTROL_ARGS = 1,
    CONTROL_CHAR = 2,
    CONTROL_PORT = 3,
    CONTROL_MAPPER = 4,
} ControlExitCodes;

/* Roc Exit Codes */
typedef enum {
    ROC_NORMAL = 0,
    ROC_ARGS = 1,
    ROC_INVALID_MAPPER = 2,
    ROC_MAPPER_REQUIRED = 3,
    ROC_MAPPER_CONNECT = 4,
    ROC_MAP_ENTRY = 5,
    ROC_DESTINATION = 6
} RocExitCodes;

/* Takes in the control exit code. Returns the control exit code and displays
 * the respective control error message. */
ControlExitCodes control_error_message(ControlExitCodes controlExitType);

/* Takes in the roc exit code. Returns the roc exit code and displays the
 * respective roc error message. */
RocExitCodes roc_error_message(RocExitCodes rocExitType);

#endif

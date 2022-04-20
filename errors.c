#include <stdio.h>
#include "errors.h"

ControlExitCodes control_error_message(ControlExitCodes controlExitType) {
    // the control error message to be fprinted to stderr
    const char* controlErrorMessage = "";

    switch (controlExitType) {
	case CONTROL_NORMAL:
	    return CONTROL_NORMAL; // Should only be used in function returns
	case CONTROL_ARGS:
	    controlErrorMessage = "Usage: control2310 id info [mapper]";
	    break;
	case CONTROL_CHAR:
	    controlErrorMessage = "Invalid char in parameter";
	    break;
	case CONTROL_PORT:
	    controlErrorMessage = "Invalid port";
	    break;
	case CONTROL_MAPPER:
	    controlErrorMessage = "Can not connect to map";
	    break;
    }
    fprintf(stderr, "%s\n", controlErrorMessage);
    return controlExitType;
}

RocExitCodes roc_error_message(RocExitCodes rocExitType) {
    // the roc error message to be fprinted to stderr
    const char* rocErrorMessage = "";

    switch (rocExitType) {
	case ROC_NORMAL:
	    return ROC_NORMAL;
	case ROC_ARGS:
	    rocErrorMessage = "Usage: roc2310 id mapper {airports}";
	    break;
	case ROC_INVALID_MAPPER:
	    rocErrorMessage = "Invalid mapper port";
	    break;
	case ROC_MAPPER_REQUIRED:
	    rocErrorMessage = "Mapper required";
	    break;
	case ROC_MAPPER_CONNECT:
	    rocErrorMessage = "Failed to connect to mapper";
	    break;
	case ROC_MAP_ENTRY:
	    rocErrorMessage = "No map entry for destination";
	    break;
	case ROC_DESTINATION:
	    rocErrorMessage = "Failed to connect to at least one destination";
	    break;
    }
    fprintf(stderr, "%s\n", rocErrorMessage);
    return rocExitType;
}

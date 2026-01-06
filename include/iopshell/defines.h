#pragma once

#include <stdint.h>

typedef void (*IOPShell_CommandCallback)(int argc, char **argv);

#define IOPSHELL_MAX_CMD_NAME 32
#define IOPSHELL_MAX_CMD_DESC 128
#define IOPSHELL_MAX_CMD_USAGE 128

typedef struct {
    char command[IOPSHELL_MAX_CMD_NAME];
    char description[IOPSHELL_MAX_CMD_DESC];
    char usage[IOPSHELL_MAX_CMD_USAGE];
    IOPShell_CommandCallback callback;
} IOPShellModule_CommandEntry;

typedef enum IOPShellModule_Error {
    IOPSHELL_MODULE_ERROR_SUCCESS                 = 0,
    IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT        = -1,
    IOPSHELL_MODULE_ERROR_HANDLE_NOT_FOUND        = -2,
    IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND        = -3,
    IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT   = -4,
    IOPSHELL_MODULE_ERROR_UNSUPPORTED_API_VERSION = -5,
    IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND     = -6,
    IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED       = -7,
    IOPSHELL_MODULE_ERROR_ALREADY_EXISTS          = -8,
    IOPSHELL_MODULE_ERROR_UNKNOWN_ERROR           = -0xFF,
} IOPShellModule_Error;

typedef int32_t IOPShellModule_APIVersion;

#define IOPSHELL_MODULE_API_VERSION_ERROR (-0xFF)
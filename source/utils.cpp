#include "logger.h"
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <coreinit/mutex.h>
#include <cstdlib>
#include <cstring>
#include <iopshell/api.h>
#include <iopshell/defines.h>

static OSDynLoad_Module sModuleHandle = nullptr;

static IOPShellModule_Error (*sISMGetVersionFn)(IOPShellModule_APIVersion *)                                                                                 = nullptr;
static IOPShellModule_Error (*sISMListCommands)(IOPShellModule_CommandEntry *outList, uint32_t bufferSize, uint32_t *outCount)                               = nullptr;
static IOPShellModule_Error (*sISMAddCommand)(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage, bool showInHelp) = nullptr;
static IOPShellModule_Error (*sISMRemoveCommand)(const char *cmdName)                                                                                        = nullptr;

static bool sLibInitDone                                = false;
static IOPShellModule_APIVersion sIOPShellModuleVersion = IOPSHELL_MODULE_API_VERSION_ERROR;

// ----------------------------------------------------------------------
// Manual Command Tracking (Replaces std::vector<std::string>)
// ----------------------------------------------------------------------

struct CommandNode {
    char *name;
    CommandNode *next;
};

static CommandNode *sCommandList = nullptr;
static OSMutex sTrackingMutex;

// Helper to add a name to the list
static void TrackCommand(const char *name) {
    if (!name) return;

    const size_t len = std::strlen(name);
    char *copy       = static_cast<char *>(std::malloc(len + 1));
    if (!copy) return;
    std::strcpy(copy, name);

    const auto node = static_cast<CommandNode *>(std::malloc(sizeof(CommandNode)));
    if (!node) {
        std::free(copy);
        return;
    }

    node->name   = copy;
    node->next   = sCommandList;
    sCommandList = node;
}

// Helper to remove a name from the list
static void UntrackCommand(const char *name) {
    if (!name || !sCommandList) return;

    CommandNode *curr = sCommandList;
    CommandNode *prev = nullptr;

    while (curr) {
        if (std::strcmp(curr->name, name) == 0) {
            // Found it, unlink
            if (prev) {
                prev->next = curr->next;
            } else {
                sCommandList = curr->next;
            }

            // Free memory
            std::free(curr->name);
            std::free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// ----------------------------------------------------------------------
// Implementation
// ----------------------------------------------------------------------

const char *IOPShellModule_GetStatusStr(const IOPShellModule_Error status) {
    switch (status) {
        case IOPSHELL_MODULE_ERROR_SUCCESS:
            return "IOPSHELL_MODULE_ERROR_SUCCESS";
        case IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT:
            return "IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT";
        case IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND:
            return "IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND";
        case IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT:
            return "IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT";
        case IOPSHELL_MODULE_ERROR_UNSUPPORTED_API_VERSION:
            return "IOPSHELL_MODULE_ERROR_UNSUPPORTED_API_VERSION";
        case IOPSHELL_MODULE_ERROR_UNKNOWN_ERROR:
            return "IOPSHELL_MODULE_ERROR_UNKNOWN_ERROR";
        case IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND:
            return "IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND";
        case IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED:
            return "IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED";
        case IOPSHELL_MODULE_ERROR_HANDLE_NOT_FOUND:
            return "IOPSHELL_MODULE_ERROR_HANDLE_NOT_FOUND";
        case IOPSHELL_MODULE_ERROR_ALREADY_EXISTS:
            return "IOPSHELL_MODULE_ERROR_ALREADY_EXISTS";
        case IOPSHELL_MODULE_ERROR_UNKNOWN_OR_FOREIGN_CMD:
            return "IOPSHELL_MODULE_ERROR_UNKNOWN_OR_FOREIGN_CMD";
    }
    return "IOPSHELL_MODULE_ERROR_UNKNOWN_ERROR";
}

IOPShellModule_Error IOPShellModule_InitLibrary() {
    if (sLibInitDone) {
        return IOPSHELL_MODULE_ERROR_SUCCESS;
    }

    if (sModuleHandle == nullptr) {
        if (OSDynLoad_Acquire("homebrew_iopshell", &sModuleHandle) != OS_DYNLOAD_OK) {
            DEBUG_FUNCTION_LINE_ERR("OSDynLoad_Acquire failed.");
            return IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND;
        }
    }

#define FIND_EXPORT(func_name, func_ptr)                                                                                                \
    if (OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, func_name, reinterpret_cast<void **>(func_ptr)) != OS_DYNLOAD_OK) { \
        DEBUG_FUNCTION_LINE_ERR("FindExport " func_name " failed.");                                                                    \
        OSDynLoad_Release(sModuleHandle);                                                                                               \
        sModuleHandle = nullptr;                                                                                                        \
        return IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT;                                                                             \
    }

    FIND_EXPORT("IOPShellModule_GetVersion", &sISMGetVersionFn);
    FIND_EXPORT("IOPShellModule_ListCommands", &sISMListCommands);
    FIND_EXPORT("IOPShellModule_AddCommand", &sISMAddCommand);
    FIND_EXPORT("IOPShellModule_RemoveCommand", &sISMRemoveCommand);

    if (IOPShellModule_GetVersion(&sIOPShellModuleVersion) != IOPSHELL_MODULE_ERROR_SUCCESS) {
        sIOPShellModuleVersion = IOPSHELL_MODULE_API_VERSION_ERROR;
        OSDynLoad_Release(sModuleHandle);
        sModuleHandle = nullptr;
        return IOPSHELL_MODULE_ERROR_UNSUPPORTED_API_VERSION;
    }
    OSInitMutex(&sTrackingMutex);
    sCommandList = nullptr;
    sLibInitDone = true;
    return IOPSHELL_MODULE_ERROR_SUCCESS;
}

IOPShellModule_Error IOPShellModule_DeInitLibrary() {
    if (sLibInitDone) {
        {
            OSLockMutex(&sTrackingMutex);
            if (sISMRemoveCommand) {
                // Iterate through our manual list and remove commands
                CommandNode *curr = sCommandList;
                while (curr) {
                    DEBUG_FUNCTION_LINE_WARN("WARNING: Calling Deinit without removing command \"%s\" first", curr->name);
                    sISMRemoveCommand(curr->name);

                    // Move to next and free current
                    CommandNode *next = curr->next;
                    std::free(curr->name);
                    std::free(curr);
                    curr = next;
                }
                sCommandList = nullptr;
            }
            OSUnlockMutex(&sTrackingMutex);
        }

        sISMGetVersionFn       = nullptr;
        sISMListCommands       = nullptr;
        sISMAddCommand         = nullptr;
        sISMRemoveCommand      = nullptr;
        sIOPShellModuleVersion = IOPSHELL_MODULE_API_VERSION_ERROR;

        OSDynLoad_Release(sModuleHandle);
        sModuleHandle = nullptr;
        sLibInitDone  = false;
    }
    return IOPSHELL_MODULE_ERROR_SUCCESS;
}

IOPShellModule_Error IOPShellModule_GetVersion(IOPShellModule_APIVersion *outVersion) {
    if (sISMGetVersionFn == nullptr) {
        if (OSDynLoad_Acquire("homebrew_iopshell", &sModuleHandle) != OS_DYNLOAD_OK) {
            DEBUG_FUNCTION_LINE_WARN("OSDynLoad_Acquire failed.");
            return IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND;
        }

        if (OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, "IOPShellModule_GetVersion", reinterpret_cast<void **>(&sISMGetVersionFn)) != OS_DYNLOAD_OK) {
            DEBUG_FUNCTION_LINE_WARN("FindExport IOPShellModule_GetVersion failed.");
            return IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT;
        }
    }

    return sISMGetVersionFn(outVersion);
}


IOPShellModule_Error IOPShellModule_AddCommandEx(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage, bool showInHelp) {
    if (sIOPShellModuleVersion == IOPSHELL_MODULE_API_VERSION_ERROR) {
        return IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED;
    }
    if (sISMListCommands == nullptr || sIOPShellModuleVersion < 1) {
        return IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND;
    }

    if (cmdName == nullptr || cb == nullptr) {
        return IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT;
    }

    const IOPShellModule_Error ret = sISMAddCommand(cmdName, cb, description, usage, showInHelp);

    if (ret == IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSLockMutex(&sTrackingMutex);

        // Check if already in list to avoid duplicates
        bool found              = false;
        const CommandNode *curr = sCommandList;
        while (curr) {
            if (std::strcmp(curr->name, cmdName) == 0) {
                found = true;
                break;
            }
            curr = curr->next;
        }

        if (!found) {
            TrackCommand(cmdName);
        }
        OSUnlockMutex(&sTrackingMutex);
    }

    return ret;
}

IOPShellModule_Error IOPShellModule_AddCommand(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage) {
    return IOPShellModule_AddCommandEx(cmdName, cb, description, usage, true);
}

IOPShellModule_Error IOPShellModule_RemoveCommand(const char *cmdName) {
    if (sIOPShellModuleVersion == IOPSHELL_MODULE_API_VERSION_ERROR) {
        return IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED;
    }
    if (sISMListCommands == nullptr || sIOPShellModuleVersion < 1) {
        return IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND;
    }

    if (!cmdName) {
        return IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT;
    }

    {
        OSLockMutex(&sTrackingMutex);
        // Check if we added that command
        bool found              = false;
        const CommandNode *curr = sCommandList;
        while (curr) {
            if (std::strcmp(curr->name, cmdName) == 0) {
                found = true;
                break;
            }
            curr = curr->next;
        }
        OSUnlockMutex(&sTrackingMutex);

        if (!found) {
            // if not, don't even try to remove it.
            return IOPSHELL_MODULE_ERROR_UNKNOWN_OR_FOREIGN_CMD;
        }
    }

    const IOPShellModule_Error ret = sISMRemoveCommand(cmdName);

    if (ret == IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSLockMutex(&sTrackingMutex);
        UntrackCommand(cmdName);
        OSUnlockMutex(&sTrackingMutex);
    }

    return ret;
}

IOPShellModule_Error IOPShellModule_ListCommands(IOPShellModule_CommandEntry *outList, uint32_t bufferSize, uint32_t *outCount) {
    if (sIOPShellModuleVersion == IOPSHELL_MODULE_API_VERSION_ERROR) {
        return IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED;
    }
    if (sISMListCommands == nullptr || sIOPShellModuleVersion < 1) {
        return IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND;
    }

    if (outCount == nullptr && outList == nullptr) {
        return IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT;
    }

    return sISMListCommands(outList, bufferSize, outCount);
}
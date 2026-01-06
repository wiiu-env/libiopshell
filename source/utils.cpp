#include "logger.h"
#include <algorithm>
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <cstdarg>
#include <iopshell/api.h>
#include <iopshell/defines.h>
#include <mutex>

static OSDynLoad_Module sModuleHandle = nullptr;

static IOPShellModule_Error (*sISMGetVersionFn)(IOPShellModule_APIVersion *)                                                                = nullptr;
static IOPShellModule_Error (*sISMListCommands)(IOPShellModule_CommandEntry *outList, uint32_t bufferSize, uint32_t *outCount)              = nullptr;
static IOPShellModule_Error (*sISMAddCommand)(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage) = nullptr;
static IOPShellModule_Error (*sISMRemoveCommand)(const char *cmdName)                                                                       = nullptr;

static bool sLibInitDone = false;

static IOPShellModule_APIVersion sIOPShellModuleVersion = IOPSHELL_MODULE_API_VERSION_ERROR;

static std::vector<std::string> sRegisteredCommands;
static std::recursive_mutex sTrackingMutex;

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

    {
        std::lock_guard lock(sTrackingMutex);
        sRegisteredCommands.clear();
    }

    sLibInitDone = true;
    return IOPSHELL_MODULE_ERROR_SUCCESS;
}

IOPShellModule_Error IOPShellModule_DeInitLibrary() {
    if (sLibInitDone) {
        {
            std::lock_guard lock(sTrackingMutex);
            if (sISMRemoveCommand) {
                for (const auto &cmd : sRegisteredCommands) {
                    DEBUG_FUNCTION_LINE_WARN("WARNING: Calling Deinit without removing command \"%s\" first", cmd.c_str());
                    sISMRemoveCommand(cmd.c_str());
                }
            }
            sRegisteredCommands.clear();
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


IOPShellModule_Error IOPShellModule_AddCommand(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage) {
    if (sIOPShellModuleVersion == IOPSHELL_MODULE_API_VERSION_ERROR) {
        return IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED;
    }
    if (sISMListCommands == nullptr || sIOPShellModuleVersion < 1) {
        return IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND;
    }

    if (cmdName == nullptr || cb == nullptr) {
        return IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT;
    }

    const IOPShellModule_Error ret = sISMAddCommand(cmdName, cb, description, usage);

    if (ret == IOPSHELL_MODULE_ERROR_SUCCESS) {
        std::lock_guard lock(sTrackingMutex);
        bool found = false;
        for (const auto &cmd : sRegisteredCommands) {
            if (cmd == cmdName) {
                found = true;
                break;
            }
        }
        if (!found) {
            sRegisteredCommands.emplace_back(cmdName);
        }
    }

    return ret;
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

    const IOPShellModule_Error ret = sISMRemoveCommand(cmdName);

    if (ret == IOPSHELL_MODULE_ERROR_SUCCESS) {
        std::lock_guard lock(sTrackingMutex);
        if (const auto it = std::ranges::remove(sRegisteredCommands, std::string(cmdName)).begin(); it != sRegisteredCommands.end()) {
            sRegisteredCommands.erase(it, sRegisteredCommands.end());
        }
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
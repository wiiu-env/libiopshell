#include "iopshell/api.h"
#include "iopshell/defines.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace IOPShellModule {
    IOPShellModule_Error Init() {
        return IOPShellModule_InitLibrary();
    }

    IOPShellModule_Error DeInit() {
        return IOPShellModule_DeInitLibrary();
    }

    const char *GetErrorString(IOPShellModule_Error error) {
        return IOPShellModule_GetStatusStr(error);
    }

    int32_t GetVersion() {
        IOPShellModule_APIVersion ver = -1;
        if (IOPShellModule_GetVersion(&ver) != IOPSHELL_MODULE_ERROR_SUCCESS) return -1;
        return ver;
    }

    std::map<std::string, std::function<void(int, char **)>> &CommandRegistry::GetHandlerMap() {
        static std::map<std::string, std::function<void(int, char **)>> handlers;
        return handlers;
    }

    std::recursive_mutex &CommandRegistry::GetMutex() {
        static std::recursive_mutex mtx;
        return mtx;
    }

    void CommandRegistry::GlobalLambdaDispatcher(int argc, char **argv) {
        if (!argv || !argv[0]) {
            return;
        }
        std::function<void(int, char **)> handler;
        {
            std::lock_guard lock(GetMutex());
            auto &map = GetHandlerMap();
            if (const auto it = map.find(argv[0]); it != map.end()) {
                handler = it->second;
            }
        }
        if (handler) handler(argc, argv);
        else
            OSReport("IOPShell Error: Command '%s' registered but no handler found.\n", argv[0]);
    }

    IOPShellModule_Error CommandRegistry::Remove(const char *name) {
        {
            std::lock_guard lock(GetMutex());
            GetHandlerMap().erase(name);
        }
        return IOPShellModule_RemoveCommand(name);
    }

    std::vector<IOPShellModule_CommandEntry> CommandRegistry::List() {
        uint32_t count           = 0;
        IOPShellModule_Error err = IOPShellModule_ListCommands(nullptr, 0, &count);
        if (err != IOPSHELL_MODULE_ERROR_SUCCESS || count == 0) return {};
        std::vector<IOPShellModule_CommandEntry> list(count);
        err = IOPShellModule_ListCommands(list.data(), count, &count);
        if (err != IOPSHELL_MODULE_ERROR_SUCCESS) return {};
        list.resize(count);
        return list;
    }

    namespace internal {
        bool IsEqualCaseInsensitive(std::string_view a, std::string_view b) {
            return std::ranges::equal(a, b, [](const char a, const char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
        }

        void LogCommandError(const char *commandName, int expected, int provided, const std::string &signature) {
            OSReport("Command '%s' expects %d args %s, but got %d\n", commandName, expected, signature.c_str(), provided);
        }

        void LogArraySizeError(size_t expected, size_t provided) {
            OSReport("Error: Array argument underflow. Expected %u elements, but got %u. Command aborted.\n", expected, provided);
        }

        void LogArraySizeWarning(size_t expected, size_t provided) {
            OSReport("Warning: Array argument overflow. Expected %u elements, but got %u. Extra elements will be ignored.\n", expected, provided);
        }

        void LogEnumError(const char *provided, const std::string &validOptions) {
            OSReport("Invalid enum argument '%s'. Expected %s\n", provided, validOptions.c_str());
        }

        void LogSignatureError(const char *name, int min, int provided, std::initializer_list<ArgDesc> args) {
            std::string sig = "(";
            const char *sep = "";
            for (const auto &arg : args) {
                sig += sep;
                if (arg.is_optional) {
                    sig += "[";
                    sig += arg.name;
                    sig += "]";
                } else {
                    sig += arg.name;
                }
                sep = ", ";
            }
            sig += ")";
            LogCommandError(name, min, provided, sig);
        }

        std::string GenerateUsageString(const char *name, std::initializer_list<ArgDesc> args) {
            std::string desc = "Usage: ";
            desc += name;
            if (args.size() == 0) {
                desc += " (no args)";
            } else {
                for (const auto &arg : args) {
                    desc += " ";
                    if (arg.is_optional) {
                        desc += "[";
                        desc += arg.name;
                        desc += "]";
                    } else {
                        desc += "<";
                        desc += arg.name;
                        desc += ">";
                    }
                }
            }
            return desc;
        }

        size_t CountCSVTokens(const char *str) {
            if (!str || *str == '\0') return 0;
            size_t count = 1;
            bool inQuote = false;
            for (const char *p = str; *p; ++p) {
                const char c = *p;
                if (inQuote && c == '\\') {
                    char next = *(p + 1);
                    if (next == '"' || next == '\\') {
                        p++;
                        continue;
                    }
                }
                if (c == '"') {
                    inQuote = !inQuote;
                    continue;
                }
                if (c == ',' && !inQuote) count++;
            }
            return count;
        }

        std::vector<std::string> SplitCSV(const char *str) {
            std::vector<std::string> tokens;
            if (!str || *str == '\0') return tokens;
            std::string currentToken;
            bool inQuote  = false;
            const char *p = str;
            while (*p) {
                const char c = *p;
                if (c == '\\') {
                    char next = *(p + 1);
                    if (next == '"') {
                        currentToken += '"';
                        p += 2;
                        continue;
                    } else if (next == '\\') {
                        currentToken += '\\';
                        p += 2;
                        continue;
                    }
                }
                if (c == '"') {
                    inQuote = !inQuote;
                } else if (c == ',' && !inQuote) {
                    tokens.push_back(currentToken);
                    currentToken.clear();
                } else {
                    currentToken += c;
                }
                p++;
            }
            tokens.push_back(currentToken);
            return tokens;
        }
    } // namespace internal

    std::optional<Command> CommandRegistry::AddRaw(const char *name, std::function<void(int, char **)> handler, const char *description, const char *usage, IOPShellModule_Error *outError) {
        // Store the C++ handler in the map
        {
            std::lock_guard lock(GetMutex());
            GetHandlerMap()[name] = std::move(handler);
        }

        // Register the Global Dispatcher with the C API
        // This dispatcher looks up the handler in the map and calls it.
        IOPShellModule_Error res = IOPShellModule_AddCommand(name, &GlobalLambdaDispatcher, description, usage);

        if (res == IOPSHELL_MODULE_ERROR_SUCCESS) {
            if (outError) *outError = IOPSHELL_MODULE_ERROR_SUCCESS;
            return Command(name); // CommandRegistry is a friend of Command, so this works
        }

        // Cleanup if registration failed
        {
            std::lock_guard lock(GetMutex());
            GetHandlerMap().erase(name);
        }

        if (outError) *outError = res;
        return std::nullopt;
    }

    CommandGroup::CommandGroup(std::string name, std::string description)
        : mName(std::move(name)), mDescription(std::move(description)) {}

    CommandGroup::~CommandGroup() = default;

    void CommandGroup::AddRawHandler(const char *name, std::function<void(int, char **)> handler, const char *desc, std::string usage) {
        if (!name) return;
        mHandlers[name] = std::move(handler);
        mHelp[name]     = {desc ? desc : "", std::move(usage)};
    }

    std::optional<Command> CommandGroup::Register() {
        // Use AddRaw to register the lambda with skipped template deduction and argument parsing logic.
        return CommandRegistry::AddRaw(
                mName.c_str(), [this](int argc, char **argv) {
                    this->Dispatch(argc, argv);
                },
                mDescription.c_str(), (std::string("Type '") + mName + " help' to see subcommands.").c_str());
    }

    void CommandGroup::Dispatch(int argc, char **argv) {
        // argv[0] is the main command (e.g., "plugins")
        // argv[1] is the sub-command (e.g., "show")

        if (argc < 2) {
            PrintHelp();
            return;
        }

        std::string sub = argv[1];

        // Built-in help check
        if (sub == "help" || sub == "--help" || sub == "-h") {
            PrintHelp();
            return;
        }

        auto it = mHandlers.find(sub);
        if (it != mHandlers.end()) {
            // Shift arguments:
            // Sub-command handler expects argv[0] to be its own name
            // "plugins show 123" -> "show 123"
            it->second(argc - 1, argv + 1);
        } else {
            OSReport("Unknown subcommand '%s'. Try '%s help'.\n", sub.c_str(), mName.c_str());
        }
    }

    void CommandGroup::PrintHelp() {
        OSReport("Usage: aroma %s <subcommand> [args]\n", mName.c_str());
        OSReport("Subcommands:\n");

        for (const auto &[name, info] : mHelp) {
            char buf[256];
            snprintf(buf, sizeof(buf), "  %-20s %-20s %s", name.c_str(), info.desc.c_str(), info.usage.c_str());
            OSReport("\t - %s\n", buf);
        }
    }
} // namespace IOPShellModule
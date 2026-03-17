#pragma once

#include "command.h"
#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief returns a string literal corresponding to the specified error code.
 *
 * Useful for logging or debugging purposes. The returned string is a static literal
 * and does not need to be freed.
 *
 * @param[in] status  The error code to translate.
 * @return A constant string pointer (e.g., "IOPSHELL_MODULE_ERROR_SUCCESS").
 * Returns "IOPSHELL_MODULE_ERROR_UNKNOWN_ERROR" if the status is unrecognized.
 */
const char *IOPShellModule_GetStatusStr(IOPShellModule_Error status);

/**
 * @brief Initializes the IOPShellModule client interface.
 *
 * This function attempts to locate the backend module and resolve necessary symbols.
 * It is idempotent; calling it multiple times has no side effects and immediately returns success.
 *
 * @pre This function must be successfully executed before calling any other library functions
 * (except IOPShellModule_GetVersion, which can lazily load the module).
 *
 * @return IOPShellModule_Error
 * @retval IOPSHELL_MODULE_ERROR_SUCCESS                  Initialization successful (or already initialized).
 * @retval IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND         The backend module could not be located. Ensure the module is loaded.
 * @retval IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT    The backend module is missing required API exports.
 * @retval IOPSHELL_MODULE_ERROR_UNSUPPORTED_API_VERSION  The backend module version is incompatible with this client.
 */
IOPShellModule_Error IOPShellModule_InitLibrary();

/**
 * @brief Finalizes the IOPShellModule client interface.
 *
 * Releases any held handles and resets the library state.
 * It is safe to call this function even if the library was not previously initialized.
 *
 * @return IOPShellModule_Error
 * @retval IOPSHELL_MODULE_ERROR_SUCCESS  Always returns success.
 */
IOPShellModule_Error IOPShellModule_DeInitLibrary();

/**
 * @brief Retrieves the API Version of the underlying IOPShellModule backend.
 *
 * If the library has not yet been initialized via IOPShellModule_InitLibrary(),
 * this function attempts to lazily acquire the module handle to retrieve the version.
 *
 * @param[out] outVersion Pointer to a variable where the version number will be written.
 *
 * @return IOPShellModule_Error
 * @retval IOPSHELL_MODULE_ERROR_SUCCESS                  Version successfully retrieved.
 * @retval IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT         The outVersion pointer is NULL.
 * @retval IOPSHELL_MODULE_ERROR_MODULE_NOT_FOUND         The backend module could not be found.
 * @retval IOPSHELL_MODULE_ERROR_MODULE_MISSING_EXPORT    The backend module does not export a version identifier.
 */
IOPShellModule_Error IOPShellModule_GetVersion(IOPShellModule_APIVersion *outVersion);

/**
 * @brief Registers a new command with the shell.
 *
 * Adds a command string and its associated callback function to the global registry.
 *
 * @param[in] cmdName      The command string that triggers the callback (e.g., "status"). Must not be NULL.
 * @param[in] cb           The function pointer to call when the command is executed. Must not be NULL.
 * @param[in] description  A short help description for the command. Can be NULL.
 * @param[in] usage        A short usage description for the command. Can be NULL.
 *
 * @return IOPShellModule_Error
 * @retval IOPSHELL_MODULE_ERROR_SUCCESS              Command successfully registered.
 * @retval IOPSHELL_MODULE_ERROR_ALREADY_EXISTS       The command name is already registered.
 * @retval IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT     `cmdName` or `cb` was NULL.
 * @retval IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED    The library has not been initialized.
 * @retval IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND  The backend version is too old or missing the register export.
 */
IOPShellModule_Error IOPShellModule_AddCommand(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage);
IOPShellModule_Error IOPShellModule_AddCommandEx(const char *cmdName, IOPShell_CommandCallback cb, const char *description, const char *usage, bool showInHelp);

/**
 * @brief Removes a previously registered command from the shell.
 *
 * @param[in] cmdName The name of the command to unregister.
 *
 * @return IOPShellModule_Error
 * @retval IOPSHELL_MODULE_ERROR_SUCCESS              Command successfully removed (or did not exist).
 * @retval IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT     `cmdName` was NULL.
 * @retval IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED    The library has not been initialized.
 * @retval IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND  The backend version is too old or missing the remove export.
 */
IOPShellModule_Error IOPShellModule_RemoveCommand(const char *cmdName);

/**
 * @brief Retrieves the list of registered shell commands.
 *
 * This function is typically used in a two-step process:
 * 1. Call with `outList = NULL` to retrieve the total number of commands in `outCount`.
 * 2. Allocate a buffer of that size.
 * 3. Call again with the allocated buffer to retrieve the actual data.
 *
 * @param[out] outList     Pointer to an array of IOPShellModule_CommandEntry where the results will be written.
 * Pass NULL to query the total number of commands.
 * @param[in]  bufferSize  The maximum number of entries that can be written to `outList`.
 * Ignored if `outList` is NULL.
 * @param[out] outCount    Pointer to a variable that receives the count.
 * - If `outList` is NULL: Receives the total number of registered commands.
 * - If `outList` is valid: Receives the actual number of commands written to the buffer.
 *
 * @return IOPShellModule_Error
 * @retval IOPSHELL_MODULE_ERROR_SUCCESS              Operation successful.
 * @retval IOPSHELL_MODULE_ERROR_INVALID_ARGUMENT     Invalid arguments provided (e.g., both `outList` and `outCount` are NULL).
 * @retval IOPSHELL_MODULE_ERROR_LIB_UNINITIALIZED    The library is not initialized or the version is invalid.
 * @retval IOPSHELL_MODULE_ERROR_UNSUPPORTED_COMMAND  The loaded module version is too old (< 1) or the export symbol was not found.
 */
IOPShellModule_Error IOPShellModule_ListCommands(IOPShellModule_CommandEntry *outList, uint32_t bufferSize, uint32_t *outCount);

#ifdef __cplusplus
}
#endif

/* ================================================================================== */
/* C++ API WRAPPER                                                                    */
/* ================================================================================== */

#ifdef __cplusplus

#include <array>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOPShellModule {
    /**
     * @brief C++ Wrapper for initializing the library.
     * @see IOPShellModule_InitLibrary
     */
    IOPShellModule_Error Init();

    /**
     * @brief C++ Wrapper for de-initializing the library.
     * @see IOPShellModule_DeInitLibrary
     */
    IOPShellModule_Error DeInit();

    /**
     * @brief C++ Wrapper to get the string representation of an error.
     * @see IOPShellModule_GetStatusStr
     */
    const char *GetErrorString(IOPShellModule_Error error);

    /**
     * @brief Retrieves the API version as a simple integer.
     * @return The API version, or -1 if an error occurred (e.g., module not found).
     * @see IOPShellModule_GetVersion
     */
    int32_t GetVersion();

    /**
     * @brief Enum Traits helper.
     * Specialized via the IOPSHELL_REGISTER_ENUM macro to provide reflection data.
     */
    template<typename T>
    struct EnumTraits {
        static constexpr bool IsRegistered = false;

        // Dummy implementations to prevent cascading compiler errors when static_assert fails
        static bool FromString(std::string_view, T &) { return false; }
        static std::vector<std::string_view> GetValidNames() { return {}; }
        static std::string GetTypeName() { return "unknown_enum"; }
    };

    namespace internal {
        bool IsEqualCaseInsensitive(std::string_view a, std::string_view b);
        size_t CountCSVTokens(const char *str);
        std::vector<std::string> SplitCSV(const char *str);

        void LogCommandError(const char *commandName, int expected, int provided, const std::string &signature);
        void LogArraySizeError(size_t expected, size_t provided);
        void LogArraySizeWarning(size_t expected, size_t provided);
        void LogEnumError(const char *provided, const std::string &validOptions);


        struct ArgDesc {
            std::string_view name;
            bool is_optional;
        };

        void LogSignatureError(const char *name, int min, int provided, std::initializer_list<ArgDesc> args);
        std::string GenerateUsageString(const char *name, std::initializer_list<ArgDesc> args);

        // --- Optional Trait ---
        template<typename T>
        struct is_optional : std::false_type {};
        template<typename T>
        struct is_optional<std::optional<T>> : std::true_type {};

        // --- Check Argument Ordering ---
        template<typename... Args>
        constexpr bool CheckOptionalOrdering() {
            bool foundOptional          = false;
            bool valid                  = true;
            [[maybe_unused]] auto check = [&](bool isOpt) {
                if (foundOptional && !isOpt) {
                    valid = false;
                }
                if (isOpt) {
                    foundOptional = true;
                }
            };
            ((check(is_optional<std::decay_t<Args>>::value)), ...);
            return valid;
        }

        // --- Argument Validator Trait ---
        // Used to check arguments BEFORE calling the function.
        template<typename T, typename Enable = void>
        struct ArgValidator {
            static bool Check(const char *str) {
                // By default, a missing argument (nullptr) is a failure.
                return (str != nullptr);
            }
        };

        // Specialization for std::optional
        template<typename T>
        struct ArgValidator<std::optional<T>> {
            static bool Check(const char *str) {
                // If missing (nullptr), it is valid (will be nullopt).
                if (!str) {
                    return true;
                }
                // If present, validate the inner type.
                return ArgValidator<T>::Check(str);
            }
        };

        // Specialization for Enums
        template<typename T>
        struct ArgValidator<T, std::enable_if_t<std::is_enum_v<T>>> {
            static bool Check(const char *str) {
                static_assert(EnumTraits<T>::IsRegistered, "Enum used in command is not registered via IOPSHELL_REGISTER_ENUM");
                if (!str || *str == '\0') {
                    LogEnumError("(empty)", EnumTraits<T>::GetTypeName());
                    return false;
                }

                if (T val; EnumTraits<T>::FromString(str, val)) {
                    return true;
                }

                // Log error
                LogEnumError(str, EnumTraits<T>::GetTypeName());
                return false;
            }
        };

        // Specialization for std::array (Enforce Size)
        template<typename T, size_t N>
        struct ArgValidator<std::array<T, N>> {
            static bool Check(const char *str) {
                if (!str || *str == '\0') {
                    if (N > 0) {
                        LogArraySizeError(N, 0); // Fatal
                        return false;
                    }
                    return true;
                }

                const size_t tokenCount = CountCSVTokens(str);
                if (tokenCount < N) {
                    LogArraySizeError(N, tokenCount);
                    return false;
                }

                if (tokenCount > N) {
                    LogArraySizeWarning(N, tokenCount);
                }

                return true;
            }
        };

        // --- Argument Parser Struct ---
        template<typename...>
        struct always_false : std::false_type {};

        template<typename T, typename Enable = void>
        struct ArgParser {
            static T Parse(const char *str) {
                static_assert(always_false<T>::value, "LibShell: Unsupported argument type.");
                return T();
            }
        };

        // Specialization for std::optional
        template<typename T>
        struct ArgParser<std::optional<T>> {
            static std::optional<T> Parse(const char *str) {
                if (!str) return std::nullopt;
                return ArgParser<T>::Parse(str);
            }
        };

        // Specialization for Enums
        template<typename T>
        struct ArgParser<T, std::enable_if_t<std::is_enum_v<T>>> {
            static T Parse(const char *str) {
                T val{};
                EnumTraits<T>::FromString(str, val); // validation already done by Validator
                return val;
            }
        };

        // --- Integers & Floats ---
        template<>
        struct ArgParser<int> {
            static int Parse(const char *str) { return static_cast<int>(std::strtol(str, nullptr, 0)); }
        };
        template<>
        struct ArgParser<long> {
            static long Parse(const char *str) { return std::strtol(str, nullptr, 0); }
        };
        template<>
        struct ArgParser<int64_t> {
            static int64_t Parse(const char *str) { return std::strtoll(str, nullptr, 0); }
        };
        template<>
        struct ArgParser<uint32_t> {
            static uint32_t Parse(const char *str) { return static_cast<uint32_t>(std::strtoul(str, nullptr, 0)); }
        };
        template<>
        struct ArgParser<uint64_t> {
            static uint64_t Parse(const char *str) { return std::strtoull(str, nullptr, 0); }
        };
        template<>
        struct ArgParser<int8_t> {
            static int8_t Parse(const char *str) { return static_cast<int8_t>(std::strtol(str, nullptr, 0)); }
        };
        template<>
        struct ArgParser<int16_t> {
            static int16_t Parse(const char *str) { return static_cast<int16_t>(std::strtol(str, nullptr, 0)); }
        };
        template<>
        struct ArgParser<uint16_t> {
            static uint16_t Parse(const char *str) { return static_cast<uint16_t>(std::strtoul(str, nullptr, 0)); }
        };
        template<>
        struct ArgParser<uint8_t> {
            static uint8_t Parse(const char *str) { return static_cast<uint8_t>(std::strtoul(str, nullptr, 0)); }
        };

        template<>
        struct ArgParser<float> {
            static float Parse(const char *str) { return std::strtof(str, nullptr); }
        };
        template<>
        struct ArgParser<double> {
            static double Parse(const char *str) { return std::strtod(str, nullptr); }
        };

        // --- Strings ---
        template<>
        struct ArgParser<std::string> {
            static std::string Parse(const char *str) { return std::string(str ? str : ""); }
        };
        template<>
        struct ArgParser<std::string_view> {
            static std::string_view Parse(const char *str) { return std::string_view(str ? str : ""); }
        };
        template<>
        struct ArgParser<const char *> {
            static const char *Parse(const char *str) { return str; }
        };
        template<>
        struct ArgParser<char *> {
            static char *Parse(const char *str) { return const_cast<char *>(str); }
        };

        // --- Boolean ---
        template<>
        struct ArgParser<bool> {
            static bool Parse(const char *str) {
                if (!str) return false;
                const std::string_view sv(str);

                // Check for "1"
                if (sv == "1") return true;

                // Check for "t" or "true" (case-insensitive)
                return IsEqualCaseInsensitive(sv, "t") || IsEqualCaseInsensitive(sv, "true");
            }
        };

        // --- Containers (std::vector) ---
        template<typename T>
        struct ArgParser<std::vector<T>> {
            static std::vector<T> Parse(const char *str) {
                if (!str || *str == '\0') return {};
                const std::vector<std::string> tokens = SplitCSV(str);
                std::vector<T> result;
                result.reserve(tokens.size());
                for (const auto &token : tokens) {
                    result.push_back(ArgParser<T>::Parse(token.c_str()));
                }
                return result;
            }
        };

        // --- Containers (std::array) ---
        template<typename T, size_t N>
        struct ArgParser<std::array<T, N>> {
            static std::array<T, N> Parse(const char *str) {
                std::array<T, N> result{};
                if (!str || *str == '\0') return result;

                const std::vector<std::string> tokens = SplitCSV(str);
                // We only parse up to N, excess is ignored (warning already logged by Validator)
                for (size_t i = 0; i < N && i < tokens.size(); ++i) {
                    result[i] = ArgParser<T>::Parse(tokens[i].c_str());
                }
                return result;
            }
        };

        // --- Type Naming Traits ---
        template<typename T, typename Enable = void>
        struct TypeName {
            static std::string Name() { return "unknown"; }
        };

        // Specialization for Enums
        template<typename T>
        struct TypeName<T, std::enable_if_t<std::is_enum_v<T>>> {
            static std::string Name() {
                return EnumTraits<T>::GetTypeName();
            }
        };

        // Specialization for Optionals
        template<typename T>
        struct TypeName<std::optional<T>> {
            static auto Name() {
                return TypeName<T>::Name();
            }
        };

        // Optimized Basic Types: Return constexpr string_view to avoid allocation in thunk
        template<>
        struct TypeName<int> {
            static constexpr std::string_view Name() { return "int"; }
        };
        template<>
        struct TypeName<long> {
            static constexpr std::string_view Name() { return "long"; }
        };
        template<>
        struct TypeName<uint32_t> {
            static constexpr std::string_view Name() { return "uint32"; }
        };
        template<>
        struct TypeName<uint64_t> {
            static constexpr std::string_view Name() { return "uint64"; }
        };
        template<>
        struct TypeName<uint8_t> {
            static constexpr std::string_view Name() { return "uint8"; }
        };
        template<>
        struct TypeName<int8_t> {
            static constexpr std::string_view Name() { return "int8"; }
        };
        template<>
        struct TypeName<int16_t> {
            static constexpr std::string_view Name() { return "int16"; }
        };
        template<>
        struct TypeName<int64_t> {
            static constexpr std::string_view Name() { return "int64"; }
        };
        template<>
        struct TypeName<float> {
            static constexpr std::string_view Name() { return "float"; }
        };
        template<>
        struct TypeName<double> {
            static constexpr std::string_view Name() { return "double"; }
        };
        template<>
        struct TypeName<bool> {
            static constexpr std::string_view Name() { return "bool"; }
        };
        template<>
        struct TypeName<const char *> {
            static constexpr std::string_view Name() { return "string"; }
        };
        template<>
        struct TypeName<char *> {
            static constexpr std::string_view Name() { return "string"; }
        };
        template<>
        struct TypeName<std::string> {
            static constexpr std::string_view Name() { return "string"; }
        };
        template<>
        struct TypeName<std::string_view> {
            static constexpr std::string_view Name() { return "string"; }
        };

        template<typename T>
        struct TypeName<std::vector<T>> {
            static std::string Name() {
                return "list<" + std::string(TypeName<T>::Name()) + ">";
            }
        };

        template<typename T, size_t N>
        struct TypeName<std::array<T, N>> {
            static std::string Name() {
                return "list<" + std::string(TypeName<T>::Name()) + ">[" + std::to_string(N) + "]";
            }
        };

        template<typename... Args>
        constexpr int CountRequired() {
            // Count arguments that are NOT optional
            return ((is_optional<std::decay_t<Args>>::value ? 0 : 1) + ... + 0);
        }

        template<typename... Args>
        struct ArgProcessor {
            static constexpr size_t Arity = sizeof...(Args);

            // Ensure valid ordering: Required args cannot follow Optional args
            static_assert(CheckOptionalOrdering<Args...>(), "Invalid Argument Order: Required arguments cannot follow Optional arguments. Once an optional argument is used, all subsequent arguments must also be optional.");

            static std::string GetUsage(const char *name) {
                return internal::GenerateUsageString(name, {internal::ArgDesc{internal::TypeName<std::decay_t<Args>>::Name(), internal::is_optional<std::decay_t<Args>>::value}...});
            }

            // Universal Call
            template<typename Callable>
            static void Call(Callable &&f, int argc, char **argv) {
                const int provided        = argc - 1;
                constexpr int minRequired = CountRequired<Args...>();

                if (provided < minRequired) {
                    internal::LogSignatureError((argc > 0 ? argv[0] : "unknown"), minRequired, provided,
                                                {internal::ArgDesc{internal::TypeName<std::decay_t<Args>>::Name(), internal::is_optional<std::decay_t<Args>>::value}...});
                    return;
                }

                if (!Validate(argc, argv, std::make_index_sequence<Arity>{})) {
                    return;
                }

                Unpack(std::forward<Callable>(f), argc, argv, std::make_index_sequence<Arity>{});
            }

            template<size_t... Is>
            static bool Validate(int argc, char **argv, std::index_sequence<Is...>) {
                [[maybe_unused]] auto safe_get = [&](size_t i) -> const char * { return (static_cast<int>(i) < argc) ? argv[i] : nullptr; };
                return (internal::ArgValidator<std::decay_t<Args>>::Check(safe_get(Is + 1)) && ...);
            }

            template<typename Callable, size_t... Is>
            static void Unpack(Callable &&f, int argc, char **argv, std::index_sequence<Is...>) {
                [[maybe_unused]] auto safe_get = [&](size_t i) -> const char * { return (static_cast<int>(i) < argc) ? argv[i] : nullptr; };
                f(internal::ArgParser<std::decay_t<Args>>::Parse(safe_get(Is + 1))...);
            }
        };

        template<typename T>
        struct FunctionTraits;

        template<typename R, typename... Args>
        struct FunctionTraits<R(Args...)> : ArgProcessor<Args...> {
            using Signature = R(Args...);
        };

        template<typename T>
        struct SignatureDeducer : SignatureDeducer<decltype(&T::operator())> {};

        template<typename C, typename R, typename... Args>
        struct SignatureDeducer<R (C::*)(Args...) const> {
            using Signature = R(Args...);
        };

        template<typename C, typename R, typename... Args>
        struct SignatureDeducer<R (C::*)(Args...)> {
            using Signature = R(Args...);
        };

        template<typename R, typename... Args>
        struct SignatureDeducer<R (*)(Args...)> {
            using Signature = R(Args...);
        };

    } // namespace internal

    class CommandRegistry {
    public:
        static std::map<std::string, std::function<void(int, char **)>> &GetHandlerMap();

        static std::recursive_mutex &GetMutex();

        static void GlobalLambdaDispatcher(int argc, char **argv);

        /**
         * @brief Registers a C++ function as a shell command.
         *
         * This template function automatically generates a C-compatible wrapper for your C++ function.
         * Arguments passed to the command in the shell are automatically parsed and converted
         * to the types expected by your function signature.
         *
         * Supported types include:
         * - Integers (int, long, uint32_t, uint64_t, etc.)
         * - Floats (float, double)
         * - Strings (std::string, const char*)
         * - Booleans (bool - supports "true", "1", "t")
         * - Lists (std::vector<T> - comma separated values)
         * - Fixed Lists (std::array<T,N> - enforces size N)
         * - Enums (requires IOPSHELL_REGISTER_ENUM)
         * - Optionals (std::optional<T> - passed as nullopt if missing)
         *
         * @tparam Func A pointer to the C++ function you wish to register.
         * @param name The command name to register in the shell.
         * @param description Optional description.
         * @param usage Optional usage. If nullptr, a usage string is automatically generated.
         * @param outError Optional pointer to receive the error code if registration fails.
         * @return std::optional<Command> An RAII handle to the command on success, or std::nullopt on failure.
         */
        template<auto Func>
        [[nodiscard]] static std::optional<Command> Add(const char *name, const char *description = nullptr, const char *usage = nullptr, IOPShellModule_Error *outError = nullptr) {
            IOPShellModule_Error res;
            if (description) {
                res = IOPShellModule_AddCommand(name, &FunctionThunk<Func>::Call, description, usage);
            } else {
                res = FunctionThunk<Func>::RegisterAutoUsage(name, description);
            }

            if (res == IOPSHELL_MODULE_ERROR_SUCCESS) {
                if (outError) {
                    *outError = IOPSHELL_MODULE_ERROR_SUCCESS;
                }
                return Command(name);
            }

            if (outError) {
                *outError = res;
            }
            return std::nullopt;
        }

        /**
         * @brief Registers a Lambda with AUTOMATIC signature deduction.
         *
         * This template function automatically generates a C-compatible wrapper for your C++ function.
         * Arguments passed to the command in the shell are automatically parsed and converted
         * to the types expected by your function signature.
         *
         * Supported types include:
         * - Integers (int, long, uint32_t, uint64_t, etc.)
         * - Floats (float, double)
         * - Strings (std::string, const char*)
         * - Booleans (bool - supports "true", "1", "t")
         * - Lists (std::vector<T> - comma separated values)
         * - Fixed Lists (std::array<T,N> - enforces size N)
         * - Enums (requires IOPSHELL_REGISTER_ENUM)
         * - Optionals (std::optional<T> - passed as nullopt if missing)
         *
         * Allows writing: Add("cmd", [](int a){ ... });
         *
         * @tparam FuncT The type of the lambda (automatically deduced).
         * @param name The command name.
         * @param lambda The lambda/functor to register.
         * @param description Optional description.
         * @param usage Optional usage.
         * @param outError Optional pointer to receive the error code if registration fails.
         * @return std::optional<Command> An RAII handle to the command on success, or std::nullopt on failure.
         */
        template<typename FuncT>
        [[nodiscard]] static std::optional<Command> Add(const char *name, FuncT &&lambda, const char *description = nullptr, const char *usage = nullptr, IOPShellModule_Error *outError = nullptr) {
            // Automatically deduce the signature from the lambda's operator()
            using DeducedSig = typename internal::SignatureDeducer<std::decay_t<FuncT>>::Signature;
            return RegisterLambda<DeducedSig>(name, std::forward<FuncT>(lambda), description, usage, outError);
        }

        /**
         * @brief Registers a Lambda with EXPLICIT signature.
         *
         * This template function automatically generates a C-compatible wrapper for your C++ function.
         * Arguments passed to the command in the shell are automatically parsed and converted
         * to the types expected by your function signature.
         *
         * Supported types include:
         * - Integers (int, long, uint32_t, uint64_t, etc.)
         * - Floats (float, double)
         * - Strings (std::string, const char*)
         * - Booleans (bool - supports "true", "1", "t")
         * - Lists (std::vector<T> - comma separated values)
         * - Fixed Lists (std::array<T,N> - enforces size N)
         * - Enums (requires IOPSHELL_REGISTER_ENUM)
         * - Optionals (std::optional<T> - passed as nullopt if missing)
         *
         * Necessary for generic lambdas or if you want to be explicit.
         * Usage: Add<void(int)>("cmd", [](auto a){ ... });
         *
         * @tparam Signature The explicit function signature.
         * @tparam FuncT The lambda type.
         * @param name The command name.
         * @param lambda The lambda/functor to register.
         * @param description Optional description.
         * @param usage Optional usage.
         * @param outError Optional pointer to receive the error code if registration fails.
         * @return std::optional<Command> An RAII handle to the command on success, or std::nullopt on failure.
         */
        template<typename Signature, typename FuncT>
        [[nodiscard]] static std::optional<Command> Add(const char *name, FuncT &&lambda, const char *description = nullptr, const char *usage = nullptr, IOPShellModule_Error *outError = nullptr) {
            return RegisterLambda<Signature>(name, std::forward<FuncT>(lambda), description, usage, outError);
        }

        /**
         * @brief Registers a raw C-style function (argc/argv).
         * \see IOPShellModule_AddCommand
         * @param name The command name.
         * @param cb The callback function.
         * @param description The command description.
         * @param usage The command usage.
         * @param outError Optional pointer to receive the error code if registration fails.
         * @return std::optional<Command> An RAII handle to the command on success, or std::nullopt on failure.
         */
        [[nodiscard]] static std::optional<Command> Add(const char *name, const IOPShell_CommandCallback cb, const char *description, const char *usage, IOPShellModule_Error *outError = nullptr) {
            const IOPShellModule_Error res = IOPShellModule_AddCommand(name, cb, description, usage);
            if (res == IOPSHELL_MODULE_ERROR_SUCCESS) {
                if (outError) *outError = IOPSHELL_MODULE_ERROR_SUCCESS;
                return Command(name);
            }
            if (outError) *outError = res;
            return std::nullopt;
        }

        /**
         * @brief Registers a raw command handler (argc/argv) that can be a capturing lambda.
         * Skips automatic argument parsing.
         */
        [[nodiscard]] static std::optional<Command> AddRaw(const char *name, std::function<void(int, char **)> handler, const char *description = nullptr, const char *usage = nullptr, IOPShellModule_Error *outError = nullptr);

        /**
         * @brief Registers an alias for an existing C++ command.
         * @param alias The new command name.
         * @param target The existing command name to point to.
         * @param outError Optional error output.
         * @return std::optional<Command> RAII handle for the alias.
         */
        [[nodiscard]] static std::optional<Command> AddAlias(const Command &target, const char *alias, IOPShellModule_Error *outError = nullptr);

        /**
         * @brief Unregisters a command.
         * \see IOPShellModule_RemoveCommand
         */
        static IOPShellModule_Error Remove(const char *name);

        /**
         * @brief Retrieves a list of all registered commands.
         * \see IOPShellModule_ListCommands
         */
        static std::vector<IOPShellModule_CommandEntry> List();

    private:
        template<auto Func>
        struct FunctionThunk;

        // Static Function Thunk
        template<typename... Args, void (*Func)(Args...)>
        struct FunctionThunk<Func> {
            using Processor = internal::ArgProcessor<Args...>;

            static IOPShellModule_Error RegisterAutoUsage(const char *name, const char *description) {
                const std::string usage = Processor::GetUsage(name);
                return IOPShellModule_AddCommand(name, &Call, description, usage.c_str());
            }

            static void Call(int argc, char **argv) {
                Processor::Call(Func, argc, argv);
            }
        };

        // Internal helper to handle the common logic of registering a lambda/functor
        template<typename Signature, typename FuncT>
        static std::optional<Command> RegisterLambda(const char *name, FuncT &&lambda, const char *description, const char *usage, IOPShellModule_Error *outError) {
            using Traits = internal::FunctionTraits<Signature>;

            // Wrap in shared_ptr for lifetime safety
            auto shared_lambda = std::make_shared<std::decay_t<FuncT>>(std::forward<FuncT>(lambda));

            {
                std::lock_guard lock(GetMutex());
                GetHandlerMap()[name] = [shared_lambda](int argc, char **argv) {
                    Traits::Call(*shared_lambda, argc, argv);
                };
            }

            IOPShellModule_Error res;
            if (description) {
                res = IOPShellModule_AddCommand(name, &GlobalLambdaDispatcher, description, usage);
            } else {
                const std::string generatedUsage = Traits::GetUsage(name);
                res                              = IOPShellModule_AddCommand(name, &GlobalLambdaDispatcher, description, generatedUsage.c_str());
            }

            if (res == IOPSHELL_MODULE_ERROR_SUCCESS) {
                if (outError) *outError = IOPSHELL_MODULE_ERROR_SUCCESS;
                return Command(name);
            }

            // Clean up the handler map entry if C API failed
            {
                std::lock_guard lock(GetMutex());
                GetHandlerMap().erase(name);
            }

            if (outError) *outError = res;
            return std::nullopt;
        }
    };

    /**
     * @brief Manages a group of sub-commands under a single main command.
     * Example: 'plugins help', 'plugins list'.
     */
    class CommandGroup {
    public:
        CommandGroup(std::string name, std::string description = "");
        ~CommandGroup();

        // Delete copy/move to ensure 'this' captured in RegisterGroup() remains valid.
        CommandGroup(const CommandGroup &) = delete;
        CommandGroup(CommandGroup &&)      = delete;
        CommandGroup &operator=(const CommandGroup &) = delete;

        /**
         * @brief Nests another CommandGroup as a subcommand.
         * On success, this group takes ownership of childGroup.
         * On failure, ownership remains with the caller.
         */
        IOPShellModule_Error AddSubGroup(std::unique_ptr<CommandGroup> childGroup);

        /**
         * @brief Registers a raw command handler (argc/argv).
         * Bypasses the library's automatic argument parsing and type deduction.
         */
        IOPShellModule_Error AddRawCommand(const char *name, std::function<void(int, char **)> handler, const char *description = nullptr, const char *usage = nullptr) {
            return AddRawHandler(name, std::move(handler), description, usage ? usage : "");
        }

        /**
         * @brief Registers a sub-command with automatic type deduction.
         */
        template<typename FuncT>
        IOPShellModule_Error AddCommand(const char *subCmdName, FuncT &&lambda, const char *description = nullptr) {
            using Sig = typename internal::SignatureDeducer<std::decay_t<FuncT>>::Signature;
            return AddCommandExplicit<Sig>(subCmdName, std::forward<FuncT>(lambda), description);
        }
        /**
         * @brief Registers an alias for an existing command object.
         * Creates a new shell command that points to the same underlying handler
         * as the target command.
         * @param target The existing Command object to alias.
         * @param alias The new command name for the alias.
         * @return IOPShellModule_Error
        */
        IOPShellModule_Error AddAlias(const char *target, const char *alias);

        /**
         * @brief Registers a sub-command with an explicit signature.
         */
        template<typename Signature, typename FuncT>
        IOPShellModule_Error AddCommandExplicit(const char *subCmdName, FuncT &&lambda, const char *description = nullptr) {
            using Traits = internal::FunctionTraits<Signature>;

            auto shared_lambda = std::make_shared<std::decay_t<FuncT>>(std::forward<FuncT>(lambda));
            auto handler       = [shared_lambda](int argc, char **argv) {
                Traits::Call(*shared_lambda, argc, argv);
            };

            std::string usage = Traits::GetUsage(subCmdName);

            return AddRawHandler(subCmdName, std::move(handler), description, std::move(usage));
        }

        /**
         * @brief Registers the MAIN command with the IOPShell.
         * Call this after adding all sub-commands.
         */
        [[nodiscard]] IOPShellModule_Error RegisterGroup();

        /**
         * @brief Removes the MAIN command from the IOPShell.
         */
        [[nodiscard]] IOPShellModule_Error RemoveGroup();

    private:
        struct HelpEntry {
            std::string desc;
            std::string usage;
        };

        // Helper to keep implementation details out of the header template
        IOPShellModule_Error AddRawHandler(const char *name, std::function<void(int, char **)> handler, const char *desc, std::string usage);

        void Dispatch(int argc, char **argv);
        void PrintHelp();

        void UpdatePath(const std::string &parentPath);


        std::string mName;
        std::string mFullCmdPath;
        std::string mDescription;
        std::optional<Command> mCommand;

        std::map<std::string, std::function<void(int, char **)>> mHandlers;
        std::map<std::string, HelpEntry> mHelp;
        std::vector<std::unique_ptr<CommandGroup>> mSubGroups;
    };

} // namespace IOPShellModule

/**
 * @brief Registers an Enum for use with IOPShell.
 * Usage: IOPSHELL_REGISTER_ENUM(MyEnum, {MyEnum::Val1, "Val1"}, {MyEnum::Val2, "Val2"})
 */
#define IOPSHELL_REGISTER_ENUM(ENUM_TYPE, ...)                                                        \
    namespace IOPShellModule {                                                                        \
        template<>                                                                                    \
        struct EnumTraits<ENUM_TYPE> {                                                                \
            static constexpr bool IsRegistered = true;                                                \
            static const std::vector<std::pair<ENUM_TYPE, std::string_view>> &GetMap() {              \
                static const std::vector<std::pair<ENUM_TYPE, std::string_view>> map = {__VA_ARGS__}; \
                return map;                                                                           \
            }                                                                                         \
            static bool FromString(std::string_view s, ENUM_TYPE &out) {                              \
                for (const auto &[val, name] : GetMap()) {                                            \
                    if (internal::IsEqualCaseInsensitive(s, name)) {                                  \
                        out = val;                                                                    \
                        return true;                                                                  \
                    }                                                                                 \
                }                                                                                     \
                return false;                                                                         \
            }                                                                                         \
            static std::vector<std::string_view> GetValidNames() {                                    \
                std::vector<std::string_view> names;                                                  \
                for (const auto &[val, name] : GetMap()) names.push_back(name);                       \
                return names;                                                                         \
            }                                                                                         \
            static std::string GetTypeName() {                                                        \
                std::string s = "enum(";                                                              \
                for (const auto &[val, name] : GetMap()) {                                            \
                    s += name;                                                                        \
                    s += "|";                                                                         \
                }                                                                                     \
                if (s.back() == '|') s.pop_back();                                                    \
                s += ")";                                                                             \
                return s;                                                                             \
            }                                                                                         \
        };                                                                                            \
    }

#endif // __cplusplus
#pragma once

#ifdef __cplusplus

#include <string>

namespace IOPShellModule {
    class CommandRegistry;

    /**
     * @brief RAII wrapper for a registered command.
     * * Automatically unregisters the command when this object is destroyed.
     * Use [[nodiscard]] to ensure the object is kept alive.
     */
    class Command {
    public:
        ~Command();
        Command(const Command &) = delete;

        Command(Command &&src) noexcept;

        Command &operator=(const Command &) = delete;

        Command &operator=(Command &&src) noexcept;

    private:
        // Only CommandRegistry can create a valid Command instance
        friend class CommandRegistry;
        explicit Command(std::string name);

        void Reset();

        std::string mName;
    };

} // namespace IOPShellModule
#endif
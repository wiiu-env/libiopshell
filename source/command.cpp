#include "iopshell/command.h"

#include "iopshell/api.h"
#include "logger.h"

#include <coreinit/debug.h>

namespace IOPShellModule {
    Command::Command(Command &&src) noexcept {
        Reset();
        mName = std::move(src.mName);
    }

    Command &Command::operator=(Command &&src) noexcept {
        if (this != &src) {
            Reset();
            mName = std::move(src.mName);
        }
        return *this;
    }

    Command::Command(std::string name) : mName(std::move(name)) {}

    Command::~Command() {
        Reset();
    }

    void Command::Reset() {
        if (!mName.empty()) {
            CommandRegistry::Remove(mName.c_str());
            mName.clear();
        }
    }

} // namespace IOPShellModule
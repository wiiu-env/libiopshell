[![Publish Docker Image](https://github.com/wiiu-env/libiopshell/actions/workflows/push_image.yml/badge.svg)](https://github.com/wiiu-env/libiopshell/actions/workflows/push_image.yml)

# libiopshell

A client library for the [IOPShellModule](https://github.com/wiiu-env/IOPShellModule). This library allows Wii U
homebrew applications to register custom commands that can be executed via the IOPShell.

Commands registered via this library are added to the **aroma** shell. For example, if you register a command named
`set_color` with arguments `int, int, int`, you will execute it in the shell like this:

```
  aroma set_color 255 0 0
```

It provides both a raw **C API** and a modern, type-safe **C++ Wrapper** that handles argument parsing automatically.

Requires the **IOPShellModule** to be running via [WUMSLoader](https://github.com/wiiu-env/WUMSLoader).
Requires [wut](https://github.com/devkitPro/wut) for building.
Install via `make install`.

## Usage

### Makefile

Make sure to define `WUMS_ROOT` (if not already defined by your environment) and add the library to your compiler flags:

```
  WUMS_ROOT := $(DEVKITPRO)/wums
```

Add `-liopshell` to `LIBS` and `$(WUMS_ROOT)` to `LIBDIRS`.

### C++ API (Recommended)

The C++ wrapper (`IOPShellModule::CommandRegistry`) allows you to register standard C++ functions or lambdas. The
library automatically parses shell arguments into the types defined in your function signature.

#### Supported Types

* **Primitives:** `int`, `long`, `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `float`, `double`, `bool`, etc.
* **Strings:** `std::string`, `std::string_view`, `const char*`, `char*`.
* **Containers:**
    * `std::vector<T>`: Variable length, comma-separated lists (e.g., input: `10,20,30`).
    * `std::array<T, N>`: Fixed length lists (e.g., input: `255,255,0`).
* **Enums:** Custom enums (requires registration via macro).
* **Optionals:** `std::optional<T>`: Arguments that can be omitted by the user.

#### Automatic Help Generation

If you do not provide a custom description string, `libiopshell` automatically generates a usage string based on your
function signature:

* **Required** arguments appear in angle brackets: `<int>`
* **Optional** arguments appear in square brackets: `[int]`
* **Enums** display their valid options: `<enum(red|green|blue)>` (or `[enum(...)]` if optional).

#### Command Management (RAII)

The `CommandRegistry::Add` function returns a `std::optional<IOPShellModule::Command>`.

* **Success:** Contains a `Command` object.
* **Failure:** Returns `std::nullopt`.

**Important:** The `Command` object follows RAII principles. When it is destroyed (e.g., goes out of scope), the command
is **automatically unregistered** from the shell. You **must** store these objects (e.g., in a `std::vector<Command>`)
to keep the commands active.

#### Enums

To use a C++ enum as a command argument, you must register it using `IOPSHELL_REGISTER_ENUM` (usually in the global
scope). This enables automatic string-to-enum conversion (case-insensitive).

```
enum class ImageSource { TV, DRC, Both };

// Maps enum values to string inputs
IOPSHELL_REGISTER_ENUM(ImageSource,
    {ImageSource::TV, "tv"},
    {ImageSource::DRC, "drc"},
    {ImageSource::Both, "both"}
);
```

#### Optional Arguments

Use `std::optional<T>` for arguments that are not mandatory.
**Note:** Once an optional argument appears in the function signature, **all subsequent arguments must also be optional
** to ensure valid parsing logic.

```
// Valid:
void Cmd(int a, std::optional<int> b);

// Invalid (compile-time error via static_assert):
void Cmd(std::optional<int> a, int b);
```

### Extended Examples

Here are various ways to use the library, ranging from simple to complex.

#### 1. Basic & Enum Usage

Registers a simple command to take a screenshot.

```
#include <iopshell/api.h>
#include <vector>

// Define and Register Enum
enum class ImageFormat { BMP, PNG, JPG };
IOPSHELL_REGISTER_ENUM(ImageFormat, 
    {ImageFormat::BMP, "bmp"}, 
    {ImageFormat::PNG, "png"}, 
    {ImageFormat::JPG, "jpg"}
);

// keep commands in memory to keep them registered.
std::vector<IOPShellModule::Command> sCommands;

void Init() {
    if(IOPShellModule::Init() != IOPSHELL_MODULE_ERROR_SUCCESS) return;

    // Usage: aroma screenshot <enum(bmp|png|jpg)>
    auto maybe_cmd = IOPShellModule::CommandRegistry::Add("screenshot", [](ImageFormat fmt) {
        if (fmt == ImageFormat::PNG) {
            // Take PNG...
        }
    });
    
    if (maybe_cmd) sCommands.push_back(std::move(*maybe_cmd));
}

void DeInitShell() {
    sCommands.clear();
    IOPShellModule::DeInit();
}
```

#### 2. Vector (Variable Length Arguments)

Use `std::vector` to accept comma-separated lists of values.

```
// Usage: aroma sum 10,20,30,40
// Shell Input: sum 10,5,5
// Output: Sum is 20
void SumNumbers(std::vector<int> numbers) {
    int sum = 0;
    for (int n : numbers) sum += n;
    OSReport("Sum is %d\n", sum);
}

void Init() {
    [...]
    auto maybe_cmd = IOPShellModule::CommandRegistry::Add("sum", SumNumbers);
    if (maybe_cmd) sCommands.push_back(std::move(*maybe_cmd));
}
```

#### 3. Fixed Array (Strict Length)

Use `std::array` to enforce a specific number of elements in a comma-separated list.

```
// Usage: aroma set_bg <list<uint8>[3]>
// Shell Input: set_bg 255,0,0
void SetBackground(std::array<uint8_t, 3> rgb) {
    OSReport("Setting BG Color: R=%d G=%d B=%d\n", rgb[0], rgb[1], rgb[2]);
}

void Init() {
    [...]
    auto maybe_cmd = IOPShellModule::CommandRegistry::Add("set_bg", SetBackground);
    if (maybe_cmd) sCommands.push_back(std::move(*maybe_cmd));
}
```

#### 4. Complex Function (Mixed Types & Optionals)

A complex command combining strings, primitives, optional arguments, and enums.

```
enum class EntityType { Orc, Elf, Human };
IOPSHELL_REGISTER_ENUM(EntityType, 
    {EntityType::Orc, "orc"}, 
    {EntityType::Elf, "elf"}, 
    {EntityType::Human, "human"}
);

// Usage: aroma spawn <string> <enum(orc|elf|human)> <int> <int> [int] [bool]
//
// Examples:
//   aroma spawn Boss1 orc 100 200
//   aroma spawn Player1 elf 0 0 100 true
void SpawnEntity(std::string name, 
                 EntityType type, 
                 int x, 
                 int y, 
                 std::optional<int> health, 
                 std::optional<bool> isHostile) {
                 
    int hp = health.value_or(100); // Default 100 HP
    bool hostile = isHostile.value_or(true); // Default Hostile
    
    OSReport("Spawning %s (%s) at %d,%d. HP: %d, Hostile: %s\n", 
             name.c_str(), 
             (type == EntityType::Orc ? "Orc" : "Other"), 
             x, y, hp, 
             hostile ? "Yes" : "No");
}

void Init() {
    [...]
    auto maybe_cmd = IOPShellModule::CommandRegistry::Add("spawn", SpawnEntity);
    if (maybe_cmd) sCommands.push_back(std::move(*maybe_cmd));
}
```

#### 5. Command Groups (Sub-commands)

You can organize related commands under a single parent command using `CommandGroup`. This creates a structure similar to CLI tools (e.g., `git commit`, `git push`).

**Note:** The `CommandGroup` object internally manages its own `Command` registration and stores the sub-command handlers. 
The instance must **remain alive** (e.g., wrapped in a `std::unique_ptr` or as a static variable) for as long as you want the command to be available.

```cpp
#include <memory>

// 1. Define the Group pointer (Must remain in memory while active)
static std::unique_ptr<IOPShellModule::CommandGroup> sPluginsGroup;

void InitPluginsCommandGroup() {
    if(IOPShellModule::Init() != IOPSHELL_MODULE_ERROR_SUCCESS) return;

    // Instantiate the group "plugins"
    sPluginsGroup = std::make_unique<IOPShellModule::CommandGroup>("plugins", "Manage aroma plugins");

    // 2. Add Sub-commands
    // Usage: aroma plugins list
    if (const auto res = sPluginsGroup->AddCommand(
            "list", 
            []() { OSReport("Listing plugins...\n"); },
            "Lists all active plugins");
        res != IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSReport("Failed to add sub command \"list\"");
    }

    // Add aliases for subcommands
    if (const auto res = sPluginsGroup->AddAlias(
            "list", 
            "show"); 
        res != IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSReport("Failed to add alias \"show\" for command \"list\"");
    }

    // 3. Register the Main Command with the shell
    // The CommandGroup internally manages the RAII Command wrapper.
    if (const auto res = sPluginsGroup->RegisterGroup(); 
            res != IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSReport("Failed to register 'aroma plugins' command: %s\n", IOPShellModule::GetErrorString(res));
    }
}

void DeInitPluginsCommandGroup() {
    // Destroying the object automatically unregisters the main command 
    // and cleans up all subcommand handlers.
    sPluginsGroup.reset();
}
```

### C API

If you prefer pure C or need to interface from a C-only project:

```
#include <iopshell/api.h>
#include <coreinit/debug.h>

void MyCommandCallback(int argc, char **argv) {
    OSReport("Hello! Arguments received: %d\n", argc);
    for(int i = 0; i < argc; i++) {
        OSReport("Arg %d: %s\n", i, argv[i]);
    }
}

void InitShell() {
    IOPShellModule_Error status = IOPShellModule_InitLibrary();
    if (status != IOPSHELL_MODULE_ERROR_SUCCESS) return;

    // Usage: aroma mycmd arg1 arg2
    IOPShellModule_Error res = IOPShellModule_AddCommand("mycmd", "Prints args", MyCommandCallback);
    if (res != IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSReport("Failed to add command\n");
    }
}

void DeinitShell() {
    if (IOPShellModule_RemoveCommand("mycmd") != IOPSHELL_MODULE_ERROR_SUCCESS) {
        OSReport("Failed to remove command\n");
    }
    IOPShellModule_DeInitLibrary();
}
```

## API Reference

### C++ (`namespace IOPShellModule`)

* **`Init()`**: Initializes the library.
* **`DeInit()`**: Closes the library.
* **`IOPSHELL_REGISTER_ENUM(Type, ...)`**: Macro to register Enum-to-String mappings.

#### CommandRegistry

* **`CommandRegistry::Add<Func>(name, description, [outError])`**: Registers a C++ function. Argument parsing is
  automated.
* **`CommandRegistry::Add(name, lambda, description, [outError])`**: Registers a lambda with automatic signature
  deduction.
* **`CommandRegistry::Add<Signature>(name, lambda, description, [outError])`**: Registers a lambda with an explicit
  signature.
* **`CommandRegistry::Add(name, description, callback, [outError])`**: Registers a raw `argc`/`argv` callback.
* **`CommandRegistry::AddRaw(name, handler, description, [outError])`**: Registers a raw command handler (
  argc/argv). Supports capturing lambdas.
* **`CommandRegistry::Remove(name)`**: Unregisters a command.
* **`CommandRegistry::List()`**: Returns a `std::vector` of all registered commands.

#### CommandGroup

Class for managing sub-commands (e.g., `cmd subcmd`).

* **`CommandGroup::CommandGroup(name, description)`**: Creates a group.
* **`CommandGroup::AddCommand(name, lambda, description)`**: Registers a sub-command with automatic type deduction.
* **`CommandGroup::AddRawCommand(name, handler, description)`**: Registers a raw sub-command.
* **`CommandGroup::Register()`**: Registers the main command with the shell. Returns `std::optional<Command>`.

### C API

* **`IOPShellModule_InitLibrary()`**: Initializes the connection to the backend module.
* **`IOPShellModule_AddCommand(name, description, callback)`**: Registers a new command.
* **`IOPShellModule_RemoveCommand(name)`**: Unregisters a command.
* **`IOPShellModule_ListCommands(...)`**: Retrieves a list of commands (requires manual buffer management).

## Use this lib in Dockerfiles

A prebuilt version of this lib can be found on dockerhub. To use it for your projects, add this to your Dockerfile:

```
[...]
COPY --from=ghcr.io/wiiu-env/libiopshell:[tag] /artifacts $DEVKITPRO
[...]
```

Replace `[tag]` with the tag you want to use. A list of tags can be
found [here](https://github.com/wiiu-env/libiopshell/pkgs/container/libiopshell/versions).
It is highly recommended to pin the version to the **latest date** instead of using `latest` to ensure reproducible
builds.

## Format the code via docker

```
docker run --rm -v ${PWD}:/src ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./source ./include -i
```
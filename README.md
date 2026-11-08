# dross 🧱

> A modern C++23 general-purpose library for self-contained applications

[![Check](https://github.com/skipbit/dross/actions/workflows/check.yml/badge.svg)](https://github.com/skipbit/dross/actions/workflows/check.yml)
[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)](CMakeLists.txt)

**dross** is a general-purpose C++23 library of building blocks for applications: value types held together by a dynamic `value`, including arbitrary precision numbers, TOML reading and writing, platform utilities for the environment, paths and XDG directories, and per-thread run loops with the threads, timers and operation queue built on them.

## ✨ Key Features

- **🔢 Arbitrary Precision Arithmetic** - String-based numbers that do not overflow; a quotient that does not terminate within 4096 digits keeps 34 significant digits
- **🧵 Dynamic Type System** - Unified `value` type using `std::variant`
- **🔧 Platform Utilities** - Environment, filesystem, and XDG support on Linux and macOS
- **⚡ No Third-Party Dependencies** - Only the C++ standard library and the system's threads
- **🎯 Modern C++23** - Concepts for compile-time constraints, and failures returned through `std::expected`

## 🚀 Quick Start

### Requirements

- **A C++23 compiler and standard library.** On Linux, GCC 13–15, or Clang
  20–22 with libstdc++ or libc++; on macOS, the Apple Clang of macOS 15 or 26.
  [Getting Started](https://skipbit.github.io/dross/getting-started.html)
  lists the supported pairings and what CI builds.
- **CMake 3.20+**, or 3.25+ to build with the presets

### Installation

#### From Source

```bash
git clone https://github.com/skipbit/dross.git
cd dross
```

##### With CMake

```bash
# Configure, build and test into build/release
cmake --workflow --preset release

# Install
sudo cmake --install build/release --prefix /usr/local
```

The presets are `debug`, `release`, `static`, `sanitize-address` and
`sanitize-thread`, each building into `build/<preset>`. The library is shared
by default; the `static` preset, or `-DBUILD_SHARED_LIBS=OFF`, builds a static
one. CMake 3.20 to 3.24 has no presets: configure with
`cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release`, then run
`cmake --build build/release`.

### Using in Your Project

#### CMake Integration

```cmake
# Option 1: find_package (after installation)
find_package(dross REQUIRED)
target_link_libraries(your_target PRIVATE dross::dross)

# Option 2: FetchContent (no installation needed)
include(FetchContent)
FetchContent_Declare(dross
    GIT_REPOSITORY https://github.com/skipbit/dross.git
    GIT_TAG <version>
)
FetchContent_MakeAvailable(dross)
target_link_libraries(your_target PRIVATE dross::dross)

# Option 3: CPM (no installation needed)
CPMAddPackage("gh:skipbit/dross#<version>")
target_link_libraries(your_target PRIVATE dross::dross)
```

Replace `<version>` with a tag from
[Releases](https://github.com/skipbit/dross/releases). `main` is the
development tip, and the ABI can change between 0.x releases.

#### pkg-config Integration

```bash
# Compile flags
pkg-config --cflags dross

# Link flags
pkg-config --libs dross

# Check version
pkg-config --modversion dross
```

`dross.pc` names the prefix given at configure time. For an install written
elsewhere with `cmake --install --prefix`, `--define-prefix` has pkg-config
take the prefix from where it finds `dross.pc` in `<prefix>/lib/pkgconfig`:

```bash
PKG_CONFIG_PATH=<prefix>/lib/pkgconfig pkg-config --define-prefix --cflags --libs dross
```

#### Manual Compilation

```bash
# Using pkg-config
g++ -std=c++23 $(pkg-config --cflags dross) main.cpp $(pkg-config --libs dross)

# Or manually
g++ -std=c++23 -I/usr/local/include main.cpp -L/usr/local/lib -ldross
```

#### Basic Usage

```cpp
#include <dross/dross.h>

int main()
{
    // Arbitrary precision arithmetic
    dross::number big_num{"99999999999999999999999999999999999999"};
    dross::number result = big_num * big_num;  // No overflow!

    // Dynamic typing. dictionary has no initializer-list constructor, so
    // entries are assigned after construction.
    dross::dictionary config;
    config["name"] = dross::string("dross");
    config["version"] = dross::string("0.1.0");
    config["features"] = dross::array{dross::string{"fast"}, dross::string{"safe"}};
    config["release_date"] = dross::timestamp{2024, 1, 21, 15, 30, 0, dross::timezone::utc()};

    dross::value root = config;

    // Platform utilities. The XDG accessors are instance methods, and the
    // application name is already part of what they return.
    dross::xdg app{"myapp"};
    if (auto config_dir = app.config_home()) {
        dross::path app_config = dross::path{*config_dir}.append("config.toml");
    }
}
```

## 📚 Core Modules

### Type System
- **`boolean`** - Type-safe boolean operations with logical operators
- **`number`** - Arbitrary precision arithmetic with string-based storage
- **`string`** - UTF-8 text held as bytes, with byte-oriented operations
- **`timestamp`** - Date and time handling with timezone support
- **`timezone`** - Type-safe timezone representation with ISO 8601 support
- **`array`** - Dynamic arrays with value semantics
- **`dictionary`** - Key-value containers
- **`data`** - Byte buffer, converted to and from strings byte for byte
- **`value`** - Polymorphic type holding any supported type
- **`error`** - A `std::error_code` with its category, returned through `std::expected`

### Format Layer
- **`toml`** - TOML parsing into a `dictionary` and serialization back to TOML

### Platform Layer
- **`environment`** - Environment variable access
- **`path`** - Filesystem operations with error handling
- **`xdg`** - XDG Base Directory specification support

### Thread Layer
- **`runloop`** - Per-thread task queue, so work can be handed to a specific thread
- **`thread`** - A handle to an OS thread, running a loop the library drives, a one-shot body, or one its owner drives
- **`timer`** - A callback installed on a run loop, firing once or on an interval
- **`operation_queue`** - A fixed set of worker threads, each running its own loop, taking submitted tasks from one shared queue by priority, and in the order they were submitted within one priority, each only once the tasks it runs after have returned
- **`operation_result`** - What a task given to `operation_queue::enqueue()` returned, to wait on and read as its own type

## 📖 Documentation

**[📘 API Reference & User Guide](https://skipbit.github.io/dross/)**

The documentation includes:
- 🔍 **API Reference** - The type system and the platform layer
- 📚 **User Guide** - An overview of the library's conventions
- 💡 **Examples** - Code examples across the modules

## 🏗️ Architecture

dross follows modern C++ best practices:

- **Pimpl Idiom** - Types keep their representation behind an opaque pointer, apart from `error`, `path`, `xdg`, `timestamp`, `timezone` and the `data` iterators, which hold theirs directly; the ABI can change in any 0.x release
- **Value Semantics** - The value types are copyable and assignable; `environment` exposes only static members
- **Error Handling** - Failures are returned through `std::expected` and `std::optional`. Where a call throws, lets an exception through, or reports its outcome another way, its comment in the public headers says so
- **Type Safety** - Concepts for compile-time constraints

## 🧪 Testing

The tests use GoogleTest:

```bash
# Run all tests, in the tree the release preset built
ctest --preset release

# Run specific test patterns
ctest --preset release -R number
./build/release/test/dross_test --gtest_filter="number_test.*"

# Verbose output
ctest --preset release -V
```

## 🗺️ Roadmap

### Planned Features
- **Format** - JSON, XML, YAML
- **Concurrency** - Coroutines
- **Multimedia** - Image processing, color management, transformations
- **Application Support** - CLI parsing, logging, preferences

## 🤝 Contributing

See the [Contributing guide](https://skipbit.github.io/dross/contributing.html) for the workflow, and [CODINGSTYLE.md](CODINGSTYLE.md) for coding guidelines.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Made with ❤️ by [Yuma Endo](https://github.com/skipbit)**

# Dross 🧱

> A modern C++23 general-purpose library designed for robust, self-contained applications

[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)](CMakeLists.txt)

**Dross** is a comprehensive C++ library that provides fundamental building blocks for modern applications. Similar in spirit to Boost, but designed with C++23 features and minimal dependencies in mind.

## ✨ Key Features

- **🔢 Arbitrary Precision Arithmetic** - String-based number system supporting unlimited precision
- **🧵 Dynamic Type System** - Unified `value` type using modern `std::variant`
- **🔧 Platform Utilities** - Cross-platform environment, filesystem, and XDG support
- **⚡ Zero Dependencies** - Self-contained with only standard library requirements
- **🛡️ Memory Safe** - RAII principles with smart pointers throughout
- **🎯 Modern C++23** - Leverages concepts, ranges, and latest language features

## 🚀 Quick Start

### Requirements

- **C++23** compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- **CMake 3.20+**

### Installation

#### Package Managers

```bash
# vcpkg (coming soon)
vcpkg install dross

# Conan (coming soon)  
conan install dross/0.0.1@

# CPM (CMake Package Manager)
CPMAddPackage("gh:skipbit/dross@0.0.1")
```

#### From Source

```bash
# Clone and build
git clone https://github.com/skipbit/dross.git
cd dross

# Configure with desired options
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# Build and install
cmake --build build
sudo cmake --install build

# Run tests (optional)
cd build && ctest -V
```

#### Library Types

```bash
# Shared library (default)
cmake -DBUILD_SHARED_LIBS=ON ...

# Static library  
cmake -DBUILD_SHARED_LIBS=OFF ...
```

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
    GIT_TAG v0.0.1
)
FetchContent_MakeAvailable(dross)
target_link_libraries(your_target PRIVATE dross::dross)
```

#### Basic Usage

```cpp
#include <dross/dross.h>
using namespace dross;

// Arbitrary precision arithmetic
number big_num{"99999999999999999999999999999999999999"};
number result = big_num * big_num;  // No overflow!

// Dynamic typing
value data = dictionary{
    {"name", string{"Dross"}},
    {"version", number{"0.0.1"}},
    {"features", array{string{"fast"}, string{"safe"}}}
};

// Platform utilities
auto config_dir = xdg::config_home();
auto app_config = config_dir / "myapp" / "config.toml";
```

## 📚 Core Modules

### Type System
- **`number`** - Arbitrary precision arithmetic with string-based storage
- **`string`** - Unicode-aware string handling
- **`array`** - Dynamic arrays with value semantics
- **`dictionary`** - Key-value containers
- **`value`** - Polymorphic type holding any supported type

### Platform Layer
- **`environment`** - Environment variable access
- **`path`** - Filesystem operations with error handling
- **`xdg`** - XDG Base Directory specification support

### Configuration
- **`toml`** - TOML configuration file parser

## 🏗️ Architecture

Dross follows modern C++ best practices:

- **Pimpl Idiom** - ABI stability through opaque pointers
- **Value Semantics** - All types are copyable and assignable
- **Error Handling** - `std::expected` and `std::optional` instead of exceptions
- **Type Safety** - Concepts for compile-time constraints
- **Zero-Cost Abstractions** - Performance without compromise

## 🧪 Testing

Comprehensive test suite using GoogleTest:

```bash
# Run all tests
ctest

# Run specific test patterns
ctest -R number
./build/debug/test/dross_test --gtest_filter="number_test.*"

# Verbose output
ctest -V
```

## 🗺️ Roadmap

### Current Modules
- ✅ Type System (number, string, array, dictionary, value)
- ✅ Platform utilities (environment, path, xdg)
- ✅ Configuration (TOML parser)

### Planned Features
- **Concurrency** - Thread management, async operations, coroutines
- **Multimedia** - Image processing, color management, transformations
- **Application Support** - CLI parsing, logging, preferences
- **Additional Formats** - JSON, XML, YAML parsers

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

See [CODINGSTYLE.md](CODINGSTYLE.md) for coding guidelines.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Inspired by the Boost libraries
- Built with modern C++23 features
- Designed for real-world applications

---

**Made with ❤️ by [Yuma Endo](https://github.com/skipbit)**
Getting Started
===============

This guide will help you get started with the dross library.

System Requirements
-------------------

To build and use dross, you need:

- **C++ Standard**: C++23 or later in your own project

  The public headers use C++23, so C++17 and C++20 are outside the supported
  range. C++26 consumers are best effort: no required job builds one, so
  neither compiling these headers as C++26 nor the ABI and ODR compatibility
  of linking such a consumer against a C++23 build of the library is verified.

- **C++ Compiler and Standard Library**: on Linux, one of these pairings

  - GCC 13 through 15 with the libstdc++ it is paired with (13, 14 or 15)
  - Clang 20 through 22 with libstdc++ 13, 14 or 15
  - Clang 20 through 22 with libc++ 20 or 22

  The version in each pairing is the version of the standard library headers
  the compiler builds against. The shared runtime a resulting binary loads
  comes from the system's own runtime package, which is versioned and updated
  separately.

  GCC with libc++ is not one of them, because upstream does not support that
  pairing: GCC has no ``-stdlib`` option to select libc++ with in the first
  place. It would be worth revisiting if GCC gained an equivalent option, or
  if libc++ started supporting GCC officially. On macOS the compiler is the
  Apple Clang shipped with macOS 15 or 26, and the standard library is not a
  separate axis there, because libc++ comes with the OS toolchain.

  The required Linux build matrix builds GCC 13 and 15, each against the
  libstdc++ paired with it, and Clang 20 and 22 (not the intermediate 21)
  against libstdc++ 13, 14 and 15 — 15 being the release Ubuntu 26.04
  provides — as well as against libc++ 20 and 22. Every libstdc++ release in
  the supported range is therefore covered in the Clang pairings; among the
  GCC ones only 13 and 15 are, since the libstdc++ version follows the
  compiler version there. GCC 14 and Clang 21 sit inside the declared range
  the same way, without a required job of their own.

  Newer versions are best effort: GCC is exercised by the nightly toolchain
  watch tracking the newest versioned GCC available once the toolchain PPA
  is in place, and Clang by the same watch tracking the specific release
  next in line to enter this range, rather than by the required build
  matrix.

  The Clang lower bound is higher than the GCC one because Clang releases
  older than 20 cannot compile this library's C++23 ``std::expected`` usage
  against the libstdc++ they are paired with on Ubuntu 24.04 — and installing
  the libstdc++ 14 headers alongside those older releases does not change that
  either. From Clang 20 onwards both standard libraries work, which is why
  every pairing listed above starts there.

- **Build System**: CMake 3.20 or later
- **Operating System**: Linux or macOS

Installation
------------

From Source
~~~~~~~~~~~

Clone the repository and build with CMake:

.. code-block:: bash

    # Clone the repository
    git clone https://github.com/skipbit/dross.git
    cd dross

    # Create build directory
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    
    # Build the library
    cmake --build build
    
    # Run tests (optional)
    cd build && ctest
    
    # Install (optional)
    sudo cmake --install build

Build Options
~~~~~~~~~~~~~

You can customize the build with these CMake options:

.. code-block:: bash

    # Build as static library (default is shared)
    cmake -S . -B build -DBUILD_SHARED_LIBS=OFF
    
    # Enable debug build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    
    # Specify installation prefix
    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local

Using dross in Your Project
---------------------------

CMake Integration
~~~~~~~~~~~~~~~~~

Add dross to your CMake project:

.. code-block:: cmake

    find_package(dross REQUIRED)
    
    add_executable(myapp main.cpp)
    target_link_libraries(myapp PRIVATE dross::dross)

Or using FetchContent:

.. code-block:: cmake

    include(FetchContent)
    FetchContent_Declare(
        dross
        GIT_REPOSITORY https://github.com/skipbit/dross.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(dross)
    
    add_executable(myapp main.cpp)
    target_link_libraries(myapp PRIVATE dross::dross)

Manual Integration
~~~~~~~~~~~~~~~~~~

If not using CMake, compile with:

.. code-block:: bash

    # Compile with shared library
    g++ -std=c++23 -I/path/to/dross/include main.cpp -L/path/to/dross/lib -ldross
    
    # Compile with static library
    g++ -std=c++23 -I/path/to/dross/include main.cpp /path/to/dross/lib/libdross.a

Your First Program
------------------

Here's a simple example using the dross type system:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/type/array.h>
    #include <dross/type/dictionary.h>
    #include <dross/type/value.h>

    int main()
    {
        using namespace dross;

        // Create a dictionary with mixed types. Name the dross type on the
        // right-hand side: a bare literal is ambiguous between value's
        // boolean, number and value assignment operators, and value{x} with
        // braces builds a one-element array instead of holding x.
        dictionary config;
        config["name"] = string("My Application");
        config["version"] = number("1.0");
        config["debug"] = boolean(true);

        // Create an array of features
        array features;
        features.append("logging");
        features.append("caching");
        features.append("monitoring");

        config["features"] = features;

        // Access values. operator[] would insert a default value for a key
        // that is absent, so ask contains() before reading.
        if (config.contains("name") && config["name"].is<string>()) {
            std::string name = config["name"].as<string>();
            std::cout << "Application: " << name << std::endl;
        }

        if (config.contains("features") && config["features"].is<array>()) {
            array feature_array = config["features"].as<array>();
            std::cout << "Features:" << std::endl;
            for (const auto& feature : feature_array) {
                // value has no operator<<; unwrap it first
                if (feature.is<string>()) {
                    std::string text = feature.as<string>();
                    std::cout << "  - " << text << std::endl;
                }
            }
        }

        return 0;
    }

Compile and run:

.. code-block:: bash

    g++ -std=c++23 example.cpp -ldross -o example
    ./example

Expected output:

.. code-block:: text

    Application: My Application
    Features:
      - logging
      - caching
      - monitoring

Next Steps
----------

- Explore the :doc:`user-guide/index` for in-depth tutorials
- Browse the :doc:`api/index` for detailed API documentation
- Check out :doc:`examples/index` for more complex use cases
- Learn about :doc:`contributing` if you want to help improve dross
Getting Started
===============

This guide will help you get started with the dross library.

System Requirements
-------------------

To build and use dross, you need:

- **C++ Compiler**: Supporting C++23 standard

  - GCC 13 through 15
  - Clang 17 through 22
  - Apple Clang shipped with macOS 15 or 26

  Newer versions are best effort: they are exercised by the nightly toolchain
  watch rather than by the required build matrix.

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
    #include <dross/value.h>
    #include <dross/array.h>
    #include <dross/dictionary.h>
    
    int main()
    {
        using namespace dross;
        
        // Create a dictionary with mixed types
        dictionary config;
        config.set("name", "My Application");
        config.set("version", 1.0);
        config.set("debug", true);
        
        // Create an array of features
        array features;
        features.append("logging");
        features.append("caching");
        features.append("monitoring");
        
        config.set("features", features);
        
        // Access values
        if (auto name = config.get("name")) {
            std::cout << "Application: " << name->to_string() << std::endl;
        }
        
        if (auto feat_val = config.get("features")) {
            if (feat_val->is_array()) {
                auto feat_array = feat_val->as_array();
                std::cout << "Features:" << std::endl;
                for (const auto& feature : feat_array) {
                    std::cout << "  - " << feature << std::endl;  // Direct stream output
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
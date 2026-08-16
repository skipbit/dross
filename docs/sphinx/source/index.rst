.. dross documentation master file

Welcome to dross
================

.. raw:: html

    <div class="github-badges">
        <a href="https://github.com/skipbit/dross">
            <img src="https://img.shields.io/github/stars/skipbit/dross?style=social" alt="GitHub stars">
        </a>
        <a href="https://github.com/skipbit/dross/network/members">
            <img src="https://img.shields.io/github/forks/skipbit/dross?style=social" alt="GitHub forks">
        </a>
        <a href="https://github.com/skipbit/dross/releases">
            <img src="https://img.shields.io/github/v/release/skipbit/dross?include_prereleases" alt="GitHub release">
        </a>
        <a href="https://github.com/skipbit/dross/blob/main/LICENSE">
            <img src="https://img.shields.io/github/license/skipbit/dross" alt="License">
        </a>
        <a href="https://github.com/skipbit/dross/actions">
            <img src="https://img.shields.io/github/actions/workflow/status/skipbit/dross/ci.yml?branch=main" alt="Build Status">
        </a>
    </div>

.. toctree::
   :hidden:
   :maxdepth: 2

   getting-started
   user-guide/index
   api/index
   examples/index
   contributing
   changelog

**dross** is a modern C++23 library providing fundamental building blocks for applications,
designed to be a general-purpose library similar to Boost with a focus on:

- **Zero external dependencies** - Only requires the standard library
- **Modern C++ design** - Leveraging C++23 features throughout
- **Errors in the return type** - ``std::optional`` and ``std::expected``
  rather than exceptions, apart from the bounds-checked accessors
- **ABI stability** - Through careful use of the Pimpl idiom
- **Comprehensive type system** - Dynamic types with value semantics

.. grid:: 2
    :gutter: 4
    :padding: 2 2 0 0
    :class-container: sd-text-center

    .. grid-item-card:: Getting Started
        :img-top: _static/icons/rocket.svg
        :class-card: intro-card
        :shadow: md

        New to dross? Start here with installation instructions and a quick tutorial.

        +++

        .. button-ref:: getting-started
            :expand:
            :color: secondary
            :click-parent:

            Get Started

    .. grid-item-card:: User Guide
        :img-top: _static/icons/book.svg
        :class-card: intro-card
        :shadow: md

        Learn how to use dross effectively with in-depth guides and best practices.

        +++

        .. button-ref:: user-guide/index
            :expand:
            :color: secondary
            :click-parent:

            Read the Guide

    .. grid-item-card:: API Reference
        :img-top: _static/icons/code.svg
        :class-card: intro-card
        :shadow: md

        Complete API documentation for all dross modules and classes.

        +++

        .. button-ref:: api/index
            :expand:
            :color: secondary
            :click-parent:

            Browse API

    .. grid-item-card:: Examples
        :img-top: _static/icons/clipboard.svg
        :class-card: intro-card
        :shadow: md

        See dross in action with practical examples and use cases.

        +++

        .. button-ref:: examples/index
            :expand:
            :color: secondary
            :click-parent:

            View Examples

Core Modules
------------

Type System
~~~~~~~~~~~

The dross type system provides dynamic typing with strong value semantics:

.. code-block:: cpp

    #include <iostream>

    #include <dross/type/array.h>
    #include <dross/type/dictionary.h>
    #include <dross/type/value.h>

    using namespace dross;

    // dictionary has no initializer-list constructor
    dictionary dict;
    dict["key"] = string("value");

    // Create various types
    value v1 = 42;                 // number
    value v2 = "hello world";      // string
    value v3 = array{1, 2, 3};     // array
    value v4 = dict;               // dictionary
    value v5 = boolean{true};      // boolean

    // Type checking, then casting
    if (v1.is<number>()) {
        auto n = v1.as<number>();
        std::cout << "Number: " << n << std::endl;  // number has operator<<
    }

Platform Utilities
~~~~~~~~~~~~~~~~~~

Cross-platform utilities for common operations:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/platform/environment.h>
    #include <dross/platform/path.h>
    #include <dross/platform/xdg.h>

    using namespace dross;

    // Environment variables
    const std::string home = environment::value("HOME").value_or("/tmp");

    // Path operations
    const path config_dir = path{home}.append(".config");
    std::cout << "Config: " << config_dir.string() << std::endl;

    // XDG Base Directory support
    xdg app{"myapp"};
    if (auto data_home = app.data_home()) {
        std::cout << "Data: " << *data_home << std::endl;
    }

Features
--------

- **Dynamic Type System**: Polymorphic value type using ``std::variant``
- **UTF-8 Strings**: Text held as UTF-8 bytes, with byte-oriented operations
- **Arbitrary Precision**: Number type with string-based storage
- **Error Handling**: Consistent use of ``std::optional`` and ``std::expected``
- **Modern C++**: Concepts, ranges, three-way comparison, and more
- **Platform Utilities**: Cross-platform environment and filesystem operations

Installation
------------

dross can be built using CMake:

.. code-block:: bash

    # Clone the repository
    git clone https://github.com/skipbit/dross.git
    cd dross
    
    # Configure and build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    
    # Run tests
    cd build && ctest

See :doc:`getting-started` for detailed installation instructions.

Contributing
------------

We welcome contributions! Please see our :doc:`contributing` guide for details on:

- Code style and conventions
- Testing requirements
- Submitting pull requests
- Reporting issues

License
-------

dross is released under the MIT License. See the LICENSE file for details.

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

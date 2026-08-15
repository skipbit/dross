Contributing to dross
=====================

Thank you for your interest in contributing to dross! This guide will help you get started.

Getting Started
---------------

1. Fork the repository on GitHub
2. Clone your fork locally:

   .. code-block:: bash

      git clone https://github.com/YOUR_USERNAME/dross.git
      cd dross

3. Add the upstream repository:

   .. code-block:: bash

      git remote add upstream https://github.com/skipbit/dross.git
      git fetch upstream

4. Create a new branch for your work:

   .. code-block:: bash

      git checkout -b feature/your-feature-name

Development Setup
-----------------

Build Requirements
~~~~~~~~~~~~~~~~~~

- A compiler configured for C++23 or later. The public headers use C++23, so
  C++17 and C++20 are outside the supported range. C++26 consumers are best
  effort: no required job builds one, so neither compiling these headers as
  C++26 nor the ABI and ODR compatibility of linking such a consumer against a
  C++23 build of the library is verified.
- A supported compiler and standard library pairing. On Linux those are:

  - GCC 13-15 with the libstdc++ it is paired with (13, 14 or 15)
  - Clang 20-22 with libstdc++ 13, 14 or 15
  - Clang 20-22 with libc++ 20 or 22

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
- The required Linux jobs build GCC 13/15, each against the libstdc++ paired
  with it, and Clang 20/22 against libstdc++ 13, 14 and 15 (15 being the
  release Ubuntu 26.04 provides) as well as against libc++ 20 and 22. Every
  libstdc++ release in the supported range is therefore covered in the Clang
  pairings; among the GCC ones only 13 and 15 are, since the libstdc++ version
  follows the compiler version there. GCC 14 and Clang 21 are inside the
  declared range but are not built by a required job either. Newer versions
  are best effort.
- CMake 3.20 or later
- Git

Building the Project
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Debug build (recommended for development)
    cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
    cmake --build build/debug

    # Run tests
    cd build/debug && ctest -V

Development Tools
~~~~~~~~~~~~~~~~~

The project generates ``compile_commands.json`` for IDE support:

.. code-block:: bash

    # The file is generated automatically in the build directory
    ls build/debug/compile_commands.json

Code Style
----------

Please follow the coding style guidelines in ``CODINGSTYLE.md``:

General Rules
~~~~~~~~~~~~~

- Use C++23 features where appropriate
- All code must be in the ``dross`` namespace
- Use 4 spaces for indentation (no tabs)
- Target 80 character line length (flexible for readability)

Naming Conventions
~~~~~~~~~~~~~~~~~~

- **Classes/Structs**: ``lowercase`` (e.g., ``value``, ``string``)
- **Functions**: ``lower_snake_case`` (e.g., ``to_string()``)
- **Variables**: ``lower_snake_case`` (e.g., ``my_variable``)
- **Private members**: ``_snake_case`` (e.g., ``_impl``)
- **Constants**: ``kCamelCase`` (e.g., ``kDefaultSize``)
- **Concepts**: ``snake_case_type`` (e.g., ``string_type``)

Code Organization
~~~~~~~~~~~~~~~~~

- Separate headers (``.h``) and implementation (``.cpp``)
- Use Pimpl idiom for ABI stability in public classes
- No exceptions - use ``std::optional`` and ``std::expected``
- Apply ``const``, ``constexpr``, and ``noexcept`` appropriately

Testing
-------

All contributions must include appropriate tests:

Writing Tests
~~~~~~~~~~~~~

Tests use GoogleTest and are located in the ``test/`` directory:

.. code-block:: cpp

    #include <string>

    #include <gtest/gtest.h>

    #include <dross/type/value.h>

    TEST(ValueTest, NumberConstruction) {
        dross::value v(42);
        EXPECT_TRUE(v.is<dross::number>());
        EXPECT_EQ(v.as<dross::number>(), dross::number(42));
    }

    TEST(ValueTest, StringConstruction) {
        dross::value v("hello");
        ASSERT_TRUE(v.is<dross::string>());
        const std::string text = v.as<dross::string>();
        EXPECT_EQ(text, "hello");
    }

Running Tests
~~~~~~~~~~~~~

.. code-block:: bash

    # Run all tests
    cd build/debug && ctest

    # Run specific test
    ./build/debug/dross_test --gtest_filter="ValueTest.*"

    # Run with detailed output
    ./build/debug/dross_test --gtest_list_tests

Test Coverage
~~~~~~~~~~~~~

Aim for:

- 100% coverage of public APIs
- >90% overall code coverage
- Test both success and error paths
- Include edge cases and boundary conditions

Documentation
-------------

API Documentation
~~~~~~~~~~~~~~~~~

Use Doxygen-style comments for all public APIs:

.. code-block:: cpp

    /**
     * @brief Converts the value to a string representation.
     * 
     * @return String representation of the value
     * @note Never throws
     */
    string to_string() const noexcept;

Documentation Building
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Build documentation locally
    cd docs
    make clean
    make html-with-doxygen

    # View documentation
    open _build/html/index.html

Submitting Changes
------------------

Pull Request Process
~~~~~~~~~~~~~~~~~~~~

1. Ensure your code follows the style guide
2. Add tests for new functionality
3. Update documentation as needed
4. Ensure all tests pass
5. Commit your changes:

   .. code-block:: bash

      git add .
      git commit -m "feat: add new feature

      Detailed description of what changed and why"

6. Push to your fork:

   .. code-block:: bash

      git push origin feature/your-feature-name

7. Create a pull request on GitHub

Commit Message Format
~~~~~~~~~~~~~~~~~~~~~

Use conventional commits format:

- ``feat:`` New feature
- ``fix:`` Bug fix
- ``docs:`` Documentation changes
- ``test:`` Test additions or modifications
- ``refactor:`` Code refactoring
- ``style:`` Code style changes
- ``perf:`` Performance improvements
- ``chore:`` Build system or auxiliary tool changes

Pull Request Guidelines
~~~~~~~~~~~~~~~~~~~~~~~

- Keep PRs focused on a single feature or fix
- Provide a clear description of the changes
- Reference any related issues
- Respond to review feedback promptly
- Ensure CI passes before requesting review

Reporting Issues
----------------

Bug Reports
~~~~~~~~~~~

When reporting bugs, please include:

1. dross version or commit hash
2. Compiler and version
3. Operating system
4. Minimal reproducible example
5. Expected vs actual behavior
6. Any error messages or logs

Feature Requests
~~~~~~~~~~~~~~~~

For feature requests, please describe:

1. The problem you're trying to solve
2. Your proposed solution
3. Alternative solutions considered
4. Any API design considerations

Community
---------

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and discussions
- **Pull Requests**: Code contributions

License
-------

By contributing to dross, you agree that your contributions will be licensed
under the same MIT License that covers the project.

Thank You!
----------

Your contributions help make dross better for everyone. We appreciate your time
and effort in improving the library!

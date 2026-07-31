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

- C++23 compatible compiler (verified in CI: GCC 13-15, Clang 17-22,
  Apple Clang on macOS 15 and 26; newer versions are best effort)
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

    #include <gtest/gtest.h>
    #include <dross/value.h>

    TEST(ValueTest, DefaultConstruction) {
        dross::value v;
        EXPECT_TRUE(v.is_null());
    }

    TEST(ValueTest, NumberConstruction) {
        dross::value v(42);
        EXPECT_TRUE(v.is_number());
        EXPECT_EQ(v.as_number(), dross::number(42));
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

Code of Conduct
---------------

Please note that this project is released with a Contributor Code of Conduct. 
By participating in this project you agree to abide by its terms.

License
-------

By contributing to dross, you agree that your contributions will be licensed
under the same MIT License that covers the project.

Thank You!
----------

Your contributions help make dross better for everyone. We appreciate your time
and effort in improving the library!
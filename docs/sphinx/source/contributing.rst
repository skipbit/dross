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

      git checkout -b fix/short-description

   Start the name with the kind of change, as in the commit message:
   ``fix/``, ``docs/``, ``ci/``, ``refactor/`` and so on.

Development Setup
-----------------

Build Requirements
~~~~~~~~~~~~~~~~~~

- The compiler, standard library and CMake listed under System Requirements
  in :doc:`getting-started`
- Git

Building the Project
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Debug build (recommended for development): configure, build and test
    cmake --workflow --preset debug

    # Run the tests again
    ctest --preset debug

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
- Use Pimpl idiom for ABI stability in public classes, apart from those that
  hold a few small values directly so that copying them does not allocate
- No exceptions - use ``std::optional`` and ``std::expected``
- Apply ``const``, ``constexpr``, and ``noexcept`` appropriately

Testing
-------

All contributions must include appropriate tests:

Writing Tests
~~~~~~~~~~~~~

Tests use GoogleTest and are located in the ``test/`` directory:

.. code-block:: cpp
    :class: example-fragment

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
    ctest --test-dir build/debug

    # Run specific test
    ./build/debug/test/dross_test --gtest_filter="value_test.*"

    # List the tests
    ./build/debug/test/dross_test --gtest_list_tests

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
    :class: example-fragment

    /**
     * @brief Converts the value to a string representation.
     *
     * @return String representation of the value
     * @note Never throws
     */
    string to_string() const noexcept;

Code Examples
~~~~~~~~~~~~~

``scripts/check-examples.py`` compiles every C++ example in the README, these
pages and the header comments, and the pull request checks run it with GCC and
with Clang and libc++. An example compiles after ``#include <dross/dross.h>``,
taken as the body of a function unless it defines ``main``. It names what it
uses with ``dross::``, includes the other headers it needs and declares what
else it relies on; ``#include`` lines and the function declarations it opens
with stay outside the function.

On these pages, a ``code-block`` with ``:class: example-namespace-scope`` is
compiled without being put in a function, and one with
``:class: example-fragment``, such as the Doxygen comment above, is not
compiled. A block names its language, such as ``.. code-block:: console``
for a shell session: Sphinx shows a literal block (``::``) or a
``code-block`` without one as C++, so the check reports it.

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

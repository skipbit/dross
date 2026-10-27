User Guide
==========

.. toctree::
   :maxdepth: 2

   type-system
   error-handling
   platform-utilities
   best-practices

Welcome to the dross User Guide. This guide provides in-depth information about
using the dross library effectively in your C++ projects.

Overview
--------

The dross library is designed around several core principles:

1. **Zero Dependencies**: Only requires the standard C++ library
2. **Value Semantics**: The value types are copyable and assignable;
   ``environment`` is the exception, exposing only static members
3. **Error Handling**: Errors are returned, not thrown - ``std::optional``
   and ``std::expected`` - apart from the bounds-checked accessors, the
   ``path`` calls that let ``std::filesystem`` exceptions through and the
   ``timestamp`` arithmetic operators, which throw outside the years 0 to 9999
4. **Modern C++**: Leverages C++23 features throughout
5. **Pimpl Idiom**: Types keep their representation behind an opaque pointer,
   apart from ``error``, ``path``, ``xdg`` and the ``data`` iterators, which
   hold theirs directly; the ABI can change in any 0.x release

Topics Covered
--------------

Type System Guide
~~~~~~~~~~~~~~~~~

Learn how to effectively use the dross type system:

- Working with dynamic values
- Type conversions and checking
- Building complex data structures
- Performance considerations

:doc:`Read more → <type-system>`

Error Handling
~~~~~~~~~~~~~~

Understanding dross's approach to error handling:

- Using ``std::optional`` for nullable values
- Working with ``std::expected`` for error propagation
- Creating and handling error types
- Best practices for robust code

:doc:`Read more → <error-handling>`

Platform Utilities
~~~~~~~~~~~~~~~~~~

Cross-platform utilities for system interaction:

- Environment variable management
- Filesystem path operations
- XDG Base Directory support
- Platform-specific considerations

:doc:`Read more → <platform-utilities>`

Best Practices
~~~~~~~~~~~~~~

Guidelines for writing efficient and maintainable code:

- Design patterns with dross
- Performance optimization tips
- Memory management strategies
- Common pitfalls to avoid

:doc:`Read more → <best-practices>`

Quick Examples
--------------

Type System
~~~~~~~~~~~

.. code-block:: cpp

    // dictionary has no initializer-list constructor, so it is built up
    dross::dictionary alice;
    alice["name"] = dross::string("Alice");
    alice["age"] = dross::number(30);

    dross::dictionary bob;
    bob["name"] = dross::string("Bob");
    bob["age"] = dross::number(25);

    dross::dictionary root;
    root["users"] = dross::array{alice, bob};
    root["count"] = dross::number(2);

    dross::value doc = root;

    // Safe access: ask contains() before reading, is<T>() before casting
    if (doc.is<dross::dictionary>()) {
        dross::dictionary top = doc.as<dross::dictionary>();
        if (top.contains("users") && top["users"].is<dross::array>()) {
            for (const auto& user : top["users"].as<dross::array>()) {
                // Process each user
            }
        }
    }

Error Handling
~~~~~~~~~~~~~~

.. code-block:: cpp
    :class: example-namespace-scope

    #include <fstream>
    #include <iterator>

    // Function returning optional
    std::optional<dross::string> get_env_config(const dross::string& key)
    {
        if (auto found = dross::environment::value(key)) {  // Implicit string conversion
            return dross::string(*found);
        }
        return std::nullopt;
    }

    // Function returning expected. dross parses TOML; reading the bytes is
    // left to the standard library.
    std::expected<dross::dictionary, dross::error> load_config(const dross::path& file)
    {
        std::ifstream input{file.string(), std::ios::binary};
        if (!input) {
            return std::unexpected(dross::error{std::errc::no_such_file_or_directory});
        }

        const std::string text{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
        return dross::toml::deserialize(dross::data{text});
    }

Platform Utilities
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Working with paths
    const std::string home = dross::environment::value("HOME").value_or("/tmp");
    const dross::path config_path = dross::path{home}.append(".config").append("myapp");

    // XDG directories: the application name is already part of the result
    dross::xdg app{"myapp"};
    const std::string app_data = app.data_home().value_or(config_path.string());

    // Create the directory. mkdir() is idempotent: it succeeds whether it
    // creates the directory or finds it already there.
    if (auto result = dross::path::mkdir(app_data); !result) {
        std::cerr << "mkdir: " << result.error().what() << std::endl;
    }

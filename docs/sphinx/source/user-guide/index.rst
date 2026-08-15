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
2. **Value Semantics**: All types are copyable and follow value semantics
3. **Error Handling**: No exceptions - uses ``std::optional`` and ``std::expected``
4. **Modern C++**: Leverages C++23 features throughout
5. **ABI Stability**: Uses Pimpl idiom to maintain stable ABI

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

    using namespace dross;

    // dictionary has no initializer-list constructor, so it is built up
    dictionary alice;
    alice["name"] = string("Alice");
    alice["age"] = number(30);

    dictionary bob;
    bob["name"] = string("Bob");
    bob["age"] = number(25);

    dictionary root;
    root["users"] = array{alice, bob};
    root["count"] = number(2);

    value data = root;

    // Safe access: ask contains() before reading, is<T>() before casting
    if (data.is<dictionary>()) {
        dictionary top = data.as<dictionary>();
        if (top.contains("users") && top["users"].is<array>()) {
            for (const auto& user : top["users"].as<array>()) {
                // Process each user
            }
        }
    }

Error Handling
~~~~~~~~~~~~~~

.. code-block:: cpp

    // Function returning optional
    std::optional<string> get_env_config(const string& key)
    {
        if (auto found = environment::value(key)) {  // Implicit string conversion
            return string(*found);
        }
        return std::nullopt;
    }

    // Function returning expected. dross parses TOML; reading the bytes is
    // left to the standard library.
    std::expected<dictionary, error> load_config(const path& file)
    {
        std::ifstream input{file.string(), std::ios::binary};
        if (!input) {
            return std::unexpected(
                error{static_cast<int>(std::errc::no_such_file_or_directory),
                      std::generic_category()});
        }

        const std::string text{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
        return toml::deserialize(data{text});
    }

Platform Utilities
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Working with paths
    const std::string home = environment::value("HOME").value_or("/tmp");
    const path config_path = path{home}.append(".config").append("myapp");

    // XDG directories: the application name is already part of the result
    xdg app{"myapp"};
    const std::string app_data = app.data_home().value_or(config_path.string());

    // Create directory if needed
    if (auto result = path::mkdir(app_data); !result) {
        std::cerr << "Failed to create directory: "
                  << result.error().what() << std::endl;
    }
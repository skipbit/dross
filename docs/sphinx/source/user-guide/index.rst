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
    
    // Dynamic typing with type safety
    value data = dictionary{
        {"users", array{
            dictionary{{"name", "Alice"}, {"age", 30}},
            dictionary{{"name", "Bob"}, {"age", 25}}
        }},
        {"count", 2}
    };
    
    // Safe access with optional
    if (auto users = data.as_dictionary().get("users")) {
        if (users->is_array()) {
            for (const auto& user : users->as_array()) {
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
        if (auto value = environment::get(key)) {  // Implicit string conversion
            return string(*value);
        }
        return std::nullopt;
    }
    
    // Function returning expected
    std::expected<dictionary, error> load_config(const string& path)
    {
        auto result = read_file(path);
        if (!result) {
            return std::unexpected(error(error_code::file_not_found, 
                                       "Config file not found"));
        }
        
        return parse_json(*result);
    }

Platform Utilities
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // Working with paths
    auto home = environment::get("HOME").value_or("/tmp");
    auto config_path = path::join(home, ".config", "myapp");
    
    // XDG directories
    auto data_home = xdg::data_home();
    auto app_data = path::join(data_home, "myapp");
    
    // Create directory if needed
    if (auto result = path::create_directory(app_data); !result) {
        std::cerr << "Failed to create directory: " 
                  << result.error().message() << std::endl;
    }
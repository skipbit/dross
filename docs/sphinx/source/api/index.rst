API Reference
=============

.. toctree::
   :maxdepth: 2
   :caption: Modules

   type-system
   platform

Overview
--------

The dross library is organized into several modules, each providing specific functionality:

Type System
~~~~~~~~~~~

The core type system provides dynamic typing with value semantics:

- :doc:`type-system` - Dynamic types including value, number, string, array, dictionary, data, and error

Platform Utilities
~~~~~~~~~~~~~~~~~~

Cross-platform utilities for system interaction:

- :doc:`platform` - Environment variables, filesystem paths, and XDG Base Directory support

Naming Conventions
------------------

The dross library follows consistent naming conventions:

- **Namespaces**: All lowercase (``dross``)
- **Classes**: All lowercase (``value``, ``string``, ``array``)
- **Functions**: Snake case (``to_string()``, ``get_value()``)
- **Constants**: CamelCase with 'k' prefix (``kDefaultValue``)
- **Concepts**: Snake case with ``_type`` suffix (``number_type``)

Error Handling
--------------

dross does not use exceptions. All operations that may fail return either:

- ``std::optional<T>`` for operations that may not produce a value
- ``std::expected<T, error>`` for operations that may fail with error information

Example:

.. code-block:: cpp

    // Using std::optional
    auto env_value = environment::get("MY_VAR");
    if (env_value) {
        std::cout << "Value: " << *env_value << std::endl;
    }
    
    // Using std::expected
    auto result = path::read_file("/path/to/file");
    if (result) {
        process_content(*result);
    } else {
        handle_error(result.error());
    }

Memory Management
-----------------

All types in dross provide value semantics:

- Types are copyable and movable
- No manual memory management required
- RAII principles throughout
- Smart pointers used internally for implementation

Thread Safety
-------------

Unless otherwise documented:

- Types are not thread-safe for modification
- Const operations are thread-safe
- Copy construction and assignment create independent instances
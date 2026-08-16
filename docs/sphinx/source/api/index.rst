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

- :doc:`type-system` - Dynamic types including value, boolean, number, string, array, dictionary, data, and error

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

Operations that may fail report it through the return type rather than by
throwing:

- ``std::optional<T>`` for operations that may not produce a value
- ``std::expected<T, error>`` for operations that may fail with error information

Some operations are exceptions to that rule:

- The bounds-checked accessors — the const ``dictionary::operator[]``,
  ``array::operator[]`` and ``array::value_at()`` — throw
  ``std::out_of_range`` when the key or index is not present. Ask
  ``dictionary::contains()`` or ``array::length()`` before indexing.
- ``path::expand()``, despite returning ``std::expected``, lets a
  ``std::filesystem::filesystem_error`` escape for any canonicalisation
  failure on a ``~`` path. ``path::resolve()`` catches those and returns
  them.
- ``path``'s ``exists()`` calls the throwing form of
  ``std::filesystem::exists``, so an error while querying the path — as
  opposed to the path simply being absent — escapes as a
  ``std::filesystem::filesystem_error``.
- The default ``path`` constructor resolves ``"."`` with the throwing form of
  ``std::filesystem::absolute``.

Example:

.. code-block:: cpp

    // Using std::optional
    auto env_value = environment::value("MY_VAR");
    if (env_value) {
        std::cout << "Value: " << *env_value << std::endl;
    }

    // Using std::expected
    auto result = path::mkdir(std::string{"/path/to/dir"});
    if (result) {
        process_path(result->string());
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

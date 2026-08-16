Changelog
=========

All notable changes to the dross project will be documented here.

The format is based on `Keep a Changelog <https://keepachangelog.com/en/1.0.0/>`_,
and this project adheres to `Semantic Versioning <https://semver.org/spec/v2.0.0.html>`_.

Unreleased
----------

Added
~~~~~

- ``timestamp`` class for date and time handling with timezone support
- ``timezone`` class with type-safe timezone representation and ISO 8601 support
- Modern C++23 chrono integration for timezone offsets
- Optional error handling for timezone parsing (``from_string`` returns ``std::optional``)
- Compositional design with ``date_part`` and ``time_part`` nested classes
- Initial documentation system with Sphinx + Breathe
- GitHub Actions workflow for automatic documentation deployment
- Comprehensive API reference documentation
- User guide with examples and best practices

Changed
~~~~~~~

- Improved timezone API: ``offset()`` returns ``std::chrono::minutes`` instead of int
- Enhanced error handling: ``timezone::from_string()`` returns ``std::optional<timezone>``
- Simplified API: removed redundant timezone methods (``is_local()``, ``has_offset()``)
- Updated documentation to reflect timestamp and timezone APIs

v0.1.0 - 2024-01-20
-------------------

Added
~~~~~

Type System
^^^^^^^^^^^

- ``value`` class for polymorphic type holding
- ``boolean`` class for type-safe boolean operations
- ``number`` class with arbitrary precision arithmetic
- ``string`` class holding UTF-8 text
- ``array`` class with range-based for loop support
- ``dictionary`` class for key-value storage
- ``data`` class for raw byte storage
- ``error`` class for structured error information

Platform Utilities
^^^^^^^^^^^^^^^^^^^

- ``environment`` class for environment variable access
- ``path`` class for filesystem operations with error handling
- ``xdg`` class implementing XDG Base Directory specification

Core Features
^^^^^^^^^^^^^

- Pimpl idiom for ABI stability across the core types
- ``std::expected`` and ``std::optional`` for error handling
- C++23 concepts for type constraints
- Three-way comparison operators for ``boolean``, ``number``, ``data``,
  ``timestamp``, ``timezone`` and ``error``
- Value semantics (copyable and assignable) for the classes listed above,
  apart from ``environment``, which exposes only static members
- Zero external dependencies (standard library only)

Build System
^^^^^^^^^^^^

- CMake build system with FetchContent for dependencies
- GoogleTest integration for unit testing
- Compile flags for C++23 standard compliance
- Generate ``compile_commands.json`` for tooling support

Testing
^^^^^^^

- Unit tests for the type system, ``environment`` and the TOML format layer
- Error path testing alongside success paths
- Boundary condition and edge case testing
- >90% code coverage for public interfaces

Development Tools
^^^^^^^^^^^^^^^^^

- Coding style guide (``CODINGSTYLE.md``)

Known Issues
~~~~~~~~~~~~

- Documentation generation requires manual Doxygen run before Sphinx
- Some C++23 features require specific compiler versions
- Windows support is experimental

v0.0.1 - 2024-01-01
-------------------

Added
~~~~~

- Initial project structure
- Basic CMake configuration
- README with project vision
- License file (MIT)

Migration Guide
---------------

From v0.0.x to v0.1.0
~~~~~~~~~~~~~~~~~~~~~

This is the first stable release, so no migration is needed. However, note that:

- All APIs are now considered stable
- ABI compatibility will be maintained in patch releases
- Breaking changes will only occur in major version bumps

Future Releases
---------------

Planned for v0.2.0
~~~~~~~~~~~~~~~~~~~

- ``uuid`` module for UUID generation and manipulation
- ``url`` module for URL parsing and construction
- Improved Windows platform support
- Performance optimizations for large data structures

Planned for v0.3.0
~~~~~~~~~~~~~~~~~~~

- ``thread`` module for threading primitives
- ``runloop`` module for event loop implementation
- ``timer`` module for high-resolution timers
- Coroutine support with ``async`` module

Long-term Roadmap
~~~~~~~~~~~~~~~~~

- Image processing modules (``image``, ``color``, ``transform``)
- Application support modules (``cli``, ``logger``, ``configuration``)
- Behavioral pattern implementations (``singleton``, ``observer``, ``factory``)
- Package manager integration (Conan, vcpkg)
- Alternative build systems (Meson, Bazel)

Version Support
---------------

Supported Versions
~~~~~~~~~~~~~~~~~~

- **v0.1.x**: Active development, receives all updates
- **v0.0.x**: Legacy, no longer supported

We follow semantic versioning:

- **Patch releases** (0.1.x): Bug fixes, documentation updates
- **Minor releases** (0.x.0): New features, backward compatible
- **Major releases** (x.0.0): Breaking changes, API redesign

Deprecation Policy
~~~~~~~~~~~~~~~~~~

- Features marked as deprecated will be removed in the next major release
- Deprecation warnings will be provided for at least one minor release
- Migration guides will be provided for breaking changes
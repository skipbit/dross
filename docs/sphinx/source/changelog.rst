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
- ``path::mkdir()`` now succeeds when the target directory already exists
  instead of reporting it as a failure with a zero ``code()``; it fails
  only when the underlying filesystem operation reports an actual error
- ``path::expand()`` now reports canonicalisation failures on a ``~`` path
  through its ``std::expected`` return value instead of letting a
  ``std::filesystem::filesystem_error`` escape
- ``find_package(dross <version>)`` now requires the same minor version
  while the major version is 0, where it previously accepted any 0.x. A
  consumer asking for ``0.1``, or for ``0``, no longer matches an installed
  ``0.9``; it has to ask for the minor it was built against. The soname is
  unchanged, so this is what a build refuses, not what the runtime linker
  refuses

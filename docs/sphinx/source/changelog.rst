Changelog
=========

All notable changes to the dross project will be documented here.

The format is based on `Keep a Changelog <https://keepachangelog.com/en/1.0.0/>`_,
and this project adheres to `Semantic Versioning <https://semver.org/spec/v2.0.0.html>`_.

Unreleased
----------

Fixed
~~~~~

- ``toml::deserialize()`` rejects input that is not valid UTF-8 with
  ``std::errc::invalid_argument``, and ``toml::serialize()`` fails with
  ``std::errc::operation_not_supported`` on a string or key that is not
  valid UTF-8
- ``toml::deserialize()`` rejects a control character other than tab with
  ``std::errc::invalid_argument``, as TOML requires, instead of keeping it
  in a string, a quoted key or a comment or skipping it as whitespace. A CR
  is accepted only as part of a CRLF line ending
- ``toml::deserialize()`` reads the ``\b`` and ``\f`` escapes in a quoted
  key as it does in a string, so a key that ``toml::serialize()`` wrote
  with them reads back
- ``toml::deserialize()`` reads the ``\uXXXX`` and ``\UXXXXXXXX`` escapes
  in strings and quoted keys, and rejects one that does not name a Unicode
  scalar value with ``std::errc::invalid_argument``
- ``toml::serialize()`` writes a control character that has no short
  escape as ``\u00XX`` instead of as it is, so the output is valid TOML
  and ``toml::deserialize()`` reads it back
- ``toml::serialize()`` fails with ``std::errc::operation_not_supported``
  on a NaN number instead of writing ``NaN``, which is not a TOML value
- ``toml::serialize()`` writes an integral value outside the 64-bit range
  of TOML integers as a float (``9223372036854775808.0``) instead of as an
  integer that TOML readers must refuse
- ``toml::deserialize()``, ``toml::serialize()`` and constructing a
  ``number`` from a string classify key and number characters by ASCII
  ranges instead of with ``<cctype>``. A byte from 0x80 up reached those
  functions as a negative value where ``char`` is signed, which is
  undefined behaviour, and under a single-byte locale such as ISO-8859-1
  a UTF-8 key whose bytes that locale counts as letters, such as U+00EA,
  was read as a bare key and written without quotes, both invalid TOML
- ``toml::serialize()`` writes an empty key as ``""`` instead of as an
  empty bare key, so ``toml::deserialize()`` reads it back
- ``toml::deserialize()`` reads a bare key that starts with a digit or a
  hyphen, such as ``1234 = 1`` or ``[-x]``, which ``toml::serialize()``
  writes bare
- ``toml::deserialize()`` rejects a key/value pair or a table header that
  is followed on the same line by anything other than a comment, such as
  ``a = 1 b = 2`` or ``[a] b = 1``, with ``std::errc::invalid_argument``
- ``toml::deserialize()`` reads an array with a comma after the last
  value, such as ``[1, 2,]``
- ``path::home()`` and the ``xdg`` accessors treat an empty ``HOME`` or
  ``XDG_*`` variable as unset, and ``path::home()`` returns
  ``std::nullopt`` when the password database entry has an empty home
  directory
- ``path::expand()`` and ``path::resolve()`` expand only ``~`` alone or a
  leading ``~/``. A path such as ``~backup.txt`` or ``~user/x`` is no
  longer joined to the current user's home directory: ``expand()`` returns
  it unchanged and ``resolve()`` resolves it against the working directory

0.1.0 - 2026-10-04
------------------

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
  ``0.9``; it has to ask for the minor it was built against
- The soname carries the minor version while the major version is 0:
  ``libdross.so.0.1`` rather than ``libdross.so.0``. A program linked
  against one 0.x no longer loads another 0.x at run time. From 1.0 the
  soname carries the major version alone

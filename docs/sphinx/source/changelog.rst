Changelog
=========

All notable changes to the dross project will be documented here.

The format is based on `Keep a Changelog <https://keepachangelog.com/en/1.0.0/>`_,
and this project adheres to `Semantic Versioning <https://semver.org/spec/v2.0.0.html>`_.

Unreleased
----------

Added
~~~~~

- ``timestamp::add()`` and ``timestamp::subtract()``, which take a
  ``std::chrono::seconds`` and report a result outside the years 0 to
  9999 as ``std::nullopt`` instead of throwing

- ``array::const_iterator`` and ``dictionary::const_iterator`` convert
  from the ``iterator`` of the same type, as ``data::const_iterator``
  does

- ``operation_errc::empty_task``, which ``operation_queue::enqueue()``
  reports for an empty task

- ``error`` compares with a ``std::error_condition`` and with an error
  condition enum from any library in either order, equal when either
  category reports them equivalent, as ``std::error_code`` does;
  ``operator!=`` is derived from ``operator==``

- ``error`` carries a detail that says what failed and where: the
  constructor ``error(std::errc, std::string)`` takes it and ``detail()``
  returns it, empty when there is none. Comparisons ignore it

Changed
~~~~~~~

- The ``xdg`` accessors ignore a relative path in an ``XDG_*`` variable,
  as the XDG Base Directory specification asks, and use the default under
  the home directory as for an empty one. ``path::home()`` ignores a
  relative ``HOME`` and falls back to the password database, and returns
  ``std::nullopt`` when the password database entry is not an absolute
  path. They used to return such a path as it was, relative to the working
  directory, and so did the ``~`` expansion of ``path::expand()`` and
  ``path::resolve()``

- ``timestamp::from_components()`` and the ``timestamp`` constructors
  that take the components build the timestamp from its parts, instead of
  building a default one and replacing each part, so they allocate less

- ``timestamp``'s ``+`` and ``-`` with a duration throw
  ``std::out_of_range`` for a result whose year, in the timestamp's own
  offset, is outside 0 to 9999, the years parsing accepts. They used to
  return it, and its string did not read back; far enough out the year
  wrapped and the result compared on the wrong side of where it started.
  Minutes and hours with more seconds than ``std::chrono::seconds`` holds
  throw as well instead of overflowing. Arithmetic no longer reaches the
  signed year that ``%Y``, ``%F`` and the ISO 8601 form write below 0

- The examples in the README, the Sphinx guides and the header comments
  write ``dross::`` before each name instead of ``using namespace dross;``.
  With glibc, ``<time.h>`` declares ``long timezone`` and ``<sys/time.h>``,
  which libstdc++'s ``<thread>`` includes, declares ``struct timezone``, so
  under the using-directive ``timezone`` and even ``timezone::utc()`` are
  ambiguous with ``dross::timezone``

- The C++ examples in the README, the Sphinx guides and the header comments
  compile after ``#include <dross/dross.h>``: each writes ``dross::`` before
  the names it uses and declares what else it relies on

- ``timestamp::format()`` with a format string writes the year of ``%Y``
  and ``%F`` as the ISO 8601 form does, in at least four digits, so the
  year 999 is ``0999``. The other conversions are still those of
  ``std::put_time``

- ``error`` compared with an error code enum equals it only when the
  category and the value both match, as ``std::error_code`` does. A
  ``system_category()`` code no longer equals an enum whose code is in
  ``generic_category()``, even when both map to the same condition

- ``error`` compares with ``std::errc`` and with an error code enum in
  either order, such as ``std::errc::invalid_argument == e``.
  ``operator!=`` for these is derived from ``operator==``; naming the
  removed member, such as ``&error::operator!=`` for ``std::errc``, does
  not compile

- An ``iterator`` of ``array``, ``dictionary`` or ``data`` compares with
  a ``const_iterator`` of the same type in either order, such as
  ``a.begin() == a.cend()``, and a ``data`` iterator also orders against
  and subtracts from one in either order. The comparisons of every
  iterator of the three types, and the difference of two ``data``
  iterators, are friend functions instead of members, and ``!=`` and the
  ordering operators are derived from ``==`` and ``<=>``. ``data``
  iterators of the same kind now also compare with ``<=>``, and their
  difference is ``noexcept``. Comparisons written with the operators
  compile as before; naming a removed member, such as
  ``&data::const_iterator::operator<``, does not

- ``dictionary::iterator`` and ``dictionary::const_iterator`` declare
  ``std::input_iterator_tag`` as their ``iterator_category``,
  ``std::forward_iterator_tag`` as their ``iterator_concept`` and ``void``
  as their ``pointer``, as the standard range adaptors do. They return a
  pair by value, which the C++17 forward iterator requirements do not
  allow, and there is no pointer to it. They still model
  ``std::forward_iterator``

- ``dictionary::iterator`` and ``dictionary::const_iterator`` declare
  ``std::pair<std::string, value>`` as their ``value_type``, as
  ``std::flat_map`` does, instead of the pair of references they return.
  An element kept through ``std::iter_value_t`` is a copy that no longer
  changes with the dictionary or dangles once it is destroyed, and copies
  can be assigned and swapped, so they can be sorted by key.
  ``reference`` is still the pair of references, and ``operator*`` is
  declared to return it. Code that names ``value_type`` to take an
  element, such as ``dictionary::iterator::value_type kv = *it;``, still
  compiles but now writes to a copy, not to the dictionary; take it with
  ``auto``, a structured binding or ``reference`` to write through it.
  ``dictionary.h`` includes ``value.h``, since ``value_type`` holds a
  ``value``

- ``error::message()`` returns the message of the code followed by ``": "``
  and the detail when the error has one, such as ``Invalid argument: line
  2, column 3: Expected '='``. Without a detail it returns the message of
  the code as before

- ``error``'s ``<=>`` returns ``std::weak_ordering`` instead of
  ``std::strong_ordering``, since errors that differ only in their detail
  compare equivalent but can be told apart. ``a < b`` and
  ``(a <=> b) == 0`` compile as before; code that stores the result in a
  ``std::strong_ordering`` does not

Fixed
~~~~~

- ``toml::deserialize()`` and ``toml::serialize()`` report why they
  failed in the detail of the error. A parse error names a line and a
  column, counted in bytes, and what was wrong: for a key or table defined
  twice, where its key/value pair or table header starts, and otherwise
  where reading stopped. They used to build the message and drop it, so
  every parse error read ``Invalid argument``

- ``error`` declares its copy assignment, so assigning one compiles under
  GCC with ``-Wextra -Werror``. The assignment the compiler declared was
  deprecated because ``error`` has its own copy constructor

- The ISO 8601 form of a ``timestamp`` and the string of its
  ``date_part`` write a year below 0 with a minus sign before four
  digits, such as ``-0001-12-31``, instead of ``00-1-12-31``

- ``error`` compared with ``std::errc`` asks both categories through
  ``equivalent()``, as ``std::error_code`` does, so a code whose category
  maps it to that condition only in ``equivalent()`` equals it

- ``operation_queue::enqueue()`` refuses an empty ``std::function`` or a
  null function pointer with the new ``operation_errc::empty_task``, as
  ``submit()`` refuses them by returning ``false``. It used to queue the
  task, and the process ended once a worker ran it

- ``path::home()`` reads the password database with ``getpwuid_r()``
  into a buffer of its own. It used ``getpwuid()``, whose entry is shared
  by the whole process, so a call on one thread, directly or through the
  ``xdg`` accessors, could overwrite an entry that another thread had
  taken from ``getpwuid()``

0.1.1 - 2026-10-24
------------------

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
- ``toml::deserialize()`` reads a float with an exponent, such as ``1e20``
  or ``6.626e-34``
- ``toml::deserialize()`` rejects a number that ``number`` cannot hold,
  such as ``1e5000`` or ``-.``, with ``std::errc::invalid_argument``
  instead of reading it as NaN
- ``toml::deserialize()`` rejects a key or table defined twice, such as
  ``x = 1`` followed by ``x = 2`` or a ``[t]`` header after ``t = 1``, with
  ``std::errc::invalid_argument`` instead of keeping only the last
  definition
- ``toml::deserialize()`` keeps table ``a.b`` when a ``[a]`` header
  follows a ``[a.b]`` header, instead of replacing table ``a``
- ``toml::deserialize()`` rejects a ``[[name]]`` array of tables header
  with ``std::errc::invalid_argument`` instead of reading it as a
  ``[name]`` table that keeps only the last table of the array
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

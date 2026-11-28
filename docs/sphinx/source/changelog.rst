Changelog
=========

All notable changes to the dross project will be documented here.

The format is based on `Keep a Changelog <https://keepachangelog.com/en/1.0.0/>`_,
and this project adheres to `Semantic Versioning <https://semver.org/spec/v2.0.0.html>`_.

Unreleased
----------

Added
~~~~~

- Notifications between processes. ``broadcaster::create()`` given a
  place, such as an ``ipc::unix_socket``, publishes the broadcaster there,
  and ``listener::create()`` given the same place hears it from another
  process; over an ``ipc::unix_socket``, one of the same user. The
  listener can be made before the broadcaster, connects again whenever the
  connection ends, and tells the broadcaster the topic names it subscribes
  to; a notification sent before a subscription reaches the broadcaster
  does not reach it. Values cross as a ``value`` through ``value_traits``:
  ``notify()`` on a published broadcaster with a type without them stops
  the program, and ``subscribe()`` on such a listener returns
  ``notification_errc::type_not_convertible``. What happens to the
  connection is reported on ``listener::link`` as a ``link_event``

- ``ipc::unix_socket``, in a new IPC module (``<dross/ipc.h>``), naming a
  place where processes meet: a Unix domain socket at a path

- A gRPC part, ``dross::grpc``, built when dross is configured with
  ``DROSS_WITH_GRPC`` on, against gRPC 1.51 or later and protobuf 3.21 or
  later installed where CMake finds them. ``ipc::grpc_address`` is a place
  to listen at over gRPC and ``ipc::grpc_target`` one to connect to, each
  with the credentials the user gives, so a broadcaster is published and
  heard across machines. A port a ``grpc_address`` listens at is not
  shared with another server, and a try to connect waits up to 5 seconds.
  Only dross talks to dross there. ``find_package(dross COMPONENTS grpc)``
  gives the part and looks for gRPC only then

- The shape of a part that carries messages between processes, in the IPC
  module: ``ipc::message_channel``, ``ipc::message_acceptor`` and
  ``ipc::message_connector``, which a part implements, with
  ``ipc::message_channel_events`` and ``ipc::message_acceptor_events``,
  which the library gives it to call. A place satisfies
  ``ipc::message_listenable_place_type`` when it can be listened at, and
  ``ipc::message_connectable_place_type`` when it can be connected to;
  ``broadcaster::create()`` and ``listener::create()`` take a place of any
  type that does, and ``ipc::unix_socket`` is one. A part that calls one
  of those events while another call on it is under way, or after its last
  call, stops the program

- ``ipc_errc`` in the IPC module, for why carrying a message between
  processes failed: ``message_too_large`` for a message over 64 KiB,
  ``queue_overflow`` for more than 1 MiB waiting to be sent on a
  connection, ``peer_not_same_user`` for a peer running as another user,
  and ``message_cut_short`` for a connection that ends in the middle of a
  message. All but ``peer_not_same_user`` can reach a listener in another
  process as the reason its ``link_event`` gives for a disconnection; a
  peer running as another user is never connected to

- ``value_traits``, specialized for a type to say how it becomes a
  ``value`` and comes back from one, with ``value_convertible_type`` to ask
  whether a type has a conversion and ``value_errc`` for why one failed.
  Conversions are given for ``value`` and the kinds it holds, ``bool``,
  integer and floating-point types, ``std::string``, and ``std::vector``,
  ``std::optional``, and ``std::map`` and ``std::unordered_map`` keyed by
  ``std::string``

- ``observable``, a value its owner thread changes and others are told
  of. ``when_changed()`` calls a function on each change where its
  ``delivery`` says, with the value after the change, or with the values
  before and after it. A ``set()`` from another thread, directly or through
  the ``proxy`` that ``make_proxy()`` gives, takes effect on the owner's run
  loop. ``get()`` from a thread that does not own it stops the program

- ``notification::cancel()``, which stops every delivery of one
  notification that has not started, and ``notify_options``, whose
  ``notify_timing::deferred`` has subscribers that chose ``direct()`` run
  on the sending thread's run loop once ``notify()`` has returned

- ``topic``, ``broadcaster``, ``listener`` and ``notification`` in the
  notification module. A ``broadcaster`` sends typed notifications within
  the process, and a ``listener`` made from it subscribes to them, each
  subscriber called where its ``delivery`` says. Topics that share a name
  but carry different types do not reach each other's subscribers.
  ``listener::subscribe()`` returns ``notification_errc::empty_function``
  for an empty function

- ``delivery`` and ``subscription``, in a new notification module
  (``<dross/notification.h>``). A ``delivery`` names where a subscriber's
  function runs: ``queued()`` on a run loop, ``direct()`` on the sending
  thread, or ``on()`` on one of an ``operation_queue``'s workers. A
  ``subscription`` is the right to go on being called, ended by
  ``reset()`` or its destructor

- ``value::get_if<T>()``, which reads what a ``value`` holds without
  copying it: a pointer to the held ``T``, or ``nullptr`` when the value
  holds another type. It cannot be called on a temporary ``value``.
  ``as<T>()`` and ``value_cast<T>()`` still return a copy

- ``timestamp::to_time_point()`` and ``timestamp::duration_since()``,
  which report a time point or a duration that ``system_clock`` cannot
  hold as ``std::nullopt``

Changed
~~~~~~~

- ``thread`` is started with ``create()``, which returns
  ``std::errc::resource_unavailable_try_again`` when the system will not
  start a thread, instead of letting ``std::system_error`` through, and
  ``std::errc::not_enough_memory`` when there is no memory to start one.
  ``operation_queue`` is made with ``create()`` too. Their constructors
  that started threads are removed: ``dross::thread worker;`` becomes
  ``auto worker = dross::thread::create();``. ``operation_queue::create(0)``
  returns ``std::errc::invalid_argument`` instead of throwing
  ``std::invalid_argument``

- The functions that name a thread or a run loop are static members of
  the type they give, as ``thread::create()`` is, and the functions of the
  old names are removed: ``main_thread()`` becomes ``thread::main()``,
  ``current_thread()`` becomes ``thread::current()``, ``all_threads()``
  becomes ``thread::all()``, ``main_runloop()`` becomes
  ``runloop::main()``, and ``current_runloop()`` becomes
  ``runloop::current()``. ``thread::is_current_thread()`` becomes
  ``thread::is_current()`` to go with them

- A thread from ``thread::create(body)`` runs its own run loop and
  accepts tasks through ``perform()``. ``body`` runs first, and the thread
  ends once ``body`` has returned, no task is queued and no timer is
  installed, so a task performed while ``body`` runs runs after it.
  ``perform()`` on such a thread used to return false. A repeating timer
  installed on it now keeps the thread until the timer stops or ``quit()``

- A thread that ``thread::create()`` or ``thread::create(body)`` started
  refuses tasks as soon as its run loop stops, and drops the tasks
  ``quit()`` left queued then, rather than accepting tasks it would never
  run until the thread had ended

- A timer stops once the last handle to it is gone, the same as after
  ``invalidate()``; its loop used to keep it firing until ``invalidate()``.
  ``timer::once()`` and ``timer::repeating()`` are ``[[nodiscard]]``, so a
  handle dropped as soon as it is made draws a warning

- ``thread::join()`` and ``thread::join_for()`` on a thread dross started
  wait until the OS thread itself has ended, not only until its
  bookkeeping has been torn down. ``join_for()`` bounds the wait for that
  bookkeeping; what the OS thread still has to tear down after it is
  waited for without a bound

- ``operation_queue::thread_count()`` becomes
  ``operation_queue::max_concurrency()``, and the parameter of
  ``operation_queue::create()`` is named ``max_concurrency`` to go with it

- ``operation_queue::submit()`` is removed; ``operation_queue::enqueue()``
  queues a task the same way, and gives back a result where ``submit()``
  gave back ``true``

- ``operation_queue::shutdown()`` is removed. A queue stops accepting
  tasks once its last handle goes, and ``wait_for()`` waits for the tasks
  given to it; nothing waits for its workers to end

- An ``operation_queue`` starts its workers as tasks need them, up to the
  count given to ``create()``, rather than all of them as it is made, and
  a worker that has had nothing to do for about half a second ends. A
  worker with a timer installed on its loop stays, even after the queue
  has stopped, until the timer stops. ``enqueue()`` returns
  ``operation_errc::queue_stopped`` when the queue has no worker and the
  system will not start one. ``thread::current().quit()`` from a task no
  longer ends its worker

- ``toml::serialize`` reads the values it writes without copying them.
  With libstdc++, writing one key holding 1000 numbers takes 30
  allocations instead of 7,036

- ``timestamp::from_time_point()`` and the ``timestamp`` constructor from
  a ``std::chrono::system_clock::time_point`` are ``noexcept``, as
  converting a ``timestamp`` back to a time point is

- ``number`` and ``boolean`` can be moved, and moving is ``noexcept``, as
  for the other types. Moving one used to copy it, and a moved-from one
  now holds zero or false

- ``dross/type/array.h`` and ``dross/type/dictionary.h`` no longer include
  ``<any>``. Code that uses ``std::any`` includes ``<any>`` itself

Fixed
~~~~~

- Converting a ``timestamp`` to ``std::chrono::system_clock::time_point``,
  and subtracting one ``timestamp`` from another, result in the nearer end
  of the clock where ``system_clock`` cannot hold the result, instead of
  overflowing a signed integer. With libstdc++, which counts nanoseconds,
  the year 1000 used to convert to 2169. Both are ``noexcept``

- ``dross.pc`` writes an absolute ``CMAKE_INSTALL_LIBDIR`` or
  ``CMAKE_INSTALL_INCLUDEDIR`` as it is, instead of appending it to the
  prefix

- A timer releases its callback once it has stopped, or when a fire that
  is running it returns. It used to keep the callback while any handle
  remained, so a callback that captured a handle to its own timer was
  never released

0.2.0 - 2026-11-03
------------------

This release is not ABI compatible with 0.1.x, and the soname changes to
``libdross.so.0.2``. Rebuild code that links against the library.

Added
~~~~~

- ``timestamp::from_time_point()``, which reports a
  ``std::chrono::system_clock::time_point`` outside the years 0 to 9999 as
  ``std::nullopt``

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

- ``number::is_floating_point()``, true for a number written with a
  decimal point or an exponent, made from a floating-point type, or
  computed with a floating-point operand or with a fraction, such as
  ``1 / 4``. It does not depend on the value, so ``number("1.0")`` is
  both floating point and an integer. Comparisons ignore it

Changed
~~~~~~~

- ``dictionary``, ``array``, ``string`` and ``value`` have move
  constructors and move assignment operators, where moving used to copy.
  ``value`` also takes a ``dictionary``, ``array``, ``string`` or ``data``
  by rvalue reference, in its constructors and its assignment, and
  ``array::append`` takes a ``value`` by rvalue reference. Moving ten entries
  of a dictionary makes one allocation instead of 31. The source of a move is
  left usable and empty, or null for a ``value``, so code that goes on reading
  what it moved from sees that instead of the old contents

- The difference of two ``timestamp`` values rounds toward zero as a whole,
  where ``system_clock`` counts more coarsely than the timestamps. Their seconds
  and fractions used to round apart. On a clock counting microseconds, 0.9999995
  seconds came to one second and now comes to 999,999 microseconds, and
  0.0000005 seconds came to one microsecond and now comes to zero, so two
  different timestamps can have a difference of zero

- ``timestamp``, ``timestamp::date_part``, ``timestamp::time_part`` and
  ``timezone`` hold their values directly instead of behind a pointer, so
  building, copying and moving one allocates nothing, apart from parsing a
  string. This changes their size and layout, and so the ABI

- The ``timestamp`` constructor that takes a
  ``std::chrono::system_clock::time_point`` results in the nearer end of
  the years 0 to 9999 for one outside them, so ``time_point::max()`` does
  not order before another timestamp. With libc++, whose ``system_clock``
  reaches past those years, it used to give a timestamp whose string did
  not read back, and adding to it threw

- The API page describes the members of ``error``, which it used to list
  by name only

- ``toml::serialize()`` writes an array whose elements are all
  dictionaries as an array of tables, with a ``[[name]]`` header for each
  element, instead of inline on one line. An empty array and an array
  that mixes dictionaries with other values are still written inline

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
  throw as well instead of overflowing

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

- Writing an ``error`` to a stream with ``<<`` adds ``": "`` and the
  detail after ``domain:code`` when the error has one, such as
  ``generic:22: line 2, column 3: Expected '='``. Without a detail it
  writes ``domain:code`` as before

- ``error``'s ``<=>`` returns ``std::weak_ordering`` instead of
  ``std::strong_ordering``, since errors that differ only in their detail
  compare equivalent but can be told apart. ``a < b`` and
  ``(a <=> b) == 0`` compile as before; code that stores the result in a
  ``std::strong_ordering`` does not

- ``number``'s ``<=>`` and ``compare()`` return ``std::weak_ordering``
  instead of ``std::strong_ordering``, since ``number("1.0")`` and
  ``number(1)`` compare equivalent but only the first is floating point.
  ``a < b`` and ``(a <=> b) == 0`` compile as before; code that stores the
  result in a ``std::strong_ordering`` does not

Fixed
~~~~~

- ``timestamp`` no longer overflows a signed integer where
  ``system_clock`` counts nanoseconds, near its ends: building one from a
  time point within a second of ``time_point::min()``, converting one back,
  or taking a difference whose seconds alone do not fit in the duration

- ``toml::deserialize()`` reads an array of tables, written with
  ``[[name]]`` headers, as an ``array`` of ``dictionary`` values. Each
  header appends a table, and the headers and pairs after it fill that
  last table. A ``[[name]]`` header fails with ``Table defined twice``
  when ``name`` is a table defined in another way, and with
  ``Table header names a key that is not a table`` when a key/value pair
  set ``name``. Reading used to fail with
  ``Arrays of tables are not supported``

- ``toml::deserialize()`` reads a datetime with an offset, such as
  ``1979-05-27T07:32:00Z`` or ``1979-05-27 00:32:00.5-07:00``, as a
  ``timestamp`` that keeps the offset, and ``toml::serialize()`` writes a
  ``timestamp`` back in that form. A local date-time, a local date and a
  local time fail with ``timestamp cannot hold a datetime without an
  offset``, since a ``timestamp`` always has an offset. Reading used to
  fail with ``Expected a newline`` or ``Leading zero in a number``, and
  writing with ``Unsupported value type for TOML serialization``

- ``toml::deserialize()`` fails on the floats ``inf`` and ``nan``, with
  or without a sign, with the detail
  ``number has no infinity and keeps nan for errors``. It used to say
  ``Invalid value format`` or ``Invalid number format``

- ``toml::deserialize()`` reads underscores between the digits of a
  number, such as ``1_000``, ``3.14_15``, ``1e1_0`` and ``0xDEAD_beef``,
  and rejects one without a digit on each side. ``toml::serialize()``
  writes the number back without them. They used to fail with
  ``Expected a newline``

- ``toml::deserialize()`` reads integers in hexadecimal, octal and
  binary, such as ``0xff``, ``0o755`` and ``0b1010``, and
  ``toml::serialize()`` writes them back in decimal. They used to fail
  with ``Expected a newline``

- ``toml::deserialize()`` reads multi-line strings, between ``"""`` or
  ``'''``, keeping each newline as it is written. ``toml::serialize()``
  writes the text back as a basic string. They used to fail with
  ``Expected a newline`` or ``Invalid value format``

- ``toml::deserialize()`` reads literal strings, such as
  ``'C:\Users'``, as values and as keys. ``toml::serialize()`` writes
  the text back as a basic string. They used to fail with
  ``Invalid value format`` or ``Invalid key format``

- ``toml::deserialize()`` reads dotted keys, such as ``a.b = 1``, at the
  top level, under a table header and in an inline table, and rejects one
  that would add to a table a header or an inline table defined, as
  TOML v1.0.0 says. They used to fail with ``Expected '='``

- ``toml::serialize()`` writes a float whose value is integral with
  ``.0``, so ``x = 1.0`` and ``x = 1e2`` read and written back are
  ``x = 1.0`` and ``x = 100.0``. They used to be written as the integers
  ``x = 1`` and ``x = 100``, which read back as integers

- ``toml::deserialize()`` rejects a number with a leading zero, such as
  ``01`` or ``-01``, or without a digit on both sides of the decimal
  point, such as ``1.`` or ``-.5``, and a newline or a comment before a
  comma or the closing brace of an inline table. It used to read them

- ``toml::deserialize()`` no longer slows down with the square of the
  number of table headers under one parent. Each header copied the tables
  that earlier headers had put under the same parent, so 2000 headers such
  as ``[servers.sN]`` took about 25 seconds in a Debug build, against 0.2
  seconds now

- ``toml::deserialize()`` and ``toml::serialize()`` report why they
  failed in the detail of the error. A parse error names a line and a
  column, counted in bytes, and what was wrong: for a key or table defined
  twice, where its key/value pair or table header starts, and otherwise
  where reading stopped. They used to build the message and drop it, so
  every parse error read ``Invalid argument``

- ``error`` declares its copy and move operations, so assigning one
  compiles under GCC with ``-Wextra -Werror``, and moving one no longer
  copies it. The assignment the compiler declared was deprecated because
  ``error`` has its own copy constructor, which also kept the compiler
  from declaring the moves

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

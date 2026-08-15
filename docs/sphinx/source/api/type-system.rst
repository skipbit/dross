Type System
===========

The dross type system provides dynamic typing with strong value semantics. All types
use the Pimpl idiom for ABI stability and provide consistent interfaces.

Core Types
----------

value
~~~~~

.. doxygenclass:: dross::value
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``value`` class is the central polymorphic type that can hold any supported type:

.. code-block:: cpp

    #include <iostream>

    #include <dross/type/array.h>
    #include <dross/type/timezone.h>
    #include <dross/type/value.h>

    dross::value v1 = 42;                       // Holds a number
    dross::value v2 = "hello";                  // Holds a string
    dross::value v3 = dross::array{};           // Holds an array
    dross::value v4 = dross::boolean{true};     // Holds a boolean
    dross::value v5 = dross::timestamp{2024, 1, 21, 15, 30, 0,
                                       dross::timezone::offset(9)};  // Holds a timestamp

    // A bare `true` would select the arithmetic constructor and end up as a
    // number, so the boolean above is wrapped explicitly.

    // Always ask is<T>() first: as<T>() has no defined result when the
    // value is holding some other type
    if (v1.is<dross::number>()) {
        auto num = v1.as<dross::number>();
        std::cout << num << std::endl;  // number has operator<<
    }

    if (v5.is<dross::timestamp>()) {
        auto ts = v5.as<dross::timestamp>();
        std::cout << "Meeting time: " << ts << std::endl;
    }

boolean
~~~~~~~

.. doxygenclass:: dross::boolean
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``boolean`` class provides type-safe boolean operations:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/type/boolean.h>

    dross::boolean flag{true};
    dross::boolean enabled{"true"};  // From string
    dross::boolean active{1};        // From integer

    // Seamless string conversion
    std::string status = flag;       // "true"
    std::cout << status << std::endl;

    // Direct output
    std::cout << flag << " " << enabled << " " << active << std::endl;

number
~~~~~~

.. doxygenclass:: dross::number
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``number`` class provides arbitrary precision numeric values:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/type/number.h>

    dross::number n1(42);
    dross::number n2("3.14159265358979323846");
    dross::number n3 = n1 + n2;

    // Seamless string conversion
    std::string result = n3;           // Direct conversion
    std::cout << result << std::endl;  // Via std::string
    std::cout << n3 << std::endl;      // Direct output

string
~~~~~~

.. doxygenclass:: dross::string
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``string`` class provides Unicode-aware string handling:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/type/string.h>

    dross::string s1("Hello");
    dross::string s2(" World");

    // Concatenation is in place; there is no operator+
    s1 += s2;

    // The buffer is UTF-8 and length() reports its size in bytes, so this is
    // 11 only because the content is ASCII
    std::cout << s1.length() << std::endl;  // 11

    // Seamless conversion to std::string, which is what streams accept
    std::string result = s1;                // "Hello World"
    std::cout << result << std::endl;

timestamp
~~~~~~~~~

.. doxygenclass:: dross::timestamp
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``timestamp`` class provides comprehensive date and time handling with timezone support:

.. code-block:: cpp

    #include <chrono>
    #include <iostream>
    #include <string>

    #include <dross/type/timestamp.h>
    #include <dross/type/timezone.h>

    // Construction from components, with a timezone object
    dross::timestamp meeting{2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)}; // +09:00
    dross::timestamp utc_meeting{2024, 1, 21, 6, 30, 0, dross::timezone::utc()}; // UTC

    // Date-only timestamps (time defaults to 00:00:00, timezone to UTC)
    dross::timestamp birthday{1990, 12, 25};

    // Construction from ISO 8601 strings. Each of these compares equal to the
    // component-built timestamp above it.
    dross::timestamp offset_time{"2024-01-21T15:30:00+09:00"};
    dross::timestamp utc_time{"2024-01-21T06:30:00Z"};
    dross::timestamp date_only{"1990-12-25"};
    std::cout << (offset_time == meeting) << " " << (utc_time == utc_meeting)
              << " " << (date_only == birthday) << std::endl;  // 1 1 1

    // Current time
    dross::timestamp now = dross::timestamp::now();

    // Duration arithmetic
    auto tomorrow = now + std::chrono::hours(24);
    auto next_week = now + std::chrono::hours(24 * 7);
    std::cout << tomorrow << " " << next_week << std::endl;

    // Formatting
    std::string iso_str = meeting.format();  // ISO 8601 format
    std::string custom = meeting.format("%Y-%m-%d %H:%M");
    std::cout << iso_str << " / " << custom << std::endl;

    // Component access (always present)
    const auto& date_part = meeting.date();
    int year = date_part.year();
    int month = date_part.month();
    int day = date_part.day();

    const auto& time_part = meeting.time(); // Always present (default 00:00:00)
    int hour = time_part.hour();
    int minute = time_part.minute();
    int second = time_part.second();

    std::cout << year << "/" << month << "/" << day << " "
              << hour << ":" << minute << ":" << second << std::endl;

    const auto& tz = meeting.timezone(); // Always present (default UTC)
    auto offset_minutes = tz.offset(); // Returns std::chrono::minutes
    std::cout << offset_minutes.count() << " minutes" << std::endl;

    // Timezone operations
    if (meeting.timezone().is_utc()) {
        std::cout << "Meeting is in UTC" << std::endl;
    }
    std::cout << "Timezone offset: " << meeting.timezone().format() << std::endl;

    // Seamless string conversion
    std::string meeting_str = meeting;       // "2024-01-21T15:30:00+09:00"
    std::cout << meeting_str << std::endl;
    std::cout << utc_meeting << " " << birthday << std::endl;  // Direct output

timezone
~~~~~~~~

.. doxygenclass:: dross::timezone
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``timezone`` class provides type-safe timezone representation with modern chrono integration:

.. code-block:: cpp

    #include <chrono>
    #include <iostream>
    #include <string>

    #include <dross/type/timezone.h>

    // Factory methods for common timezones
    auto utc = dross::timezone::utc();           // UTC (+00:00)
    auto jst = dross::timezone::offset(9);       // Japan Standard Time (+09:00)
    auto pdt = dross::timezone::offset(-7, 0);   // Pacific Daylight Time (-07:00)
    auto ist = dross::timezone::offset(5, 30);   // India Standard Time (+05:30)

    // Chrono-based factory method for type safety
    auto cet = dross::timezone::offset(std::chrono::minutes(60)); // Central European Time (+01:00)
    auto jst_chrono = dross::timezone::offset(std::chrono::minutes(540)); // +09:00
    std::cout << pdt << " " << ist << " " << cet << " " << jst_chrono << std::endl;

    // Parse from ISO 8601 strings (returns optional for error handling)
    if (auto parsed_utc = dross::timezone::from_string("Z")) {
        std::cout << "Parsed UTC: " << parsed_utc->format() << std::endl;
    }
    if (auto parsed_offset = dross::timezone::from_string("+09:00")) {
        std::cout << "Parsed offset: " << parsed_offset->format() << std::endl;
    }

    // Invalid strings return nullopt
    auto invalid = dross::timezone::from_string("invalid");
    if (!invalid) {
        std::cout << "Failed to parse invalid timezone string" << std::endl;
    }

    // Timezone operations
    if (jst.is_utc()) {
        std::cout << "This is UTC" << std::endl;
    }

    // Get offset as std::chrono::minutes for type safety
    auto minutes = jst.offset(); // std::chrono::minutes (540 minutes)
    std::cout << "Offset: " << minutes.count() << " minutes" << std::endl;

    // Formatting and string conversion
    std::string utc_str = utc.format();     // "Z"
    std::string jst_str = jst.format();     // "+09:00"
    std::cout << utc_str << " " << jst_str << std::endl;

    // Implicit string conversion
    std::string tz_string = jst;            // "+09:00"
    std::cout << "Timezone: " << tz_string << " " << jst << std::endl;

array
~~~~~

.. doxygenclass:: dross::array
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``array`` class provides a dynamic array of values:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/type/array.h>
    #include <dross/type/value.h>

    dross::array arr;
    arr.append(42);
    arr.append("hello");
    arr.append(dross::array{1, 2, 3});

    std::cout << "Length: " << arr.length() << std::endl;  // 3

    // Range-based for loop. value itself has no operator<<, so dispatch on
    // the contained type and print that.
    for (const auto& val : arr) {
        if (val.is<dross::number>()) {
            std::cout << val.as<dross::number>() << std::endl;
        } else if (val.is<dross::string>()) {
            std::string text = val.as<dross::string>();
            std::cout << text << std::endl;
        } else if (val.is<dross::array>()) {
            std::cout << "array of " << val.as<dross::array>().length()
                      << std::endl;
        }
    }

dictionary
~~~~~~~~~~

.. doxygenclass:: dross::dictionary
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``dictionary`` class provides key-value storage:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/type/dictionary.h>
    #include <dross/type/value.h>

    dross::dictionary dict;

    // Name the dross type on the right-hand side. A bare `dict["age"] = 30;`
    // is ambiguous between value's boolean, number and value assignment
    // operators, and `dross::value{x}` with braces selects the
    // initializer-list constructor, producing a one-element array.
    dict["name"] = dross::string("John Doe");
    dict["age"] = dross::number(30);
    dict["active"] = dross::boolean(true);

    std::cout << "Size: " << dict.size() << std::endl;  // 3

    // operator[] inserts a default-constructed value for a missing key,
    // so ask contains() first when you only mean to read.
    if (dict.contains("name")) {
        const dross::value& name = dict["name"];
        if (name.is<dross::string>()) {
            std::string text = name.as<dross::string>();
            std::cout << "Name: " << text << std::endl;
        }
    }

data
~~~~

.. doxygenclass:: dross::data
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``data`` class provides raw byte storage:

.. code-block:: cpp

    #include <cstdint>
    #include <iostream>
    #include <vector>

    #include <dross/type/data.h>

    std::vector<uint8_t> bytes = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    dross::data d(bytes);

    std::cout << "Size: " << d.size() << " bytes" << std::endl;

error
~~~~~

.. doxygenclass:: dross::error
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``error`` class provides structured error information:

.. code-block:: cpp

    #include <iostream>
    #include <system_error>

    #include <dross/type/error.h>

    // error wraps a std::error_code: a numeric value plus its category.
    // std::errc is an error *condition* enum, so it is converted explicitly
    // rather than passed to the error_enum_type constructor.
    dross::error err{static_cast<int>(std::errc::invalid_argument),
                     std::generic_category()};

    std::cout << "Domain: " << err.domain() << std::endl;
    std::cout << "Code: " << err.code() << std::endl;
    std::cout << "Error: " << err.message() << std::endl;

Type Concepts
-------------

The type system uses C++20 concepts to constrain template parameters:

.. doxygenconcept:: dross::number_type
   :project: dross

.. doxygenconcept:: dross::string_type
   :project: dross

.. doxygenconcept:: dross::error_enum_type
   :project: dross

.. doxygenconcept:: dross::container_type
   :project: dross

Type Conversion
---------------

``boolean``, ``number``, ``string``, ``data``, ``timestamp`` and ``timezone``
convert to ``std::string`` in two ways:

- **Implicit conversion**: ``std::string s = type_instance;``
- **STL-style function**: ``std::string s = to_string(type_instance);`` —
  declared in ``<dross/type.h>``, not in the individual type headers

Stream output is provided for ``boolean``, ``number``, ``data``, ``timestamp``,
``timezone`` and ``error``. ``string`` and ``value`` have no ``operator<<``:
convert a ``string`` to ``std::string`` first, and unwrap a ``value`` before
printing it.

``value`` is inspected and unwrapped with member templates:

- ``is<T>()`` - Check whether the value currently holds type ``T``
- ``as<T>()`` - Retrieve the value as type ``T``. The result is unspecified
  unless ``is<T>()`` is true, so always check first. It does not throw

Example:

.. code-block:: cpp

    // Multiple string conversion approaches
    dross::boolean flag{true};
    dross::number n{42};
    dross::string text{"hello"};
    dross::timestamp meeting{2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)}; // +09:00

    // 1. Implicit conversion to std::string
    std::string flag_str = flag;      // "true"
    std::string num_str = n;          // "42"
    std::string text_str = text;      // "hello"
    std::string timestamp_str = meeting; // "2024-01-21T15:30:00+09:00"

    // 2. STL-style explicit conversion
    using dross::to_string;
    auto flag_string = to_string(flag);      // "true"
    auto num_string = to_string(n);          // "42"
    auto text_string = to_string(text);      // "hello"
    auto timestamp_string = to_string(meeting); // "2024-01-21T15:30:00+09:00"

    // 3. Direct stream output. string has no operator<<, so it reaches the
    //    stream through its std::string conversion (text_str above).
    std::cout << flag << " " << n << " " << text_str << " " << meeting << std::endl;

    // Value type conversion
    dross::value v = 42;
    if (v.is<dross::number>()) {
        dross::number num = v.as<dross::number>();
        std::string s = to_string(num);  // STL-style conversion
    }

    // Timestamp in value
    dross::value ts_value = dross::timestamp::now();
    if (ts_value.is<dross::timestamp>()) {
        auto ts = ts_value.as<dross::timestamp>();
        std::cout << "Current time: " << ts.format("%Y-%m-%d %H:%M:%S") << std::endl;
    }

Comparison Operations
---------------------

``boolean``, ``number``, ``data``, ``timestamp``, ``timezone`` and ``error``
provide three-way comparison (the spaceship operator). ``value``, ``string``,
``array`` and ``dictionary`` provide only ``==`` and ``!=``.

.. code-block:: cpp

    dross::number n1 = 42;
    dross::number n2 = 43;

    if (n1 < n2) {
        std::cout << "n1 is less than n2" << std::endl;
    }

    auto ordering = n1 <=> n2;  // std::strong_ordering

    // value is only equality-comparable
    dross::value v1 = 42;
    dross::value v2 = 43;

    if (v1 != v2) {
        std::cout << "v1 and v2 hold different values" << std::endl;
    }

Error Handling
--------------

Type operations that may fail use ``std::optional`` or ``std::expected``:

.. code-block:: cpp

    // Returns std::optional<timezone>
    if (auto tz = dross::timezone::from_string("+09:00")) {
        process(tz->format());
    }

    // Error handling with expected
    auto result = dross::toml::deserialize(toml_input);
    if (!result) {
        std::cerr << "Parse error: " << result.error().message() << std::endl;
    }

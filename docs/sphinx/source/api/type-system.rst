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

    #include <dross/value.h>
    
    dross::value v1 = 42;                                    // Holds a number
    dross::value v2 = "hello";                               // Holds a string
    dross::value v3 = dross::array{};                        // Holds an array
    dross::value v4 = true;                                  // Holds a boolean
    dross::value v5 = dross::datetime{2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)}; // Holds a datetime
    
    // Type checking with seamless string conversion
    if (v1.is<dross::number>()) {
        auto num = v1.as<dross::number>();
        std::cout << num << std::endl;  // Direct output
    }
    
    if (v5.is<dross::datetime>()) {
        auto dt = v5.as<dross::datetime>();
        std::cout << "Meeting time: " << dt << std::endl;
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

    #include <dross/boolean.h>
    
    dross::boolean flag{true};
    dross::boolean enabled{"true"};  // From string
    dross::boolean active{1};        // From integer
    
    // Seamless string conversion
    std::string status = flag;       // "true"
    std::cout << flag << std::endl;  // Direct output

number
~~~~~~

.. doxygenclass:: dross::number
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``number`` class provides arbitrary precision numeric values:

.. code-block:: cpp

    #include <dross/number.h>
    
    dross::number n1(42);
    dross::number n2("3.14159265358979323846");
    dross::number n3 = n1 + n2;
    
    // Seamless string conversion
    std::string result = n3;         // Direct conversion
    std::cout << n3 << std::endl;    // Direct output

string
~~~~~~

.. doxygenclass:: dross::string
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``string`` class provides Unicode-aware string handling:

.. code-block:: cpp

    #include <dross/string.h>
    
    dross::string s1("Hello");
    dross::string s2(" World");
    dross::string s3 = s1 + s2;
    
    // Seamless string conversion
    std::string result = s3;         // Direct conversion
    std::cout << s3 << std::endl;    // Direct output

datetime
~~~~~~~~

.. doxygenclass:: dross::datetime
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

timezone
~~~~~~~~

.. doxygenclass:: dross::timezone
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``timezone`` class provides type-safe timezone representation:

.. code-block:: cpp

    #include <dross/timezone.h>
    
    // Factory methods for common timezones
    auto utc = dross::timezone::utc();           // UTC (+00:00)
    auto local = dross::timezone::local();       // System local timezone
    auto jst = dross::timezone::offset(9);       // Japan Standard Time (+09:00)
    auto pdt = dross::timezone::offset(-7, 0);   // Pacific Daylight Time (-07:00)
    auto ist = dross::timezone::offset(5, 30);   // India Standard Time (+05:30)
    
    // Parse from ISO 8601 strings
    auto parsed_utc = dross::timezone::from_string("Z");
    auto parsed_offset = dross::timezone::from_string("+09:00");
    
    // Timezone operations
    if (jst.is_utc()) {
        std::cout << "This is UTC" << std::endl;
    }
    if (local.is_local()) {
        std::cout << "This is local timezone" << std::endl;
    }
    if (jst.has_offset()) {
        auto minutes = jst.offset_minutes().value(); // 540 minutes
        std::cout << "Offset: " << minutes << " minutes" << std::endl;
    }
    
    // Formatting and string conversion
    std::string utc_str = utc.format();     // "Z"
    std::string jst_str = jst.format();     // "+09:00"
    std::string local_str = local.format(); // "" (empty for local)
    
    // Implicit string conversion
    std::string tz_string = jst;            // "+09:00"
    std::cout << "Timezone: " << jst << std::endl;

The ``datetime`` class provides comprehensive date and time handling with timezone support:

.. code-block:: cpp

    #include <dross/datetime.h>
    
    // Construction with timezone objects (modern API)
    dross::datetime meeting{2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)}; // +09:00
    dross::datetime local_time{2024, 6, 15, 12, 30, 45, dross::timezone::local()}; // Local timezone
    dross::datetime utc_meeting{2024, 1, 21, 6, 30, 0, dross::timezone::utc()}; // UTC
    
    // Factory methods for specific precision
    auto birthday = dross::datetime::date(1990, 12, 25); // Date only
    auto alarm = dross::datetime::time(7, 30, 0); // Time only
    
    // Construction from ISO 8601 strings
    dross::datetime utc_time{"2024-01-21T15:30:00Z"};
    dross::datetime offset_time{"2024-01-21T15:30:00+09:00"};
    dross::datetime date_only{"2024-01-21"};
    dross::datetime time_only{"15:30:00"};
    
    // Current time
    dross::datetime now = dross::datetime::now();
    
    // Duration arithmetic
    auto tomorrow = now + std::chrono::hours(24);
    auto next_week = now + std::chrono::hours(24 * 7);
    
    // Formatting
    std::string iso_str = meeting.format();  // ISO 8601 format
    std::string custom = meeting.format("%Y-%m-%d %H:%M");
    
    // Component access
    int year = meeting.year();
    int month = meeting.month();
    bool has_tz = meeting.has_timezone();
    auto tz = meeting.timezone(); // Get timezone object
    auto prec = meeting.precision(); // Get precision level
    
    // Timezone operations
    if (meeting.timezone().is_utc()) {
        std::cout << "Meeting is in UTC" << std::endl;
    }
    std::cout << "Timezone offset: " << meeting.timezone().format() << std::endl;
    
    // Seamless string conversion
    std::string meeting_str = meeting;       // "2024-01-21T15:30:00+09:00"
    std::cout << meeting << std::endl;       // Direct output

array
~~~~~

.. doxygenclass:: dross::array
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``array`` class provides a dynamic array of values:

.. code-block:: cpp

    #include <dross/array.h>
    
    dross::array arr;
    arr.append(42);
    arr.append("hello");
    arr.append(dross::array{1, 2, 3});
    
    // Range-based for loop
    for (const auto& val : arr) {
        std::cout << val << std::endl;  // Direct stream output
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

    #include <dross/dictionary.h>
    
    dross::dictionary dict;
    dict.set("name", "John Doe");
    dict.set("age", 30);
    dict.set("active", true);
    
    if (auto name = dict.get("name")) {
        std::cout << "Name: " << name->to_string() << std::endl;
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

    #include <dross/data.h>
    
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

    #include <dross/error.h>
    
    dross::error err(dross::error_code::invalid_argument, 
                     "Invalid value provided");
    
    std::cout << "Error: " << err.message() << std::endl;

Type Concepts
-------------

The type system uses C++20 concepts to constrain template parameters:

.. doxygenconcept:: dross::value_type
   :project: dross

.. doxygenconcept:: dross::number_type
   :project: dross

.. doxygenconcept:: dross::string_type
   :project: dross

.. doxygenconcept:: dross::array_type
   :project: dross

.. doxygenconcept:: dross::dictionary_type
   :project: dross

Type Conversion
---------------

All types provide seamless string conversion through multiple approaches:

- **Implicit conversion**: ``std::string s = type_instance;``
- **STL-style function**: ``std::string s = to_string(type_instance);``
- **Stream output**: ``std::cout << type_instance;``
- ``as_T()`` - Convert value to specific type T (for value type)
- ``is_T()`` - Check if value is of type T (for value type)

Example:

.. code-block:: cpp

    // Multiple string conversion approaches
    dross::boolean flag{true};
    dross::number n{42};
    dross::string text{"hello"};
    dross::datetime meeting{2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)}; // +09:00
    
    // 1. Implicit conversion to std::string
    std::string flag_str = flag;      // "true"
    std::string num_str = n;          // "42"
    std::string text_str = text;      // "hello"
    std::string datetime_str = meeting; // "2024-01-21T15:30:00+09:00"
    
    // 2. STL-style explicit conversion
    using dross::to_string;
    auto flag_string = to_string(flag);      // "true"
    auto num_string = to_string(n);          // "42"
    auto text_string = to_string(text);      // "hello"
    auto datetime_string = to_string(meeting); // "2024-01-21T15:30:00+09:00"
    
    // 3. Direct stream output
    std::cout << flag << " " << n << " " << text << " " << meeting << std::endl;
    
    // Value type conversion
    dross::value v = 42;
    if (v.is<dross::number>()) {
        dross::number num = v.as<dross::number>();
        std::string s = to_string(num);  // STL-style conversion
    }
    
    // Datetime in value
    dross::value dt_value = dross::datetime::now();
    if (dt_value.is<dross::datetime>()) {
        auto dt = dt_value.as<dross::datetime>();
        std::cout << "Current time: " << dt.format("%Y-%m-%d %H:%M:%S") << std::endl;
    }

Comparison Operations
---------------------

All types support three-way comparison (spaceship operator):

.. code-block:: cpp

    dross::value v1 = 42;
    dross::value v2 = 43;
    
    if (v1 < v2) {
        std::cout << "v1 is less than v2" << std::endl;
    }
    
    auto result = v1 <=> v2;  // std::strong_ordering

Error Handling
--------------

Type operations that may fail use ``std::optional`` or ``std::expected``:

.. code-block:: cpp

    dross::dictionary dict;
    
    // Returns std::optional<value>
    if (auto val = dict.get("key")) {
        process(*val);
    }
    
    // Error handling with expected
    auto result = parse_json(json_string);
    if (!result) {
        std::cerr << "Parse error: " << result.error().message() << std::endl;
    }
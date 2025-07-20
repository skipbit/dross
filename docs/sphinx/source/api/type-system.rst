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
    
    dross::value v1 = 42;              // Holds a number
    dross::value v2 = "hello";          // Holds a string
    dross::value v3 = dross::array{};   // Holds an array
    
    // Type checking
    if (v1.is_number()) {
        auto num = v1.as_number();
        std::cout << num.to_string() << std::endl;
    }

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
    
    std::cout << n3.to_string() << std::endl;

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
    
    std::cout << s3.to_string() << std::endl;

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
        std::cout << val.to_string() << std::endl;
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

All types provide consistent conversion methods:

- ``to_string()`` - Convert to string representation
- ``as_T()`` - Convert value to specific type T
- ``is_T()`` - Check if value is of type T

Example:

.. code-block:: cpp

    dross::value v = 42;
    
    // Check type
    if (v.is_number()) {
        // Convert to specific type
        dross::number n = v.as_number();
        
        // Convert to string
        std::string s = n.to_string();
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
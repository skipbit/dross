Examples
========

.. toctree::
   :maxdepth: 2
   
   basic-types
   configuration
   data-structures

This section contains practical examples demonstrating various features of the dross library.

Basic Examples
--------------

Working with Values
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <iostream>

    // The umbrella header: to_string() is declared here, not in the
    // individual type headers.
    #include <dross/type.h>

    using namespace dross;

    void print_type_info(const value& v)
    {
        std::cout << "Type: ";

        // is<T>() is the only way to ask what a value holds. A default
        // constructed value holds none of these, and no separate predicate
        // for the empty case exists.
        if (v.is<boolean>()) {
            std::cout << "boolean: " << to_string(v.as<boolean>());
        } else if (v.is<number>()) {
            std::cout << "number: " << to_string(v.as<number>());
        } else if (v.is<string>()) {
            std::cout << "string: " << to_string(v.as<string>());
        } else if (v.is<timestamp>()) {
            std::cout << "timestamp: " << to_string(v.as<timestamp>());
        } else if (v.is<data>()) {
            std::cout << "data: " << to_string(v.as<data>());
        } else if (v.is<array>()) {
            // array and dictionary have no to_string overload
            std::cout << "array of " << v.as<array>().length();
        } else if (v.is<dictionary>()) {
            std::cout << "dictionary of " << v.as<dictionary>().size();
        } else {
            std::cout << "empty";
        }

        std::cout << std::endl;
    }

    int main()
    {
        dictionary dict;
        dict["x"] = number(1);

        // Different value types
        print_type_info(value());                    // empty
        print_type_info(value(boolean(true)));       // boolean
        print_type_info(value(42));                  // number
        print_type_info(value("hello"));             // string
        print_type_info(value(array{1, 2, 3}));      // array
        print_type_info(value(dict));                // dictionary

        return 0;
    }

Building Data Structures
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <cstddef>
    #include <iostream>
    #include <string>

    #include <dross/type/array.h>
    #include <dross/type/dictionary.h>
    #include <dross/type/value.h>

    using namespace dross;

    dictionary create_person(const string& name, int age,
                             const array& hobbies)
    {
        dictionary person;
        person["name"] = name;
        person["age"] = number(age);
        person["hobbies"] = hobbies;
        person["created"] = string("2024-01-20");

        return person;
    }

    int main()
    {
        // Create a list of people
        array people;

        people.append(create_person("Alice", 30,
                                    array{"reading", "hiking"}));
        people.append(create_person("Bob", 25,
                                    array{"gaming", "cooking"}));
        people.append(create_person("Charlie", 35,
                                    array{"photography", "travel"}));

        // Create a database-like structure. array reports its size through
        // length(); dictionary and data use size().
        dictionary database;
        database["version"] = string("1.0");
        database["people"] = people;
        database["count"] = number(people.length());

        // Access and print data
        if (database.contains("people") && database["people"].is<array>()) {
            array people_array = database["people"].as<array>();

            for (size_t i = 0; i < people_array.length(); ++i) {
                const value& entry = people_array[i];
                if (!entry.is<dictionary>()) {
                    continue;
                }

                dictionary person = entry.as<dictionary>();
                if (person.contains("name") && person["name"].is<string>()) {
                    std::string name = person["name"].as<string>();
                    std::cout << "Person " << i + 1 << ": "
                              << name << std::endl;
                }
            }
        }

        return 0;
    }

Advanced Examples
-----------------

Configuration Management
~~~~~~~~~~~~~~~~~~~~~~~~

dross parses TOML, and leaves file I/O to the standard library. The two meet at
``data``, which is what ``toml::deserialize`` consumes and ``toml::serialize``
produces.

.. code-block:: cpp

    #include <expected>
    #include <fstream>
    #include <ios>
    #include <iterator>
    #include <string>
    #include <system_error>

    #include <dross/format/toml.h>
    #include <dross/platform/path.h>
    #include <dross/type/data.h>
    #include <dross/type/dictionary.h>
    #include <dross/type/value.h>

    using namespace dross;

    class config_manager {
    private:
        dictionary _config;
        path _config_path;

    public:
        explicit config_manager(const path& config_path)
            : _config_path(config_path)
        {
            set_defaults();
        }

        void set_defaults()
        {
            _config["theme"] = string("dark");
            _config["language"] = string("en");
            _config["auto_save"] = boolean(true);
            _config["save_interval"] = number(300); // 5 minutes

            dictionary window;
            window["width"] = number(1024);
            window["height"] = number(768);
            window["maximized"] = boolean(false);
            _config["window"] = window;
        }

        // Merges the file over the defaults. A missing file is not an error:
        // the defaults simply stay in place.
        std::expected<void, error> load()
        {
            std::ifstream input{_config_path.string(), std::ios::binary};
            if (!input) {
                return {};
            }

            const std::string text{std::istreambuf_iterator<char>{input},
                                   std::istreambuf_iterator<char>{}};

            const auto parsed = toml::deserialize(data{text});
            if (!parsed) {
                return std::unexpected(parsed.error());
            }

            for (const auto& [key, val] : *parsed) {
                _config[key] = val;
            }

            return {};
        }

        std::expected<void, error> save() const
        {
            const auto serialized = toml::serialize(_config);
            if (!serialized) {
                return std::unexpected(serialized.error());
            }

            std::ofstream output{_config_path.string(), std::ios::binary};
            const std::string text = *serialized;
            output << text;
            if (!output) {
                return std::unexpected(
                    error{static_cast<int>(std::errc::io_error),
                          std::generic_category()});
            }

            return {};
        }

        bool contains(const std::string& key) const
        {
            return _config.contains(key);
        }

        // Callers must check contains() first: the const operator[] throws
        // std::out_of_range for a key that is not present.
        const value& get(const std::string& key) const
        {
            return _config[key];
        }

        void set(const std::string& key, const value& val)
        {
            _config[key] = val;
        }
    };

Data Processing Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <functional>
    #include <iostream>

    #include <dross/type/array.h>
    #include <dross/type/dictionary.h>
    #include <dross/type/value.h>

    using namespace dross;

    class data_processor {
    public:
        // Filter items based on a condition
        array filter(const array& items,
                     std::function<bool(const value&)> predicate)
        {
            array result;
            for (const auto& item : items) {
                if (predicate(item)) {
                    result.append(item);
                }
            }
            return result;
        }

        // Transform items using a function
        array map(const array& items,
                  std::function<value(const value&)> transform)
        {
            array result;
            for (const auto& item : items) {
                result.append(transform(item));
            }
            return result;
        }

        // Reduce array to single value
        value reduce(const array& items,
                     std::function<value(const value&, const value&)> reducer,
                     const value& initial)
        {
            value result = initial;
            for (const auto& item : items) {
                result = reducer(result, item);
            }
            return result;
        }

        // Group items by a key
        dictionary group_by(const array& items,
                            std::function<string(const value&)> key_func)
        {
            dictionary groups;

            for (const auto& item : items) {
                const string key = key_func(item);

                if (groups.contains(key) && groups[key].is<array>()) {
                    array group = groups[key].as<array>();
                    group.append(item);
                    groups[key] = group;
                } else {
                    groups[key] = array{item};
                }
            }

            return groups;
        }
    };

    // Example usage
    int main()
    {
        // dictionary has no initializer-list constructor, so entries are
        // assigned after construction.
        auto make_product = [](const char* name, int price,
                               const char* category) {
            dictionary product;
            product["name"] = string(name);
            product["price"] = number(price);
            product["category"] = string(category);
            return product;
        };

        // Sample data: list of products
        array products;
        products.append(make_product("Laptop", 999, "Electronics"));
        products.append(make_product("Mouse", 29, "Electronics"));
        products.append(make_product("Desk", 299, "Furniture"));
        products.append(make_product("Chair", 199, "Furniture"));
        products.append(make_product("Monitor", 399, "Electronics"));

        data_processor processor;

        // Filter expensive items (price > 200)
        auto expensive = processor.filter(products, [](const value& v) {
            if (v.is<dictionary>()) {
                dictionary product = v.as<dictionary>();
                if (product.contains("price") &&
                    product["price"].is<number>()) {
                    return product["price"].as<number>() > number(200);
                }
            }
            return false;
        });

        // Calculate total price
        auto total = processor.reduce(products,
            [](const value& sum, const value& item) {
                if (item.is<dictionary>()) {
                    dictionary product = item.as<dictionary>();
                    if (product.contains("price") &&
                        product["price"].is<number>()) {
                        return value(sum.as<number>() +
                                     product["price"].as<number>());
                    }
                }
                return sum;
            },
            value(number(0))
        );

        // Group by category
        auto by_category = processor.group_by(products, [](const value& v) {
            if (v.is<dictionary>()) {
                dictionary product = v.as<dictionary>();
                if (product.contains("category") &&
                    product["category"].is<string>()) {
                    return product["category"].as<string>();
                }
            }
            return string("Unknown");
        });

        // value has no operator<<, so unwrap it before printing
        std::cout << "Expensive items: " << expensive.length() << std::endl;
        std::cout << "Total value: $" << total.as<number>() << std::endl;
        std::cout << "Categories: " << by_category.size() << std::endl;

        return 0;
    }

More Examples
-------------

For more examples, visit:

- :doc:`basic-types` - Working with individual type classes
- :doc:`configuration` - Configuration file handling
- :doc:`data-structures` - Building complex data structures

The library's own test suite, under ``test/`` in the source repository, is a
further source of compiling, executable usage.

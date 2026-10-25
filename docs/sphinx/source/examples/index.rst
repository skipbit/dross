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

    void print_type_info(const dross::value& v)
    {
        std::cout << "Type: ";

        // is<T>() is the only way to ask what a value holds. A default
        // constructed value holds none of these, and no separate predicate
        // for the empty case exists.
        if (v.is<dross::boolean>()) {
            std::cout << "boolean: " << dross::to_string(v.as<dross::boolean>());
        } else if (v.is<dross::number>()) {
            std::cout << "number: " << dross::to_string(v.as<dross::number>());
        } else if (v.is<dross::string>()) {
            std::cout << "string: " << dross::to_string(v.as<dross::string>());
        } else if (v.is<dross::timestamp>()) {
            std::cout << "timestamp: " << dross::to_string(v.as<dross::timestamp>());
        } else if (v.is<dross::data>()) {
            std::cout << "data: " << dross::to_string(v.as<dross::data>());
        } else if (v.is<dross::array>()) {
            // array and dictionary have no to_string overload
            std::cout << "array of " << v.as<dross::array>().length();
        } else if (v.is<dross::dictionary>()) {
            std::cout << "dictionary of " << v.as<dross::dictionary>().size();
        } else {
            std::cout << "empty";
        }

        std::cout << std::endl;
    }

    int main()
    {
        dross::dictionary dict;
        dict["x"] = dross::number(1);

        // Different value types
        print_type_info(dross::value());                    // empty
        print_type_info(dross::value(dross::boolean(true)));       // boolean
        print_type_info(dross::value(42));                  // number
        print_type_info(dross::value("hello"));             // string
        print_type_info(dross::value(dross::array{1, 2, 3}));      // array
        print_type_info(dross::value(dict));                // dictionary

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

    dross::dictionary create_person(const dross::string& name, int age,
                             const dross::array& hobbies)
    {
        dross::dictionary person;
        person["name"] = name;
        person["age"] = dross::number(age);
        person["hobbies"] = hobbies;
        person["created"] = dross::string("2024-01-20");

        return person;
    }

    int main()
    {
        // Create a list of people
        dross::array people;

        people.append(create_person("Alice", 30,
                                    dross::array{"reading", "hiking"}));
        people.append(create_person("Bob", 25,
                                    dross::array{"gaming", "cooking"}));
        people.append(create_person("Charlie", 35,
                                    dross::array{"photography", "travel"}));

        // Create a database-like structure. array reports its size through
        // length(); dictionary and data use size().
        dross::dictionary database;
        database["version"] = dross::string("1.0");
        database["people"] = people;
        database["count"] = dross::number(people.length());

        // Access and print data
        if (database.contains("people") && database["people"].is<dross::array>()) {
            dross::array people_array = database["people"].as<dross::array>();

            for (size_t i = 0; i < people_array.length(); ++i) {
                const dross::value& entry = people_array[i];
                if (!entry.is<dross::dictionary>()) {
                    continue;
                }

                dross::dictionary person = entry.as<dross::dictionary>();
                if (person.contains("name") && person["name"].is<dross::string>()) {
                    std::string name = person["name"].as<dross::string>();
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

    class config_manager {
    private:
        dross::dictionary _config;
        dross::path _config_path;

    public:
        explicit config_manager(const dross::path& config_path)
            : _config_path(config_path)
        {
            set_defaults();
        }

        void set_defaults()
        {
            _config["theme"] = dross::string("dark");
            _config["language"] = dross::string("en");
            _config["auto_save"] = dross::boolean(true);
            _config["save_interval"] = dross::number(300); // 5 minutes

            dross::dictionary window;
            window["width"] = dross::number(1024);
            window["height"] = dross::number(768);
            window["maximized"] = dross::boolean(false);
            _config["window"] = window;
        }

        // Merges the file over the defaults. A missing file is not an error:
        // the defaults simply stay in place.
        std::expected<void, dross::error> load()
        {
            std::ifstream input{_config_path.string(), std::ios::binary};
            if (!input) {
                return {};
            }

            const std::string text{std::istreambuf_iterator<char>{input},
                                   std::istreambuf_iterator<char>{}};

            const auto parsed = dross::toml::deserialize(dross::data{text});
            if (!parsed) {
                return std::unexpected(parsed.error());
            }

            for (const auto& [key, val] : *parsed) {
                _config[key] = val;
            }

            return {};
        }

        std::expected<void, dross::error> save() const
        {
            const auto serialized = dross::toml::serialize(_config);
            if (!serialized) {
                return std::unexpected(serialized.error());
            }

            std::ofstream output{_config_path.string(), std::ios::binary};
            const std::string text = *serialized;
            output << text;
            if (!output) {
                return std::unexpected(dross::error{std::errc::io_error});
            }

            return {};
        }

        bool contains(const std::string& key) const
        {
            return _config.contains(key);
        }

        // Callers must check contains() first: the const operator[] throws
        // std::out_of_range for a key that is not present.
        const dross::value& get(const std::string& key) const
        {
            return _config[key];
        }

        void set(const std::string& key, const dross::value& val)
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

    class data_processor {
    public:
        // Filter items based on a condition
        dross::array filter(const dross::array& items,
                     std::function<bool(const dross::value&)> predicate)
        {
            dross::array result;
            for (const auto& item : items) {
                if (predicate(item)) {
                    result.append(item);
                }
            }
            return result;
        }

        // Transform items using a function
        dross::array map(const dross::array& items,
                  std::function<dross::value(const dross::value&)> transform)
        {
            dross::array result;
            for (const auto& item : items) {
                result.append(transform(item));
            }
            return result;
        }

        // Reduce array to single value
        dross::value reduce(const dross::array& items,
                     std::function<dross::value(const dross::value&, const dross::value&)> reducer,
                     const dross::value& initial)
        {
            dross::value result = initial;
            for (const auto& item : items) {
                result = reducer(result, item);
            }
            return result;
        }

        // Group items by a key
        dross::dictionary group_by(const dross::array& items,
                            std::function<dross::string(const dross::value&)> key_func)
        {
            dross::dictionary groups;

            for (const auto& item : items) {
                const dross::string key = key_func(item);

                if (groups.contains(key) && groups[key].is<dross::array>()) {
                    dross::array group = groups[key].as<dross::array>();
                    group.append(item);
                    groups[key] = group;
                } else {
                    groups[key] = dross::array{item};
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
            dross::dictionary product;
            product["name"] = dross::string(name);
            product["price"] = dross::number(price);
            product["category"] = dross::string(category);
            return product;
        };

        // Sample data: list of products
        dross::array products;
        products.append(make_product("Laptop", 999, "Electronics"));
        products.append(make_product("Mouse", 29, "Electronics"));
        products.append(make_product("Desk", 299, "Furniture"));
        products.append(make_product("Chair", 199, "Furniture"));
        products.append(make_product("Monitor", 399, "Electronics"));

        data_processor processor;

        // Filter expensive items (price > 200)
        auto expensive = processor.filter(products, [](const dross::value& v) {
            if (v.is<dross::dictionary>()) {
                dross::dictionary product = v.as<dross::dictionary>();
                if (product.contains("price") &&
                    product["price"].is<dross::number>()) {
                    return product["price"].as<dross::number>() > dross::number(200);
                }
            }
            return false;
        });

        // Calculate total price
        auto total = processor.reduce(products,
            [](const dross::value& sum, const dross::value& item) {
                if (item.is<dross::dictionary>()) {
                    dross::dictionary product = item.as<dross::dictionary>();
                    if (product.contains("price") &&
                        product["price"].is<dross::number>()) {
                        return dross::value(sum.as<dross::number>() +
                                     product["price"].as<dross::number>());
                    }
                }
                return sum;
            },
            dross::value(dross::number(0))
        );

        // Group by category
        auto by_category = processor.group_by(products, [](const dross::value& v) {
            if (v.is<dross::dictionary>()) {
                dross::dictionary product = v.as<dross::dictionary>();
                if (product.contains("category") &&
                    product["category"].is<dross::string>()) {
                    return product["category"].as<dross::string>();
                }
            }
            return dross::string("Unknown");
        });

        // value has no operator<<, so unwrap it before printing
        std::cout << "Expensive items: " << expensive.length() << std::endl;
        std::cout << "Total value: $" << total.as<dross::number>() << std::endl;
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

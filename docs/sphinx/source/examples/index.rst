Examples
========

.. toctree::
   :maxdepth: 2
   
   basic-types
   json-processing
   configuration
   data-structures

This section contains practical examples demonstrating various features of the dross library.

Basic Examples
--------------

Working with Values
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <iostream>
    #include <dross/value.h>
    
    using namespace dross;
    
    void print_type_info(const value& v)
    {
        std::cout << "Value: " << v.to_string() << std::endl;
        std::cout << "Type: ";
        
        if (v.is_null()) std::cout << "null";
        else if (v.is_boolean()) std::cout << "boolean";
        else if (v.is_number()) std::cout << "number";
        else if (v.is_string()) std::cout << "string";
        else if (v.is_array()) std::cout << "array";
        else if (v.is_dictionary()) std::cout << "dictionary";
        else if (v.is_data()) std::cout << "data";
        else if (v.is_error()) std::cout << "error";
        
        std::cout << std::endl << std::endl;
    }
    
    int main()
    {
        // Different value types
        print_type_info(value());                    // null
        print_type_info(value(true));                // boolean
        print_type_info(value(42));                  // number
        print_type_info(value("hello"));             // string
        print_type_info(value(array{1, 2, 3}));     // array
        print_type_info(value(dictionary{{"x", 1}})); // dictionary
        
        return 0;
    }

Building Data Structures
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <iostream>
    #include <dross/dictionary.h>
    #include <dross/array.h>
    
    using namespace dross;
    
    dictionary create_person(const string& name, int age, 
                           const array& hobbies)
    {
        dictionary person;
        person.set("name", name);
        person.set("age", age);
        person.set("hobbies", hobbies);
        person.set("created", "2024-01-20");
        
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
        
        // Create a database-like structure
        dictionary database;
        database.set("version", "1.0");
        database.set("people", people);
        database.set("count", people.size());
        
        // Access and print data
        if (auto people_val = database.get("people")) {
            if (people_val->is_array()) {
                auto people_array = people_val->as_array();
                
                for (size_t i = 0; i < people_array.size(); ++i) {
                    if (people_array[i].is_dictionary()) {
                        auto person = people_array[i].as_dictionary();
                        
                        if (auto name = person.get("name")) {
                            std::cout << "Person " << i + 1 << ": " 
                                     << name->to_string() << std::endl;
                        }
                    }
                }
            }
        }
        
        return 0;
    }

Advanced Examples
-----------------

Configuration Management
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <dross/dictionary.h>
    #include <dross/path.h>
    #include <dross/environment.h>
    
    using namespace dross;
    
    class config_manager {
    private:
        dictionary _config;
        string _config_path;
        
    public:
        config_manager()
        {
            // Determine config path
            auto home = environment::get("HOME").value_or("/tmp");
            _config_path = path::join(home, ".config", "myapp", "config.json");
            
            // Set defaults
            set_defaults();
            
            // Load user config if exists
            load();
        }
        
        void set_defaults()
        {
            _config.set("theme", "dark");
            _config.set("language", "en");
            _config.set("auto_save", true);
            _config.set("save_interval", 300); // 5 minutes
            
            dictionary window;
            window.set("width", 1024);
            window.set("height", 768);
            window.set("maximized", false);
            _config.set("window", window);
        }
        
        std::expected<void, error> load()
        {
            auto content = path::read_file(_config_path);
            if (!content) {
                // File doesn't exist, use defaults
                return {};
            }
            
            auto parsed = parse_json(*content);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            
            // Merge with defaults
            if (parsed->is_dictionary()) {
                merge_config(parsed->as_dictionary());
            }
            
            return {};
        }
        
        std::expected<void, error> save()
        {
            auto json = to_json(_config);
            return path::write_file(_config_path, json);
        }
        
        std::optional<value> get(const string& key) const
        {
            return _config.get(key);
        }
        
        void set(const string& key, const value& val)
        {
            _config.set(key, val);
        }
        
    private:
        void merge_config(const dictionary& user_config)
        {
            for (const auto& [key, value] : user_config) {
                _config.set(key, value);
            }
        }
    };

Data Processing Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <dross/array.h>
    #include <dross/dictionary.h>
    #include <algorithm>
    #include <numeric>
    
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
                string key = key_func(item);
                
                if (auto group = groups.get(key)) {
                    if (group->is_array()) {
                        auto arr = group->as_array();
                        arr.append(item);
                        groups.set(key, arr);
                    }
                } else {
                    groups.set(key, array{item});
                }
            }
            
            return groups;
        }
    };
    
    // Example usage
    int main()
    {
        // Sample data: list of products
        array products{
            dictionary{{"name", "Laptop"}, {"price", 999}, {"category", "Electronics"}},
            dictionary{{"name", "Mouse"}, {"price", 29}, {"category", "Electronics"}},
            dictionary{{"name", "Desk"}, {"price", 299}, {"category", "Furniture"}},
            dictionary{{"name", "Chair"}, {"price", 199}, {"category", "Furniture"}},
            dictionary{{"name", "Monitor"}, {"price", 399}, {"category", "Electronics"}}
        };
        
        data_processor processor;
        
        // Filter expensive items (price > 200)
        auto expensive = processor.filter(products, [](const value& v) {
            if (v.is_dictionary()) {
                auto dict = v.as_dictionary();
                if (auto price = dict.get("price")) {
                    return price->as_number() > number(200);
                }
            }
            return false;
        });
        
        // Calculate total price
        auto total = processor.reduce(products, 
            [](const value& sum, const value& item) {
                if (item.is_dictionary()) {
                    auto dict = item.as_dictionary();
                    if (auto price = dict.get("price")) {
                        return sum.as_number() + price->as_number();
                    }
                }
                return sum;
            }, 
            number(0)
        );
        
        // Group by category
        auto by_category = processor.group_by(products, [](const value& v) {
            if (v.is_dictionary()) {
                auto dict = v.as_dictionary();
                if (auto cat = dict.get("category")) {
                    return cat->as_string();
                }
            }
            return string("Unknown");
        });
        
        std::cout << "Total value: $" << total.to_string() << std::endl;
        std::cout << "Categories: " << by_category.size() << std::endl;
        
        return 0;
    }

More Examples
-------------

For more examples, visit:

- :doc:`basic-types` - Working with individual type classes
- :doc:`json-processing` - JSON parsing and generation
- :doc:`configuration` - Configuration file handling
- :doc:`data-structures` - Building complex data structures

You can also find runnable examples in the `examples/` directory of the source repository.
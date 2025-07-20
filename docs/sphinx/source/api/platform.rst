Platform Utilities
==================

The platform module provides cross-platform utilities for interacting with the operating system.

environment
-----------

.. doxygenclass:: dross::environment
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``environment`` class provides access to environment variables:

.. code-block:: cpp

    #include <dross/environment.h>
    
    // Get environment variable
    if (auto home = dross::environment::get("HOME")) {
        std::cout << "Home directory: " << *home << std::endl;
    }
    
    // Set environment variable
    dross::environment::set("MY_VAR", "my_value");
    
    // Remove environment variable
    dross::environment::unset("MY_VAR");
    
    // Get all environment variables
    auto all_vars = dross::environment::get_all();
    for (const auto& [key, value] : all_vars) {
        std::cout << key << "=" << value << std::endl;
    }

path
----

.. doxygenclass:: dross::path
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``path`` class provides filesystem path operations:

.. code-block:: cpp

    #include <dross/path.h>
    
    // Join path components
    auto config_path = dross::path::join("/home/user", ".config", "app");
    
    // Get absolute path
    auto abs_path = dross::path::absolute("../file.txt");
    
    // Check if path exists
    if (dross::path::exists("/etc/passwd")) {
        std::cout << "System has passwd file" << std::endl;
    }
    
    // Read file contents
    auto result = dross::path::read_file("/path/to/file.txt");
    if (result) {
        std::cout << "Content: " << *result << std::endl;
    } else {
        std::cerr << "Error: " << result.error().message() << std::endl;
    }
    
    // Write file contents
    auto write_result = dross::path::write_file("/path/to/output.txt", 
                                                "Hello, World!");
    if (!write_result) {
        std::cerr << "Write failed: " << write_result.error().message() << std::endl;
    }

Path Operations
~~~~~~~~~~~~~~~

Common path operations include:

- **join()** - Join multiple path components
- **dirname()** - Get directory part of path
- **basename()** - Get filename part of path
- **extension()** - Get file extension
- **stem()** - Get filename without extension
- **absolute()** - Convert to absolute path
- **normalize()** - Normalize path (remove . and ..)
- **relative()** - Get relative path between two paths

File Operations
~~~~~~~~~~~~~~~

File and directory operations:

- **exists()** - Check if path exists
- **is_file()** - Check if path is a regular file
- **is_directory()** - Check if path is a directory
- **file_size()** - Get file size in bytes
- **read_file()** - Read entire file contents
- **write_file()** - Write data to file
- **create_directory()** - Create directory (with parents)
- **remove()** - Remove file or empty directory
- **remove_all()** - Remove recursively

xdg
---

.. doxygenclass:: dross::xdg
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``xdg`` class implements the XDG Base Directory Specification:

.. code-block:: cpp

    #include <dross/xdg.h>
    
    // Get user-specific data directory
    auto data_home = dross::xdg::data_home();
    // Default: $HOME/.local/share
    
    // Get user-specific configuration directory
    auto config_home = dross::xdg::config_home();
    // Default: $HOME/.config
    
    // Get user-specific cache directory
    auto cache_home = dross::xdg::cache_home();
    // Default: $HOME/.cache
    
    // Get user-specific state directory
    auto state_home = dross::xdg::state_home();
    // Default: $HOME/.local/state
    
    // Get runtime directory
    if (auto runtime_dir = dross::xdg::runtime_dir()) {
        std::cout << "Runtime dir: " << *runtime_dir << std::endl;
    }
    
    // Get system data directories
    auto data_dirs = dross::xdg::data_dirs();
    // Default: /usr/local/share:/usr/share
    
    // Get system config directories
    auto config_dirs = dross::xdg::config_dirs();
    // Default: /etc/xdg

XDG Directories
~~~~~~~~~~~~~~~

The XDG Base Directory Specification defines standard locations for:

- **Data files** - Application data that should persist
- **Configuration** - User-specific configuration files
- **Cache** - Non-essential cached data
- **State** - Application state data (logs, history, etc.)
- **Runtime** - Runtime files (sockets, PIDs, etc.)

Example Usage
~~~~~~~~~~~~~

Creating application directories:

.. code-block:: cpp

    #include <dross/xdg.h>
    #include <dross/path.h>
    
    // Create app-specific directories
    auto app_config = dross::path::join(dross::xdg::config_home(), "myapp");
    auto app_data = dross::path::join(dross::xdg::data_home(), "myapp");
    auto app_cache = dross::path::join(dross::xdg::cache_home(), "myapp");
    
    // Create directories if they don't exist
    dross::path::create_directory(app_config);
    dross::path::create_directory(app_data);
    dross::path::create_directory(app_cache);
    
    // Store configuration
    auto config_file = dross::path::join(app_config, "settings.json");
    dross::path::write_file(config_file, config_json);
    
    // Store application data
    auto data_file = dross::path::join(app_data, "database.db");
    
    // Store cached data
    auto cache_file = dross::path::join(app_cache, "thumbnails.cache");

Platform Considerations
-----------------------

Windows Support
~~~~~~~~~~~~~~~

On Windows systems:

- XDG directories map to appropriate Windows locations
- Path separators are handled automatically
- Environment variables use Windows conventions

macOS Support
~~~~~~~~~~~~~

On macOS:

- XDG directories follow macOS conventions where appropriate
- ``~/Library`` paths are used for some directories
- Full POSIX compatibility is maintained

Error Handling
--------------

All filesystem operations return ``std::expected`` for error handling:

.. code-block:: cpp

    auto result = dross::path::read_file("/nonexistent/file");
    if (!result) {
        switch (result.error().code()) {
            case dross::error_code::file_not_found:
                std::cerr << "File not found" << std::endl;
                break;
            case dross::error_code::permission_denied:
                std::cerr << "Permission denied" << std::endl;
                break;
            default:
                std::cerr << "Error: " << result.error().message() << std::endl;
        }
    }
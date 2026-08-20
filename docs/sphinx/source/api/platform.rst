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

The ``environment`` class provides read-only access to environment variables
through a single static accessor, ``value()``:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/platform/environment.h>

    // Read an environment variable. The result is std::nullopt only when
    // the variable is unset; a variable set to "" yields an optional
    // holding an empty string. Handle the missing case explicitly.
    if (auto home = dross::environment::value("HOME")) {
        std::cout << "Home directory: " << *home << std::endl;
    } else {
        std::cout << "HOME is not set" << std::endl;
    }

    // Or fold the missing case into a default
    std::string shell = dross::environment::value("SHELL").value_or("/bin/sh");
    std::cout << "Shell: " << shell << std::endl;

path
----

.. doxygenclass:: dross::path
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``path`` class provides filesystem path operations:

.. code-block:: cpp

    #include <iostream>
    #include <string>

    #include <dross/platform/path.h>

    // home() returns std::optional<path>; append() builds on top of it
    if (auto home = dross::path::home()) {
        dross::path config_path = home->append(".config").append("app");
        std::cout << "Config path: " << config_path.string() << std::endl;

        // Create the directory, including any missing parents. mkdir() is
        // idempotent: it succeeds whether it creates the directory or
        // finds it already there.
        if (auto created = dross::path::mkdir(config_path.string())) {
            std::cout << "Created: " << created->string() << std::endl;
        } else {
            std::cerr << "mkdir: " << created.error().what() << std::endl;
        }
    }

    // A bare string literal is ambiguous between the std::string and the
    // std::filesystem::path constructor, so name the type you mean.
    dross::path relative{std::string{"../file.txt"}};

    // Convert to an absolute, canonical path
    if (auto resolved = relative.resolve()) {
        std::cout << "Resolved: " << resolved->string() << std::endl;
    } else {
        std::cerr << "Resolve failed: " << resolved.error().what() << std::endl;
    }

    // Check whether a path exists
    if (relative.exists()) {
        std::cout << "The relative path exists" << std::endl;
    }

    // Expand a leading ~ to the home directory. For a ~ path, expand()
    // canonicalises and turns any failure the standard library reports as
    // a std::filesystem::filesystem_error -- a missing target, a
    // permission problem, a symlink loop -- into the returned
    // std::expected instead of letting it escape. A path that does not
    // begin with ~ is returned unchanged.
    dross::path user_config{std::string{"~/.config/app"}};
    if (auto expanded = user_config.expand()) {
        std::cout << "Expanded: " << expanded->string() << std::endl;
    } else {
        std::cerr << "Expand failed: " << expanded.error().what() << std::endl;
    }

Path Operations
~~~~~~~~~~~~~~~

Building and inspecting a path, without touching the filesystem:

- **append()** - Return a new path with a component appended
- **string()** - Get the native string representation
- **separator()** - Get the platform's path separator (static)

Filesystem Operations
~~~~~~~~~~~~~~~~~~~~~

Operations that consult the filesystem:

- **exists()** - Check whether the path exists
- **expand()** - Expand a leading ``~`` to the home directory
- **resolve()** - Convert to an absolute, canonical path
- **mkdir()** - Create a directory and any missing parents (static)
- **home()** - Get the user's home directory (static)

``expand()``, ``resolve()`` and ``mkdir()`` are declared to return
``std::expected<path, std::filesystem::filesystem_error>``; ``home()`` returns
``std::optional<path>``. Reading and writing file *contents* is not part of
``path`` — use the standard library's ``<fstream>`` for that.

Some caveats apply to the current implementation:

- ``mkdir()`` is idempotent: it succeeds whether it creates
  the directory or finds it already there. It fails only when
  ``std::filesystem::create_directories`` reports an actual error, such as
  a path component that exists and is not a directory. The operation is
  not atomic — directories created before the failure may remain. Some
  failures are rejected before anything is created at all. Because an
  already-present directory is accepted without inspection, a directory,
  or a symbolic link that resolves to one, left there by another party
  is accepted too. Checking beforehand does not close that gap — the
  check and the use are separate operations, and the entry can be replaced
  in between. What closes it is a location where no other party can write
  to any ancestor of the directory: a writable ancestor can be renamed or
  replaced, so securing only the immediate parent is not enough.
- ``expand()`` routes canonicalisation failures through its return type.
  For a path beginning with ``~`` it canonicalises and converts any error
  the standard library reports as a ``std::filesystem::filesystem_error``
  — a missing target, a permission problem, a symlink loop, an invalid
  component — into the returned ``std::expected`` rather than letting it
  escape. A path that does not begin with ``~`` is returned unchanged.
  Home directory resolution failing (``path::home()`` returning
  ``std::nullopt``) is reported the same way.
- ``exists()`` calls the throwing form of ``std::filesystem::exists``. An
  absent path is simply ``false``, but an error while querying it — an
  over-long name, or a directory the process may not traverse — escapes as a
  ``std::filesystem::filesystem_error``.

xdg
---

.. doxygenclass:: dross::xdg
   :project: dross
   :members:
   :protected-members:
   :undoc-members:

The ``xdg`` class implements the XDG Base Directory Specification:

.. code-block:: cpp

    #include <iostream>

    #include <dross/platform/xdg.h>

    // The accessors are instance methods: the application name given here is
    // appended to every directory they return.
    dross::xdg app{"myapp"};

    // User-specific data directory
    if (auto data_home = app.data_home()) {
        std::cout << "Data: " << *data_home << std::endl;
        // Default: $HOME/.local/share/myapp
    }

    // User-specific configuration directory
    if (auto config_home = app.config_home()) {
        std::cout << "Config: " << *config_home << std::endl;
        // Default: $HOME/.config/myapp
    }

    // User-specific cache directory
    if (auto cache_home = app.cache_home()) {
        std::cout << "Cache: " << *cache_home << std::endl;
        // Default: $HOME/.cache/myapp
    }

    // User-specific state directory
    if (auto state_home = app.state_home()) {
        std::cout << "State: " << *state_home << std::endl;
        // Default: $HOME/.local/state/myapp
    }

XDG Directories
~~~~~~~~~~~~~~~

``xdg`` exposes the four per-user base directories, each already suffixed with
the application name passed to the constructor:

- **data_home()** - Application data that should persist
- **config_home()** - User-specific configuration files
- **cache_home()** - Non-essential cached data
- **state_home()** - Application state data (logs, history, etc.)

Every accessor returns ``std::optional<std::string>`` and yields
``std::nullopt`` when the home directory cannot be determined. The directory
itself is not created for you — pass the result to ``path::mkdir()``, which
accepts an already-present directory as success.

Example Usage
~~~~~~~~~~~~~

Creating application directories:

.. code-block:: cpp

    #include <iostream>

    #include <dross/platform/path.h>
    #include <dross/platform/xdg.h>

    dross::xdg app{"myapp"};

    // Create the config directory, then name a file inside it. mkdir()
    // accepts an already-present directory as success, so any failure
    // here is a real problem.
    if (auto config_home = app.config_home()) {
        const dross::path config_dir{*config_home};
        if (auto created = dross::path::mkdir(*config_home); !created) {
            std::cerr << "mkdir: " << created.error().what() << std::endl;
        }
        dross::path config_file = config_dir.append("settings.toml");
        std::cout << "Config file: " << config_file.string() << std::endl;
    }

    // The data directory works the same way
    if (auto data_home = app.data_home()) {
        dross::path data_file = dross::path{*data_home}.append("database.db");
        std::cout << "Data file: " << data_file.string() << std::endl;
    }

    // ...and so does the cache directory
    if (auto cache_home = app.cache_home()) {
        dross::path cache_file =
            dross::path{*cache_home}.append("thumbnails.cache");
        std::cout << "Cache file: " << cache_file.string() << std::endl;
    }

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

Fallible ``path`` operations return
``std::expected<path, std::filesystem::filesystem_error>``. The error type is
the standard library's, so it is inspected with ``code()`` and reported with
``what()``:

.. code-block:: cpp

    auto result = dross::path::mkdir(std::string{"/nonexistent/dir"});
    if (!result) {
        const std::error_code code = result.error().code();
        if (code == std::errc::no_such_file_or_directory) {
            std::cerr << "No such file or directory" << std::endl;
        } else if (code == std::errc::permission_denied) {
            std::cerr << "Permission denied" << std::endl;
        } else {
            std::cerr << "Error: " << result.error().what() << std::endl;
        }
    }

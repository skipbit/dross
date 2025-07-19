# dross Coding Style Guide

## 1. General

*   **Language:** Use C++23.
*   **File Encoding:** UTF-8.
*   **File Extensions:** Use `.h` for header files and `.cpp` for implementation files.
*   **Include Guards:** Use `#pragma once`.
*   **Dependencies:** Limit dependencies to the standard library and dross itself. External library dependencies are only permitted when implemented as wrappers.

## 2. Naming Conventions

*   **Namespace:** Use `dross`. All lowercase.
*   **Class/Struct Names:** Use `lowercase`. (e.g., `class myclass;`)
*   **Function/Method Names:** Use `lower_snake_case`. (e.g., `void my_function();`)
*   **Variable Names:** Use `lower_snake_case`. (e.g., `int my_variable;`)
*   **Private Member Variables:** Use `snake_case` beginning with `_` (underscore). (e.g., `int _my_member;`)
*   **Concepts:** Use `snake_case` with `_type` suffix. (e.g., `template <typename T> concept my_concept_type = ...`)
*   **Constants:** Use `CamelCase` beginning with `k`. (e.g., `const int kMyConstant = 10;`)
*   **Macros:** Avoid using macros for constants or operations with side effects.

## 3. Formatting

*   **Indentation:** Use 4 spaces.
    *  Do not indent class or function definitions within namespaces.
*   **Braces (`{}`):**
    *   Place opening braces for classes and control structures on the same line; place opening braces for functions on a new line.
    ```cpp
    namespace dross {
    class MyClass {
    public:
        void my_function()
        {
            if (condition) {
                // ...
            }
        }
    };
    }
    ```
    *  For lambda expressions, place the opening brace on the same line and the closing brace on a new line.
*   **Declaration and Implementation:** Separate header files and source files. As a principle, header files should contain only declarations whenever possible, with all implementations (including `= default;`, `= delete;`, and function bodies) written in .cpp files. Exceptions are made only for special cases such as templates and iterator definitions required for range-based for loops. Header files should contain only class and function declarations.
*   **Line Length:** Target 80 characters, but be flexible within reasonable limits to maintain readability.
*   **Reference and Pointer Positioning:** Position reference (`&`) and pointer (`*`) symbols close to the variable name, with a space after the type.
    ```cpp
    // Correct
    void function(const Type& parameter);
    Type& get_reference();
    Type* get_pointer();

    // Incorrect
    void function(const Type &parameter);
    Type &get_reference();
    Type *get_pointer();
    ```
*   **Blank Lines:**
    *   Insert one blank line between logical code blocks.
    *   Insert two blank lines between class and function definitions.

## 4. Coding Style

*   **`Pimpl` Idiom:** To maintain ABI stability, actively use the `Pimpl` (Pointer to implementation) idiom for classes in public headers. Hide internal implementation in private classes named `storage` or `impl`.
*   **Error Handling:**
    *   Do not use exceptions.
    *   For operations that may fail, return `std::optional` or `std::expected`.
    *   Represent error information using the `dross::error` class.
*   **`const` Usage:**
    *   Apply `const` to variables and parameters that are not modified.
    *   Apply `const` to methods that do not modify member variables.
    *   Use `constexpr` whenever possible to define compile-time constants.
*   **`noexcept` Usage:** Apply `noexcept` to functions that are guaranteed not to throw exceptions.
*   **Smart Pointers:** Use `std::unique_ptr` and `std::shared_ptr` for resource management; avoid using raw pointers.
*   **C++20/23 Features:**
    *   Leverage concepts (`concept`) to clarify template type constraints.
    *   Actively use range-based `for` loops (`for (const auto& ...)`).
    *   Use `std::ranges` to write algorithms concisely.

## 5. Documentation

*   **Comments:** Document public APIs using Doxygen-style comments.
    ```cpp
    /**
     * @brief Function that performs a specific operation.
     * @param p1 Description of the first parameter.
     * @return Description of the return value.
     */
    ```

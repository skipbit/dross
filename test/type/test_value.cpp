#include "dross/type/array.h"
#include "dross/type/boolean.h"
#include "dross/type/data.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/timestamp.h"
#include "dross/type/value.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

TEST(value_test, init_with_raw_int)
{
    dross::value v = 1;
    EXPECT_EQ(v, 1);
}

TEST(value_test, init_with_raw_char)
{
    dross::value v = "abc";
    EXPECT_EQ(v, "abc");
}

TEST(value_test, init_with_array)
{
    dross::value v = dross::array{ 1, 2, 3 };
    EXPECT_EQ(v, dross::array({ 1, 2, 3 }));
}

// Braces around a single argument hold that argument, not a one-element array.
TEST(value_test, braces_hold_the_enclosed_value)
{
    EXPECT_TRUE((dross::value{ 1 }.is<dross::number>()));
    EXPECT_TRUE((dross::value{ dross::number{ 42 } }.is<dross::number>()));
    EXPECT_TRUE((dross::value{ dross::string{ "x" } }.is<dross::string>()));
    EXPECT_TRUE((dross::value{ dross::array{ 1, 2 } }.is<dross::array>()));
    EXPECT_EQ((dross::value{ dross::array{ 1, 2 } }.as<dross::array>().length()), 2u);
}

// bool is held as a boolean, other arithmetic types as a number, and a
// character is refused rather than read as its code.
TEST(value_test, bool_holds_a_boolean_and_characters_are_refused)
{
    const dross::value flag = true;
    EXPECT_TRUE(flag.is<dross::boolean>());
    EXPECT_TRUE(static_cast<bool>(flag.as<dross::boolean>()));

    const dross::value small = std::uint8_t{ 7 };
    EXPECT_TRUE(small.is<dross::number>());

    static_assert(! std::is_constructible_v<dross::value, char>);
    static_assert(! std::is_constructible_v<dross::value, char8_t>);
    static_assert(! std::is_assignable_v<dross::value&, char>);
    static_assert(! std::is_constructible_v<dross::value, int*>);
}

// Assignment holds a literal the way construction does.
TEST(value_test, assignment_from_literals)
{
    dross::value v;
    v = true;
    EXPECT_TRUE(v.is<dross::boolean>());
    v = 30;
    EXPECT_TRUE(v.is<dross::number>());
    EXPECT_EQ(v.as<dross::number>(), dross::number(30));
    v = 2.5;
    EXPECT_TRUE(v.is<dross::number>());
    v = "abc";
    EXPECT_TRUE(v.is<dross::string>());

    char buffer[] = "xyz";
    v = buffer;
    EXPECT_TRUE(v.is<dross::string>());
    const dross::value from_buffer = buffer;
    EXPECT_EQ(std::string(from_buffer.as<dross::string>()), "xyz");

    dross::dictionary d;
    d["age"] = 30;
    d["active"] = false;
    d["name"] = "x";
    EXPECT_TRUE(d["age"].is<dross::number>());
    EXPECT_TRUE(d["active"].is<dross::boolean>());
    EXPECT_TRUE(d["name"].is<dross::string>());
}

// Constructor tests
TEST(value_test, default_constructor)
{
    dross::value v;
    EXPECT_FALSE(v);  // should be falsy when default constructed
}

TEST(value_test, copy_constructor)
{
    dross::value original = 42;
    dross::value copy(original);
    EXPECT_EQ(copy, original);
    EXPECT_EQ(copy, 42);
}

TEST(value_test, number_constructor)
{
    dross::number n(123);
    dross::value v(n);
    EXPECT_EQ(v, n);
    EXPECT_TRUE(v.is<dross::number>());
}

TEST(value_test, string_constructor)
{
    dross::string s("hello");
    dross::value v(s);
    EXPECT_EQ(v, s);
    EXPECT_TRUE(v.is<dross::string>());
}

TEST(value_test, array_constructor)
{
    dross::array a({ 1, 2, 3 });
    dross::value v(a);
    EXPECT_EQ(v, a);
    EXPECT_TRUE(v.is<dross::array>());
}

TEST(value_test, dictionary_constructor)
{
    dross::dictionary d;
    d["key"] = dross::value("value");
    dross::value v(d);
    EXPECT_EQ(v, d);
    EXPECT_TRUE(v.is<dross::dictionary>());
}

TEST(value_test, boolean_constructor)
{
    dross::boolean b(true);
    dross::value v(b);
    EXPECT_EQ(v, b);
    EXPECT_TRUE(v.is<dross::boolean>());
}

TEST(value_test, template_constructor_with_int)
{
    dross::value v(42);
    EXPECT_EQ(v, 42);
    EXPECT_TRUE(v.is<dross::number>());
}

TEST(value_test, template_constructor_with_double)
{
    dross::value v(3.14);
    EXPECT_EQ(v, 3.14);
    EXPECT_TRUE(v.is<dross::number>());
}

TEST(value_test, template_constructor_with_string_literal)
{
    dross::value v("test");
    EXPECT_EQ(v, "test");
    EXPECT_TRUE(v.is<dross::string>());
}

TEST(value_test, template_constructor_with_std_string)
{
    std::string str = "hello world";
    dross::value v(str);
    EXPECT_EQ(v, str);
    EXPECT_TRUE(v.is<dross::string>());
}

// Assignment operator tests
TEST(value_test, assignment_nullptr)
{
    dross::value v = 42;
    EXPECT_TRUE(v);
    v = nullptr;
    EXPECT_FALSE(v);
}

TEST(value_test, assignment_value)
{
    dross::value v1 = 42;
    dross::value v2 = "hello";
    v2 = v1;
    EXPECT_EQ(v2, 42);
    EXPECT_TRUE(v2.is<dross::number>());
}

TEST(value_test, assignment_number)
{
    dross::value v = "hello";
    dross::number n(99);
    v = n;
    EXPECT_EQ(v, n);
    EXPECT_TRUE(v.is<dross::number>());
}

TEST(value_test, assignment_boolean)
{
    dross::value v = 42;
    dross::boolean b(true);
    v = b;
    EXPECT_EQ(v, b);
    EXPECT_TRUE(v.is<dross::boolean>());
}

TEST(value_test, assignment_array)
{
    dross::value v = 42;
    dross::array a({ "a", "b", "c" });
    v = a;
    EXPECT_EQ(v, a);
    EXPECT_TRUE(v.is<dross::array>());
}

TEST(value_test, assignment_dictionary)
{
    dross::value v = 42;
    dross::dictionary d;
    d["test"] = dross::value("value");
    v = d;
    EXPECT_EQ(v, d);
    EXPECT_TRUE(v.is<dross::dictionary>());
}

// Comparison operator tests
TEST(value_test, equality_same_type)
{
    dross::value v1 = 42;
    dross::value v2 = 42;
    dross::value v3 = 43;

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_TRUE(v1.equals(v2));
    EXPECT_FALSE(v1.equals(v3));
}

TEST(value_test, equality_different_types)
{
    dross::value num = 42;
    dross::value str = "42";
    dross::value arr = dross::array({ 42 });

    EXPECT_FALSE(num == str);
    EXPECT_FALSE(num == arr);
    EXPECT_FALSE(str == arr);
}

TEST(value_test, inequality_operator)
{
    dross::value v1 = 42;
    dross::value v2 = 43;
    dross::value v3 = 42;

    EXPECT_TRUE(v1 != v2);
    EXPECT_FALSE(v1 != v3);
}

TEST(value_test, equality_with_empty_values)
{
    dross::value v1;
    dross::value v2;
    dross::value v3 = 42;

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
}

// Type checking tests
TEST(value_test, is_type_checking)
{
    dross::value bool_val = dross::boolean(true);
    dross::value num_val = 42;
    dross::value str_val = "hello";
    dross::value arr_val = dross::array({ 1, 2 });
    dross::value dict_val = dross::dictionary();
    dross::value empty_val;

    EXPECT_TRUE(bool_val.is<dross::boolean>());
    EXPECT_FALSE(bool_val.is<dross::number>());
    EXPECT_FALSE(bool_val.is<dross::string>());
    EXPECT_FALSE(bool_val.is<dross::array>());
    EXPECT_FALSE(bool_val.is<dross::dictionary>());

    EXPECT_FALSE(num_val.is<dross::boolean>());
    EXPECT_TRUE(num_val.is<dross::number>());
    EXPECT_FALSE(num_val.is<dross::string>());
    EXPECT_FALSE(num_val.is<dross::array>());
    EXPECT_FALSE(num_val.is<dross::dictionary>());

    EXPECT_FALSE(str_val.is<dross::boolean>());
    EXPECT_FALSE(str_val.is<dross::number>());
    EXPECT_TRUE(str_val.is<dross::string>());
    EXPECT_FALSE(str_val.is<dross::array>());
    EXPECT_FALSE(str_val.is<dross::dictionary>());

    EXPECT_FALSE(arr_val.is<dross::boolean>());
    EXPECT_FALSE(arr_val.is<dross::number>());
    EXPECT_FALSE(arr_val.is<dross::string>());
    EXPECT_TRUE(arr_val.is<dross::array>());
    EXPECT_FALSE(arr_val.is<dross::dictionary>());

    EXPECT_FALSE(dict_val.is<dross::boolean>());
    EXPECT_FALSE(dict_val.is<dross::number>());
    EXPECT_FALSE(dict_val.is<dross::string>());
    EXPECT_FALSE(dict_val.is<dross::array>());
    EXPECT_TRUE(dict_val.is<dross::dictionary>());

    // Empty value should not be any specific type
    EXPECT_FALSE(empty_val.is<dross::boolean>());
    EXPECT_FALSE(empty_val.is<dross::number>());
    EXPECT_FALSE(empty_val.is<dross::string>());
    EXPECT_FALSE(empty_val.is<dross::array>());
    EXPECT_FALSE(empty_val.is<dross::dictionary>());
}

// Type casting tests
TEST(value_test, value_cast_success)
{
    dross::value bool_val = dross::boolean(true);
    dross::value num_val = 42;
    dross::value str_val = "hello";
    dross::value arr_val = dross::array({ 1, 2, 3 });

    dross::boolean extracted_bool = dross::value_cast<dross::boolean>(bool_val);
    dross::number extracted_num = dross::value_cast<dross::number>(num_val);
    dross::string extracted_str = dross::value_cast<dross::string>(str_val);
    dross::array extracted_arr = dross::value_cast<dross::array>(arr_val);

    EXPECT_EQ(extracted_bool, dross::boolean(true));
    EXPECT_EQ(extracted_num, dross::number(42));
    EXPECT_EQ(extracted_str, dross::string("hello"));
    EXPECT_EQ(extracted_arr, dross::array({ 1, 2, 3 }));
}

TEST(value_test, value_cast_wrong_type)
{
    dross::value num_val = 42;

    // Casting to wrong type should return default constructed object
    dross::boolean extracted_bool = dross::value_cast<dross::boolean>(num_val);
    dross::string extracted_str = dross::value_cast<dross::string>(num_val);
    dross::array extracted_arr = dross::value_cast<dross::array>(num_val);

    EXPECT_EQ(extracted_bool, dross::boolean());
    EXPECT_EQ(extracted_str, dross::string());
    EXPECT_EQ(extracted_arr, dross::array());
}

TEST(value_test, value_cast_empty_value)
{
    dross::value empty_val;

    // Casting empty value should return default constructed objects
    dross::number extracted_num = dross::value_cast<dross::number>(empty_val);
    dross::string extracted_str = dross::value_cast<dross::string>(empty_val);
    dross::array extracted_arr = dross::value_cast<dross::array>(empty_val);
    dross::dictionary extracted_dict = dross::value_cast<dross::dictionary>(empty_val);

    EXPECT_EQ(extracted_num, dross::number());
    EXPECT_EQ(extracted_str, dross::string());
    EXPECT_EQ(extracted_arr, dross::array());
    EXPECT_EQ(extracted_dict, dross::dictionary());
}

// Bool conversion tests
TEST(value_test, bool_conversion)
{
    dross::value empty_val;
    dross::value num_val = 0;
    dross::value str_val = "";
    dross::value arr_val = dross::array();
    dross::value dict_val = dross::dictionary();
    dross::value false_val = false;

    EXPECT_FALSE(empty_val);  // empty should be false
    EXPECT_TRUE(num_val);     // any stored value should be true, even 0
    EXPECT_TRUE(str_val);     // any stored value should be true, even empty string
    EXPECT_TRUE(arr_val);     // any stored value should be true, even empty array
    EXPECT_TRUE(dict_val);    // any stored value should be true, even empty dictionary
    EXPECT_TRUE(false_val);   // a null test, not the truth of the held boolean
}

// Complex scenarios
TEST(value_test, nested_array_with_mixed_types)
{
    dross::value v = dross::array{ 42, "hello", dross::array({ 1, 2, 3 }), dross::dictionary() };

    EXPECT_TRUE(v.is<dross::array>());

    dross::array extracted = dross::value_cast<dross::array>(v);
    EXPECT_EQ(extracted.length(), 4);
}

TEST(value_test, reassignment_type_changes)
{
    dross::value v = 42;
    EXPECT_TRUE(v.is<dross::number>());

    v = dross::string("hello");
    EXPECT_TRUE(v.is<dross::string>());
    EXPECT_FALSE(v.is<dross::number>());

    v = dross::array({ 1, 2, 3 });
    EXPECT_TRUE(v.is<dross::array>());
    EXPECT_FALSE(v.is<dross::string>());

    v = nullptr;
    EXPECT_FALSE(v.is<dross::array>());
    EXPECT_FALSE(v);
}

TEST(value_test, self_assignment)
{
    dross::value v = 42;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    v = v;  // self assignment
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    EXPECT_EQ(v, 42);
    EXPECT_TRUE(v.is<dross::number>());
}

TEST(value_test, copy_and_modify)
{
    dross::value original = 42;
    dross::value copy = original;

    copy = dross::string("modified");

    EXPECT_EQ(original, 42);
    EXPECT_EQ(copy, "modified");
    EXPECT_TRUE(original.is<dross::number>());
    EXPECT_TRUE(copy.is<dross::string>());
}

// Edge cases with various numeric types
TEST(value_test, various_numeric_types)
{
    dross::value v_int = static_cast<int>(42);
    dross::value v_long = static_cast<long>(42L);
    dross::value v_float = static_cast<float>(3.14f);
    dross::value v_double = static_cast<double>(3.14);

    EXPECT_TRUE(v_int.is<dross::number>());
    EXPECT_TRUE(v_long.is<dross::number>());
    EXPECT_TRUE(v_float.is<dross::number>());
    EXPECT_TRUE(v_double.is<dross::number>());
}

// Test with large strings
TEST(value_test, large_string_handling)
{
    std::string large_str(10000, 'x');
    dross::value v = large_str;

    EXPECT_TRUE(v.is<dross::string>());
    dross::string extracted = dross::value_cast<dross::string>(v);
    EXPECT_EQ(extracted.length(), 10000);
}

// Test with empty containers
TEST(value_test, empty_containers)
{
    dross::value empty_array = dross::array();
    dross::value empty_dict = dross::dictionary();

    EXPECT_TRUE(empty_array.is<dross::array>());
    EXPECT_TRUE(empty_dict.is<dross::dictionary>());
    EXPECT_TRUE(empty_array);  // should still be truthy
    EXPECT_TRUE(empty_dict);   // should still be truthy
}

// Chain assignments
TEST(value_test, chain_assignments)
{
    dross::value v1, v2, v3;
    v3 = dross::number(42);
    v2 = v3;
    v1 = v2;

    EXPECT_EQ(v1, 42);
    EXPECT_EQ(v2, 42);
    EXPECT_EQ(v3, 42);
}

namespace {

dross::array three_elements()
{
    return dross::array{ dross::value(dross::number(1)), dross::value(dross::number(2)), dross::value(dross::number(3)) };
}

}  // namespace

TEST(value_test, move_construction_takes_what_is_held)
{
    dross::value original{ three_elements() };
    const dross::value moved(std::move(original));
    EXPECT_EQ(moved, dross::value(three_elements()));
    EXPECT_FALSE(original);
}

TEST(value_test, move_assignment_takes_what_is_held)
{
    dross::value original{ three_elements() };
    dross::value target{ dross::number(0) };
    target = std::move(original);
    EXPECT_EQ(target, dross::value(three_elements()));
    EXPECT_FALSE(original);
}

TEST(value_test, moving_cannot_throw)
{
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<dross::value>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<dross::value>);
}

TEST(value_test, move_assigning_to_itself_keeps_what_is_held)
{
    dross::value v{ three_elements() };
    dross::value& same = v;
    v = std::move(same);
    EXPECT_EQ(v, dross::value(three_elements()));
}

// Every operation with no precondition has to work on a moved-from value,
// which holds nothing.
TEST(value_test, moved_from_can_be_used_as_a_null_value)
{
    dross::value original{ dross::number(1) };
    const dross::value moved(std::move(original));

    EXPECT_FALSE(original);
    EXPECT_EQ(original, dross::value());
    EXPECT_EQ(dross::value(original), dross::value());
}

TEST(value_test, moved_from_can_be_assigned_again)
{
    dross::value original{ three_elements() };
    dross::value moved(std::move(original));

    original = dross::number(4);
    EXPECT_EQ(original, dross::value(dross::number(4)));

    dross::value emptied;
    moved = std::move(emptied);
    EXPECT_FALSE(moved);
    emptied = std::move(moved);
    EXPECT_FALSE(emptied);
}

TEST(value_test, building_from_a_temporary_takes_its_contents)
{
    dross::array elements = three_elements();
    const dross::value from_array(std::move(elements));
    EXPECT_EQ(from_array, dross::value(three_elements()));
    EXPECT_TRUE(elements.empty());

    dross::dictionary entries;
    entries["a"] = dross::value(dross::number(1));
    const dross::dictionary expected_entries(entries);
    const dross::value from_dictionary(std::move(entries));
    EXPECT_EQ(from_dictionary, dross::value(expected_entries));
    EXPECT_TRUE(entries.empty());

    dross::string text{ "a string long enough that it does not fit in the small string buffer" };
    const dross::string expected_text(text);
    const dross::value from_string(std::move(text));
    EXPECT_EQ(from_string, dross::value(expected_text));
    EXPECT_EQ(text.length(), 0u);

    dross::data bytes{ "Hello" };
    const dross::data expected_bytes(bytes);
    const dross::value from_data(std::move(bytes));
    EXPECT_EQ(from_data, dross::value(expected_bytes));
    EXPECT_TRUE(bytes.empty());
}

TEST(value_test, building_from_a_named_object_leaves_it_alone)
{
    const dross::array kept = three_elements();
    const dross::value copied(kept);
    EXPECT_EQ(copied, dross::value(kept));
    EXPECT_EQ(kept, three_elements());

    dross::array also_kept = three_elements();
    const dross::value copied_too(also_kept);
    EXPECT_EQ(also_kept, three_elements());
}

TEST(value_test, assigning_a_temporary_takes_its_contents)
{
    dross::value v;

    dross::array elements = three_elements();
    v = std::move(elements);
    EXPECT_EQ(v, dross::value(three_elements()));
    EXPECT_TRUE(elements.empty());

    dross::dictionary entries;
    entries["a"] = dross::value(dross::number(1));
    const dross::dictionary expected_entries(entries);
    v = std::move(entries);
    EXPECT_EQ(v, dross::value(expected_entries));
    EXPECT_TRUE(entries.empty());

    dross::string text{ "a string long enough that it does not fit in the small string buffer" };
    const dross::string expected_text(text);
    v = std::move(text);
    EXPECT_EQ(v, dross::value(expected_text));
    EXPECT_EQ(text.length(), 0u);

    dross::data bytes{ "Hello" };
    const dross::data expected_bytes(bytes);
    v = std::move(bytes);
    EXPECT_EQ(v, dross::value(expected_bytes));
    EXPECT_TRUE(bytes.empty());
}

TEST(value_test, assigning_a_named_object_leaves_it_alone)
{
    dross::array elements = three_elements();
    dross::value v;
    v = elements;
    EXPECT_EQ(v, dross::value(three_elements()));
    EXPECT_EQ(elements, three_elements());
}

// as<T>() hands back the held object itself, so asking twice gives the same
// address, where a copy would give two.
TEST(value_test, as_reads_what_is_held_without_copying)
{
    const dross::value from_boolean{ dross::boolean(true) };
    const dross::value from_number{ dross::number(1) };
    const dross::value from_string{ dross::string("text") };
    const dross::value from_array{ three_elements() };
    dross::dictionary entries;
    entries["a"] = dross::value(dross::number(1));
    const dross::value from_dictionary{ entries };
    const dross::value from_timestamp{ dross::timestamp("2024-01-21T15:30:00Z") };
    const dross::value from_data{ dross::data{ "Hello" } };

    EXPECT_EQ(&from_boolean.as<dross::boolean>(), &from_boolean.as<dross::boolean>());
    EXPECT_EQ(&from_number.as<dross::number>(), &from_number.as<dross::number>());
    EXPECT_EQ(&from_string.as<dross::string>(), &from_string.as<dross::string>());
    EXPECT_EQ(&from_array.as<dross::array>(), &from_array.as<dross::array>());
    EXPECT_EQ(&from_dictionary.as<dross::dictionary>(), &from_dictionary.as<dross::dictionary>());
    EXPECT_EQ(&from_timestamp.as<dross::timestamp>(), &from_timestamp.as<dross::timestamp>());
    EXPECT_EQ(&from_data.as<dross::data>(), &from_data.as<dross::data>());

    EXPECT_EQ(from_array.as<dross::array>(), three_elements());
    EXPECT_EQ(from_dictionary.as<dross::dictionary>(), entries);
}

TEST(value_test, as_another_type_reads_an_empty_object)
{
    const dross::value v{ dross::number(1) };
    EXPECT_TRUE(v.as<dross::array>().empty());
    EXPECT_TRUE(v.as<dross::dictionary>().empty());
    EXPECT_EQ(v.as<dross::string>().length(), 0u);
    EXPECT_EQ(dross::value().as<dross::number>(), dross::number());
}

// A reference into a temporary value would dangle, so a temporary hands over
// what it holds instead.
TEST(value_test, as_on_a_temporary_takes_what_is_held)
{
    EXPECT_TRUE((std::is_same_v<decltype(std::declval<dross::value>().as<dross::array>()), dross::array>));

    std::size_t seen = 0;
    for (const auto& element : dross::value{ three_elements() }.as<dross::array>()) {
        EXPECT_TRUE(element.is<dross::number>());
        ++seen;
    }
    EXPECT_EQ(seen, 3u);

    dross::value original{ three_elements() };
    const dross::array taken = std::move(original).as<dross::array>();
    EXPECT_EQ(taken, three_elements());
    EXPECT_TRUE(original.as<dross::array>().empty());
}

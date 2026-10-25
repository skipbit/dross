#!/usr/bin/env python3
#
# Tests how check-examples.py reads an example and what it compiles.
#
#   python3 scripts/test_check_examples.py

import importlib.util
import os
import sys
import unittest

# Loading the script would otherwise leave a __pycache__ in scripts/.
sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location(
    'check_examples', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'check-examples.py'))
check_examples = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_examples)


def function_body(source):
    return source[source.index('void example()\n{\n') + len('void example()\n{\n'):source.rindex('\n}\n')]


def above_function(source):
    return source[:source.index('void example()')]


class source_of_test(unittest.TestCase):
    def test_statements_become_the_body_of_a_function(self):
        source = check_examples.source_of(['dross::number n{42};', 'n += 1;'], None)

        self.assertEqual(function_body(source), 'dross::number n{42};\nn += 1;')

    def test_include_lines_go_above_the_function(self):
        source = check_examples.source_of(['dross::number n{42};', '#include <list>'], None)

        self.assertIn('#include <list>', above_function(source))
        self.assertNotIn('#include', function_body(source))

    def test_leading_function_declarations_go_above_the_function(self):
        body = ['#include <thread>', '', 'int expensive_work();', 'void show(const std::string& text);', '',
                'show(std::to_string(expensive_work()));']
        source = check_examples.source_of(body, None)

        self.assertIn('int expensive_work();', above_function(source))
        self.assertIn('void show(const std::string& text);', above_function(source))
        self.assertEqual(function_body(source), 'show(std::to_string(expensive_work()));')

    def test_a_declaration_after_a_statement_stays_in_the_body(self):
        source = check_examples.source_of(['dross::number n{42};', 'int later();'], None)

        self.assertEqual(function_body(source), 'dross::number n{42};\nint later();')

    def test_a_comment_above_the_declarations_goes_with_them(self):
        source = check_examples.source_of(['// helpers', 'int work();', 'work();'], None)

        self.assertIn('// helpers\nint work();', above_function(source))
        self.assertEqual(function_body(source), 'work();')

    def test_calls_and_definitions_are_not_taken_as_declarations(self):
        for line in ['worker.join();', 'dross::main_runloop().run();', 'dross::operation_queue queue{ 4 };',
                     'auto loaded = queue.enqueue(load);', 'std::thread worker(run);', 'dross::number n(42);',
                     'dross::path home(std::string{"/home"});', 'return compute(x);', 'return make();',
                     'throw failure();']:
            self.assertFalse(check_examples.is_function_declaration(line), line)

    def test_declarations_are_recognised(self):
        for line in ['int expensive_work();', 'void show(int answer);', 'void show(const std::string& text);',
                     'void tick(void);', 'std::string name() noexcept;', 'int size() const;',
                     'std::optional<dross::string> lookup(const dross::string& key, int depth);']:
            self.assertTrue(check_examples.is_function_declaration(line), line)

    def test_an_example_with_main_is_compiled_as_it_is(self):
        body = ['int main()', '{', '    return 0;', '}']
        source = check_examples.source_of(body, None)

        self.assertEqual(source, '#include <dross/dross.h>\n' + '\n'.join(body) + '\n')

    def test_a_main_with_a_trailing_return_type_is_compiled_as_it_is(self):
        body = ['auto main() -> int', '{', '    return 0;', '}']
        source = check_examples.source_of(body, None)

        self.assertEqual(source, '#include <dross/dross.h>\n' + '\n'.join(body) + '\n')

    def test_main_in_a_comment_or_a_string_does_not_count(self):
        for body in [['// call this from int main()'], ['std::puts("int main()");'], ['/*', 'int main()', '*/']]:
            source = check_examples.source_of(body + ['work();'], None)

            self.assertIn('void example()', source, body)

    def test_a_namespace_scope_example_is_compiled_as_it_is(self):
        body = ['int twice(int x)', '{', '    return x * 2;', '}']
        source = check_examples.source_of(body, 'example-namespace-scope')

        self.assertEqual(source, '#include <dross/dross.h>\n' + '\n'.join(body) + '\n')


class sphinx_examples_test(unittest.TestCase):
    def test_reads_the_class_option_and_the_indented_block(self):
        lines = ['.. code-block:: cpp', '    :class: example-fragment', '', '    int x = 1;', '', 'Text']

        self.assertEqual(list(check_examples.sphinx_examples(lines)), [(4, ['int x = 1;'], 'example-fragment')])

    def test_a_block_without_a_class_has_no_kind(self):
        lines = ['.. code-block:: cpp', '', '    int x = 1;']

        self.assertEqual(list(check_examples.sphinx_examples(lines)), [(3, ['int x = 1;'], None)])

    def test_reads_the_class_option_after_a_hyphenated_one(self):
        lines = ['.. code-block:: cpp', '    :emphasize-lines: 2', '    :class: example-fragment', '', '    int x = 1;']

        self.assertEqual(list(check_examples.sphinx_examples(lines)), [(5, ['int x = 1;'], 'example-fragment')])

    def test_finds_the_marker_among_other_classes(self):
        lines = ['.. code-block:: cpp', '    :class: wide example-fragment', '', '    int x = 1;']

        self.assertEqual(list(check_examples.sphinx_examples(lines)), [(4, ['int x = 1;'], 'example-fragment')])

    def test_both_markers_on_one_block_are_an_error(self):
        lines = ['.. code-block:: cpp', '    :class: example-fragment example-namespace-scope', '', '    int x = 1;']

        with self.assertRaises(check_examples.BlockError):
            list(check_examples.sphinx_examples(lines))

    def test_reads_the_other_names_for_cpp(self):
        for opener in ['.. code-block:: c++', '.. code:: cpp', '.. sourcecode:: cpp']:
            lines = [opener, '', '    int x = 1;']

            self.assertEqual(list(check_examples.sphinx_examples(lines)), [(3, ['int x = 1;'], None)], opener)

    def test_skips_a_block_in_another_language(self):
        lines = ['.. code-block:: bash', '', '    make html::', '', 'Text']

        self.assertEqual(list(check_examples.sphinx_examples(lines)), [])

    def test_a_block_with_no_language_is_an_error(self):
        for lines in [['Example::', '', '    int x = 1;'], ['.. code-block::', '', '    int x = 1;'],
                      ['.. code::', '', '    int x = 1;']]:
            with self.assertRaises(check_examples.BlockError, msg=lines[0]):
                list(check_examples.sphinx_examples(lines))

    def test_a_block_with_no_body_is_an_error(self):
        with self.assertRaises(check_examples.BlockError):
            list(check_examples.sphinx_examples(['.. code-block:: cpp', '']))


class markdown_examples_test(unittest.TestCase):
    def test_reads_the_other_names_for_cpp(self):
        for opener in ['```cpp', '```c++', '```cpp title="main.cpp"']:
            self.assertEqual(list(check_examples.markdown_examples([opener, 'int x = 1;', '```'])),
                             [(2, ['int x = 1;'], None)], opener)

    def test_skips_a_block_in_another_language(self):
        self.assertEqual(list(check_examples.markdown_examples(['```cmake', 'project(x)', '```'])), [])

    def test_an_unclosed_block_is_an_error(self):
        with self.assertRaises(check_examples.BlockError):
            list(check_examples.markdown_examples(['```cpp', 'int x = 1;']))


class header_examples_test(unittest.TestCase):
    def test_reads_code_marked_as_cpp(self):
        for opener in [' * @code', ' * @code{.cpp}']:
            self.assertEqual(list(check_examples.header_examples([opener, ' * int x = 1;', ' * @endcode'])),
                             [(2, ['int x = 1;'], None)], opener)

    def test_an_unclosed_block_is_an_error(self):
        with self.assertRaises(check_examples.BlockError):
            list(check_examples.header_examples([' * @code', ' * int x = 1;']))


if __name__ == '__main__':
    unittest.main()

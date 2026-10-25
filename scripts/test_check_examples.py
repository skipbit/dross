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

    def test_calls_and_definitions_are_not_taken_as_declarations(self):
        for line in ['worker.join();', 'dross::main_runloop().run();', 'dross::operation_queue queue{ 4 };',
                     'auto loaded = queue.enqueue(load);']:
            self.assertIsNone(check_examples.FUNCTION_DECLARATION.match(line), line)

    def test_an_example_with_main_is_compiled_as_it_is(self):
        body = ['int main()', '{', '    return 0;', '}']
        source = check_examples.source_of(body, None)

        self.assertEqual(source, '#include <dross/dross.h>\n' + '\n'.join(body) + '\n')

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


if __name__ == '__main__':
    unittest.main()

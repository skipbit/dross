#!/usr/bin/env python3
#
# Compiles the C++ examples in README.md, the Sphinx sources and the header
# comments, so an example that stops compiling fails a check instead of a
# reader who copies it.
#
#   scripts/check-examples.py [compiler]   default: $CXX, then c++
#
# $CXXFLAGS is added to the flags, such as -stdlib=libc++.
#
# Each example is compiled on its own with -fsyntax-only, after
# #include <dross/dross.h>. One that defines main is compiled as it is; any
# other is taken as the body of a function, with its #include lines and the
# function declarations it opens with moved above it. An example names what
# it uses with dross:: and declares whatever else it relies on. In the Sphinx
# sources, a code-block marked ":class: example-namespace-scope" is compiled
# as it is without main, and one marked ":class: example-fragment" is not
# compiled.

import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# An example often stops at the value it shows how to get, which leaves the
# variable unused once it is compiled as a function body.
FLAGS = [
    '-std=c++23', '-fsyntax-only', '-Wall', '-Wextra', '-Werror',
    '-Wno-unused-variable', '-Wno-unused-but-set-variable',
]


def tracked_files():
    out = subprocess.run(['git', 'ls-files', 'README.md', 'docs', 'include'],
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return out.split()


MARKERS = {'example-fragment', 'example-namespace-scope'}


class BlockError(Exception):
    def __init__(self, line, message):
        super().__init__(message)
        self.line = line


def markdown_examples(lines):
    i = 0
    while i < len(lines):
        if lines[i].strip() == '```cpp':
            j = i + 1
            while lines[j].strip() != '```':
                j += 1
            yield i + 2, lines[i + 1:j], None
            i = j + 1
        else:
            i += 1


def sphinx_examples(lines):
    i = 0
    while i < len(lines):
        if re.match(r'\s*\.\. code-block:: cpp\s*$', lines[i]):
            j = i + 1
            kind = None
            while j < len(lines) and re.match(r'\s+:[\w-]+:', lines[j]):
                option = re.match(r'\s+:class:(.*)', lines[j])
                if option:
                    marked = MARKERS & set(option.group(1).split())
                    if len(marked) > 1:
                        raise BlockError(j + 1, 'has more than one of ' + ', '.join(sorted(MARKERS)))
                    kind = marked.pop() if marked else None
                j += 1
            while j < len(lines) and lines[j].strip() == '':
                j += 1
            indent = len(lines[j]) - len(lines[j].lstrip())
            start = j
            while j < len(lines) and (lines[j].strip() == ''
                                      or len(lines[j]) - len(lines[j].lstrip()) >= indent):
                j += 1
            body = [line[indent:] for line in lines[start:j]]
            while body and body[-1].strip() == '':
                body.pop()
            yield start + 1, body, kind
            i = j
        else:
            i += 1


def header_examples(lines):
    i = 0
    while i < len(lines):
        if re.match(r'\s*\*\s*@code\s*$', lines[i]):
            j = i + 1
            while not re.match(r'\s*\*\s*@endcode', lines[j]):
                j += 1
            yield i + 2, [re.sub(r'^\s*\* ?', '', line) for line in lines[i + 1:j]], None
            i = j + 1
        else:
            i += 1


def examples():
    found = []
    problems = []
    for name in tracked_files():
        if name.endswith('.md'):
            extract = markdown_examples
        elif name.endswith('.rst'):
            extract = sphinx_examples
        elif name.endswith('.h'):
            extract = header_examples
        else:
            continue
        with open(os.path.join(ROOT, name), encoding='utf-8') as f:
            lines = f.read().split('\n')
        try:
            for line, body, kind in extract(lines):
                found.append((name, line, body, kind))
        except BlockError as e:
            problems.append('%s:%d: %s' % (name, e.line, e))
    return found, problems


FUNCTION_DECLARATION = re.compile(r'(?!(?:return|throw|co_return|co_yield)\b)'
                                  r'[A-Za-z_][\w:<>, ]*[\s&*]+[A-Za-z_]\w*\(([^;{}]*)\)'
                                  r'(?:\s*(?:const|noexcept))*\s*;\s*$')


def is_function_declaration(line):
    match = FUNCTION_DECLARATION.match(line)
    if not match:
        return False
    # A parameter is a type and a name, where an argument such as run or 42
    # is one word, so std::thread worker(run); stays a variable.
    parameters = match.group(1).strip()
    return parameters in ('', 'void') or all(len(p.split()) > 1 for p in parameters.split(','))


BLOCK_COMMENT = re.compile(r'/\*.*?\*/', re.S)


# Found at the start of a line, so only a comment over several lines can
# hide one.
def defines_main(code):
    return re.search(r'^\s*(?:int|auto)\s+main\s*\(', BLOCK_COMMENT.sub('', code), re.M) is not None


def source_of(body, kind):
    code = '\n'.join(body)
    if kind == 'example-namespace-scope' or defines_main(code):
        return '#include <dross/dross.h>\n' + code + '\n'
    # A function declaration inside a function body draws -Wvexing-parse, so
    # the ones that open an example go above the function with the includes.
    leading = 0
    while leading < len(body) and (body[leading].strip() == ''
                                   or body[leading].startswith(('#include', '//'))
                                   or is_function_declaration(body[leading])):
        leading += 1
    above = body[:leading] + [line for line in body[leading:] if line.startswith('#include')]
    statements = [line for line in body[leading:] if not line.startswith('#include')]
    return ('#include <dross/dross.h>\n' + '\n'.join(above) + '\n'
            + 'void example()\n{\n' + '\n'.join(statements) + '\n}\n')


def compile_one(compiler, directory, index, body, kind):
    path = os.path.join(directory, 'example_%03d.cpp' % index)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(source_of(body, kind))
    flags = FLAGS + os.environ.get('CXXFLAGS', '').split()
    result = subprocess.run([compiler, *flags, '-I', os.path.join(ROOT, 'include'), path],
                            capture_output=True, text=True)
    return result.returncode, result.stderr


def main():
    compiler = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('CXX', 'c++')
    found, problems = examples()
    # A pattern that stopped matching would otherwise check nothing and pass.
    if not found:
        print('no examples found', file=sys.stderr)
        return 1

    for problem in problems:
        print(problem)
    compiled = [(i, e) for i, e in enumerate(found) if e[3] != 'example-fragment']
    failed = len(problems)
    with tempfile.TemporaryDirectory() as directory:
        with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
            jobs = [(e, pool.submit(compile_one, compiler, directory, i, e[2], e[3]))
                    for i, e in compiled]
            for (name, line, _, _), job in jobs:
                rc, stderr = job.result()
                if rc != 0:
                    failed += 1
                    print('%s:%d: does not compile' % (name, line))
                    print(stderr)

    print('%d examples, %d compiled, %d skipped as fragments, %d failed'
          % (len(found), len(compiled), len(found) - len(compiled), failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
